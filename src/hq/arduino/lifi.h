#ifndef LIFI_H
#define LIFI_H

#include <Arduino.h>
#include "config.h"
#include "ir.h"

// ==================== UTILITY FUNCTIONS ====================

inline uint16_t simpleHash(String s){
  uint16_t h = 0;
  for (int i = 0; i < s.length(); i++){
    h = (h * 31) + s[i];
  }
  return h;
}

inline bool isNew(String src, uint16_t hash){
  #if DEBUG_CACHE
    Serial.print(">>> CACHE: Checking (src='");
    Serial.print(src);
    Serial.print("', hash=0x");
    Serial.print(hash, HEX);
    Serial.println(")");
  #endif
  
  for (int i = 0; i < CACHE_SIZE; i++){
    if (cache[i].src == src && cache[i].msgHash == hash){
      #if DEBUG_CACHE
        Serial.println(">>> CACHE: HIT - Duplicate");
      #endif
      return false;
    }
  }
  
  #if DEBUG_CACHE
    Serial.println(">>> CACHE: MISS - New message");
  #endif
  
  cache[cacheIndex].src = src;
  cache[cacheIndex].msgHash = hash;
  cacheIndex = (cacheIndex + 1) % CACHE_SIZE;
  
  return true;
}

// ==================== IR COMMUNICATION ====================

inline void irSendRaw(String header, String message = ""){
  const int txPins[] = {IR_TX_FRONT, IR_TX_RIGHT, IR_TX_BACK, IR_TX_LEFT};
  const char* dirNames[] = {"FRONT", "RIGHT", "BACK", "LEFT"};
  
  Serial.println("╔════════════════════════════════════╗");
  Serial.println("║   IR TX (4 DIRECTIONS)             ║");
  Serial.println("╚════════════════════════════════════╝");
  Serial.print("Header: ");
  Serial.println(header);
  if(message.length() > 0){
    Serial.print("Message: ");
    Serial.println(message);
  }
  
  IrReceiver.stop();
  
  for(int i = 0; i < 4; i++){
    Serial.print("Direction: ");
    Serial.println(dirNames[i]);
    
    String headerWithDelim = header + " ";
    irSendString(headerWithDelim.c_str(), txPins[i]);
    
    if(message.length() > 0){
      delay(50);
      String messageWithDelim = message + " ";
      irSendString(messageWithDelim.c_str(), txPins[i]);
    }
    
    if(i < 3) delay(IR_DIRECTION_GAP);
  }
  
  IrReceiver.start();
  Serial.println("════════════════════════════════════\n");
}

inline bool irReceive(String &header, String &message){
  static bool waitingForMessage = false;
  static String receivedHeader = "";
  static unsigned long headerReceivedTime = 0;
  
  String line;
  if(irReceiveString(line)){
    line.trim();
    
    // INIT (9 chars)
    if(line.length() == HEADER_LENGTH_INIT && line[8] == MSG_TYPE_INIT){
      header = line;
      message = "";
      Serial.println("RX: INIT packet");
      if(waitingForMessage){
        waitingForMessage = false;
        receivedHeader = "";
      }
      return true;
    }
    
    // SOS (11 chars)
    if(line.length() == HEADER_LENGTH_SOS && line[8] == MSG_TYPE_SOS){
      header = line;
      message = "";
      Serial.println("RX: SOS packet");
      if(waitingForMessage){
        waitingForMessage = false;
        receivedHeader = "";
      }
      return true;
    }
    
    // Two-segment messages
    if(!waitingForMessage){
      if(line.length() == HEADER_LENGTH_STANDARD || line.length() == HEADER_LENGTH_MESSAGE){
        receivedHeader = line;
        waitingForMessage = true;
        headerReceivedTime = millis();
        Serial.println("RX: Header received");
      }
      return false;
    } else {
      header = receivedHeader;
      message = line;
      waitingForMessage = false;
      receivedHeader = "";
      Serial.println("RX: Message received");
      return true;
    }
  }
  
  // Timeout check
  if(waitingForMessage && (millis() - headerReceivedTime > IR_MESSAGE_TIMEOUT)){
    Serial.println("RX: Timeout, resetting");
    waitingForMessage = false;
    receivedHeader = "";
  }
  
  return false;
}

// ==================== HQ FUNCTIONS ====================

/*
 * Send INIT Message
 * Builds gradient map from HQ outward
 */
inline void sendInit(String initID){
  char hopStr[3];
  sprintf(hopStr, "%02d", HQ_HOP);  // HQ is always hop 0
  
  String header = String(NODE_ID) + initID + String(hopStr) + MSG_TYPE_INIT;
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║   SENDING INIT MESSAGE             ║");
  Serial.println("╚════════════════════════════════════╝");
  Serial.print("INIT ID: "); Serial.println(initID);
  Serial.print("HQ Hop: "); Serial.println(HQ_HOP);
  Serial.print("Header: "); Serial.println(header);
  
  isNew(NODE_ID, 0);  // Add to cache
  
  LED_ON();
  irSendRaw(header);
  LED_OFF();
  
  Serial.println("✓ INIT transmitted\n");
}

