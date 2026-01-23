#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==================== NODE CONFIGURATION ====================

// HQ Node ID (always "000h" for headquarters)
#define NODE_ID      "000h"

// Reserved ID for broadcast messages (all nodes receive)
#define BROADCAST_ID "FFFF"

// Headquarters/Base Station ID (same as NODE_ID for HQ)
#define HQ_ID        "000h"

// HQ is always at hop 0 (closest to itself!)
#define HQ_HOP       0

// ==================== PIN ASSIGNMENTS ====================

// Directional IR TX pins (4 directions for street lamp mesh)
#define IR_TX_FRONT    D2  // Forward direction
#define IR_TX_RIGHT    D3  // Right direction  
#define IR_TX_BACK     D0  // Backward direction
#define IR_TX_LEFT     D7  // Left direction

#define IR_RX_PIN      D5  // IR receiver module (INPUT)
#define LED_STATUS     D1  // Status LED for visual feedback (OUTPUT)

// ==================== LED CONFIGURATION ====================

// LED polarity configuration
#define LED_INVERTED 0

#if LED_INVERTED
  #define LED_ON()  digitalWrite(LED_STATUS, LOW)
  #define LED_OFF() digitalWrite(LED_STATUS, HIGH)
#else
  #define LED_ON()  digitalWrite(LED_STATUS, HIGH)
  #define LED_OFF() digitalWrite(LED_STATUS, LOW)
#endif

// ==================== DEBUG CONFIGURATION ====================

#define DEBUG_IR_TX       1
#define DEBUG_IR_RX       1
#define DEBUG_CACHE       1
#define DEBUG_TIMING      1
#define DEBUG_LED         1
#define DEBUG_COMMAND     1  // Serial command processing

// ==================== TIMING CONSTANTS ====================

const unsigned long IR_DIRECTION_GAP = 100;
const unsigned long IR_MESSAGE_TIMEOUT = 3000;

// ==================== MESSAGE TYPE DEFINITIONS ====================

#define MSG_TYPE_INIT      '0'  // HQ → All lamps (gradient setup)
#define MSG_TYPE_BROADCAST '1'  // HQ → All lamps
#define MSG_TYPE_TARGETED  '2'  // HQ → Specific lamp
#define MSG_TYPE_SOS       '3'  // Lamp → HQ (emergency)
#define MSG_TYPE_MESSAGE   '4'  // Node → HQ

// Header lengths
#define HEADER_LENGTH_INIT     9
#define HEADER_LENGTH_STANDARD 13
#define HEADER_LENGTH_SOS      11
#define HEADER_LENGTH_MESSAGE  15

// ==================== CACHE ====================

#define CACHE_SIZE 8  // Larger cache for HQ

struct MsgCache {
  String src;
  uint16_t msgHash;
};

extern MsgCache cache[CACHE_SIZE];
extern int cacheIndex;

#endif // CONFIG_H
