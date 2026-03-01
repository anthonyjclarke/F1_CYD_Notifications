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
#include "types.h"
#include "config_manager.h"
#include "wifi_setup.h"
#include "time_utils.h"
#include "f1_data.h"
#include "display_renderer.h"
#include "display_states.h"
#include "telegram_handler.h"
#include "web_server.h"

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
}

void checkTouch() {
    if (!touchEnabled) return;
    uint16_t x, y;
    if (tft.getTouch(&x, &y)) {
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
        resultsPollActive = true;
    } else {
        resultsPollActive = false;
    }

    if (resultsPollActive &&
        millis() - lastResultsPoll >= RESULTS_RETRY_MS) {
        lastResultsPoll = millis();
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
        Serial.println(F("[Main] Refreshing schedule..."));
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
    Serial.println(F("\n=== F1 CYD Notifications ==="));

    // 1. Init display and show splash
    initDisplay();
    drawSplashScreen();

    // 2. Init LED
    initLED();
    setLED(false, false, true);  // Blue = connecting

    // 3. Init LittleFS and load config
    initStorage();
    loadConfig(appConfig);

    // 4. Apply brightness
    updateBrightness(appConfig.brightness);

    // 5. Connect WiFi (blocks until connected or restarts)
    setupWiFi(appConfig);
    setLED(false, true, false);  // Green = connected

    // 6. Sync NTP time
    drawStatusMessage("Syncing time...");
    if (!initTime(appConfig.timezone, appConfig.ntpServer)) {
        Serial.println(F("[Main] Time sync warning - continuing"));
    }

    // 7. Fetch F1 schedule
    drawStatusMessage("Loading F1 schedule...");
    if (!fetchSchedule()) {
        Serial.println(F("[Main] Fetch failed, trying cache"));
        if (!loadScheduleFromCache()) {
            drawStatusMessage("No schedule data available");
            delay(2000);
        }
    }
    lastScheduleFetch = millis();

    // 8. Init Telegram
    if (appConfig.telegramEnabled) {
        initTelegram(appConfig.botToken);
    }

    // 9. Start web server + OTA
    setupWebServer(appConfig);

    // 10. Init touch
    initTouch();

    // 11. Ready
    setLED(false, false, false);  // LED off
    requestRedraw();

    Serial.printf("[Main] Setup complete. Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("[Main] IP: %s\n", WiFi.localIP().toString().c_str());
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
