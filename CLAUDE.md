# Project: F1 CYD Notifications

## Overview
An F1 race schedule display and Telegram notification system running on the ESP32-2432S028R ("Cheap Yellow Display"). Fetches the current F1 season schedule from the sportstimes GitHub JSON feed and post-race results from the Jolpica API. The display cycles through a countdown timer, session schedule table, and circuit layout during race week, then switches to podium/standings screens for 7 days after each race. Sends Telegram notifications for race week start, key sessions (qualifying, sprint, race), and race results.

## Hardware
- **MCU**: ESP32-2432S028R (2.8" Cheap Yellow Display)
- **Display**: ILI9341 TFT, 240×320 (used landscape: 320×240)
- **Touch**: XPT2046 resistive touchscreen (tap to manually advance display)
- **Onboard RGB LED**: Active-LOW, GPIO 4/16/17 — used as boot status indicator
- **Onboard LDR**: GPIO34 — ADC input for auto-brightness mode
- **Storage**: LittleFS on internal flash
- **Power**: USB (standard CYD power delivery)

## Build Environment
- **Framework**: Arduino
- **Platform**: espressif32 @ 6.9.0
- **Board**: esp32dev
- **Filesystem**: LittleFS (min_spiffs partition)
- **Key Libraries**:
  - TFT_eSPI 2.5.43
  - ArduinoJson 7.4.0
  - WiFiManager (git)
  - Universal-Arduino-Telegram-Bot (git)
  - ESPAsyncWebServer 3.6.2 / AsyncTCP 3.3.3
  - ElegantOTA 3.1.6
  - ezTime 0.8.3

> TFT_eSPI is configured entirely via `build_flags` in `platformio.ini` (USER_SETUP_LOADED=1). Do NOT create a User_Setup.h — the build flags are the source of truth.

## Project Structure
```
src/
  main.cpp                — Setup, loop, timing orchestration, LED, touch
include/
  config.h                — All #defines: pins, colors, timing constants, API URLs
  types.h                 — Structs: RaceData, SessionInfo, AppConfig, enums
  config_manager.h        — LittleFS: load/save AppConfig, schedule/results cache
  wifi_setup.h            — WiFiManager captive portal with custom params
  time_utils.h            — NTP init (ezTime), timezone conversion, countdown calc
  f1_data.h               — HTTP fetch + JSON parse for schedule and results/standings
  display_renderer.h      — All TFT_eSPI drawing functions (no state logic here)
  display_states.h        — Display state machine: phase detection + rotation
  telegram_handler.h      — Bot init, message formatting, notification dispatch
  web_server.h            — ESPAsyncWebServer routes, embedded HTML UI, ElegantOTA
  track_images.h          — XBM circuit image store (placeholder — not yet populated)
```

All application logic lives in header files. `main.cpp` is the only translation unit — this is intentional.

## Pin Mapping
| Function     | GPIO | Notes                          |
|--------------|------|--------------------------------|
| TFT MOSI     | 13   | SPI @ 55 MHz                   |
| TFT MISO     | 12   |                                |
| TFT SCLK     | 14   |                                |
| TFT CS       | 15   |                                |
| TFT DC       | 2    |                                |
| TFT RST      | -1   | Not connected on CYD           |
| Backlight    | 21   | LEDC PWM ch0, 5 kHz, 8-bit     |
| Touch CS     | 33   | SPI @ 2.5 MHz                  |
| Touch IRQ    | 36   | Input-only pin                 |
| RGB LED Red  | 4    | Active LOW                     |
| RGB LED Green| 16   | Active LOW                     |
| RGB LED Blue | 17   | Active LOW                     |
| LDR          | 34   | ADC — brightness=0 enables auto|

## Configuration
- **Config file**: `/config.json` (LittleFS) — timezone, NTP server, Telegram bot token/chat ID, brightness
- **Schedule cache**: `/races.json` (LittleFS) — raw JSON from sportstimes, used as offline fallback
- **Results cache**: `/results.json` (LittleFS) — defined but not yet used
- **Initial setup**: WiFiManager captive portal (AP: "F1-Display") — exposes timezone, bot token, chat ID fields
- **Runtime config**: Web UI at `http://<device-ip>/` — all settings + brightness slider
- **OTA updates**: ElegantOTA at `http://<device-ip>/update`
- **Key constants** (all in `config.h`):
  - `COUNTDOWN_WEEK_DAYS 7` — days before first session to enter race-week mode
  - `POST_RACE_DAYS 7` — days after GP to show post-race screens
  - `NOTIFY_HOURS_BEFORE 1` — hours before session to fire Telegram notification
  - `RESULTS_POLL_AFTER_SEC` — start polling results 3h after GP start
  - `SCHEDULE_URL` — sportstimes 2026 JSON (update for new season)
  - `JOLPICA_BASE_URL` — Jolpica Ergast-compatible API base (update year each season)

## Current State
Core functionality is implemented and structured. The display state machine, schedule parsing, Telegram notifications, web config UI, and OTA are all in place. The main outstanding gap is circuit layout images — the track screen renders correctly but shows "Track image not available" for all circuits. Touch input works for manual screen advancement. The project has not yet been tested end-to-end on hardware.

## Architecture Notes
- **Display state machine** (`display_states.h`): `determinePhase()` maps current time vs race timestamps to one of three phases (idle / race-week / post-race). Within each phase, `advanceDisplayState()` rotates between sub-states on a timer or touch tap.
- **Non-blocking timing**: All periodic tasks (schedule refresh, results polling, notification check, brightness update) use `millis()` comparisons — no `delay()` in the loop.
- **Notification deduplication**: A per-round bitmask (`notificationBits`) is persisted to LittleFS so notifications aren't re-sent across reboots.
- **Data flow**: Schedule → GitHub raw JSON → `parseSchedule()` → `races[]` static array (prev/current/next). Post-race → Jolpica API → `podium[]` + `driverStandings[]` + `constructorStandings[]`.
- **HTTPS**: `WiFiClientSecure` with `setInsecure()` used for GitHub (schedule) and Telegram. Jolpica uses plain HTTP.
- **Season rollover**: `SCHEDULE_URL` and `JOLPICA_BASE_URL` both have the year hardcoded — must be updated each season.

## Known Issues
- **Track images not implemented**: `getTrackImage()` always returns `nullptr`. The track layout screen shows a fallback text message for all 24 circuits.
- **Touch calibration unchecked**: Calibration data `{300, 3600, 300, 3600, 7}` is approximate and may need tuning for the specific unit.
- **Telegram re-init on config change**: Updating the bot token via the web UI sets `telegramEnabled` but does not call `initTelegram()`. A reboot is required for a new token to take effect.
- **Results not cached**: `cacheResults()` / `loadCachedResults()` are implemented in `config_manager.h` but never called — post-race results are lost on reboot.
- **No WiFi reconnect**: If WiFi drops after boot there is no reconnection attempt.
- **NTP server not in captive portal**: The WiFiManager portal exposes timezone, bot token, and chat ID — but not the NTP server field (only configurable via web UI after first connect).

## TODO
- [ ] Generate XBM circuit layout images for all 24 2026 circuits and populate `track_images.h`
- [ ] Tune touch calibration values on actual hardware
- [ ] Call `initTelegram()` in the web server POST handler when bot token changes
- [ ] Wire up `cacheResults()` call in `fetchPostRaceData()` so results survive reboots
- [ ] Add WiFi reconnection logic in the main loop
- [ ] Add NTP server field to WiFiManager captive portal parameters
- [ ] Update `SCHEDULE_URL` and `JOLPICA_BASE_URL` year constant each season
- [ ] Remove or wire up unused `getNextRace()` function
- [ ] End-to-end hardware test and calibration
- [ ] Add Double Reset to reset Wifi Settings
- [ ] Add Debug Methodology from Previous Projects
- [ ] Add Startup information to Serial Port
- [ ] Add F1 Logo to Race Schedule
- [ ] When Telegram info updated on WebUI or manually selected (add UI Button) send Telegram notification
- [ ] 
