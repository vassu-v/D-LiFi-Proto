
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==================== NODE CONFIGURATION ====================

// Unique ID for this node (4 characters, alphanumeric)
// IMPORTANT: Change this for each node! Examples: "102a", "203b", "304c"
#define NODE_ID      "102a"

// Reserved ID for broadcast messages (all nodes receive)
#define BROADCAST_ID "FFFF"

// Headquarters/Base Station ID (SOS messages are sent here)
// Using "000h" pattern: 3 digits + 'h' for headquarters
#define HQ_ID        "000h"

// ==================== PIN ASSIGNMENTS ====================

#define SOS_PIN        D3  // Pushbutton for SOS (INPUT_PULLUP, active LOW)
#define IR_TX_PIN      D1  // IR LED transmitter (OUTPUT)
#define IR_RX_PIN      D2  // IR receiver module (INPUT)
#define LED_STATUS     D4  // Status LED for visual feedback (OUTPUT)
#define LAMP_LIGHT_PIN D5  // Lamp LED - for LiFi transmission (OUTPUT)

// ==================== TIMING CONSTANTS ====================

// SOS button cooldown period (3 minutes = 180,000 milliseconds)
const unsigned long SOS_COOLDOWN = 180000;

// LiFi rebroadcast interval for phone receivers (1 minute = 60,000 milliseconds)
const unsigned long LIFI_REBROADCAST_INTERVAL = 60000;

// Cache size for message deduplication
#define CACHE_SIZE 3

// ==================== MESSAGE TYPE DEFINITIONS ====================

/*
 * Message Type System:
 * 
 * Type '1' - BROADCAST (HQ → All Lamps)
 *   All lamps broadcast message to phones via LiFi
 * 
 * Type '2' - TARGETED BROADCAST (HQ → Specific Lamp)
 *   Only target lamp broadcasts to phones via LiFi
 * 
 * Type '3' - SOS (Lamp → HQ)
 *   Emergency alert routes silently to HQ (no phone broadcast)
 * 
 * Type '4' - MESSAGE (Node → HQ)
 *   Normal status/info messages to HQ (no phone broadcast)
 */

#define MSG_TYPE_BROADCAST '1'  // HQ → All lamps
#define MSG_TYPE_TARGETED  '2'  // HQ → Specific lamp
#define MSG_TYPE_SOS       '3'  // Lamp → HQ (emergency)
#define MSG_TYPE_MESSAGE   '4'  // Node → HQ (normal message)

// ==================== SOS CONFIGURATION ====================

// Pre-computed hash for SOS messages (all SOS send "HELP!")
#define SOS_MESSAGE "HELP!"
#define SOS_HASH    0x28F9  // simpleHash("HELP!") = 0x28F9

// ==================== DATA STRUCTURES ====================

/*
 * Message Cache Structure
 * Used for deduplication to prevent:
 *   - Infinite forwarding loops
 *   - Duplicate processing
 *   - Broadcast storms
 */
struct MsgCache {
  String src;       // Source node ID
  uint16_t msgHash; // Hash of message content
};

// ==================== GLOBAL VARIABLES (declared extern) ====================

// Cache array (defined in main.ino)
extern MsgCache cache[CACHE_SIZE];
extern int cacheIndex;

#endif // CONFIG_H
