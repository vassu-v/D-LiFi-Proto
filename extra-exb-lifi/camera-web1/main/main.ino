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

// -------- HTML (CAMERA + BRIGHTNESS LOGIC) --------
String pageHTML(){
  return String(
    "<html><body style='background:black;color:lime;font-family:monospace;text-align:center;'>"
    "<h2>Broadcast Viewer</h2>"
    "<video id='cam' autoplay playsinline style='width:90%;max-width:400px;border:2px solid lime;'></video><br>"
    "<canvas id='cv' style='display:none;'></canvas>"
    "<button onclick='go()'>Select & Decode</button>"
    "<p id=s>Idle</p>"
    "<p id=r></p>"
    "<script>"
    "let stream=null;"
    "let samples=0,totalBright=0;"
    "const BRIGHTNESS_THRESHOLD = 110;"   // change this
    "const CAMERA_TIME_MS = 3000;"         // change this

    "function getBrightness(){"
      "let v=document.getElementById('cam');"
      "let c=document.getElementById('cv');"
      "let ctx=c.getContext('2d');"
      "c.width=v.videoWidth; c.height=v.videoHeight;"
      "ctx.drawImage(v,0,0);"
      "let d=ctx.getImageData(0,0,c.width,c.height).data;"
      "let sum=0;"
      "for(let i=0;i<d.length;i+=4){sum+=(d[i]+d[i+1]+d[i+2])/3;}"
      "return sum/(d.length/4);"
    "}"

    "async function go(){"
      "document.getElementById('s').innerHTML='Requesting camera...';"
      "document.getElementById('r').innerHTML='';"
      "samples=0; totalBright=0;"
      "try{"
        "stream=await navigator.mediaDevices.getUserMedia({video:{facingMode:{ideal:'environment'}}});"
        "document.getElementById('cam').srcObject=stream;"
        "document.getElementById('s').innerHTML='Sampling brightness...';"
      "}catch(e){"
        "document.getElementById('s').innerHTML='Camera blocked';"
        "return;"
      "}"

      "let t0=Date.now();"
      "let iv=setInterval(()=>{"
        "let b=getBrightness(); totalBright+=b; samples++;"
        "if(Date.now()-t0>CAMERA_TIME_MS){"
          "clearInterval(iv);"
          "let avg=totalBright/samples;"
          "if(stream){stream.getTracks().forEach(t=>t.stop());}"
          "document.getElementById('cam').srcObject=null;"
          "document.getElementById('s').innerHTML='Avg brightness: '+avg.toFixed(1);"
          "if(avg>BRIGHTNESS_THRESHOLD){"
            "fetch('/last').then(r=>r.text()).then(t=>{"
              "document.getElementById('r').innerHTML='Last: '+t;"
            "});"
          "}else{"
            "document.getElementById('r').innerHTML='hgu7kt7itr2@@t';"
          "}"
        "}"
      "},200);"
    "}"
    "</script></body></html>"
  );
}

// -------- Web Handlers --------
void handleRoot(){ server.send(200, "text/html", pageHTML()); }

void handleLast(){
  Serial.print("[WEB] /last requested, sending: ");
  Serial.println(lastMessage);
  server.send(200, "text/plain", lastMessage);
}

// -------- WiFi Setup --------
void setupWiFi(){
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP", "emergency2025");
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP: "); Serial.println(myIP);
  server.on("/", handleRoot);
  server.on("/last", handleLast);
  server.begin();
}

// -------- SOS Button --------
void checkSOSButton(){
  bool now = digitalRead(SOS_BUTTON_PIN);
  if(lastSOSState==HIGH && now==LOW){
    Serial.println("[BTN] SOS pressed");
    generateSOS();
  }
  lastSOSState = now;
}

// -------- Status LED Blink --------
void updateStatusLED(){
  unsigned long now = millis();
  if(now-lastBlinkTime>=LED_BLINK_INTERVAL){
    lastBlinkTime=now;
    ledState=!ledState;
#if LED_INVERTED
    digitalWrite(LED_STATUS, ledState?LOW:HIGH);
#else
    digitalWrite(LED_STATUS, ledState?HIGH:LOW);
#endif
  }
}

// -------- Setup --------
void setup(){
  Serial.begin(115200);
  pinMode(SOS_BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_STATUS, OUTPUT);
  myHop = INITIAL_HOP;
  lastInitID = "";
  setupWiFi();
  irInit();
  Serial.println("[BOOT] Setup complete");
}

// -------- Loop --------
void loop(){
  server.handleClient();
  processRetransmitQueue();
  checkSOSButton();
  updateStatusLED();

  String header,message;
  if(irReceive(header,message)){
    forwardPacket(header,message,lastMessage);
  }
}
