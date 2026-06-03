#pragma once

// ─── Hardware ────────────────────────────────────────────────────────────────

// GPIO pins for the three relay channels (active-LOW relay module).
#define RELAY_GPIO_1  3
#define RELAY_GPIO_2   9
#define RELAY_GPIO_3  10 

// ─── Relay safety ───────────────────────────────────────────────────────────

// Minimum gap (ms) between successive relay ON activations.
// Prevents inrush current overloads when multiple zones start close together.
#define RELAY_ON_MIN_GAP_MS  700

// Maximum time (ms) any relay is allowed to stay ON without an explicit OFF
// command. After this period the relay is forced OFF as a safety measure.
// 3 hours = 3 * 60 * 60 * 1000 ms
#define RELAY_MAX_ON_MS  (3UL * 60UL * 60UL * 1000UL)

// ─── MQTT / Home Assistant ────────────────────────────────────────────────────

// Node ID: used as the MQTT client-id and as a prefix in all topic paths /
// HA unique_ids. Must be unique per device on the network.
#define MQTT_NODE_ID     "esp32_irrigation"
#define MQTT_BROKER_PORT 1883
