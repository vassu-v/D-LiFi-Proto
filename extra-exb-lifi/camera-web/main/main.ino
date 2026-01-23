#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "config.h"
#include "ir.h"
#include "lifi.h"

// -------- Mesh Globals --------
MsgCache cache[CACHE_SIZE];
int cacheIndex = 0;
RetransmitEntry retransmitQueue[RETRANSMIT_QUEUE_SIZE];
String lastInitID = "";
uint8_t myHop = INITIAL_HOP;

// -------- WiFi/Web --------
ESP8266WebServer server(80);
String lastMessage = "No messages yet.";

// -------- SOS Button --------
#define SOS_BUTTON_PIN D6
bool lastSOSState = HIGH;

// -------- Status LED Blink --------
#define LED_BLINK_INTERVAL 500
unsigned long lastBlinkTime = 0;
bool ledState = false;

// -------- HTML (WITH CAMERA) --------
String pageHTML(){
  return String(
    "<html><body style='background:black;color:lime;font-family:monospace;text-align:center;'>"
    "<h2>Broadcast Viewer</h2>"
    "<video id='cam' autoplay playsinline style='width:90%;max-width:400px;border:2px solid lime;'></video><br>"
    "<button onclick='go()'>Select & Decode</button>"
    "<p id=s>Idle</p>"
    "<p id=r></p>"
    "<script>"
    "let stream=null;"
    "async function go(){"
      "document.getElementById('s').innerHTML='Requesting camera...';"
      "document.getElementById('r').innerHTML='';"
      "try{"
        "stream = await navigator.mediaDevices.getUserMedia({video:{facingMode:{ideal:'environment'}}});"
        "document.getElementById('cam').srcObject = stream;"
        "document.getElementById('s').innerHTML='Camera active...';"
      "}catch(e){"
        "document.getElementById('s').innerHTML='Camera blocked, continuing...';"
      "}"
      "setTimeout(async()=>{"
        "if(stream){stream.getTracks().forEach(t=>t.stop());}"
        "document.getElementById('cam').srcObject=null;"
        "document.getElementById('s').innerHTML='Fetching last broadcast...';"
        "let t = await fetch('/last').then(r=>r.text());"
        "document.getElementById('r').innerHTML='Last: '+t;"
        "document.getElementById('s').innerHTML='Done';"
      "},3000);"
    "}"
    "</script></body></html>"
  );
}

// -------- Web Handlers --------
void handleRoot(){
  server.send(200, "text/html", pageHTML());
}

void handleLast(){
  Serial.print("[WEB] /last requested, sending: ");
  Serial.println(lastMessage);
  server.send(200, "text/plain", lastMessage);
}

// -------- WiFi Setup --------
void setupWiFi(){
  Serial.print("Configuring access point...");
  WiFi.mode(WIFI_AP);
  delay(100);

  WiFi.softAP("ESP", "");
  delay(100);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print(" AP IP address: ");
  Serial.println(myIP);

  server.on("/", handleRoot);
  server.on("/last", handleLast);
  server.begin();
  Serial.println("HTTP server started");
}

// -------- SOS Button Check --------
void checkSOSButton(){
  bool now = digitalRead(SOS_BUTTON_PIN);
  if(lastSOSState == HIGH && now == LOW){
    Serial.println("[BTN] SOS pressed");
    generateSOS();
  }
  lastSOSState = now;
}

// -------- Status LED Blink --------
void updateStatusLED(){
  unsigned long now = millis();
  if(now - lastBlinkTime >= LED_BLINK_INTERVAL){
    lastBlinkTime = now;
    ledState = !ledState;
#if LED_INVERTED
    digitalWrite(LED_STATUS, ledState ? LOW : HIGH);
#else
    digitalWrite(LED_STATUS, ledState ? HIGH : LOW);
#endif
  }
}

// -------- Setup --------
void setup(){
  delay(1000);
  Serial.begin(115200);
  Serial.println();

  pinMode(SOS_BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_STATUS, OUTPUT);

  myHop = INITIAL_HOP;
  lastInitID = "";

  setupWiFi();
  delay(300); yield();

  irInit();
  delay(200); yield();

  Serial.println("[BOOT] Setup complete");
}

// -------- Loop --------
void loop(){
  server.handleClient();
  processRetransmitQueue();
  checkSOSButton();
  updateStatusLED();

  String header, message;
  if(irReceive(header, message)){
    Serial.println("=================================");
    Serial.println("[IR] Packet received!");
    Serial.print("[IR] Raw Header: ");
    Serial.println(header);
    Serial.print("[IR] Header length: ");
    Serial.println(header.length());
    Serial.print("[IR] Raw Message: ");
    Serial.println(message);
    Serial.print("[IR] Message length: ");
    Serial.println(message.length());

    Serial.print("[BEFORE] lastMessage = ");
    Serial.println(lastMessage);

    forwardPacket(header, message, lastMessage);

    Serial.print("[AFTER] lastMessage = ");
    Serial.println(lastMessage);
    Serial.println("=================================");
  }

  yield();
}
