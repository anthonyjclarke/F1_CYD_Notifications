# CYD ESP32 F1 Notification Project Brief

## Hardware

### Main Board
- ESP32-2432S028 (2.8" CYD) as the default and starting point
- ESP32-2432S024 (2.4") as an alternate
- Configurable to other ESP32 boards with an attached user display

### Sensors
- None

### Other Hardware
- Possible future support for additional displays and/or sensors

## Features

- Model some features from [witnessmenow/F1-Arduino-Notifications](https://github.com/witnessmenow/F1-Arduino-Notifications), specifically Telegram notifications.
- Display the F1 race schedule during race week and send Telegram notifications.
- Race information source: [sportstimes/f1 2026.json](https://github.com/sportstimes/f1/blob/main/_db/f1/2026.json)
- Additional references:
  - [Formula 1 2026 season](https://www.formula1.com/en/racing/2026)
  - [Formula 1 calendar](https://calendar.formula1.com/)

Display concept (based on locally set timezone):

```text
┌───────────────────────────────────────┐
│           F1 RACE SCHEDULE            │
│      Sunday, 08 Mar 2026 - 15:00      │
├───────────────────────────────────────┤
│  Next Race: Australian                │
├─────────────────────┬────────┬────────┤
│  SESSION            │  DAY   │  TIME  │
├─────────────────────┼────────┼────────┤
│  Free Practice 1    │  Fri   │  12:30 │
│  Free Practice 2    │  Fri   │  16:00 │
│  Free Practice 3    │  Sat   │  12:30 │
│  Qualifying         │  Sat   │  16:00 │
│  Race               │  Sun   │  15:00 │
└─────────────────────┴────────┴────────┘
```

- In the week preceding a race, show a large day countdown to the first session, rotating between countdown and schedule views.
- Once race day is over, start countdown to the next race.

### Web Configuration Interface

Settings to expose:
- [X] WiFi credentials
- [X] Timezone / NTP server
- [X] Display brightness
- [X] Telegram API / Chat ID input

Interface style:
- Simple HTML / styled / minimal API-only

### OTA Updates

- Method preference: ArduinoOTA with optional custom web upload
- Password protected: No

### WiFi

- Initial setup: WiFiManager captive portal
- Fallback AP mode: Yes

## Preferences

### Libraries

- Display: TFT_eSPI
- Web server: no preference
- Config storage: no preference
- Other libraries to use: [Universal-Arduino-Telegram-Bot](https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot)
- Libraries to avoid: none

### Code Style

- Single `main.cpp` or split into modules: single file
- Configuration approach: `config.h` defines
- Include `.h` files in `include/`
- Images in `/images`
- Documentation in `/docs`

### Development

- PlatformIO: Yes
- Git repo structure: standard
- Target framework: Arduino

## Display Layout

- (Reserved)

## Anything Else

- For 7 days post-race, display:
  - Race winner
  - Individual leaderboard
  - Constructor leaderboard
- Display track layout
