#pragma once

#include <ezTime.h>
#include "config.h"
#include "debug.h"
#include "config_manager.h"
#include "timezone_ntp_options.h"

static Timezone myTZ;
static bool _ntpSyncedOnce = false;  // True once a real NTP UDP response has been received

// Returns true if time has been confirmed by at least one successful NTP sync this session.
// Used to guard LittleFS time saves so a stale fallback epoch isn't re-saved over a good one.
bool ntpHasSynced() { return _ntpSyncedOnce; }

// Fill buf with the POSIX rule for a web UI timezone name. Returns false for
// names not in the table (e.g. typed into the captive portal).
// "GMT+N" / "GMT-N" mean UTC+N / UTC-N as labelled (not the inverted Etc/GMT
// convention): "GMT+10" -> "<+10>-10".
bool posixForTimezone(const char* name, char* buf, size_t len) {
    for (size_t i = 0; i < TIMEZONE_POSIX_COUNT; i++) {
        if (strcmp(name, TIMEZONE_POSIX[i].name) == 0) {
            strlcpy(buf, TIMEZONE_POSIX[i].posix, len);
            return true;
        }
    }
    if (strncmp(name, "GMT", 3) == 0 && (name[3] == '+' || name[3] == '-')) {
        char* end = nullptr;
        long hours = strtol(name + 3, &end, 10);
        if (*end == '\0' && hours >= -12 && hours <= 14) {
            snprintf(buf, len, "<%+03ld>%ld", hours, -hours);
            return true;
        }
    }
    return false;
}

// Apply a timezone from the built-in table (no network). Unknown names fall
// back to ezTime's online lookup (UDP to timezoned.rop.nl, 2s timeout incl.
// DNS, often times out), retried, then UTC.
static constexpr uint8_t TZ_LOOKUP_ATTEMPTS = 3;

void applyTimezone(const char* tzString) {
    char posix[48];
    if (posixForTimezone(tzString, posix, sizeof(posix))) {
        myTZ.setPosix(posix);
        DBG_INFO("[Time] Timezone set: %s (%s)", tzString, posix);
        return;
    }
    for (uint8_t attempt = 1; attempt <= TZ_LOOKUP_ATTEMPTS; attempt++) {
        if (myTZ.setLocation(tzString)) {
            DBG_INFO("[Time] Timezone set online: %s (attempt %u)", tzString, attempt);
            return;
        }
        DBG_WARN("[Time] Timezone lookup %u/%u for '%s' failed: %s", attempt,
                 TZ_LOOKUP_ATTEMPTS, tzString, errorString().c_str());
    }
    DBG_WARN("[Time] Invalid timezone '%s', falling back to UTC", tzString);
    myTZ.setPosix("UTC0");
}

// Initialize NTP and timezone.
// On success: NTP-synced time applied; epoch saved to LittleFS.
// On failure: last saved epoch (if plausible) applied as fallback so date math stays sane.
// The timezone is applied last, once the network has carried NTP traffic.
// Returns true if NTP sync succeeded, false if running on fallback time.
bool initTime(const char* tzString, const char* ntpServer) {
    setServer(ntpServer);
    setInterval(3600);  // Re-sync every hour

    bool synced = false;
    DBG_INFO("[Time] Syncing NTP via %s", ntpServer);
    if (waitForSync(15)) {
        _ntpSyncedOnce = true;
        synced = true;
        DBG_INFO("[Time] NTP synced. now()=%ld", (long)::now());
        saveLastKnownTime(::now());
    } else {
        // NTP timed out — try to restore last known good time from LittleFS so that
        // race schedule parsing doesn't resolve the wrong race due to a bad clock.
        DBG_WARN("[Time] NTP sync timed out");
        time_t saved = loadLastKnownTime();
        if (saved > MIN_PLAUSIBLE_EPOCH) {
            setTime(saved);
            DBG_WARN("[Time] Using saved time as fallback: %ld (~%d days stale)",
                     (long)saved, (int)((millis() / 1000) / 86400));
        } else {
            DBG_ERROR("[Time] No valid fallback time — clock is unreliable");
        }
    }

    applyTimezone(tzString);
    return synced;
}

// Force an NTP re-sync as soon as possible.
// Safe to call after WiFi reconnects; non-blocking (re-sync happens on next events() call).
void resyncNTP() {
    DBG_INFO("[Time] Requesting NTP resync");
    setInterval(1);  // Minimum interval — ezTime will query on next events() call
    events();        // Process immediately; UDP response may arrive on subsequent calls
    setInterval(3600);
    // If sync succeeded synchronously, mark it
    if (timeStatus() == timeSet && ::now() > MIN_PLAUSIBLE_EPOCH) {
        _ntpSyncedOnce = true;
    }
}

// Get current UTC time as time_t
time_t nowUTC() {
    // Use TimeLib epoch directly. This is the canonical NTP-synced UTC epoch
    // and avoids timezone-object side effects.
    return ::now();
}

