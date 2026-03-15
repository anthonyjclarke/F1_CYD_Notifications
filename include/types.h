#pragma once

#include <Arduino.h>
#include <time.h>

// --- Session Types ---
enum SessionType : uint8_t {
    SESSION_FP1,
    SESSION_FP2,
    SESSION_FP3,
    SESSION_SPRINT_QUALIFYING,
    SESSION_SPRINT,
    SESSION_QUALIFYING,
    SESSION_GP
};

// --- Display States ---
enum DisplayState : uint8_t {
    STATE_IDLE,                     // Between race weeks - countdown to next race
    STATE_RACE_WEEK_COUNTDOWN,      // Large digit countdown to first session
    STATE_RACE_WEEK_EVENT_DETAILS,  // Event info: round, date range, sprint flag
    STATE_RACE_WEEK_SCHEDULE,       // Session schedule table
    STATE_POST_RACE_WINNER,         // Race winner / podium
    STATE_POST_RACE_DRIVERS,        // Driver standings
    STATE_POST_RACE_CONSTRUCTORS,   // Constructor standings
    STATE_POST_RACE_NEXT_RACE       // Countdown to next race
};

// --- Notification Bitmask ---
#define NOTIFY_RACE_WEEK     (1 << 0)
#define NOTIFY_PRE_QUALI     (1 << 1)
#define NOTIFY_PRE_RACE      (1 << 2)
#define NOTIFY_RESULT        (1 << 3)
#define NOTIFY_PRE_SPRINT    (1 << 4)
#define NOTIFY_PRE_SPRINT_Q  (1 << 5)

// --- Data Structures ---

struct SessionInfo {
    SessionType type;
    time_t utcTime;
    char label[22];       // "Sprint Qualifying" is longest
    char dayAbbrev[4];    // "Fri", "Sat", "Sun"
    char localTime[6];    // "12:30"
};

struct RaceData {
    char name[32];
    char location[32];
    uint8_t round;
    bool isSprint;
    uint8_t sessionCount;
    SessionInfo sessions[MAX_SESSIONS];
    time_t firstSessionUtc;
    time_t gpTimeUtc;
};

struct RaceResult {
    char driverCode[4];
    char driverName[32];
    char constructor[24];
    uint8_t position;
};

struct StandingEntry {
    char code[4];         // Driver code or constructor short
    char name[32];        // Full name
    uint16_t points;
    uint8_t position;
    uint8_t wins;
};

struct AppConfig {
    char timezone[48];
    char ntpServer[64];
    char botToken[64];
    char chatId[16];
    uint8_t brightness;           // 0=auto, 1-255=manual
    uint8_t lastNotifiedRound;
    uint8_t notificationBits;     // Bitmask of sent notifications for current round
    bool telegramEnabled;
};

// --- Default Config ---
inline void setDefaultConfig(AppConfig& cfg) {
    strlcpy(cfg.timezone, "Europe/London", sizeof(cfg.timezone));
    strlcpy(cfg.ntpServer, "pool.ntp.org", sizeof(cfg.ntpServer));
    cfg.botToken[0] = '\0';
    cfg.chatId[0] = '\0';
    cfg.brightness = 128;
    cfg.lastNotifiedRound = 0;
    cfg.notificationBits = 0;
    cfg.telegramEnabled = false;
}
