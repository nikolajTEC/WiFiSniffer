#include "tracking.h"
#include "secrets.h"
#include "calculations.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>

TrackedDevice devices[MAX_TRACKED];

void NodeReading::addSample(float d) {
    samples[sampleHead] = d;
    sampleHead = (sampleHead + 1) % AVG_SAMPLES;
    if (sampleCount < AVG_SAMPLES) sampleCount++;
    receivedAt = millis();
    valid = true;
}

float NodeReading::average() const {
    if (sampleCount == 0) return 0;
    float sum = 0;
    for (int i = 0; i < sampleCount; i++) sum += samples[i];
    return sum / sampleCount;
}

TrackedDevice* getDevice(const uint8_t* mac) {
    for (int i = 0; i < MAX_TRACKED; i++) {
        if (devices[i].active && memcmp(devices[i].mac, mac, 6) == 0) {
            return &devices[i];
        }
    }

    for (int i = 0; i < MAX_TRACKED; i++) {
        if (!devices[i].active) {
            memcpy(devices[i].mac, mac, 6);
            devices[i].active = true;
            return &devices[i];
        }
    }

    return nullptr;
}

// Check if a MAC is in the watched list
bool isWatched(const uint8_t* mac) {
    if (!FILTER_ENABLED) return true;
    for (int i = 0; i < WATCHED_COUNT; i++) {
        if (memcmp(mac, WATCHED_MACS[i], 6) == 0)
            return true;
    }
    return false;
}

void macToString(const uint8_t* mac, char* out) {
    snprintf(
        out,
        18,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0], mac[1], mac[2],
        mac[3], mac[4], mac[5]
    );
}

void printTimestamp() {
    Serial.printf("[%10lu ms] ", millis());
}

void processMasterReading(const ProbeReport& r) {

    if (!isWatched(r.mac))
        return;

    char macStr[18];
    macToString(r.mac, macStr);

    printTimestamp();

    Serial.printf(
        "[PROBE] %-12s | MAC: %s | RSSI: %4d dBm | Dist: %6.2f m\n",
        NODE_NAMES[r.nodeIndex],
        macStr,
        r.rssi,
        r.distance
    );

    TrackedDevice* dev = getDevice(r.mac);

    if (!dev)
        return;

    dev->readings[r.nodeIndex].addSample(r.distance);

    uint32_t now = millis();

    bool allValid = true;

    for (int i = 0; i < 3; i++) {
        if (!dev->readings[i].valid) { allValid = false; break; }
        if ((now - dev->readings[i].receivedAt) > STALE_MS) { allValid = false; break; }
    }

    if (!allValid) return;

    float r0 = dev->readings[0].average();
    float r1 = dev->readings[1].average();
    float r2 = dev->readings[2].average();

    if (r0 > 50 || r1 > 50 || r2 > 50) return;

    float posX, posY;

    if (trilaterate(r0, r1, r2, posX, posY)) {
        printTimestamp();
        Serial.printf(
            "[POS]   MAC: %s | X: %6.2f m | Y: %6.2f m | "
            "A: %.2f  B: %.2f  C: %.2f\n",
            macStr,
            posX,
            posY,
            r0,
            r1,
            r2
        );
    }
}
