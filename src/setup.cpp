#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "esp_wifi.h"
#include "secrets.h"
#include "mqtt/ca_cert.h"
#include "mqtt/mqtt_ntp.h"
#include "calculations.h"
#include "tracking.h"
#include "callbacks.h"
#include "globals.h"

void setup() {
    Serial.begin(115200);
    delay(1000);
    WiFi.mode(WIFI_AP_STA);

    memcpy(MASTER_MAC, MAC_MASTER, 6);

    uint8_t ownMac[6];
    esp_wifi_get_mac(WIFI_IF_STA, ownMac);

    if (memcmp(ownMac, MAC_MASTER, 6) == 0) {
        MqttNtp::connectMQTT();
        nodeRole  = 0;
        nodeIndex = 0;
    } else if (memcmp(ownMac, MAC_NODE_B, 6) == 0) {
        nodeRole  = 1;
        nodeIndex = 1;
    } else if (memcmp(ownMac, MAC_NODE_C, 6) == 0) {
        nodeRole  = 1;
        nodeIndex = 2;
    } else {
        Serial.println("\n[BOOT] ERROR: Unknown MAC");
        while (true) delay(1000);
    }

    Serial.printf(
        "[BOOT] Own MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        ownMac[0], ownMac[1], ownMac[2],
        ownMac[3], ownMac[4], ownMac[5]
    );

    // Print concise role + display name from secrets
    const char* roleStr = (nodeRole == 0) ? "Master" : "Slave";
    Serial.printf("[BOOT] Role: %s, Name: %s\n", roleStr, NODE_NAMES[nodeIndex]);

    // Use node name and configured password
    WiFi.softAP(NODE_NAMES[nodeIndex], WIFI_PASSWORD);

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

    Serial.println("\nSniffing probe requests...\n");
}
