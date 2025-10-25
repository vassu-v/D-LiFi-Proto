
#include <Arduino.h>
#include "config.h"
#include "lifi.h"

// ==================== GLOBAL VARIABLES ====================

// Cache for message deduplication (defined here, declared extern in config.h)
MsgCache cache[CACHE_SIZE];
int cacheIndex = 0;

// Button state tracking
unsigned long lastSOSTime = 0;
bool lastButtonState = HIGH;  // HIGH = not pressed (INPUT_PULLUP)

// LiFi rebroadcast tracking
String latestLiFiMessage = "";
unsigned long lastLiFiBroadcastTime = 0;

// ==================== UTILITY FUNCTIONS ====================

uint16_t simpleHash(String s){
  uint16_t h = 0;
  for (int i = 0; i < s.length(); i++){
    h = (h * 31) + s[i]; // Polynomial rolling hash
  }
  return h;
}

bool isNew(String src, uint16_t hash){
  // Search cache for matching entry
  for (int i = 0; i < CACHE_SIZE; i++){
    if (cache[i].src == src && cache[i].msgHash == hash){
      return false; // Duplicate found
    }
  }
  
  // Message is new, add to cache
  cache[cacheIndex].src = src;
  cache[cacheIndex].msgHash = hash;
  cacheIndex = (cacheIndex + 1) % CACHE_SIZE; // Circular increment
  
  return true;
}

// ==================== COMMUNICATION FUNCTIONS ====================

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

void lifiTransmit(String message){
  Serial.print("LiFi Broadcast: "); 
  Serial.println(message);
  
  // Placeholder: Flash lamp to indicate broadcast
  // Real implementation: Modulate lamp at kHz frequency with encoded data
  digitalWrite(LAMP_LIGHT_PIN, HIGH);
  delay(100);  // Longer pulse to distinguish from IR forwarding
  digitalWrite(LAMP_LIGHT_PIN, LOW);
}

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

// ==================== PROTOCOL FUNCTIONS ====================

void generateSOS(){
  String msg = SOS_MESSAGE;  // "HELP!"
  uint16_t h = SOS_HASH;     // Pre-computed

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
    return;
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
  
  // Type 1: BROADCAST (HQ → All) - All nodes broadcast to phones
  if(type == MSG_TYPE_BROADCAST && dst == BROADCAST_ID){
    Serial.println("=== BROADCAST FROM HQ ===");
    Serial.print("Message: ");
    Serial.println(message);
    
    latestLiFiMessage = message;
    lastLiFiBroadcastTime = millis();
    lifiTransmit(message);
  }
  
  // Type 2: TARGETED BROADCAST (HQ → Specific lamp)
  else if(type == MSG_TYPE_TARGETED && dst == NODE_ID){
    Serial.println("=== TARGETED BROADCAST FROM HQ ===");
    Serial.print("Message: ");
    Serial.println(message);
    Serial.println("Broadcasting to phones in this area...");
    
    latestLiFiMessage = message;
    lastLiFiBroadcastTime = millis();
    lifiTransmit(message);
  }
  
  // Type 3: SOS (Lamp → HQ) - Only HQ processes with special alert
  else if(type == MSG_TYPE_SOS && dst == HQ_ID && NODE_ID == HQ_ID){
    Serial.println("╔════════════════════════════╗");
    Serial.println("║   SOS ALERT RECEIVED       ║");
    Serial.println("╚════════════════════════════╝");
    Serial.print("From Node: "); Serial.println(src);
    Serial.print("Message: "); Serial.println(message);
    Serial.println("────────────────────────────");
  }
  
  // Type 4: MESSAGE (Node → HQ) - HQ processes as normal message
  else if(type == MSG_TYPE_MESSAGE && dst == HQ_ID && NODE_ID == HQ_ID){
    Serial.println("=== Message from Node ===");
    Serial.print("From: "); Serial.println(src);
    Serial.print("Message: "); Serial.println(message);
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
  
  lastButtonState = currentButtonState;

  // Check for incoming messages via IR mesh
  String header, message;
  if(irReceive(header, message)){
    forwardPacket(header, message);
  }

  // Periodic LiFi rebroadcast for phone receivers
  if(latestLiFiMessage != "" && 
     (millis() - lastLiFiBroadcastTime >= LIFI_REBROADCAST_INTERVAL)){
    
    Serial.println("Rebroadcasting to phones...");
    lifiTransmit(latestLiFiMessage);
    lastLiFiBroadcastTime = millis();
  }

  delay(10);
}
