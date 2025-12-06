# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-based bell controller with web interface for scheduling automated bell rings. Single-file Arduino/C++ application that runs on ESP32 hardware and provides WiFi-connected scheduling functionality with NTP time synchronization.

## Build & Development Commands

```bash
# Build and upload to ESP32
pio run --target upload

# Monitor serial output (115200 baud)
pio device monitor

# Build without uploading
pio run

# Erase flash (useful if NVS preferences are corrupted)
pio run --target erase
```

## Architecture

**Single-file architecture**: All code is in `src/main.cpp` (embedded web server HTML/CSS/JS included as raw string literal).

**Key components:**
- **NTP time sync**: Uses `NTPClient` library to get current time from `pool.ntp.org`
- **Web server**: Async web server (`ESPAsyncWebServer`) serving on port 80 with REST API
- **Persistent storage**: ESP32 Preferences (NVS) for schedule persistence across reboots
- **Schedule system**: Array-based (max 20 schedules), checked every loop iteration
- **Bell control**: GPIO-based relay control with configurable duration timer

**Time handling**: Uses ESP32's native `configTzTime()` with POSIX timezone strings for automatic DST handling. Timezone is configurable via web interface and stored in NVS preferences.

**Web interface**: Single-page HTML embedded in main.cpp. Uses vanilla JavaScript with fetch API for REST endpoints:
- `GET /` - Main web interface
- `GET /time` - Current time as JSON (in configured timezone)
- `GET /timezone` - Get current timezone setting
- `POST /timezone` - Set timezone (triggers ESP32 restart)
- `GET /schedules` - List all schedules
- `POST /schedule` - Add new schedule
- `DELETE /schedule/{id}` - Remove schedule by index
- `POST /ring` - Trigger bell immediately

**Hardware I/O:**
- GPIO 5 (`BELL_PIN`): Relay control output
- GPIO 4 (`BUTTON_PIN`): Physical button input (internal pullup enabled)

## Configuration Requirements

Before first upload, user MUST edit `src/main.cpp`:
- Lines 9-10: WiFi credentials (`ssid` and `password`)

Static IP configuration:
- Automatically uses `.215` as last octet (configurable at line 13: `STATIC_IP_LAST_OCTET`)
- Connects via DHCP first to auto-detect network (192.168.x.x, 10.0.x.x, etc.)
- Reconnects with static IP using detected network + custom last octet
- Always results in predictable IP address ending

Timezone configuration:
- Set via web interface after upload (supports major timezones with automatic DST)
- Stored in NVS preferences and persists across reboots
- Changing timezone triggers automatic ESP32 restart to apply changes

Optional hardware configurations in `src/main.cpp`:
- Lines 13-14: GPIO pin assignments (`BELL_PIN`, `BUTTON_PIN`)
- Line 17: Bell duration in milliseconds (`BELL_DURATION`)

## Dependencies

Managed via PlatformIO (`platformio.ini`):
- `esphome/ESPAsyncWebServer-esphome` - Async web server
- `esphome/ESPAsyncTCP-esphome` - TCP library for web server
- `ArduinoJson` - JSON serialization for REST API

Framework: Arduino for ESP32 (espressif32 platform)

Time synchronization uses ESP32's built-in NTP client via `configTzTime()` (no external library needed)

## State Management

**Schedule storage**: Uses ESP32 Preferences library with namespace "bell". Each schedule stored with key pattern `s{index}_{field}` where fields are:
- `en` - enabled flag (bool)
- `day` - day of week 0-6 (int)
- `hr` - hour 0-23 (int)
- `min` - minute 0-59 (int)

**Schedule count**: Stored separately as `count` key

**Timezone storage**: Stored as `timezone` key (POSIX timezone string)

**Trigger prevention**: Each schedule has a `triggered` boolean flag to prevent multiple rings during the same minute. Reset when time advances.

## Hardware Setup

ESP32 must be connected to:
- Relay module on GPIO 5 (controls bell)
- Push button on GPIO 4 to GND (with internal pullup)
- Bell/buzzer connected through relay

Network: Requires 2.4GHz WiFi (ESP32 does not support 5GHz)
