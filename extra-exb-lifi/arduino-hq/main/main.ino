/*
 * LiFi Mesh HQ Node with Built-in Web Server
 * 
 * Features:
 * - WiFi Access Point (no internet needed!)
 * - Web interface for sending commands
 * - Displays received SOS messages
 * - Mobile-friendly design
 * 
 * Connect to WiFi:
 * SSID: LiFi-HQ
 * Password: emergency2025
 * 
 * Open browser: http://192.168.4.1
 * 
 * Compatible with V3 Lamp Nodes with gradient routing
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <IRremote.h>

// ==================== CONFIGURATION ====================

#define NODE_ID      "000h"  // HQ Node ID
#define BROADCAST_ID "FFFF"
#define HQ_ID        "000h"
#define HQ_HOP       0

// WiFi AP credentials
const char* ssid = "LiFi-HQ";
const char* password = "emergency2025";

// Pin definitions
#define IR_TX_FRONT    D2
#define IR_TX_RIGHT    D3
#define IR_TX_BACK     D0
#define IR_TX_LEFT     D7
#define IR_RX_PIN      D5

// Message types
#define MSG_TYPE_INIT      '0'
#define MSG_TYPE_BROADCAST '1'
#define MSG_TYPE_TARGETED  '2'
#define MSG_TYPE_SOS       '3'
#define MSG_TYPE_MESSAGE   '4'

// Header lengths
#define HEADER_LENGTH_INIT     9
#define HEADER_LENGTH_STANDARD 13
#define HEADER_LENGTH_SOS      11
#define HEADER_LENGTH_MESSAGE  15

// Cache for deduplication
#define CACHE_SIZE 1

// Timing
const unsigned long IR_DIRECTION_GAP = 100;
const unsigned long IR_MESSAGE_TIMEOUT = 3000;

// ==================== GLOBAL VARIABLES ====================

ESP8266WebServer server(80);

// Message cache
struct MsgCache {
  String src;
  uint16_t msgHash;
};
MsgCache cache[CACHE_SIZE];
int cacheIndex = 0;

// Message log (stores last 20 messages)
#define MAX_MESSAGES 20
struct MessageLog {
  String nodeID;
  String type;
  String content;
  unsigned long timestamp;
};
MessageLog messages[MAX_MESSAGES];
int messageCount = 0;

// ==================== UTILITY FUNCTIONS ====================

uint16_t simpleHash(String s) {
  uint16_t h = 0;
  for (int i = 0; i < s.length(); i++) {
    h = (h * 31) + s[i];
  }
  return h;
}

bool isNew(String src, uint16_t hash) {
  for (int i = 0; i < CACHE_SIZE; i++) {
    if (cache[i].src == src && cache[i].msgHash == hash) {
      return false; // Duplicate
    }
  }
  cache[cacheIndex].src = src;
  cache[cacheIndex].msgHash = hash;
  cacheIndex = (cacheIndex + 1) % CACHE_SIZE;
  return true;
}

// ==================== IR FUNCTIONS ====================

void irInit() {
  IrReceiver.begin(IR_RX_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("IR receiver initialized on D" + String(IR_RX_PIN));
}

void irSendString(const char* str, int txPin) {
  IrSender.begin(txPin, ENABLE_LED_FEEDBACK);
  while (*str) {
    IrSender.sendNEC(0x00, *str, 0);
    delay(100);
    str++;
  }
}

void irSendRaw(String header, String message = "") {
  const int txPins[] = {IR_TX_FRONT, IR_TX_RIGHT, IR_TX_BACK, IR_TX_LEFT};
  const char* dirNames[] = {"FRONT", "RIGHT", "BACK", "LEFT"};
  
  Serial.println("╔════════════════════════════════════╗");
  Serial.println("║   IR TX (4 DIRECTIONS)             ║");
  Serial.println("╚════════════════════════════════════╝");
  Serial.print("Header: ");
  Serial.println(header);
  if(message.length() > 0) {
    Serial.print("Message: ");
    Serial.println(message);
  }
  
  IrReceiver.stop();
  
  for(int i = 0; i < 4; i++) {
    Serial.print("Direction: ");
    Serial.println(dirNames[i]);
    
    String headerWithDelim = header + " ";
    irSendString(headerWithDelim.c_str(), txPins[i]);
    
    if(message.length() > 0) {
      delay(50);
      String messageWithDelim = message + " ";
      irSendString(messageWithDelim.c_str(), txPins[i]);
    }
    
    if(i < 3) delay(IR_DIRECTION_GAP);
  }
  
  IrReceiver.start();
  Serial.println("════════════════════════════════════\n");
}

bool irReceiveString(String &receivedLine) {
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

bool irReceive(String &header, String &message) {
  static bool waitingForMessage = false;
  static String receivedHeader = "";
  static unsigned long headerReceivedTime = 0;
  
  String line;
  if(irReceiveString(line)) {
    line.trim();
    
    // INIT (9 chars)
    if(line.length() == HEADER_LENGTH_INIT && line[8] == MSG_TYPE_INIT) {
      header = line;
      message = "";
      Serial.println("RX: INIT packet");
      if(waitingForMessage) {
        waitingForMessage = false;
        receivedHeader = "";
      }
      return true;
    }
    
    // SOS (11 chars)
    if(line.length() == HEADER_LENGTH_SOS && line[8] == MSG_TYPE_SOS) {
      header = line;
      message = "";
      Serial.println("RX: SOS packet");
      if(waitingForMessage) {
        waitingForMessage = false;
        receivedHeader = "";
      }
      return true;
    }
    
    // Two-segment messages
    if(!waitingForMessage) {
      if(line.length() == HEADER_LENGTH_STANDARD || line.length() == HEADER_LENGTH_MESSAGE) {
        receivedHeader = line;
        waitingForMessage = true;
        headerReceivedTime = millis();
        Serial.println("RX: Header received");
      }
      return false;
    } else {
      header = receivedHeader;
      message = line;
      waitingForMessage = false;
      receivedHeader = "";
      Serial.println("RX: Message received");
      return true;
    }
  }
  
  // Timeout check
  if(waitingForMessage && (millis() - headerReceivedTime > IR_MESSAGE_TIMEOUT)) {
    Serial.println("RX: Timeout, resetting");
    waitingForMessage = false;
    receivedHeader = "";
  }
  
  return false;
}

// ==================== PROTOCOL FUNCTIONS ====================

void sendInit(String initID) {
  char hopStr[3];
  sprintf(hopStr, "%02d", HQ_HOP);
  
  String header = String(NODE_ID) + initID + String(hopStr) + MSG_TYPE_INIT;
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║   SENDING INIT MESSAGE             ║");
  Serial.println("╚════════════════════════════════════╝");
  Serial.print("INIT ID: "); Serial.println(initID);
  Serial.print("Header: "); Serial.println(header);
  
  isNew(NODE_ID, 0);
  irSendRaw(header);
  
  Serial.println("✓ INIT transmitted\n");
}

void sendBroadcast(String message) {
  uint16_t hash = simpleHash(message);
  char hashStr[5];
  sprintf(hashStr, "%04X", hash);
  
  String header = String(NODE_ID) + BROADCAST_ID + MSG_TYPE_BROADCAST + String(hashStr);
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║   SENDING BROADCAST                ║");
  Serial.println("╚════════════════════════════════════╝");
  Serial.print("Message: "); Serial.println(message);
  Serial.print("Header: "); Serial.println(header);
  
  isNew(NODE_ID, hash);
  irSendRaw(header, message);
  
  Serial.println("✓ Broadcast transmitted\n");
}

void sendTargeted(String nodeID, String message) {
  uint16_t hash = simpleHash(message);
  char hashStr[5];
  sprintf(hashStr, "%04X", hash);
  
  String header = String(NODE_ID) + nodeID + MSG_TYPE_TARGETED + String(hashStr);
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║   SENDING TARGETED MESSAGE         ║");
  Serial.println("╚════════════════════════════════════╝");
  Serial.print("To: "); Serial.println(nodeID);
  Serial.print("Message: "); Serial.println(message);
  Serial.print("Header: "); Serial.println(header);
  
  isNew(NODE_ID, hash);
  irSendRaw(header, message);
  
  Serial.println("✓ Targeted message transmitted\n");
}

void addMessage(String nodeID, String type, String content) {
  if(messageCount >= MAX_MESSAGES) {
    // Shift old messages
    for(int i = 0; i < MAX_MESSAGES - 1; i++) {
      messages[i] = messages[i + 1];
    }
    messageCount = MAX_MESSAGES - 1;
  }
  
  messages[messageCount].nodeID = nodeID;
  messages[messageCount].type = type;
  messages[messageCount].content = content;
  messages[messageCount].timestamp = millis();
  messageCount++;
  
  Serial.print("Received: ");
  Serial.print(nodeID);
  Serial.print(" - ");
  Serial.println(content);
}

void processPacket(String header, String message) {
  if(header.length() < 9) return;
  
  String src = header.substring(0, 4);
  String dst = header.substring(4, 8);
  char type = header[8];
  
  // Type 3: SOS
  if(type == MSG_TYPE_SOS && header.length() == HEADER_LENGTH_SOS) {
    String hopStr = header.substring(9, 11);
    uint8_t msgHop = hopStr.toInt();
    
    if(isNew(src, 0)) {  // Deduplicate SOS
      Serial.println("\n[SOS ALERT RECEIVED]");
      Serial.print("From Node: "); Serial.println(src);
      Serial.print("Distance: "); Serial.print(msgHop); Serial.println(" hops");
      Serial.println("════════════════════════════════════\n");
      
      addMessage(src, "SOS", "Distance: " + String(msgHop) + " hops");
    }
  }
  
  // Type 4: MESSAGE
  else if(type == MSG_TYPE_MESSAGE && header.length() == HEADER_LENGTH_MESSAGE) {
    String hashStr = header.substring(9, 13);
    String hopStr = header.substring(13, 15);
    uint16_t receivedHash = (uint16_t) strtol(hashStr.c_str(), NULL, 16);
    uint8_t msgHop = hopStr.toInt();
    
    uint16_t computedHash = simpleHash(message);
    if(computedHash != receivedHash) {
      Serial.println(">>> ERROR: Hash mismatch");
      return;
    }
    
    if(isNew(src, receivedHash)) {
      Serial.println("\n[MESSAGE RECEIVED]");
      Serial.print("From Node: "); Serial.println(src);
      Serial.print("Distance: "); Serial.print(msgHop); Serial.println(" hops");
      Serial.print("Message: "); Serial.println(message);
      Serial.println("════════════════════════════════════\n");
      
      addMessage(src, "MSG", message + " (" + String(msgHop) + " hops)");
    }
  }
}

// ==================== WEB SERVER FUNCTIONS ====================

const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>LiFi HQ Control</title>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
  font-family: Arial, sans-serif;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  min-height: 100vh;
  padding: 10px;
}
.container {
  max-width: 600px;
  margin: 0 auto;
  background: white;
  border-radius: 15px;
  padding: 20px;
  box-shadow: 0 10px 30px rgba(0,0,0,0.3);
}
h1 {
  color: #667eea;
  text-align: center;
  margin-bottom: 20px;
  font-size: 24px;
}
.card {
  background: #f8f9fa;
  border-radius: 10px;
  padding: 15px;
  margin-bottom: 15px;
  border-left: 4px solid #667eea;
}
.card h2 {
  font-size: 18px;
  color: #333;
  margin-bottom: 10px;
}
input, select, textarea {
  width: 100%;
  padding: 10px;
  margin: 5px 0 10px 0;
  border: 2px solid #ddd;
  border-radius: 5px;
  font-size: 16px;
}
button {
  width: 100%;
  padding: 12px;
  background: #667eea;
  color: white;
  border: none;
  border-radius: 5px;
  font-size: 16px;
  font-weight: bold;
  cursor: pointer;
}
button:active { background: #5568d3; }
.sos-alert {
  background: #fee;
  border-left-color: #f44;
}
.message-item {
  background: white;
  padding: 10px;
  margin: 5px 0;
  border-radius: 5px;
  border-left: 3px solid #f44;
}
.status {
  text-align: center;
  padding: 10px;
  background: #d4edda;
  border-radius: 5px;
  margin: 10px 0;
  display: none;
}
</style>
</head>
<body>
<div class="container">
  <h1>LiFi HQ Control Panel</h1>
  
  <div id="status" class="status"></div>
  
  <div class="card">
    <h2>Initialize Network</h2>
    <input type="text" id="initID" placeholder="INIT ID (2 chars)" maxlength="2" value="01">
    <button onclick="sendInit()">Send INIT</button>
  </div>
  
  <div class="card">
    <h2>Broadcast to All Lamps</h2>
    <textarea id="broadcastMsg" rows="2" placeholder="Enter broadcast message..."></textarea>
    <button onclick="sendBroadcast()">Send Broadcast</button>
  </div>
  
  <div class="card">
    <h2>Target Specific Lamp</h2>
    <input type="text" id="targetNode" placeholder="Node ID (e.g., 102a)" maxlength="4">
    <textarea id="targetMsg" rows="2" placeholder="Enter targeted message..."></textarea>
    <button onclick="sendTargeted()">Send Targeted</button>
  </div>
  
  <div class="card sos-alert">
    <h2>Received Messages</h2>
    <button onclick="refreshMessages()" style="background:#f44; margin-bottom:10px;">Refresh Messages</button>
    <div id="messages"></div>
  </div>
</div>

<script>
function showStatus(msg) {
  const s = document.getElementById('status');
  s.textContent = msg;
  s.style.display = 'block';
  setTimeout(() => s.style.display = 'none', 3000);
}

function sendInit() {
  const id = document.getElementById('initID').value;
  if(id.length !== 2) {
    alert('INIT ID must be 2 characters!');
    return;
  }
  fetch('/sendInit?id=' + id)
    .then(r => r.text())
    .then(data => showStatus('INIT sent: ' + id));
}

function sendBroadcast() {
  const msg = document.getElementById('broadcastMsg').value;
  if(!msg) {
    alert('Enter a message!');
    return;
  }
  fetch('/sendBroadcast?msg=' + encodeURIComponent(msg))
    .then(r => r.text())
    .then(data => {
      showStatus('Broadcast sent');
      document.getElementById('broadcastMsg').value = '';
    });
}

function sendTargeted() {
  const node = document.getElementById('targetNode').value;
  const msg = document.getElementById('targetMsg').value;
  if(node.length !== 4 || !msg) {
    alert('Node ID must be 4 chars and message required!');
    return;
  }
  fetch('/sendTargeted?node=' + node + '&msg=' + encodeURIComponent(msg))
    .then(r => r.text())
    .then(data => {
      showStatus('Targeted message sent');
      document.getElementById('targetNode').value = '';
      document.getElementById('targetMsg').value = '';
    });
}

function refreshMessages() {
  fetch('/getMessages')
    .then(r => r.json())
    .then(data => {
      const div = document.getElementById('messages');
      if(data.length === 0) {
        div.innerHTML = '<p style="color:#888;">No messages yet</p>';
        return;
      }
      let html = '';
      for(let i = data.length - 1; i >= 0; i--) {
        const m = data[i];
        html += '<div class="message-item">';
        html += '<strong>' + m.nodeID + '</strong> - ' + m.type + '<br>';
        html += m.content;
        html += '</div>';
      }
      div.innerHTML = html;
    });
}

// Auto-refresh messages every 5 seconds
setInterval(refreshMessages, 5000);
refreshMessages();
</script>
</body>
</html>
)=====";

void handleRoot() {
  server.send_P(200, "text/html", MAIN_page);
}

void handleSendInit() {
  if(server.hasArg("id")) {
    String id = server.arg("id");
    sendInit(id);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing ID");
  }
}

void handleSendBroadcast() {
  if(server.hasArg("msg")) {
    String msg = server.arg("msg");
    sendBroadcast(msg);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing message");
  }
}

void handleSendTargeted() {
  if(server.hasArg("node") && server.hasArg("msg")) {
    String node = server.arg("node");
    String msg = server.arg("msg");
    sendTargeted(node, msg);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleGetMessages() {
  String json = "[";
  for(int i = 0; i < messageCount; i++) {
    if(i > 0) json += ",";
    json += "{\"nodeID\":\"" + messages[i].nodeID + "\",";
    json += "\"type\":\"" + messages[i].type + "\",";
    json += "\"content\":\"" + messages[i].content + "\",";
    json += "\"timestamp\":" + String(messages[i].timestamp) + "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

// ==================== SETUP ====================

void setup() {
  delay(1000);  // CRITICAL: Wait before anything else
  Serial.begin(115200);
  delay(100);
  Serial.println();
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║   LiFi Mesh HQ with Web Server    ║");
  Serial.println("╚════════════════════════════════════╝\n");
  
  // Initialize cache FIRST
  for(int i = 0; i < CACHE_SIZE; i++) {
    cache[i].src = "";
    cache[i].msgHash = 0;
  }
  
  // Setup WiFi AP (BEFORE IR init, like your reference)
  Serial.print("Configuring access point...");
  WiFi.mode(WIFI_AP);
  delay(100);
  yield();  // Important!
  
  bool apStarted = WiFi.softAP(ssid, password);
  delay(100);
  yield();  // Important!
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print(" AP IP address: ");
  Serial.println(IP);
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/sendInit", handleSendInit);
  server.on("/sendBroadcast", handleSendBroadcast);
  server.on("/sendTargeted", handleSendTargeted);
  server.on("/getMessages", handleGetMessages);
  
  server.begin();
  Serial.println("HTTP server started");
  
  delay(300);
  yield();  // Important!
  
  // Initialize pins AFTER WiFi
  pinMode(IR_TX_FRONT, OUTPUT);
  pinMode(IR_TX_RIGHT, OUTPUT);
  pinMode(IR_TX_BACK, OUTPUT);
  pinMode(IR_TX_LEFT, OUTPUT);
  pinMode(IR_RX_PIN, INPUT);
  
  // Initialize IR LAST
  irInit();
  delay(200);
  yield();  // Important!
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.print("║ SSID: ");
  Serial.print(ssid);
  for(int i = strlen(ssid); i < 28; i++) Serial.print(" ");
  Serial.println("║");
  Serial.print("║ Password: ");
  Serial.print(password);
  for(int i = strlen(password); i < 21; i++) Serial.print(" ");
  Serial.println("║");
  Serial.print("║ IP: ");
  Serial.print(IP);
  Serial.print("                    ║");
  Serial.println();
  Serial.println("╚════════════════════════════════════╝\n");
  Serial.println("Setup complete!");
}

// ==================== MAIN LOOP ====================

void loop() {
  // Handle web requests
  server.handleClient();
  
  // Check for incoming IR messages
  String header, message;
  if(irReceive(header, message)) {
    processPacket(header, message);
  }
  
  yield();  // Important for ESP8266 stability
  delay(10);
}
