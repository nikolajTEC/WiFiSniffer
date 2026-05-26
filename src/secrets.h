#pragma once
#include <stdint.h>
// WiFi credentials
#define WIFI_SSID     "IoT_H3/4"
#define WIFI_PASSWORD "98806829"

// MQTT broker
#define MQTT_HOST    "wilsons.local"
#define MQTT_PORT    8883

// Device credentials
#define MQTT_USER    "device02"
#define MQTT_PASS    "wt2V4z8G"
#define DEVICE_NAME  "device02"

constexpr bool FILTER_ENABLED = true;

// Example watched MACs (replace with real values as needed)
const uint8_t WATCHED_MACS[][6] = {
    {0xBC, 0x6E, 0xE2, 0x97, 0x98, 0x2A},
    {0x08, 0x9D, 0xF4, 0x92, 0x7A, 0xD9},
    {0xA8, 0x93, 0x4A, 0x03, 0x81, 0x75},
    {0xDC, 0xC4, 0x9C, 0x40, 0x16, 0xEB},
};

constexpr uint8_t WATCHED_COUNT = sizeof(WATCHED_MACS) / 6;

// Node MAC addresses — set to your device MACs
const uint8_t MAC_MASTER[] = {0xE8, 0x6B, 0xEA, 0xD4, 0x05, 0xB8};
const uint8_t MAC_NODE_B[] = {0xE8, 0x6B, 0xEA, 0xD3, 0x6B, 0xC8};
const uint8_t MAC_NODE_C[] = {0x40, 0x22, 0xD8, 0x07, 0x15, 0x10};
// Node names (display names used for boot messages: "Role: Master/Slave, Name: <display>")
// Set to your desired display names for each node (master + 2 slaves)

extern const char* NODE_NAMES[];


// const char* NODE_NAMES[3] = {
//     "nikolajnode",
//     "carstennode",
//     "lokenode"
// };

// Node positions (for trilateration)
// Set to the actual (x,y) coordinates of your nodes in meters. For example: (0,0), (0,4), (5,4)
const float NODE_POS[3][2] = {
    {0.0f, 0.0f}, // nikolajnode
    {0.0f, 4.0f}, // carstennode
    {5.0f, 4.0f}, // lokenode
};

// // WiFi AP password — set to your desired AP password
// constexpr const char* WIFI_PASSWORD = "98806829";