#pragma once
#include "Arduino.h"
#include <cassert>
#include <vector>

#define TFT_BLACK 0x0000
#define TFT_WHITE 0xFFFF
enum { TC_DATUM, MC_DATUM, ML_DATUM };
struct GFXfont {};
static GFXfont FreeSans9pt7b, FreeSans12pt7b, FreeSansBold9pt7b;
static GFXfont FreeSansBold12pt7b, FreeMonoBold24pt7b;

class TFT_eSPI {
public:
    std::vector<uint16_t> pixels = std::vector<uint16_t>(320 * 240, 0);
    std::vector<std::string> labels;
    int fills = 0;
    int imageRows = 0;
    bool swapped = false;
    void init() {}
    void setRotation(int) {}
    void setFreeFont(const GFXfont*) {}
    void setTextDatum(int) {}
    void setTextColor(uint16_t, uint16_t = 0) {}
    void startWrite() {}
    void endWrite() {}
    bool getSwapBytes() const { return swapped; }
    void setSwapBytes(bool value) { swapped = value; }
    int textWidth(const char* text) { return std::strlen(text) * 9; }
    int fontHeight() { return 17; }
    void drawString(const char* text, int, int) { labels.emplace_back(text); }
    void fillScreen(uint16_t color) {
        ++fills;
        std::fill(pixels.begin(), pixels.end(), color);
    }
    void fillRect(int x, int y, int w, int h, uint16_t color) {
        for (int yy = std::max(0, y); yy < std::min(240, y + h); ++yy)
            for (int xx = std::max(0, x); xx < std::min(320, x + w); ++xx)
                pixels[yy * 320 + xx] = color;
    }
    void fillRoundRect(int x, int y, int w, int h, int, uint16_t color) {
        fillRect(x, y, w, h, color);
    }
    void drawFastHLine(int x, int y, int w, uint16_t color) { fillRect(x, y, w, 1, color); }
    void drawFastVLine(int x, int y, int h, uint16_t color) { fillRect(x, y, 1, h, color); }
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }
    void pushImage(int x, int y, int w, int h, const uint16_t* data) {
        assert(x >= 0 && y >= 0 && x + w <= 320 && y + h <= 240);
        imageRows += h;
        for (int yy = 0; yy < h; ++yy)
            for (int xx = 0; xx < w; ++xx)
                pixels[(y + yy) * 320 + x + xx] = data[yy * w + xx];
    }
};
