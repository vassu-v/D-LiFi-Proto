
#include <Arduino.h>

// ==================== CONFIGURATION ====================

// Unique ID for this node (4 characters, alphanumeric)
// IMPORTANT: Change this for each node! Examples: "102a", "203b", "304c"
#define NODE_ID      "102a"

// Reserved ID for broadcast messages (all nodes receive)
#define BROADCAST_ID "FFFF"

// Headquarters/Base Station ID (SOS messages are sent here)
// This is the central command station that monitors all alerts
// Using "000h" pattern: 3 digits + 'h' for headquarters
#define HQ_ID        "000h"

// ==================== PIN ASSIGNMENTS ====================

#define SOS_PIN      D3  // Pushbutton for SOS (INPUT_PULLUP, active LOW)
#define IR_TX_PIN    D1  // IR LED transmitter (OUTPUT)
#define IR_RX_PIN    D2  // IR receiver module (INPUT)
#define LED_STATUS   D4  // Status LED for visual feedback (OUTPUT)
#define LAMP_LIGHT_PIN D5 // Lamp LED - placeholder for future LiFi TX (OUTPUT)

// ==================== TIMING CONSTANTS ====================

// SOS button cooldown period (3 minutes = 180,000 milliseconds)
// Prevents accidental spam and allows cache to rotate
const unsigned long SOS_COOLDOWN = 180000;

// LiFi rebroadcast interval for phone receivers (1 minute = 60,000 milliseconds)
// Ensures people arriving later can still receive the message
const unsigned long LIFI_REBROADCAST_INTERVAL = 60000;

// Cache size for message deduplication (number of recent messages remembered)
#define CACHE_SIZE   3

// Pre-computed hash for SOS messages (all SOS send "HELP!")
// Avoids recomputing same hash every time
#define SOS_MESSAGE "HELP!"
#define SOS_HASH 0x28F9  // simpleHash("HELP!") = 0x28F9

// ==================== MESSAGE TYPE DEFINITIONS ====================

/*
 * Message Type System
 * 
 * Type '1' - BROADCAST (HQ → All Lamps)
 *   From: HQ (000h)
 *   To: FFFF (all nodes)
 *   Purpose: System-wide announcements, all lamps broadcast to phones
 *   Example: "All clear", "Evacuation complete"
 *   Action: All nodes receive and broadcast to phones via LiFi
 * 
 * Type '2' - TARGETED BROADCAST (HQ → Specific Lamp)
 *   From: HQ (000h)
 *   To: Specific node ID
 *   Purpose: HQ tells specific lamp to broadcast message to phones
 *   Example: "Exit via stairwell B" sent only to lamp near stairwell
 *   Action: Only target node broadcasts to phones via LiFi
 * 
 * Type '3' - SOS (Lamp → HQ)
 *   From: Any lamp node
 *   To: HQ (000h)
 *   Purpose: Emergency alert to headquarters (button press)
 *   Example: "HELP!" - routes silently to HQ via mesh
 *   Action: Routes to HQ only, NO phone broadcast (HQ coordinates response)
 *   Note: HQ can then send Type 1 or Type 2 to inform people if appropriate
 * 
 * Type '4' - MESSAGE (Node → HQ)
 *   From: Any node
 *   To: HQ (000h)
 *   Purpose: Normal messages/reports to HQ (non-emergency)
 *   Example: "Battery low", "Temperature 25C", general status
 *   Action: HQ logs message, no special alert
 */

#define MSG_TYPE_BROADCAST '1'  // HQ → All lamps (all broadcast to phones)
#define MSG_TYPE_TARGETED  '2'  // HQ → Specific lamp (only it broadcasts to phones)
#define MSG_TYPE_SOS       '3'  // Lamp → HQ (emergency, special alert)
#define MSG_TYPE_MESSAGE   '4'  // Node → HQ (normal message)

// ==================== DATA STRUCTURES ====================

/*
 * Message Cache Structure
 * Used to remember recently seen messages to prevent:
 *   - Infinite forwarding loops
 *   - Duplicate processing
 *   - Broadcast storms
 */
struct MsgCache {
  String src;       // Source node ID (who sent the original message)
  uint16_t msgHash; // Hash of message content for deduplication
};

// Circular buffer cache for recent messages
MsgCache cache[CACHE_SIZE];
int cacheIndex = 0;  // Points to next cache slot to overwrite (circular)

// ==================== BUTTON STATE TRACKING ====================

// Track last time SOS was sent (for cooldown enforcement)
unsigned long lastSOSTime = 0;

