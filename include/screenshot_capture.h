#pragma once

// =============================================================
// TFT Screenshot Capture Module
//
// Purpose:
//   Capture the current TFT frame and save it as a 24-bit BMP file
//   on a MicroSD card for debug/documentation workflows.
//
// Design Notes:
// - No full-frame buffer allocation (writes one scanline at a time).
// - Trigger model is request-based (safe to queue from web/button and
//   execute in the main loop).
// - Module is self-contained and portable across similar TFT_eSPI projects.
// =============================================================

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <sys/utime.h>
#include "config.h"
#include "debug.h"
#include "time_utils.h"
#include "display_renderer.h"

static SPIClass screenshotSdSpi(HSPI);
static bool screenshotSdReady = false;
static volatile uint8_t screenshotPendingCount = 0;
static bool screenshotBusy = false;
static int8_t screenshotButtonPin = -1;
static bool screenshotPrevButtonPressed = false;
static unsigned long screenshotLastButtonMs = 0;
static char screenshotPendingReason[16] = "manual";
static char screenshotLastPath[64] = "";
static char screenshotLastError[64] = "";
static unsigned long screenshotSeq = 0;
static const uint8_t SCREENSHOT_CHUNK_ROWS = 16;

// RAM capture state (used when no SD card is present)
static uint8_t* screenshotRamBuf = nullptr;
static size_t screenshotRamBufSize = 0;
static bool screenshotRamReady = false;
static volatile uint8_t screenshotRamPending = 0;

static void writeLE16(File& f, uint16_t v) {
    f.write((uint8_t)(v & 0xFF));
    f.write((uint8_t)((v >> 8) & 0xFF));
}

static void writeLE32(File& f, uint32_t v) {
    f.write((uint8_t)(v & 0xFF));
    f.write((uint8_t)((v >> 8) & 0xFF));
    f.write((uint8_t)((v >> 16) & 0xFF));
    f.write((uint8_t)((v >> 24) & 0xFF));
}

// Buffer variants for RAM capture (write into a uint8_t* and advance pointer)
static void writeBufLE16(uint8_t*& p, uint16_t v) {
    *p++ = v & 0xFF;
    *p++ = (v >> 8) & 0xFF;
}

static void writeBufLE32(uint8_t*& p, uint32_t v) {
    *p++ = v & 0xFF;
    *p++ = (v >> 8) & 0xFF;
    *p++ = (v >> 16) & 0xFF;
    *p++ = (v >> 24) & 0xFF;
}

static bool ensureScreenshotDir() {
    if (SD.exists(SCREENSHOT_DIR)) {
        return true;
    }
    if (!SD.mkdir(SCREENSHOT_DIR)) {
        DBG_ERROR("[Shot] Failed to create directory: %s", SCREENSHOT_DIR);
        return false;
    }
    return true;
}

static void applyFileTimestamp(const char* path) {
    time_t utc = nowUTC();
    if (utc <= 1000000000 || !path || path[0] == '\0') {
        return;
    }
    // FAT timestamps are timezone-less; apply user-configured local clock.
    time_t ts = myTZ.tzTime(utc, UTC_TIME);
    struct utimbuf tb;
    tb.actime = ts;
    tb.modtime = ts;
    if (utime(path, &tb) == 0) {
        return;
    }

    // SD is mounted under "/sd" by default in Arduino-ESP32 SD library.
    if (path[0] == '/') {
        char vfsPath[96];
        snprintf(vfsPath, sizeof(vfsPath), "/sd%s", path);
        if (utime(vfsPath, &tb) == 0) {
            return;
        }
    }

    DBG_WARN("[Shot] utime failed for %s", path);
}

static void buildScreenshotPath(char* out, size_t len) {
    time_t utc = nowUTC();
    if (utc > 1000000000) {
        tmElements_t tm;
        breakTime(myTZ.tzTime(utc, UTC_TIME), tm);

        // Preferred timestamp format using user-configured local date/time.
        // Example: /shots/shot_20260302_134501.bmp
        for (uint8_t n = 0; n < 100; n++) {
            if (n == 0) {
                snprintf(out, len, "%s/shot_%04d%02d%02d_%02d%02d%02d.bmp",
                         SCREENSHOT_DIR,
                         tm.Year + 1970, tm.Month, tm.Day,
                         tm.Hour, tm.Minute, tm.Second);
            } else {
                snprintf(out, len, "%s/shot_%04d%02d%02d_%02d%02d%02d_%02u.bmp",
                         SCREENSHOT_DIR,
                         tm.Year + 1970, tm.Month, tm.Day,
                         tm.Hour, tm.Minute, tm.Second, n);
            }
            if (!SD.exists(out)) return;
        }
    }

    // Fallback if time isn't synced (or collisions exhausted)
    for (uint16_t attempt = 0; attempt < 1000; attempt++) {
        screenshotSeq = (screenshotSeq + 1) % 1000000UL;
        snprintf(out, len, "%s/shot_unsynced_%06lu.bmp", SCREENSHOT_DIR, screenshotSeq);
        if (!SD.exists(out)) return;
    }
    snprintf(out, len, "%s/shot_unsynced_%06lu.bmp", SCREENSHOT_DIR, millis() % 1000000UL);
}

