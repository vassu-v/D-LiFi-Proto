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

// ==================== IR COMMUNICATION FUNCTIONS ====================

/*
 * IR Transmission (Node to Node Mesh)
 * Sends header (and optional message) via IR in four directions (clockwise)
 * 
 * Current: Placeholder using Serial output + directional LED sequence
 * Sequence: FRONT → RIGHT → BACK → LEFT (clockwise)
 * To implement real IR: Replace with IRremote library calls
 * 
 * Note: SOS messages are header-only (no message content)
 */
inline void irSend(String header, String message = ""){
  // Array of TX pins in clockwise order: Front, Right, Back, Left
  const int txPins[] = {IR_TX_FRONT, IR_TX_RIGHT, IR_TX_BACK, IR_TX_LEFT};
  const char* dirNames[] = {"FRONT", "RIGHT", "BACK", "LEFT"};
  
  // Debug output
  Serial.print("TX (all directions): ");
  Serial.print(header);
  if(message.length() > 0){
    Serial.print(" | ");
    Serial.print(message);
  } else {
    Serial.print(" (header-only)");
  }
  Serial.println();
  
  // Transmit in all 4 directions sequentially
  for(int i = 0; i < 4; i++){
    // Placeholder: Toggle LED to indicate transmission
    // Real implementation: Send encoded header + message via IR protocol
    digitalWrite(txPins[i], HIGH);
    delayMicroseconds(500);  // Brief pulse
    digitalWrite(txPins[i], LOW);
    
    // Small gap before next direction (unless it's the last one)
    if(i < 3){
      delay(IR_DIRECTION_GAP);
    }
    
    // Debug: show which direction transmitted
    Serial.print("  → ");
    Serial.println(dirNames[i]);
  }
}

/*
 * IR Reception (Node to Node Mesh)
 * Receives header and message via IR in two bursts
 * 
 * Current: Placeholder using Serial input
 * Format: Line 1 = header (13 chars), Line 2 = message
 * 
 * To implement real IR: Replace with IRremote library calls
 */
inline bool irReceive(String &header, String &message){
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

// ==================== LIFI BROADCAST FUNCTIONS ====================

/*
 * LiFi Broadcast (Node to Phones)
 * Broadcasts message to phones via lamp light modulation
 * 
 * Current: Placeholder - flashes LED
 * To implement real LiFi: Add high-speed PWM modulation at kHz frequencies
 */
inline void lifiTransmit(String message){
  Serial.print("LiFi Broadcast: "); 
  Serial.println(message);
  
  // Placeholder: Flash lamp to indicate broadcast
  // Real implementation: Modulate lamp at kHz frequency with encoded data
  digitalWrite(LAMP_LIGHT_PIN, HIGH);
  delay(100);  // Longer pulse to distinguish from IR forwarding
  digitalWrite(LAMP_LIGHT_PIN, LOW);
}

// ==================== PROTOCOL FUNCTIONS ====================

/*
 * Generate SOS Emergency Message
 * Creates Type 3 header-only message and sends to HQ via mesh
 * Does NOT broadcast to phones - only routes to HQ
 * 
 * SOS Format: Header-only, no hash, no message content
 * All SOS are identical, so no deduplication needed beyond source tracking
 */
inline void generateSOS(){
  // Build SOS header: [src][dst][type] = 9 chars (no hash, no message)
  String header = String(NODE_ID) + HQ_ID + MSG_TYPE_SOS;

  // For SOS, we only track by source (no hash needed, all SOS are same)
  isNew(NODE_ID, 0);  // Use hash=0 for all SOS from this node
  
  irSend(header);  // Send header-only, no message content

  // Visual feedback only - NO LiFi broadcast for SOS
  digitalWrite(LED_STATUS, HIGH);
  delay(200);
  digitalWrite(LED_STATUS, LOW);
  
  Serial.println("SOS sent to HQ (header-only, no phone broadcast)");
}

/*
 * Process and Forward Incoming Packet
 * 
 * Core mesh networking function:
 *   1. Validates header format (length varies by type)
 *   2. Verifies message integrity (hash check for types with messages)
 *   3. Forwards messages via mesh (routing)
 *   4. Processes messages based on type and destination
 * 
 * Header Formats:
 *   Type 1,2,4: [src(4)][dst(4)][type(1)][hash(4)] = 13 chars
 *   Type 3 (SOS): [src(4)][dst(4)][type(1)] = 9 chars (header-only)
 */
inline void forwardPacket(String header, String message, 
                          String &latestLiFiMessage, 
                          unsigned long &lastLiFiBroadcastTime){
  // Parse basic header info
  if(header.length() < 9){
    Serial.println("Invalid header (too short)");
    return;
  }
  
  String src = header.substring(0,4);
  String dst = header.substring(4,8);
  char type = header[8];
  
  // Type 3 (SOS) is header-only, no hash validation needed
  if(type == MSG_TYPE_SOS){
    if(header.length() != HEADER_LENGTH_SOS){
      Serial.println("Invalid SOS header length");
      return;
    }
    
    // Forward SOS if new (use hash=0 for all SOS from same source)
    if(isNew(src, 0)){
      irSend(header);  // Forward header-only
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
    irSend(header, message);
    digitalWrite(LED_STATUS, HIGH);
    delay(50);
    digitalWrite(LED_STATUS, LOW);
  }

  // Process based on type and destination
  
  // Type 1: BROADCAST (HQ → All) - Only process if from authorized HQ
  if(type == MSG_TYPE_BROADCAST && dst == BROADCAST_ID && IS_FROM_HQ(src)){
    Serial.println("=== BROADCAST FROM HQ ===");
    Serial.print("From HQ: "); Serial.println(src);
    Serial.print("Message: ");
    Serial.println(message);
    
    latestLiFiMessage = message;
    lastLiFiBroadcastTime = millis();
    lifiTransmit(message);
  }
  
  // Type 2: TARGETED BROADCAST (HQ → Specific lamp) - Only process if from authorized HQ
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
