// tracking.h
#ifndef TRACKING_H
#define TRACKING_H

#include <stdint.h>
#include <Arduino.h>

#define MAX_TRACKED  30
#define STALE_MS     15000
#define AVG_SAMPLES  3

// ESP-NOW packet sent between nodes
struct ProbeReport {
    uint8_t  nodeIndex;
    uint8_t  mac[6];
    int8_t   rssi;
    float    distance;
    uint32_t timestamp;
};

// Process a reading received for the master node
void processMasterReading(const ProbeReport& r);

struct NodeReading {
    float samples[AVG_SAMPLES];
    uint8_t sampleCount = 0;
    uint8_t sampleHead  = 0;

    uint32_t receivedAt = 0;
    bool valid = false;

    void addSample(float d);
    float average() const;
};

struct TrackedDevice {
    uint8_t mac[6];
    NodeReading readings[3];
    bool active = false;
};

extern TrackedDevice devices[MAX_TRACKED];

TrackedDevice* getDevice(const uint8_t* mac);

void macToString(const uint8_t* mac, char* out);

#endif // TRACKING_H
