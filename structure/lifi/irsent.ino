#include <IRremote.h>
#define IR_SEND_PIN D2

void setup() {
  Serial.begin(115200);
  IrSender.begin(IR_SEND_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("Sender Ready");
  delay(1000);
}

void loop() {
  sendMessage("HELLO WORLD");
  delay(2000);
}

void sendMessage(const char* msg) {
  while (*msg) {
    IrSender.sendNEC(0x00, *msg, 0);
    Serial.print("Sent: ");
    Serial.println(*msg);
    delay(100);  // small gap between chars
    msg++;
  }
}
