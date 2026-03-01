#pragma once

#include <WiFiManager.h>
#include "config.h"
#include "types.h"
#include "config_manager.h"

// Custom WiFiManager parameters
static WiFiManagerParameter* wmTzParam    = nullptr;
static WiFiManagerParameter* wmBotParam   = nullptr;
static WiFiManagerParameter* wmChatParam  = nullptr;

// Pointer to config for save callback
static AppConfig* _wmConfigPtr = nullptr;

void wmSaveParamsCallback() {
    if (!_wmConfigPtr) return;
    if (wmTzParam)   strlcpy(_wmConfigPtr->timezone, wmTzParam->getValue(),  sizeof(_wmConfigPtr->timezone));
    if (wmBotParam)  strlcpy(_wmConfigPtr->botToken, wmBotParam->getValue(), sizeof(_wmConfigPtr->botToken));
    if (wmChatParam) strlcpy(_wmConfigPtr->chatId,   wmChatParam->getValue(), sizeof(_wmConfigPtr->chatId));
    _wmConfigPtr->telegramEnabled = strlen(_wmConfigPtr->botToken) > 0;
    saveConfig(*_wmConfigPtr);
    Serial.println(F("[WiFi] Params saved from captive portal"));
}

// Setup WiFi using WiFiManager captive portal
bool setupWiFi(AppConfig& cfg) {
    _wmConfigPtr = &cfg;

    WiFiManager wm;
    wm.setConfigPortalTimeout(WIFI_TIMEOUT_SEC);

    // Add custom parameters
    wmTzParam   = new WiFiManagerParameter("tz",   "Timezone (IANA)", cfg.timezone, 48);
    wmBotParam  = new WiFiManagerParameter("bot",  "Telegram Bot Token", cfg.botToken, 64);
    wmChatParam = new WiFiManagerParameter("chat", "Telegram Chat ID", cfg.chatId, 16);

    wm.addParameter(wmTzParam);
    wm.addParameter(wmBotParam);
    wm.addParameter(wmChatParam);

    wm.setSaveParamsCallback(wmSaveParamsCallback);

    Serial.println(F("[WiFi] Starting WiFiManager..."));
    bool connected = wm.autoConnect(WIFI_AP_NAME, WIFI_AP_PASSWORD);

    // Clean up
    delete wmTzParam;   wmTzParam = nullptr;
    delete wmBotParam;  wmBotParam = nullptr;
    delete wmChatParam; wmChatParam = nullptr;
    _wmConfigPtr = nullptr;

    if (connected) {
        Serial.printf("[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println(F("[WiFi] Failed to connect, restarting..."));
        ESP.restart();
    }
    return connected;
}