// Initialize SD-backed screenshot capture.
// buttonPin:
//   -1 disables physical trigger
//   >=0 uses INPUT_PULLUP, active LOW (press to capture)
bool initScreenshotCapture(uint8_t sdCsPin, int8_t buttonPin = -1) {
    // Use conservative SD clock for stability across CYD variants/cards.
    screenshotSdSpi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, sdCsPin);
    if (!SD.begin(sdCsPin, screenshotSdSpi, 8000000)) {
        DBG_WARN("[Shot] SD init failed on CS=%d", sdCsPin);
        screenshotSdReady = false;
        return false;
    }

    if (!ensureScreenshotDir()) {
        screenshotSdReady = false;
        return false;
    }

    screenshotSdReady = true;
    screenshotButtonPin = buttonPin;

    if (screenshotButtonPin >= 0) {
        pinMode(screenshotButtonPin, INPUT_PULLUP);
        screenshotPrevButtonPressed = (digitalRead(screenshotButtonPin) == LOW);
        DBG_INFO("[Shot] Button trigger enabled on GPIO%d", screenshotButtonPin);
    }

    DBG_INFO("[Shot] Screenshot capture ready (CS=%d SCK=%d MISO=%d MOSI=%d)",
             sdCsPin, PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI);
    return true;
}

bool isScreenshotReady() {
    return screenshotSdReady;
}

bool isScreenshotBusy() {
    return screenshotBusy;
}

const char* getLastScreenshotPath() {
    return screenshotLastPath;
}

const char* getLastScreenshotError() {
    return screenshotLastError;
}

bool isScreenshotRamReady() {
    return screenshotRamReady;
}

const uint8_t* getScreenshotRamBuf() {
    return screenshotRamBuf;
}

size_t getScreenshotRamSize() {
    return screenshotRamBufSize;
}

void clearScreenshotRam() {
    if (screenshotRamBuf) {
        free(screenshotRamBuf);
        screenshotRamBuf = nullptr;
    }
    screenshotRamBufSize = 0;
    screenshotRamReady = false;
}

// Queue a RAM-backed screenshot capture (used when no SD card is available).
// Returns false immediately if there is insufficient heap.
bool requestScreenshotToRam() {
    constexpr uint32_t needed = SCREEN_WIDTH * SCREEN_HEIGHT * 3UL + 54
                                + (size_t)SCREEN_WIDTH * SCREENSHOT_CHUNK_ROWS * 2;  // chunk565 buffer
    if (ESP.getFreeHeap() < needed + 20480) {
        snprintf(screenshotLastError, sizeof(screenshotLastError),
                 "low heap (%u B)", (unsigned)ESP.getFreeHeap());
        DBG_WARN("[Shot] RAM capture refused: need ~%u B, have %u B",
                 needed + 20480, (unsigned)ESP.getFreeHeap());
        return false;
    }
    if (screenshotRamPending < 255) screenshotRamPending++;
    DBG_INFO("[Shot] RAM capture queued, pending=%u", screenshotRamPending);
    return true;
}