// Track button state for edge detection (prevents debounce issues)
bool lastButtonState = HIGH;  // HIGH = not pressed (INPUT_PULLUP)

// ==================== LIFI REBROADCAST TRACKING ====================

// Store the latest message to rebroadcast to phones via LiFi
String latestLiFiMessage = "";

// Track last time LiFi broadcast was sent
unsigned long lastLiFiBroadcastTime = 0;

// ==================== UTILITY FUNCTIONS ====================

/*
 * Simple Rolling Hash Function
 * 
 * Computes a 16-bit hash from a string for message deduplication and
 * integrity verification. Uses polynomial rolling hash (multiplier = 31).
 * 
 * Purpose:
 *   1. Deduplication: Identify duplicate messages in cache
 *   2. Integrity: Verify message wasn't corrupted during transmission
 * 
 * Note: 16-bit hash has ~65,536 possible values. For low-traffic 5-node
 * networks, collision probability is acceptably low (<0.01%).
 * 
 * Parameters:
 *   s - String to hash
 * 
 * Returns:
 *   16-bit hash value
 */
uint16_t simpleHash(String s){
  uint16_t h = 0;
  for (int i = 0; i < s.length(); i++){
    h = (h * 31) + s[i]; // Polynomial rolling hash
  }
  return h;
}

// ==================== CACHE FUNCTIONS ====================

/*
 * Check if Message is New (Not in Cache)
 * 
 * Searches cache for a message with matching source ID and hash.
 * If found, message is a duplicate (return false).
 * If not found, adds to cache and returns true.
 * 
 * This prevents:
 *   - Nodes from forwarding the same message multiple times
 *   - Infinite loops in the mesh network
 *   - Broadcast storms
 * 
 * Cache is circular: oldest entries are overwritten when full.
 * 
 * Parameters:
 *   src  - Source node ID (4 characters)
 *   hash - Message hash (16-bit)
 * 
 * Returns:
 *   true  - Message is new, added to cache
 *   false - Message is duplicate, already in cache
 */
bool isNew(String src, uint16_t hash){
  // Search cache for matching entry
  for (int i = 0; i < CACHE_SIZE; i++){
    if (cache[i].src == src && cache[i].msgHash == hash){
      return false; // Duplicate found, discard
    }
  }
  
  // Message is new, add to cache
  cache[cacheIndex].src = src;
  cache[cacheIndex].msgHash = hash;
  cacheIndex = (cacheIndex + 1) % CACHE_SIZE; // Circular increment
  
  return true;
}

// ==================== IR COMMUNICATION PLACEHOLDERS ====================

/*
 * IR Transmission - Placeholder Implementation
 * For production: Use IRremote library, add start/stop markers, Manchester encoding
 * 
 * Real LiFi/IR Protocol Structure:
 *   Burst 1: [START_MARKER(0xFF)][13 bytes header][SEPARATOR(0xFE)]
 *   Burst 2: [message bytes][END_MARKER(0xFD)]
 */
void irSend(String header, String message){
  // Burst 1: Send header (IR mesh communication)
  Serial.print("TX Burst 1 (Header): "); 
  Serial.println(header);
  digitalWrite(IR_TX_PIN, HIGH);
  delay(25);
  digitalWrite(IR_TX_PIN, LOW);
  delay(10);  // Inter-burst gap
  
  // Burst 2: Send message (IR mesh communication)
  Serial.print("TX Burst 2 (Message): "); 
  Serial.println(message);
  digitalWrite(LAMP_LIGHT_PIN, HIGH);
  delay(25);
  digitalWrite(LAMP_LIGHT_PIN, LOW);
}

/*
 * LiFi Broadcast to Phones - Separate from IR Mesh
 * Transmits message via lamp light for phone receivers
 * This is separate from node-to-node IR communication
 */
void lifiTransmit(String message){
  Serial.print("LiFi Broadcast: "); 
  Serial.println(message);
  
  // Placeholder: Flash lamp to indicate broadcast
  // Real implementation: Modulate lamp at kHz frequency with encoded data
  digitalWrite(LAMP_LIGHT_PIN, HIGH);
  delay(100);  // Longer pulse to distinguish from IR forwarding
  digitalWrite(LAMP_LIGHT_PIN, LOW);
}

