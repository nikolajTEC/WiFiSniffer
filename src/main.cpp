// main.cpp
#include <Arduino.h>
#include "config.h"
#include "networking.h"

uint8_t nodeRole = 1; 
uint8_t nodeIndex = 0;

void setup() {
    Serial.begin(115200);
    delay(100);
    
    initNetwork();
}

void loop() {
    handleNetworkLoop();
}