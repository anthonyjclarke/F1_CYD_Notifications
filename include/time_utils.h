#pragma once

#include <ezTime.h>
#include "config.h"

static Timezone myTZ;

// Initialize NTP and timezone
bool initTime(const char* tzString, const char* ntpServer) {
    setServer(ntpServer);
    setInterval(3600);  // Re-sync every hour

    Serial.printf("[Time] Syncing NTP via %s...\n", ntpServer);
    if (!waitForSync(15)) {
        Serial.println(F("[Time] NTP sync failed"));
        return false;
    }
    Serial.println(F("[Time] NTP synced"));

    if (!myTZ.setLocation(tzString)) {
        Serial.printf("[Time] Invalid timezone: %s, falling back to UTC\n", tzString);
        myTZ.setLocation("UTC");
        return false;
    }
    Serial.printf("[Time] Timezone set: %s\n", tzString);
    return true;
}

// Get current UTC time as time_t
time_t nowUTC() {
    return UTC.now();
}

// Format UTC epoch to local day abbreviation ("Fri")
void formatLocalDay(time_t utc, char* buf, size_t len) {
    tmElements_t tm;
    breakTime(myTZ.tzTime(utc), tm);
    const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    strlcpy(buf, days[tm.Wday - 1], len);
}

// Format UTC epoch to local time string ("12:30")
void formatLocalTime(time_t utc, char* buf, size_t len) {
    tmElements_t tm;
    breakTime(myTZ.tzTime(utc), tm);
    snprintf(buf, len, "%02d:%02d", tm.Hour, tm.Minute);
}

// Format UTC epoch to full local date string ("Sunday, 08 Mar 2026 - 15:00")
void formatLocalFullDate(time_t utc, char* buf, size_t len) {
    tmElements_t tm;
    breakTime(myTZ.tzTime(utc), tm);
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

Countdown getCountdown(time_t targetUtc) {
    Countdown cd = {0, 0, 0, 0, false};
    time_t now = nowUTC();
    if (targetUtc <= now) {
        cd.expired = true;
        return cd;
    }
    time_t diff = targetUtc - now;
    cd.days    = diff / 86400;
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
