#ifndef IR_H
#define IR_H

#include <Arduino.h>
#include <IRremote.h>
#include "config.h"

// ==================== IR COMMUNICATION LAYER ====================

/*
 * Initialize IR Hardware
 * Call this in setup() after Serial.begin()
 */
inline void irInit() {
  IrSender.begin(IR_TX_FRONT, ENABLE_LED_FEEDBACK);  // Use FRONT as primary
  IrReceiver.begin(IR_RX_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("IR transceiver initialized");
  delay(100);
}

/*
 * Send String via IR (Character-by-Character)
 * Uses NEC protocol with 0x00 address
 */
inline void irSendString(const char* str) {
  IrReceiver.stop();  // Stop listening while sending
  
  while (*str) {
    IrSender.sendNEC(0x00, *str, 0);
    delay(100);  // Gap between characters
    str++;
  }
  
  IrReceiver.start();  // Resume listening
}

/*
 * Receive Characters via IR (Non-blocking)
 * Returns true if a complete line is received
 * Accumulates characters until newline or timeout
 */
inline bool irReceiveString(String &receivedLine) {
  static String buffer = "";
  static unsigned long lastCharTime = 0;
  const unsigned long TIMEOUT = 2000;  // 2 second timeout between messages
  
  // Check for timeout (incomplete message)
  if (buffer.length() > 0 && (millis() - lastCharTime > TIMEOUT)) {
    Serial.println("IR RX timeout, clearing buffer");
    buffer = "";
  }
  
  // Try to decode incoming IR
  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.protocol == NEC) {
      char c = (char)IrReceiver.decodedIRData.command;
      
      if (c == '\n') {
        // End of message
        receivedLine = buffer;
        buffer = "";
        IrReceiver.resume();
        return true;  // Complete message received
      } else {
        // Accumulate character
        buffer += c;
        lastCharTime = millis();
      }
    }
    IrReceiver.resume();
  }
  
  return false;  // No complete message yet
}

#endif // IR_H
