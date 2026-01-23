#include <Arduino.h>
#include "config.h"
#include "lifi.h"

// ==================== GLOBAL VARIABLES ====================

MsgCache cache[CACHE_SIZE];
int cacheIndex = 0;

// ==================== SETUP ====================

void setup(){
  Serial.begin(115200);
  delay(100);
  
  pinMode(LED_STATUS, OUTPUT);
  pinMode(IR_RX_PIN, INPUT);
  
  irInit();
  
  // Initialize cache
  for(int i = 0; i < CACHE_SIZE; i++){
    cache[i].src = "";
    cache[i].msgHash = 0;
  }

  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║   LiFi Mesh HQ Node V3             ║");
  Serial.println("║   (Gradient System Controller)     ║");
  Serial.println("╚════════════════════════════════════╝");
  Serial.print("Node ID: "); Serial.println(NODE_ID);
  Serial.print("HQ Hop: "); Serial.println(HQ_HOP);
  Serial.println("4-Direction TX enabled");
  Serial.println("════════════════════════════════════\n");
  
  Serial.println("Commands:");
  Serial.println("  INIT|<id>              - Send INIT (e.g., INIT|01)");
  Serial.println("  BROADCAST|<message>    - Type 1: Broadcast to all");
  Serial.println("  TARGET|<nodeID>|<msg>  - Type 2: Target lamp");
  Serial.println("  MESSAGE|<nodeID>|<msg> - Type 4: Send message");
  Serial.println();
  
  LED_ON();
  delay(100);
  LED_OFF();
  
  Serial.println("READY");
}

// ==================== MAIN LOOP ====================

void loop(){
  // ===== TASK 1: Process serial commands from Python =====
  if(Serial.available()){
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    #if DEBUG_COMMAND
      Serial.print(">>> CMD: Received '");
      Serial.print(cmd);
      Serial.println("'");
    #endif
    
    // Parse command
    if(cmd.startsWith("INIT|")){
      String initID = cmd.substring(5);
      if(initID.length() == 2){
        sendInit(initID);
      } else {
        Serial.println("ERROR: INIT ID must be 2 chars");
      }
    }
    else if(cmd.startsWith("BROADCAST|")){
      String message = cmd.substring(10);
      if(message.length() > 0){
        sendBroadcast(message);
      } else {
        Serial.println("ERROR: Empty message");
      }
    }
    else if(cmd.startsWith("TARGET|")){
      int pipePos = cmd.indexOf('|', 7);
      if(pipePos > 0){
        String nodeID = cmd.substring(7, pipePos);
        String message = cmd.substring(pipePos + 1);
        if(nodeID.length() == 4 && message.length() > 0){
          sendTargeted(nodeID, message);
        } else {
          Serial.println("ERROR: Invalid format");
        }
      } else {
        Serial.println("ERROR: Missing separator");
      }
    }
    else if(cmd.startsWith("MESSAGE|")){
      int pipePos = cmd.indexOf('|', 8);
      if(pipePos > 0){
        String nodeID = cmd.substring(8, pipePos);
        String message = cmd.substring(pipePos + 1);
        if(nodeID.length() == 4 && message.length() > 0){
          sendMessage(nodeID, message);
        } else {
          Serial.println("ERROR: Invalid format");
        }
      } else {
        Serial.println("ERROR: Missing separator");
      }
    }
    else {
      Serial.println("ERROR: Unknown command");
    }
  }
  
  // ===== TASK 2: Check for incoming messages =====
  String header, message;
  if(irReceive(header, message)){
    processPacket(header, message);
  }

  delay(10);
}
