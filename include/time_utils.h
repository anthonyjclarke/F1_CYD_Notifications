#pragma once

#include <ezTime.h>
#include "config.h"
#include "debug.h"

static Timezone myTZ;

// Initialize NTP and timezone
bool initTime(const char* tzString, const char* ntpServer) {
    setServer(ntpServer);
    setInterval(3600);  // Re-sync every hour

    // Always apply timezone, even if initial NTP sync times out.
    // This ensures local date math is correct as soon as time becomes valid.
    if (!myTZ.setLocation(tzString)) {
        DBG_WARN("[Time] Invalid timezone '%s', falling back to UTC", tzString);
        myTZ.setLocation("UTC");
    } else {
        DBG_INFO("[Time] Timezone set: %s", tzString);
    }

    DBG_INFO("[Time] Syncing NTP via %s", ntpServer);
    if (!waitForSync(15)) {
        DBG_WARN("[Time] NTP sync timed out (timezone already applied)");
        return false;
    }
    DBG_INFO("[Time] NTP synced");
    DBG_INFO("[Time] Epoch check: now()=%ld UTC.now()=%ld", (long)::now(), (long)UTC.now());
    return true;
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
    Countdown cd = {0, 0, 0, 0, false};
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
