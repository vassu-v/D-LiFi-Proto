#include <Arduino.h>
#include "config.h"
#include "lifi.h"

// ==================== GLOBAL VARIABLES ====================

// Cache for message deduplication (defined here, declared extern in config.h)
MsgCache cache[CACHE_SIZE];
int cacheIndex = 0;

// ==================== SETUP ====================

void setup(){
  // Initialize hardware pins
  pinMode(IR_TX_PIN, OUTPUT);
  pinMode(IR_RX_PIN, INPUT);
  pinMode(LED_STATUS, OUTPUT);

  // Initialize cache to empty state (prevent garbage data)
  for(int i = 0; i < CACHE_SIZE; i++){
    cache[i].src = "";
    cache[i].msgHash = 0;
  }

  Serial.begin(115200);
  
  // Wait for serial connection
  delay(1000);
  
  Serial.println("READY|HQ Node Online");
  Serial.print("INFO|Node ID: ");
  Serial.println(NODE_ID);
  Serial.print("INFO|Cache Size: ");
  Serial.println(CACHE_SIZE);
  
  // Startup LED flash
  digitalWrite(LED_STATUS, HIGH);
  delay(500);
  digitalWrite(LED_STATUS, LOW);
}

// ==================== MAIN LOOP ====================

void loop(){
  // ===== TASK 1: Check for commands from Python =====
  if(Serial.available()){
    String line = Serial.readStringUntil('\n');
    line.trim();
    
    // Check if this is a TX command from Python
    if(line.startsWith("TX|")){
      handlePythonCommand(line);
    }
    // Otherwise, it's a simulated IR message (for testing)
    else {
      String header, message;
      // Put the line back for irReceive to process
      // (This is a workaround for testing via Serial Monitor)
      // In production with real IR, this branch won't be needed
    }
  }
  
  // ===== TASK 2: Check for incoming IR messages from mesh =====
  String header, message;
  if(irReceive(header, message)){
    processPacket(header, message);
  }

  delay(10);
}
