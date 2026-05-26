#include "location_report.h"
#include "mqtt_ntp.h"

// =============================================================================
//  location_report.cpp
// =============================================================================

// =============================================================================
//  LocationReport::toJSON
// =============================================================================
String LocationReport::toJSON() const {
  // ── Build JSON manually ───────────────────────────────────────────────────
  // No external JSON library needed for a fixed-shape payload this size.
  // snprintf into a stack buffer; 256 bytes is comfortable for this schema.
  char buf[256];
  snprintf(buf, sizeof(buf),
    "{"
      "\"timestamp\":\"%s\","
      "\"location\":{\"x\":%.2f,\"y\":%.2f},"
      "\"nodes\":["
        "{\"name\":\"%s\",\"distance\":%.2f},"
        "{\"name\":\"%s\",\"distance\":%.2f},"
        "{\"name\":\"%s\",\"distance\":%.2f}"
      "]"
    "}",
    timestamp.c_str(),
    x, y,
    nodeA.name.c_str(), nodeA.distance,
    nodeB.name.c_str(), nodeB.distance,
    nodeC.name.c_str(), nodeC.distance
  );
  return String(buf);
}

// =============================================================================
//  LocationReport::mock
//  Hardcoded stand-in so the MQTT pipeline can be tested without real sensors.
// =============================================================================
LocationReport LocationReport::mock() {
  LocationReport r;

  // ── Timestamp ─────────────────────────────────────────────────────────────
  // Use the live clock if NTP has synced, otherwise fall back to a fixed
  // string so the test still produces a readable payload.
  r.timestamp = MqttNtp::getTimestamp();

  // ── Computed position ─────────────────────────────────────────────────────
  // These would normally come from your trilateration algorithm.
  r.x = 3.20f;
  r.y = 5.75f;

  // ── Node readings ─────────────────────────────────────────────────────────
  // Names match whatever identifiers the slave nodes broadcast.
  r.nodeA = { "NodeA", 2.50f };
  r.nodeB = { "NodeB", 4.10f };
  r.nodeC = { "NodeC", 3.80f };

  return r;
}

// =============================================================================
//  publishReport
// =============================================================================
bool publishReport(const char* topic, const LocationReport& report) {
  // Serialise first so the JSON string is visible in the log before we
  // attempt the publish – makes debugging a failed send much easier.
  String json = report.toJSON();
  Serial.printf("[Report] Payload: %s\n", json.c_str());

  return MqttNtp::publish(topic, json.c_str());
}