#include "mqtt_ntp.h"

// ── Credentials / certificates (project-local, not committed to VCS) ─────────
#include "secrets.h"   // MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASS
#include "ca_cert.h"   // MQTT_CA_CERT

#include <WiFi.h>
#include <time.h>

// =============================================================================
//  mqtt_ntp.cpp
//  Implementation of the MqttNtp namespace declared in mqtt_ntp.h
// =============================================================================

namespace {

  // ── Module-private network objects ────────────────────────────────────────
  // Declared in an anonymous namespace so they are invisible outside this TU.
  WiFiClientSecure secureClient;
  PubSubClient     mqttClient(secureClient);

} // anonymous namespace

// =============================================================================
//  MqttNtp::syncNTP
// =============================================================================
bool MqttNtp::syncNTP() {
  // Configure the ESP32 SNTP stack with the POSIX timezone string and the
  // NTP server address.  configTzTime() sets both the TZ env var and kicks
  // off the SNTP background task in one call.
  configTzTime(TIMEZONE, NTP_SERVER);
  Serial.print("[NTP] Syncing");

  struct tm ti{};
  unsigned long start = millis();

  // Poll until the RTC is set to a sane year, or the timeout expires.
  // A year < 2020 means the clock hasn't been updated yet (still at epoch 0).
  while (!getLocalTime(&ti) || ti.tm_year < (2020 - 1900)) {
    if (millis() - start > NTP_TIMEOUT_MS) {
      Serial.println("\n[NTP] Sync timeout!");
      return false;
    }
    delay(200);
    Serial.print(".");
  }

  // Log the synchronised time for debugging
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ti);
  Serial.printf("\n[NTP] Synced: %s (%s)\n", buf, TIMEZONE);
  return true;
}

// =============================================================================
//  MqttNtp::getTimestamp
// =============================================================================
String MqttNtp::getTimestamp() {
  struct tm ti{};

  // getLocalTime() fills the struct from the RTC.
  // Returns false if the clock has never been synced.
  if (!getLocalTime(&ti)) {
    return "1970-01-01 00:00:00";   // safe fallback
  }

  char buf[24];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ti);
  return String(buf);
}

// =============================================================================
//  MqttNtp::connectMQTT
// =============================================================================
bool MqttNtp::connectMQTT(const char* clientId) {
  // ── TLS setup ─────────────────────────────────────────────────────────────
  // Supply the broker's CA certificate so the TLS handshake can verify the
  // server identity.  Without this the connection would either fail or be
  // trivially MITM-able.
  secureClient.setCACert(MQTT_CA_CERT);

  // ── Broker address + buffer ───────────────────────────────────────────────
  // MQTT_HOST / MQTT_PORT come from secrets.h.
  // Bumping the buffer beyond the 256-byte default avoids silent truncation
  // of larger payloads (e.g. JSON with multiple fields).
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setBufferSize(MQTT_BUFFER_BYTES);

  Serial.printf("[MQTT] Connecting to %s:%d as client '%s' ...\n",
                MQTT_HOST, MQTT_PORT, clientId);

  // ── Authenticate and open session ─────────────────────────────────────────
  // MQTT_USER / MQTT_PASS come from secrets.h.
  if (mqttClient.connect(clientId, MQTT_USER, MQTT_PASS)) {
    Serial.println("[MQTT] Connected.");
    return true;
  }

  // PubSubClient state codes: https://pubsubclient.knolleary.net/api#state
  Serial.printf("[MQTT] Connection failed. State code: %d\n", mqttClient.state());
  return false;
}

// =============================================================================
//  MqttNtp::publish
// =============================================================================
bool MqttNtp::publish(const char* topic, const char* payload) {
  // Retained flag is false – the broker will not cache the message for late
  // subscribers.  Change to true if you need the last value to persist.
  if (mqttClient.publish(topic, payload, /*retained=*/false)) {
    Serial.printf("[MQTT] Published → %s : %s\n", topic, payload);
    return true;
  }

  Serial.println("[MQTT] Publish failed.");
  return false;
}

// =============================================================================
//  MqttNtp::disconnect
// =============================================================================
void MqttNtp::disconnect() {
  // ── Close MQTT session ────────────────────────────────────────────────────
  // Sends a DISCONNECT packet so the broker can clean up the session cleanly,
  // rather than waiting for the keep-alive timeout to expire.
  mqttClient.disconnect();

  // ── Bring down WiFi ───────────────────────────────────────────────────────
  // disconnect(true) powers down the radio immediately.
  // The brief delay lets the TCP stack flush the FIN/ACK before the radio
  // is cut – important before calling esp_deep_sleep_start().
  WiFi.disconnect(true);
  delay(100);

  Serial.println("[MQTT] Disconnected. WiFi off.");
}