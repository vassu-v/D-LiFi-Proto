#include <Arduino.h>
#include "config.h"
#include "lifi.h"

// ==================== GLOBAL VARIABLES ====================

MsgCache cache[CACHE_SIZE];
int cacheIndex = 0;

RetransmitEntry retransmitQueue[RETRANSMIT_QUEUE_SIZE];

String lastInitID = "";
uint8_t myHop = INITIAL_HOP;

unsigned long lastSOSTime = 0;
bool lastButtonState = HIGH;

String latestLiFiMessage = "";
unsigned long lastLiFiBroadcastTime = 0;

bool lifiBlinkActive = false;
unsigned long lifiBlinkStartTime = 0;
bool lifiBlinkState = false;
unsigned long lifiLastToggleTime = 0;

// ==================== SETUP ====================

void setup(){
  Serial.begin(115200);
  delay(100);

  pinMode(SOS_PIN, INPUT_PULLUP);
  pinMode(LAMP_LIGHT_PIN, OUTPUT);

  pinMode(LED_RX_PIN, OUTPUT);
  pinMode(LED_TX_PIN, OUTPUT);
  LED_RX_OFF();
  LED_TX_OFF();

  pinMode(IR_TX_FRONT, OUTPUT);
  pinMode(IR_TX_RIGHT, OUTPUT);
  pinMode(IR_TX_BACK, OUTPUT);
  pinMode(IR_TX_LEFT, OUTPUT);
  pinMode(IR_RX_PIN, INPUT);

  irInit();

  for(int i = 0; i < CACHE_SIZE; i++){
    cache[i].src = "";
    cache[i].msgHash = 0;
  }

  for(int i = 0; i < RETRANSMIT_QUEUE_SIZE; i++){
    retransmitQueue[i].active = false;
  }

  LED_RX_ON();
  LED_TX_ON();
  digitalWrite(LAMP_LIGHT_PIN, HIGH);
  delay(200);
  LED_RX_OFF();
  LED_TX_OFF();
  digitalWrite(LAMP_LIGHT_PIN, LOW);
}

// ==================== LOOP ====================

void loop(){

  // --- SOS Button ---
  bool currentButtonState = digitalRead(SOS_PIN);
  if(currentButtonState == LOW && lastButtonState == HIGH){
    unsigned long dt = millis() - lastSOSTime;
    if(dt >= SOS_COOLDOWN){
      generateSOS();
      lastSOSTime = millis();
    }
  }
  lastButtonState = currentButtonState;

  // --- IR Receive ---
  String header, message;
  if(irReceive(header, message)){
    forwardPacket(header, message,
                  latestLiFiMessage,
                  lastLiFiBroadcastTime,
                  lifiBlinkActive,
                  lifiBlinkStartTime);
  }

  // --- Retransmit ---
  processRetransmitQueue();

  // --- Visual Blink ---
  lifiProcessBlink(lifiBlinkActive,
                   lifiBlinkStartTime,
                   lifiBlinkState,
                   lifiLastToggleTime);

  // --- Periodic Rebroadcast ---
  if(latestLiFiMessage != "" &&
     !lifiBlinkActive &&
     (millis() - lastLiFiBroadcastTime >= LIFI_REBROADCAST_INTERVAL)){

    lifiTransmit(latestLiFiMessage);
    lastLiFiBroadcastTime = millis();
    lifiBlinkActive = true;
    lifiBlinkStartTime = millis();
  }

  delay(10);
}
