#pragma once

#include <TFT_eSPI.h>
#include "config.h"
#include "f1_race_car.h"

// Compose one scanline at a time: 640 bytes of scratch RAM, no heap allocation.
// Each row includes its background, so moving pixels leave no trails and there
// is no separate erase pass flashing black between frames.
class RaceAnimation {
public:
    void start(uint32_t nowMs) {
        active = true;
        startedMs = nowMs;
        lastFrameMs = nowMs - ON_NOW_FRAME_MS;
    }

    void stop() { active = false; }
    bool isActive() const { return active; }

    // True exactly once, after the car AND its trailing smoke have left.
    bool update(TFT_eSPI& display, uint32_t nowMs) {
        if (!active) return false;
        uint32_t elapsed = nowMs - startedMs;
        if (elapsed >= ON_NOW_ANIMATION_MS) {
            active = false;
            return true;
        }
        if (nowMs - lastFrameMs < ON_NOW_FRAME_MS) return false;
        lastFrameMs = nowMs;
        drawFrame(display, elapsed);
        return false;
    }

private:
    bool active = false;
    uint32_t startedMs = 0;
    uint32_t lastFrameMs = 0;

    void drawFrame(TFT_eSPI& display, uint32_t elapsed) {
        constexpr int carWidth = 70;
        constexpr int carHeight = 28;
        constexpr int stripY = 94;
        constexpr int stripHeight = 56;
        constexpr int carY = 16;  // Relative to the animation strip
        constexpr int roadY = carY + carHeight - 2;
        constexpr int smokeClearance = 128;

        // Gentle acceleration, using fixed point arithmetic without overflow.
        uint32_t progress = elapsed * 1024UL / ON_NOW_ANIMATION_MS;
        int carX = -carWidth + (int)((SCREEN_WIDTH + carWidth + smokeClearance)
                                    * (progress * progress / 1024UL) / 1024UL);
        uint16_t row[SCREEN_WIDTH];
        bool previousSwap = display.getSwapBytes();
        display.setSwapBytes(true);  // The source asset contains native RGB565.
        display.startWrite();

        for (int y = 0; y < stripHeight; ++y) {
            for (int x = 0; x < SCREEN_WIDTH; ++x) row[x] = COLOR_BG;

            // Six expanding, fading puffs behind the rear wheels.
            for (int puff = 5; puff >= 0; --puff) {
                int radius = 3 + puff * 2;
                int centerX = carX - 8 - puff * 16;
                int centerY = carY + 20 - puff * 2
                              + (int)((elapsed / 120 + puff) % 3) - 1;
                int dy = y - centerY;
                if (dy < -radius || dy > radius) continue;
                uint8_t shade = 150 - puff * 22;
                uint16_t color = display.color565(shade, shade, shade);
                for (int dx = -radius; dx <= radius; ++dx) {
                    int x = centerX + dx;
                    if (x >= 0 && x < SCREEN_WIDTH &&
                        dx * dx + dy * dy <= radius * radius) row[x] = color;
                }
            }

            if (y >= carY && y < carY + carHeight) {
                int sourceY = (y - carY) * F1_CAR_HEIGHT / carHeight;
                for (int x = 0; x < carWidth; ++x) {
                    int screenX = carX + x;
                    if (screenX < 0 || screenX >= SCREEN_WIDTH) continue;
                    int sourceX = x * F1_CAR_WIDTH / carWidth;
                    row[screenX] = pgm_read_word(&f1_race_car[sourceY * F1_CAR_WIDTH + sourceX]);
                }
            }
            if (y == roadY) {
                for (int x = 0; x < SCREEN_WIDTH; ++x) row[x] = COLOR_GRID;
            }
            display.pushImage(0, stripY + y, SCREEN_WIDTH, 1, row);
        }

        display.endWrite();
        display.setSwapBytes(previousSwap);
    }
};
