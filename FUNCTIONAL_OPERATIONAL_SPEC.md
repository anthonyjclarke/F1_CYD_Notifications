# F1 CYD Notifications: Functional and Operational Specification

## 1. Purpose and Scope
This document specifies the current implemented behavior of the F1 CYD Notifications firmware for ESP32-2432S028R (CYD), based on the code in this repository.

The device:
- Connects to Wi-Fi and syncs time via NTP.
- Fetches and caches Formula 1 schedule data.
- Renders race countdown/schedule/results on TFT display.
- Optionally sends Telegram notifications.
- Exposes a local web UI for configuration, status, schedule view, debug control, and OTA update.

## 2. Functional Specification

### 2.1 Boot and Initialization Flow
On boot, the firmware performs this sequence:
1. Initialize serial logging.
2. Initialize TFT.
3. Initialize screenshot subsystem (MicroSD + optional button trigger).
4. Render splash screen.
5. Initialize RGB status LED (active-low).
6. Mount LittleFS and load config from `/config.json` (or defaults if missing/invalid).
7. Apply configured brightness.
8. Connect Wi-Fi via WiFiManager captive portal (`F1-Display`, 180s timeout). Reboots on failure.
9. Sync NTP/timezone (`initTime`).
10. Fetch F1 schedule from network; fallback to cached schedule from `/races.json`.
11. Initialize Telegram client if enabled.
12. Start Async web server + ElegantOTA.
13. Initialize touch input, clear LED, trigger first display render.

### 2.2 Data Sources and Race Model
- Schedule source: `https://raw.githubusercontent.com/sportstimes/f1/main/_db/f1/2026.json`
- Results/standings source: `http://api.jolpi.ca/ergast/f1/2026`

Schedule parse behavior:
- Loads race fields: name, location, round, slug, sessions.
- Finds current race anchor as first race where `gp + 7 days > now`.
- Stores 3 races in RAM: previous/current-next/next-after.
- Builds compact upcoming race list (up to 25 rounds) for web season table.

Session modeling:
- Supports FP1/FP2/FP3, Sprint Qualifying, Sprint, Qualifying, Race.
- Sprint weekends include: FP1, Sprint Qualifying, Sprint, Qualifying, Race.
- Standard weekends include: FP1, FP2, FP3, Qualifying, Race.

### 2.3 Display Behavior
Display phases:
- `IDLE`: outside race week, shows countdown to first session.
- `RACE_WEEK_*`: starts when first session is within 7 days.
- `POST_RACE_*`: starts after GP time and lasts up to 7 days.

Display states:
- Countdown
- Schedule table
- Track layout
- Race result podium
- Driver standings
- Constructor standings

Rotation behavior:
- Race week states rotate every 8s.
- Post-race states rotate every 10s.
- Countdown digits refresh every 1s with partial redraw to reduce flicker.
- Touch press manually advances to next state in current phase.

Track layouts:
- Track image lookup currently returns `nullptr`; UI shows “Track image not available”.

### 2.4 Brightness Control
- Brightness range: `0..255`.
- `0` = auto mode using LDR (`analogRead(PIN_LDR)` mapped to 20..255).
- Non-zero = fixed PWM brightness.
- Brightness reapplied every 10 seconds and immediately on web config update.

### 2.5 Telegram Notification Logic
Telegram is active only if:
- `telegramEnabled == true`
- bot token configured and bot initialized
- chat ID configured

Notification types:
- Race-week message (Monday during race-week window).
- Pre-session messages (1 hour before): Qualifying, Sprint Qualifying, Sprint, Race.
- Results message after post-race data becomes available.

Deduplication/state:
- Per-round bitmask (`notificationBits`) prevents duplicates.
- Bitmask resets when round changes (`lastNotifiedRound` mismatch).
- Notification bits are persisted to `/config.json`.

### 2.6 Post-Race Data Polling
- Polling starts at GP + 3 hours.
- Poll interval: every 30 minutes.
- Polling stops when:
  - all post-race data fetches succeed, or
  - GP + 24 hours is exceeded.

Fetched post-race datasets:
- Podium top 3 for race round.
- Driver standings top 10.
- Constructor standings top 10.

### 2.7 Web UI and HTTP API
Server:
- Async web server on port `80`.
- Root UI is embedded in firmware (`INDEX_HTML` in PROGMEM).
- ElegantOTA mounted at `/update`.

