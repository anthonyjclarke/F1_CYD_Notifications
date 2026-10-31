#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#define PROGMEM
#define pgm_read_word(address) (*(address))
static uint32_t fakeMillis = 0;
inline uint32_t millis() { return fakeMillis; }
inline int analogRead(int) { return 2048; }
inline void ledcSetup(int, int, int) {}
inline void ledcAttachPin(int, int) {}
inline void ledcWrite(int, int) {}
inline long map(long value, long low, long high, long outLow, long outHigh) {
    return (value - low) * (outHigh - outLow) / (high - low) + outLow;
}
inline size_t hostStrlcpy(char* dst, const char* src, size_t size) {
    size_t length = std::strlen(src);
    if (size) {
        size_t count = std::min(length, size - 1);
        std::memcpy(dst, src, count);
        dst[count] = '\0';
    }
    return length;
}
#define strlcpy hostStrlcpy
