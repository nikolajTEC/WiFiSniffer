#include "mqtt_ntp.h"
#include "secrets.h"   // DEVICE_NAME, MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASS
#include "ca_cert.h"   // MQTT_CA_CERT

#include <WiFi.h>
#include <time.h>

// =============================================================================
//  mqtt_ntp.cpp
// =============================================================================

namespace {

  // ── Module-private network objects ─────────────────────────────────────────
  WiFiClientSecure secureClient;
  PubSubClient     mqttClient(secureClient);

  // ── Reconnect throttle ─────────────────────────────────────────────────────
  // Tracks when the last reconnect attempt was made so maintain() does not
  // hammer the broker if it is temporarily unreachable.
  unsigned long lastReconnectAttempt = 0;

  // ── Internal helpers ───────────────────────────────────────────────────────

  bool connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) return true;

    Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
      if (millis() - start > 10000) {
        Serial.println("\n[WiFi] Timeout!");
        return false;
      }
      delay(250);
      Serial.print(".");
    }
    Serial.printf("\n[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }

  bool syncNTP() {
    // Configure SNTP; the ESP32 background task handles periodic re-sync
    // automatically, so this only needs to be called once at boot.
    configTzTime(MqttNtp::TIMEZONE, MqttNtp::NTP_SERVER);
    Serial.print("[NTP] Syncing");

    struct tm ti{};
    unsigned long start = millis();
    while (!getLocalTime(&ti) || ti.tm_year < (2020 - 1900)) {
      if (millis() - start > MqttNtp::NTP_TIMEOUT_MS) {
        Serial.println("\n[NTP] Sync timeout!");
        return false;
      }
      delay(200);
      Serial.print(".");
    }

    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ti);
    Serial.printf("\n[NTP] Synced: %s\n", buf);
    return true;
  }

  // Opens a fresh MQTT session using credentials from secrets.h.
  // Separated from connectMQTT() so maintain() can call it on reconnects
  // without repeating the WiFi / NTP steps.
  bool mqttConnect() {
    Serial.printf("[MQTT] Connecting to %s:%d as '%s' ...\n",
                  MQTT_HOST, MQTT_PORT, DEVICE_NAME);

    if (mqttClient.connect(DEVICE_NAME "_esp32", MQTT_USER, MQTT_PASS)) {
      Serial.println("[MQTT] Connected.");
      return true;
    }

    Serial.printf("[MQTT] Failed. State code: %d\n", mqttClient.state());
    return false;
  }

} // anonymous namespace

// =============================================================================
//  MqttNtp::connectMQTT
//  Full boot sequence: WiFi → NTP → MQTT.
// =============================================================================
bool MqttNtp::connectMQTT() {
  // ── TLS: verify broker identity against our CA cert ────────────────────────
  secureClient.setCACert(MQTT_CA_CERT);

  // ── Point PubSubClient at the broker ──────────────────────────────────────
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setBufferSize(MQTT_BUFFER_BYTES);

  if (!connectWiFi()) return false;
  if (!syncNTP())     return false;
  return mqttConnect();
}

// =============================================================================
//  MqttNtp::maintain
//  Call every loop() iteration — never blocks for more than RECONNECT_DELAY_MS.
// =============================================================================
void MqttNtp::maintain() {
  if (mqttClient.connected()) {
    // ── Happy path: drive the PubSubClient state machine ──────────────────
    // Handles keep-alive pings and any incoming messages (subscriptions).
    mqttClient.loop();
    return;
  }

  // ── Connection is down: attempt reconnect after the throttle delay ─────────
  unsigned long now = millis();
  if (now - lastReconnectAttempt < RECONNECT_DELAY_MS) return;
  lastReconnectAttempt = now;

  Serial.println("[MQTT] Connection lost. Attempting reconnect...");

  // Re-check WiFi first — if the AP dropped, reconnect that too.
  if (!connectWiFi()) return;

  mqttConnect();   // single attempt; maintain() will retry next cycle if needed
}

// =============================================================================
//  MqttNtp::publish
// =============================================================================
bool MqttNtp::publish(const char* topic, const char* payload) {
  if (!mqttClient.connected()) {
    Serial.println("[MQTT] Publish skipped — not connected.");
    return false;
  }

  if (mqttClient.publish(topic, payload, /*retained=*/false)) {
    Serial.printf("[MQTT] Published → %s : %s\n", topic, payload);
    return true;
  }

  Serial.println("[MQTT] Publish failed.");
  return false;
}

// =============================================================================
//  MqttNtp::getTimestamp
// =============================================================================
String MqttNtp::getTimestamp() {
  struct tm ti{};
  if (!getLocalTime(&ti)) return "1970-01-01 00:00:00";
  char buf[24];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ti);
  return String(buf);
}

// =============================================================================
//  MqttNtp::disconnect
//  Only for intentional shutdown (e.g. before deep sleep).
// =============================================================================
void MqttNtp::disconnect() {
  mqttClient.disconnect();
  WiFi.disconnect(true);
  delay(100);
  Serial.println("[MQTT] Disconnected. WiFi off.");
}