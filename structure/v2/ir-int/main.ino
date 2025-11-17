#include <Arduino.h>
#include "config.h"
#include "lifi.h"

// ==================== GLOBAL VARIABLES ====================

// Cache for message deduplication (defined here, declared extern in config.h)
MsgCache cache[CACHE_SIZE];
int cacheIndex = 0;

// Retransmission queue (defined here, declared extern in config.h)
RetransmitEntry retransmitQueue[RETRANSMIT_QUEUE_SIZE];

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
  
  // Initialize all 4 directional IR TX pins
  pinMode(IR_TX_FRONT, OUTPUT);
  pinMode(IR_TX_RIGHT, OUTPUT);
  pinMode(IR_TX_BACK, OUTPUT);
  pinMode(IR_TX_LEFT, OUTPUT);
  
  pinMode(IR_RX_PIN, INPUT);
  pinMode(LED_STATUS, OUTPUT);
  pinMode(LAMP_LIGHT_PIN, OUTPUT);

  // Initialize cache to empty state
  for(int i = 0; i < CACHE_SIZE; i++){
    cache[i].src = "";
    cache[i].msgHash = 0;
  }

  // Initialize retransmit queue to empty state
  for(int i = 0; i < RETRANSMIT_QUEUE_SIZE; i++){
    retransmitQueue[i].active = false;
  }

  Serial.begin(115200);
  delay(100);
  
  // Initialize IR hardware
  irInit();
  
  Serial.println("\n================================");
  Serial.println("   LiFi Mesh Lamp Node V2");
  Serial.println("================================");
  Serial.print("Node ID: "); Serial.println(NODE_ID);
  Serial.print("SOS Cooldown: "); Serial.print(SOS_COOLDOWN/1000); Serial.println("s");
  Serial.print("Retransmit Count: "); Serial.println(RETRANSMIT_COUNT);
  Serial.print("Retransmit Interval: "); Serial.print(RETRANSMIT_INTERVAL/1000); Serial.println("s");
  Serial.println("4-Direction TX enabled");
  Serial.println("Header-only SOS support enabled");
  Serial.println("================================\n");
  
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

  // ===== TASK 3: Process retransmission queue =====
  processRetransmitQueue();

  // ===== TASK 4: Periodic LiFi rebroadcast =====
  if(latestLiFiMessage != "" && 
     (millis() - lastLiFiBroadcastTime >= LIFI_REBROADCAST_INTERVAL)){
    
    Serial.println("Rebroadcasting to phones...");
    lifiTransmit(latestLiFiMessage);
    lastLiFiBroadcastTime = millis();
  }

  delay(10);
}