// Capture the full TFT into a heap-allocated 24-bit BMP.
// Must be called from the main loop (same context as all other TFT operations).
static bool captureScreenshotToRam() {
    if (screenshotBusy) {
        DBG_WARN("[Shot] RAM capture skipped: busy");
        return false;
    }
    screenshotBusy = true;

    // Release any previous buffer before allocating the new one
    clearScreenshotRam();

    constexpr uint16_t w = SCREEN_WIDTH;
    constexpr uint16_t h = SCREEN_HEIGHT;
    constexpr uint32_t rowBytes = w * 3UL;
    constexpr uint32_t rowPad = (4 - (rowBytes % 4)) % 4;
    constexpr uint32_t pixelDataSize = (rowBytes + rowPad) * h;
    constexpr uint32_t totalSize = 54 + pixelDataSize;

    screenshotRamBuf = (uint8_t*)malloc(totalSize);
    if (!screenshotRamBuf) {
        snprintf(screenshotLastError, sizeof(screenshotLastError),
                 "malloc failed (%lu B)", totalSize);
        DBG_ERROR("[Shot] RAM capture: %s", screenshotLastError);
        screenshotBusy = false;
        return false;
    }
    screenshotRamBufSize = totalSize;

    // Write BMP file header + DIB header (54 bytes total)
    uint8_t* p = screenshotRamBuf;
    *p++ = 'B'; *p++ = 'M';
    writeBufLE32(p, totalSize);
    writeBufLE16(p, 0); writeBufLE16(p, 0);  // reserved
    writeBufLE32(p, 54);                       // pixel data offset
    writeBufLE32(p, 40);                       // DIB header size
    writeBufLE32(p, w);
    writeBufLE32(p, h);                        // positive = bottom-up
    writeBufLE16(p, 1);                        // planes
    writeBufLE16(p, 24);                       // 24-bit RGB
    writeBufLE32(p, 0);                        // BI_RGB (no compression)
    writeBufLE32(p, pixelDataSize);
    writeBufLE32(p, 2835); writeBufLE32(p, 2835);  // 72 DPI
    writeBufLE32(p, 0); writeBufLE32(p, 0);

    // Chunk buffer for TFT readRect
    uint16_t* chunk565 = (uint16_t*)malloc((size_t)w * SCREENSHOT_CHUNK_ROWS * sizeof(uint16_t));
    if (!chunk565) {
        snprintf(screenshotLastError, sizeof(screenshotLastError), "chunk alloc failed");
        DBG_ERROR("[Shot] RAM capture: %s", screenshotLastError);
        clearScreenshotRam();
        screenshotBusy = false;
        return false;
    }

    const uint8_t pad[3] = {0, 0, 0};

    // BMP rows are bottom-up: capture from screen bottom, write sequentially
    for (int yTop = h - 1; yTop >= 0; yTop -= SCREENSHOT_CHUNK_ROWS) {
        uint16_t rows = (yTop + 1 >= SCREENSHOT_CHUNK_ROWS) ? SCREENSHOT_CHUNK_ROWS : (uint16_t)(yTop + 1);
        int firstY = yTop - rows + 1;

        tft.readRect(0, firstY, w, rows, chunk565);

        // Write strip bottom->top to preserve BMP bottom-up order
        for (int ry = rows - 1; ry >= 0; ry--) {
            const uint16_t* srcRow = &chunk565[(size_t)ry * w];
            for (uint16_t x = 0; x < w; x++) {
                uint16_t px = srcRow[x];
                // TFT_eSPI readback is byte-swapped RGB565 on ESP32.
                px = (uint16_t)((px << 8) | (px >> 8));
                *p++ = (uint8_t)((px & 0x1F) * 255 / 31);          // B
                *p++ = (uint8_t)(((px >> 5) & 0x3F) * 255 / 63);   // G
                *p++ = (uint8_t)(((px >> 11) & 0x1F) * 255 / 31);  // R
            }
            if (rowPad) memcpy(p, pad, rowPad), p += rowPad;
        }
        yield();
    }

    free(chunk565);
    screenshotRamReady = true;
    screenshotLastError[0] = '\0';
    DBG_INFO("[Shot] RAM capture complete (%lu bytes)", totalSize);
    screenshotBusy = false;
    return true;
}

// Queue a screenshot to be captured in the main loop.
bool requestScreenshot(const char* reason = "manual") {
    if (!screenshotSdReady) {
        DBG_WARN("[Shot] Request ignored: SD not ready");
        return false;
    }
    strlcpy(screenshotPendingReason, reason ? reason : "manual", sizeof(screenshotPendingReason));
    if (screenshotPendingCount < 255) {
        screenshotPendingCount++;
    }
    DBG_INFO("[Shot] Capture requested (%s), pending=%u", screenshotPendingReason, screenshotPendingCount);
    return true;
}

