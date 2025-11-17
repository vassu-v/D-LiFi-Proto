#include <Arduino.h>
#include "config.h"
#include "lifi.h"

// ==================== GLOBAL VARIABLES ====================

// Cache for message deduplication (defined here, declared extern in config.h)
MsgCache cache[CACHE_SIZE];
int cacheIndex = 0;

// Retransmission queue (defined here, declared extern in config.h)
RetransmitEntry retransmitQueue[RETRANSMIT_QUEUE_SIZE];

// Gradient system state (defined here, declared extern in config.h)
String lastInitID = "";
uint8_t myHop = INITIAL_HOP;  // Start at max distance (uninitialized)

// Button state tracking
unsigned long lastSOSTime = 0;
bool lastButtonState = HIGH;  // HIGH = not pressed (INPUT_PULLUP)

// LiFi rebroadcast tracking
String latestLiFiMessage = "";
unsigned long lastLiFiBroadcastTime = 0;

// ==================== SETUP ====================

void setup(){
  // Initialize Serial
  Serial.begin(115200);
  delay(100);
  
  // Initialize hardware pins
  pinMode(SOS_PIN, INPUT_PULLUP);
  pinMode(LED_STATUS, OUTPUT);
  pinMode(LAMP_LIGHT_PIN, OUTPUT);
  
  // Note: IR TX pins initialized per-transmission in ir.h
  pinMode(IR_RX_PIN, INPUT);
  
  // Initialize IR hardware (receiver only)
  irInit();
  
  // Initialize cache to empty state
  for(int i = 0; i < CACHE_SIZE; i++){
    cache[i].src = "";
    cache[i].msgHash = 0;
  }

  // Initialize retransmit queue to empty state
  for(int i = 0; i < RETRANSMIT_QUEUE_SIZE; i++){
    retransmitQueue[i].active = false;
  }

  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║   LiFi Mesh Lamp Node V3           ║");
  Serial.println("║   (Hop-Based Gradient System)      ║");
  Serial.println("╚════════════════════════════════════╝");
  Serial.print("Node ID: "); Serial.println(NODE_ID);
  Serial.print("Initial Hop: "); Serial.println(myHop);
  Serial.print("Gradient Tolerance (K): "); Serial.println(GRADIENT_TOLERANCE);
  Serial.print("SOS Cooldown: "); Serial.print(SOS_COOLDOWN/1000); Serial.println("s");
  Serial.print("Retransmit Count: "); Serial.println(RETRANSMIT_COUNT);
  Serial.print("Retransmit Interval: "); Serial.print(RETRANSMIT_INTERVAL/1000); Serial.println("s");
  Serial.println("4-Direction TX enabled");
  Serial.print("LED Mode: ");
  #if LED_INVERTED
    Serial.println("Active LOW (inverted)");
  #else
    Serial.println("Active HIGH");
  #endif
  Serial.println("════════════════════════════════════\n");
  
  // Startup LED flash (brief, non-blocking)
  LED_ON();
  digitalWrite(LAMP_LIGHT_PIN, HIGH);
  delay(100);  // Brief flash only
  LED_OFF();
  digitalWrite(LAMP_LIGHT_PIN, LOW);
}

// ==================== MAIN LOOP ====================

void loop(){
  // ===== TASK 1: Non-blocking SOS button handling =====
  bool currentButtonState = digitalRead(SOS_PIN);
  
  // Check for falling edge (button press moment)
  if(currentButtonState == LOW && lastButtonState == HIGH){
    #if DEBUG_BUTTON
      Serial.println();
      Serial.println(">>> BUTTON: SOS button pressed (falling edge detected)");
    #endif
    
    unsigned long timeSinceLastSOS = millis() - lastSOSTime;
    
    if(timeSinceLastSOS >= SOS_COOLDOWN){
      #if DEBUG_BUTTON
        Serial.println(">>> BUTTON: Cooldown OK, generating SOS...");
      #endif
      generateSOS();
      lastSOSTime = millis();
    } else {
      #if DEBUG_BUTTON
        Serial.println(">>> BUTTON: Still in cooldown period!");
        Serial.print("    Time remaining: ");
        Serial.print((SOS_COOLDOWN - timeSinceLastSOS) / 1000);
        Serial.println("s");
      #endif
    }
  }
  
  lastButtonState = currentButtonState;

  // ===== TASK 2: Check for incoming messages =====
  String header, message;
  if(irReceive(header, message)){
    Serial.println();
    Serial.println("╔════════════════════════════════════╗");
    Serial.println("║   COMPLETE PACKET RECEIVED         ║");
    Serial.println("╚════════════════════════════════════╝");
    Serial.print("Header: ");
    Serial.println(header);
    Serial.print("Message: ");
    Serial.println(message.length() > 0 ? message : "(none)");
    Serial.println("Processing packet...");
    Serial.println();
    
    forwardPacket(header, message, latestLiFiMessage, lastLiFiBroadcastTime);
  }

  // ===== TASK 3: Process retransmission queue =====
  processRetransmitQueue();

  // ===== TASK 4: Periodic LiFi rebroadcast =====
  if(latestLiFiMessage != "" && 
     (millis() - lastLiFiBroadcastTime >= LIFI_REBROADCAST_INTERVAL)){
    
    Serial.println(">>> LiFi: Periodic rebroadcast triggered");
    lifiTransmit(latestLiFiMessage);
    lastLiFiBroadcastTime = millis();
  }

  // ===== TASK 5: Display current gradient status =====
  static unsigned long lastStatusPrint = 0;
  if(millis() - lastStatusPrint > 30000){  // Every 30 seconds
    Serial.println();
    Serial.println("╔════════════════════════════════════╗");
    Serial.println("║      GRADIENT STATUS               ║");
    Serial.println("╚════════════════════════════════════╝");
    Serial.print("myHop: ");
    Serial.println(myHop == INITIAL_HOP ? "Uninitialized (99)" : String(myHop));
    Serial.print("lastInitID: ");
    Serial.println(lastInitID.length() > 0 ? lastInitID : "None");
    Serial.println("════════════════════════════════════");
    Serial.println();
    lastStatusPrint = millis();
  }

  delay(10);
}
