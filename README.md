# ESP32-C3 Irrigation Controller

Controls three irrigation zones via Home Assistant using MQTT Discovery.

## Features

- 3 relay channels (active-LOW module)
- Auto-reconnect WiFi
- Home Assistant MQTT Discovery — zones appear automatically as switch entities
- Minimum gap enforcement between relay activations (prevents inrush current)
- Retained MQTT state — HA stays in sync after broker or device restart

## Hardware

| Relay | GPIO |
|-------|------|
| Zone 1 | 10 |
| Zone 2 | 20 |
| Zone 3 | 9  |

## Setup

**1. Credentials**

```bash
cp main/include/secrets.h.example main/include/secrets.h
```

Edit `secrets.h` and fill in your WiFi SSID/password and MQTT broker details.

**2. Configuration**

Adjust GPIO pins, MQTT node ID, broker port, or the relay activation gap in `main/include/config.h`.

**3. Build & flash**

```bash
idf.py build flash monitor
```

## MQTT Topics

All topics are prefixed with `homeassistant/switch/<node_id>_relay<N>/`.

| Topic | Direction | Payload |
|-------|-----------|---------|
| `.../set`   | HA → device | `ON` / `OFF` |
| `.../state` | device → HA | `ON` / `OFF` (retained) |

Device discovery is published to `homeassistant/device/<node_id>/config` on every broker connection.

## Acknowledgements

Developed with the assistance of [GitHub Copilot](https://github.com/features/copilot).

## Dependencies

- ESP-IDF ≥ 5.x
- [idf-component-manager](https://docs.espressif.com/projects/idf-component-manager/) (managed components resolved automatically)
