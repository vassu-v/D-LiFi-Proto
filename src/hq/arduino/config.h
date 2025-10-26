#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==================== NODE CONFIGURATION ====================

// HQ Node ID (headquarters identifier)
#define NODE_ID      "000h"

// Reserved ID for broadcast messages (all nodes receive)
#define BROADCAST_ID "FFFF"

// Headquarters/Base Station ID (same as NODE_ID for HQ)
#define HQ_ID        "000h"

// ==================== PIN ASSIGNMENTS ====================

#define IR_TX_PIN      D1  // IR LED transmitter (OUTPUT)
#define IR_RX_PIN      D2  // IR receiver module (INPUT)
#define LED_STATUS     D4  // Status LED for visual feedback (OUTPUT)

// ==================== TIMING CONSTANTS ====================

// Cache size for message deduplication
#define CACHE_SIZE 8  // Larger cache for HQ (receives from all nodes)

// ==================== MESSAGE TYPE DEFINITIONS ====================

/*
 * Message Type System:
 * 
 * Type '1' - BROADCAST (HQ → All Lamps)
 *   HQ sends system-wide announcements
 * 
 * Type '2' - TARGETED BROADCAST (HQ → Specific Lamp)
 *   HQ tells specific lamp to broadcast message
 * 
 * Type '3' - SOS (Lamp → HQ)
 *   Emergency alert from lamps (HQ receives only)
 * 
 * Type '4' - MESSAGE (Node ↔ HQ or Node ↔ Node)
 *   Normal status/info messages
 */

#define MSG_TYPE_BROADCAST '1'  // HQ → All lamps
#define MSG_TYPE_TARGETED  '2'  // HQ → Specific lamp
#define MSG_TYPE_SOS       '3'  // Lamp → HQ (emergency)
#define MSG_TYPE_MESSAGE   '4'  // Node → HQ (normal message)

// ==================== DATA STRUCTURES ====================

/*
 * Message Cache Structure
 * Prevents duplicate message processing
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
