// ===== main/config.h =====
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ================= NODE TYPE =================
#define NODE_TYPE_MESH 0
#define NODE_TYPE_AP   1
#define THIS_NODE_TYPE NODE_TYPE_AP

// ================= NODE CONFIG =================
#define NODE_ID      "bb01"
#define BROADCAST_ID "FFFF"
#define HQ_ID        "000h"
#define IS_FROM_HQ(src) ((src) == HQ_ID)

// ================= PINS =================
#define IR_TX_FRONT    D2
#define IR_TX_RIGHT    D1
#define IR_TX_BACK     D0
#define IR_TX_LEFT     D7
#define IR_RX_PIN      D5
#define LED_STATUS     D8

// ================= LED =================
#define LED_INVERTED 0
#if LED_INVERTED
  #define LED_ON()  digitalWrite(LED_STATUS, LOW)
  #define LED_OFF() digitalWrite(LED_STATUS, HIGH)
#else
  #define LED_ON()  digitalWrite(LED_STATUS, HIGH)
  #define LED_OFF() digitalWrite(LED_STATUS, LOW)
#endif

// ================= DEBUG =================
#define DEBUG_IR_TX 1
#define DEBUG_IR_RX 1
#define DEBUG_CACHE 1
#define DEBUG_RETRANSMIT 1
#define DEBUG_GRADIENT 1

// ================= TIMING =================
const unsigned long IR_DIRECTION_GAP = 100;
const unsigned long IR_MESSAGE_TIMEOUT = 3000;

// ================= RELIABILITY =================
#define RETRANSMIT_COUNT 2
const unsigned long RETRANSMIT_INTERVAL = 10000;
const unsigned long REDUNDANCY_WINDOW = 60000;
#define CACHE_SIZE 3
#define RETRANSMIT_QUEUE_SIZE 3

// ================= GRADIENT =================
#define GRADIENT_TOLERANCE 1
#define INITIAL_HOP 99

// ================= MESSAGE TYPES =================
#define MSG_TYPE_INIT      '0'
#define MSG_TYPE_BROADCAST '1'
#define MSG_TYPE_TARGETED  '2'
#define MSG_TYPE_SOS       '3'
#define MSG_TYPE_MESSAGE   '4'

#define HEADER_LENGTH_INIT     9
#define HEADER_LENGTH_STANDARD 13
#define HEADER_LENGTH_SOS      11
#define HEADER_LENGTH_MESSAGE  15

// ================= STRUCTS =================
struct MsgCache {
  String src;
  uint16_t msgHash;
};

struct RetransmitEntry {
  String header;
  String message;
  unsigned long firstSentTime;
  uint8_t sentCount;
  bool active;
};

// ================= GLOBALS =================
extern MsgCache cache[CACHE_SIZE];
extern int cacheIndex;
extern RetransmitEntry retransmitQueue[RETRANSMIT_QUEUE_SIZE];
extern String lastInitID;
extern uint8_t myHop;

#endif
