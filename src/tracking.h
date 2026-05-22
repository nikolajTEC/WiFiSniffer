// tracking.h
#ifndef TRACKING_H
#define TRACKING_H

#include "config.h"

bool isWatched(const uint8_t* mac);
float rssiToMeters(int8_t rssi);
void processMasterReading(const ProbeReport& r);

#endif