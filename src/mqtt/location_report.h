#pragma once

// ── Standard / Arduino includes ───────────────────────────────────────────────
#include <Arduino.h>

// =============================================================================
//  location_report.h
//
//  Defines the LocationReport data structure and the helpers to:
//    • Build a report from trilateration output
//    • Serialize it to a JSON string
//    • Publish it via MqttNtp::publish()
//
//  Depends on mqtt_ntp.h being included and MqttNtp::connectMQTT() having
//  succeeded before publishReport() is called.
//
//  Typical call sequence (master node):
//    LocationReport r = LocationReport::mock();   // or build from real data
//    Serial.println(r.toJSON());                  // inspect before sending
//    bool ok = publishReport("/devices/master/location", r);
// =============================================================================

// ── Per-node distance reading ─────────────────────────────────────────────────
// Holds the identity and measured distance of one slave node.
struct NodeReading {
  String  name;       // human-readable node identifier, e.g. "NodeA"
  float   distance;   // estimated distance in metres
};

// ── Full trilateration report ─────────────────────────────────────────────────
// Aggregates the three slave readings and the computed XY position.
struct LocationReport {

  String      timestamp;  // "YYYY-MM-DD HH:MM:SS"  – from MqttNtp::getTimestamp()
  float       x;          // computed X position (metres, or your chosen unit)
  float       y;          // computed Y position

  NodeReading nodeA;      // first slave: name + measured distance
  NodeReading nodeB;      // second slave
  NodeReading nodeC;      // third slave

  // -------------------------------------------------------------------------
  //  toJSON()
  //  Serialises the report to a compact JSON string ready for MQTT.
  //
  //  Output shape:
  //  {
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
  //  Returns a LocationReport pre-filled with hardcoded values so you can
  //  verify the MQTT pipeline end-to-end before any real sensor data exists.
  // -------------------------------------------------------------------------
  static LocationReport mock();
};

// =============================================================================
//  publishReport()
//  Serialises `report` and hands the resulting JSON to MqttNtp::publish().
//
//  Parameters:
//    topic  – full MQTT topic string (e.g. "/devices/master/location")
//    report – populated LocationReport to send
//
//  Returns: true if the broker acknowledged the publish, false otherwise.
// =============================================================================
bool publishReport(const char* topic, const LocationReport& report);