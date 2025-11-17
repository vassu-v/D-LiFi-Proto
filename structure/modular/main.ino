
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
  // ===== TASK 1: Non-blocking SOS button handling =====
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

  // ===== TASK 2: Check for incoming messages =====
  String header, message;
  if(irReceive(header, message)){
    forwardPacket(header, message, latestLiFiMessage, lastLiFiBroadcastTime);
  }

  // ===== TASK 3: Periodic LiFi rebroadcast =====
  if(latestLiFiMessage != "" && 
     (millis() - lastLiFiBroadcastTime >= LIFI_REBROADCAST_INTERVAL)){
    
    Serial.println("Rebroadcasting to phones...");
    lifiTransmit(latestLiFiMessage);
    lastLiFiBroadcastTime = millis();
  }

  delay(10);
}
