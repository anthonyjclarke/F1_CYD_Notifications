# Changelog

All notable changes to this project will be documented in this file.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Version scheme: `MAJOR.MINOR.PATCH`
- MAJOR: breaking hardware/config change
- MINOR: new feature or significant addition
- PATCH: bug fix or small tweak

---

## [To Do]
  - Create a "Production" Flag that will not compile the diagnostics tools like Screenshot Capture
  - Stop Display Flicker on "RACE_WEEK_XXXXX"
  - How to maximise memory available for code

## [Unreleased]

---

## [0.5.1] - 2026-03-16

### Fixed
- **Wrong race displayed after overnight run** — root cause: ESP32 has no battery-backed RTC; if NTP sync fails on boot (e.g. router WiFi sleeping, brief outage), `parseSchedule()` resolved the current race against a bad clock (often epoch or a very old time), making a past race appear upcoming and showing the wrong location/date on the countdown screen. Reboot forced a fresh NTP sync and corrected it.

### Added
- **Persistent last-known-good time** (`/lasttime.json` on LittleFS): UTC epoch saved after every successful NTP sync and every 15 minutes while running. On boot, if `waitForSync()` times out, the saved epoch is applied via `setTime()` as a fallback — keeping race schedule parsing correct even without network. Guarded by `MIN_PLAUSIBLE_EPOCH` (2025-01-01) to reject corrupt or empty files.
- **WiFi reconnect detection** (`checkWiFiReconnect()`, polled every 30 s): detects loss of connectivity and calls `WiFi.reconnect()`; on reconnection immediately calls `resyncNTP()` to restore accurate time without waiting up to an hour for ezTime's automatic re-sync interval.
- **`resyncNTP()`** in `time_utils.h`: sets ezTime sync interval to 1 s, calls `events()` to dispatch the NTP query, then restores the 1-hour interval — safe to call at any point from the main loop.
- **Fallback warn log** in `getCountdownTarget()`: if no future sessions are found in the schedule or upcoming-races list (the "shouldn't happen" path), a `DBG_WARN` now logs `now()` to make NTP/schedule issues immediately visible in serial output.

---

## [0.5.0] - 2026-03-09

### Added
- Combined race-week + post-race screen rotation: when previous race results are available during race week, display cycles through 5 screens (COUNTDOWN → EVENT_DETAILS → SCHEDULE → WINNER → DRIVERS → CONSTRUCTORS → loop) instead of the standard 3 race-week screens only
- `getPrevRace()` in `f1_data.h` returns `races[0]` for use in combined mode; winner screen correctly attributes results to the previous race, not the upcoming one
- `checkPostRaceExpiry()` in `main.cpp`: detects when the post-race window closes and immediately triggers a schedule refresh — eliminates up to 24h IDLE gap before race-week countdown activates
- Boot-time post-race results fetch: if the device boots inside the post-race window, results are fetched immediately at startup rather than waiting up to 30 minutes for the first poll cycle
- `STATE_POST_RACE_NEXT_RACE` included in pure post-race rotation: after constructor standings, shows countdown to next race before cycling back to winner screen
- Top 5 race result display on RACE RESULT screen (was top 3 / podium only)
- Top 8 driver and constructor standings display (was top 5)
- Full driver name shown in DRIVER STANDINGS when it fits the column width; falls back to surname only when truncation would occur (using `tft.textWidth()`)

### Changed
- `POST_RACE_DAYS` reduced from 7 to 3: tighter post-race window, leaving more days for the dedicated race-week countdown rotation before the next event
- `STANDINGS_TOP_N` increased from 5 to 8 — controls both the Jolpica API `?limit=` parameter and the renderer loop count
- `MAX_PODIUM` increased from 3 to 5 — controls both the Jolpica API `?limit=` and the race result screen
- `RESULTS_GIVE_UP_SEC` changed from hardcoded `24h` to `POST_RACE_DAYS × 86400` so the polling window tracks the configured post-race duration
- `renderCurrentState()` now accepts a `phase` parameter to distinguish combined race-week context from pure post-race, used to select the correct race data for the winner screen
- RACE RESULT screen uses uniform `FreeSans9pt7b` throughout (was mixed 12pt/9pt); constructor team shown in `COLOR_SESSION_TEXT` below driver name; positions 1–3 highlighted in `COLOR_HIGHLIGHT` (yellow), 4–5 in `COLOR_TEXT` (white)

