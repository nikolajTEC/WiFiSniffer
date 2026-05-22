#include "tracking.h"
#include "networking.h"

TrackedDevice devices[MAX_TRACKED];

bool isWatched(const uint8_t* mac) {
    if (!FILTER_ENABLED) return true;
    for (int i = 0; i < WATCHED_COUNT; i++) {
        if (memcmp(mac, WATCHED_MACS[i], 6) == 0) return true;
    }
    return false;
}

float rssiToMeters(int8_t rssi) {
    const int RSSI_REF = -59;
    const float PATH_LOSS = 2.3f;
    return powf(10.0f, (float)(RSSI_REF - rssi) / (10.0f * PATH_LOSS));
}

static TrackedDevice* getDevice(const uint8_t* mac) {
    for (int i = 0; i < MAX_TRACKED; i++) {
        if (devices[i].active && memcmp(devices[i].mac, mac, 6) == 0) return &devices[i];
    }
    for (int i = 0; i < MAX_TRACKED; i++) {
        if (!devices[i].active) {
            memcpy(devices[i].mac, mac, 6);
            devices[i].active = true;
            for (int j = 0; j < NODE_COUNT; j++) devices[i].readings[j].valid = false;
            return &devices[i];
        }
    }
    return nullptr;
}

static bool trilaterate(float r0, float r1, float r2, float& outX, float& outY) {
    float x0 = NODE_POS[0][0], y0 = NODE_POS[0][1];
    float x1 = NODE_POS[1][0], y1 = NODE_POS[1][1];
    float x2 = NODE_POS[2][0], y2 = NODE_POS[2][1];

    float A = 2.0f * (x1 - x0);
    float B = 2.0f * (y1 - y0);
    float C = r0*r0 - r1*r1 + x1*x1 - x0*x0 + y1*y1 - y0*y0;

    float D = 2.0f * (x2 - x0);
    float E = 2.0f * (y2 - y0);
    float F = r0*r0 - r2*r2 + x2*x2 - x0*x0 + y2*y2 - y0*y0;

    float det = A * E - B * D;
    if (fabsf(det) < 0.0001f) return false;

    outX = (C * E - B * F) / det;
    outY = (A * F - C * D) / det;
    return true;
}

void processMasterReading(const ProbeReport& r) {
    if (!isWatched(r.mac)) return;

    char macStr[18];
    macToString(r.mac, macStr);
    
    // Switch to actual network clock timestamp instead of millis counters
    String cphTime = getCPHTimestamp();

    if (ENABLE_CONSOLE_DEBUG) {
        printTimestamp(cphTime.c_str());
        Serial.printf("[PROBE] %-12s | MAC: %s | RSSI: %4d dBm | Dist: %6.2f m\n",
                      NODE_NAMES[r.nodeIndex], macStr, r.rssi, r.distance);
    }

    TrackedDevice* dev = getDevice(r.mac);
    if (!dev) return;

    dev->readings[r.nodeIndex].addSample(r.distance);

    uint32_t now = millis();
    for (int i = 0; i < NODE_COUNT; i++) {
        if (!dev->readings[i].valid || (now - dev->readings[i].receivedAt) > STALE_MS) return;
    }

    float r0 = dev->readings[0].average();
    float r1 = dev->readings[1].average();
    float r2 = dev->readings[2].average();

    if (r0 > 50.0f || r1 > 50.0f || r2 > 50.0f) return;

    float posX, posY;
    if (trilaterate(r0, r1, r2, posX, posY)) {
        if (ENABLE_CONSOLE_DEBUG) {
            printTimestamp(cphTime.c_str());
            Serial.printf("[POS]   MAC: %s | X: %6.2f m | Y: %6.2f m | A: %.2f  B: %.2f  C: %.2f\n",
                          macStr, posX, posY, r0, r1, r2);
        }
        
        sendLocationToMQTT(macStr, posX, posY);
    }
}