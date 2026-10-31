# F1 CYD Notifications – Phased Implementation Plan

## Objective and Current Position

Address the findings in `docs/CODE_ANALYSIS.md`, preserve installed settings and notification history, and improve the live-session experience through small, independently reviewable changes.

The smaller car, smoke trail, automatic screen advancement, GP phase correction and midnight-session detection are implemented in the working tree. Firmware compilation and host display tests pass. Physical CYD validation is still outstanding. All work listed below is planned unless explicitly identified as existing functionality.

The recommended order is Phase 0 → Phase 1 → Phase 2 → Phase 3 → Phase 4 → Phase 5 → Phase 6 → Phase 7. Phase 6 is optional and can be omitted from a reliability release. Hardware checks occur throughout; Phase 7 consolidates release evidence rather than postponing validation until the end.

This document is an implementation roadmap. It does not authorize flashing, sending test messages, changing connected services or publishing releases.

---

## Phase 0 – Establish the Baseline and Compatibility Contract

**Priority:** prerequisite. **Effort:** small. **Dependency:** none.

### Work

- [ ] P0.1 – Record the working firmware version, resolved dependencies, application size, static RAM and hardware boot heap. Keep the existing animation changes separate from subsequent fixes in the review history.
- [ ] P0.2 – Create deterministic fixtures for standard and sprint weekends, consecutive race weekends, a midnight session, no schedule, malformed schedule, season completion and older configuration formats. Use synthetic credentials and notification history.
- [ ] P0.3 – Validate the current animation on a CYD: car size, smoke placement, colors, smoothness, screen advancement, touch interruption and normal rotation. Record the board, firmware build and observed limitations.
- [ ] P0.4 – Document the migration contract: preserve `PROJECT_NAME`, `min_spiffs.csv`, existing Wi-Fi credentials, timezone, brightness, Telegram credentials and notification deduplication fields. Keep a known-good application image for recovery.

### Acceptance and Deliverable

The project has a reproducible local baseline, fixture coverage for the important time boundaries, and a recorded hardware result for the existing animation. Compatibility expectations are explicit before configuration or threading changes begin. Deliver a baseline test record and fixtures; do not change the partition layout.

---

## Phase 1 – Close the Immediate Security Gaps

**Priority:** highest. **Effort:** medium to large. **Dependency:** Phase 0. Parser bounds checks should be the first implementation change.

### Work

- [ ] P1.1 – Harden `lib/ImprovWiFi/src/ImprovWiFiLibrary.cpp`: enforce frame capacity before writing, validate minimum RPC length and nested credential lengths, reject oversized/truncated packets, and recover cleanly for the next valid packet. Retain the existing installer compatibility patches.
- [ ] P1.2 – Define and implement local management access in `include/web_server.h` and `include/wifi_setup.h`. Recommended policy: public schedule viewing, authenticated settings/diagnostics/screenshots/Telegram actions/OTA, and per-device administrator credentials with a documented physical recovery path. Verify the installed web-server and ElegantOTA authentication capabilities before choosing their integration; do not invent a cryptographic protocol or ship shared default credentials.
- [ ] P1.3 – Stop returning the Telegram token from `GET /api/config`. Return a configured flag, preserve the saved token when the replacement field is blank, and provide a distinct explicit clear action. Validate settings lengths/ranges, reject oversized input, and add request-origin and CSRF protection appropriate to the selected authentication mechanism. Keep secrets out of logs, diagnostics and screenshots used for documentation.
- [ ] P1.4 – Replace `setInsecure()` with maintained certificate trust for device HTTPS requests. Define behavior when the clock is unconfirmed: use the existing cached schedule and defer requests whose certificates cannot be validated; never silently disable verification. Include a certificate update strategy and secure first-time provisioning/recovery flow, with any setup secret shown locally rather than shared across devices.

### Acceptance and Deliverable

Malformed serial frames pass sanitizer-backed tests without memory errors, and back-to-back valid installer requests still work. Unauthenticated management and update requests are denied, secret reads are removed, and invalid cross-origin state-changing requests are rejected. Existing devices have a tested way to establish administrator access without losing settings or being locked out.