### Fixed
- **Jolpica API HTTPS redirect** (critical): all three Jolpica endpoints (`results`, `driverStandings`, `constructorStandings`) were using `http://api.jolpi.ca` which returns HTTP 301; the ESP32 `HTTPClient` did not follow the redirect, causing all post-race screens to display "Status Pending" with no data. Fixed by switching to `WiFiClientSecure` with `client.setInsecure()` and `https://` base URL in `config.h` and all fetch functions in `f1_data.h`
- Post-race polling silently stopping before data arrived: `RESULTS_GIVE_UP_SEC` was fixed at 24h, so any device booting more than 24h after the GP would never poll for results even though the post-race window was still open
- Post-race → race-week transition gap: when the post-race window expired, the device remained in IDLE for up to 24h (until the next scheduled `fetchSchedule()` ran). `checkPostRaceExpiry()` now triggers an immediate refresh the moment the window closes

---

## [0.4.0] - 2026-03-08

### Added
- SD screenshot capture subsystem (`include/screenshot_capture.h`) with:
  - Web trigger endpoint `POST /api/screenshot`
  - Optional GPIO button trigger (`PIN_SHOT_BTN`, debounce)
  - Optional startup capture points controlled by `SCREENSHOT_STARTUP_CAPTURES`
  - 24-bit BMP output to `/shots`
- Dedicated HSPI wiring for SD screenshot path: `PIN_SD_CS`, `PIN_SD_SCK`, `PIN_SD_MISO`, `PIN_SD_MOSI`
- "On Now" session display state (`include/display_renderer.h`) with real F1 racing car pixel art image (`include/f1_race_car.h`)
- F1 racing car image (`include/f1_race_car.h`): 100×40 RGB565 PROGMEM side-view pixel art; black background matches `COLOR_BG` — no transparency needed; generated by `tools/generate_f1_car.py`
- `tools/generate_f1_car.py`: Python/Pillow script to regenerate car art and header; produces preview PNGs alongside header
- Enhanced countdown logic with session detection (`getCountdownWithSession()` in `time_utils.h`)
- WebUI screenshot download endpoint `GET /api/screenshot/download`
- `GET /api/screenshot/status` now returns `lastPath` and `lastError` fields for client polling
- Timezone and NTP server selectable from curated dropdowns (`include/timezone_ntp_options.h`) in Web UI
- `include/timezone_ntp_options.h` with ~60 IANA timezone entries and ~14 NTP server options

### Changed
- Screenshot capture robustness:
  - Chunked TFT readback (16-row strips) to reduce RAM pressure
  - Color correction for TFT readback byte-order
  - Timestamp-based filenames after time sync
  - Explicit unsynced fallback filenames (`shot_unsynced_XXXXXX.bmp`)
  - File timestamp metadata stamping via `utime()` fallback path
- Splash version string now uses `APP_VERSION` from `config.h` (removed hardcoded `v0.2.0`)
- `getCountdownTarget()` in `display_states.h` now finds next upcoming session across all races in season
- Fixed display coordinates to match actual TFT_eSPI configuration (240x320 → 320x240 landscape)
- Web server endpoints separated: `POST /api/screenshot` for capture, `GET /api/screenshot/download` for file serving
- Web UI screenshot flow: SD captures now poll `/api/screenshot/status` until complete before showing download link (was showing stale previous path on first press)

### Fixed
- Countdown showing 00:00:00 after sessions end by improving session target selection logic
- Display positioning issues caused by SCREEN_WIDTH/HEIGHT mismatch with TFT hardware
- F1 car image in "On Now" state: was using `fillRect` green placeholder; now uses correct `pushImage` with properly sized 4000-element PROGMEM array
- TFT `pushImage` byte-swap issue: avoided using transparency key colour for PROGMEM images on ESP32; background pixels are set to `0x0000` (matching `COLOR_BG`) instead
- Screenshot Web UI first-press bug: `POST /api/screenshot` response returned stale `lastPath` before capture completed; JS now polls `/api/screenshot/status` for both SD and RAM paths
- Fixed `fillRect` height parameter bug in "On Now" car image area clear

