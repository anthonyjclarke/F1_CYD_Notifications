# F1 CYD Notifications – Agent Guide

## Project and Source of Truth

This is Arduino C++ firmware for the ESP32-2432S028R Cheap Yellow Display. It displays the 2026 F1 schedule, session countdowns and post-race results, sends optional Telegram notifications, and serves a local configuration UI with ElegantOTA.

Use `include/config.h` for firmware identity and behavior constants. The reviewed working version is `0.10.0-dev`. Older prose and the README badge may describe the latest release instead. Review `docs/CODE_ANALYSIS.md` for outstanding findings; do not assume existing behavior is correct merely because a comment describes it.

Read applicable instructions and inspect `git status --short` before editing. Preserve unrelated user changes. This repository has no general-purpose application runtime or package manager beyond PlatformIO and the optional Python tools.

---

## Build and Verification

Build with `pio run -e cyd`. If `pio` is absent from `PATH`, use the installed PlatformIO executable, commonly `~/.platformio/penv/bin/platformio`. The environment is `cyd`, using `espressif32@6.9.0`, `esp32dev` and the Arduino framework. Dependency ranges in `platformio.ini` can resolve newer compatible versions; record the actual dependency graph when diagnosing a build.

Run the deterministic display regression checks with `python3 tests/host/run_tests.py`. These require Python 3 and a C++11 compiler with AddressSanitizer and UndefinedBehaviorSanitizer. The runner stages unchanged production display/time headers beside hardware and network stubs in a temporary directory. It does not test ESP32 drivers, live APIs, actual font rendering, Wi-Fi, OTA or physical touch.

Run `python3 tools/preview_race_animation.py` for a browser preview in the system temporary directory. Its animation strip uses actual host-rendered pixels; surrounding browser text only approximates the TFT layout. The preview also creates a contact sheet for visual inspection.

For display changes, build firmware, run the relevant host checks, and describe remaining physical checks. Check animation smoothness, colors, text placement, touch interruption, session boundaries and normal rotation on the CYD. Do not claim hardware validation from a successful compile or host framebuffer test. Uploads, release publishing and erases require task authorization; a local build alone is not an upload request.

---

## Code Map

`src/main.cpp` owns setup and loop orchestration, touch, LEDs, refresh timers, Wi-Fi recovery and results polling. `src/network/improv_setup.cpp` integrates the vendored Improv serial protocol with WiFiManager.

`include/display_states.h` owns phase selection, session targeting, automatic rotation and manual advancement. `include/display_renderer.h` owns the TFT instance, layouts, countdown partial updates and brightness. `include/race_animation.h` owns the live-session car and smoke animation.

`include/f1_data.h` parses and fetches schedule/results data. `include/time_utils.h` owns UTC time, timezone conversion, countdowns and NTP fallback. `include/timezone_ntp_options.h` supplies the local POSIX timezone rules.

`include/config_manager.h` owns LittleFS configuration/cache access. `include/telegram_handler.h` formats and sends messages and maintains notification deduplication. `include/wifi_setup.h` owns captive portal setup.

`include/web_server.h` contains both HTTP handlers and the complete embedded HTML/CSS/JavaScript UI. Edit its inline About content there. `archive/web_about.h` and `archive/track_images.h` are unused historical scaffolding.

`include/screenshot_capture.h` queues and executes SD/RAM screenshots. `include/f1_logo.h` and `include/f1_race_car.h` are RGB565 assets in flash. `tools/generate_f1_car.py` is the historical Pillow generator; its output paths currently need repair before use on another machine.

`tools/merge_bin.py` creates flash metadata and a merged image after builds. `.github/workflows/firmware.yml` delegates builds/releases to the shared installer workflow. Treat `.pio/` and `_site/` as generated output, not firmware source.

Most modules define functions and static storage in headers included by `src/main.cpp`. Avoid including these implementation headers in additional translation units: that can duplicate state or cause link errors. Keep standalone `.cpp` modules explicitly separated, as Improv currently is.

---

## Hardware and Rendering

The ILI9341 display is 320 × 240 in landscape, with rotation `1`. TFT and XPT2046 touch SPI pins are build flags in `platformio.ini`; other pins are in `include/config.h`. Backlight is GPIO `21`, RGB LEDs are active LOW on `4`, `16`, `17`, and the LDR uses `34`. SD uses a separate SPI instance with CS `5`, MOSI `23`, MISO `19`, SCK `18`. The optional screenshot button is GPIO `27` and disabled by default.

Keep TFT operations in the main loop/setup context. Async HTTP handlers should queue display work. Avoid repeated full-screen erases for changing digits or animation frames. State/phase transitions must clear `displayPartialUpdate` so the next renderer receives a full redraw.

The logo belongs on a white card. The car asset has a black background; do not rely on the ESP32 SPI transparent-image overload. Save and restore TFT byte-swap state when rendering native RGB565 buffers.

