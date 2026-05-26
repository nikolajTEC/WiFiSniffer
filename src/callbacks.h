// callbacks.h
#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <stdint.h>
#include "esp_wifi.h"

#define SRC_MAC_OFFSET 10

// Forward declarations for callbacks implemented in callbacks.cpp
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len);
void IRAM_ATTR snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type);

#endif // CALLBACKS_H
