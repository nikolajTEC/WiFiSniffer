#include "mqtt_test.h"
#include "mqtt_ntp.h"
#include "location_report.h"
#include "secrets.h"   // DEVICE_NAME

// =============================================================================
//  mqtt_test.cpp
// =============================================================================

#define TEST_TOPIC     "/devices/" DEVICE_NAME "/location"
#define PUBLISH_INTERVAL_MS  10000   // publish a mock report every 10 s

namespace {
  // Tracks when the last publish happened so tick() never blocks.
  unsigned long lastPublishMs = 0;
}

// =============================================================================
//  MqttTest::begin
//  Full boot sequence — call once from setup().
// =============================================================================
void MqttTest::begin() {
  Serial.println("\n========== MQTT TEST BEGIN ==========");

  if (MqttNtp::connectMQTT()) {
    Serial.println("[Test] Ready — publishing every 10 s.");
  } else {
    Serial.println("[Test] FAIL – connection failed. tick() will keep retrying.");
  }

  Serial.println("=====================================\n");
}

// =============================================================================
//  MqttTest::tick
//  Call every loop() — non-blocking.
//  • Delegates connection keep-alive / reconnect to MqttNtp::maintain().
//  • Publishes a fresh mock report once per PUBLISH_INTERVAL_MS.
// =============================================================================
void MqttTest::tick() {
  // ── Keep the MQTT session alive (and reconnect if it dropped) ─────────────
  MqttNtp::maintain();

  // ── Publish on interval ───────────────────────────────────────────────────
  unsigned long now = millis();
  if (now - lastPublishMs < PUBLISH_INTERVAL_MS) return;
  lastPublishMs = now;

  // Build a fresh mock report each time so the timestamp advances correctly.
  LocationReport report = LocationReport::mock();

  if (publishReport(TEST_TOPIC, report)) {
    Serial.println("[Test] PASS – mock report published.");
  } else {
    Serial.println("[Test] FAIL – publish rejected (will retry next interval).");
  }
}