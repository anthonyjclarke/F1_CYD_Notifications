#pragma once

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "config.h"
#include "types.h"
#include "debug.h"
#include "time_utils.h"
#include "config_manager.h"

// Web UI Test / Resend requests. The AsyncTCP handler only queues them;
// loop() sends via handleTelegramRequests() so a slow send never blocks the
// web server task and never overlaps an automatic notification.
enum TelegramRequest : uint8_t { TG_REQ_NONE, TG_REQ_TEST, TG_REQ_RESEND };
static volatile uint8_t telegramRequest = TG_REQ_NONE;
static volatile int8_t telegramLastResult = -1;  // -1 none yet, 0 failed, 1 sent

bool telegramHasLastMessage() {
    File f = LittleFS.open(TELEGRAM_LAST_FILE, "r");
    if (!f) return false;
    bool hasMessage = f.size() > 0;
    f.close();
    return hasMessage;
}

bool saveLastTelegramMessage(const String& message) {
    File f = LittleFS.open(TELEGRAM_LAST_FILE, "w");
    if (!f) {
        DBG_WARN("[Telegram] Failed to save last message");
        return false;
    }
    f.print(message);
    f.close();
    return true;
}

String loadLastTelegramMessage() {
    File f = LittleFS.open(TELEGRAM_LAST_FILE, "r");
    if (!f) return "";
    String message = f.readString();
    f.close();
    return message;
}

// One HTTPS POST to the Bot API; returns the HTTP code (negative = transport error).
// Never retried here: UniversalTelegramBot re-POSTed for up to 8 s whenever it
// misread a slow reply, so Telegram received (and delivered) the message twice.
static int postTelegramMessage(const AppConfig& cfg, const String& message, bool markdown) {
    WiFiClientSecure client;
    client.setInsecure();  // No cert pinning for api.telegram.org
    HTTPClient http;

    // The Bot API takes the token in the path; never log this URL.
    String url = "https://api.telegram.org/bot";
    url += cfg.botToken;
    url += "/sendMessage";
    if (!http.begin(client, url)) {
        DBG_WARN("[Telegram] HTTP begin failed");
        return -1;
    }
    http.setTimeout(TELEGRAM_TIMEOUT_MS);
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["chat_id"] = cfg.chatId;
    doc["text"]    = message;
    if (markdown) doc["parse_mode"] = "Markdown";
    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    if (code > 0 && code != HTTP_CODE_OK) {
        // Error replies carry a "description" (never the token) - log it for diagnosis
        DBG_WARN("[Telegram] HTTP %d: %s", code, http.getString().c_str());
    }
    http.end();
    return code;
}

// Send a message via Telegram (Markdown). Returns true once Telegram accepted it.
bool sendTelegramMessage(const AppConfig& cfg, const String& message) {
    if (!cfg.telegramEnabled || WiFi.status() != WL_CONNECTED) {
        DBG_WARN("[Telegram] Cannot send: not configured or WiFi down");
        return false;
    }
    DBG_VERBOSE("[Telegram] Sending to chat %s (%d chars)", cfg.chatId, message.length());
    int code = postTelegramMessage(cfg, message, true);
    if (code == HTTP_CODE_BAD_REQUEST) {
        // 400 = rejected, nothing delivered (e.g. a name breaks Markdown) - safe to resend as plain text
        DBG_WARN("[Telegram] Markdown rejected, resending as plain text");
        code = postTelegramMessage(cfg, message, false);
    }
    if (code == HTTP_CODE_OK) {
        DBG_INFO("[Telegram] Message sent OK");
        saveLastTelegramMessage(message);
        return true;
    }
    DBG_WARN("[Telegram] Send failed (%d)", code);
    return false;
}

String formatTelegramConfigTestMessage() {
    String msg = "*F1 Display Telegram test*\n\n";
    msg += "Telegram credentials are configured and this chat can receive messages.";
    return msg;
}

bool sendTelegramConfigTest(const AppConfig& cfg) {
    if (!cfg.telegramEnabled) {
        DBG_WARN("[Telegram] Test skipped: token/chat not configured");
        return false;
    }
    return sendTelegramMessage(cfg, formatTelegramConfigTestMessage());
}

bool resendLastTelegramMessage(const AppConfig& cfg) {
    String message = loadLastTelegramMessage();
    if (message.length() == 0) {
        DBG_WARN("[Telegram] No saved message to resend");
        return false;
    }
    DBG_INFO("[Telegram] Resending last saved message");
    return sendTelegramMessage(cfg, message);
}

// Queue a web UI request. Returns false if one is already pending.
bool queueTelegramRequest(TelegramRequest req) {
    if (telegramRequest != TG_REQ_NONE) return false;
    telegramLastResult = -1;
    telegramRequest = req;
    return true;
}

bool telegramRequestPending() {
    return telegramRequest != TG_REQ_NONE;
}

// Run a queued web UI request - called from loop()
void handleTelegramRequests(const AppConfig& cfg) {
    uint8_t req = telegramRequest;
    if (req == TG_REQ_NONE) return;
    bool ok = (req == TG_REQ_TEST) ? sendTelegramConfigTest(cfg) : resendLastTelegramMessage(cfg);
    telegramLastResult = ok ? 1 : 0;
    telegramRequest = TG_REQ_NONE;
}

