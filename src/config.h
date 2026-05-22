#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "secrets.h"

// Set to 'false' to instantly silence debug prints across all modules
const bool ENABLE_CONSOLE_DEBUG = true;

const bool FILTER_ENABLED = true;
const uint32_t HOP_INTERVAL_MS = 350;
#define MAX_TRACKED  30
#define STALE_MS     15000
#define AVG_SAMPLES  3
#define SRC_MAC_OFFSET 10

// MQTT Topic Definitions built at compile time
#define MQTT_TOPIC       "/devices/" DEVICE_NAME "/Gruppe2ESP"
#define MQTT_CLIENT_ID   DEVICE_NAME "_esp32"

// Time Configuration Definitions
#define TIMEZONE         "CET-1CEST,M3.5.0,M10.5.0/3" // Europe/Copenhagen
#define NTP_SERVER       "pool.ntp.org"
#define NTP_SYNC_TIMEOUT_MS 8000

const uint8_t WATCHED_MACS[][6] = {
    {0xBC, 0x6E, 0xE2, 0x97, 0x98, 0x2A},
    {0x08, 0x9D, 0xF4, 0x92, 0x7A, 0xD9},
    {0xA8, 0x93, 0x4A, 0x03, 0x81, 0x75},
    {0xDC, 0xC4, 0x9C, 0x40, 0x16, 0xEB},
};
const uint8_t WATCHED_COUNT = sizeof(WATCHED_MACS) / sizeof(WATCHED_MACS[0]);

const uint8_t NODE_MACS[][6] = {
    {0xE8, 0x6B, 0xEA, 0xD4, 0x05, 0xB8}, // Index 0: Master (nikolajnode)
    {0xE8, 0x6B, 0xEA, 0xD3, 0x6B, 0xC8}, // Index 1: Slave B (carstennode)
    {0x40, 0x22, 0xD8, 0x07, 0x15, 0x10}  // Index 2: Slave C (lokenode)
};
const uint8_t NODE_COUNT = sizeof(NODE_MACS) / sizeof(NODE_MACS[0]);

const char* const NODE_NAMES[NODE_COUNT] = { "nikolajnode", "carstennode", "lokenode" };
const float NODE_POS[NODE_COUNT][2] = { {0.0f, 0.0f}, {0.0f, 4.0f}, {5.0f, 4.0f} };

struct ProbeReport {
    uint8_t  nodeIndex;
    uint8_t  mac[6];
    int8_t   rssi;
    float    distance;
    uint32_t timestamp;
};

struct NodeReading {
    float samples[AVG_SAMPLES];
    uint8_t sampleCount = 0;
    uint8_t sampleHead  = 0;
    uint32_t receivedAt = 0;
    bool valid = false;

    void addSample(float d) {
        samples[sampleHead] = d;
        sampleHead = (sampleHead + 1) % AVG_SAMPLES;
        if (sampleCount < AVG_SAMPLES) sampleCount++;
        receivedAt = millis();
        valid = true;
    }
    float average() const {
        if (sampleCount == 0) return 0;
        float sum = 0;
        for (int i = 0; i < sampleCount; i++) sum += samples[i];
        return sum / sampleCount;
    }
};

struct TrackedDevice {
    uint8_t mac[6];
    NodeReading readings[NODE_COUNT];
    bool active = false;
};

extern uint8_t nodeRole; 
extern uint8_t nodeIndex;

inline void macToString(const uint8_t* mac, char* out) {
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

inline void printTimestamp(const char* timeStr) {
    if (ENABLE_CONSOLE_DEBUG) {
        Serial.printf("[%s] ", timeStr);
    }
}

#endif