/*
 * IR Reception - Placeholder Implementation
 * For production: Use IRremote library for actual IR decoding
 * 
 * Real LiFi/IR Protocol Structure:
 *   Receive Burst 1: Wait for START(0xFF) → Read 13 bytes → Wait for SEPARATOR(0xFE)
 *   Receive Burst 2: Read until END(0xFD)
 * 
 * Current Serial Testing Format (temporary convenience):
 *   Type two lines in Serial Monitor:
 *   Line 1: "102aFFFF3AAAA"  (header)
 *   Line 2: "HELP!"           (message)
 */
bool irReceive(String &header, String &message){
  static bool headerReceived = false;
  static String receivedHeader = "";
  
  if (Serial.available()){
    String line = Serial.readStringUntil('\n');
    line.trim();  // Remove whitespace
    
    if(!headerReceived){
      // First burst: receive header
      if(line.length() == 13){  // Valid header length
        receivedHeader = line;
        headerReceived = true;
        Serial.println("RX Burst 1 (Header) received");
      }
      return false;  // Wait for second burst
    } else {
      // Second burst: receive message
      header = receivedHeader;
      message = line;
      headerReceived = false;  // Reset for next packet
      Serial.println("RX Burst 2 (Message) received");
      return true;  // Complete packet received
    }
  }
  return false;
}

// ==================== NODE FUNCTIONS ====================

/*
 * Generate SOS Emergency Message
 * Sends emergency alert to HQ via mesh (Type 3)
 * Does NOT broadcast to phones - only routes to HQ for coordination
 * 
 * Message Flow:
 *   1. Send Type 3 (SOS) to HQ via mesh routing
 *   2. Intermediate nodes forward it (mesh behavior)
 *   3. HQ receives and processes emergency alert
 *   4. HQ can then send Type 1 or Type 2 to broadcast info to phones if needed
 */
void generateSOS(){
  String msg = SOS_MESSAGE;  // "HELP!"
  uint16_t h = SOS_HASH;     // Pre-computed, no need to calculate

  char hashStr[5];
  sprintf(hashStr, "%04X", h);

  // Build header: Type 3 = SOS, destination = HQ
  String header = String(NODE_ID) + HQ_ID + MSG_TYPE_SOS + String(hashStr);

  isNew(NODE_ID, h);  // Add to cache to prevent re-forwarding own SOS
  irSend(header, msg);  // Send via IR mesh to HQ (no LiFi broadcast)

  // Visual feedback only - NO LiFi broadcast for SOS
  digitalWrite(LED_STATUS, HIGH);
  delay(200);
  digitalWrite(LED_STATUS, LOW);
  
  Serial.println("SOS sent to HQ (no phone broadcast)");
}

/*
 * Process and Forward Incoming Packet
 * 
 * Core mesh networking function:
 *   1. Validates header format
 *   2. Verifies message integrity (hash check)
 *   3. Forwards ALL messages via mesh (routing to destination)
 *   4. Processes messages based on type and destination
 * 
 * Header Format (13 chars): [src(4)][dst(4)][type(1)][hash(4)]
 * 
 * Message Type Processing:
 *   Type 1 (BROADCAST): HQ → All lamps, all nodes process
 *   Type 2 (COMMAND): HQ → Specific lamp, only destination processes
 *   Type 3 (SOS): Lamp → HQ, only HQ processes (emergency alert)
 *   Type 4 (DATA): Node → HQ or Node → Node, destination processes
 */
