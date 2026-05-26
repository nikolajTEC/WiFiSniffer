#include <Arduino.h>
#include <esp_now.h>
#include <cstring>
#include "callbacks.h"
#include "globals.h"
#include "calculations.h"
#include "tracking.h"

void macToString(const uint8_t* mac, char* out);

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (nodeRole != 1) return;

    char macStr[18];
    macToString(mac_addr, macStr);
    Serial.printf(
        "[ESP-NOW] Send result to master %s: %s\n",
        macStr,
        status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAILED"
    );
}

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

    if (nodeRole == 0) {
        processMasterReading(r);
    } else {
        char masterMacStr[18];
        macToString(MASTER_MAC, masterMacStr);
        Serial.printf(
            "[ESP-NOW] Slave trying to send to master %s | Node: %s | RSSI: %4d | Dist: %6.2f m\n",
            masterMacStr,
            NODE_NAMES[nodeIndex],
            r.rssi,
            r.distance
        );

        esp_err_t sendResult = esp_now_send(MASTER_MAC, (uint8_t*)&r, sizeof(r));
        if (sendResult != ESP_OK) {
            Serial.printf("[ESP-NOW] Immediate send failure: %d\n", sendResult);
        }
    }
}
