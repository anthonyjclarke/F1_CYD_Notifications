#pragma once

#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "config.h"
#include "types.h"
#include "time_utils.h"

static WiFiClientSecure telegramClient;
static UniversalTelegramBot* bot = nullptr;
static bool telegramReady = false;

// Initialize Telegram bot
void initTelegram(const char* token) {
    if (strlen(token) == 0) {
        telegramReady = false;
        return;
    }
    telegramClient.setInsecure();  // Skip cert verification
    bot = new UniversalTelegramBot(token, telegramClient);
    telegramReady = true;
    Serial.println(F("[Telegram] Bot initialized"));
}

// Send a message via Telegram
bool sendTelegramMessage(const char* chatId, const String& message) {
    if (!telegramReady || !bot || strlen(chatId) == 0) return false;
    Serial.printf("[Telegram] Sending to %s...\n", chatId);
    bool ok = bot->sendMessage(chatId, message, "");
    if (ok) {
        Serial.println(F("[Telegram] Sent OK"));
    } else {
        Serial.println(F("[Telegram] Send failed"));
    }
    return ok;
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

// Check and send notifications - called every minute from loop()
void checkNotifications(RaceData& race, AppConfig& cfg) {
    if (!cfg.telegramEnabled || !telegramReady) return;

    time_t now = nowUTC();

    // Reset notification bits when we move to a new round
    if (cfg.lastNotifiedRound != race.round) {
        cfg.lastNotifiedRound = race.round;
        cfg.notificationBits = 0;
    }

    // Race week notification - Monday of race week
    long daysToFirst = (race.firstSessionUtc - now) / 86400;
    if (daysToFirst >= 0 && daysToFirst <= COUNTDOWN_WEEK_DAYS &&
        isMonday() &&
        !(cfg.notificationBits & NOTIFY_RACE_WEEK)) {
        String msg = formatRaceWeekMessage(race);
        if (sendTelegramMessage(cfg.chatId, msg)) {
            cfg.notificationBits |= NOTIFY_RACE_WEEK;
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
            String msg = formatPreSessionMessage(s.label, race);
            if (sendTelegramMessage(cfg.chatId, msg)) {
                cfg.notificationBits |= notifyBit;
            }
        }
    }

    // Results notification - after GP
    if (resultsAvailable && podiumCount > 0 &&
        !(cfg.notificationBits & NOTIFY_RESULT)) {
        String msg = formatResultsMessage(race, podium, podiumCount);
        if (sendTelegramMessage(cfg.chatId, msg)) {
            cfg.notificationBits |= NOTIFY_RESULT;
        }
    }
}
