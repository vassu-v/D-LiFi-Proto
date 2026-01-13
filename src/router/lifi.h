// ===== main/lifi.h  (mesh + protocol, no LiFi) =====
#ifndef LIFI_H
#define LIFI_H

#include <Arduino.h>
#include "config.h"
#include "ir.h"

// ===== HASH =====
inline uint16_t simpleHash(String s){
  uint16_t h = 0;
  for (int i = 0; i < s.length(); i++) h = (h * 31) + s[i];
  return h;
}

// ===== CACHE =====
inline bool isNew(String src, uint16_t hash){
  for (int i = 0; i < CACHE_SIZE; i++){
    if (cache[i].src == src && cache[i].msgHash == hash) return false;
  }
  cache[cacheIndex].src = src;
  cache[cacheIndex].msgHash = hash;
  cacheIndex = (cacheIndex + 1) % CACHE_SIZE;
  return true;
}

// ===== RAW SEND =====
inline void irSendRaw(String header, String message = ""){
  const int txPins[] = {IR_TX_FRONT, IR_TX_RIGHT, IR_TX_BACK, IR_TX_LEFT};
  IrReceiver.stop();
  for(int i = 0; i < 4; i++){
    irSendString((header + " ").c_str(), txPins[i]);
    yield();
    if(message.length() > 0){
      delay(50); yield();
      irSendString((message + " ").c_str(), txPins[i]);
    }
    if(i < 3){
      delay(IR_DIRECTION_GAP);
      yield();
    }
  }
  IrReceiver.start();
}

// ===== PUBLIC SEND =====
inline void irSend(String header, String message = ""){
  irSendRaw(header, message);
  for(int i = 0; i < RETRANSMIT_QUEUE_SIZE; i++){
    if(!retransmitQueue[i].active){
      retransmitQueue[i] = {header, message, millis(), 1, true};
      return;
    }
  }
}

// ===== RETRANSMIT =====
inline void processRetransmitQueue(){
  unsigned long now = millis();
  for(int i = 0; i < RETRANSMIT_QUEUE_SIZE; i++){
    if(!retransmitQueue[i].active) continue;
    unsigned long elapsed = now - retransmitQueue[i].firstSentTime;
    if(elapsed > REDUNDANCY_WINDOW){
      retransmitQueue[i].active = false;
      continue;
    }
    unsigned long nextSendTime = retransmitQueue[i].sentCount * RETRANSMIT_INTERVAL;
    if(elapsed >= nextSendTime && retransmitQueue[i].sentCount < RETRANSMIT_COUNT){
      irSendRaw(retransmitQueue[i].header, retransmitQueue[i].message);
      retransmitQueue[i].sentCount++;
      yield();
    }
  }
}

// ===== RECEIVE PACKET =====
inline bool irReceive(String &header, String &message){
  static bool waiting = false;
  static String h = "";
  static unsigned long t = 0;
  String line;
  if(irReceiveString(line)){
    line.trim();
    if(!waiting){
      h = line;
      waiting = true;
      t = millis();
      return false;
    } else {
      header = h;
      message = line;
      waiting = false;
      h = "";
      return true;
    }
  }
  if(waiting && millis() - t > IR_MESSAGE_TIMEOUT){
    waiting = false;
    h = "";
  }
  return false;
}

// ===== INIT PROCESS =====
inline void processInit(String header){
  String initID = header.substring(4,6);
  uint8_t hop = header.substring(6,8).toInt();
  if(initID != lastInitID || hop+1 < myHop){
    lastInitID = initID;
    myHop = hop + 1;
  }
  char hopStr[3];
  sprintf(hopStr, "%02d", hop+1);
  irSend(header.substring(0,6) + String(hopStr) + MSG_TYPE_INIT);
}

// ===== SOS =====
inline void generateSOS(){
  char hopStr[3];
  sprintf(hopStr, "%02d", myHop);
  String header = String(NODE_ID) + HQ_ID + MSG_TYPE_SOS + String(hopStr);
  isNew(NODE_ID, 0);
  irSend(header);
}

// ===== FORWARD =====
inline void forwardPacket(String header, String message, String &latestMessage){
  if(header.length() < 9) return;
  String src = header.substring(0,4);
  String dst = header.substring(4,8);
  char type = header[8];

  if(type == MSG_TYPE_INIT && header.length() == HEADER_LENGTH_INIT){
    processInit(header);
    return;
  }

  if(type == MSG_TYPE_SOS && header.length() == HEADER_LENGTH_SOS){
    uint8_t msgHop = header.substring(9,11).toInt();
    if(myHop <= msgHop + GRADIENT_TOLERANCE && isNew(src,0)){
      uint8_t newHop = msgHop>0?msgHop-1:0;
      char hopStr[3]; sprintf(hopStr,"%02d",newHop);
      irSend(src + dst + type + String(hopStr));
    }
    return;
  }

  if(type == MSG_TYPE_MESSAGE && header.length() == HEADER_LENGTH_MESSAGE){
    String hashStr = header.substring(9,13);
    uint16_t h = strtol(hashStr.c_str(),NULL,16);
    if(simpleHash(message) != h) return;
    uint8_t msgHop = header.substring(13,15).toInt();
    if(myHop <= msgHop + GRADIENT_TOLERANCE && isNew(src,h)){
      uint8_t newHop = msgHop>0?msgHop-1:0;
      char hopStr[3]; sprintf(hopStr,"%02d",newHop);
      irSend(src + dst + type + hashStr + String(hopStr), message);
    }
    if(dst == HQ_ID && NODE_ID == HQ_ID){
      latestMessage = message;
    }
    return;
  }

  if(header.length() == HEADER_LENGTH_STANDARD){
    String hashStr = header.substring(9,13);
    uint16_t h = strtol(hashStr.c_str(),NULL,16);
    if(simpleHash(message) != h) return;
    if(isNew(src,h)){
      irSend(header, message);
    }
    if((type==MSG_TYPE_BROADCAST && dst==BROADCAST_ID && IS_FROM_HQ(src)) ||
       (type==MSG_TYPE_TARGETED && dst==NODE_ID && IS_FROM_HQ(src))){
      latestMessage = message;
    }
  }
}

#endif
