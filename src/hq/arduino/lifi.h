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
 * IR Transmission (HQ to Mesh)
 * Sends header and message via IR in two bursts
 * 
 * Current: Placeholder using Serial output
 * To implement real IR: Replace with IRremote library calls
 */
inline void irSend(String header, String message){
  // Burst 1: Send header (IR mesh communication)
  Serial.print("TX|"); 
  Serial.print(header);
  Serial.print("|");
  Serial.println(message);
  
  digitalWrite(IR_TX_PIN, HIGH);
  delay(25);
  digitalWrite(IR_TX_PIN, LOW);
  delay(10);  // Inter-burst gap
  
  digitalWrite(IR_TX_PIN, HIGH);
  delay(25);
  digitalWrite(IR_TX_PIN, LOW);
}

/*
 * IR Reception (Mesh to HQ)
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
    
    // Check if this is a command from Python (starts with "TX|")
    if(line.startsWith("TX|")){
      return false;  // This is handled separately, not an IR receive
    }
    
    if(!headerReceived){
      // First burst: receive header
      if(line.length() == 13){  // Valid header length
        receivedHeader = line;
        headerReceived = true;
      }
      return false;  // Wait for second burst
    } else {
      // Second burst: receive message
      header = receivedHeader;
      message = line;
      headerReceived = false;  // Reset for next packet
      return true;  // Complete packet received
    }
  }
  return false;
}

// ==================== PROTOCOL FUNCTIONS ====================

/*
 * Process Incoming Packet from Mesh
 * 
 * HQ behavior:
 *   1. Validates header format
 *   2. Verifies message integrity (hash check)
 *   3. Deduplicates using cache (filters duplicates BEFORE sending to Python)
 *   4. Forwards to Python via Serial in format: <src> <type> <message>
 *   5. Does NOT forward back to mesh (edge node)
 * 
 * Header Format (13 chars): [src(4)][dst(4)][type(1)][hash(4)]
 */
inline void processPacket(String header, String message){
  // Validate header
  if(header.length() != 13){
    return;  // Invalid header, discard silently
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
    return;  // Corrupted message, discard silently
  }

  // Check if message is new (deduplicate)
  if(!isNew(src, receivedHash)){
    return;  // Duplicate message, already processed
  }

  // Visual feedback: blink LED for received message
  digitalWrite(LED_STATUS, HIGH);
  
  // Forward to Python via Serial
  // Format: <src> <type> <message>
  Serial.print(src);
  Serial.print(" ");
  Serial.print(type);
  Serial.print(" ");
  Serial.println(message);
  
  delay(50);
  digitalWrite(LED_STATUS, LOW);
}

/*
 * Handle Command from Python
 * 
 * Format: TX|<dst>|<type>|<message>
 * Example: TX|FFFF|1|Evacuation route open
 * Example: TX|102a|2|Check battery
 * Example: TX|203b|4|Status request
 */
inline void handlePythonCommand(String command){
  // Parse command: TX|dst|type|message
  int firstPipe = command.indexOf('|');
  int secondPipe = command.indexOf('|', firstPipe + 1);
  int thirdPipe = command.indexOf('|', secondPipe + 1);
  
  if(firstPipe == -1 || secondPipe == -1 || thirdPipe == -1){
    Serial.println("ERR|Invalid command format");
    return;
  }
  
  String dst = command.substring(firstPipe + 1, secondPipe);
  String typeStr = command.substring(secondPipe + 1, thirdPipe);
  String message = command.substring(thirdPipe + 1);
  
  // Validate
  if(dst.length() != 4 || typeStr.length() != 1){
    Serial.println("ERR|Invalid destination or type");
    return;
  }
  
  char type = typeStr[0];
  
  // Compute hash
  uint16_t hash = simpleHash(message);
  char hashStr[5];
  sprintf(hashStr, "%04X", hash);
  
  // Build header: [src(4)][dst(4)][type(1)][hash(4)]
  String header = String(NODE_ID) + dst + type + String(hashStr);
  
  // Add to cache (don't process our own messages)
  isNew(NODE_ID, hash);
  
  // Send via IR
  irSend(header, message);
  
  // Confirm to Python
  Serial.println("OK|Message sent");
}

#endif // LIFI_H
