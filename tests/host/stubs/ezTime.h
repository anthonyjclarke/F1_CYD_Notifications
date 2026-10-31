#pragma once
#include "Arduino.h"
#include <ctime>

static time_t fakeUtc = 1793491200;
static long fakeLocalOffset = 0;
constexpr int UTC_TIME = 1;
struct tmElements_t { int Year, Month, Day, Hour, Minute, Second, Wday; };
inline time_t now() { return fakeUtc; }
inline void breakTime(time_t value, tmElements_t& out) {
    std::tm t = *std::gmtime(&value);
    out = {t.tm_year - 70, t.tm_mon + 1, t.tm_mday, t.tm_hour,
           t.tm_min, t.tm_sec, t.tm_wday + 1};
}
class Timezone {
public:
    time_t tzTime(time_t value, int) { return value + fakeLocalOffset; }
    int weekday() { tmElements_t t; breakTime(tzTime(now(), UTC_TIME), t); return t.Wday; }
    void setPosix(const char*) {}
    bool setLocation(const char*) { return true; }
};
inline time_t lastNtpUpdateTime() { return 0; }
inline void setServer(const char*) {}
inline void setInterval(int) {}
inline void events() {}
inline bool waitForSync(int) { return false; }
inline void setTime(time_t value) { fakeUtc = value; }
inline std::string errorString() { return "stub"; }
