
#include <Arduino.h>

// ==================== CONFIGURATION ====================

// Unique ID for this node (4 characters, alphanumeric)
// IMPORTANT: Change this for each node! Examples: "102a", "203b", "304c"
#define NODE_ID      "102a"

// Reserved ID for broadcast messages (all nodes receive)
#define BROADCAST_ID "FFFF"

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

// Cache size for message deduplication (number of recent messages remembered)
#define CACHE_SIZE   3

// Pre-computed hash for SOS messages (all SOS send "HELP!")
// Avoids recomputing same hash every time
#define SOS_MESSAGE "HELP!"
#define SOS_HASH 0x28F9  // simpleHash("HELP!") = 0x28F9

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
  // Burst 1: Send header
  Serial.print("TX Burst 1 (Header): "); 
  Serial.println(header);
  digitalWrite(IR_TX_PIN, HIGH);
  delay(25);
  digitalWrite(IR_TX_PIN, LOW);
  delay(10);  // Inter-burst gap
  
  // Burst 2: Send message
  Serial.print("TX Burst 2 (Message): "); 
  Serial.println(message);
  digitalWrite(LAMP_LIGHT_PIN, HIGH);
  delay(25);
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
 * Creates and broadcasts emergency alert with pre-computed hash
 */
void generateSOS(){
  String msg = SOS_MESSAGE;  // "HELP!"
  uint16_t h = SOS_HASH;     // Pre-computed, no need to calculate

  char hashStr[5];
  sprintf(hashStr, "%04X", h);

  String header = String(NODE_ID) + BROADCAST_ID + "3" + String(hashStr);

  isNew(NODE_ID, h);  // Add to cache to prevent re-forwarding own SOS
  irSend(header, msg);

  // Visual feedback
  digitalWrite(LED_STATUS, HIGH);
  delay(200);
  digitalWrite(LED_STATUS, LOW);
}

/*
 * Process and Forward Incoming Packet
 * 
 * Core mesh networking function:
 *   1. Validates header format
 *   2. Verifies message integrity (hash check)
 *   3. Deduplicates and forwards new messages with random backoff
 *   4. Processes messages addressed to this node
 * 
 * Header Format (13 chars): [src(4)][dst(4)][type(1)][hash(4)]
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
  // If hashes don't match, message was corrupted - discard it
  // If they match, message is good - continue processing
  uint16_t computedHash = simpleHash(message);
  if(computedHash != receivedHash){
    Serial.println("Corrupted message (hash mismatch) - discarded");
    return;  // Exit early, don't process bad data
  }

  // Message integrity verified, now forward if new
  if(isNew(src, receivedHash)){
    // Optional: Uncomment next line if collision issues observed
    // delay(random(10, 100));  // Random backoff reduces collision probability
    
    irSend(header, message);

    digitalWrite(LED_STATUS, HIGH);
    digitalWrite(LAMP_LIGHT_PIN, HIGH);
    delay(50);
    digitalWrite(LED_STATUS, LOW);
    digitalWrite(LAMP_LIGHT_PIN, LOW);
  }

  // Process if for this node
  if(dst == NODE_ID || dst == BROADCAST_ID){
    Serial.print("From "); 
    Serial.print(src);
    Serial.print(": "); 
    Serial.println(message);
  }
}

// ==================== SETUP ====================

void setup(){
  pinMode(SOS_PIN, INPUT_PULLUP);
  pinMode(IR_TX_PIN, OUTPUT);
  pinMode(IR_RX_PIN, INPUT);
  pinMode(LED_STATUS, OUTPUT);
  pinMode(LAMP_LIGHT_PIN, OUTPUT);

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

  // Check for incoming messages
  String header, message;
  if(irReceive(header, message)){
    forwardPacket(header, message);
  }

  delay(10);
}
