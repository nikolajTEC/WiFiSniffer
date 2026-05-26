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
//    • TLS-secured MQTT connection, keep-alive, and publish
//
//  Designed for always-on devices: connect once at boot, call maintain()
//  every loop iteration to keep the session alive and auto-reconnect on drops.
//
//  Required companion files:
//    secrets.h  – WIFI_SSID, WIFI_PASSWORD, MQTT_HOST, MQTT_PORT,
//                 MQTT_USER, MQTT_PASS, DEVICE_NAME
//    ca_cert.h  – MQTT_CA_CERT  (PEM string of broker CA certificate)
//
//  Typical usage (always-on device):
//
//    void setup() {
//      MqttNtp::connectMQTT();   // connect once at boot
//    }
//
//    void loop() {
//      MqttNtp::maintain();      // keeps session alive, reconnects if dropped
//
//      // ... read sensors, trilaterate, etc. ...
//
//      MqttNtp::publish("/devices/device02/location", payload);
//    }
//
//  disconnect() is intentionally kept for devices that need to power down
//  the radio (e.g. before deep sleep), but should NOT be called each cycle.
// =============================================================================

namespace MqttNtp {

  // ── Timezone / NTP constants ───────────────────────────────────────────────
  constexpr const char* TIMEZONE       = "CET-1CEST,M3.5.0,M10.5.0/3";
  constexpr const char* NTP_SERVER     = "pool.ntp.org";
  constexpr uint32_t    NTP_TIMEOUT_MS = 8000;

  // ── MQTT constants ─────────────────────────────────────────────────────────
  constexpr uint16_t MQTT_BUFFER_BYTES  = 512;
  // How long to wait between reconnect attempts (ms).
  // Prevents hammering the broker if it is temporarily unreachable.
  constexpr uint32_t RECONNECT_DELAY_MS = 5000;

  // -------------------------------------------------------------------------
  //  connectMQTT()
  //  Connects WiFi, syncs NTP, and opens a TLS MQTT session.
  //  Client ID and credentials are taken from secrets.h (DEVICE_NAME,
  //  MQTT_USER, MQTT_PASS).  Call once from setup().
  //
  //  Returns: true  – fully connected and ready to publish
  //           false – one of the steps failed (check Serial for details)
  // -------------------------------------------------------------------------
  bool connectMQTT();

  // -------------------------------------------------------------------------
  //  maintain()
  //  Must be called every loop() iteration.
  //  • Drives the PubSubClient keep-alive (PINGREQ/PINGRESP) so the broker
  //    does not drop an idle session.
  //  • Detects a lost connection and attempts to reconnect, but only after
  //    RECONNECT_DELAY_MS has elapsed since the last attempt, so a dead
  //    broker never blocks the rest of your loop.
  // -------------------------------------------------------------------------
  void maintain();

  // -------------------------------------------------------------------------
  //  publish()
  //  Publishes a payload to the given topic.
  //  Safe to call even if the connection is temporarily down — returns false
  //  immediately rather than blocking.
  //
  //  Returns: true  – broker acknowledged the publish
  //           false – not connected, or broker rejected the message
  // -------------------------------------------------------------------------
  bool publish(const char* topic, const char* payload);

  // -------------------------------------------------------------------------
  //  getTimestamp()
  //  Returns the current local time as "YYYY-MM-DD HH:MM:SS".
  //  Falls back to "1970-01-01 00:00:00" if NTP has not synced yet.
  // -------------------------------------------------------------------------
  String getTimestamp();

  // -------------------------------------------------------------------------
  //  disconnect()
  //  Cleanly closes the MQTT session and powers down WiFi.
  //  Only needed before deep sleep or intentional shutdown.
  //  Do NOT call this in a normal publish-then-reconnect cycle.
  // -------------------------------------------------------------------------
  void disconnect();

} // namespace MqttNtp