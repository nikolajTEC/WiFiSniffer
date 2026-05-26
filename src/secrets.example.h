// secrets.example.h — template for secrets.h
#ifndef SECRETS_EXAMPLE_H
#define SECRETS_EXAMPLE_H

#include <stdint.h>

// Copy this file to src/secrets.h and fill in real values.

// Enable/disable MAC filtering
constexpr bool FILTER_ENABLED = true;

// Example watched MACs (replace with real values as needed)
constexpr uint8_t WATCHED_MACS[][6] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

constexpr uint8_t WATCHED_COUNT = sizeof(WATCHED_MACS) / 6;

// Node MAC addresses — set to your device MACs
constexpr uint8_t MAC_MASTER[] = {0x00,0x00,0x00,0x00,0x00,0x00};
constexpr uint8_t MAC_NODE_B[] = {0x00,0x00,0x00,0x00,0x00,0x00};
constexpr uint8_t MAC_NODE_C[] = {0x00,0x00,0x00,0x00,0x00,0x00};

// Node names (display names used for boot messages: "Role: Master/Slave, Name: <display>")
// Set to your desired display names for each node (master + 2 slaves)
constexpr const char* NODE_NAMES[3] = {
    "node1Name",
    "node2Name",
    "node3Name"
};

// Node positions (for trilateration)
// Set to the actual (x,y) coordinates of your nodes in meters. For example: (0,0), (0,4), (5,4)
constexpr float NODE_POS[3][2] = {
    {0.0f, 0.0f},
    {0.0f, 0.0f},
    {0.0f, 0.0f},
};

// WiFi AP password — set to your desired AP password
constexpr const char* WIFI_PASSWORD = "your_wifi_password_here";

#endif // SECRETS_EXAMPLE_H
