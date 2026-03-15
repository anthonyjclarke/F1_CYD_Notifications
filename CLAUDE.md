# Project: F1 CYD Notifications

## Overview
An F1 race schedule display and Telegram notification system running on the ESP32-2432S028R ("Cheap Yellow Display"). Fetches the current F1 season schedule from the sportstimes GitHub JSON feed and post-race results from the Jolpica API. The display cycles through a countdown timer, event details card, and session schedule table during race week, then switches to podium/standings screens for `POST_RACE_DAYS` days after each race. Sends Telegram notifications for race week start, key sessions (qualifying, sprint, race), and race results.

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
  config_manager.h        — LittleFS: load/save AppConfig, schedule/results/time cache
  wifi_setup.h            — WiFiManager captive portal with custom params
  time_utils.h            — NTP init (ezTime), timezone conversion, countdown calc, resyncNTP()
  f1_data.h               — HTTP fetch + JSON parse for schedule/results; UpcomingRace season array
  f1_logo.h               — F1 logo (118×64 RGB565 PROGMEM)
  display_renderer.h      — All TFT_eSPI drawing functions (no state logic here)
  display_states.h        — Display state machine: phase detection + rotation; displayPartialUpdate flag
  telegram_handler.h      — Bot init, message formatting, notification dispatch
  screenshot_capture.h    — SD-backed TFT BMP capture (HSPI), web/button trigger, chunked readRect
  web_server.h            — ESPAsyncWebServer routes, embedded HTML UI (two tabs), ElegantOTA
  f1_race_car.h           — 100×40 RGB565 PROGMEM F1 car image; shown during "On Now" session state
  timezone_ntp_options.h  — Curated IANA timezone list + NTP server list; used by web UI dropdowns
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
- **Time cache**: `/lasttime.json` (LittleFS) — last-known-good UTC epoch; applied via `setTime()` if NTP fails on boot
- **Results cache**: `/results.json` (LittleFS) — defined but not yet used
- **Initial setup**: WiFiManager captive portal (AP: "F1-Display") — exposes timezone, bot token, chat ID fields
- **Runtime config**: Web UI at `http://<device-ip>/` — all settings + brightness slider + screenshot button
- **OTA updates**: ElegantOTA at `http://<device-ip>/update`
- **Key constants** (all in `config.h`):
  - `COUNTDOWN_WEEK_DAYS 7` — days before first session to enter race-week mode
  - `POST_RACE_DAYS 3` — days after GP to show post-race screens
  - `NOTIFY_HOURS_BEFORE 1` — hours before session to fire Telegram notification
  - `RESULTS_POLL_AFTER_SEC` — start polling results 3h after GP start
  - `SCHEDULE_URL` / `JOLPICA_BASE_URL` — year hardcoded, must update each season
  - `STANDINGS_TOP_N 8` — number of standings entries to fetch and display
  - `SCREENSHOT_WEB_ENABLED 1` / `SCREENSHOT_BUTTON_ENABLED 0` — screenshot trigger flags
  - `SCREENSHOT_STARTUP_CAPTURES` — captures BMP at 5 boot stages; currently `0` (release-safe)
  - `FORCE_POST_RACE_TEST_DISPLAYS` — forces post-race screen rotation skipping time logic; currently `0` (release-safe)
  - `WIFI_CHECK_MS (30s)` — WiFi reconnect polling interval
  - `TIME_SAVE_MS (15m)` — periodic LittleFS time save interval
  - `MIN_PLAUSIBLE_EPOCH 1735689600` — 2025-01-01 UTC; guards against corrupt/zero saved times

