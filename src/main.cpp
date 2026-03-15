// =============================================================
// F1 CYD Notifications
// ESP32-2432S028R (2.8" Cheap Yellow Display)
//
// Displays F1 race schedule, countdown timers, and post-race
// results. Sends Telegram notifications for race events.
// =============================================================

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "debug.h"
#include "types.h"
#include "config_manager.h"
#include "wifi_setup.h"
#include "time_utils.h"
#include "f1_data.h"
#include "display_renderer.h"
#include "display_states.h"
#include "telegram_handler.h"
#include "screenshot_capture.h"
#include "web_server.h"
#include "f1_logo.h"

// --- Global State ---
AppConfig appConfig;

// --- Timing (millis-based, non-blocking) ---
unsigned long lastScheduleFetch   = 0;
unsigned long lastResultsPoll     = 0;
unsigned long lastNotificationChk = 0;
unsigned long lastBrightnessChk   = 0;
unsigned long lastWiFiCheck       = 0;
unsigned long lastTimeSave        = 0;
bool resultsPollActive            = false;

// --- Touch ---
bool touchEnabled = false;

void initTouch() {
    // Touch is handled by TFT_eSPI via the TOUCH_CS pin
    // Calibration values for CYD 320x240 landscape
    uint16_t calData[5] = {300, 3600, 300, 3600, 7};  // Approximate, tune later
    tft.setTouch(calData);
    touchEnabled = true;
    DBG_INFO("[Main] Touch initialized");
}

void checkTouch() {
    if (!touchEnabled) return;
    uint16_t x, y;
    if (tft.getTouch(&x, &y)) {
        DBG_VERBOSE("[Main] Touch at (%d, %d)", x, y);
        manualAdvanceDisplay(getCurrentRace());
    }
}

// --- RGB LED ---
void initLED() {
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    // CYD LED is active LOW
    digitalWrite(PIN_LED_R, HIGH);
    digitalWrite(PIN_LED_G, HIGH);
    digitalWrite(PIN_LED_B, HIGH);
}

void setLED(bool r, bool g, bool b) {
    digitalWrite(PIN_LED_R, !r);
    digitalWrite(PIN_LED_G, !g);
    digitalWrite(PIN_LED_B, !b);
}

// --- Fetch post-race data with per-step TFT status messages ---
bool fetchPostRaceDataWithStatus(uint8_t round) {
    drawStatusMessage("Fetching race result...");
    bool resultsOk = fetchRaceResults(round);

    drawStatusMessage("Fetching driver standings...");
    bool driversOk = fetchDriverStandings();

    drawStatusMessage("Fetching constructor standings...");
    bool constructorsOk = fetchConstructorStandings();

    bool ok = resultsOk && driversOk && constructorsOk;
    resultsAvailable = ok;
    DBG_INFO("[Main] Post-race fetch: podium=%s drivers=%s constructors=%s",
             resultsOk ? "ok" : "fail",
             driversOk ? "ok" : "fail",
             constructorsOk ? "ok" : "fail");
    return ok;
}

// --- Post-Race Data Polling ---
void checkResultsPolling() {
    time_t now = nowUTC();

    // Use the most recently completed race: current if its GP has passed, else previous.
    // This handles the overlap where we're in race-week for the next race but still
    // within the results window for the race just gone.
    RaceData& raceForResults = (now >= getCurrentRace().gpTimeUtc) ? getCurrentRace() : getPrevRace();
    long secsAfterGP = now - raceForResults.gpTimeUtc;

    // Start polling after GP + 3 hours
    if (secsAfterGP >= RESULTS_POLL_AFTER_SEC &&
        secsAfterGP < RESULTS_GIVE_UP_SEC &&
        !resultsAvailable) {
        if (!resultsPollActive) {
            DBG_INFO("[Main] Results polling activated for R%d (%.1fh after GP)",
                     raceForResults.round, secsAfterGP / 3600.0f);
        }
        resultsPollActive = true;
    } else {
        resultsPollActive = false;
    }

    if (resultsPollActive &&
        millis() - lastResultsPoll >= RESULTS_RETRY_MS) {
        lastResultsPoll = millis();
        DBG_INFO("[Main] Polling for race results (R%d)...", raceForResults.round);
        if (fetchPostRaceDataWithStatus(raceForResults.round)) {
            resultsPollActive = false;
            requestRedraw();
            // Save notification config (results bit will be set by checkNotifications)
            saveConfig(appConfig);
        }
    }
}

