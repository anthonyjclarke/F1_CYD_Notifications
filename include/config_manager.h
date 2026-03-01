#pragma once

#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"
#include "types.h"

// Initialize LittleFS
bool initStorage() {
    if (!LittleFS.begin(true)) {
        Serial.println(F("[Storage] LittleFS mount failed"));
        return false;
    }
    Serial.println(F("[Storage] LittleFS mounted"));
    return true;
}

// Load config from LittleFS
bool loadConfig(AppConfig& cfg) {
    File f = LittleFS.open(CONFIG_FILE, "r");
    if (!f) {
        Serial.println(F("[Config] No config file, using defaults"));
        setDefaultConfig(cfg);
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.printf("[Config] Parse error: %s\n", err.c_str());
        setDefaultConfig(cfg);
        return false;
    }

    strlcpy(cfg.timezone,  doc["tz"]    | "Europe/London",  sizeof(cfg.timezone));
    strlcpy(cfg.ntpServer, doc["ntp"]   | "pool.ntp.org",   sizeof(cfg.ntpServer));
    strlcpy(cfg.botToken,  doc["bot"]   | "",               sizeof(cfg.botToken));
    strlcpy(cfg.chatId,    doc["chat"]  | "",               sizeof(cfg.chatId));
    cfg.brightness         = doc["bright"]   | 128;
    cfg.lastNotifiedRound  = doc["notRound"] | 0;
    cfg.notificationBits   = doc["notBits"]  | 0;
    cfg.telegramEnabled    = doc["tgOn"]     | false;

    Serial.printf("[Config] Loaded: TZ=%s, Telegram=%s\n",
                  cfg.timezone, cfg.telegramEnabled ? "on" : "off");
    return true;
}

// Save config to LittleFS
bool saveConfig(const AppConfig& cfg) {
    JsonDocument doc;
    doc["tz"]       = cfg.timezone;
    doc["ntp"]      = cfg.ntpServer;
    doc["bot"]      = cfg.botToken;
    doc["chat"]     = cfg.chatId;
    doc["bright"]   = cfg.brightness;
    doc["notRound"] = cfg.lastNotifiedRound;
    doc["notBits"]  = cfg.notificationBits;
    doc["tgOn"]     = cfg.telegramEnabled;

    File f = LittleFS.open(CONFIG_FILE, "w");
    if (!f) {
        Serial.println(F("[Config] Failed to open for write"));
        return false;
    }
    serializeJson(doc, f);
    f.close();
    Serial.println(F("[Config] Saved"));
    return true;
}

// Cache raw schedule JSON to LittleFS
bool cacheSchedule(const String& json) {
    File f = LittleFS.open(RACE_CACHE_FILE, "w");
    if (!f) return false;
    f.print(json);
    f.close();
    return true;
}

// Load cached schedule JSON
String loadCachedSchedule() {
    File f = LittleFS.open(RACE_CACHE_FILE, "r");
    if (!f) return "";
    String json = f.readString();
    f.close();
    return json;
}

// Cache results JSON
bool cacheResults(const String& json) {
    File f = LittleFS.open(RESULTS_CACHE_FILE, "w");
    if (!f) return false;
    f.print(json);
    f.close();
    return true;
}

// Load cached results JSON
String loadCachedResults() {
    File f = LittleFS.open(RESULTS_CACHE_FILE, "r");
    if (!f) return "";
    String json = f.readString();
    f.close();
    return json;
}