### Planned
- Touch calibration tuning on hardware
- `initTelegram()` call in web config POST when bot token changes
- `cacheResults()` wired into `fetchPostRaceData()` for reboot persistence
- WiFi reconnection logic in main loop
- NTP server field in WiFiManager captive portal
- Double-reset detection to clear WiFi credentials (e.g. ESP_DoubleResetDetector)
- Season rollover: update `SCHEDULE_URL` and `JOLPICA_BASE_URL` year
- End-to-end hardware test and display calibration
- Remove unused `getNextRace()` or wire it into active flow
- Send Telegram notification when Telegram config is updated from Web UI/manual action

---

## [0.3.0] - 2026-03-02

### Added
- **F1 logo on TFT splash screen** — `pushImage()` with `TFT_WHITE` transparency renders logo on dark background; replaces previous "F1" text header with branded image; thin red separator divides logo from subtitle text
- **F1 logo on TFT countdown screen** — logo displayed between location text and day-count when `cd.days > 0`; compact 9pt race name/location to accommodate logo; HH:MM:SS urgent mode (days == 0) unchanged
- **`/logo.raw` web endpoint** — serves raw RGB565 PROGMEM bytes with 24 h browser cache header; no RAM copy needed (`beginResponse_P`)
- **`/api/schedule` web endpoint** — returns current race as JSON: `name`, `location`, `round`, `isSprint`, and `sessions[]` array with `label`, `day`, `time`, `type` (enum int), `utc` (epoch)
- **Schedule tab in web UI** — two-tab layout (⚙ Config / 📅 Schedule); Schedule tab shows race header with sprint badge, session table with SESSION / DAY / TIME / IN columns
- **Live countdown column** — "In" cells update every second via `setInterval`; format: `Xd Yh` (days), `Xh Ym` (hours), `Xm YYs` (< 1 hour); past sessions show em-dash
- **Session row colour coding** — next upcoming session highlighted green; GP row label red; Sprint label yellow; past sessions dimmed; all via CSS classes applied at render time
- **Canvas logo in web UI** — `<canvas>` element renders RGB565 data from `/logo.raw` via `Uint16Array`; white pixels made transparent (alpha=0) for seamless dark-theme display; scaled 2× via CSS `image-rendering: pixelated`

### Changed
- `drawSplashScreen()` — logo replaces "F1" text in large red bar; layout now: logo (y=10–74) → red line (y=82) → "Race Schedule" subtitle → status text
- `drawCountdown()` — days > 0 branch: race name shrunk to FreeSans9pt7b to recover vertical space; logo inserted at y=66; day count repositioned to y=163 (clear of logo); DAYS label at y=200; session info at y=222
- `include/display_renderer.h` — added `#include "f1_logo.h"`
- `include/web_server.h` — added `#include "f1_logo.h"`; removed `nextRace` field from `/api/status` (now served by `/api/schedule`); HTML rewritten with tab structure and schedule table

---

## [0.2.0] - 2026-03-02

### Added
- `include/debug.h` — leveled serial logging system with runtime control
  - Four macros: `DBG_ERROR`, `DBG_WARN`, `DBG_INFO`, `DBG_VERBOSE`
  - Levels: 0=Off, 1=Error, 2=Warn, 3=Info (default), 4=Verbose
  - `debugLevel` variable adjustable at runtime without recompile
- Web API endpoints for debug level control
  - `GET /api/debug` → `{"level": N}`
  - `POST /api/debug` body `level=N` → updates `debugLevel` and returns new value
- Debug level selector (0–4 dropdown + Apply button) added to web config UI
- Numbered setup step banners in `setup()` — `[Main] 1/11 ... 11/11` — for clear boot sequence tracing on serial monitor
- `stateName()` helper in `display_states.h` for human-readable state names in logs
- Display phase transition and screen rotation logging (`[Display] Phase change →`, `Screen rotate: A → B`)
- Touch coordinate logging at VERBOSE level
- Results polling activation log with elapsed hours since GP
- WiFi RSSI logged alongside IP on successful connection

