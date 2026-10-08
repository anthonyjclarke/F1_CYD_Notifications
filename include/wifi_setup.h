#pragma once

#include <WiFiManager.h>
#include "config.h"
#include "types.h"
#include "debug.h"
#include "config_manager.h"
#include "network/improv_setup.h"

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
    _wmConfigPtr->telegramEnabled =
        strlen(_wmConfigPtr->botToken) > 0 && strlen(_wmConfigPtr->chatId) > 0;
    saveConfig(*_wmConfigPtr);
    DBG_INFO("[WiFi] Params saved from captive portal");
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

    DBG_INFO("[WiFi] Starting WiFiManager (AP: %s, timeout: %ds)", WIFI_AP_NAME, WIFI_TIMEOUT_SEC);
#if IMPROV_SETUP_ENABLED
    // Non-blocking portal so Improv (web installer "Configure WiFi") is served
    // alongside the captive portal on a device with no WiFi.
    wm.setConfigPortalBlocking(false);
#endif
    bool connected = wm.autoConnect(WIFI_AP_NAME, WIFI_AP_PASSWORD);
#if IMPROV_SETUP_ENABLED
    while (!connected && wm.getConfigPortalActive()) {
        if (wm.process()) { connected = true; break; }
        improvTick();  // restarts once Improv credentials connect
        delay(5);
    }
#endif

    // Clean up
    delete wmTzParam;   wmTzParam = nullptr;
    delete wmBotParam;  wmBotParam = nullptr;
    delete wmChatParam; wmChatParam = nullptr;
    _wmConfigPtr = nullptr;

    if (connected) {
        DBG_INFO("[WiFi] Connected. IP: %s, RSSI: %d dBm",
                 WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
        DBG_ERROR("[WiFi] Failed to connect - restarting");
        ESP.restart();
    }
    return connected;
}
