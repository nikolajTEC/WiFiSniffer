#include <Arduino.h>
#include "esp_wifi.h"
#include "esp_now.h"
#include "globals.h"
#include "mqtt/mqtt_ntp.h"

static void maintainEspNowPeer() {
    if (nodeRole != 1) return;

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, MASTER_MAC, 6);
    peer.channel = 0;
    peer.encrypt = false;

    esp_err_t err = esp_now_add_peer(&peer);
    if (err == ESP_OK) {
        Serial.println("[ESP-NOW] Master peer re-added");
    } else if (err == ESP_ERR_ESPNOW_EXIST) {
        return;
    } else {
        Serial.printf("[ESP-NOW] Peer maintain failed: %d\n", err);
    }
}

void loop() {
    if (nodeRole == 0) {
        MqttNtp::maintain();
    } else {
        maintainEspNowPeer();
    }

    if (millis() - lastHopTime >= HOP_INTERVAL_MS) {
        currentChannelIndex = (currentChannelIndex + 1) % CHANNEL_COUNT;
        esp_wifi_set_channel(CHANNELS[currentChannelIndex], WIFI_SECOND_CHAN_NONE);
        lastHopTime = millis();
    }
}
