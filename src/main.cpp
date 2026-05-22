#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "esp_wifi.h"

// ═══════════════════════════════════════════════════════════════
//  CONFIGURE THIS BEFORE FLASHING EACH BOARD
//  Roles: MASTER (0) = does trilateration + sniffs
//         SLAVE  (1) = sniffs and reports to master
// ═══════════════════════════════════════════════════════════════
#define NODE_ROLE   0          // 0 = master (node_a), 1 = slave (node_b or node_c)
#define NODE_INDEX  0          // 0 = node_a, 1 = node_b, 2 = node_c

// ── Physical positions of all 3 nodes (metres) ───────────────
// Measure these once you place the boards in your space.
// Origin (0,0) = wherever you put node_a.
const float NODE_POS[3][2] = {
    {0.0f, 0.0f},   // node_a (master)
    {0.0f, 4.0f},   // node_b
    {5.0f, 4.0f},   // node_c
};

// ── Master MAC — slaves need this to send reports ─────────────
// Run the master once and read its MAC from the boot line, then
// paste it here before flashing the slaves.
uint8_t MASTER_MAC[] = {0xE8, 0x6B, 0xEA, 0xD4, 0x05, 0xB8};

// ── Distance estimation ───────────────────────────────────────
const int   RSSI_REF  = -59;
const float PATH_LOSS = 2.7f;

float rssiToMeters(int8_t rssi) {
    return powf(10.0f, (float)(RSSI_REF - rssi) / (10.0f * PATH_LOSS));
}

// ═══════════════════════════════════════════════════════════════
//  ESP-NOW PACKET
//  Both master and slaves use the same struct
// ═══════════════════════════════════════════════════════════════
struct ProbeReport {
    uint8_t  nodeIndex;       // which ESP32 saw this (0/1/2)
    uint8_t  mac[6];          // probing device's MAC
    int8_t   rssi;
    float    distance;
    uint32_t timestamp;       // millis() on the sending node
};

// ═══════════════════════════════════════════════════════════════
//  MASTER-ONLY: trilateration state
// ═══════════════════════════════════════════════════════════════
#if NODE_ROLE == 0

// Keep the latest reading per MAC per node.
// Simple fixed-size table — enough for a school demo.
#define MAX_TRACKED 30
#define STALE_MS    3000     // discard readings older than this

struct NodeReading {
    float    distance;
    uint32_t receivedAt;   // local millis() when we got this
    bool     valid;
};

struct TrackedDevice {
    uint8_t     mac[6];
    NodeReading readings[3];   // one slot per node (0/1/2)
    bool        active;
};

TrackedDevice devices[MAX_TRACKED];

// ── Find or create a slot for this MAC ───────────────────────
TrackedDevice* getDevice(const uint8_t* mac) {
    for (int i = 0; i < MAX_TRACKED; i++) {
        if (devices[i].active && memcmp(devices[i].mac, mac, 6) == 0)
            return &devices[i];
    }
    // Not found — grab an empty slot
    for (int i = 0; i < MAX_TRACKED; i++) {
        if (!devices[i].active) {
            memcpy(devices[i].mac, mac, 6);
            devices[i].active = true;
            for (int j = 0; j < 3; j++) devices[i].readings[j].valid = false;
            return &devices[i];
        }
    }
    return nullptr;   // table full
}

// ── Trilateration ─────────────────────────────────────────────
// Linearise the 3-circle system by subtracting equation 0 from
// equations 1 and 2, giving a 2×2 linear system → Cramer's rule.
//
//   Circle i: (x - xi)² + (y - yi)² = ri²
//
//   After subtraction:
//   2(x1-x0)·x + 2(y1-y0)·y = r0²-r1² + x1²-x0² + y1²-y0²
//   2(x2-x0)·x + 2(y2-y0)·y = r0²-r2² + x2²-x0² + y2²-y0²
//
bool trilaterate(float r0, float r1, float r2, float& outX, float& outY) {
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
    if (fabsf(det) < 1e-6f) return false;   // nodes are collinear, can't solve

    outX = (C * E - B * F) / det;
    outY = (A * F - C * D) / det;
    return true;
}

