# P4 Wall Keypad — ESP32-P4 Smart Keypad for Home Assistant

A wall-mounted smart controller built on the **ESP32-P4-NANO** (RISC-V): 4″ MIPI-DSI touchscreen, 10 physical buttons, rotary encoder, wired Ethernet, MQTT integration with Home Assistant, on-device web UI for configuration — and an optional mmWave presence radar.

| | | |
|---|---|---|
| ![Front](docs/photos/prototype_front.jpg) | ![PCB](docs/photos/pcb_side.jpg) | ![Back](docs/photos/pcb_back.jpg) |

*Working prototype on a custom PCB (schematic + layout designed from scratch for this project).*

## What it does

- **10 configurable buttons** → toggle lights or open/close covers; **long-hold** enters an on-screen control mode (brightness / color-temperature / RGB hue / cover position) adjusted with the **rotary encoder**
- **4″ 480×800 DSI display** (LVGL) shows a live tile grid with entity states and icons — bilingual UI (English/Arabic)
- **On-device web UI** (port 80): button configuration, live control, network settings, and **auto-generated Home Assistant YAML** for one-paste setup
- **MQTT-first architecture** — the keypad never calls HA's REST API; all logic runs through MQTT automations, so it survives HA restarts and stays install-anywhere
- **100M wired Ethernet** (DHCP or static) — no Wi-Fi flakiness on a wall device
- **Optional LD2450 mmWave radar** publishes room presence over MQTT
- Config persisted in NVS; designed for a trusted-LAN security model

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  app_main loop (main.cpp)                                    │
│  • Button debounce / short & long press                      │
│  • Encoder queue → brightness/CCT/hue/cover adjustments      │
│  • Radar presence → MQTT (retained)                          │
└──────────────┬──────────────────────────────┬───────────────┘
               │                              │
    ┌──────────▼──────────┐        ┌──────────▼──────────┐
    │  ui_keypad (LVGL)   │        │  MQTT client        │
    │  Queue → LVGL thread│◄──────►│  keypad/button/*    │
    └─────────────────────┘        │  keypad/state/#     │
               ▲                   └──────────┬──────────┘
               │                              │
    ┌──────────┴──────────┐        ┌──────────▼──────────┐
    │  Webserver.cpp      │        │  Home Assistant     │
    │  HTTP :80           │        │  (automations)      │
    └──────────┬──────────┘        └─────────────────────┘
               │
    ┌──────────▼──────────┐
    │  ethernet.c         │
    │  DHCP / static+mDNS │
    └─────────────────────┘
```

- **MQTT-first:** the keypad publishes button events and listens for entity state; all logic lives in Home Assistant automations.
- **UI thread isolation:** input handling queues updates to a dedicated LVGL thread, so the display never blocks button response.
- **Stack:** ESP-IDF (C/C++), LVGL 9, ESP-IDF HTTP server, MQTT, mDNS, NVS.

## Hardware

ESP32-P4-NANO · 4″ DSI touch panel (ST7701, GT911) · custom keypad PCB (10 tactile switches + rotary encoder) · optional LD2450 mmWave radar.

## Using it

See the [Usage Guide](USAGE_GUIDE.md): what homeowners see on the wall and in the browser.

## Source code

This is a **showcase repository**: it documents the product without publishing the firmware or build files.
**Source code is available to employers on request**; contact me via [LinkedIn](https://www.linkedin.com/in/basel-yasar-b5522b260).

## Authorship & provenance

I designed the **system architecture, the custom PCB (schematic + layout), and the hardware integration**, and led the firmware development. Built during my work at an IoT company; published in sanitized form with permission.
