#include "globals.h"
#include "secrets.h"
uint8_t MASTER_MAC[6];
uint8_t nodeRole  = 1; // 0=master, 1=slave
uint8_t nodeIndex = 0;

const uint8_t CHANNELS[] = {1};

const uint8_t CHANNEL_COUNT = sizeof(CHANNELS) / sizeof(CHANNELS[0]);
const uint32_t HOP_INTERVAL_MS = 350;

uint8_t currentChannelIndex = 0;
uint32_t lastHopTime = 0;

const char* NODE_NAMES[3] = {
    "nikolajnode",
    "carstennode",
    "lokenode"
};
