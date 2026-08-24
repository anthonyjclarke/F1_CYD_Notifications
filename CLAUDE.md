# Project: F1 CYD Notifications

F1 race schedule display and Telegram notification system for ESP32-2432S028R. Fetches 2026 schedule from sportstimes GitHub JSON and post-race results from Jolpica API. Cycles through countdown, event details, and session schedule during race week; post-race screens (winner, standings) for 3 days after GP. Sends Telegram notifications for race week start, key sessions, and results. Current version 0.5.2.

## Hardware
- **MCU**: ESP32-2432S028R (2.8" Cheap Yellow Display)
- **Display**: ILI9341 TFT, 240×320 (landscape: 320×240)
- **Touch**: XPT2046 resistive touchscreen (tap to advance display)
- **Onboard RGB LED**: Active-LOW, GPIO 4/16/17 — boot status indicator
- **Onboard LDR**: GPIO 34 — ADC for auto-brightness (brightness=0 enables it)
- **Storage**: LittleFS internal flash + microSD (HSPI) for optional screenshots

## Pin Mapping
| Function        | GPIO | Notes                      |
|-----------------|------|---------------------------|
| Touch CS        | 33   | SPI @ 2.5 MHz              |
| Touch IRQ       | 36   | Input-only pin             |
| RGB LED Red     | 4    | Active LOW                 |
| RGB LED Green   | 16   | Active LOW                 |
| RGB LED Blue    | 17   | Active LOW                 |
| LDR             | 34   | ADC — brightness=0 auto    |
| SD CS           | 5    | HSPI @ 8 MHz               |
| SD MOSI         | 23   | HSPI                       |
| SD MISO         | 19   | HSPI                       |
| SD SCK          | 18   | HSPI                       |
| Screenshot btn  | 27   | Active LOW, INPUT_PULLUP   |

## Libraries
- TFT_eSPI 2.5.43 (configured via `build_flags` in platformio.ini; no User_Setup.h)
- ArduinoJson 7.4.0
- WiFiManager (git)
- Universal-Arduino-Telegram-Bot (git)
- ESPAsyncWebServer 3.6.2 / AsyncTCP 3.3.3
- ElegantOTA 3.1.6
- ezTime 0.8.3

## Configuration Files (LittleFS)
- `/config.json` — timezone, NTP server, Telegram token/chat ID, brightness (loaded on boot, editable via web UI)
- `/races.json` — cached schedule JSON (offline fallback)
- `/lasttime.json` — last-known-good UTC epoch (applied if NTP fails on boot; saved periodically and on sync)
- `/telegram_last.txt` — last successfully sent Telegram message for Web UI resend
- `/results.json` — defined but unused
- Notifications: per-round sent bitmask persisted (not re-sent across reboots)

## Architecture Notes
- **Display state machine** (`determinePhase()`): Maps time vs race timestamps to three modes: idle (single countdown), race-week (8s rotation: COUNTDOWN→EVENT_DETAILS→SCHEDULE; adds WINNER→DRIVERS→CONSTRUCTORS if results cached), post-race (10s rotation: WINNER→DRIVERS→CONSTRUCTORS→NEXT_RACE for 3 days). Respects `FORCE_POST_RACE_TEST_DISPLAYS` compile flag to bypass time logic.
- **Anti-flicker countdown**: `displayPartialUpdate` flag. Partial updates (1s ticks) erase only digit region; state transitions full redraw. `static int16_t _prevCountdownDays` and `static bool _prevOnNow` force redraws at day boundary or isOnNow transition. During live sessions, switches to "On Now" display (red header, F1 car PROGMEM image, countdown).
- **Session-aware countdown** (`CountdownTarget` struct + `getCountdownTarget(race)`): Walks sessions to find next/current target. If a session is running, returns it; if a session-day has a future session, returns that; else falls back to next race in `upcomingRaces[]`. Session durations: FP 60 min, Sprint Qualifying 30 min, Sprint 90 min, Qualifying 60 min, GP 120 min. `isOnNow = true` when now is within session window.
- **Persistent time / NTP fallback**: `initTime()` tries NTP first; saves epoch to `/lasttime.json` on success. On NTP timeout, loads saved epoch if `> MIN_PLAUSIBLE_EPOCH` (2025-01-01 UTC). Static `_ntpSyncedOnce` flag prevents re-saving stale times before real NTP sync. Time also saved every 15 min during runtime.
- **WiFi reconnect**: `checkWiFiReconnect()` polls `WiFi.status()` every 30s. On drop → `WiFi.reconnect()`. On reconnect → `resyncNTP()` (sets 1s interval, fires `events()`, restores 1h interval) without blocking loop.
- **Timezone-correct day counting** (`daysUntilLocalDate(targetUtc)`): Converts now and target to local civil dates using Gregorian day numbers, diffs to avoid DST/midnight issues. All `myTZ.tzTime()` calls use `UTC_TIME` flag. `nowUTC()` returns `::now()` (TimeLib epoch).
- **F1 logo** (118×64 RGB565 PROGMEM): Rendered on white card (fillRoundRect + pushImage); designed for white backgrounds only.
- **Season calendar**: `UpcomingRace` struct array (up to 25 races) with round, isSprint, name, location, gpTimeUtc. Populated from `nextIdx` (first race where gp + POST_RACE_DAYS*86400 > now) to season end. Served via `/api/races`.
- **Standings display**: `STANDINGS_TOP_N=8` controls fetch limit and renderer loop for driver/constructor standings. Both fetch functions pass `?limit=STANDINGS_TOP_N` to API.
- **Race pointers**: `getPrevRace()=races[0]`, `getCurrentRace()=races[1]`, `getNextRace()=races[2]`. `parseSchedule()` loads prev/current/next around `nextIdx`. Overlap check: if `nextIdx+1` race is within 7 days, advances `nextIdx` to enable combined rotation (results+race-week).
- **Post-race expiry check** (`checkPostRaceExpiry()`): Detects post-race→non-post-race transition, immediately calls `fetchSchedule()` to advance races[1] without waiting 24h.
- **Screenshot** (HSPI @ 8 MHz): `tft.readRect()` in 16-row chunks, BMP written bottom-up with byte-swap. Request-queue model; web/button queues, main loop executes. Filenames: `/shots/shot_YYYYMMDD_HHMMSS.bmp` (user TZ after sync) or `/shots/shot_unsynced_XXXXXX.bmp` before sync. Polls `/api/screenshot/status` (250 ms, 10s max) until `busy` clears.
- **PROGMEM image transparency gotcha**: TFT_eSPI `pushImage` with transparency key on ESP32 SPI is unreliable. For images on black background, set pixels to `0x0000`, call `pushImage` without transparency key, and `fillRect(COLOR_BG)` before to clear region.
- **Non-blocking timing**: All periodic tasks use `millis()` — no `delay()` in loop.
- **Notification deduplication**: Per-round bitmask persisted to LittleFS.
- **Telegram config validation**: Web UI `POST /api/config` detects token/chat changes, re-initializes the bot immediately, and sends a confirmation message when both fields are present. `POST /api/telegram/test` sends a fresh verification message; `POST /api/telegram/resend` resends `/telegram_last.txt`.

## Known Quirks
- Touch calibration untested (approximate: {300, 3600, 300, 3600, 7}).
- Results not cached across reboots (cacheResults() never called).
