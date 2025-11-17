#ifndef LIFI_H
#define LIFI_H

#include <Arduino.h>
#include "config.h"
#include "ir.h"  // IR communication layer

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
  #if DEBUG_CACHE
    Serial.print(">>> CACHE: Checking (src='");
    Serial.print(src);
    Serial.print("', hash=0x");
    Serial.print(hash, HEX);
    Serial.println(")");
  #endif
  
  // Search cache for matching entry
  for (int i = 0; i < CACHE_SIZE; i++){
    if (cache[i].src == src && cache[i].msgHash == hash){
      #if DEBUG_CACHE
        Serial.println(">>> CACHE: HIT - Message is duplicate (not forwarding)");
        Serial.print("    Found at cache slot ");
        Serial.println(i);
      #endif
      return false; // Duplicate found
    }
  }
  
  #if DEBUG_CACHE
    Serial.println(">>> CACHE: MISS - Message is NEW");
    Serial.print("    Adding to cache slot ");
    Serial.println(cacheIndex);
  #endif
  
  // Message is new, add to cache
  cache[cacheIndex].src = src;
  cache[cacheIndex].msgHash = hash;
  cacheIndex = (cacheIndex + 1) % CACHE_SIZE; // Circular increment
  
  return true;
}

// Forward declaration for retransmit queue
inline void irSendRaw(String header, String message);

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
      
      // Resend via IR
      irSendRaw(retransmitQueue[i].header, retransmitQueue[i].message);
      
      retransmitQueue[i].sentCount++;
    }
  }
}

// ==================== IR COMMUNICATION FUNCTIONS ====================

/*
 * Raw IR Transmission (used internally by retransmit and initial send)
 * Sends header (and optional message) to ALL 4 directions sequentially
 * Uses IRremote library with pin parameter for multi-directional TX
 */
inline void irSendRaw(String header, String message = ""){
  const int txPins[] = {IR_TX_FRONT, IR_TX_RIGHT, IR_TX_BACK, IR_TX_LEFT};
  const char* dirNames[] = {"FRONT", "RIGHT", "BACK", "LEFT"};
  
  Serial.println("╔════════════════════════════════════╗");
  Serial.println("║   IR TRANSMISSION (4 DIRECTIONS)   ║");
  Serial.println("╚════════════════════════════════════╝");
  Serial.print("Header: ");
  Serial.println(header);
  if(message.length() > 0){
    Serial.print("Message: ");
    Serial.println(message);
  } else {
    Serial.println("Message: (none - header-only)");
  }
  
  #if DEBUG_IR_RX
    Serial.println(">>> IR RX: STOPPING receiver for transmission...");
  #endif
  
  // Stop receiver during entire transmission sequence
  IrReceiver.stop();
  
  #if DEBUG_TIMING
    unsigned long txStartTime = millis();
  #endif
  
  // Transmit to all 4 directions sequentially
  for(int i = 0; i < 4; i++){
    Serial.println("────────────────────────────────────");
    Serial.print("Direction ");
    Serial.print(i + 1);
    Serial.print("/4: ");
    Serial.println(dirNames[i]);
    
    #if DEBUG_TIMING
      unsigned long dirStartTime = millis();
    #endif
    
    // Send header with space delimiter
    String headerWithDelim = header + " ";
    irSendString(headerWithDelim.c_str(), txPins[i]);
    
    // Send message if present
    if(message.length() > 0){
      #if DEBUG_TIMING
        Serial.println(">>> Delay 50ms before message...");
      #endif
      delay(50);  // Small gap between header and message
      
      String messageWithDelim = message + " ";
      irSendString(messageWithDelim.c_str(), txPins[i]);
    }
    
    #if DEBUG_TIMING
      unsigned long dirDuration = millis() - dirStartTime;
      Serial.print(">>> Direction transmission time: ");
      Serial.print(dirDuration);
      Serial.println("ms");
    #endif
    
    // Gap before next direction (except after last one)
    if(i < 3){
      #if DEBUG_TIMING
        Serial.print(">>> Delay ");
        Serial.print(IR_DIRECTION_GAP);
        Serial.println("ms before next direction...");
      #endif
      delay(IR_DIRECTION_GAP);
    }
  }
  
  #if DEBUG_TIMING
    unsigned long txTotalTime = millis() - txStartTime;
    Serial.println("────────────────────────────────────");
    Serial.print(">>> Total transmission time: ");
    Serial.print(txTotalTime);
    Serial.println("ms");
  #endif
  
  #if DEBUG_IR_RX
    Serial.println(">>> IR RX: RESTARTING receiver...");
  #endif
  
  // Resume receiver after all transmissions complete
  IrReceiver.start();
  
  #if DEBUG_IR_RX
    Serial.println(">>> IR RX: Receiver ACTIVE again");
  #endif
  
  Serial.println("════════════════════════════════════");
  Serial.println();
}

