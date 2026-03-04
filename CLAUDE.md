# Project: F1 CYD Notifications

## Overview
An F1 race schedule display and Telegram notification system running on the ESP32-2432S028R ("Cheap Yellow Display"). Fetches the current F1 season schedule from the sportstimes GitHub JSON feed and post-race results from the Jolpica API. The display cycles through a countdown timer, session schedule table, and circuit layout during race week, then switches to podium/standings screens for 7 days after each race. Sends Telegram notifications for race week start, key sessions (qualifying, sprint, race), and race results.

## Hardware
- **MCU**: ESP32-2432S028R (2.8" Cheap Yellow Display)
- **Display**: ILI9341 TFT, 240×320 (used landscape: 320×240)
- **Touch**: XPT2046 resistive touchscreen (tap to manually advance display)
- **Onboard RGB LED**: Active-LOW, GPIO 4/16/17 — used as boot status indicator
- **Onboard LDR**: GPIO34 — ADC input for auto-brightness mode
- **Storage**: LittleFS on internal flash; microSD on HSPI (optional, for screenshots)
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
  main.cpp                — Setup (12 steps), loop, timing orchestration, LED, touch
include/
  config.h                — All #defines: pins, colors, timing constants, API URLs, screenshot flags
  types.h                 — Structs: RaceData, SessionInfo, AppConfig, enums
  config_manager.h        — LittleFS: load/save AppConfig, schedule/results cache
  wifi_setup.h            — WiFiManager captive portal with custom params
  time_utils.h            — NTP init (ezTime), timezone conversion, countdown calc
  f1_data.h               — HTTP fetch + JSON parse for schedule/results; UpcomingRace season array
  f1_logo.h               — F1 logo (118×64 RGB565 PROGMEM)
  display_renderer.h      — All TFT_eSPI drawing functions (no state logic here)
  display_states.h        — Display state machine: phase detection + rotation; displayPartialUpdate flag
  telegram_handler.h      — Bot init, message formatting, notification dispatch
  screenshot_capture.h    — SD-backed TFT BMP capture (HSPI), web/button trigger, chunked readRect
  web_server.h            — ESPAsyncWebServer routes, embedded HTML UI (two tabs), ElegantOTA
  track_images.h          — XBM circuit image store (placeholder — not yet populated)
```

All application logic lives in header files. `main.cpp` is the only translation unit — this is intentional.

## Pin Mapping
| Function        | GPIO | Notes                              |
|-----------------|------|------------------------------------|
| TFT MOSI        | 13   | SPI @ 55 MHz                       |
| TFT MISO        | 12   |                                    |
| TFT SCLK        | 14   |                                    |
| TFT CS          | 15   |                                    |
| TFT DC          | 2    |                                    |
| TFT RST         | -1   | Not connected on CYD               |
| Backlight       | 21   | LEDC PWM ch0, 5 kHz, 8-bit         |
| Touch CS        | 33   | SPI @ 2.5 MHz                      |
| Touch IRQ       | 36   | Input-only pin                     |
| RGB LED Red     | 4    | Active LOW                         |
| RGB LED Green   | 16   | Active LOW                         |
| RGB LED Blue    | 17   | Active LOW                         |
| LDR             | 34   | ADC — brightness=0 enables auto    |
| SD CS           | 5    | HSPI — screenshot module           |
| SD MOSI         | 23   | HSPI                               |
| SD MISO         | 19   | HSPI                               |
| SD SCK          | 18   | HSPI                               |
| Screenshot btn  | 27   | Optional, active LOW, INPUT_PULLUP |

## Configuration
- **Config file**: `/config.json` (LittleFS) — timezone, NTP server, Telegram bot token/chat ID, brightness
- **Schedule cache**: `/races.json` (LittleFS) — raw JSON from sportstimes, used as offline fallback
- **Results cache**: `/results.json` (LittleFS) — defined but not yet used
- **Initial setup**: WiFiManager captive portal (AP: "F1-Display") — exposes timezone, bot token, chat ID fields
- **Runtime config**: Web UI at `http://<device-ip>/` — all settings + brightness slider + screenshot button
- **OTA updates**: ElegantOTA at `http://<device-ip>/update`
- **Key constants** (all in `config.h`):
  - `COUNTDOWN_WEEK_DAYS 7` — days before first session to enter race-week mode
  - `POST_RACE_DAYS 7` — days after GP to show post-race screens
  - `NOTIFY_HOURS_BEFORE 1` — hours before session to fire Telegram notification
  - `RESULTS_POLL_AFTER_SEC` — start polling results 3h after GP start
  - `SCHEDULE_URL` / `JOLPICA_BASE_URL` — year hardcoded, must update each season
  - `STANDINGS_TOP_N 5` — number of standings entries to fetch and display
  - `SCREENSHOT_WEB_ENABLED 1` / `SCREENSHOT_BUTTON_ENABLED 0` — screenshot trigger flags
  - `SCREENSHOT_STARTUP_CAPTURES 1` — captures BMP at 5 boot stages; **set to 0 before release builds**
  - `FORCE_POST_RACE_TEST_DISPLAYS 1` — forces post-race screen rotation skipping time logic; **set to 0 before release builds**

## Web Endpoints
| Method   | Path              | Description                               |
|----------|-------------------|-------------------------------------------|
| GET      | `/`               | Config + Schedule UI (two-tab)            |
| GET/POST | `/api/config`     | JSON config read/write                    |
| GET      | `/api/status`     | System status JSON                        |
| GET      | `/api/schedule`   | Current race + sessions array JSON        |
| GET      | `/api/races`      | Full remaining 2026 season races JSON     |
| GET/POST | `/api/debug`      | Debug level read/write                    |
| GET      | `/logo.raw`       | Raw RGB565 PROGMEM bytes (F1 logo)        |
| POST     | `/api/screenshot` | Trigger SD screenshot capture             |
| GET      | `/update`         | ElegantOTA firmware update UI             |

## Current State (v0.3.0+)
Core functionality implemented: display state machine, schedule parsing, Telegram notifications, web config UI, OTA, F1 logo branding (TFT + web), season calendar, anti-flicker countdown, and SD screenshot capture. Main outstanding gap is circuit layout images — track screen shows "Track image not available" for all 24 circuits. Not yet tested end-to-end on hardware.

Post-v0.3.0 improvements (uncommitted): timezone-correct day counting (`daysUntilLocalDate()`), fixed `myTZ.tzTime()` UTC_TIME flag throughout, standings limited to top 5 via `STANDINGS_TOP_N`, `FORCE_POST_RACE_TEST_DISPLAYS` test flag, per-fetch diagnostic logging for post-race data.

## Architecture Notes
- **Display state machine** (`display_states.h`): `determinePhase()` maps current time vs race timestamps to idle / race-week / post-race. Respects `FORCE_POST_RACE_TEST_DISPLAYS` compile flag to bypass time logic for UI testing. `advanceDisplayState()` rotates sub-states on timer or touch tap.
- **Anti-flicker partial countdown**: `displayPartialUpdate` flag distinguishes 1-second digit ticks (partial) from state transitions (full redraw). `drawCountdown(race, cd, bool partialUpdate)` erases only the number region on partial updates. `static int16_t _prevCountdownDays` forces full redraw at day-boundary transitions.
- **Timezone-correct day counting** (`time_utils.h`): `daysUntilLocalDate(targetUtc)` converts both now and target to local civil dates using Gregorian day numbers, then diffs — avoids DST/midnight artefacts. All `myTZ.tzTime()` calls use the `UTC_TIME` flag to correctly treat input as UTC. `nowUTC()` returns `::now()` (TimeLib epoch) to avoid timezone-object side effects. Timezone is set before NTP sync so date math is valid even on sync timeout.
- **F1 logo** (`f1_logo.h`): 118×64 RGB565 PROGMEM. Rendered on white card (`fillRoundRect` + `pushImage`) on TFT (splash screen y=5 and countdown y=66) and web canvas (CSS `background:#fff`). Logo is designed for white backgrounds.
- **Season calendar** (`f1_data.h`): `UpcomingRace` struct (round, isSprint, name[24], location[16], gpTimeUtc) — up to 25 entries. Populated in `parseSchedule()` from `nextIdx` to season end. Served via `/api/races`; web Schedule tab renders full 2026 table.
- **Standings display**: `STANDINGS_TOP_N 5` controls both API fetch limit and renderer loop count for driver and constructor standings. Points shown as "N pts". Both `fetchDriverStandings()` and `fetchConstructorStandings()` pass `STANDINGS_TOP_N` as the `?limit=` query param.
- **Screenshot capture** (`screenshot_capture.h`): SD via HSPI @ 8 MHz. `tft.readRect()` in 16-row chunks, writes 24-bit BMP bottom-up with ESP32 byte-swap. Request-queue model — web/button queues, main loop executes. Filenames: `/shots/shot_YYYYMMDD_HHMMSS.bmp` (user timezone after sync) or `/shots/shot_unsynced_XXXXXX.bmp` before time sync. `utime()` sets file timestamps.
- **Non-blocking timing**: All periodic tasks use `millis()` — no `delay()` in loop.
- **Notification deduplication**: Per-round bitmask persisted to LittleFS; not re-sent across reboots.
- **HTTPS**: `WiFiClientSecure` with `setInsecure()` for GitHub and Telegram. Jolpica uses plain HTTP.

## Known Issues
- **Track images not implemented**: `getTrackImage()` always returns `nullptr`; all 24 circuits show fallback text.
- **Touch calibration untested**: Values `{300, 3600, 300, 3600, 7}` are approximate.
- **Telegram re-init on config change**: New bot token requires reboot; web UI update does not call `initTelegram()`.
- **Results not cached**: `cacheResults()` / `loadCachedResults()` implemented but never called — results lost on reboot.
- **No WiFi reconnect**: No reconnection attempt if WiFi drops after boot.
- **NTP server not in captive portal**: Only configurable via web UI after first connect.
- **`SCREENSHOT_STARTUP_CAPTURES 1`**: Must be set to 0 in `config.h` before release builds.
- **`FORCE_POST_RACE_TEST_DISPLAYS 1`**: Must be set to 0 in `config.h` before release builds.
- **`getNextRace()`**: Exists in `f1_data.h` but is unused.
