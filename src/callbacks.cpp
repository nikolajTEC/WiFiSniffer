#include <Arduino.h>
#include <esp_now.h>
#include <cstring>
#include "callbacks.h"
#include "globals.h"
#include "calculations.h"
#include "tracking.h"

// ESP-NOW receive callback for the master node
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
    if (len != sizeof(ProbeReport))
        return;

    ProbeReport r;
    memcpy(&r, data, sizeof(r));
    processMasterReading(r);
}

// Promiscuous sniffer callback for probe requests
void IRAM_ATTR snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return;

    const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    const uint8_t* payload = pkt->payload;

    if (((payload[0] & 0x0C) >> 2) != 0) return;
    if (((payload[0] & 0xF0) >> 4) != 4) return;

    ProbeReport r;
    r.nodeIndex = nodeIndex;
    memcpy(r.mac, &payload[SRC_MAC_OFFSET], 6);
    r.rssi = pkt->rx_ctrl.rssi;
    r.distance = rssiToMeters(r.rssi);
    r.timestamp = millis();

    if (nodeRole == 0)
        processMasterReading(r);
    else
        esp_now_send(MASTER_MAC, (uint8_t*)&r, sizeof(r));
}
