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
volatile bool pendingSend = false;
volatile bool pendingSOS = false;
String pendingMessage = "";

// -------- HTML --------
String pageHTML(){
  return String(
    "<html><body>"
    "<h2>Last Message</h2><p>" + lastMessage + "</p>"
    "<form action='/send'>"
    "<input name='msg'>"
    "<button>Send</button></form><br>"
    "<form action='/sos'>"
    "<button style='color:red;font-size:20px'>SOS</button></form>"
    "</body></html>"
  );
}

// -------- Handlers --------
void handleRoot(){
  server.send(200, "text/html", pageHTML());
}

void handleSend(){
  String msg = server.arg("msg");
  Serial.print("[WEB] Send pressed, msg = ");
  Serial.println(msg);

  if(msg.length() > 0){
    lastMessage = msg;
    pendingMessage = msg;
    pendingSend = true;
    Serial.println("[WEB] Message queued for mesh send");
  }
  server.sendHeader("Location","/");
  server.send(303);
}

void handleSOS(){
  Serial.println("[WEB] SOS button pressed");
  pendingSOS = true;
  server.sendHeader("Location","/");
  server.send(303);
}

// -------- Mesh Inject --------
void sendMessageToHQ(String msg){
  Serial.print("[MESH] Preparing to send: ");
  Serial.println(msg);

  uint16_t h = simpleHash(msg);
  char hashStr[5]; sprintf(hashStr,"%04X",h);
  char hopStr[3];  sprintf(hopStr,"%02d",myHop);
  String header = String(NODE_ID) + HQ_ID + MSG_TYPE_MESSAGE + hashStr + hopStr;

  Serial.print("[MESH] Header = ");
  Serial.println(header);

  isNew(NODE_ID, h);
  irSend(header, msg);

  Serial.println("[MESH] irSend() called");
}

// -------- WiFi (Reference-Style) --------
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
  server.on("/send", handleSend);
  server.on("/sos", handleSOS);
  server.begin();
  Serial.println("HTTP server started");
}

// -------- Setup --------
void setup(){
  delay(1000);
  Serial.begin(115200);
  Serial.println();

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

  if(pendingSend){
    Serial.println("[LOOP] Pending message detected, sending now");
    sendMessageToHQ(pendingMessage);
    pendingSend = false;
  }

  if(pendingSOS){
    Serial.println("[LOOP] Pending SOS detected, sending now");
    generateSOS();
    pendingSOS = false;
  }

  String header, message;
  if(irReceive(header, message)){
    Serial.println("[IR] Packet received!");
    Serial.print("[IR] Header: ");
    Serial.println(header);
    Serial.print("[IR] Message: ");
    Serial.println(message);

    forwardPacket(header, message, lastMessage);
    Serial.println("[IR] Packet processed");
  }

  yield();
}
