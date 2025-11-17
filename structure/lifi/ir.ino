#include <IRremote.h>

#define IR_SEND_PIN D2
#define IR_RECV_PIN D5

void setup() {
  Serial.begin(115200);
  IrSender.begin(IR_SEND_PIN, ENABLE_LED_FEEDBACK);
  IrReceiver.begin(IR_RECV_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("IR Half-Duplex Ready");
  delay(1000);
}

void loop() {
  // Listen first
  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.protocol == NEC) {
      char c = (char)IrReceiver.decodedIRData.command;
      Serial.print(c);
    }
    IrReceiver.resume();
  }

  // Then check if user wants to send something
  if (Serial.available()) {
    delay(10);
    String msg = Serial.readStringUntil('\n');
    sendMessage(msg.c_str());
  }
}

void sendMessage(const char* msg) {
  IrReceiver.stop(); // stop listening while sending
  while (*msg) {
    IrSender.sendNEC(0x00, *msg, 0);
    Serial.print("Sent: ");
    Serial.println(*msg);
    delay(100); // gap between characters
    msg++;
  }
  IrReceiver.start(); // <-- fixed
}
