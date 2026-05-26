#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "esp_wifi.h"
#include "secrets.h"
#include "ca_cert.h"
#include "calculations.h"
#include "tracking.h"
#include "callbacks.h"

uint8_t MASTER_MAC[6];
uint8_t nodeRole  = 1; // 0=master, 1=slave
uint8_t nodeIndex = 0;

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

const uint8_t CHANNEL_COUNT = sizeof(CHANNELS) / sizeof(CHANNELS[0]);

const uint32_t HOP_INTERVAL_MS = 350;

uint8_t currentChannelIndex = 0;
uint32_t lastHopTime = 0;

#define SRC_MAC_OFFSET 10

// ═══════════════════════════════════════════════════════════════
//  SNIFFER CALLBACK
// ═══════════════════════════════════════════════════════════════
void IRAM_ATTR snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return;

    const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    const uint8_t* payload = pkt->payload;

    // Only handle probe requests (subtype 4, frame type management)
    if (((payload[0] & 0x0C) >> 2) != 0) return;
    if (((payload[0] & 0xF0) >> 4) != 4) return;

    ProbeReport r;
    r.nodeIndex = nodeIndex;
    memcpy(r.mac, &payload[SRC_MAC_OFFSET], 6);
    r.rssi = pkt->rx_ctrl.rssi;
    r.distance = rssiToMeters(r.rssi);
    r.timestamp = millis();

    if (nodeRole == 0) processMasterReading(r);
    else esp_now_send(MASTER_MAC, (uint8_t*)&r, sizeof(r));
}

// ═══════════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════════
void loop() {

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