// ── Called whenever a reading arrives (from any node) ─────────
void processMasterReading(const ProbeReport& r) {
    // ── Always print the raw reading so something shows up ────
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             r.mac[0], r.mac[1], r.mac[2],
             r.mac[3], r.mac[4], r.mac[5]);

    Serial.printf("[PROBE]  node_%c  MAC: %s  |  RSSI: %4d dBm  |  ~%.1f m\n",
                  'a' + r.nodeIndex, macStr, r.rssi, r.distance);

    // ── Store reading ─────────────────────────────────────────
    TrackedDevice* dev = getDevice(r.mac);
    if (!dev) return;
    dev->readings[r.nodeIndex] = { r.distance, millis(), true };

    // ── Attempt trilateration only when all 3 nodes are fresh ─
    uint32_t now = millis();
    for (int i = 0; i < 3; i++) {
        if (!dev->readings[i].valid ||
            (now - dev->readings[i].receivedAt) > STALE_MS)
            return;   // not ready yet
    }

    float posX, posY;
    if (trilaterate(dev->readings[0].distance,
                    dev->readings[1].distance,
                    dev->readings[2].distance,
                    posX, posY)) {
        Serial.printf("[POS]   MAC: %s  |  X: %.2f m  Y: %.2f m\n",
                      macStr, posX, posY);
    }
}

// ── ESP-NOW receive callback (slave reports arriving) ─────────
void onDataRecv(const uint8_t* senderMac, const uint8_t* data, int len) {
    if (len != sizeof(ProbeReport)) return;
    ProbeReport r;
    memcpy(&r, data, sizeof(r));
    processMasterReading(r);
}

#endif // NODE_ROLE == 0

// ═══════════════════════════════════════════════════════════════
//  SHARED: channel hopping
// ═══════════════════════════════════════════════════════════════
const uint8_t  CHANNELS[]      = {1,2,3,4,5,6,7,8,9,10,11,12,13};
const uint8_t  CHANNEL_COUNT   = sizeof(CHANNELS);
const uint32_t HOP_INTERVAL_MS = 200;
uint8_t  currentChannelIndex   = 0;
uint32_t lastHopTime           = 0;

#define SRC_MAC_OFFSET 10

// ═══════════════════════════════════════════════════════════════
//  SHARED: probe sniffer
// ═══════════════════════════════════════════════════════════════
void IRAM_ATTR snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return;

    const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    const uint8_t* payload = pkt->payload;

    if (((payload[0] & 0x0C) >> 2) != 0) return;    // not management
    if (((payload[0] & 0xF0) >> 4) != 4) return;    // not probe request

    ProbeReport r;
    r.nodeIndex = NODE_INDEX;
    memcpy(r.mac, &payload[SRC_MAC_OFFSET], 6);
    r.rssi      = pkt->rx_ctrl.rssi;
    r.distance  = rssiToMeters(r.rssi);
    r.timestamp = millis();

#if NODE_ROLE == 0
    // Master handles its own readings directly (no ESP-NOW round-trip)
    processMasterReading(r);
#else
    // Slave sends the report to the master via ESP-NOW
    esp_now_send(MASTER_MAC, (uint8_t*)&r, sizeof(r));
#endif
}

// ═══════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    delay(500);

    WiFi.mode(WIFI_AP_STA);

#if NODE_ROLE == 0
    WiFi.softAP("ESP32-node_a", "12345678");
    Serial.printf("\n[MASTER] node_a  MAC: %s\n",
                  WiFi.macAddress().c_str());
    Serial.println("[MASTER] Paste the MAC above into MASTER_MAC on the slaves.");
    memset(devices, 0, sizeof(devices));
#else
    WiFi.softAP("ESP32-node_" + String((char)('a' + NODE_INDEX)), "12345678");
    Serial.printf("\n[SLAVE] node_%c  MAC: %s\n",
                  'a' + NODE_INDEX, WiFi.macAddress().c_str());
#endif

    // ── Init ESP-NOW ─────────────────────────────────────────
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERROR] ESP-NOW init failed");
        return;
    }

#if NODE_ROLE == 0
    esp_now_register_recv_cb(onDataRecv);
#else
    // Register master as a peer so we can send to it
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, MASTER_MAC, 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
#endif

    // ── Start sniffing ────────────────────────────────────────
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&snifferCallback);
    esp_wifi_set_channel(CHANNELS[0], WIFI_SECOND_CHAN_NONE);
    lastHopTime = millis();

    Serial.println("Sniffing probe requests...\n");
}

// ═══════════════════════════════════════════════════════════════
//  LOOP — just channel hopping
// ═══════════════════════════════════════════════════════════════
void loop() {
    if (millis() - lastHopTime >= HOP_INTERVAL_MS) {
        currentChannelIndex = (currentChannelIndex + 1) % CHANNEL_COUNT;
        esp_wifi_set_channel(CHANNELS[currentChannelIndex], WIFI_SECOND_CHAN_NONE);
        lastHopTime = millis();
    }
}