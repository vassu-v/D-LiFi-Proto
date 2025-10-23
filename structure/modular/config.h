// Unique ID for this node (4 characters, alphanumeric)
#define NODE_ID      "102a"

// Reserved ID for broadcast messages (all nodes receive)
#define BROADCAST_ID "FFFF"

// Headquarters/Base Station ID (SOS messages are sent here)
#define HQ_ID        "000h"

// Pin assignments
#define SOS_PIN      D3
#define IR_TX_PIN    D1
#define IR_RX_PIN    D2
#define LED_STATUS   D4
#define LAMP_LIGHT_PIN D5

// Timing constants
const unsigned long SOS_COOLDOWN = 180000;
const unsigned long LIFI_REBROADCAST_INTERVAL = 60000;
#define CACHE_SIZE   3

// Message type definitions
#define MSG_TYPE_BROADCAST '1'
#define MSG_TYPE_TARGETED  '2'
#define MSG_TYPE_SOS       '3'
#define MSG_TYPE_MESSAGE   '4'

// Cache-related variables
int cacheIndex = 0;

// Pre-computed hash for SOS messages
#define SOS_MESSAGE "HELP!"
#define SOS_HASH 0x28F9
