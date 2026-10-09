# Web installer – conversion record

F1_CYD_Notifications adopted the shared
[cyd-web-installer](https://github.com/anthonyjclarke/cyd-web-installer)
tooling in 0.6.0. The design and its rules live there and in the pilot,
CYD_AnimatedPixelClock (`docs/WEB_INSTALLER_PLAN.md`). This file records what
is specific to this project and the hardware test results.

---

## Project specifics

| Item                | Value                                      |
|:--------------------|:-------------------------------------------|
| Installer env       | `cyd` – CYD 2.8″ ESP32-2432S028R           |
| Platform            | `espressif32@6.9.0` (kept)                 |
| Partitions          | `min_spiffs.csv` – dual-OTA, 1.9 MB slots  |
| App size (0.6.0)    | 1,394,560 B of 1,966,080 B (70.6%)         |
| `PROJECT_NAME`      | `F1_CYD_Notifications` – frozen            |
| Setup hotspot       | `F1-Display` (WiFiManager)                 |
| Web OTA             | ElegantOTA at `/update`                    |
| Settings            | LittleFS `/config.json`, made at boot      |
| FS image            | None – Update leaves LittleFS untouched    |

Settings, including the Telegram bot token, live in LittleFS, which the four
installer parts never touch. WiFi credentials live in NVS, which an Update
also leaves alone. A clean install with the erase question answered yes
wipes both; LittleFS formats itself on the next boot.

Improv is ticked at the top of `loop()` and from the non-blocking WiFiManager
portal loop. `setup()` is not ticked after `improvBegin()`: on a provisioned
board the WiFi join, NTP wait (up to 15 s) and schedule fetch run first, so
ESP Web Tools' post-install Improv probe can miss on a provisioned board.
Connect on a board that is already running always answers.

---

## Smoke test (RUNBOOK 5a)

One board, CI preview image, macOS Chrome.

| Check                                      | Result | Date | Board MAC |
|:-------------------------------------------|:-------|:-----|:----------|
| CI green for every env (`cyd`)             | Pass   | 10-10-2026 | –                 |
| Erase, fresh install, yes to erase         | Pass   | 10-10-2026 | b0:cb:d8:da:ae:8c |
| Configure WiFi (Improv), joins WiFi        | Pass   | 10-10-2026 | b0:cb:d8:da:ae:8c |
| Boot log `Running from app0`, no crash     | Pass   | 10-10-2026 | b0:cb:d8:da:ae:8c |
| Connect again: "Connected to" name + ver   | Pass   | 10-10-2026 | b0:cb:d8:da:ae:8c |

Board: ESP32-D0WD-V3 2.8″ CYD, CI image from Firmware run 37964304664
(`06285f1`). Connect showed "Connected to F1-CBB0 – F1_CYD_Notifications
0.6.0-dev (ESP32)". After a reset the log showed `Running from app0`, WiFi
joined, NTP synced, schedule fetched, setup completed with 213,976 B free
heap, and the display rotation ran with no crash.

---

## Tests owed

Smoke-tested only. Run these on the next real work on this project, or before
the next release, and tick them off with date and board MAC. This is the first
release with the installer on an ElegantOTA project, so case 3 matters.

- [x] Case 2 – Update on a provisioned board (settings kept) – pass
  10-10-2026, b0:cb:d8:da:ae:8c: live page updated 0.6.0-dev → 0.6.0 with no
  erase; boots `app0`, `/config.json` loaded and the saved WiFi rejoined.
- [x] Case 3 – Update from `app1` after an ElegantOTA `/update` (settings kept) – pass
  10-10-2026, b0:cb:d8:da:ae:8c: board on v0.6.0 in `app1`; live page offered
  Update to v0.6.1 (in a fresh Incognito window – see note); boots `app0`,
  `/config.json` loaded, saved WiFi rejoined.
- [x] Case 5 – `*-firmware.bin` via the web UI's `/update` – pass
  10-10-2026, b0:cb:d8:da:ae:8c: v0.6.0 release
  `F1_CYD_Notifications-v0.6.0-cyd-firmware.bin` (SHA256 checked) uploaded to
  ElegantOTA; boots `Running from app1`, settings and WiFi kept.

All owed cases are cleared.

Note: GitHub Pages serves the manifest and parts with `max-age=600`. A browser
that opened the page before a release can keep the old manifest for up to
10 minutes, and then shows neither Install nor Update for a board on the old
version. A hard reload refreshes the page header but not the manifest that
ESP Web Tools fetches on Connect; an Incognito window (or waiting) does.

Case 1 has no remaining boards (one env), and case 4 does not apply.
