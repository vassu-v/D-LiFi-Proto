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
      
      #if DEBUG_RETRANSMIT
        Serial.print(">>> RETRANSMIT: Added to queue (slot ");
        Serial.print(i);
        Serial.println(")");
      #endif
      return;
    }
  }
  Serial.println(">>> RETRANSMIT: Warning - Queue full!");
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
      #if DEBUG_RETRANSMIT
        Serial.print(">>> RETRANSMIT: Complete for slot ");
        Serial.println(i);
      #endif
      continue;
    }
    
    // Check if it's time for next retransmission
    unsigned long nextSendTime = retransmitQueue[i].sentCount * RETRANSMIT_INTERVAL;
    
    if(elapsed >= nextSendTime && retransmitQueue[i].sentCount < RETRANSMIT_COUNT){
      // Time to retransmit!
      #if DEBUG_RETRANSMIT
        Serial.print(">>> RETRANSMIT: #");
        Serial.print(retransmitQueue[i].sentCount + 1);
        Serial.print(" for slot ");
        Serial.println(i);
      #endif
      
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
 * Handles ALL header formats with timeout protection:
 *   - 9 chars: INIT (Type 0)
 *   - 11 chars: SOS (Type 3)
 *   - 13 chars: Broadcast/Targeted (Type 1/2) + expects message
 *   - 15 chars: Message (Type 4) + expects message
 */
inline bool irReceive(String &header, String &message){
  static bool waitingForMessage = false;
  static String receivedHeader = "";
  static unsigned long headerReceivedTime = 0;
  
  String line;
  if(irReceiveString(line)){
    line.trim();
    
    // Check for header-only INIT packet (9 chars, Type 0)
    if(line.length() == HEADER_LENGTH_INIT && line[8] == MSG_TYPE_INIT){
      header = line;
      message = "";
      Serial.println("RX IR: INIT header-only packet");
      
      // Reset waiting state if we were waiting for a different message
      if(waitingForMessage){
        Serial.println("Warning: Previous message segment lost, resetting");
        waitingForMessage = false;
        receivedHeader = "";
      }
      
      return true;
    }
    
    // Check for header-only SOS packet (11 chars, Type 3)
    if(line.length() == HEADER_LENGTH_SOS && line[8] == MSG_TYPE_SOS){
      header = line;
      message = "";
      Serial.println("RX IR: SOS header-only packet");
      
      // Reset waiting state if we were waiting for a different message
      if(waitingForMessage){
        Serial.println("Warning: Previous message segment lost, resetting");
        waitingForMessage = false;
        receivedHeader = "";
      }
      
      return true;
    }
    
    // Otherwise, handle standard two-segment format
    if(!waitingForMessage){
      // First segment: receive header
      if(line.length() == HEADER_LENGTH_STANDARD || line.length() == HEADER_LENGTH_MESSAGE){
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
  Serial.print(">>> LiFi: Broadcasting to phones: "); 
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
  
  Serial.println();
  Serial.println("╔════════════════════════════════════╗");
  Serial.println("║      INIT MESSAGE RECEIVED         ║");
  Serial.println("╚════════════════════════════════════╝");
  Serial.print("From: "); Serial.println(src);
  Serial.print("ID: "); Serial.println(initID);
  Serial.print("Received Hop: "); Serial.println(receivedHop);
  
  // Check if this is a new INIT ID or an update to existing one
  if(initID == lastInitID){
    // Same ID, update hop only if smaller
    if(receivedHop < myHop - 1){
      uint8_t oldHop = myHop;
      myHop = receivedHop + 1;
      
      #if DEBUG_GRADIENT
        Serial.print(">>> GRADIENT: myHop updated ");
        Serial.print(oldHop);
        Serial.print(" → ");
        Serial.println(myHop);
      #endif
    } else {
      #if DEBUG_GRADIENT
        Serial.print(">>> GRADIENT: No update (received=");
        Serial.print(receivedHop);
        Serial.print(", myHop=");
        Serial.print(myHop);
        Serial.println(")");
      #endif
    }
  } else {
    // New INIT ID, replace everything
    lastInitID = initID;
    myHop = receivedHop + 1;
    
    #if DEBUG_GRADIENT
      Serial.println(">>> GRADIENT: NEW INIT ID detected!");
      Serial.print("    lastInitID = '");
      Serial.print(lastInitID);
      Serial.println("'");
      Serial.print("    myHop = ");
      Serial.println(myHop);
    #endif
  }
  
  // Forward INIT with incremented hop (spreads outward)
  uint8_t newHop = receivedHop + 1;
  char newHopStr[3];
  sprintf(newHopStr, "%02d", newHop);
  
  String newHeader = src + initID + String(newHopStr) + MSG_TYPE_INIT;
  
  Serial.print("Forwarding INIT with hop=");
  Serial.println(newHop);
  
  irSend(newHeader);  // Will be retransmitted automatically
  
  Serial.println("════════════════════════════════════");
  Serial.println();
}

// ==================== PROTOCOL FUNCTIONS ====================

/*
 * Generate SOS Emergency Message
 * Creates Type 3 header-only message with current hop and sends to HQ
 */
inline void generateSOS(){
  Serial.println();
  Serial.println("╔════════════════════════════════════╗");
  Serial.println("║      SOS BUTTON PRESSED!           ║");
  Serial.println("╚════════════════════════════════════╝");
  
  char hopStr[3];
  sprintf(hopStr, "%02d", myHop);
  
  String header = String(NODE_ID) + HQ_ID + MSG_TYPE_SOS + String(hopStr);
  
  Serial.print("Generating SOS header: ");
  Serial.println(header);
  Serial.print("Length: ");
  Serial.print(header.length());
  Serial.println(" chars (header-only, with hop)");
  Serial.print("My Hop: ");
  Serial.println(myHop);

  isNew(NODE_ID, 0);  // Use hash=0 for SOS tracking
  
  #if DEBUG_LED
    Serial.println(">>> LED: Turning ON for SOS indication...");
  #endif
  
  LED_ON();
  
  irSend(header);  // Send header-only to all 4 directions
  
  LED_OFF();
  
  Serial.println("✓ SOS transmitted to HQ via gradient mesh");
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
    Serial.println(">>> ERROR: Invalid header (too short)");
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
    
    Serial.println();
    Serial.println("╔════════════════════════════════════╗");
    Serial.println("║      SOS PACKET RECEIVED           ║");
    Serial.println("╚════════════════════════════════════╝");
    Serial.print("From: "); Serial.println(src);
    Serial.print("Message Hop: "); Serial.println(msgHop);
    Serial.print("My Hop: "); Serial.println(myHop);
    
    // Gradient check: only forward if we're close enough
    if(myHop <= msgHop + GRADIENT_TOLERANCE){
      #if DEBUG_GRADIENT
        Serial.print(">>> GRADIENT: CHECK PASSED (myHop=");
        Serial.print(myHop);
        Serial.print(" <= msgHop+K=");
        Serial.print(msgHop + GRADIENT_TOLERANCE);
        Serial.println(")");
      #endif
      
      if(isNew(src, 0)){
        // Calculate new hop (decrement toward HQ, floor at 0)
        uint8_t newHop = (msgHop > 0) ? (msgHop - 1) : 0;
        
        char newHopStr[3];
        sprintf(newHopStr, "%02d", newHop);
        String newHeader = src + dst + type + String(newHopStr);
        
        Serial.print("Forwarding SOS with hop=");
        Serial.println(newHop);
        
        #if DEBUG_LED
          Serial.println(">>> LED: Brief blink for SOS forward");
        #endif
        
        LED_ON();
        irSend(newHeader);
        LED_OFF();
      }
    } else {
      #if DEBUG_GRADIENT
        Serial.print(">>> GRADIENT: CHECK FAILED (myHop=");
        Serial.print(myHop);
        Serial.print(" > msgHop+K=");
        Serial.print(msgHop + GRADIENT_TOLERANCE);
        Serial.println(")");
        Serial.println(">>> GRADIENT: NOT forwarding (too far downstream)");
      #endif
    }
    
    // Process if this is HQ
    if(dst == HQ_ID && NODE_ID == HQ_ID){
      Serial.println("╔════════════════════════════╗");
      Serial.println("║   SOS ALERT AT HQ          ║");
      Serial.println("╚════════════════════════════╝");
      Serial.print("From Node: "); Serial.println(src);
      Serial.print("Distance: "); Serial.print(msgHop); Serial.println(" hops");
      Serial.println("────────────────────────────");
    }
    
    Serial.println("════════════════════════════════════");
    Serial.println();
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
      Serial.println(">>> ERROR: Corrupted message (hash mismatch) - discarded");
      return;
    }
    
    Serial.println();
    Serial.println("╔════════════════════════════════════╗");
    Serial.println("║     MESSAGE PACKET RECEIVED        ║");
    Serial.println("╚════════════════════════════════════╝");
    Serial.print("From: "); Serial.println(src);
    Serial.print("Message Hop: "); Serial.println(msgHop);
    Serial.print("My Hop: "); Serial.println(myHop);
    
    // Gradient check
    if(myHop <= msgHop + GRADIENT_TOLERANCE){
      #if DEBUG_GRADIENT
        Serial.println(">>> GRADIENT: CHECK PASSED");
      #endif
      
      if(isNew(src, receivedHash)){
        // Calculate new hop
        uint8_t newHop = (msgHop > 0) ? (msgHop - 1) : 0;
        
        char newHopStr[3];
        sprintf(newHopStr, "%02d", newHop);
        String newHeader = src + dst + type + hashStr + String(newHopStr);
        
        Serial.print("Forwarding message with hop=");
        Serial.println(newHop);
        
        #if DEBUG_LED
          Serial.println(">>> LED: Brief blink for message forward");
        #endif
        
        LED_ON();
        irSend(newHeader, message);
        LED_OFF();
      }
    } else {
      #if DEBUG_GRADIENT
        Serial.println(">>> GRADIENT: CHECK FAILED - NOT forwarding");
      #endif
    }
    
    // Process if this is HQ
    if(dst == HQ_ID && NODE_ID == HQ_ID){
      Serial.println("=== Message from Node ===");
      Serial.print("From: "); Serial.println(src);
      Serial.print("Distance: "); Serial.print(msgHop); Serial.println(" hops");
      Serial.print("Message: "); Serial.println(message);
    }
    
    Serial.println("════════════════════════════════════");
    Serial.println();
    return;
  }
  
  // ===== Type 1/2: BROADCAST/TARGETED - No gradient, normal forwarding =====
  if(header.length() == HEADER_LENGTH_STANDARD){
    String hashStr = header.substring(9, 13);
    uint16_t receivedHash = (uint16_t) strtol(hashStr.c_str(), NULL, 16);
    
    // Verify integrity
    uint16_t computedHash = simpleHash(message);
    if(computedHash != receivedHash){
      Serial.println(">>> ERROR: Corrupted message (hash mismatch) - discarded");
      return;
    }
    
    // Forward if new (no gradient check for HQ broadcasts)
    if(isNew(src, receivedHash)){
      #if DEBUG_LED
        Serial.println(">>> LED: Brief blink for broadcast forward");
      #endif
      
      LED_ON();
      irSend(header, message);
      LED_OFF();
    }
    
    // Type 1: BROADCAST (HQ → All)
    if(type == MSG_TYPE_BROADCAST && dst == BROADCAST_ID && IS_FROM_HQ(src)){
      Serial.println("╔════════════════════════════════════╗");
      Serial.println("║   BROADCAST FROM HQ                ║");
      Serial.println("╚════════════════════════════════════╝");
      Serial.print("From HQ: "); Serial.println(src);
      Serial.print("Message: ");
      Serial.println(message);
      Serial.println("════════════════════════════════════");
      
      latestLiFiMessage = message;
      lastLiFiBroadcastTime = millis();
      lifiTransmit(message);
    }
    
    // Type 2: TARGETED BROADCAST (HQ → Specific lamp)
    else if(type == MSG_TYPE_TARGETED && dst == NODE_ID && IS_FROM_HQ(src)){
      Serial.println("╔════════════════════════════════════╗");
      Serial.println("║  TARGETED BROADCAST FROM HQ        ║");
      Serial.println("╚════════════════════════════════════╝");
      Serial.print("From HQ: "); Serial.println(src);
      Serial.print("Message: ");
      Serial.println(message);
      Serial.println("Broadcasting to phones in this area...");
      Serial.println("════════════════════════════════════");
      
      latestLiFiMessage = message;
      lastLiFiBroadcastTime = millis();
      lifiTransmit(message);
    }
    return;
  }
  
  Serial.println(">>> ERROR: Unknown message format");
}

#endif // LIFI_H
