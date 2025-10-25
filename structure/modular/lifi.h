
#ifndef LIFI_H
#define LIFI_H

#include <Arduino.h>
#include "config.h"

// ==================== UTILITY FUNCTIONS ====================

/*
 * Simple Rolling Hash Function
 * Computes 16-bit hash for message deduplication and integrity verification
 */
uint16_t simpleHash(String s);

/*
 * Check if Message is New (Not in Cache)
 * Returns true if new, false if duplicate
 * Automatically adds new messages to cache
 */
bool isNew(String src, uint16_t hash);

// ==================== COMMUNICATION FUNCTIONS ====================

/*
 * IR Transmission (Node to Node Mesh)
 * Sends header and message via IR in two bursts
 * Currently placeholder - uses Serial output for testing
 */
void irSend(String header, String message);

/*
 * IR Reception (Node to Node Mesh)
 * Receives header and message via IR in two bursts
 * Currently placeholder - uses Serial input for testing
 * Format: Line 1 = header (13 chars), Line 2 = message
 */
bool irReceive(String &header, String &message);

/*
 * LiFi Broadcast (Node to Phones)
 * Broadcasts message to phones via lamp light modulation
 * Currently placeholder - flashes LED to indicate broadcast
 */
void lifiTransmit(String message);

// ==================== PROTOCOL FUNCTIONS ====================

/*
 * Generate SOS Emergency Message
 * Creates Type 3 message and sends to HQ via mesh
 * Does NOT broadcast to phones - only routes to HQ
 */
void generateSOS();

/*
 * Process and Forward Incoming Packet
 * Core mesh networking function:
 *   1. Validates header format
 *   2. Verifies message integrity (hash check)
 *   3. Forwards messages via mesh (routing)
 *   4. Processes messages based on type and destination
 */
void forwardPacket(String header, String message);

#endif // LIFI_H