// Capture the full TFT into a BMP file.
// Returns true on successful write.
bool captureScreenshotNow(const char* reason = "manual") {
    if (!screenshotSdReady) {
        DBG_WARN("[Shot] Capture failed: SD not ready");
        return false;
    }
    if (screenshotBusy) {
        DBG_WARN("[Shot] Capture skipped: already busy");
        return false;
    }
    screenshotBusy = true;

    constexpr uint16_t w = SCREEN_WIDTH;
    constexpr uint16_t h = SCREEN_HEIGHT;
    constexpr uint32_t rowBytes = w * 3UL;
    constexpr uint32_t rowPad = (4 - (rowBytes % 4)) % 4;
    constexpr uint32_t pixelDataSize = (rowBytes + rowPad) * h;
    constexpr uint32_t fileSize = 54 + pixelDataSize;

    if (!ensureScreenshotDir()) {
        // SD operation failed — card may have been removed; disable SD path so
        // the next request automatically falls back to RAM capture.
        screenshotSdReady = false;
        screenshotBusy = false;
        return false;
    }

    char path[64];
    buildScreenshotPath(path, sizeof(path));

    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        snprintf(screenshotLastError, sizeof(screenshotLastError), "open failed: %s", path);
        DBG_ERROR("[Shot] Cannot open %s for write", path);
        screenshotSdReady = false;  // Mark SD not ready; next request uses RAM path
        screenshotBusy = false;
        return false;
    }

    // BMP header (BITMAPFILEHEADER + BITMAPINFOHEADER)
    f.write('B');
    f.write('M');
    writeLE32(f, fileSize);
    writeLE16(f, 0);
    writeLE16(f, 0);
    writeLE32(f, 54);

    writeLE32(f, 40);         // DIB header size
    writeLE32(f, w);
    writeLE32(f, h);          // Positive: bottom-up rows
    writeLE16(f, 1);          // planes
    writeLE16(f, 24);         // 24-bit RGB
    writeLE32(f, 0);          // BI_RGB (no compression)
    writeLE32(f, pixelDataSize);
    writeLE32(f, 2835);       // 72 DPI
    writeLE32(f, 2835);
    writeLE32(f, 0);
    writeLE32(f, 0);

    // Use chunked capture to keep RAM usage low while limiting TFT/SD bus switches.
    const uint16_t maxChunk = SCREENSHOT_CHUNK_ROWS;
    uint16_t* chunk565 = (uint16_t*)malloc((size_t)w * maxChunk * sizeof(uint16_t));
    if (!chunk565) {
        snprintf(screenshotLastError, sizeof(screenshotLastError), "alloc failed (%u rows)", maxChunk);
        DBG_ERROR("[Shot] Chunk buffer allocation failed");
        f.close();
        screenshotBusy = false;
        return false;
    }
    uint8_t line[rowBytes];
    uint8_t pad[3] = {0, 0, 0};

    // BMP rows are bottom-up. Capture and write in strips.
    for (int yTop = h - 1; yTop >= 0; yTop -= maxChunk) {
        uint16_t rows = (yTop + 1 >= maxChunk) ? maxChunk : (uint16_t)(yTop + 1);
        int firstY = yTop - rows + 1;

        tft.readRect(0, firstY, w, rows, chunk565);

        // Write strip bottom->top to preserve BMP bottom-up order.
        for (int ry = rows - 1; ry >= 0; ry--) {
            const uint16_t* srcRow = &chunk565[(size_t)ry * w];

            for (uint16_t x = 0; x < w; x++) {
                uint16_t px = srcRow[x];
                // TFT_eSPI readback can be byte-swapped RGB565 on ESP32.
                px = (uint16_t)((px << 8) | (px >> 8));
                uint8_t r = (uint8_t)(((px >> 11) & 0x1F) * 255 / 31);
                uint8_t g = (uint8_t)(((px >> 5)  & 0x3F) * 255 / 63);
                uint8_t b = (uint8_t)((px & 0x1F) * 255 / 31);
                uint32_t o = x * 3UL;
                line[o]     = b;
                line[o + 1] = g;
                line[o + 2] = r;
            }

            if (f.write(line, rowBytes) != rowBytes) {
                snprintf(screenshotLastError, sizeof(screenshotLastError), "write failed at row %d", firstY + ry);
                DBG_ERROR("[Shot] Write error at row %d", firstY + ry);
                free(chunk565);
                f.close();
                screenshotBusy = false;
                return false;
            }
            if (rowPad) {
                f.write(pad, rowPad);
            }
        }

        yield();
    }

    free(chunk565);
    f.close();
    applyFileTimestamp(path);
    strlcpy(screenshotLastPath, path, sizeof(screenshotLastPath));
    screenshotLastError[0] = '\0';
    DBG_INFO("[Shot] Saved (%s): %s", reason ? reason : "manual", screenshotLastPath);
    screenshotBusy = false;
    return true;
}

// Poll optional physical button and queue capture on press edge.
void pollScreenshotButton() {
    if (!screenshotSdReady || screenshotButtonPin < 0) return;

    bool pressed = (digitalRead(screenshotButtonPin) == LOW);
    unsigned long nowMs = millis();
    if (pressed && !screenshotPrevButtonPressed &&
        nowMs - screenshotLastButtonMs >= SCREENSHOT_DEBOUNCE_MS) {
        screenshotLastButtonMs = nowMs;
        requestScreenshot("button");
    }
    screenshotPrevButtonPressed = pressed;
}

// Run this from loop(); executes queued screenshot jobs (SD or RAM).
void handleScreenshotRequests() {
    if (screenshotBusy) return;
    if (screenshotPendingCount > 0) {
        screenshotPendingCount--;
        captureScreenshotNow(screenshotPendingReason);
    } else if (screenshotRamPending > 0) {
        screenshotRamPending--;
        captureScreenshotToRam();
    }
}
