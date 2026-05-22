// networking.h
#ifndef NETWORKING_H
#define NETWORKING_H

#include "config.h"

void initNetwork();
void handleNetworkLoop();
void sendLocationToMQTT(const char* macStr, float x, float y);

#endif