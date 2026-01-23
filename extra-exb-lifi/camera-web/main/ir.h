// ===== main/ir.h =====
#ifndef IR_H
#define IR_H

#include <Arduino.h>
#include <IRremote.h>
#include "config.h"

inline void irInit() {
  IrReceiver.begin(IR_RX_PIN, ENABLE_LED_FEEDBACK);
  delay(100);
  yield();
}

inline void irSendString(const char* str, int txPin) {
  IrSender.begin(txPin, ENABLE_LED_FEEDBACK);
  while (*str) {
    IrSender.sendNEC(0x00, *str, 0);
    delay(100);
    yield();
    str++;
  }
}

inline bool irReceiveString(String &receivedLine) {
  static String buffer = "";
  static unsigned long lastCharTime = 0;
  const unsigned long TIMEOUT = 2000;

  if (buffer.length() > 0 && (millis() - lastCharTime > TIMEOUT)) {
    buffer = "";
  }

  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.protocol == NEC) {
      char c = (char)IrReceiver.decodedIRData.command;
      if (c == ' ') {
        receivedLine = buffer;
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

#endif