On hardware, verify initial setup, Wi-Fi changes, authenticated web OTA, mDNS/IP access, valid HTTPS, an untrusted certificate and unavailable NTP. Document that HTTP authentication on a trusted LAN does not itself provide encrypted browser transport. Deliver parser hardening separately from management-access and certificate-trust changes so each can be reviewed and diagnosed independently.

---

## Phase 2 – Make Shared State and Persistence Safe

**Priority:** high. **Effort:** large. **Dependency:** Phase 1; the parser fix can ship independently before this phase.

### Work

- [ ] P2.1 – Give configuration, notification state and filesystem writes a single owner. Async handlers validate and enqueue bounded commands; the owner applies them and publishes read-only snapshots. Assign request IDs, explicit busy/error results and completion status. Remove reliance on `volatile` as synchronization.
- [ ] P2.2 – Protect screenshot lifetime in `include/screenshot_capture.h` and the download handlers. Recommended first implementation: keep one immutable capture while downloads use it and reject recapture with a clear busy response. Acquire/release ownership safely through completion, disconnect and error paths; do not hold a lock while streaming or duplicate the full image just to avoid synchronization.
- [ ] P2.3 – Introduce checked persistence in `include/config_manager.h`: write a temporary file, verify its contents, commit with a retained recovery copy, and recover incomplete operations on boot. Verify actual LittleFS replacement behavior under interrupted writes. Propagate write/commit failures to callers and web status instead of returning unconditional success.
- [ ] P2.4 – Define durable notification intent and outcome handling. Preserve `notGp`, `resGp` and legacy migration behavior. If delivery is ambiguous or a confirmed-send result cannot be persisted, retain an explicit uncertain state and avoid automatic resend until reconciled. If durable intent cannot be recorded, defer the automatic send and report the failure locally. Keep deliberate user Resend as a separate action.

### Acceptance and Deliverable

Concurrent settings requests, captures and downloads do not corrupt memory or expose partially updated configuration. Queue overflow and busy responses are predictable. Simulated write/open/rename failures preserve a recoverable configuration and appear as failures in the UI.

Reset/power-interruption tests cover each persistence boundary and notification outcome. Previously sent notifications remain suppressed after migration. Document the tradeoff for uncertain delivery: Telegram POSTs cannot provide an exactly-once guarantee across a lost reply and power failure. Deliver ownership/queue changes, screenshot lifetime protection and persistence/notification recovery as separate review units.

---

## Phase 3 – Correct Schedule, Time and Result Modeling

**Priority:** high. **Effort:** medium. **Dependency:** Phase 2 for ownership and checked cache commits.

### Work

- [ ] P3.1 – Parse incoming schedules into bounded temporary data. Validate required fields, supported session types, ISO date syntax/ranges, nonzero timestamps, ordering and race identifiers. Enforce a response-size budget. Publish new race data and replace `/races.json` only after validation; retain the last valid in-memory/cache data on failure.
- [ ] P3.2 – Add explicit no-schedule and season-complete states to `include/types.h`, `include/display_states.h` and `include/display_renderer.h`. Distinguish a valid but stale offline schedule from unavailable data. Retain automatic recovery when a valid refresh arrives.
- [ ] P3.3 – Separate signed local-date differences used for phase selection from nonnegative countdown formatting. Retain active-session detection across midnight, nominal GP end, the existing post-race expiry anchor, and the rule that the next weekend cannot take ownership before the current GP finishes. Verify NTP recovery, timezone/DST changes and consecutive weekends against fixtures.
- [ ] P3.4 – Preserve fractional championship points using an explicitly supported precision, such as fixed-point tenths if the API data contract permits it. Validate representable values rather than silently truncating. Keep round ownership checks for podium and standings, and align fetch limits with display capacity.

### Acceptance and Deliverable

Malformed or oversized HTTP 200 responses cannot replace a good schedule. With no usable schedule, the display remains informative instead of rendering a zero-valued race. The completed season has a stable final state, and a new valid season can recover without erasing settings.

