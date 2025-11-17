#ifndef IR_H
#define IR_H

#include <Arduino.h>
#include <IRremote.h>
#include "config.h"

// ==================== IR COMMUNICATION LAYER ====================

inline void irInit() {
  #if DEBUG_IR_RX
    Serial.println(">>> IR Init: Starting receiver...");
  #endif
  
  IrReceiver.begin(IR_RX_PIN, ENABLE_LED_FEEDBACK);
  
  #if DEBUG_IR_RX
    Serial.println(">>> IR Init: Receiver ACTIVE on D" + String(IR_RX_PIN));
  #endif
  
  delay(100);
}

inline void irSendString(const char* str, int txPin) {
  #if DEBUG_IR_TX
    Serial.print(">>> IR TX: Pin D");
    Serial.print(txPin);
    Serial.print(" - '");
    Serial.print(str);
    Serial.println("'");
  #endif
  
  IrSender.begin(txPin, ENABLE_LED_FEEDBACK);
  
  int charCount = 0;
  while (*str) {
    IrSender.sendNEC(0x00, *str, 0);
    
    #if DEBUG_IR_TX && DEBUG_TIMING
      Serial.print("    Char ");
      Serial.print(charCount++);
      Serial.print(": '");
      Serial.print(*str);
      Serial.println("'");
    #endif
    
    delay(100);
    str++;
  }
}

inline bool irReceiveString(String &receivedLine) {
  static String buffer = "";
  static unsigned long lastCharTime = 0;
  const unsigned long TIMEOUT = 2000;
  
  if (buffer.length() > 0 && (millis() - lastCharTime > TIMEOUT)) {
    #if DEBUG_IR_RX
      Serial.println(">>> IR RX: TIMEOUT - Clearing buffer");
    #endif
    buffer = "";
  }
  
  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.protocol == NEC) {
      char c = (char)IrReceiver.decodedIRData.command;
      
      #if DEBUG_IR_RX
        Serial.print(">>> IR RX: Char '");
        Serial.print(c);
        Serial.println("'");
      #endif
      
      if (c == ' ') {
        receivedLine = buffer;
        
        #if DEBUG_IR_RX
          Serial.print(">>> IR RX: COMPLETE - '");
          Serial.print(receivedLine);
          Serial.println("'");
        #endif
        
        buffer = "";
        IrReceiver.resume();
        return true;
      } else {
        buffer += c;
        lastCharTime = millis();
      }
    }
    IrReceiver.resume();
  }
  
  return false;
}

#endif // IR_H
