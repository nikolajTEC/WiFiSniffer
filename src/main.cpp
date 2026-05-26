#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "esp_wifi.h"
#include "mqtt/mqtt_test.h"
#include "mqtt/mqtt_ntp.h"
//  MAC FILTER
// ═══════════════════════════════════════════════════════════════
const bool FILTER_ENABLED = true;

const uint8_t WATCHED_MACS[][6] = {
    {0xBC, 0x6E, 0xE2, 0x97, 0x98, 0x2A},
    {0x08, 0x9D, 0xF4, 0x92, 0x7A, 0xD9},
    {0xA8, 0x93, 0x4A, 0x03, 0x81, 0x75},
    {0xDC, 0xC4, 0x9C, 0x40, 0x16, 0xEB},
};

const uint8_t WATCHED_COUNT = sizeof(WATCHED_MACS) / 6;

bool isWatched(const uint8_t* mac) {
    if (!FILTER_ENABLED) return true;

    for (int i = 0; i < WATCHED_COUNT; i++) {
        if (memcmp(mac, WATCHED_MACS[i], 6) == 0)
            return true;
    }

    return false;
}

// ═══════════════════════════════════════════════════════════════
//  NODE MACS
// ═══════════════════════════════════════════════════════════════
const uint8_t MAC_MASTER[] = {0xE8, 0x6B, 0xEA, 0xD4, 0x05, 0xB8};
const uint8_t MAC_NODE_B[] = {0xE8, 0x6B, 0xEA, 0xD3, 0x6B, 0xC8};
const uint8_t MAC_NODE_C[] = {0x40, 0x22, 0xD8, 0x07, 0x15, 0x10};

const char* NODE_NAMES[3] = {
    "nikolajnode",
    "carstennode",
    "lokenode"
};

uint8_t MASTER_MAC[6];

uint8_t nodeRole  = 1; // 0=master, 1=slave
uint8_t nodeIndex = 0;

// ═══════════════════════════════════════════════════════════════
//  NODE POSITIONS
// ═══════════════════════════════════════════════════════════════
const float NODE_POS[3][2] = {
    {0.0f, 0.0f}, // nikolajnode
    {0.0f, 4.0f}, // carstennode
    {5.0f, 4.0f}, // lokenode
};

// ═══════════════════════════════════════════════════════════════
//  DISTANCE ESTIMATION
// ═══════════════════════════════════════════════════════════════
const int   RSSI_REF  = -59;
const float PATH_LOSS = 2.3f;

float rssiToMeters(int8_t rssi) {
    return powf(10.0f, (float)(RSSI_REF - rssi) / (10.0f * PATH_LOSS));
}

// ═══════════════════════════════════════════════════════════════
//  ESP-NOW PACKET
// ═══════════════════════════════════════════════════════════════
struct ProbeReport {
    uint8_t  nodeIndex;
    uint8_t  mac[6];
    int8_t   rssi;
    float    distance;
    uint32_t timestamp;
};

// ═══════════════════════════════════════════════════════════════
//  TRACKING
// ═══════════════════════════════════════════════════════════════
#define MAX_TRACKED  30
#define STALE_MS     15000
#define AVG_SAMPLES  3

struct NodeReading {
    float samples[AVG_SAMPLES];
    uint8_t sampleCount = 0;
    uint8_t sampleHead  = 0;

    uint32_t receivedAt = 0;
    bool valid = false;

    void addSample(float d) {
        samples[sampleHead] = d;

        sampleHead = (sampleHead + 1) % AVG_SAMPLES;

        if (sampleCount < AVG_SAMPLES)
            sampleCount++;

        receivedAt = millis();
        valid = true;
    }

    float average() const {
        if (sampleCount == 0)
            return 0;

        float sum = 0;

        for (int i = 0; i < sampleCount; i++)
            sum += samples[i];

        return sum / sampleCount;
    }
};

struct TrackedDevice {
    uint8_t mac[6];
    NodeReading readings[3];
    bool active = false;
};

TrackedDevice devices[MAX_TRACKED];

TrackedDevice* getDevice(const uint8_t* mac) {

    for (int i = 0; i < MAX_TRACKED; i++) {
        if (devices[i].active &&
            memcmp(devices[i].mac, mac, 6) == 0) {
            return &devices[i];
        }
    }

    for (int i = 0; i < MAX_TRACKED; i++) {

        if (!devices[i].active) {

            memcpy(devices[i].mac, mac, 6);

            devices[i].active = true;

            for (int j = 0; j < 3; j++) {
                devices[i].readings[j].valid = false;
            }

            return &devices[i];
        }
    }

    return nullptr;
}

// ═══════════════════════════════════════════════════════════════
//  TRILATERATION
// ═══════════════════════════════════════════════════════════════
bool trilaterate(float r0, float r1, float r2,
                 float& outX, float& outY) {

    float x0 = NODE_POS[0][0];
    float y0 = NODE_POS[0][1];

    float x1 = NODE_POS[1][0];
    float y1 = NODE_POS[1][1];

    float x2 = NODE_POS[2][0];
    float y2 = NODE_POS[2][1];

    float A = 2.0f * (x1 - x0);
    float B = 2.0f * (y1 - y0);
    float C = r0*r0 - r1*r1 + x1*x1 - x0*x0 + y1*y1 - y0*y0;

    float D = 2.0f * (x2 - x0);
    float E = 2.0f * (y2 - y0);
    float F = r0*r0 - r2*r2 + x2*x2 - x0*x0 + y2*y2 - y0*y0;

    float det = A * E - B * D;

    if (fabsf(det) < 0.0001f)
        return false;

    outX = (C * E - B * F) / det;
    outY = (A * F - C * D) / det;

    return true;
}

