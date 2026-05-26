#include <Arduino.h>
#include "esp_wifi.h"
#include "globals.h"

void loop() {
    if (millis() - lastHopTime >= HOP_INTERVAL_MS) {
        currentChannelIndex = (currentChannelIndex + 1) % CHANNEL_COUNT;
        esp_wifi_set_channel(CHANNELS[currentChannelIndex], WIFI_SECOND_CHAN_NONE);
        lastHopTime = millis();
    }
}