Endpoints:
- `GET /` web UI.
- `GET /logo.raw` RGB565 F1 logo bytes.
- `GET /api/config` current config JSON.
- `POST /api/config` update timezone, NTP, Telegram credentials, brightness; saves config; reapplies time/brightness.
- `GET /api/status` heap, uptime, IP.
- `GET /api/schedule` current race sessions with local day/time and UTC.
- `GET /api/races` compact upcoming season list.
- `GET /api/debug` get runtime debug level.
- `POST /api/debug` set runtime debug level (0-4).
- `POST /api/screenshot` queue screenshot capture.

### 2.8 Screenshot Capture
- Saves TFT captures as 24-bit BMP files to SD folder `/shots`.
- Trigger sources:
  - Web endpoint (`POST /api/screenshot`)
  - Optional physical button (`PIN_SHOT_BTN`, active LOW, debounce)
  - Optional startup capture points controlled by `SCREENSHOT_STARTUP_CAPTURES`
- Naming behavior:
  - Time synced: `shot_YYYYMMDD_HHMMSS.bmp` (user timezone)
  - Time unsynced: `shot_unsynced_XXXXXX.bmp`

## 3. Operational Specification

### 3.1 Hardware/Platform
- Target board: ESP32 dev profile for CYD ESP32-2432S028R.
- Display: ILI9341 320x240 landscape.
- Touch: TFT_eSPI touch interface using configured calibration.
- LED: onboard RGB, active-low.
- Storage: LittleFS (`min_spiffs.csv` partition).

### 3.2 Software Stack
- Framework: Arduino (PlatformIO).
- Key libraries: TFT_eSPI, ArduinoJson, WiFiManager, UniversalTelegramBot, ESPAsyncWebServer, AsyncTCP, ElegantOTA, ezTime.

### 3.3 Runtime Cadence and Timing
- NTP maintenance: `events()` each loop; ezTime resync every 1h.
- Notification check: every 60s.
- Schedule refresh: every 24h.
- Results polling: every 30m during active polling window.
- Brightness check: every 10s.
- Display countdown refresh: every 1s.

### 3.4 Persistence Model
Files:
- `/config.json`: user configuration + notification state.
- `/races.json`: cached schedule payload.
- `/results.json`: helper functions exist; currently not used by active flows.

Config keys in `/config.json`:
- `tz`, `ntp`, `bot`, `chat`, `bright`, `notRound`, `notBits`, `tgOn`.

Defaults when config missing/invalid:
- Timezone `Europe/London`
- NTP `pool.ntp.org`
- Brightness `128`
- Telegram disabled

### 3.5 Failure and Recovery Behavior
- Wi-Fi connect failure after portal timeout -> device restart.
- NTP sync failure -> continues without synced time (warnings logged).
- Schedule fetch failure -> fallback to cached schedule; if cache unavailable, continues with warning/status message.
- Post-race data partial failure -> marked incomplete and retried in next polling interval.
- Telegram send failure -> no bit set for that message; can retry on next check if condition still true.

### 3.6 Observability and Control
- Serial debug with runtime levels 0..4.
- Debug level adjustable via web API without reboot.
- Logs include module prefixes (`[Main]`, `[F1]`, `[Web]`, `[Telegram]`, etc).

### 3.7 Security/Trust Characteristics (Current Implementation)
- GitHub schedule fetch uses HTTPS with certificate verification disabled (`setInsecure()`).
- Telegram client uses HTTPS with certificate verification disabled (`setInsecure()`).
- Jolpica API calls are HTTP (plaintext).
- Web UI and APIs are unauthenticated on local network.
- OTA update endpoint is exposed through ElegantOTA default integration.

### 3.8 Known Functional Gaps
- Track images are not yet implemented (`getTrackImage` currently returns no data).
- Season year is hardcoded to 2026 in schedule/results URLs.
- WiFiManager captures timezone/bot/chat only; NTP server and brightness are adjusted via web UI/config file.

## 4. Acceptance Checklist (As Implemented)
- Device can boot to operational state with or without existing config.
- Device shows countdown/schedule/results screens according to race phase.
- Device refreshes schedule daily and retries results after race.
- Device persists user config and notification bitmask.
- Web UI can read/update config and show live status/schedule data.
- Debug level can be changed at runtime.
- OTA endpoint is reachable on local network.
