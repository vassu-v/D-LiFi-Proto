#ifndef LIFI_H
#define LIFI_H

#include <Arduino.h>
#include "config.h"

// ==================== UTILITY FUNCTIONS ====================

/*
 * Simple Rolling Hash Function
 * Computes 16-bit hash for message deduplication and integrity verification
 */
inline uint16_t simpleHash(String s){
  uint16_t h = 0;
  for (int i = 0; i < s.length(); i++){
    h = (h * 31) + s[i]; // Polynomial rolling hash
  }
  return h;
}

/*
 * Check if Message is New (Not in Cache)
 * Returns true if new, false if duplicate
 * Automatically adds new messages to cache
 */
inline bool isNew(String src, uint16_t hash){
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

// ==================== RETRANSMISSION QUEUE MANAGEMENT ====================

/*
 * Add Message to Retransmission Queue
 * Messages will be sent RETRANSMIT_COUNT times over the first minute
 */
inline void addToRetransmitQueue(String header, String message = ""){
  // Find empty slot
  for(int i = 0; i < RETRANSMIT_QUEUE_SIZE; i++){
    if(!retransmitQueue[i].active){
      retransmitQueue[i].header = header;
      retransmitQueue[i].message = message;
      retransmitQueue[i].firstSentTime = millis();
      retransmitQueue[i].sentCount = 1;  // First transmission already done
      retransmitQueue[i].active = true;
      
      Serial.print("Added to retransmit queue (slot ");
      Serial.print(i);
      Serial.println(")");
      return;
    }
  }
  Serial.println("Warning: Retransmit queue full!");
}

/*
 * Process Retransmission Queue
 * Called every loop iteration to check if any messages need resending
 */
inline void processRetransmitQueue(){
  unsigned long now = millis();
  
  for(int i = 0; i < RETRANSMIT_QUEUE_SIZE; i++){
    if(!retransmitQueue[i].active) continue;
    
    unsigned long elapsed = now - retransmitQueue[i].firstSentTime;
    
    // Check if redundancy window expired (1 minute passed)
    if(elapsed > REDUNDANCY_WINDOW){
      retransmitQueue[i].active = false;  // Deactivate slot
      Serial.print("Retransmit complete for slot ");
      Serial.println(i);
      continue;
    }
    
    // Check if it's time for next retransmission
    unsigned long nextSendTime = retransmitQueue[i].sentCount * RETRANSMIT_INTERVAL;
    
    if(elapsed >= nextSendTime && retransmitQueue[i].sentCount < RETRANSMIT_COUNT){
      // Time to retransmit!
      Serial.print("Retransmit #");
      Serial.print(retransmitQueue[i].sentCount + 1);
      Serial.print(" for slot ");
      Serial.println(i);
      
      // Resend via IR (external function, declared below)
      irSendRaw(retransmitQueue[i].header, retransmitQueue[i].message);
      
      retransmitQueue[i].sentCount++;
    }
  }
}

// ==================== IR COMMUNICATION FUNCTIONS ====================

/*
 * Raw IR Transmission (used internally by retransmit and initial send)
 * Sends header (and optional message) via IR in four directions
 */
inline void irSendRaw(String header, String message = ""){
  const int txPins[] = {IR_TX_FRONT, IR_TX_RIGHT, IR_TX_BACK, IR_TX_LEFT};
  const char* dirNames[] = {"FRONT", "RIGHT", "BACK", "LEFT"};
  
  // Transmit in all 4 directions sequentially
  for(int i = 0; i < 4; i++){
    digitalWrite(txPins[i], HIGH);
    delayMicroseconds(500);
    digitalWrite(txPins[i], LOW);
    
    if(i < 3){
      delay(IR_DIRECTION_GAP);
    }
  }
}

/*
 * IR Transmission (Node to Node Mesh)
 * Sends header (and optional message) via IR + adds to retransmit queue
 * 
 * This is the public function - it handles both initial send and queuing
 */
inline void irSend(String header, String message = ""){
  Serial.print("TX (all directions): ");
  Serial.print(header);
  if(message.length() > 0){
    Serial.print(" | ");
    Serial.print(message);
  } else {
    Serial.print(" (header-only)");
  }
  Serial.println();
  
  // Send immediately
  irSendRaw(header, message);
  
  // Add to retransmit queue for redundancy in first minute
  addToRetransmitQueue(header, message);
}

/*
 * IR Reception (Node to Node Mesh)
 * Handles multiple header formats:
 *   - 9 chars: INIT (Type 0)
 *   - 11 chars: SOS (Type 3)
 *   - 13 chars: Broadcast/Targeted (Type 1/2) + expects message
 *   - 15 chars: Message (Type 4) + expects message
 * 
 * Current: Placeholder using Serial input
 * To implement real IR: Replace with IRremote library calls
 */
inline bool irReceive(String &header, String &message){
  static bool waitingForMessage = false;
  static String receivedHeader = "";
  
  if (Serial.available()){
    String line = Serial.readStringUntil('\n');
    line.trim();
    
    // Check for header-only packets (INIT or SOS)
    if(line.length() == HEADER_LENGTH_INIT && line[8] == MSG_TYPE_INIT){
      header = line;
      message = "";
      Serial.println("RX: INIT header-only packet");
      return true;
    }
    
    if(line.length() == HEADER_LENGTH_SOS && line[8] == MSG_TYPE_SOS){
      header = line;
      message = "";
      Serial.println("RX: SOS header-only packet");
      return true;
    }
    
    // Handle two-burst packets (Types 1, 2, 4)
    if(!waitingForMessage){
      // First burst: receive header
      if(line.length() == HEADER_LENGTH_STANDARD || line.length() == HEADER_LENGTH_MESSAGE){
        receivedHeader = line;
        waitingForMessage = true;
        Serial.println("RX Burst 1 (Header) received, waiting for message...");
      }
      return false;
    } else {
      // Second burst: receive message
      header = receivedHeader;
      message = line;
      waitingForMessage = false;
      Serial.println("RX Burst 2 (Message) received");
      return true;
    }
  }
  return false;
}

// ==================== LIFI BROADCAST FUNCTIONS ====================

/*
 * LiFi Broadcast (Node to Phones)
 * Broadcasts message to phones via lamp light modulation
 */
inline void lifiTransmit(String message){
  Serial.print("LiFi Broadcast: "); 
  Serial.println(message);
  
  digitalWrite(LAMP_LIGHT_PIN, HIGH);
  delay(100);
  digitalWrite(LAMP_LIGHT_PIN, LOW);
}

// ==================== GRADIENT SYSTEM FUNCTIONS ====================

/*
 * Process INIT Message
 * Updates node's hop distance and forwards INIT with incremented hop
 */
inline void processInit(String header){
  String src = header.substring(0, 4);
  String initID = header.substring(4, 6);
  String hopStr = header.substring(6, 8);
  uint8_t receivedHop = hopStr.toInt();
  
  Serial.println("=== INIT MESSAGE RECEIVED ===");
  Serial.print("From: "); Serial.println(src);
  Serial.print("ID: "); Serial.println(initID);
  Serial.print("Hop: "); Serial.println(receivedHop);
  
  // Check if this is a new INIT ID or an update to existing one
  if(initID == lastInitID){
    // Same ID, update hop only if smaller
    if(receivedHop < myHop - 1){
      myHop = receivedHop + 1;
      Serial.print("Updated myHop to: "); Serial.println(myHop);
    } else {
      Serial.println("No hop update (received hop not smaller)");
    }
  } else {
    // New INIT ID, replace everything
    lastInitID = initID;
    myHop = receivedHop + 1;
    Serial.print("New INIT ID detected! Updated myHop to: "); Serial.println(myHop);
  }
  
  // Forward INIT with incremented hop (spreads outward)
  uint8_t newHop = receivedHop + 1;
  char newHopStr[3];
  sprintf(newHopStr, "%02d", newHop);
  
  String newHeader = src + initID + String(newHopStr) + MSG_TYPE_INIT;
  irSend(newHeader);  // Will be retransmitted automatically
  
  Serial.println("INIT forwarded with hop=" + String(newHop));
  Serial.println("=============================\n");
}

// ==================== PROTOCOL FUNCTIONS ====================

/*
 * Generate SOS Emergency Message
 * Creates Type 3 header-only message with current hop and sends to HQ
 */
inline void generateSOS(){
  char hopStr[3];
  sprintf(hopStr, "%02d", myHop);
  
  String header = String(NODE_ID) + HQ_ID + MSG_TYPE_SOS + String(hopStr);

  isNew(NODE_ID, 0);  // Use hash=0 for SOS tracking
  
  irSend(header);  // Send header-only (will be retransmitted 3x in first minute)

  digitalWrite(LED_STATUS, HIGH);
  delay(200);
  digitalWrite(LED_STATUS, LOW);
  
  Serial.print("SOS sent to HQ with hop=");
  Serial.println(myHop);
}

/*
 * Process and Forward Incoming Packet
 */
inline void forwardPacket(String header, String message, 
                          String &latestLiFiMessage, 
                          unsigned long &lastLiFiBroadcastTime){
  if(header.length() < 9){
    Serial.println("Invalid header (too short)");
    return;
  }
  
  String src = header.substring(0, 4);
  String dst = header.substring(4, 8);
  char type = header[8];
  
  // ===== Type 0: INIT - Process gradient update =====
  if(type == MSG_TYPE_INIT && header.length() == HEADER_LENGTH_INIT){
    processInit(header);
    return;
  }
  
  // ===== Type 3: SOS - Header-only with gradient =====
  if(type == MSG_TYPE_SOS && header.length() == HEADER_LENGTH_SOS){
    String hopStr = header.substring(9, 11);
    uint8_t msgHop = hopStr.toInt();
    
    Serial.print("SOS received from "); Serial.print(src);
    Serial.print(" with hop="); Serial.println(msgHop);
    
    // Gradient check: only forward if we're close enough
    if(myHop <= msgHop + GRADIENT_TOLERANCE){
      if(isNew(src, 0)){
        // Calculate new hop (decrement toward HQ, floor at 0)
        uint8_t newHop = (msgHop > 0) ? (msgHop - 1) : 0;
        
        char newHopStr[3];
        sprintf(newHopStr, "%02d", newHop);
        String newHeader = src + dst + type + String(newHopStr);
        
        Serial.print("Gradient OK, forwarding with hop="); Serial.println(newHop);
        irSend(newHeader);
        
        digitalWrite(LED_STATUS, HIGH);
        delay(50);
        digitalWrite(LED_STATUS, LOW);
      }
    } else {
      Serial.print("Gradient check failed: myHop=");
      Serial.print(myHop);
      Serial.print(" > msgHop+K=");
      Serial.println(msgHop + GRADIENT_TOLERANCE);
    }
    
    // Process if this is HQ
    if(dst == HQ_ID && NODE_ID == HQ_ID){
      Serial.println("╔════════════════════════════╗");
      Serial.println("║   SOS ALERT RECEIVED       ║");
      Serial.println("╚════════════════════════════╝");
      Serial.print("From Node: "); Serial.println(src);
      Serial.print("Distance: "); Serial.print(msgHop); Serial.println(" hops");
      Serial.println("────────────────────────────");
    }
    return;
  }
  
  // ===== Type 4: MESSAGE - Standard message with gradient =====
  if(type == MSG_TYPE_MESSAGE && header.length() == HEADER_LENGTH_MESSAGE){
    String hashStr = header.substring(9, 13);
    String hopStr = header.substring(13, 15);
    uint16_t receivedHash = (uint16_t) strtol(hashStr.c_str(), NULL, 16);
    uint8_t msgHop = hopStr.toInt();
    
    // Verify message integrity
    uint16_t computedHash = simpleHash(message);
    if(computedHash != receivedHash){
      Serial.println("Corrupted message (hash mismatch) - discarded");
      return;
    }
    
    Serial.print("Message received from "); Serial.print(src);
    Serial.print(" with hop="); Serial.println(msgHop);
    
    // Gradient check
    if(myHop <= msgHop + GRADIENT_TOLERANCE){
      if(isNew(src, receivedHash)){
        // Calculate new hop
        uint8_t newHop = (msgHop > 0) ? (msgHop - 1) : 0;
        
        char newHopStr[3];
        sprintf(newHopStr, "%02d", newHop);
        String newHeader = src + dst + type + hashStr + String(newHopStr);
        
        Serial.print("Gradient OK, forwarding with hop="); Serial.println(newHop);
        irSend(newHeader, message);
        
        digitalWrite(LED_STATUS, HIGH);
        delay(50);
        digitalWrite(LED_STATUS, LOW);
      }
    } else {
      Serial.print("Gradient check failed: myHop=");
      Serial.print(myHop);
      Serial.print(" > msgHop+K=");
      Serial.println(msgHop + GRADIENT_TOLERANCE);
    }
    
    // Process if this is HQ
    if(dst == HQ_ID && NODE_ID == HQ_ID){
      Serial.println("=== Message from Node ===");
      Serial.print("From: "); Serial.println(src);
      Serial.print("Distance: "); Serial.print(msgHop); Serial.println(" hops");
      Serial.print("Message: "); Serial.println(message);
    }
    return;
  }
  
  // ===== Type 1/2: BROADCAST/TARGETED - No gradient, normal forwarding =====
  if(header.length() == HEADER_LENGTH_STANDARD){
    String hashStr = header.substring(9, 13);
    uint16_t receivedHash = (uint16_t) strtol(hashStr.c_str(), NULL, 16);
    
    // Verify integrity
    uint16_t computedHash = simpleHash(message);
    if(computedHash != receivedHash){
      Serial.println("Corrupted message (hash mismatch) - discarded");
      return;
    }
    
    // Forward if new (no gradient check for HQ broadcasts)
    if(isNew(src, receivedHash)){
      irSend(header, message);
      digitalWrite(LED_STATUS, HIGH);
      delay(50);
      digitalWrite(LED_STATUS, LOW);
    }
    
    // Type 1: BROADCAST (HQ → All)
    if(type == MSG_TYPE_BROADCAST && dst == BROADCAST_ID && IS_FROM_HQ(src)){
      Serial.println("=== BROADCAST FROM HQ ===");
      Serial.print("From HQ: "); Serial.println(src);
      Serial.print("Message: "); Serial.println(message);
      
      latestLiFiMessage = message;
      lastLiFiBroadcastTime = millis();
      lifiTransmit(message);
    }
    
    // Type 2: TARGETED BROADCAST (HQ → Specific lamp)
    else if(type == MSG_TYPE_TARGETED && dst == NODE_ID && IS_FROM_HQ(src)){
      Serial.println("=== TARGETED BROADCAST FROM HQ ===");
      Serial.print("From HQ: "); Serial.println(src);
      Serial.print("Message: "); Serial.println(message);
      Serial.println("Broadcasting to phones in this area...");
      
      latestLiFiMessage = message;
      lastLiFiBroadcastTime = millis();
      lifiTransmit(message);
    }
    return;
  }
  
  Serial.println("Unknown message format");
}

#endif // LIFI_H
