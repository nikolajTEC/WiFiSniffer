#pragma once

// =============================================================================
//  mqtt_test.h
//
//  Looping smoke-test for the MQTT + NTP pipeline.
//  Publishes a mock LocationReport every 10 s, staying connected between sends.
//
//  Add two calls to your existing main — nothing else changes:
//
//      #include "mqtt_test.h"
//
//      void setup() {
//          Serial.begin(115200);
//          MqttTest::begin();   // connect once
//      }
//
//      void loop() {
//          MqttTest::tick();    // keep alive + publish every 10 s
//      }
// =============================================================================

namespace MqttTest {

  // -------------------------------------------------------------------------
  //  begin()
  //  Connects WiFi → syncs NTP → opens MQTT session.
  //  Call once from setup().
  // -------------------------------------------------------------------------
  void begin();

  // -------------------------------------------------------------------------
  //  tick()
  //  Keeps the MQTT session alive and publishes a mock report every 10 s.
  //  Call every loop() iteration — non-blocking.
  // -------------------------------------------------------------------------
  void tick();

} // namespace MqttTest