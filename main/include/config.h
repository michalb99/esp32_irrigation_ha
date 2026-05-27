#pragma once

// ─── Hardware ────────────────────────────────────────────────────────────────

// GPIO pins for the three relay channels (active-LOW relay module).
#define RELAY_GPIO_1  10
#define RELAY_GPIO_2  20
#define RELAY_GPIO_3   9

// ─── Relay safety ───────────────────────────────────────────────────────────

// Minimum gap (ms) between successive relay ON activations.
// Prevents inrush current overloads when multiple zones start close together.
#define RELAY_ON_MIN_GAP_MS  10000

// ─── MQTT / Home Assistant ────────────────────────────────────────────────────

// Node ID: used as the MQTT client-id and as a prefix in all topic paths /
// HA unique_ids. Must be unique per device on the network.
#define MQTT_NODE_ID     "esp32_irrigation"
#define MQTT_BROKER_PORT 1883