## Web Endpoints
| Method   | Path              | Description                               |
|----------|-------------------|-------------------------------------------|
| GET      | `/`               | Config + Schedule UI (two-tab)            |
| GET/POST | `/api/config`     | JSON config read/write                    |
| GET      | `/api/status`     | System status JSON                        |
| GET      | `/api/schedule`            | Current race + sessions array JSON                       |
| GET      | `/api/races`               | Full remaining 2026 season races JSON                    |
| GET/POST | `/api/debug`               | Debug level read/write                                   |
| GET      | `/logo.raw`                | Raw RGB565 PROGMEM bytes (F1 logo)                       |
| POST     | `/api/screenshot`          | Trigger capture (SD → file, no SD → RAM heap buffer)     |
| GET      | `/api/screenshot/status`   | `{sd_ready, ram_ready, busy, lastPath, lastError}` for client polling |
| GET      | `/api/screenshot/download` | `?file=<name>` (SD) or `?ram=1` (RAM buffer) → BMP file |
| GET      | `/update`                  | ElegantOTA firmware update UI                            |

## Current State (v0.5.1)
Core functionality fully implemented: display state machine, schedule parsing, Telegram notifications, web config UI, OTA, F1 logo branding (TFT + web), season calendar, anti-flicker countdown, SD screenshot capture, session-aware "On Now" countdown state with real F1 car pixel art, web UI timezone/NTP dropdowns, combined race-week+results rotation, WiFi reconnect with NTP resync, and persistent last-known-good time. Both test flags (`FORCE_POST_RACE_TEST_DISPLAYS`, `SCREENSHOT_STARTUP_CAPTURES`) are set to 0 (release-safe). Tested on hardware through R2 Chinese GP 2026.

## Display State Flow

`determinePhase()` checks conditions in priority order each loop tick:

1. **Post-race** (`secsAfterGP >= 0 && daysAfterGP < POST_RACE_DAYS`):
   Rotation every `POST_RACE_ROTATE_MS` (10 s):
   `WINNER → DRIVERS → CONSTRUCTORS → NEXT_RACE → (loop)`
   Exits after `POST_RACE_DAYS` (3) days — `checkPostRaceExpiry()` immediately re-fetches schedule.

2. **Race week** (`daysToFirst >= 0 && daysToFirst <= COUNTDOWN_WEEK_DAYS` = 7):
   Rotation every `DISPLAY_ROTATE_MS` (8 s).
   - **Standard** (no previous results): `COUNTDOWN → EVENT_DETAILS → SCHEDULE → (loop)`
   - **Combined** (results available): `COUNTDOWN → EVENT_DETAILS → SCHEDULE → WINNER → DRIVERS → CONSTRUCTORS → (loop)`

3. **Idle** (between race weeks):
   Single `COUNTDOWN` screen, digits refresh every `COUNTDOWN_UPDATE_MS` (1 s).

**Screen content:**
| State | What it shows |
|---|---|
| `STATE_IDLE` / `STATE_RACE_WEEK_COUNTDOWN` | Days or HH:MM:SS countdown with F1 logo; "On Now" car view during live session |
| `STATE_RACE_WEEK_EVENT_DETAILS` | Round N · race name · location · date range · Sprint/Standard badge · first session + GP times |
| `STATE_RACE_WEEK_SCHEDULE` | Session table: name / day / local time for all sessions |
| `STATE_POST_RACE_WINNER` | Top-5 race result |
| `STATE_POST_RACE_DRIVERS` | Driver standings, top `STANDINGS_TOP_N` |
| `STATE_POST_RACE_CONSTRUCTORS` | Constructor standings, top `STANDINGS_TOP_N` |
| `STATE_POST_RACE_NEXT_RACE` | Countdown to next race |

**Key config constants** (all in `config.h`) that drive the above:
- `COUNTDOWN_WEEK_DAYS 7` — days before first session to enter race-week mode
- `POST_RACE_DAYS 3` — days after GP to show post-race screens
- `DISPLAY_ROTATE_MS 8000` — ms between race-week screen advances
- `POST_RACE_ROTATE_MS 10000` — ms between post-race screen advances
- `COUNTDOWN_UPDATE_MS 1000` — countdown digit refresh interval
- `STANDINGS_TOP_N 8` — rows shown and fetched for standings screens
- `MAX_PODIUM 5` — positions shown on race result screen
- `FORCE_POST_RACE_TEST_DISPLAYS 0` — set to 1 to force post-race rotation for UI testing

