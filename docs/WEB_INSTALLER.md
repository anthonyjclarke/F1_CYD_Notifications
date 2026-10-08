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

## Hardware test matrix

| #   | Case                               | Expect                             | Status |
|:----|:-----------------------------------|:-----------------------------------|:-------|
| 1   | Fresh install, erased              | Install + erase; Improv WiFi       | –      |
| 2   | Update on provisioned board        | "Update"; settings kept            | –      |
| 3   | Board on `app1` (ElegantOTA)       | Update boots `app0`, settings kept | –      |
| 5   | `*-firmware.bin` via `/update`     | Updates normally                   | –      |
| 6   | macOS Chrome                       | Port found, flash completes        | –      |
| 7   | Windows Edge                       | Optional – needs the user          | –      |

Case 4 (wrong board) does not apply: there is only one env.