Fixture tests cover GP start/end, post-race expiry, first-session boundaries, midnight, DST, fallback time recovery, sprint weekends and consecutive races. Fractional points survive parsing and fit the TFT columns. Deliver cache validation before introducing the new states, then complete time/result model changes.

---

## Phase 4 – Keep the Display and Installer Responsive

**Priority:** medium to high. **Effort:** large. **Dependency:** Phases 2 and 3.

### Work

- [ ] P4.1 – Measure main-loop service gaps and frame intervals without logging secrets or flooding serial output. Establish before/after results for normal operation, schedule refresh, results polling, Telegram requests, Wi-Fi loss and a slow/unreachable server.
- [ ] P4.2 – Move blocking HTTPS operations behind one bounded network worker and command/result queues. Snapshot inputs before dispatch and apply results through the state owner. Keep TFT access in its current context, serialize Telegram attempts, and preserve the single-POST/Markdown-rejection retry rules. Do not run one worker per service or overlap multiple large TLS allocations.
- [ ] P4.3 – Bound each job's connection/read duration, queue size, retry/backoff and memory use. Tag results with the requested race/configuration generation so late responses cannot overwrite newer state. Handle Wi-Fi/configuration changes and stale jobs without publishing partial results or triggering unintended message retries.
- [ ] P4.4 – Rework the Improv connection wait and Wi-Fi scan path so serial/display servicing continues while provisioning progresses. Preserve protocol reply order, failed-credential restoration and reboot behavior. Coordinate worker shutdown with OTA and keep screenshot capture pauses explicit and bounded.

### Acceptance and Deliverable

Slow or failed network requests no longer pause the animation for seconds. Proposed hardware targets are a normal animation service gap at the 95th percentile of at most 50 ms, no gap above 100 ms during the defined network stress workload, and an Improv device-info reply within one second. Confirm feasibility on the baseline board before treating those targets as release commitments; intentional capture/OTA transitions are measured separately.

Run long-duration hardware checks with repeated failures and reconnects. Verify no progressive heap loss, watchdog resets, queue growth, stale-race publication or duplicate automatic sends. Deliver measurements first, then migrate one network job at a time before changing provisioning behavior.

---

## Phase 5 – Align Touch, Browser Behavior and Maintenance Tools

**Priority:** medium. **Effort:** medium. **Dependency:** Phase 3 for state semantics and Phase 4 for responsive input; portable tooling cleanup can proceed earlier.

### Work

- [ ] P5.1 – Advance only on a debounced touch press edge, with release required before the next advance. Calibrate the physical panel and verify deliberate taps, held fingers, drag/noise and taps during animation.
- [ ] P5.2 – Publish a schedule generation/ownership marker and nominal session live status through the device API. Refresh browser schedule data when that marker changes or on a bounded poll. Rebuild active-session row/card states at boundaries, and recover from connection loss without requiring a page reload.
- [ ] P5.3 – Keep historical winners and full browser standings fetched directly from Jolpica. Refresh them on a sensible cadence or relevant round change, with bounded retries and visible stale/error states. Avoid turning one browser refresh into extra device HTTPS requests.
- [ ] P5.4 – Make `tools/generate_f1_car.py` resolve paths from its own location and document the optional Pillow dependency. Reconcile `README.md`, `CLAUDE.md`, `FUNCTIONAL_SPEC.md` and `AGENTS.md` with implemented behavior. Pin validated release dependencies and the shared workflow to reviewed versions/commits, and document how those pins are updated.

### Acceptance and Deliverable

One physical press advances one screen. An already-open browser follows session and race transitions automatically, including during a GP, after reconnect and at season completion. Settings remain usable with the Phase 1 secret-preservation behavior.

The asset generator runs from a different checkout path, normal firmware builds do not require Pillow, and release documentation distinguishes implemented behavior from planned work. Validate a build with the chosen pins and record the resolved dependency set. Deliver touch, browser consistency and maintenance cleanup separately.

---

## Phase 6 – Add Optional Animation Polish

**Priority:** optional. **Effort:** small to medium. **Dependency:** Phases 2, 4 and 5. Complete the reliability work before adding more motion.

### Recommended Sequence

