#pragma once

// ── Standard / Arduino includes ───────────────────────────────────────────────
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// =============================================================================
//  mqtt_ntp.h
//
//  Self-contained utility for:
//    • NTP time-sync with a fixed IANA timezone (defaults to Europe/Copenhagen)
//    • TLS-secured MQTT connection and single-publish helper
//
//  Required companion files (not supplied here):
//    secrets.h  – must define: WIFI_SSID, WIFI_PASSWORD,
//                              MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASS
//    ca_cert.h  – must define: MQTT_CA_CERT  (PEM string of broker CA cert)
//
//  Usage example:
//    #include "mqtt_ntp.h"
//
//    void setup() {
//      if (MqttNtp::syncNTP())           { /* time is ready */ }
//      if (MqttNtp::connectMQTT("myId")) { /* broker is ready */ }
//      MqttNtp::publish("/my/topic", "{\"key\":\"value\"}");
//      MqttNtp::disconnect();
//    }
// =============================================================================

namespace MqttNtp {

  // ── Timezone / NTP constants ─────────────────────────────────────────────
  // POSIX tz string for Europe/Copenhagen (CET/CEST with DST rules).
  // Replace with your own POSIX string if a different zone is needed.
  constexpr const char* TIMEZONE          = "CET-1CEST,M3.5.0,M10.5.0/3";
  constexpr const char* NTP_SERVER        = "pool.ntp.org";
  constexpr uint32_t    NTP_TIMEOUT_MS    = 8000;   // max wait for NTP sync

  // ── MQTT constants ───────────────────────────────────────────────────────
  constexpr uint16_t    MQTT_BUFFER_BYTES = 512;    // PubSubClient TX/RX buffer

  // -------------------------------------------------------------------------
  //  syncNTP()
  //  Configures the ESP32 SNTP client with TIMEZONE and NTP_SERVER, then
  //  blocks until the system clock is valid (year ≥ 2020) or the timeout
  //  fires.
  //
  //  Returns: true  – clock is synced and getLocalTime() will return valid data
  //           false – timed out; timestamp calls will return the epoch fallback
  // -------------------------------------------------------------------------
  bool syncNTP();

  // -------------------------------------------------------------------------
  //  getCPHTimestamp()
  //  Returns the current local time as a formatted string.
  //  Relies on syncNTP() having been called first.
  //
  //  Format: "YYYY-MM-DD HH:MM:SS"
  //  Falls back to "1970-01-01 00:00:00" when the clock is not set.
  // -------------------------------------------------------------------------
  String getTimestamp();

  // -------------------------------------------------------------------------
  //  connectMQTT()
  //  Opens a TLS connection to the broker defined in secrets.h, using the CA
  //  certificate from ca_cert.h to verify the server identity, then performs
  //  an MQTT CONNECT with the supplied clientId.
  //
  //  Parameters:
  //    clientId – unique MQTT client identifier string
  //
  //  Returns: true  – MQTT session is established and publish() can be called
  //           false – connection failed (check Serial for the state code)
  // -------------------------------------------------------------------------
  bool connectMQTT(const char* clientId);

  // -------------------------------------------------------------------------
  //  publish()
  //  Publishes a payload to the given topic.  connectMQTT() must succeed
  //  before calling this.
  //
  //  Parameters:
  //    topic   – full MQTT topic string (e.g. "/devices/device01/data")
  //    payload – null-terminated message body (e.g. JSON string)
  //
  //  Returns: true  – broker acknowledged the publish
  //           false – publish failed (no connection or broker rejected it)
  // -------------------------------------------------------------------------
  bool publish(const char* topic, const char* payload);

  // -------------------------------------------------------------------------
  //  disconnect()
  //  Cleanly closes the MQTT session and brings down the WiFi interface.
  //  Call this before entering deep sleep so the TCP stack has time to flush.
  // -------------------------------------------------------------------------
  void disconnect();

} // namespace MqttNtp