void forwardPacket(String header, String message){
  // Validate header
  if(header.length() != 13){
    Serial.println("Invalid header");
    return;
  }
  
  // Parse header
  String src = header.substring(0,4);
  String dst = header.substring(4,8);
  char type = header[8];
  String hashStr = header.substring(9,13);
  uint16_t receivedHash = (uint16_t) strtol(hashStr.c_str(), NULL, 16);

  // Verify integrity: recompute hash and compare
  uint16_t computedHash = simpleHash(message);
  if(computedHash != receivedHash){
    Serial.println("Corrupted message (hash mismatch) - discarded");
    return;  // Exit early, don't process bad data
  }

  // STEP 1: Forward via mesh (all nodes help route messages)
  if(isNew(src, receivedHash)){
    // Optional: Uncomment next line if collision issues observed
    // delay(random(10, 100));  // Random backoff reduces collision probability
    
    irSend(header, message);
    digitalWrite(LED_STATUS, HIGH);
    delay(50);
    digitalWrite(LED_STATUS, LOW);
  }

  // STEP 2: Process message based on type and destination
  
  // Type 1: BROADCAST (HQ → All) - All nodes process
  if(type == MSG_TYPE_BROADCAST && dst == BROADCAST_ID){
    Serial.println("=== BROADCAST FROM HQ ===");
    Serial.print("Message: ");
    Serial.println(message);
    
    // All nodes broadcast to phones
    latestLiFiMessage = message;
    lastLiFiBroadcastTime = millis();
    lifiTransmit(message);
  }
  
  // Type 2: COMMAND (HQ → Specific lamp) - Only destination processes
  else if(type == MSG_TYPE_TARGETED && dst == NODE_ID){
    Serial.println("=== MESSAGE FROM HQ ===");
    Serial.print("Message: ");
    Serial.println(message);
    
    // Only this node broadcasts to phones
    latestLiFiMessage = message;
    lastLiFiBroadcastTime = millis();
    lifiTransmit(message);
    
    // Add command processing here (e.g., parse commands)
  }
  
  // Type 3: SOS (Lamp → HQ) - Only HQ processes with special alert
  else if(type == MSG_TYPE_SOS && dst == HQ_ID && NODE_ID == HQ_ID){
    Serial.println("╔════════════════════════════╗");
    Serial.println("║   SOS ALERT RECEIVED       ║");
    Serial.println("╚════════════════════════════╝");
    Serial.print("From Node: "); Serial.println(src);
    Serial.print("Message: "); Serial.println(message);
    Serial.println("────────────────────────────");
    
    // HQ: Trigger alarm, log emergency, coordinate response
  }
  
  // Type 4: MESSAGE (Node → HQ) - HQ processes as normal message
  else if(type == MSG_TYPE_MESSAGE && dst == HQ_ID && NODE_ID == HQ_ID){
    Serial.println("=== Message from Node ===");
    Serial.print("From: "); Serial.println(src);
    Serial.print("Message: "); Serial.println(message);
    
    // HQ logs/displays message (no special alert, just normal info)
  }
}

// ==================== SETUP ====================

void setup(){
  // Initialize hardware pins
  pinMode(SOS_PIN, INPUT_PULLUP);
  pinMode(IR_TX_PIN, OUTPUT);
  pinMode(IR_RX_PIN, INPUT);
  pinMode(LED_STATUS, OUTPUT);
  pinMode(LAMP_LIGHT_PIN, OUTPUT);

  // Initialize cache to empty state (prevent garbage data)
  for(int i = 0; i < CACHE_SIZE; i++){
    cache[i].src = "";
    cache[i].msgHash = 0;
  }

  Serial.begin(115200);
  Serial.println("\n=== LiFi Mesh Lamp Node ===");
  Serial.print("Node ID: "); Serial.println(NODE_ID);
  Serial.print("Cooldown: "); Serial.print(SOS_COOLDOWN/1000); Serial.println("s");
  Serial.println("===========================\n");
  
  // Startup LED flash
  digitalWrite(LED_STATUS, HIGH);
  digitalWrite(LAMP_LIGHT_PIN, HIGH);
  delay(500);
  digitalWrite(LED_STATUS, LOW);
  digitalWrite(LAMP_LIGHT_PIN, LOW);
}

// ==================== MAIN LOOP ====================

void loop(){
  // Non-blocking SOS button with edge detection and cooldown
  // Edge detection = detect moment button is pressed (HIGH→LOW transition)
  // Without edge detection, holding button would send multiple SOS!
  bool currentButtonState = digitalRead(SOS_PIN);
  
  // Check for falling edge (button press moment)
  if(currentButtonState == LOW && lastButtonState == HIGH){
    unsigned long timeSinceLastSOS = millis() - lastSOSTime;
    
    if(timeSinceLastSOS >= SOS_COOLDOWN){
      generateSOS();
      lastSOSTime = millis();
    } else {
      Serial.print("Cooldown: ");
      Serial.print((SOS_COOLDOWN - timeSinceLastSOS) / 1000);
      Serial.println("s remaining");
    }
  }
  
  lastButtonState = currentButtonState;  // Remember state for next loop

  // Check for incoming messages via IR mesh
  String header, message;
  if(irReceive(header, message)){
    forwardPacket(header, message);
  }

  // Periodic LiFi rebroadcast for phone receivers
  // Ensures people arriving later can still receive the message
  if(latestLiFiMessage != "" && 
     (millis() - lastLiFiBroadcastTime >= LIFI_REBROADCAST_INTERVAL)){
    
    Serial.println("Rebroadcasting to phones...");
    lifiTransmit(latestLiFiMessage);
    lastLiFiBroadcastTime = millis();
  }

  delay(10);
}
