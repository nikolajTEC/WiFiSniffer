#pragma once

// =============================================================================
//  mqtt_test.h
//
//  Completely self-contained smoke-test for the MQTT + NTP pipeline.
//  Drop this file (and mqtt_test.cpp) into your project and call ONE method
//  from your existing setup():
//
//      #include "mqtt_test.h"
//      void setup() {
//          MqttTest::run();   // that's it
//      }
//
//  Nothing in here touches your main logic.
// =============================================================================

namespace MqttTest {

  // -------------------------------------------------------------------------
  //  run()
  //  Connects WiFi → syncs NTP → connects MQTT → publishes a mock report →
  //  disconnects.  Prints a clear PASS / FAIL summary to Serial.
  // -------------------------------------------------------------------------
  void run();

} // namespace MqttTest