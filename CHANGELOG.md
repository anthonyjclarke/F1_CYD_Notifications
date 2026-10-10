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

## [0.7.0] Unreleased

### Added
- Web UI **Hardware & Diagnostics** panel on the new **System / Diagnostics** tab (replaces Links / Status on the Config tab): firmware version, build time, running partition, Arduino core / IDF; board, chip, flash, device name, MAC, SD card; WiFi SSID and signal, IP, NTP sync state, local time; uptime, last reset reason (e.g. watchdog, brownout, crash), free / minimum heap and largest block, LittleFS usage; current race and which round's results are cached. Served by the extended `GET /api/status`.

### Fixed
- Web UI showed brightness 128 when it was set to 0 (auto), so saving any other setting switched auto brightness off. It now shows "Auto" and keeps 0.
- Web UI messages stopped appearing after the first one had timed out (an inline `display:none` was left on the message box). Messages now also stay in view while scrolling.
- Some Telegram notifications arrived twice. Universal-Arduino-Telegram-Bot 1.3.0 re-POSTs a message for up to 8 s until it reads `"ok":true`, but stops reading the reply after the first chunk (1.5 s limit), so a slow reply from Telegram was taken as a failure and the already-delivered message was sent again. If every attempt was misread, the next minute's check sent it once more. Messages are now sent with one `HTTPClient` POST per attempt (`TELEGRAM_TIMEOUT_MS` 10 s), counted as sent on HTTP 200.
- Notification bits were cleared on any change of round, including a step back to an earlier round from a schedule parsed against a stale fallback clock, so the race-week message could be re-sent once real time returned. Bits are now tied to the race's GP time (`notGp` in `/config.json`) and reset only for a later race; notifications wait until NTP has synced. Configs from 0.6.2 and earlier keep their bits for the current round.
- A notification bit is saved to `/config.json` straight after its message is sent, not at the end of the check, so a reboot in between can't re-send it.
- The "NTP synced" flag was never set when ezTime synced in the background after a boot that used the saved fallback time (so the 15-minute time saves never ran), and `resyncNTP()` set it on the fallback time alone. It now follows ezTime's own last-sync time.
- Telegram messages showed literal `*` characters; they are now sent with `parse_mode=Markdown`. A message Telegram rejects as Markdown (HTTP 400, nothing delivered) is resent as plain text.
- Once one race's results had been fetched, `resultsAvailable` stayed true until reboot: the next race's results were never polled, its results notification was pre-marked as sent, and its post-race screen showed the previous podium. Results are now tied to the round they were fetched for (`hasResultsFor()`), and invalidated while a fetch is in progress.
- The results notification is now tracked by the GP time of the race it was for (`resGp` in `/config.json`) instead of a bit in the current round's bitmask, which in the combined window belonged to the next race and could suppress that race's results. Configs from 0.6.2 and earlier are converted from the old bit, so a result already sent isn't sent again.
- On back-to-back weekends, a schedule refresh from the Friday of race N found race N+1's FP1 exactly 7 days away and made it current, so race N's qualifying and race reminders were lost and the countdown showed the wrong race. `parseSchedule()` now advances only once race N's GP has finished (start + 2 h), and the cached schedule is re-parsed at that moment so the combined rotation starts straight away.
- After a boot on the saved fallback clock, the schedule is re-parsed once NTP syncs, so prev/current/next are picked against real time.

### Changed
- Web UI redesigned in a light style (paper background, white panels, F1-red accents). Three tabs: **Schedule**, **Settings** and **System / Diagnostics** (diagnostics, OTA, screenshot and debug level moved off the Config tab). A status bar shows connection, firmware and NTP state; the Schedule tab adds cards for the next session countdown, Grand Prix time and races left. The open tab is kept in the URL hash. No API changes.
- The web UI status bar can be dismissed (×); the choice is remembered in the browser, and the bar comes back by itself if the device is unreachable or NTP isn't synced.
- Removed the hardcoded `upload_port = /dev/cu.usbserial-240` from `platformio.ini`; PlatformIO auto-detects the port (override with `--upload-port`).
- Removed Universal-Arduino-Telegram-Bot from `lib_deps`; `telegram_handler.h` calls the Bot API directly.
- Web UI **Test Telegram**, **Resend Last** and the confirmation after saving new credentials are queued and sent from `loop()` instead of the web server task, which they could block for 8 s while racing an automatic notification on the same client. The endpoints return `202`; the page polls `GET /api/telegram/status` (`pending`, `lastResult`) for the outcome.
- `/config.json` is no longer rewritten every minute; it is saved only when notification state changes.
- `POST /api/config` no longer runs `initTime()` (up to 15 s waiting for NTP) inside the web server task on every save. A changed timezone or NTP server is applied by `loop()` with `applyTimezone()` (no network for zones in the built-in table), and session times are refreshed for all three loaded races, not just the current one.
- Removed the unused duplicate `fetchPostRaceData()`.

## [0.6.2] 10-10-2026

### Fixed
- Timezone still fell back to UTC on some boots in 0.6.1: all three ezTime lookups to `timezoned.rop.nl` timed out even after NTP. That server also serves stale or wrong rules (Mexico_City with DST, Cairo without, `America/Buenos_Aires` not found, `GMT-1` resolved as `Etc/GMT-14`, `GMT+1` as `Etc/GMT+12`). Every zone in the web UI dropdown now maps to a built-in POSIX rule (`TIMEZONE_POSIX` in `timezone_ntp_options.h`, tzdata 2025) applied with `setPosix()` – no network, no delay. `GMT+N` / `GMT-N` mean UTC+N / UTC-N, as labelled. Names typed into the captive portal that aren't in the table still use the online lookup, then UTC. Verified on hardware: "Timezone set: Europe/London (GMT0BST,M3.5.0/1,M10.5.0)".