### Changed
- All `Serial.println(F(...))` and `Serial.printf(...)` calls replaced with `DBG_*` macros across all modules:
  - `src/main.cpp`
  - `include/config_manager.h`
  - `include/wifi_setup.h`
  - `include/time_utils.h`
  - `include/f1_data.h`
  - `include/telegram_handler.h`
  - `include/display_states.h`
  - `include/web_server.h`
- Previously silent HTTP failure paths in `fetchDriverStandings()` and `fetchConstructorStandings()` now emit `DBG_WARN`
- `saveConfig()` demoted from `DBG_INFO` to `DBG_VERBOSE` (called every minute in loop)
- Telegram send path: per-send detail at VERBOSE, result (OK/fail) at INFO/WARN
- Config load summary extended to include brightness value
- F1 schedule parse now logs JSON payload size and whether weekend is sprint or standard

---

## [0.1.0] - 2026-03-02

### Added
- `platformio.ini` — ESP32-2432S028R (2.8" CYD) target
  - `espressif32@6.9.0`, `esp32dev` board, Arduino framework
  - TFT_eSPI configured entirely via `build_flags` (no `User_Setup.h`)
  - ILI9341 240×320 @ 55 MHz SPI, XPT2046 touch @ 2.5 MHz
  - LittleFS filesystem (min_spiffs partition)
  - Libraries: TFT_eSPI 2.5.43, ArduinoJson 7.4.0, WiFiManager, Universal-Arduino-Telegram-Bot, ESPAsyncWebServer 3.6.2, AsyncTCP 3.3.3, ElegantOTA 3.1.6, ezTime 0.8.3
- `include/config.h` — all pin definitions, display constants, timing intervals, colour palette (RGB565), file paths, API URLs
- `include/types.h` — core data structures: `RaceData`, `SessionInfo`, `RaceResult`, `StandingEntry`, `AppConfig`; `DisplayState` and `SessionType` enums; notification bitmask defines
- `include/config_manager.h` — LittleFS init, `loadConfig`/`saveConfig` (JSON), schedule and results cache read/write
- `include/wifi_setup.h` — WiFiManager captive portal with custom parameters for timezone, Telegram bot token and chat ID
- `include/time_utils.h` — NTP initialisation via ezTime, IANA timezone, `formatLocalDay/Time/FullDate`, `getCountdown`, `isWithinHours`, `isMonday`, `daysSince`
- `include/f1_data.h` — F1 schedule fetch from sportstimes GitHub JSON; post-race results, driver standings, constructor standings from Jolpica (Ergast-compatible) API; `parseSchedule` loads prev/current/next race into fixed array
- `include/display_renderer.h` — all TFT_eSPI drawing functions: splash screen, schedule table, countdown (day/HH:MM:SS), race winner/podium, driver standings, constructor standings, status overlay, LDR auto-brightness
- `include/display_states.h` — 7-state display state machine: `IDLE`, `RACE_WEEK_COUNTDOWN`, `RACE_WEEK_EVENT_DETAILS`, `RACE_WEEK_SCHEDULE`, `POST_RACE_WINNER`, `POST_RACE_DRIVERS`, `POST_RACE_CONSTRUCTORS`, `POST_RACE_NEXT_RACE`; auto-rotation via `millis()` timer; touch-driven manual advance
- `include/telegram_handler.h` — bot init, message formatting for race week / pre-session / results; `checkNotifications()` with per-round bitmask deduplication persisted to LittleFS
- `include/web_server.h` — ESPAsyncWebServer config UI (PROGMEM HTML, dark F1-themed), `GET/POST /api/config`, `GET /api/status`; ElegantOTA at `/update`
- `src/main.cpp` — `setup()` / `loop()`; RGB LED boot status (blue=connecting, green=connected); resistive touch input; non-blocking `millis()` scheduling for: display updates (1 s countdown tick, 8/10 s screen rotation), notification checks (1 min), post-race results polling (30 min retry, 3–24 h window), schedule refresh (24 h), brightness update (10 s)
- `CLAUDE.md` — project documentation for Claude Code: hardware, pin map, architecture, known issues, TODO list
- `F1 Notification CYD Project_Brief.txt` — original project specification and feature requirements