// ═══════════════════════════════════════════════════════════════
//  PRINT HELPERS
// ═══════════════════════════════════════════════════════════════
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

// ═══════════════════════════════════════════════════════════════
//  PROCESS READING
// ═══════════════════════════════════════════════════════════════
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

        if (!dev->readings[i].valid) {
            allValid = false;
            break;
        }

        if ((now - dev->readings[i].receivedAt) > STALE_MS) {
            allValid = false;
            break;
        }
    }

    if (!allValid)
        return;

    float r0 = dev->readings[0].average();
    float r1 = dev->readings[1].average();
    float r2 = dev->readings[2].average();

    // sanity clamp
    if (r0 > 50 || r1 > 50 || r2 > 50)
        return;

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

// ═══════════════════════════════════════════════════════════════
//  ESP-NOW RECEIVE
// ═══════════════════════════════════════════════════════════════
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len)
{
    if (len != sizeof(ProbeReport))
        return;

    ProbeReport r;
    memcpy(&r, data, sizeof(r));

    processMasterReading(r);
}

// ═══════════════════════════════════════════════════════════════
//  CHANNEL HOPPING
// ═══════════════════════════════════════════════════════════════
const uint8_t CHANNELS[] = {
    1,2,3,4,5,6,7,8,9,10,11,12,13
};

const uint8_t CHANNEL_COUNT = sizeof(CHANNELS);

const uint32_t HOP_INTERVAL_MS = 350;

uint8_t currentChannelIndex = 0;
uint32_t lastHopTime = 0;

#define SRC_MAC_OFFSET 10

// ═══════════════════════════════════════════════════════════════
//  SNIFFER
// ═══════════════════════════════════════════════════════════════
void IRAM_ATTR snifferCallback(
    void* buf,
    wifi_promiscuous_pkt_type_t type
) {

    if (type != WIFI_PKT_MGMT)
        return;

    const wifi_promiscuous_pkt_t* pkt =
        (wifi_promiscuous_pkt_t*)buf;

    const uint8_t* payload = pkt->payload;

    // subtype check
    if (((payload[0] & 0x0C) >> 2) != 0)
        return;

    if (((payload[0] & 0xF0) >> 4) != 4)
        return;

    ProbeReport r;

    r.nodeIndex = nodeIndex;

    memcpy(r.mac, &payload[SRC_MAC_OFFSET], 6);

    r.rssi = pkt->rx_ctrl.rssi;

    r.distance = rssiToMeters(r.rssi);

    r.timestamp = millis();

    if (nodeRole == 0) {
        processMasterReading(r);
    } else {
        esp_now_send(
            MASTER_MAC,
            (uint8_t*)&r,
            sizeof(r)
        );
    }
}

// ═══════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════
void setup() {

    Serial.begin(115200);

    delay(1000);

    MqttNtp::connectMQTT();
    MqttTest::begin();
    WiFi.mode(WIFI_AP_STA);

    memcpy(MASTER_MAC, MAC_MASTER, 6);

    uint8_t ownMac[6];

    esp_wifi_get_mac(WIFI_IF_STA, ownMac);

    if (memcmp(ownMac, MAC_MASTER, 6) == 0) {

        nodeRole  = 0;
        nodeIndex = 0;

        Serial.println("\n[BOOT] MASTER => nikolajnode");

    } else if (memcmp(ownMac, MAC_NODE_B, 6) == 0) {

        nodeRole  = 1;
        nodeIndex = 1;

        Serial.println("\n[BOOT] SLAVE => carstennode");

    } else if (memcmp(ownMac, MAC_NODE_C, 6) == 0) {

        nodeRole  = 1;
        nodeIndex = 2;

        Serial.println("\n[BOOT] SLAVE => lokenode");

    } else {

        Serial.println("\n[BOOT] ERROR: Unknown MAC");

        while (true)
            delay(1000);
    }

    Serial.printf(
        "[BOOT] Own MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        ownMac[0], ownMac[1], ownMac[2],
        ownMac[3], ownMac[4], ownMac[5]
    );

    Serial.printf(
        "[BOOT] Node Name: %s\n",
        NODE_NAMES[nodeIndex]
    );

    char apName[32];

    snprintf(
        apName,
        sizeof(apName),
        "%s",
        NODE_NAMES[nodeIndex]
    );

    WiFi.softAP(apName, "12345678");

    memset(devices, 0, sizeof(devices));

    if (esp_now_init() != ESP_OK) {

        Serial.println("[ERROR] ESP-NOW init failed");

        return;
    }

    if (nodeRole == 0) {

        esp_now_register_recv_cb(onDataRecv);

    } else {

        esp_now_peer_info_t peer = {};

        memcpy(peer.peer_addr, MASTER_MAC, 6);

        peer.channel = 0;

        peer.encrypt = false;

        esp_now_add_peer(&peer);
    }

    esp_wifi_set_promiscuous(true);

    esp_wifi_set_promiscuous_rx_cb(&snifferCallback);

    esp_wifi_set_channel(
        CHANNELS[0],
        WIFI_SECOND_CHAN_NONE
    );

    lastHopTime = millis();

    Serial.println("\nSniffing probe requests...\n");
}

// ═══════════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════════
void loop() {
    MqttNtp::maintain();
    MqttTest::tick();
    //  MqttNtp::publish("/devices/device02/location", payload); den reele metode, efter vi har trilaterated.
    if (millis() - lastHopTime >= HOP_INTERVAL_MS) {

        currentChannelIndex =
            (currentChannelIndex + 1) % CHANNEL_COUNT;

        esp_wifi_set_channel(
            CHANNELS[currentChannelIndex],
            WIFI_SECOND_CHAN_NONE
        );

        lastHopTime = millis();
    }
}