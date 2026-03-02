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

// --- Post-Race Data Polling ---
void checkResultsPolling() {
    RaceData& race = getCurrentRace();
    time_t now = nowUTC();
    long secsAfterGP = now - race.gpTimeUtc;

    // Start polling after GP + 3 hours
    if (secsAfterGP >= RESULTS_POLL_AFTER_SEC &&
        secsAfterGP < RESULTS_GIVE_UP_SEC &&
        !resultsAvailable) {
        if (!resultsPollActive) {
            DBG_INFO("[Main] Results polling activated (%.1fh after GP)", secsAfterGP / 3600.0f);
        }
        resultsPollActive = true;
    } else {
        resultsPollActive = false;
    }

    if (resultsPollActive &&
        millis() - lastResultsPoll >= RESULTS_RETRY_MS) {
        lastResultsPoll = millis();
        DBG_INFO("[Main] Polling for race results (R%d)...", race.round);
        if (fetchPostRaceData(race.round)) {
            resultsPollActive = false;
            requestRedraw();
            // Save notification config (results bit will be set by checkNotifications)
            saveConfig(appConfig);
        }
    }
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
    drawStatusMessage("Syncing time...", 6);
#if SCREENSHOT_STARTUP_CAPTURES
    captureScreenshotNow("boot_sync_time");
#endif
    if (!initTime(appConfig.timezone, appConfig.ntpServer)) {
        DBG_WARN("[Main] Time sync failed - continuing without NTP");
    }

    // 9. Fetch F1 schedule
    DBG_INFO("[Main] 9/12 Fetching F1 schedule");
    drawStatusMessage("Loading F1 schedule...", 6);
#if SCREENSHOT_STARTUP_CAPTURES
    captureScreenshotNow("boot_loading_schedule");
#endif
    if (!fetchSchedule()) {
        DBG_WARN("[Main] Schedule fetch failed, trying cache");
        if (!loadScheduleFromCache()) {
            DBG_ERROR("[Main] No schedule data available");
            drawStatusMessage("No schedule data available", 6);
#if SCREENSHOT_STARTUP_CAPTURES
            captureScreenshotNow("boot_no_schedule_data");
#endif
            delay(2000);
        }
    }
    lastScheduleFetch = millis();

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

    // Schedule refresh - every 24 hours
    checkScheduleRefresh();

    // Brightness update - every 10 seconds
    if (millis() - lastBrightnessChk >= BRIGHTNESS_CHECK_MS) {
        lastBrightnessChk = millis();
        updateBrightness(appConfig.brightness);
    }

    // ElegantOTA loop handler
    ElegantOTA.loop();

    yield();
}
