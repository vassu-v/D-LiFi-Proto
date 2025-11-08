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

// Multi-HQ Support (optional additional headquarters)
// Uncomment and configure if multiple HQ stations are needed
// #define HQ_ID_2      "001h"
// #define HQ_ID_3      "002h"

// Helper macro to check if source is authorized HQ
// Add additional HQ IDs here if using multi-HQ setup
#define IS_FROM_HQ(src) ((src) == HQ_ID)
// For multi-HQ: #define IS_FROM_HQ(src) ((src) == HQ_ID || (src) == HQ_ID_2 || (src) == HQ_ID_3)

// ==================== PIN ASSIGNMENTS ====================

#define SOS_PIN        D3  // Pushbutton for SOS (INPUT_PULLUP, active LOW)

// Directional IR TX pins (4 directions for street lamp mesh)
#define IR_TX_FRONT    D1  // Forward direction
#define IR_TX_RIGHT    D5  // Right direction
#define IR_TX_BACK     D6  // Backward direction
#define IR_TX_LEFT     D7  // Left direction

#define IR_RX_PIN      D2  // IR receiver module (INPUT)
#define LED_STATUS     D4  // Status LED for visual feedback (OUTPUT)
#define LAMP_LIGHT_PIN D8  // Lamp LED - for LiFi transmission (OUTPUT)

// ==================== TIMING CONSTANTS ====================

// SOS button cooldown period (3 minutes = 180,000 milliseconds)
const unsigned long SOS_COOLDOWN = 180000;

// LiFi rebroadcast interval for phone receivers (1 minute = 60,000 milliseconds)
const unsigned long LIFI_REBROADCAST_INTERVAL = 60000;

// IR transmission timing (milliseconds)
const unsigned long IR_DIRECTION_GAP = 10;  // Gap between transmitting each direction

// ==================== REDUNDANCY & RELIABILITY ====================

// Number of times to retransmit a message in the first minute
// This ensures reliable delivery without ACKs in the initial critical period
#define RETRANSMIT_COUNT 3

// Interval between retransmissions (milliseconds)
// 60000ms / 3 retransmits = ~20 seconds between each send
const unsigned long RETRANSMIT_INTERVAL = 20000;  // 20 seconds

// Total redundancy window (first minute after message generation/reception)
const unsigned long REDUNDANCY_WINDOW = 60000;  // 1 minute

// Cache size for message deduplication
#define CACHE_SIZE 3

// ==================== GRADIENT SYSTEM ====================

// Gradient tolerance (K value)
// Allows forwarding from nodes up to K hops farther away
// Higher values = more redundancy, lower values = more selective forwarding
#define GRADIENT_TOLERANCE 1

// Initial hop value for nodes (max distance, uninitialized)
#define INITIAL_HOP 99

// ==================== MESSAGE TYPE DEFINITIONS ====================

/*
 * Message Type System:
 *
 * Type '0' - INIT (HQ → All Lamps)
 *   Builds gradient map, spreads outward from HQ
 *   Header: [src(4)][id(2)][hop(2)][0] = 9 chars
 *   No message content, no hash
 *   Hop increments as it spreads (HQ=00, adjacent=01, etc.)
 *
 * Type '1' - BROADCAST (HQ → All Lamps)
 *   All lamps broadcast message to phones via LiFi
 *   Header: [src(4)][dst(4)][type(1)][hash(4)] = 13 chars
 *   No gradient check, forwards normally
 *
 * Type '2' - TARGETED BROADCAST (HQ → Specific Lamp)
 *   Only target lamp broadcasts to phones via LiFi
 *   Header: [src(4)][dst(4)][type(1)][hash(4)] = 13 chars
 *   No gradient check, forwards normally
 *
 * Type '3' - SOS (Lamp → HQ)
 *   Emergency alert routes to HQ using gradient
 *   Header: [src(4)][dst(4)][type(1)][hop(2)] = 11 chars
 *   No hash, no message content
 *   Hop decrements toward HQ (floors at 0)
 *   Gradient check: only forward if myHop <= msgHop + K
 *
 * Type '4' - MESSAGE (Node → HQ)
 *   Normal status/info messages to HQ using gradient
 *   Header: [src(4)][dst(4)][type(1)][hash(4)][hop(2)] = 15 chars
 *   Has message content and hash
 *   Hop decrements toward HQ (floors at 0)
 *   Gradient check: only forward if myHop <= msgHop + K
 */

#define MSG_TYPE_INIT      '0'  // HQ → All lamps (gradient setup)
#define MSG_TYPE_BROADCAST '1'  // HQ → All lamps (broadcast to phones)
#define MSG_TYPE_TARGETED  '2'  // HQ → Specific lamp (targeted broadcast)
#define MSG_TYPE_SOS       '3'  // Lamp → HQ (emergency, header-only)
#define MSG_TYPE_MESSAGE   '4'  // Node → HQ (normal message with content)

// Header lengths for validation
#define HEADER_LENGTH_INIT     9   // Type 0 with id and hop
#define HEADER_LENGTH_STANDARD 13  // Types 1, 2 with hash
#define HEADER_LENGTH_SOS      11  // Type 3 with hop, no hash
#define HEADER_LENGTH_MESSAGE  15  // Type 4 with hash and hop

// ==================== SOS CONFIGURATION ====================

// SOS is header-only, no message content needed
// All SOS messages are identical emergency alerts
#define SOS_MESSAGE "SOS"  // For display purposes only, not transmitted

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

/*
 * Retransmission Tracker
 * Tracks messages that need redundant sending in first minute
 */
struct RetransmitEntry {
  String header;                    // Full header to retransmit
  String message;                   // Message content (empty for SOS/INIT)
  unsigned long firstSentTime;      // Timestamp of first transmission
  uint8_t sentCount;                // How many times sent so far
  bool active;                      // Is this slot in use?
};

// Maximum number of concurrent messages being retransmitted
#define RETRANSMIT_QUEUE_SIZE 3

// ==================== GLOBAL VARIABLES (declared extern) ====================

// Cache array (defined in main.ino)
extern MsgCache cache[CACHE_SIZE];
extern int cacheIndex;

// Retransmission queue (defined in main.ino)
extern RetransmitEntry retransmitQueue[RETRANSMIT_QUEUE_SIZE];

// Gradient system state (defined in main.ino)
extern String lastInitID;  // Last seen INIT ID
extern uint8_t myHop;      // This node's distance from HQ

#endif // CONFIG_H
