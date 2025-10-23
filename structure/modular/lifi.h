#ifndef LIFI_H
#define LIFI_H

#include <Arduino.h>

void irSend(String header, String message);
void lifiTransmit(String message);
bool irReceive(String &header, String &message);

#endif // LIFI_H
