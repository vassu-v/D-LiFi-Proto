#include <Arduino.h>
#include "config.h"
#include "lifi.h"

// ==================== BUTTON STATE TRACKING ====================

// Track last time SOS was sent (for cooldown enforcement)
unsigned long lastSOSTime = 0;

// Track button state for edge detection (prevents debounce issues)
bool lastButtonState = HIGH;  // HIGH = not pressed (INPUT_PULLUP)

// ==================== LIFI REBROADCAST TRACKING ====================

// Store the latest message to rebroadcast to phones via LiFi
String latestLiFiMessage = "";

// Track last time LiFi broadcast was sent
unsigned long lastLiFiBroadcastTime = 0;

// ==================== CACHE FUNCTIONS ====================

/*
 * Check if Message is New (Not in Cache)
 * 
 * Searches cache for a message with matching source ID and hash.
 * If found, message is a duplicate (return false).
 * If not found, adds to cache and returns true.
 * 
 * This prevents:
 *   - Nodes from forwarding the same message multiple times
 *   - Infinite loops in the mesh network
 *   - Broadcast storms
 * 
 * Cache is circular: oldest entries are overwritten when full.
 * 
 * Parameters:
 *   src  - Source node ID (4 characters)
 *   hash - Message hash (16-bit)
 * 
 * Returns:
 *   true  - Message is new, added to cache
 *   false - Message is duplicate, already in cache
 */

bool isNew(String src, uint16_t hash) {
  // Search cache for matching entry
  for (int i = 0; i < CACHE_SIZE; i++) {
    if (cache[i].src == src && cache[i].msgHash == hash) {
      return false; // Duplicate found, discard
    }
  }

  // Message is new, add to cache
  cache[cacheIndex].src = src;
  cache[cacheIndex].msgHash = hash;
  cacheIndex = (cacheIndex + 1) % CACHE_SIZE; // Circular increment
  return true;
}

// ==================== NODE FUNCTIONS ====================

/*
 * Generate SOS Emergency Message
 * Sends emergency alert to HQ via mesh (Type 3)
 * Does NOT broadcast to phones - only routes to HQ for coordination
 * 
 * Message Flow:
 *   1. Send Type 3 (SOS) to HQ via mesh routing
 *   2. Intermediate nodes forward it (mesh behavior)
 *   3. HQ receives and processes emergency alert
 *   4. HQ can then send Type 1 or Type 2 to broadcast info to phones if needed
 */

void generateSOS() {
  String msg = SOS_MESSAGE;  // "HELP!"
  uint16_t h = SOS_HASH;     // Pre-computed, no need to calculate
  char hashStr[5];
  sprintf(hashStr, "%04X", h);

  // Build header: Type 3 = SOS, destination = HQ
  String header = String(NODE_ID) + HQ_ID + MSG_TYPE_SOS + String(hashStr);

  isNew(NODE_ID, h);  // Add to cache to prevent re-forwarding own SOS
  irSend(header, msg);  // Send via IR mesh to HQ (no LiFi broadcast)

  // Visual feedback only - NO LiFi broadcast for SOS
  digitalWrite(LED_STATUS, HIGH);
  delay(200);
  digitalWrite(LED_STATUS, LOW);
  Serial.println("SOS sent to HQ (no phone broadcast)");
}

/*
 * Process and Forward Incoming Packet
 * 
 * Core mesh networking function:
 *   1. Validates header format
 *   2. Verifies message integrity (hash check)
 *   3. Forwards ALL messages via mesh (routing to destination)
 *   4. Processes messages based on type and destination
 * 
 * Header Format (13 chars): [src(4)][dst(4)][type(1)][hash(4)]
 * 
 * Message Type Processing:
 *   Type 1 (BROADCAST): HQ → All lamps, all nodes process
 *   Type 2 (COMMAND): HQ → Specific lamp, only destination processes
 *   Type 3 (SOS): Lamp → HQ, only HQ processes (emergency alert)
 *   Type 4 (DATA): Node → HQ or Node → Node, destination processes
 */