The live countdown displays a 70 × 28 car moving left to right with trailing gray smoke. `ON_NOW_ANIMATION_MS` controls its full pass (`4200` ms); `ON_NOW_FRAME_MS` controls frame cadence (`40` ms). It composes a 320 × 56 strip with one 640-byte scanline buffer, without allocating a sprite or delaying the loop. Redraws do not restart it. Completion advances once to the next state; touch or a phase change cancels it. Regular countdowns keep their existing one-second updates and standard screen rotation.

---

## Time, Phases and Race Ownership

Store session timestamps in UTC. Use `nowUTC()` as the canonical clock and `myTZ.tzTime(value, UTC_TIME)` for local display conversion. Use local civil dates for calendar-day countdowns and epoch differences for durations. Check running sessions before imposing same-local-date rules so a session crossing midnight stays live.

Race week rotates countdown → event details → schedule every eight seconds, except the live animation owns its own duration. Available previous-race results append winner → drivers → constructors. Pure post-race rotation is winner → drivers → constructors → next-race countdown every ten seconds. Post-race starts at the nominal GP end (`gpFinishedUtc()`), while its expiry remains three days from GP start. Session windows are estimates, not live telemetry.

Race slots are `races[0]` previous, `races[1]` current, and `races[2]` next. Parsing may advance to a nearby next race only after the current GP has finished. Preserve that constraint to avoid losing reminders on consecutive race weekends. `seasonRaces[]` is a compact calendar, not a complete session list.

Results belong to `resultsRound`. Check `hasResultsFor(race)` rather than checking `podiumCount` alone. Keep standings fetch/render limits aligned with `STANDINGS_TOP_N`. The browser fetches its full standings and historical winners directly from Jolpica; avoid extra device requests for those UI panels.

The supported timezone dropdown and POSIX rule table must stay in sync. Prefer built-in `setPosix()` rules; unknown names use the online fallback. `GMT+N` and `GMT-N` mean their displayed UTC offsets, not inverted `Etc/GMT` signs.

A saved fallback clock does not prove NTP sync. Preserve `ntpHasSynced()` and its fallback stamp check; notifications and periodic time persistence wait for a real sync. Re-parse the schedule after NTP recovery from fallback time.

---

## Network, Persistence and Concurrency

Use rollover-safe unsigned `millis()` subtraction for periodic work. Do not add animation loops, sleeps or delays to `loop()`. Service Improv frequently, with roughly one-second responsiveness as the target. Existing synchronous HTTPS operations can still stall the loop; do not describe all runtime work as non-blocking.

Telegram HTTP sends belong in `loop()`, including web Test, Resend and credential confirmation requests. Keep one POST per attempt; only a rejected Markdown request (`400`) gets the plain-text retry. Do not reintroduce the old bot library retry behavior.

Notification bits are tied to `notifiedGpUtc` and reset only for a later race. Results notifications use their separate `resultsNotifiedGpUtc`. Preserve migration behavior for older saved configs, and persist successful sends promptly. Never log bot-token URLs, passwords or configuration secrets.

LittleFS holds `/config.json`, `/races.json`, `/lasttime.json` and `/telegram_last.txt`. `/results.json` helpers currently have no active callers. Wi-Fi credentials are in NVS. Avoid destructive filesystem formatting/partition changes as a routine troubleshooting step.

AsyncTCP handlers and `loop()` can execute concurrently. `volatile` alone does not provide task synchronization or safe lifetime management. Queue bounded requests or use explicit synchronization for shared configuration, screenshot buffers and filesystem operations when changing those paths.

---

## Installer and Releases

Keep `PROJECT_NAME` and `board_build.partitions = min_spiffs.csv` unchanged unless an explicitly planned migration requires otherwise. They affect Update recognition and installed data compatibility. Keep Improv vendored in `lib/ImprovWiFi`; do not also add it to `lib_deps`.

Follow `docs/WEB_INSTALLER.md`. Publish release images through CI using a `v*` tag on `main`. Do not publish local `_site/` contents or local builds as release artifacts. Web OTA accepts the application `firmware.bin`, not the merged image.

Never use `firmware-merged.bin` in an update manifest. The merged image fills the NVS gap and erases Wi-Fi/settings stored there. Installer manifests use separate parts to preserve NVS and LittleFS. Distinguish those storage areas when explaining erase behavior.

---

## Documentation Formatting

For Markdown and plain-text project files, use spaces, never tabs, with four spaces per indentation level. Use en-dashes for prose dashes and ranges; retain literal hyphens in paths, code and Markdown syntax.

Markdown uses `#` for titles, `##` for sections and `###` for subsections, without skipping levels. Separate major sections with `---`. Plain text uses a consistent title/section/subsection hierarchy. Enclose paths, field names and constants in backticks.

Prefer prose for explanations of three items or fewer. If using pipe tables, align pipes in monospace and keep each cell to 45 characters or fewer; use prose if that would lose meaning. Update behavior documentation and `CHANGELOG.md` for user-visible changes, keeping build/hardware validation claims precise.