// Format race week notification message
String formatRaceWeekMessage(RaceData& race) {
    String msg = "🏎 *F1 Race Week!*\n\n";
    msg += "📍 *";
    msg += race.name;
    msg += " Grand Prix*\n";
    msg += race.location;
    msg += "\n\n";

    if (race.isSprint) {
        msg += "⚡ Sprint Weekend\n\n";
    }

    msg += "📅 *Session Schedule:*\n";
    for (uint8_t i = 0; i < race.sessionCount; i++) {
        SessionInfo& s = race.sessions[i];
        msg += "  ";
        msg += s.label;
        msg += " - ";
        msg += s.dayAbbrev;
        msg += " ";
        msg += s.localTime;
        msg += "\n";
    }
    return msg;
}

// Format pre-session notification
String formatPreSessionMessage(const char* sessionName, RaceData& race) {
    String msg = "⏰ *";
    msg += sessionName;
    msg += "* starts in 1 hour!\n\n";
    msg += "📍 ";
    msg += race.name;
    msg += " Grand Prix";
    return msg;
}

// Format results notification
String formatResultsMessage(RaceData& race, RaceResult* results, uint8_t count) {
    String msg = "🏁 *";
    msg += race.name;
    msg += " Grand Prix - Results*\n\n";

    const char* medals[] = {"🥇", "🥈", "🥉"};
    for (uint8_t i = 0; i < count && i < 3; i++) {
        msg += medals[i];
        msg += " ";
        msg += results[i].driverName;
        msg += " (";
        msg += results[i].constructor;
        msg += ")\n";
    }
    return msg;
}

// Record a sent notification and persist it straight away, so a reboot
// before the next save can't send it again.
static void markNotified(AppConfig& cfg, uint8_t bit) {
    cfg.notificationBits |= bit;
    saveConfig(cfg);
}

// Check and send notifications - called every minute from loop()
void checkNotifications(RaceData& race, AppConfig& cfg) {
    if (!cfg.telegramEnabled) return;

    // A fallback (saved) clock can resolve an earlier round - wait for real NTP time.
    if (!ntpHasSynced()) return;

    time_t now = nowUTC();

    // The bits belong to the race whose GP is notifiedGpUtc. Only a later race resets
    // them, so a schedule parsed against a stale clock can't step back a round, clear
    // the bits and re-send. A race older than the notified one is ignored.
    if (race.gpTimeUtc < cfg.notifiedGpUtc) return;
    if (race.gpTimeUtc > cfg.notifiedGpUtc) {
        // Config saved by <= 0.6.2 has no notGp: keep its bits if they are for this round.
        bool upgradedSameRound = (cfg.notifiedGpUtc == 0 && cfg.lastNotifiedRound == race.round);
        if (!upgradedSameRound) {
            // During the overlap window (post-race R(n) + countdown to R(n+1)), R(n) results
            // are still in memory - keep NOTIFY_RESULT so they aren't re-sent for R(n+1).
            DBG_INFO("[Telegram] New round (%d→%d), resetting notification bits",
                     cfg.lastNotifiedRound, race.round);
            cfg.notificationBits = (resultsAvailable && podiumCount > 0) ? NOTIFY_RESULT : 0;
        }
        cfg.notifiedGpUtc = race.gpTimeUtc;
        cfg.lastNotifiedRound = race.round;
        saveConfig(cfg);
    }

    // Race week notification - fires once when we first enter the countdown window.
    // Previously required isMonday() but that caused misses during overlap windows or
    // after late firmware deploys. The NOTIFY_RACE_WEEK dedup bit prevents re-sending.
    long daysToFirst = (race.firstSessionUtc - now) / 86400;
    if (daysToFirst >= 0 && daysToFirst <= COUNTDOWN_WEEK_DAYS &&
        !(cfg.notificationBits & NOTIFY_RACE_WEEK)) {
        DBG_INFO("[Telegram] Sending race week notification: %s", race.name);
        if (sendTelegramMessage(cfg, formatRaceWeekMessage(race))) {
            markNotified(cfg, NOTIFY_RACE_WEEK);
        }
    }

    // Pre-session notifications (1 hour before)
    for (uint8_t i = 0; i < race.sessionCount; i++) {
        SessionInfo& s = race.sessions[i];
        if (!isWithinHours(s.utcTime, NOTIFY_HOURS_BEFORE)) continue;

        uint8_t notifyBit = 0;
        switch (s.type) {
            case SESSION_QUALIFYING:        notifyBit = NOTIFY_PRE_QUALI; break;
            case SESSION_GP:                notifyBit = NOTIFY_PRE_RACE; break;
            case SESSION_SPRINT:            notifyBit = NOTIFY_PRE_SPRINT; break;
            case SESSION_SPRINT_QUALIFYING: notifyBit = NOTIFY_PRE_SPRINT_Q; break;
            default: continue;
        }

        if (!(cfg.notificationBits & notifyBit)) {
            DBG_INFO("[Telegram] Sending pre-session notification: %s", s.label);
            if (sendTelegramMessage(cfg, formatPreSessionMessage(s.label, race))) {
                markNotified(cfg, notifyBit);
            }
        }
    }

    // Results notification - after GP.
    // During the combined overlap window, getCurrentRace() is R(n+1) but podium data
    // belongs to R(n). Use getPrevRace() when the current race GP hasn't happened yet.
    if (resultsAvailable && podiumCount > 0 &&
        !(cfg.notificationBits & NOTIFY_RESULT)) {
        RaceData& raceForResults = (now >= race.gpTimeUtc) ? race : getPrevRace();
        DBG_INFO("[Telegram] Sending race results notification for R%d (%s)",
                 raceForResults.round, raceForResults.name);
        if (sendTelegramMessage(cfg, formatResultsMessage(raceForResults, podium, podiumCount))) {
            markNotified(cfg, NOTIFY_RESULT);
        }
    }
}
