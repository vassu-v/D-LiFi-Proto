#ifndef IR_H
#define IR_H

#include <Arduino.h>
#include <IRremote.h>
#include "config.h"

// ==================== IR COMMUNICATION LAYER ====================

/*
 * Initialize IR Hardware
 * Only initializes receiver (RX) - TX pins are initialized per-transmission
 * Call this in setup() after Serial.begin()
 */
inline void irInit() {
  #if DEBUG_IR_RX
    Serial.println(">>> IR Init: Starting receiver initialization...");
  #endif
  
  IrReceiver.begin(IR_RX_PIN, ENABLE_LED_FEEDBACK);
  
  #if DEBUG_IR_RX
    Serial.println(">>> IR Init: Receiver ACTIVE on pin D" + String(IR_RX_PIN));
    Serial.println(">>> IR Init: Ready to receive NEC protocol");
  #endif
  
  delay(100);
}

/*
 * Send String via IR (Character-by-Character) to Specific Pin
 * Uses NEC protocol with 0x00 address
 * 
 * @param str - Null-terminated string to send
 * @param txPin - Pin number to transmit from (e.g., IR_TX_FRONT)
 */
inline void irSendString(const char* str, int txPin) {
  #if DEBUG_IR_TX
    Serial.print(">>> IR TX: Initializing pin D");
    Serial.print(txPin);
    Serial.println("...");
  #endif
  
  // Initialize sender for this specific TX pin
  IrSender.begin(txPin, ENABLE_LED_FEEDBACK);
  
  #if DEBUG_IR_TX
    Serial.print(">>> IR TX: Sending '");
    Serial.print(str);
    Serial.print("' (");
    Serial.print(strlen(str));
    Serial.println(" chars)");
  #endif
  
  // Send each character
  int charCount = 0;
  while (*str) {
    IrSender.sendNEC(0x00, *str, 0);
    
    #if DEBUG_IR_TX && DEBUG_TIMING
      Serial.print("    Char ");
      Serial.print(charCount++);
      Serial.print(": '");
      Serial.print(*str);
      Serial.println("' sent");
    #endif
    
    delay(100);  // Gap between characters
    str++;
  }
  
  #if DEBUG_IR_TX
    Serial.println(">>> IR TX: Transmission complete");
  #endif
}

/*
 * Receive Characters via IR (Non-blocking)
 * Returns true if a complete line is received
 * Accumulates characters until space (' ') delimiter or timeout
 */
inline bool irReceiveString(String &receivedLine) {
  static String buffer = "";
  static unsigned long lastCharTime = 0;
  const unsigned long TIMEOUT = 2000;  // 2 second timeout between messages
  
  // Check for timeout (incomplete message)
  if (buffer.length() > 0 && (millis() - lastCharTime > TIMEOUT)) {
    #if DEBUG_IR_RX
      Serial.println(">>> IR RX: TIMEOUT - Clearing buffer (incomplete message)");
      Serial.print("    Buffer had: '");
      Serial.print(buffer);
      Serial.println("'");
    #endif
    buffer = "";
  }
  
  // Try to decode incoming IR
  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.protocol == NEC) {
      char c = (char)IrReceiver.decodedIRData.command;
      
      #if DEBUG_IR_RX
        Serial.print(">>> IR RX: Received char '");
        Serial.print(c);
        Serial.print("' (0x");
        Serial.print((int)c, HEX);
        Serial.println(")");
      #endif
      
      if (c == ' ') {
        // Space delimiter = end of message segment
        receivedLine = buffer;
        
        #if DEBUG_IR_RX
          Serial.println(">>> IR RX: COMPLETE SEGMENT RECEIVED");
          Serial.print("    Content: '");
          Serial.print(receivedLine);
          Serial.print("' (");
          Serial.print(receivedLine.length());
          Serial.println(" chars)");
        #endif
        
        buffer = "";
        IrReceiver.resume();
        return true;  // Complete message segment received
      } else {
        // Accumulate character
        buffer += c;
        lastCharTime = millis();
        
        #if DEBUG_IR_RX
          Serial.print("    Buffer now: '");
          Serial.print(buffer);
          Serial.print("' (");
          Serial.print(buffer.length());
          Serial.println(" chars)");
        #endif
      }
    }
    IrReceiver.resume();
  }
  
  return false;  // No complete message yet
}

#endif // IR_H