/*
 * Send Broadcast Message (Type 1)
 */
inline void sendBroadcast(String message){
  uint16_t hash = simpleHash(message);
  char hashStr[5];
  sprintf(hashStr, "%04X", hash);
  
  String header = String(NODE_ID) + BROADCAST_ID + MSG_TYPE_BROADCAST + String(hashStr);
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║   SENDING BROADCAST                ║");
  Serial.println("╚════════════════════════════════════╝");
  Serial.print("Message: "); Serial.println(message);
  Serial.print("Header: "); Serial.println(header);
  
  isNew(NODE_ID, hash);
  
  LED_ON();
  irSendRaw(header, message);
  LED_OFF();
  
  Serial.println("✓ Broadcast transmitted\n");
}

/*
 * Send Targeted Message (Type 2)
 */
inline void sendTargeted(String nodeID, String message){
  uint16_t hash = simpleHash(message);
  char hashStr[5];
  sprintf(hashStr, "%04X", hash);
  
  String header = String(NODE_ID) + nodeID + MSG_TYPE_TARGETED + String(hashStr);
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║   SENDING TARGETED MESSAGE         ║");
  Serial.println("╚════════════════════════════════════╝");
  Serial.print("To: "); Serial.println(nodeID);
  Serial.print("Message: "); Serial.println(message);
  Serial.print("Header: "); Serial.println(header);
  
  isNew(NODE_ID, hash);
  
  LED_ON();
  irSendRaw(header, message);
  LED_OFF();
  
  Serial.println("✓ Targeted message transmitted\n");
}

/*
 * Send Message (Type 4)
 */
inline void sendMessage(String nodeID, String message){
  uint16_t hash = simpleHash(message);
  char hashStr[5];
  sprintf(hashStr, "%04X", hash);
  
  char hopStr[3];
  sprintf(hopStr, "%02d", HQ_HOP);
  
  String header = String(NODE_ID) + nodeID + MSG_TYPE_MESSAGE + String(hashStr) + String(hopStr);
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║   SENDING MESSAGE                  ║");
  Serial.println("╚════════════════════════════════════╝");
  Serial.print("To: "); Serial.println(nodeID);
  Serial.print("Message: "); Serial.println(message);
  Serial.print("Header: "); Serial.println(header);
  
  isNew(NODE_ID, hash);
  
  LED_ON();
  irSendRaw(header, message);
  LED_OFF();
  
  Serial.println("✓ Message transmitted\n");
}

/*
 * Process Received Packet at HQ
 */
inline void processPacket(String header, String message){
  if(header.length() < 9) return;
  
  String src = header.substring(0, 4);
  String dst = header.substring(4, 8);
  char type = header[8];
  
  // === Type 3: SOS ===
  if(type == MSG_TYPE_SOS && header.length() == HEADER_LENGTH_SOS){
    String hopStr = header.substring(9, 11);
    uint8_t msgHop = hopStr.toInt();
    
    if(isNew(src, 0)){  // Deduplicate SOS
      Serial.println("\n╔════════════════════════════════════╗");
      Serial.println("║   🚨 SOS ALERT RECEIVED            ║");
      Serial.println("╚════════════════════════════════════╝");
      Serial.print("From Node: "); Serial.println(src);
      Serial.print("Distance: "); Serial.print(msgHop); Serial.println(" hops");
      Serial.println("════════════════════════════════════\n");
      
      // Send to Python
      Serial.print(src);
      Serial.print(" ");
      Serial.print(type);
      Serial.print(" ");
      Serial.println("SOS");
    }
    return;
  }
  
  // === Type 4: MESSAGE ===
  if(type == MSG_TYPE_MESSAGE && header.length() == HEADER_LENGTH_MESSAGE){
    String hashStr = header.substring(9, 13);
    String hopStr = header.substring(13, 15);
    uint16_t receivedHash = (uint16_t) strtol(hashStr.c_str(), NULL, 16);
    uint8_t msgHop = hopStr.toInt();
    
    uint16_t computedHash = simpleHash(message);
    if(computedHash != receivedHash){
      Serial.println(">>> ERROR: Hash mismatch");
      return;
    }
    
    if(isNew(src, receivedHash)){
      Serial.println("\n╔════════════════════════════════════╗");
      Serial.println("║   MESSAGE RECEIVED                 ║");
      Serial.println("╚════════════════════════════════════╝");
      Serial.print("From Node: "); Serial.println(src);
      Serial.print("Distance: "); Serial.print(msgHop); Serial.println(" hops");
      Serial.print("Message: "); Serial.println(message);
      Serial.println("════════════════════════════════════\n");
      
      // Send to Python
      Serial.print(src);
      Serial.print(" ");
      Serial.print(type);
      Serial.print(" ");
      Serial.println(message);
    }
    return;
  }
  
  // HQ doesn't process Type 0, 1, 2 (those are HQ → Lamps)
}

#endif // LIFI_H