## Architecture Notes
- **Display state machine** (`display_states.h`): `determinePhase()` maps current time vs race timestamps to idle / race-week / post-race. Respects `FORCE_POST_RACE_TEST_DISPLAYS` compile flag to bypass time logic for UI testing. `advanceDisplayState()` rotates sub-states on timer or touch tap.
- **Anti-flicker partial countdown**: `displayPartialUpdate` flag distinguishes 1-second digit ticks (partial) from state transitions (full redraw). `drawCountdown(race, cd, bool partialUpdate)` erases only the number region on partial updates. `static int16_t _prevCountdownDays` and `static bool _prevOnNow` force full redraw at day-boundary or isOnNow transitions. During active F1 sessions, switches to "On Now" display (red header, race name/location, F1 car `pushImage` from PROGMEM, "ON / NOW!" text).
- **Session-aware countdown** (`display_states.h`): `CountdownTarget` struct pairs `time_t utcTime` with `SessionType`. `getCountdownTarget(race)` walks sessions to find the next/current target: if a session is running it returns that session; if a session-day has a future session it returns that; otherwise falls back to next race in `upcomingRaces[]`. `renderCurrentState()` calls `getCountdownWithSession(utcTime, sessionType)` which sets `cd.isOnNow = true` when now falls within the session window (using `getSessionDurationSeconds()`). Fallback path logs `DBG_WARN` with `nowUTC()` to surface clock/schedule issues.
- **Session durations** (`time_utils.h`): `getSessionDurationSeconds(SessionType)` — FP 60 min, Sprint Qualifying 30 min, Sprint 90 min, Qualifying 60 min, GP 120 min. Used for isOnNow window calculation.
- **Persistent time / NTP fallback** (`time_utils.h`, `config_manager.h`): `initTime()` attempts NTP first; on success saves epoch to `/lasttime.json` via `saveLastKnownTime()`. On NTP timeout, loads the saved epoch via `loadLastKnownTime()` and applies it with `setTime()` if `> MIN_PLAUSIBLE_EPOCH` — keeps race schedule resolution correct even without network. Static `_ntpSyncedOnce` flag (exposed via `ntpHasSynced()`) prevents re-saving a stale fallback time before a real NTP sync occurs. Time is also saved periodically every `TIME_SAVE_MS` (15 min) while running.
- **WiFi reconnect** (`main.cpp`): `checkWiFiReconnect()` polls `WiFi.status()` every `WIFI_CHECK_MS` (30 s). On drop, calls `WiFi.reconnect()`. On reconnection, calls `resyncNTP()` which sets ezTime's sync interval to 1 s, fires `events()`, then restores the 1-hour interval — triggers an NTP query without blocking the loop.
- **Web UI dropdowns** (`timezone_ntp_options.h`): `TIMEZONE_OPTIONS[]` (IANA strings, ~60 entries) and `NTP_SERVER_OPTIONS[]` (~14 entries) as `constexpr` arrays; web UI populates `<select>` elements from `/api/config` current values.
- **Timezone-correct day counting** (`time_utils.h`): `daysUntilLocalDate(targetUtc)` converts both now and target to local civil dates using Gregorian day numbers, then diffs — avoids DST/midnight artefacts. All `myTZ.tzTime()` calls use the `UTC_TIME` flag to correctly treat input as UTC. `nowUTC()` returns `::now()` (TimeLib epoch) to avoid timezone-object side effects. Timezone is set before NTP sync so date math is valid even on sync timeout.
- **F1 logo** (`f1_logo.h`): 118×64 RGB565 PROGMEM. Rendered on white card (`fillRoundRect` + `pushImage`) on TFT (splash screen y=5 and countdown y=66) and web canvas (CSS `background:#fff`). Logo is designed for white backgrounds.
- **Season calendar** (`f1_data.h`): `UpcomingRace` struct (round, isSprint, name[24], location[16], gpTimeUtc) — up to 25 entries. Populated in `parseSchedule()` from `nextIdx` to season end. Served via `/api/races`; web Schedule tab renders full 2026 table.
- **Standings display**: `STANDINGS_TOP_N 8` controls both API fetch limit and renderer loop count for driver and constructor standings. Points shown as "N pts". Both `fetchDriverStandings()` and `fetchConstructorStandings()` pass `STANDINGS_TOP_N` as the `?limit=` query param.
- **getPrevRace()=races[0], getCurrentRace()=races[1], getNextRace()=races[2]** — all used. `parseSchedule()` determines `nextIdx` (first race where `gp + POST_RACE_DAYS*86400 > now`) and loads prev/current/next around it. Overlap check: if `nextIdx+1` race is within `COUNTDOWN_WEEK_DAYS`, advances `nextIdx` to enable combined rotation.
- **Combined race-week rotation** (when `resultsAvailable`): COUNTDOWN→EVENT_DETAILS→SCHEDULE→WINNER→DRIVERS→CONSTRUCTORS (8s each). In combined mode, `renderCurrentState()` uses `getPrevRace()` (races[0]) for the winner screen title.
- **Pure post-race rotation**: WINNER→DRIVERS→CONSTRUCTORS→NEXT_RACE (10s each). NEXT_RACE shows `getNextRace()` = races[2].
- **`checkPostRaceExpiry()`** in main loop: detects post-race→non-post-race transition, immediately calls `fetchSchedule()` so races[1] advances to next race without waiting 24h timer.
- **Boot-time post-race fetch**: if `secsAfterGP` is in `[RESULTS_POLL_AFTER_SEC, RESULTS_GIVE_UP_SEC)`, results are fetched immediately on boot rather than waiting up to 30 min for the first poll.
- **Screenshot capture** (`screenshot_capture.h`): SD via HSPI @ 8 MHz. `tft.readRect()` in 16-row chunks, writes 24-bit BMP bottom-up with ESP32 byte-swap. Request-queue model — web/button queues, main loop executes. Filenames: `/shots/shot_YYYYMMDD_HHMMSS.bmp` (user timezone after sync) or `/shots/shot_unsynced_XXXXXX.bmp` before time sync. `utime()` sets file timestamps. Web UI polls `/api/screenshot/status` (250 ms, up to 10 s) until `busy` clears, then shows download link from `lastPath`.
- **PROGMEM image transparency caveat**: TFT_eSPI `pushImage` with a transparency key on ESP32 SPI is unreliable due to byte-swap timing — the key comparison fails. For images on a black background, set background pixels to `0x0000` and call `pushImage` without a transparency parameter. The `fillRect(COLOR_BG)` before the call clears the region. See `f1_race_car.h` and `tools/generate_f1_car.py`.
- **Non-blocking timing**: All periodic tasks use `millis()` — no `delay()` in loop.
- **Notification deduplication**: Per-round bitmask persisted to LittleFS; not re-sent across reboots.
- **HTTPS**: `WiFiClientSecure` with `setInsecure()` for GitHub, Telegram, and Jolpica.

## Known Issues
- **Touch calibration untested**: Values `{300, 3600, 300, 3600, 7}` are approximate.
- **Telegram re-init on config change**: New bot token requires reboot; web UI update does not call `initTelegram()`.
- **Results not cached**: `cacheResults()` / `loadCachedResults()` implemented but never called — results lost on reboot.
- **NTP server not in captive portal**: Only configurable via web UI after first connect.
- **`getNextRace()`**: Now used in `STATE_POST_RACE_NEXT_RACE` render state.
- **Duplicate timezone entries**: `timezone_ntp_options.h` has `America/Toronto` and `Asia/Bangkok` listed twice.
