# F1 CYD Notifications – Code Analysis

## Scope and Assessment

Reviewed on 1 November 2026 against the local `0.10.0-dev` working tree. The review covered first-party firmware, embedded web UI, vendored Improv source, configuration, asset generation, build/release integration and operational documentation. Generated binaries and third-party PlatformIO libraries were not exhaustively audited. No live schedule API, Telegram account, device firmware or deployment was changed.

The firmware has clear feature boundaries, fixed-size race storage, useful debug logging and careful notification/result ownership. Its main structural constraint is that implementation and global state largely live in headers. The scheduler uses `millis()` timers, but several jobs execute synchronous network/filesystem work once selected. Responsiveness therefore depends on those operations finishing quickly.

The most urgent remaining findings are missing serial parser bounds checks and unauthenticated local management access. Schedule validation/cache integrity and shared-buffer synchronization follow. The findings below are source-level observations; they are not claims of exploitation or a complete security audit.

The phased roadmap in [Implementation Plan](IMPLEMENTATION_PLAN.md) maps every finding to implementation tasks, dependencies, acceptance checks and release milestones. Its tasks remain planned until implementation and verification evidence are recorded.

---

## Display Changes Completed

### Live-session animation

The static 100 × 40 car is now sampled at 70 × 28 pixels, accelerates left to right and leaves six expanding gray smoke puffs. Each visit plays one 4.2-second pass, including time for the smoke to leave, then advances to event details. Touch still advances immediately. The animation repeats on the next visit while the session remains active.

`include/race_animation.h` renders a narrow strip at up to 25 frames per second with a 640-byte scanline buffer. It does not allocate heap memory or add delays. Each composed scanline replaces the old pixels directly. Actual SPI tearing, color fidelity and frame rate still require hardware verification.

### Timing and transition corrections

`determinePhase()` previously entered post-race at GP start, hiding the live GP countdown. Display phase selection, web schedule classification, post-race expiry tracking and parse diagnostics now use the nominal GP finish for entry. The existing three-day expiry remains measured from GP start.

Running sessions are checked independently of their local start date so they remain live across midnight. Screen and phase changes also clear partial-redraw state and cancel animation, preventing a digit-only redraw or a delayed animation completion from affecting the next screen.

---

## Outstanding Findings

### High – Improv parser can exceed its buffers

`lib/ImprovWiFi/src/ImprovWiFiLibrary.cpp`, in `handleSerial()` and `parseImprovSerial()`, accepts the incoming payload length without comparing the resulting frame size to the 128-byte `_buffer`. Subsequent writes can exceed that array. `parseImprovData()` also reads command/length and credential fields without first proving that those offsets fit the payload. A malformed USB serial frame can corrupt memory or access bytes outside the received payload.

Bound the frame before storing bytes, require the minimum RPC length, and validate SSID/password lengths before indexing or constructing strings. Add malformed/truncated-frame tests while preserving the existing installer parser patches. This remains unfixed in the display change.

### High – Local settings and OTA have no authentication

`include/web_server.h`, in `setupWebServer()`, returns the bot token through unauthenticated `GET /api/config`, accepts settings/Telegram actions without authentication, and calls `ElegantOTA.begin(&server)` without credentials. A client that can reach the device can read its bot token, alter configuration, trigger messages or access firmware update endpoints. The captive portal is also configured with an empty AP password.

Define and implement the intended local-management trust model, protect secrets and update routes, and add request-origin/CSRF protection where applicable. This is an existing deployment characteristic, not a newly introduced animation behavior.

### Medium – HTTPS peer verification is disabled

Device requests in `include/f1_data.h` and `include/telegram_handler.h` use `WiFiClientSecure::setInsecure()`. HTTPS encrypts the connection but does not establish that the peer is the intended schedule/results/Telegram service. An intercepting peer could supply false data or receive the bot token. Use an appropriate maintained trust store or root certificates with a time/bootstrap strategy.

### Medium – Schedule cache is replaced before validation

`fetchSchedule()` in `include/f1_data.h` writes a successful HTTP response to `/races.json` before calling `parseSchedule()`. A malformed HTTP 200 response can overwrite a good offline cache. Parsing verifies the JSON array but does not fully validate session timestamps: `parseISO8601()` checks length and then uses `atoi()`, and `addSession()` accepts a zero/invalid parsed epoch. Missing GP fields or malformed dates can therefore produce an invalid race anchor or display.

Validate schema, calendar fields and required session ordering into temporary race data, then commit memory and cache atomically after success. Keep the last valid cache if download or validation fails.

### Medium – Synchronous tasks can pause the animation and installer

`checkNotifications()`, `handleTelegramRequests()`, `checkResultsPolling()` and `checkScheduleRefresh()` run synchronous HTTPS requests from `src/main.cpp`. Results fetches are sequential, with a ten-second timeout for each request, and the schedule timeout is fifteen seconds. Improv Wi-Fi connection attempts can also block for fifteen seconds. These calls prevent the main loop from servicing animation, touch and serial requests during that time.

