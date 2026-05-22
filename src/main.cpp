#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "esp_wifi.h"

// ═══════════════════════════════════════════════════════════════
//  MAC FILTER
//  Set to false to see all devices, true to only see the list below
// ═══════════════════════════════════════════════════════════════
const bool FILTER_ENABLED = true;

const uint8_t WATCHED_MACS[][6] = {
    {0xBC, 0x6E, 0xE2, 0x97, 0x98, 0x2A},   // your device 1
    {0x08, 0x9D, 0xF4, 0x92, 0x7A, 0xD9},   // your device 2 (also master board)
    {0xA8, 0x93, 0x4A, 0x03, 0x81, 0x75},   // your device 3
};
const uint8_t WATCHED_COUNT = sizeof(WATCHED_MACS) / 6;

bool isWatched(const uint8_t* mac) {
    if (!FILTER_ENABLED) return true;
    for (int i = 0; i < WATCHED_COUNT; i++)
        if (memcmp(mac, WATCHED_MACS[i], 6) == 0) return true;
    return false;
}

// ═══════════════════════════════════════════════════════════════
//  KNOWN BOARD MACs — role is auto-detected at boot
//  master = 08:9D:F4:92:7A:D9
//  node_b = E8:6B:EA:D3:6B:C8
//  node_c = anything else
// ═══════════════════════════════════════════════════════════════
const uint8_t MAC_MASTER[] = {0x08, 0x9D, 0xF4, 0x92, 0x7A, 0xD9};
const uint8_t MAC_NODE_B[] = {0xE8, 0x6B, 0xEA, 0xD3, 0x6B, 0xC8};

uint8_t MASTER_MAC[6];   // filled at boot from MAC_MASTER
uint8_t nodeRole  = 1;   // 0 = master, 1 = slave — set in setup()
uint8_t nodeIndex = 2;   // 0/1/2 — set in setup()

// ── Physical positions of all 3 nodes (metres) ───────────────
const float NODE_POS[3][2] = {
    {0.0f, 0.0f},   // node_a master
    {0.0f, 4.0f},   // node_b
    {5.0f, 4.0f},   // node_c
};

// ── Distance estimation ───────────────────────────────────────
const int   RSSI_REF  = -59;
const float PATH_LOSS = 2.7f;

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
//  TRILATERATION STATE (used by master only, compiled on all)
// ═══════════════════════════════════════════════════════════════
#define MAX_TRACKED 30
#define STALE_MS    3000

struct NodeReading {
    float    distance;
    uint32_t receivedAt;
    bool     valid;
};

struct TrackedDevice {
    uint8_t     mac[6];
    NodeReading readings[3];
    bool        active;
};

TrackedDevice devices[MAX_TRACKED];

TrackedDevice* getDevice(const uint8_t* mac) {
    for (int i = 0; i < MAX_TRACKED; i++)
        if (devices[i].active && memcmp(devices[i].mac, mac, 6) == 0)
            return &devices[i];
    for (int i = 0; i < MAX_TRACKED; i++) {
        if (!devices[i].active) {
            memcpy(devices[i].mac, mac, 6);
            devices[i].active = true;
            for (int j = 0; j < 3; j++) devices[i].readings[j].valid = false;
            return &devices[i];
        }
    }
    return nullptr;
}

bool trilaterate(float r0, float r1, float r2, float& outX, float& outY) {
    float x0 = NODE_POS[0][0], y0 = NODE_POS[0][1];
    float x1 = NODE_POS[1][0], y1 = NODE_POS[1][1];
    float x2 = NODE_POS[2][0], y2 = NODE_POS[2][1];

    float A = 2.0f * (x1 - x0),  B = 2.0f * (y1 - y0);
    float C = r0*r0 - r1*r1 + x1*x1 - x0*x0 + y1*y1 - y0*y0;
    float D = 2.0f * (x2 - x0),  E = 2.0f * (y2 - y0);
    float F = r0*r0 - r2*r2 + x2*x2 - x0*x0 + y2*y2 - y0*y0;

    float det = A * E - B * D;
    if (fabsf(det) < 1e-6f) return false;

    outX = (C * E - B * F) / det;
    outY = (A * F - C * D) / det;
    return true;
}

