#include "mqtt_test.h"

// ── Utility layers this test exercises ───────────────────────────────────────
#include "mqtt_ntp.h"
#include "location_report.h"

// ── Credentials (WiFi + MQTT) ─────────────────────────────────────────────────
#include "secrets.h"    // WIFI_SSID, WIFI_PASSWORD

#include <WiFi.h>

// =============================================================================
//  mqtt_test.cpp
// =============================================================================

// ── Test topic + client ID (both derived from DEVICE_NAME in secrets.h) ───────
// Topic matches the convention used in the original project: /devices/<name>/...
#define TEST_TOPIC      "/devices/" DEVICE_NAME "/location"
#define TEST_CLIENT_ID  DEVICE_NAME "_test"

// =============================================================================
//  File-private helpers  (not exposed in the header)
// =============================================================================

// ── connectWiFi() ─────────────────────────────────────────────────────────────
// Blocks until connected or 10-second timeout.
static bool connectWiFi() {
  Serial.printf("[Test] Connecting to WiFi: %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 10000) {
      Serial.println("\n[Test] WiFi timeout!");
      return false;
    }
    delay(250);
    Serial.print(".");
  }

  Serial.printf("\n[Test] WiFi connected. IP: %s\n",
                WiFi.localIP().toString().c_str());
  return true;
}

// =============================================================================
//  MqttTest::run
// =============================================================================
void MqttTest::run() {
  Serial.println("\n========== MQTT TEST START ==========");

  // ── Step 1: WiFi ──────────────────────────────────────────────────────────
  if (!connectWiFi()) {
    Serial.println("[Test] FAIL – could not connect to WiFi.");
    Serial.println("========== MQTT TEST END ============\n");
    return;
  }

  // ── Step 2: NTP time sync ─────────────────────────────────────────────────
  if (!MqttNtp::syncNTP()) {
    Serial.println("[Test] FAIL – NTP sync timed out.");
    WiFi.disconnect(true);
    Serial.println("========== MQTT TEST END ============\n");
    return;
  }

  // ── Step 3: MQTT connection ───────────────────────────────────────────────
  if (!MqttNtp::connectMQTT(TEST_CLIENT_ID)) {
    Serial.println("[Test] FAIL – could not connect to MQTT broker.");
    WiFi.disconnect(true);
    Serial.println("========== MQTT TEST END ============\n");
    return;
  }

  // ── Step 4: Build mock report and publish ─────────────────────────────────
  // mock() uses the live NTP timestamp, so it also confirms the clock is right.
  LocationReport report = LocationReport::mock();

  if (publishReport(TEST_TOPIC, report)) {
    Serial.println("[Test] PASS – mock report published successfully.");
  } else {
    Serial.println("[Test] FAIL – publish was rejected by the broker.");
  }

  // ── Step 5: Clean up ──────────────────────────────────────────────────────
  MqttNtp::disconnect();
  Serial.println("========== MQTT TEST END ============\n");
}