The new animation itself is cooperative and resumes from elapsed time, so a stall can skip frames or finish the pass immediately when control returns. Move slow jobs to bounded steps or an appropriate worker/queue design if uninterrupted animation or serial responsiveness is required. HTTP/socket behavior and watchdog handling need device tests.

### Medium – Screenshot buffer and shared state lack lifetime protection

`captureScreenshotToRam()` in `include/screenshot_capture.h` frees the previous screenshot while the asynchronous download filler in `include/web_server.h` obtains and copies the global buffer. A recapture overlapping a download can race with that copy, potentially accessing freed memory. Request counters and configuration structures are also shared across AsyncTCP and the main loop without a consistent lock/snapshot policy; `volatile` does not make those compound operations safe.

Give downloads ownership of an immutable snapshot until completion, reject recapture while a buffer is in use, or synchronize buffer replacement and readers. Apply a similar explicit ownership policy to configuration and queues.

### Medium – Expired or absent schedules can look like race week

`daysUntilLocalDate()` in `include/time_utils.h` clamps past dates to zero, while `determinePhase()` uses that value to select race week. After the season ends, `parseSchedule()` retains the final race, so after its post-race window expires the display can return to race-week mode for an expired event. A boot with no schedule likewise lacks a durable no-data display state; a temporary status message is later replaced by normal rendering of empty race data.

Add explicit valid-schedule and season-complete states, and use a signed date delta where phase selection needs to distinguish past from future. Keep countdown formatting's nonnegative behavior separate.

### Medium – Persistence failures are often reported as success

`saveConfig()` truncates `/config.json` in place, and most callers ignore its return value. `POST /api/config` returns `ok: true` even if saving failed. Notification state is marked in memory after a successful send, but a failed save can allow a duplicate after reboot. Schedule and saved-time writes have similar interruption risks.

Use a checked temporary write and rename for critical files, report failures to the caller/UI, and retain the prior valid file. Do not automatically retry an ambiguous Telegram transport failure as though delivery were known to have failed.

### Low – Held touch advances repeatedly

`checkTouch()` in `src/main.cpp` invokes manual advance on every loop while the controller reports a press. A held finger can skip several screens, including immediately bypassing the new animation. Track press/release edges and debounce/calibrate on the physical panel.

### Low – Web schedule status becomes stale without refresh

The embedded UI polls `/api/status` and updates countdown text, but fetches schedule/races/standings only at page load or explicit Refresh. It can retain an old race across automatic firmware schedule changes. The browser also classifies a session as past at its start and may show all sessions as done during the GP, even though the TFT uses a nominal live-session window. Poll schedule ownership changes or periodically reload relevant data, and apply session durations consistently if live browser status is desired.

### Low – Fractional standings points are truncated

`StandingEntry::points` is `uint16_t`, and both device standings fetchers cast API floating-point points to that type. Fractional points would be lost on the TFT, while the browser displays the original API value. Store a representation that preserves fractional values and format it deliberately.

### Low – Tooling and documentation drift

`tools/generate_f1_car.py` hardcodes a different user's absolute checkout path and imports Pillow, which is absent from the system Python used for this review. It should resolve paths from `__file__` and document its optional dependency. The firmware builds without running that generator.

`CLAUDE.md`, `README.md` and portions of `FUNCTIONAL_SPEC.md` describe older versions or removed behavior, including bot initialization, the unused results cache and an old upcoming-only calendar model. Dependency ranges and the shared workflow's `@main` reference also mean a later build can resolve different code. Keep released versus working behavior explicit and consider immutable release tooling references.

---

## Validation and Limits

`pio run -e cyd` succeeds with the installed toolchain. The build resolves TFT_eSPI `2.5.43`, ArduinoJson `7.4.3`, WiFiManager `2.0.17`, ESPAsyncWebServer `3.12.0`, AsyncTCP `3.5.0`, ElegantOTA `3.1.7` and ezTime `0.8.3`. This is a local development build, not a published release.

`python3 tools/preview_race_animation.py` produces a replayable browser preview and a static contact sheet using pixels exported by the host renderer. The contact sheet was visually inspected for the smaller car, smoke trail, clipping and road alignment; browser typography is an approximation.

`python3 tests/host/run_tests.py` passes with AddressSanitizer and UndefinedBehaviorSanitizer. It exercises actual display/time headers with deterministic clock and hardware/network stubs: GP entry/end, a session crossing local midnight, countdown and full redraws, exact animation completion, touch cancellation, phase cancellation, ordinary timed rotation, live entry near a rotation deadline, clipping, smoke/car composition, pixel cleanup, frame cadence, byte-swap restoration and 32-bit millisecond rollover.

Physical TFT smoothness, SPI color transfer, calibrated touch, live network concurrency, notifications and OTA were not tested. No firmware was flashed. The outstanding findings above remain review recommendations except for the display timing/transition corrections explicitly described as completed.

---

## Optional Visual Directions

A short stationary burnout before acceleration would make the launch more noticeable. A checkered-flag wipe could bridge the exit to event details. A setting for animation speed or enabling/disabling the effect would let users choose between motion and a quieter display. The current implementation keeps the existing charcoal/red livery and uses one pass per live-countdown visit.
