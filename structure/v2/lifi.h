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
 * Handles BOTH header-only (SOS) and header+message packets
 * 
 * Current: Placeholder using Serial input
 * Format: 
 *   - Single line with 9 chars = SOS header-only
 *   - Line 1 (13 chars) + Line 2 = Standard header + message
 * 
 * To implement real IR: Replace with IRremote library calls
 */
inline bool irReceive(String &header, String &message){
  static bool waitingForMessage = false;
  static String receivedHeader = "";
  
  if (Serial.available()){
    String line = Serial.readStringUntil('\n');
    line.trim();
    
    // Check if this is a header-only SOS packet (9 chars)
    if(line.length() == HEADER_LENGTH_SOS && line[8] == MSG_TYPE_SOS){
      header = line;
      message = "";  // No message for SOS
      Serial.println("RX: SOS header-only packet");
      return true;  // Complete SOS packet received
    }
    
    // Otherwise, handle standard two-burst format
    if(!waitingForMessage){
      // First burst: receive header
      if(line.length() == HEADER_LENGTH_STANDARD){
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
      return true;  // Complete packet received
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

// ==================== PROTOCOL FUNCTIONS ====================

/*
 * Generate SOS Emergency Message
 * Creates Type 3 header-only message and sends to HQ via mesh
 */
inline void generateSOS(){
  String header = String(NODE_ID) + HQ_ID + MSG_TYPE_SOS;

  isNew(NODE_ID, 0);  // Use hash=0 for SOS tracking
  
  irSend(header);  // Send header-only (will be retransmitted 3x in first minute)

  digitalWrite(LED_STATUS, HIGH);
  delay(200);
  digitalWrite(LED_STATUS, LOW);
  
  Serial.println("SOS sent to HQ (header-only, will retransmit 3x in first minute)");
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
  
  String src = header.substring(0,4);
  String dst = header.substring(4,8);
  char type = header[8];
  
  // Type 3 (SOS) is header-only
  if(type == MSG_TYPE_SOS){
    if(header.length() != HEADER_LENGTH_SOS){
      Serial.println("Invalid SOS header length");
      return;
    }
    
    // Forward SOS if new
    if(isNew(src, 0)){
      irSend(header);  // Will be retransmitted automatically
      digitalWrite(LED_STATUS, HIGH);
      delay(50);
      digitalWrite(LED_STATUS, LOW);
    }
    
    // Process if this is HQ
    if(dst == HQ_ID && NODE_ID == HQ_ID){
      Serial.println("╔════════════════════════════╗");
      Serial.println("║   SOS ALERT RECEIVED       ║");
      Serial.println("╚════════════════════════════╝");
      Serial.print("From Node: "); Serial.println(src);
      Serial.println("────────────────────────────");
    }
    return;
  }
  
  // For all other types, validate standard header with hash
  if(header.length() != HEADER_LENGTH_STANDARD){
    Serial.println("Invalid header length");
    return;
  }
  
  String hashStr = header.substring(9,13);
  uint16_t receivedHash = (uint16_t) strtol(hashStr.c_str(), NULL, 16);

  // Verify message integrity
  uint16_t computedHash = simpleHash(message);
  if(computedHash != receivedHash){
    Serial.println("Corrupted message (hash mismatch) - discarded");
    return;
  }

  // Forward if new
  if(isNew(src, receivedHash)){
    irSend(header, message);  // Will be retransmitted automatically
    digitalWrite(LED_STATUS, HIGH);
    delay(50);
    digitalWrite(LED_STATUS, LOW);
  }

  // Process based on type and destination
  
  // Type 1: BROADCAST (HQ → All)
  if(type == MSG_TYPE_BROADCAST && dst == BROADCAST_ID && IS_FROM_HQ(src)){
    Serial.println("=== BROADCAST FROM HQ ===");
    Serial.print("From HQ: "); Serial.println(src);
    Serial.print("Message: ");
    Serial.println(message);
    
    latestLiFiMessage = message;
    lastLiFiBroadcastTime = millis();
    lifiTransmit(message);
  }
  
  // Type 2: TARGETED BROADCAST (HQ → Specific lamp)
  else if(type == MSG_TYPE_TARGETED && dst == NODE_ID && IS_FROM_HQ(src)){
    Serial.println("=== TARGETED BROADCAST FROM HQ ===");
    Serial.print("From HQ: "); Serial.println(src);
    Serial.print("Message: ");
    Serial.println(message);
    Serial.println("Broadcasting to phones in this area...");
    
    latestLiFiMessage = message;
    lastLiFiBroadcastTime = millis();
    lifiTransmit(message);
  }
  
  // Type 4: MESSAGE (Node → HQ)
  else if(type == MSG_TYPE_MESSAGE && dst == HQ_ID && NODE_ID == HQ_ID){
    Serial.println("=== Message from Node ===");
    Serial.print("From: "); Serial.println(src);
    Serial.print("Message: "); Serial.println(message);
  }
}

#endif // LIFI_H