// --- WiFi Reconnect ---
// Monitors WiFi connectivity. When a drop is detected, WiFi.reconnect() is called.
// On reconnection, an NTP resync is triggered so the clock is corrected promptly
// rather than waiting up to an hour for ezTime's automatic re-sync interval.
void checkWiFiReconnect() {
    static bool wifiWasConnected = true;  // Assume connected — setup() blocked until connected
    bool connected = (WiFi.status() == WL_CONNECTED);

    if (!connected && wifiWasConnected) {
        DBG_WARN("[WiFi] Connection lost — attempting reconnect");
        WiFi.reconnect();
    } else if (connected && !wifiWasConnected) {
        DBG_INFO("[WiFi] Reconnected. IP: %s", WiFi.localIP().toString().c_str());
        resyncNTP();
    }
    wifiWasConnected = connected;
}

// --- Post-Race Expiry ---
// When the post-race window ends, immediately refresh the schedule so races[1] advances
// to the next upcoming race and the race-week countdown activates without waiting 24h.
void checkPostRaceExpiry() {
    static bool wasInPostRace = false;
    RaceData& race = getCurrentRace();
    long secsAfterGP = (long)(nowUTC() - race.gpTimeUtc);
    int daysAfterGP = (int)(secsAfterGP / 86400);
    bool inPostRace = (secsAfterGP >= 0 && daysAfterGP < POST_RACE_DAYS);

    if (wasInPostRace && !inPostRace) {
        DBG_INFO("[Main] Post-race window ended — refreshing schedule for next race");
        if (fetchSchedule()) {
            requestRedraw();
            lastScheduleFetch = millis();
        }
    }
    wasInPostRace = inPostRace;
}

// --- Schedule Refresh ---
void checkScheduleRefresh() {
    if (millis() - lastScheduleFetch >= SCHEDULE_REFRESH_MS) {
        lastScheduleFetch = millis();
        DBG_INFO("[Main] 24h schedule refresh triggered");
        if (fetchSchedule()) {
            requestRedraw();
        }
    }
}

// =============================================================
// SETUP
// =============================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    DBG_INFO("=== F1 CYD Notifications ===");
    DBG_INFO("[Main] Debug level: %d", debugLevel);

    // 1. Init display
    DBG_INFO("[Main] 1/12 Initializing display");
    initDisplay();

    // 2. Init screenshot capture (must be ready before startup captures)
    DBG_INFO("[Main] 2/12 Initializing screenshot capture");
#if SCREENSHOT_BUTTON_ENABLED
    initScreenshotCapture(PIN_SD_CS, PIN_SHOT_BTN);
#else
    initScreenshotCapture(PIN_SD_CS, -1);
#endif

    // 3. Show splash
    DBG_INFO("[Main] 3/12 Drawing splash screen");
    drawSplashScreen();
#if SCREENSHOT_STARTUP_CAPTURES
    captureScreenshotNow("boot_splash");
#endif

    // 4. Init LED
    DBG_INFO("[Main] 4/12 Initializing LED");
    initLED();
    setLED(false, false, true);  // Blue = connecting

    // 5. Init LittleFS and load config
    DBG_INFO("[Main] 5/12 Mounting filesystem and loading config");
    initStorage();
    loadConfig(appConfig);

    // 6. Apply brightness
    DBG_INFO("[Main] 6/12 Applying brightness: %d", appConfig.brightness);
    updateBrightness(appConfig.brightness);

    // 7. Connect WiFi (blocks until connected or restarts)
    DBG_INFO("[Main] 7/12 Starting WiFi");
    setupWiFi(appConfig);
    setLED(false, true, false);  // Green = connected

    // 8. Sync NTP time
    DBG_INFO("[Main] 8/12 Syncing NTP time (TZ: %s)", appConfig.timezone);
    drawStatusMessage("Syncing time...");
#if SCREENSHOT_STARTUP_CAPTURES
    captureScreenshotNow("boot_sync_time");
#endif
    if (!initTime(appConfig.timezone, appConfig.ntpServer)) {
        DBG_WARN("[Main] Time sync failed - continuing without NTP");
    }

    // 9. Fetch F1 schedule
    DBG_INFO("[Main] 9/12 Fetching F1 schedule");
    drawStatusMessage("Loading F1 schedule...");
#if SCREENSHOT_STARTUP_CAPTURES
    captureScreenshotNow("boot_loading_schedule");
#endif
    if (!fetchSchedule()) {
        DBG_WARN("[Main] Schedule fetch failed, trying cache");
        if (!loadScheduleFromCache()) {
            DBG_ERROR("[Main] No schedule data available");
            drawStatusMessage("No schedule data available");
#if SCREENSHOT_STARTUP_CAPTURES
            captureScreenshotNow("boot_no_schedule_data");
#endif
            delay(2000);
        }
    }
    lastScheduleFetch = millis();

