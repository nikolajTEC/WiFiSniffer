#include "location_report.h"
#include "mqtt_ntp.h"
#include "mbedtls/md.h"

// =============================================================================
//  location_report.cpp
// =============================================================================

// =============================================================================
//  pseudonymiseMac
//  Hashes the detected device's raw MAC and returns 8 hex characters.
//
//  The same raw MAC always produces the same output, which is intentional:
//  it lets the subscriber count unique devices and correlate repeated sightings
//  of the same device — without ever knowing the actual hardware address.
// =============================================================================
String pseudonymiseMac(const String& rawMac) {
  // Strip colons so formatting differences ("AA:BB:..." vs "AABB...") don't
  // produce different hashes for the same physical device.
  String mac = rawMac;
  mac.replace(":", "");
  mac.toUpperCase();   // normalise case for the same reason

  uint8_t digest[32];
  mbedtls_md_context_t ctx;
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, info, 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const uint8_t*)mac.c_str(), mac.length());
  mbedtls_md_finish(&ctx, digest);
  mbedtls_md_free(&ctx);

  // First 8 hex chars = 4 bytes = 32 bits of the digest
  char shortHash[9];
  snprintf(shortHash, sizeof(shortHash),
           "%02x%02x%02x%02x",
           digest[0], digest[1], digest[2], digest[3]);

  return String(shortHash);
}

// =============================================================================
//  LocationReport::toJSON
// =============================================================================
String LocationReport::toJSON() const {
  char buf[300];
  snprintf(buf, sizeof(buf),
    "{"
      "\"deviceId\":\"%s\","
      "\"timestamp\":\"%s\","
      "\"location\":{\"x\":%.2f,\"y\":%.2f},"
      "\"nodes\":["
        "{\"name\":\"%s\",\"distance\":%.2f},"
        "{\"name\":\"%s\",\"distance\":%.2f},"
        "{\"name\":\"%s\",\"distance\":%.2f}"
      "]"
    "}",
    deviceId.c_str(),
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
// =============================================================================
LocationReport LocationReport::mock() {
  LocationReport r;

  // Simulate a sniffed device MAC — this would normally come from your
  // WiFi sniffer callback.
  r.deviceId  = pseudonymiseMac("AA:BB:CC:DD:EE:FF");
  r.timestamp = MqttNtp::getTimestamp();
  r.x         = 3.20f;
  r.y         = 5.75f;
  r.nodeA     = { "NodeA", 2.50f };
  r.nodeB     = { "NodeB", 4.10f };
  r.nodeC     = { "NodeC", 3.80f };

  return r;
}

// =============================================================================
//  publishReport
// =============================================================================
bool publishReport(const char* topic, const LocationReport& report) {
  String json = report.toJSON();
  Serial.printf("[Report] Payload: %s\n", json.c_str());
  return MqttNtp::publish(topic, json.c_str());
}