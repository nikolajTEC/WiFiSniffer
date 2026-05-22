#include "networking.h"
#include "tracking.h"
#include "secrets.h"
#include <esp_now.h>
#include <esp_wifi.h>
#include "ca_cert.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>

const uint8_t CHANNELS[] = {1,2,3,4,5,6,7,8,9,10,11,12,13};
const uint8_t CHANNEL_COUNT = sizeof(CHANNELS);

static uint8_t currentChannelIndex = 0;
static uint32_t lastHopTime = 0;

WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);

static void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
    if (len != sizeof(ProbeReport)) return;

    ProbeReport r;
    memcpy(&r, data, sizeof(r));
    processMasterReading(r);
}

void IRAM_ATTR snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return;

    const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    const uint8_t* payload = pkt->payload;

    if (((payload[0] & 0x0C) >> 2) != 0 || ((payload[0] & 0xF0) >> 4) != 4) return;

    ProbeReport r;
    r.nodeIndex = nodeIndex;
    memcpy(r.mac, &payload[SRC_MAC_OFFSET], 6);
    r.rssi = pkt->rx_ctrl.rssi;
    r.distance = rssiToMeters(r.rssi);
    r.timestamp = millis();

    if (nodeRole == 0) {
        processMasterReading(r);
    } else {
        esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
        esp_now_send(NODE_MACS[0], (uint8_t*)&r, sizeof(r));
    }
}

// Exact implementation match of your layout's connectWiFi handler
bool connectWiFi() {
    if (ENABLE_CONSOLE_DEBUG) Serial.printf("Connecting to WiFi: %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 10000) {
            if (ENABLE_CONSOLE_DEBUG) Serial.println("\nWiFi timeout!");
            return false;
        }
        delay(250);
        if (ENABLE_CONSOLE_DEBUG) Serial.print(".");
    }
    if (ENABLE_CONSOLE_DEBUG) Serial.printf("\nWiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

// Exact implementation match of your layout's syncNTP handler
bool syncNTP() {
    configTzTime(TIMEZONE, NTP_SERVER);
    if (ENABLE_CONSOLE_DEBUG) Serial.print("Syncing NTP");

    struct tm ti{};
    unsigned long start = millis();
    while (!getLocalTime(&ti) || ti.tm_year < (2020 - 1900)) {
        if (millis() - start > NTP_SYNC_TIMEOUT_MS) {
            if (ENABLE_CONSOLE_DEBUG) Serial.println("\nNTP sync timeout!");
            return false;
        }
        delay(200);
        if (ENABLE_CONSOLE_DEBUG) Serial.print(".");
    }

    if (ENABLE_CONSOLE_DEBUG) {
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ti);
        Serial.printf("\nNTP synced: %s (Copenhagen)\n", buf);
    }
    return true;
}

// Exact implementation match of your layout's getCPHTimestamp helper
String getCPHTimestamp() {
    struct tm ti{};
    if (!getLocalTime(&ti)) return "1970-01-01 00:00:00";
    char buf[24];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ti);
    return String(buf);
}

// Exact implementation match of your layout's connectMQTT handler
bool connectMQTT() {
    secureClient.setCACert(MQTT_CA_CERT);
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setBufferSize(512);

    if (ENABLE_CONSOLE_DEBUG) Serial.printf("Connecting to MQTT %s:%d as %s ...\n", MQTT_HOST, MQTT_PORT, MQTT_USER);

    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
        if (ENABLE_CONSOLE_DEBUG) Serial.println("MQTT connected.");
        return true;
    }

    if (ENABLE_CONSOLE_DEBUG) Serial.printf("MQTT connect failed. State: %d\n", mqttClient.state());
    return false;
}

static void maintainNetworkMaster() {
    if (WiFi.status() != WL_CONNECTED) {
        if (connectWiFi()) {
            syncNTP();
        }
    }
    
    if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
        connectMQTT();
    }
    
    if (mqttClient.connected()) {
        mqttClient.loop();
    }
}

void initNetwork() {
    uint8_t ownMac[6];
    WiFi.mode(WIFI_STA);
    esp_wifi_get_mac(WIFI_IF_STA, ownMac);

    bool identityFound = false;
    for (uint8_t i = 0; i < NODE_COUNT; i++) {
        if (memcmp(ownMac, NODE_MACS[i], 6) == 0) {
            nodeRole = (i == 0) ? 0 : 1;
            nodeIndex = i;
            identityFound = true;
            break;
        }
    }

    if (!identityFound) {
        Serial.println("\n[BOOT] ERROR: Unregistered Local Network MAC Identity Layout Profile.");
        while (true) delay(1000);
    }

    if (nodeRole == 0) {
        if (ENABLE_CONSOLE_DEBUG) Serial.println("\n[BOOT] Init mode MASTER -> Running Cloud Network links.");
        if (connectWiFi()) {
            syncNTP();
            connectMQTT();
        }

        if (esp_now_init() != ESP_OK) {
            Serial.println("[ERROR] Core ESP-NOW engine runtime launch failed");
            return;
        }
        esp_now_register_recv_cb(onDataRecv);
    } else {
        if (ENABLE_CONSOLE_DEBUG) Serial.printf("\n[BOOT] Init mode SLAVE => Active tracking tag node: %s\n", NODE_NAMES[nodeIndex]);
        
        if (esp_now_init() != ESP_OK) {
            Serial.println("[ERROR] Core ESP-NOW engine runtime launch failed");
            return;
        }

        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, NODE_MACS[0], 6);
        peer.channel = 1; 
        peer.encrypt = false;
        esp_now_add_peer(&peer);

        esp_wifi_set_promiscuous(true);
        esp_wifi_set_promiscuous_rx_cb(&snifferCallback);
        esp_wifi_set_channel(CHANNELS[0], WIFI_SECOND_CHAN_NONE);
        lastHopTime = millis();
    }
}

void sendLocationToMQTT(const char* macStr, float x, float y) {
    if (nodeRole != 0) return; 

    if (!mqttClient.connected()) {
        if (ENABLE_CONSOLE_DEBUG) Serial.println("[MQTT] Cannot publish location data, connection lost.");
        return;
    }

    char payload[180];
    snprintf(payload, sizeof(payload), 
             "{\"mac\":\"%s\",\"timestamp\":\"%s\",\"x\":%.2f,\"y\":%.2f}", 
             macStr, getCPHTimestamp().c_str(), x, y);

    if (mqttClient.publish(MQTT_TOPIC, payload, /*retained=*/false)) {
        if (ENABLE_CONSOLE_DEBUG) {
            Serial.printf("Published → %s : %s\n", MQTT_TOPIC, payload);
        }
    } else {
        if (ENABLE_CONSOLE_DEBUG) Serial.println("MQTT publish failed.");
    }
}

void handleNetworkLoop() {
    if (nodeRole == 0) {
        maintainNetworkMaster();
    } else {
        if (millis() - lastHopTime >= HOP_INTERVAL_MS) {
            currentChannelIndex = (currentChannelIndex + 1) % CHANNEL_COUNT;
            esp_wifi_set_channel(CHANNELS[currentChannelIndex], WIFI_SECOND_CHAN_NONE);
            lastHopTime = millis();
        }
    }
}