#include <IRremote.h>
#define IR_RECV_PIN D5

void setup() {
  Serial.begin(115200);
  IrReceiver.begin(IR_RECV_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("Receiver Ready");
}

void loop() {
  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.protocol == NEC) {
      char c = (char)IrReceiver.decodedIRData.command;
      Serial.print(c);
    }
    IrReceiver.resume();
  }
}
