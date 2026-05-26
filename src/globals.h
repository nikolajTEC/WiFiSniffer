#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdint.h>

extern uint8_t MASTER_MAC[6];
extern uint8_t nodeRole;
extern uint8_t nodeIndex;

extern const uint8_t CHANNELS[];
extern const uint8_t CHANNEL_COUNT;
extern const uint32_t HOP_INTERVAL_MS;

extern uint8_t currentChannelIndex;
extern uint32_t lastHopTime;

#endif // GLOBALS_H