void forwardPacket(String header, String message) {
  // Validate header
  if (header.length() != 13) {
    Serial.println("Invalid header");
    return;
  }

  // Parse header
  String src = header.substring(0, 4);
  String dst = header.substring(4, 8);
  char type = header[8];
  String hashStr = header.substring(9, 13);
  uint16_t receivedHash = (uint16_t) strtol(hashStr.c_str(), NULL, 16);

  // Verify integrity: recompute hash and compare
  uint16_t computedHash = simpleHash(message);
  if (computedHash != receivedHash) {
    Serial.println("Corrupted message (hash mismatch) - discarded");
    return;  // Exit early, don't process bad data
  }

  // STEP 1: Forward via mesh (all nodes help route messages)
  if (isNew(src, receivedHash)) {
    // Optional: Uncomment next line if collision issues observed
    // delay(random(10, 100));  // Random backoff reduces collision probability
    irSend(header, message);
    digitalWrite(LED_STATUS, HIGH);
    delay(50);
    digitalWrite(LED_STATUS, LOW);
  }

  // STEP 2: Process message based on type and destination
  // Type 1: BROADCAST (HQ → All) - All nodes process
  if (type == MSG_TYPE_BROADCAST && dst == BROADCAST_ID) {
    Serial.println("=== BROADCAST FROM HQ ===");
    Serial.print("Message: ");
    Serial.println(message);
    // All nodes broadcast to phones
    latestLiFiMessage = message;
    lastLiFiBroadcastTime = millis();
    lifiTransmit(message);
  }
  // Type 2: COMMAND (HQ → Specific lamp) - Only destination processes
  else if (type == MSG_TYPE_TARGETED && dst == NODE_ID) {
    Serial.println("=== MESSAGE FROM HQ ===");
    Serial.print("Message: ");
    Serial.println(message);
    // Only this node broadcasts to phones
    latestLiFiMessage = message;
    lastLiFiBroadcastTime = millis();
    lifiTransmit(message);
    // Add command processing here (e.g., parse commands)
  }
  // Type 3: SOS (Lamp → HQ) - Only HQ processes with special alert
  else if (type == MSG_TYPE_SOS && dst == HQ_ID && NODE_ID == HQ_ID) {
    Serial.println("╔════════════════════════════╗");
    Serial.println("║   SOS ALERT RECEIVED       ║");
    Serial.println("╚════════════════════════════╝");
    Serial.print("From Node: "); Serial.println(src);
    Serial.print("Message: "); Serial.println(message);
    Serial.println("────────────────────────────");
    // HQ: Trigger alarm, log emergency, coordinate response
  }
  // Type 4: MESSAGE (Node → HQ) - HQ processes as normal message
  else if (type == MSG_TYPE_MESSAGE && dst == HQ_ID && NODE_ID == HQ_ID) {
    Serial.println("=== Message from Node ===");
    Serial.print("From: "); Serial.println(src);
    Serial.print("Message: "); Serial.println(message);
    // HQ logs/displays message (no special alert, just normal info)
  }
}

// ==================== SETUP ====================

void setup() {
  // Initialize hardware pins
  pinMode(SOS_PIN, INPUT_PULLUP);
  pinMode(IR_TX_PIN, OUTPUT);
  pinMode(IR_RX_PIN, INPUT);
  pinMode(LED_STATUS, OUTPUT);
  pinMode(LAMP_LIGHT_PIN, OUTPUT);

  // Initialize cache to empty state (prevent garbage data)
  for (int i = 0; i < CACHE_SIZE; i++) {
    cache[i].src = "";
    cache[i].msgHash = 0;
  }

  Serial.begin(115200);
  Serial.println("\n=== LiFi Mesh Lamp Node ===");
  Serial.print("Node ID: "); Serial.println(NODE_ID);
  Serial.print("Cooldown: "); Serial.print(SOS_COOLDOWN / 1000); Serial.println("s");
  Serial.println("===========================\n");

  // Startup LED flash
  digitalWrite(LED_STATUS, HIGH);
  digitalWrite(LAMP_LIGHT_PIN, HIGH);
  delay(500);
  digitalWrite(LED_STATUS, LOW);
  digitalWrite(LAMP_LIGHT_PIN, LOW);
}

// ==================== MAIN LOOP ====================

void loop() {
  // Non-blocking SOS button with edge detection and cooldown
  bool currentButtonState = digitalRead(SOS_PIN);
  // Check for falling edge (button press moment)
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    unsigned long timeSinceLastSOS = millis() - lastSOSTime;
    if (timeSinceLastSOS >= SOS_COOLDOWN) {
      generateSOS();
      lastSOSTime = millis();
    } else {
      Serial.print("Cooldown: ");
      Serial.print((SOS_COOLDOWN - timeSinceLastSOS) / 1000);
      Serial.println("s remaining");
    }
  }
  lastButtonState = currentButtonState;  // Remember state for next loop

  // Check for incoming messages via IR mesh
  String header, message;
  if (irReceive(header, message)) {
    forwardPacket(header, message);
  }

  // Periodic LiFi rebroadcast for phone receivers
  if (latestLiFiMessage != "" && 
     (millis() - lastLiFiBroadcastTime >= LIFI_REBROADCAST_INTERVAL)) {
    Serial.println("Rebroadcasting to phones...");
    lifiTransmit(latestLiFiMessage);
    lastLiFiBroadcastTime = millis();
  }

  delay(10);
}
