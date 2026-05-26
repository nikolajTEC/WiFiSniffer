#pragma once

#include <Arduino.h>

// =============================================================================
//  location_report.h
//
//  ── Device identifier / GDPR note ──────────────────────────────────────────
//  The detected devices' raw MAC addresses are personal data under GDPR
//  Article 4(1): they are permanent, globally unique identifiers that can be
//  linked to an individual's movement patterns.
//
//  This file pseudonymises each detected MAC before it ever leaves the device
//  (GDPR Article 4(5)): the raw MAC is hashed with SHA-256 and only the first
//  8 hex characters are published (e.g. "a3f9c12b").
//
//  Properties of the chosen approach:
//    • Consistent   – same detected MAC always produces the same hash, so
//                     multiple readings from one device can be correlated
//    • Unique       – two different MACs will not collide for any realistic
//                     fleet size (32-bit space, ~4 billion values)
//    • Non-reversible – the raw MAC cannot be recovered from the hash alone
//    • Separated    – no mapping between hash and MAC is stored or transmitted;
//                     if needed it must be held in secured internal records
//
//  The ESP32 node's own identity is NOT stored in the payload — it is already
//  implicit in the MQTT topic (/devices/<DEVICE_NAME>/...) and does not need
//  to be repeated here.
// =============================================================================

// ── Per-node distance reading ─────────────────────────────────────────────────
struct NodeReading {
  String name;      // node identifier (DEVICE_NAME of the slave)
  float  distance;  // estimated distance in metres
};

// ── Full trilateration report ─────────────────────────────────────────────────
struct LocationReport {

  String      deviceId;   // pseudonymised detected-device identifier (8-char MAC hash)
  String      timestamp;  // "YYYY-MM-DD HH:MM:SS"
  float       x;          // computed X position (metres)
  float       y;          // computed Y position

  NodeReading nodeA;
  NodeReading nodeB;
  NodeReading nodeC;

  // -------------------------------------------------------------------------
  //  toJSON()
  //  Output shape:
  //  {
  //    "deviceId":  "a3f9c12b",
  //    "timestamp": "2025-05-20 14:32:07",
  //    "location":  { "x": 3.20, "y": 5.75 },
  //    "nodes": [
  //      { "name": "NodeA", "distance": 2.50 },
  //      { "name": "NodeB", "distance": 4.10 },
  //      { "name": "NodeC", "distance": 3.80 }
  //    ]
  //  }
  // -------------------------------------------------------------------------
  String toJSON() const;

  // -------------------------------------------------------------------------
  //  mock()
  //  Returns a report with a hardcoded detected MAC so the hash path is
  //  exercised during testing without needing a real sniffed device.
  // -------------------------------------------------------------------------
  static LocationReport mock();
};

// =============================================================================
//  pseudonymiseMac()
//  Takes the raw MAC of a *detected* device (e.g. "AA:BB:CC:DD:EE:FF"),
//  hashes it with SHA-256, and returns the first 8 hex characters.
//
//  Call this whenever a new MAC is sniffed — pass the result as deviceId.
//  Never store or forward the raw MAC string beyond this call.
// =============================================================================
String pseudonymiseMac(const String& rawMac);

// =============================================================================
//  publishReport()
// =============================================================================
bool publishReport(const char* topic, const LocationReport& report);