### Changed
- `timezone_ntp_options.h`: the unused `TIMEZONE_OPTIONS` name list (with duplicate Toronto/Bangkok entries) replaced by the name → POSIX table.

## [0.6.1] 10-10-2026

### Fixed
- Timezone fell back to UTC on boot ("Invalid timezone 'Europe/London'"). ezTime resolves IANA names over UDP via `timezoned.rop.nl` with a 2 s timeout that includes DNS, and the lookup ran straight after WiFi joined. `initTime()` now syncs NTP first, then applies the timezone (`applyTimezone()`) with up to 3 attempts and logs ezTime's error reason. Verified on hardware: "Timezone set: Europe/London (attempt 1)", local session times in BST.

---

## [0.6.0] 10-10-2026

### Added
- Browser installer at https://anthonyjclarke.github.io/F1_CYD_Notifications/ (ESP Web Tools): install, **Update** that keeps WiFi and settings, and WiFi setup over USB.
- Improv-Serial always on (`src/network/improv_setup.*`, vendored `lib/ImprovWiFi` with the parser fix), served from `loop()` and from a now non-blocking WiFiManager portal.
- `Firmware` GitHub Actions workflow using the shared `cyd-web-installer` reusable workflow: every push builds; a `v*` tag on `main` publishes the release (`*-firmware.bin`, `*-merged.bin`, `SHA256SUMS.txt`) and the installer page.
- `tools/merge_bin.py` post-build script (`flash_parts.json`, `firmware-merged.bin`) and installer label on the `cyd` env.
- Boot log line `Running from app0|app1`.

### Fixed
- Vendored `lib/ImprovWiFi` re-copied from cyd-web-installer 1.0.1: each Improv packet now starts on a new line, so ESP Web Tools reliably parses the Connect reply and offers **Update** instead of **Install**.

### Changed
- `APP_VERSION` renamed to `FIRMWARE_VERSION`; added `PROJECT_NAME` (`F1_CYD_Notifications`, frozen), `PROJECT_REPO_URL` and `AP_NAME` (`WIFI_AP_NAME` now aliases it).
- Platform stays pinned at `espressif32@6.9.0`; partition table stays `min_spiffs.csv` (dual-OTA), so no erase is needed when updating from 0.5.x.
- Changelog dates switched to DD-MM-YYYY.

## [0.5.2] 25-08-2026

### Added
- Telegram Web UI verification controls:
  - `Test Telegram` sends a fresh test message using the saved bot token and chat ID.
  - `Resend Last` resends the last successfully delivered Telegram message.
- Telegram API endpoints:
  - `GET /api/telegram/status`
  - `POST /api/telegram/test`
  - `POST /api/telegram/resend`
- `/telegram_last.txt` LittleFS cache for the last successfully sent Telegram message, allowing `Resend Last` to survive reboot.
- Comprehensive Telegram setup and troubleshooting documentation for confirmation, test, and resend flows.

### Changed
- Web UI config save now detects Telegram token/chat changes, re-initializes the bot immediately, and attempts a confirmation message without requiring a reboot.
- Telegram is considered enabled only when both bot token and chat ID are non-empty.
- Config load derives Telegram enabled state from saved token/chat values to avoid stale `tgOn` values blocking valid credentials.
- Documentation updated across README, Telegram setup guide, functional spec, and project notes for v0.5.2.

### Fixed
- Lost/re-entered Telegram credentials can now be validated immediately from the Web UI instead of waiting for the next scheduled race notification.
- Changing Telegram credentials from the Web UI no longer leaves the running bot instance on the old token until reboot.

---

## [0.5.1] 16-03-2026

### Fixed
- **Wrong race displayed after overnight run** — root cause: ESP32 has no battery-backed RTC; if NTP sync fails on boot (e.g. router WiFi sleeping, brief outage), `parseSchedule()` resolved the current race against a bad clock (often epoch or a very old time), making a past race appear upcoming and showing the wrong location/date on the countdown screen. Reboot forced a fresh NTP sync and corrected it.

### Added
- **Persistent last-known-good time** (`/lasttime.json` on LittleFS): UTC epoch saved after every successful NTP sync and every 15 minutes while running. On boot, if `waitForSync()` times out, the saved epoch is applied via `setTime()` as a fallback — keeping race schedule parsing correct even without network. Guarded by `MIN_PLAUSIBLE_EPOCH` (2025-01-01) to reject corrupt or empty files.
- **WiFi reconnect detection** (`checkWiFiReconnect()`, polled every 30 s): detects loss of connectivity and calls `WiFi.reconnect()`; on reconnection immediately calls `resyncNTP()` to restore accurate time without waiting up to an hour for ezTime's automatic re-sync interval.
- **`resyncNTP()`** in `time_utils.h`: sets ezTime sync interval to 1 s, calls `events()` to dispatch the NTP query, then restores the 1-hour interval — safe to call at any point from the main loop.
- **Fallback warn log** in `getCountdownTarget()`: if no future sessions are found in the schedule or upcoming-races list (the "shouldn't happen" path), a `DBG_WARN` now logs `now()` to make NTP/schedule issues immediately visible in serial output.

---

## [0.5.0] 09-03-2026

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

## [0.4.0] 08-03-2026

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

## [0.3.0] 02-03-2026

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

## [0.2.0] 02-03-2026

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

## [0.1.0] 02-03-2026

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