#if FORCE_POST_RACE_TEST_DISPLAYS
    DBG_INFO("[Main] Test mode enabled: forcing post-race displays and prefetching post-race data");
    fetchPostRaceDataWithStatus(getCurrentRace().round);
#else
    // If we're already in the post-race results window on boot, fetch immediately rather
    // than waiting RESULTS_RETRY_MS (30 min) for the first poll to trigger.
    // Use the most recently completed race — this handles the race-week overlap where
    // the NEXT race is within COUNTDOWN_WEEK_DAYS but results for the PREVIOUS race
    // are still due (e.g. boot on day 2 after R1 when R2 is 5 days away).
    {
        time_t now = nowUTC();
        RaceData& raceForResults = (now >= getCurrentRace().gpTimeUtc) ? getCurrentRace() : getPrevRace();
        long secsAfterGP = (long)(now - raceForResults.gpTimeUtc);
        if (secsAfterGP >= RESULTS_POLL_AFTER_SEC && secsAfterGP < RESULTS_GIVE_UP_SEC) {
            DBG_INFO("[Main] Post-race window at boot — fetching results for R%d", raceForResults.round);
            if (fetchPostRaceDataWithStatus(raceForResults.round)) {
                DBG_INFO("[Main] Post-race data ready");
            } else {
                DBG_WARN("[Main] Post-race fetch failed — will retry in loop");
            }
            lastResultsPoll = millis();  // Suppress duplicate poll for 30 min
        }
    }
#endif

    // 10. Init Telegram
    DBG_INFO("[Main] 10/12 Telegram: %s", appConfig.telegramEnabled ? "enabled" : "disabled");
    if (appConfig.telegramEnabled) {
        initTelegram(appConfig.botToken);
    }

    // 11. Start web server + OTA
    DBG_INFO("[Main] 11/12 Starting web server");
    setupWebServer(appConfig);

    // 12. Init touch
    DBG_INFO("[Main] 12/12 Initializing touch");
    initTouch();

    // Ready
    setLED(false, false, false);  // LED off
    requestRedraw();
#if SCREENSHOT_STARTUP_CAPTURES
    updateDisplay(getCurrentRace());  // Ensure current UI frame is rendered before capture
    captureScreenshotNow("boot_ready");
#endif

    DBG_INFO("[Main] 12/12 Setup complete");
    DBG_INFO("[Main] Free heap: %u bytes", ESP.getFreeHeap());
    DBG_INFO("[Main] IP: %s", WiFi.localIP().toString().c_str());
}

// =============================================================
// LOOP
// =============================================================
void loop() {
    // ezTime maintenance (NTP re-sync)
    events();

    // Touch input
    checkTouch();

    // Display update (handles state machine + rendering)
    updateDisplay(getCurrentRace());

    // Screenshot triggers and queued capture execution
    pollScreenshotButton();
    handleScreenshotRequests();

    // Periodic tasks (all non-blocking, millis-based)

    // Notification check - every minute
    if (millis() - lastNotificationChk >= NOTIFICATION_CHECK_MS) {
        lastNotificationChk = millis();
        checkNotifications(getCurrentRace(), appConfig);
        // Save notification bits if changed
        saveConfig(appConfig);
    }

    // Post-race results polling
    checkResultsPolling();

    // Post-race expiry — triggers immediate schedule refresh when window ends
    checkPostRaceExpiry();

    // Schedule refresh - every 24 hours
    checkScheduleRefresh();

    // WiFi reconnect + NTP resync — every 30 seconds
    if (millis() - lastWiFiCheck >= WIFI_CHECK_MS) {
        lastWiFiCheck = millis();
        checkWiFiReconnect();
    }

    // Periodic time save — every 15 minutes, only after a real NTP sync.
    // Guards against re-saving a stale fallback time on repeated failed boots.
    if (millis() - lastTimeSave >= TIME_SAVE_MS) {
        lastTimeSave = millis();
        if (ntpHasSynced() && nowUTC() > MIN_PLAUSIBLE_EPOCH) {
            saveLastKnownTime(nowUTC());
        }
    }

    // Brightness update - every 10 seconds
    if (millis() - lastBrightnessChk >= BRIGHTNESS_CHECK_MS) {
        lastBrightnessChk = millis();
        updateBrightness(appConfig.brightness);
    }

    // ElegantOTA loop handler
    ElegantOTA.loop();

    yield();
}