void processMasterReading(const ProbeReport& r) {
    if (!isWatched(r.mac)) return;

    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             r.mac[0], r.mac[1], r.mac[2],
             r.mac[3], r.mac[4], r.mac[5]);

    Serial.printf("[PROBE]  node_%c  MAC: %s  |  RSSI: %4d dBm  |  ~%.1f m\n",
                  'a' + r.nodeIndex, macStr, r.rssi, r.distance);

    TrackedDevice* dev = getDevice(r.mac);
    if (!dev) return;
    dev->readings[r.nodeIndex] = { r.distance, millis(), true };

    uint32_t now = millis();
    for (int i = 0; i < 3; i++)
        if (!dev->readings[i].valid || (now - dev->readings[i].receivedAt) > STALE_MS)
            return;

    float posX, posY;
    if (trilaterate(dev->readings[0].distance,
                    dev->readings[1].distance,
                    dev->readings[2].distance,
                    posX, posY))
        Serial.printf("[POS]    MAC: %s  |  X: %.2f m  Y: %.2f m\n",
                      macStr, posX, posY);
}

void onDataRecv(const uint8_t* senderMac, const uint8_t* data, int len) {
    if (len != sizeof(ProbeReport)) return;
    ProbeReport r;
    memcpy(&r, data, sizeof(r));
    processMasterReading(r);
}

// ═══════════════════════════════════════════════════════════════
//  CHANNEL HOPPING
// ═══════════════════════════════════════════════════════════════
const uint8_t  CHANNELS[]      = {1,2,3,4,5,6,7,8,9,10,11,12,13};
const uint8_t  CHANNEL_COUNT   = sizeof(CHANNELS);
const uint32_t HOP_INTERVAL_MS = 200;
uint8_t  currentChannelIndex   = 0;
uint32_t lastHopTime           = 0;

#define SRC_MAC_OFFSET 10

// ═══════════════════════════════════════════════════════════════
//  SNIFFER
// ═══════════════════════════════════════════════════════════════
void IRAM_ATTR snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return;
    const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    const uint8_t* payload = pkt->payload;
    if (((payload[0] & 0x0C) >> 2) != 0) return;
    if (((payload[0] & 0xF0) >> 4) != 4) return;

    ProbeReport r;
    r.nodeIndex = nodeIndex;
    memcpy(r.mac, &payload[SRC_MAC_OFFSET], 6);
    r.rssi      = pkt->rx_ctrl.rssi;
    r.distance  = rssiToMeters(r.rssi);
    r.timestamp = millis();

    if (nodeRole == 0)
        processMasterReading(r);
    else
        esp_now_send(MASTER_MAC, (uint8_t*)&r, sizeof(r));
}

// ═══════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    delay(500);

    WiFi.mode(WIFI_AP_STA);

    // ── Detect role from own MAC ──────────────────────────────
    memcpy(MASTER_MAC, MAC_MASTER, 6);

    uint8_t ownMac[6];
    esp_wifi_get_mac(WIFI_IF_STA, ownMac);

    if (memcmp(ownMac, MAC_MASTER, 6) == 0) {
        nodeRole  = 0;
        nodeIndex = 0;
        Serial.println("\n[BOOT] Role: MASTER (node_a)");
    } else if (memcmp(ownMac, MAC_NODE_B, 6) == 0) {
        nodeRole  = 1;
        nodeIndex = 1;
        Serial.println("\n[BOOT] Role: SLAVE (node_b)");
    } else {
        nodeRole  = 1;
        nodeIndex = 2;
        Serial.println("\n[BOOT] Role: SLAVE (node_c)");
    }

    Serial.printf("[BOOT] Own MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  ownMac[0], ownMac[1], ownMac[2],
                  ownMac[3], ownMac[4], ownMac[5]);
    Serial.printf("[BOOT] Filter: %s\n\n", FILTER_ENABLED ? "ON" : "OFF");

    char apName[20];
    snprintf(apName, sizeof(apName), "ESP32-node_%c", 'a' + nodeIndex);
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
    esp_wifi_set_channel(CHANNELS[0], WIFI_SECOND_CHAN_NONE);
    lastHopTime = millis();

    Serial.println("Sniffing probe requests...\n");
}

// ═══════════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════════
void loop() {
    if (millis() - lastHopTime >= HOP_INTERVAL_MS) {
        currentChannelIndex = (currentChannelIndex + 1) % CHANNEL_COUNT;
        esp_wifi_set_channel(CHANNELS[currentChannelIndex], WIFI_SECOND_CHAN_NONE);
        lastHopTime = millis();
    }
}