Implement the enable/disable preference first, with the existing 4.2-second effect as the default. Disabled mode should show a small static car and use normal screen rotation. Add a bounded speed preference next; define its value as the total animation duration, including any later launch/exit effects, so screen timing remains predictable.

Try a short stationary burnout, approximately 300–500 ms, within that total duration. The car pauses while smoke grows, then accelerates across the screen. Keep touch interruption immediate and avoid a second animation timer that can advance the screen twice.

Treat the checkered-flag wipe as a separate opt-in experiment after the burnout has been reviewed on the panel. Keep it brief and limited to a transition region; retain the simple car exit if the wipe reduces readability or smoothness.

### Acceptance and Deliverable

Preferences persist safely, older configurations receive compatible defaults, and invalid duration values are rejected. Each selected effect completes once, advances once, cancels on touch/phase change, and survives millisecond rollover. Preview and hardware checks confirm that the car, smoke and text remain clear at every supported speed without increasing frame-buffer allocation.

Deliver preference controls, burnout and any retained flag wipe as separate changes. Each visual addition can be omitted without delaying the reliability release.

---

## Phase 7 – Release Qualification and Recovery

**Priority:** required before publishing. **Effort:** medium. **Dependency:** all selected phases; optional animation effects need not be selected.

### Work

- [ ] P7.1 – Run the final firmware build, host sanitizer regressions, parser malformed-input tests, schedule/state fixtures, persistence failure tests and browser checks. Record application size, static RAM, boot/minimum heap, largest free block and observed frame timing against Phase 0.
- [ ] P7.2 – Validate fresh provisioning and upgrade from a representative existing firmware/configuration. Verify Wi-Fi, administrator setup, Telegram settings, saved notification history, timezone, brightness and cached schedule survive the intended update path. Exercise administrator recovery and interrupted-write recovery without changing partitions.
- [ ] P7.3 – Perform hardware stress and soak checks covering slow HTTPS, certificate rejection, unavailable NTP, Wi-Fi reconnect, concurrent screenshot/download requests, touch, OTA from both application slots and a session/race boundary. Use synthetic timelines where waiting for a real weekend is impractical; distinguish simulated time from live-service validation.
- [ ] P7.4 – Update `CHANGELOG.md`, user setup/recovery instructions and `docs/WEB_INSTALLER.md` with evidence and any limitations. Build the release through the established CI/tag flow only when publishing is authorized. Retain the known-good image and verify the rollback path before release.

### Acceptance and Deliverable

All selected findings have linked implementation/test evidence, remaining limitations are explicit, settings migration and recovery are demonstrated, and the release fits the existing application slots. Do not claim a complete security audit or uninterrupted animation from host tests alone.

An older firmware image may not understand new configuration fields or authentication state. Document and test backward compatibility, or provide a recoverable backup/restore procedure before calling rollback safe. Recovery must not depend on publishing a merged image that erases NVS.

---

## Suggested Release Boundaries

Use an early hardening milestone for Phase 1 once its own upgrade/recovery and hardware checks pass. Keep the combined ownership, persistence and data corrections in a reliability milestone spanning Phases 2–3. Follow with responsiveness and browser/touch improvements from Phases 4–5. Treat Phase 6 as an optional visual milestone.

These are scope boundaries, not promised version numbers or dates. Apply the relevant Phase 7 checks to every published milestone. A large architectural change should not delay an independently validated parser fix.

---

## Finding Coverage and Completion Tracking

The serial parser, management access and HTTPS trust findings map to Phase 1. Screenshot lifetime/shared state and persistence map to Phase 2. Schedule validation, missing/expired data and fractional points map to Phase 3. Blocking runtime work maps to Phase 4. Held touch, stale browser state and tooling/documentation drift map to Phase 5. Burnout, flag wipe and animation preferences map to Phase 6.

For each task ID, record the implementation change, automated result, hardware evidence where applicable and any remaining limitation. Update `docs/CODE_ANALYSIS.md` only when a finding is actually resolved; a planned task or passing compile is not resolution evidence. Keep all checkboxes open until their acceptance conditions are met.