/*
 * IR Transmission (Node to Node Mesh)
 * Sends header (and optional message) via IR + adds to retransmit queue
 * 
 * This is the public function - it handles both initial send and queuing
 */
inline void irSend(String header, String message = ""){
  // Send immediately to all 4 directions
  irSendRaw(header, message);
  
  // Add to retransmit queue for redundancy in first minute
  addToRetransmitQueue(header, message);
}

/*
 * IR Reception (Node to Node Mesh)
 * Handles BOTH header-only (SOS) and header+message packets
 * Uses IRremote library for actual reception
 * 
 * Format: 
 *   - Single segment with 9 chars = SOS header-only
 *   - Segment 1 (13 chars) + Segment 2 = Standard header + message
 * 
 * Includes timeout protection: if message segment doesn't arrive within
 * IR_MESSAGE_TIMEOUT, state resets to prevent misinterpretation of future packets
 */
inline bool irReceive(String &header, String &message){
  static bool waitingForMessage = false;
  static String receivedHeader = "";
  static unsigned long headerReceivedTime = 0;
  
  String line;
  if(irReceiveString(line)){
    line.trim();
    
    // Check if this is a header-only SOS packet (9 chars)
    if(line.length() == HEADER_LENGTH_SOS && line[8] == MSG_TYPE_SOS){
      header = line;
      message = "";  // No message for SOS
      Serial.println("RX IR: SOS header-only packet");
      
      // Reset waiting state if we were waiting for a different message
      if(waitingForMessage){
        Serial.println("Warning: Previous message segment lost, resetting");
        waitingForMessage = false;
        receivedHeader = "";
      }
      
      return true;  // Complete SOS packet received
    }
    
    // Otherwise, handle standard two-segment format
    if(!waitingForMessage){
      // First segment: receive header
      if(line.length() == HEADER_LENGTH_STANDARD){
        receivedHeader = line;
        waitingForMessage = true;
        headerReceivedTime = millis();  // Record time for timeout check
        Serial.println("RX IR: Header received, waiting for message...");
      }
      return false;
    } else {
      // Second segment: receive message
      header = receivedHeader;
      message = line;
      waitingForMessage = false;
      receivedHeader = "";
      Serial.println("RX IR: Message received (complete packet)");
      return true;  // Complete packet received
    }
  }
  
  // Timeout check: if waiting too long for message segment, reset state
  if(waitingForMessage && (millis() - headerReceivedTime > IR_MESSAGE_TIMEOUT)){
    Serial.println("RX IR: Message segment timeout, resetting state");
    waitingForMessage = false;
    receivedHeader = "";
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
  Serial.println();
  Serial.println("╔════════════════════════════════════╗");
  Serial.println("║      SOS BUTTON PRESSED!           ║");
  Serial.println("╚════════════════════════════════════╝");
  
  String header = String(NODE_ID) + HQ_ID + MSG_TYPE_SOS;
  
  Serial.print("Generating SOS header: ");
  Serial.println(header);
  Serial.print("Length: ");
  Serial.print(header.length());
  Serial.println(" chars (header-only, no message)");

  isNew(NODE_ID, 0);  // Use hash=0 for SOS tracking
  
  #if DEBUG_LED
    Serial.println(">>> LED: Turning ON for SOS indication...");
  #endif
  
  LED_ON();
  
  irSend(header);  // Send header-only to all 4 directions
  // Note: LED will turn off naturally during IR transmission (receiver stop/start cycle)
  
  LED_OFF();
  
  Serial.println("✓ SOS transmitted to HQ via mesh");
  Serial.println("════════════════════════════════════");
  Serial.println();
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
      #if DEBUG_LED
        Serial.println(">>> LED: Brief blink for SOS forward");
      #endif
      
      LED_ON();
      irSend(header);  // Will be retransmitted automatically
      LED_OFF();  // Turn off immediately (transmission handles LED)
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
    #if DEBUG_LED
      Serial.println(">>> LED: Brief blink for message forward");
    #endif
    
    LED_ON();
    irSend(header, message);  // Will be retransmitted automatically
    LED_OFF();  // Turn off immediately (transmission handles LED)
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