// Format UTC epoch to local day abbreviation ("Fri")
void formatLocalDay(time_t utc, char* buf, size_t len) {
    tmElements_t tm;
    breakTime(myTZ.tzTime(utc, UTC_TIME), tm);
    const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    strlcpy(buf, days[tm.Wday - 1], len);
}

// Format UTC epoch to local time string ("12:30")
void formatLocalTime(time_t utc, char* buf, size_t len) {
    tmElements_t tm;
    breakTime(myTZ.tzTime(utc, UTC_TIME), tm);
    snprintf(buf, len, "%02d:%02d", tm.Hour, tm.Minute);
}

// Format UTC epoch to full local date string ("Sunday, 08 Mar 2026 - 15:00")
void formatLocalFullDate(time_t utc, char* buf, size_t len) {
    tmElements_t tm;
    breakTime(myTZ.tzTime(utc, UTC_TIME), tm);
    const char* days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    snprintf(buf, len, "%s, %02d %s %04d - %02d:%02d",
             days[tm.Wday - 1], tm.Day, months[tm.Month - 1],
             tm.Year + 1970, tm.Hour, tm.Minute);
}

// Calculate countdown from now to target UTC time
struct Countdown {
    int days;
    int hours;
    int minutes;
    int seconds;
    bool expired;
    bool isOnNow;  // True if target session is currently running
};

// Convert a civil date to a monotonic day number (Gregorian calendar).
static long localCivilToDays(int year, int month, int day) {
    year -= month <= 2;
    const long era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = (unsigned)(year - era * 400);                 // [0, 399]
    const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5
                         + (unsigned)day - 1;                           // [0, 365]
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;        // [0, 146096]
    return era * 146097L + (long)doe;
}

// Calendar-day delta in configured local timezone (target date - now date).
int daysUntilLocalDate(time_t targetUtc) {
    time_t now = nowUTC();

    tmElements_t nowTm;
    tmElements_t targetTm;
    breakTime(myTZ.tzTime(now, UTC_TIME), nowTm);
    breakTime(myTZ.tzTime(targetUtc, UTC_TIME), targetTm);

    long nowDays = localCivilToDays(nowTm.Year + 1970, nowTm.Month, nowTm.Day);
    long targetDays = localCivilToDays(targetTm.Year + 1970, targetTm.Month, targetTm.Day);
    long delta = targetDays - nowDays;

    if (delta < 0) return 0;
    return (int)delta;
}

Countdown getCountdown(time_t targetUtc) {
    Countdown cd = {0, 0, 0, 0, false, false};
    time_t now = nowUTC();
    if (targetUtc <= now) {
        cd.expired = true;
        return cd;
    }
    time_t diff = targetUtc - now;
    cd.days    = daysUntilLocalDate(targetUtc);
    cd.hours   = (diff % 86400) / 3600;
    cd.minutes = (diff % 3600) / 60;
    cd.seconds = diff % 60;
    return cd;
}

// Get session duration in seconds based on session type
uint16_t getSessionDurationSeconds(SessionType type) {
    switch (type) {
        case SESSION_FP1:               return 3600;  // 60 minutes
        case SESSION_FP2:               return 3600;  // 60 minutes
        case SESSION_FP3:               return 3600;  // 60 minutes
        case SESSION_SPRINT_QUALIFYING: return 1800;  // 30 minutes
        case SESSION_SPRINT:            return 5400;  // 90 minutes (conservative)
        case SESSION_QUALIFYING:        return 3600;  // 60 minutes
        case SESSION_GP:                return 7200;  // ~120 minutes (average, actual varies)
        default:                        return 3600;
    }
}

// Get countdown with session "on now" detection
Countdown getCountdownWithSession(time_t targetUtc, SessionType sessionType) {
    Countdown cd = {0, 0, 0, 0, false, false};
    time_t now = nowUTC();
    uint16_t durationSecs = getSessionDurationSeconds(sessionType);
    time_t sessionEndTime = targetUtc + durationSecs;

    // Check if session is currently running
    if (now >= targetUtc && now < sessionEndTime) {
        cd.isOnNow = true;
        cd.expired = false;
        return cd;
    }

    // Not running - return countdown to start
    if (targetUtc <= now) {
        cd.expired = true;
        return cd;
    }

    time_t diff = targetUtc - now;
    cd.days    = daysUntilLocalDate(targetUtc);
    cd.hours   = (diff % 86400) / 3600;
    cd.minutes = (diff % 3600) / 60;
    cd.seconds = diff % 60;
    return cd;
}

// Check if a given UTC time is within N hours from now
bool isWithinHours(time_t targetUtc, int hours) {
    time_t now = nowUTC();
    time_t diff = targetUtc - now;
    return diff > 0 && diff <= (hours * 3600L);
}

// Check if today (local) is a Monday
bool isMonday() {
    return myTZ.weekday() == 2;  // ezTime: 1=Sun, 2=Mon, ...
}

// Get days since a UTC timestamp
int daysSince(time_t utc) {
    time_t now = nowUTC();
    if (now <= utc) return 0;
    return (now - utc) / 86400;
}
