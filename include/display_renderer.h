#pragma once

#include <TFT_eSPI.h>
#include "config.h"
#include "types.h"
#include "time_utils.h"

static TFT_eSPI tft = TFT_eSPI();

// --- Initialization ---

void initDisplay() {
    tft.init();
    tft.setRotation(TFT_ROTATION);
    tft.fillScreen(COLOR_BG);

    // Backlight PWM
    ledcSetup(BL_PWM_CHANNEL, BL_PWM_FREQ, BL_PWM_RES);
    ledcAttachPin(PIN_BL, BL_PWM_CHANNEL);
    ledcWrite(BL_PWM_CHANNEL, 128);  // Default brightness
}

void setBacklight(uint8_t brightness) {
    ledcWrite(BL_PWM_CHANNEL, brightness);
}

// --- Helper: Draw centered text ---

void drawCenteredText(const char* text, int y, const GFXfont* font, uint16_t color) {
    tft.setFreeFont(font);
    tft.setTextColor(color, COLOR_BG);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(text, SCREEN_WIDTH / 2, y);
}

// --- Splash Screen ---

void drawSplashScreen() {
    tft.fillScreen(COLOR_BG);

    // F1 red header bar
    tft.fillRect(0, 0, SCREEN_WIDTH, 50, COLOR_F1_RED);
    tft.setTextColor(COLOR_HEADER_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSansBold18pt7b);
    tft.drawString("F1", SCREEN_WIDTH / 2, 28);

    tft.setTextColor(COLOR_TEXT);
    tft.setFreeFont(&FreeSans12pt7b);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Race Schedule", SCREEN_WIDTH / 2, 90);

    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(COLOR_SESSION_TEXT);
    tft.drawString("Connecting to WiFi...", SCREEN_WIDTH / 2, 140);
    tft.drawString("v1.0", SCREEN_WIDTH / 2, 220);
}

// --- Schedule Table ---

void drawScheduleTable(RaceData& race) {
    tft.fillScreen(COLOR_BG);

    // Header bar - "F1 RACE SCHEDULE"
    tft.fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_F1_RED);
    tft.setTextColor(COLOR_HEADER_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSansBold9pt7b);
    tft.drawString("F1 RACE SCHEDULE", SCREEN_WIDTH / 2, 13);

    // Race date line
    char dateStr[48];
    formatLocalFullDate(race.gpTimeUtc, dateStr, sizeof(dateStr));
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(COLOR_SESSION_TEXT);
    tft.drawString(dateStr, SCREEN_WIDTH / 2, 38);

    // Divider
    tft.drawFastHLine(0, 52, SCREEN_WIDTH, COLOR_GRID);

    // Race name
    char raceLabel[48];
    snprintf(raceLabel, sizeof(raceLabel), "Next Race: %s", race.name);
    tft.setTextColor(COLOR_TEXT);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(raceLabel, 8, 66);

    // Table header line
    tft.drawFastHLine(0, 80, SCREEN_WIDTH, COLOR_GRID);

    // Column positions
    constexpr int colSession = 8;
    constexpr int colDay     = 200;
    constexpr int colTime    = 265;
    constexpr int headerY    = 92;

    tft.setFreeFont(&FreeSansBold9pt7b);
    tft.setTextColor(COLOR_HIGHLIGHT);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("SESSION", colSession, headerY);
    tft.drawString("DAY", colDay, headerY);
    tft.drawString("TIME", colTime, headerY);

    // Header underline
    tft.drawFastHLine(0, 104, SCREEN_WIDTH, COLOR_GRID);

    // Column dividers
    tft.drawFastVLine(195, 80, SCREEN_HEIGHT - 80, COLOR_GRID);
    tft.drawFastVLine(258, 80, SCREEN_HEIGHT - 80, COLOR_GRID);

    // Session rows
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(COLOR_TEXT);
    int y = 118;
    constexpr int rowHeight = 24;

    for (uint8_t i = 0; i < race.sessionCount && i < MAX_SESSIONS; i++) {
        SessionInfo& s = race.sessions[i];

        // Highlight race/sprint row
        uint16_t textColor = COLOR_TEXT;
        if (s.type == SESSION_GP) {
            textColor = COLOR_F1_RED;
            tft.setTextColor(textColor);
        } else if (s.type == SESSION_SPRINT) {
            textColor = COLOR_HIGHLIGHT;
            tft.setTextColor(textColor);
        } else {
            tft.setTextColor(COLOR_TEXT);
        }

        tft.setTextDatum(ML_DATUM);
        tft.drawString(s.label, colSession, y);
        tft.drawString(s.dayAbbrev, colDay, y);
        tft.drawString(s.localTime, colTime, y);

        y += rowHeight;

        // Row separator
        if (i < race.sessionCount - 1) {
            tft.drawFastHLine(0, y - rowHeight / 2 + 1, SCREEN_WIDTH, COLOR_GRID);
        }
    }
}

// --- Countdown Display ---

void drawCountdown(RaceData& race, Countdown& cd) {
    tft.fillScreen(COLOR_BG);

    // Header
    tft.fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_F1_RED);
    tft.setTextColor(COLOR_HEADER_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSansBold9pt7b);

    if (cd.days > 0) {
        tft.drawString("RACE WEEK COUNTDOWN", SCREEN_WIDTH / 2, 13);
    } else {
        tft.drawString("SESSION STARTING SOON", SCREEN_WIDTH / 2, 13);
    }

    // Race name
    tft.setFreeFont(&FreeSans12pt7b);
    tft.setTextColor(COLOR_TEXT);
    tft.drawString(race.name, SCREEN_WIDTH / 2, 50);
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(COLOR_SESSION_TEXT);
    tft.drawString(race.location, SCREEN_WIDTH / 2, 72);

    // Large countdown digits
    char countBuf[16];
    if (cd.days > 0) {
        // Show days prominently
        snprintf(countBuf, sizeof(countBuf), "%d", cd.days);
        tft.setFreeFont(&FreeMonoBold24pt7b);
        tft.setTextColor(COLOR_COUNTDOWN);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(countBuf, SCREEN_WIDTH / 2, 130);

        tft.setFreeFont(&FreeSans12pt7b);
        tft.setTextColor(COLOR_SESSION_TEXT);
        tft.drawString(cd.days == 1 ? "DAY" : "DAYS", SCREEN_WIDTH / 2, 168);
    } else {
        // Show hours:minutes:seconds
        snprintf(countBuf, sizeof(countBuf), "%02d:%02d:%02d", cd.hours, cd.minutes, cd.seconds);
        tft.setFreeFont(&FreeMonoBold24pt7b);
        tft.setTextColor(COLOR_COUNTDOWN);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(countBuf, SCREEN_WIDTH / 2, 138);
    }

    // Next session info at bottom
    if (race.sessionCount > 0) {
        // Find next upcoming session
        time_t now = nowUTC();
        for (uint8_t i = 0; i < race.sessionCount; i++) {
            if (race.sessions[i].utcTime > now) {
                char sessionBuf[48];
                snprintf(sessionBuf, sizeof(sessionBuf), "%s - %s %s",
                         race.sessions[i].label, race.sessions[i].dayAbbrev,
                         race.sessions[i].localTime);
                tft.setFreeFont(&FreeSans9pt7b);
                tft.setTextColor(COLOR_SESSION_TEXT);
                tft.setTextDatum(MC_DATUM);
                tft.drawString(sessionBuf, SCREEN_WIDTH / 2, 210);
                break;
            }
        }
    }
}

// --- Track Layout Display ---

void drawTrackLayout(const uint8_t* xbmData, uint16_t w, uint16_t h, RaceData& race) {
    tft.fillScreen(COLOR_BG);

    // Header
    tft.fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_F1_RED);
    tft.setTextColor(COLOR_HEADER_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSansBold9pt7b);
    tft.drawString("CIRCUIT LAYOUT", SCREEN_WIDTH / 2, 13);

    // Race name
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(COLOR_TEXT);
    tft.drawString(race.name, SCREEN_WIDTH / 2, 38);
    tft.setTextColor(COLOR_SESSION_TEXT);
    tft.drawString(race.location, SCREEN_WIDTH / 2, 55);

    // Draw XBM track image centered
    if (xbmData) {
        int x = (SCREEN_WIDTH - w) / 2;
        int y = 65 + (SCREEN_HEIGHT - 65 - h) / 2;
        tft.drawXBitmap(x, y, xbmData, w, h, COLOR_TRACK);
    } else {
        tft.setTextColor(COLOR_GRID);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Track image not available", SCREEN_WIDTH / 2, 150);
    }
}

// --- Post-Race: Winner Display ---

void drawRaceWinner(RaceData& race) {
    tft.fillScreen(COLOR_BG);

    // Header
    tft.fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_F1_RED);
    tft.setTextColor(COLOR_HEADER_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSansBold9pt7b);
    tft.drawString("RACE RESULT", SCREEN_WIDTH / 2, 13);

    // Race name
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(COLOR_SESSION_TEXT);
    tft.drawString(race.name, SCREEN_WIDTH / 2, 40);

    if (podiumCount == 0) {
        tft.setTextColor(COLOR_GRID);
        tft.drawString("Results pending...", SCREEN_WIDTH / 2, 120);
        return;
    }

    // Podium positions
    uint16_t colors[] = {COLOR_PODIUM_GOLD, COLOR_PODIUM_SILVER, COLOR_PODIUM_BRONZE};
    const char* posLabels[] = {"1st", "2nd", "3rd"};
    int yPos[] = {75, 135, 175};

    for (uint8_t i = 0; i < podiumCount; i++) {
        // Position badge
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(colors[i]);
        tft.setFreeFont(i == 0 ? &FreeSansBold12pt7b : &FreeSansBold9pt7b);
        tft.drawString(posLabels[i], 15, yPos[i]);

        // Driver name
        tft.setTextColor(COLOR_TEXT);
        tft.setFreeFont(i == 0 ? &FreeSans12pt7b : &FreeSans9pt7b);
        tft.drawString(podium[i].driverName, 70, yPos[i]);

        // Constructor
        tft.setTextColor(COLOR_SESSION_TEXT);
        tft.setFreeFont(&FreeSans9pt7b);
        tft.drawString(podium[i].constructor, 70, yPos[i] + (i == 0 ? 24 : 20));
    }
}

// --- Post-Race: Driver Standings ---

void drawDriverStandings() {
    tft.fillScreen(COLOR_BG);

    // Header
    tft.fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_F1_RED);
    tft.setTextColor(COLOR_HEADER_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSansBold9pt7b);
    tft.drawString("DRIVER STANDINGS", SCREEN_WIDTH / 2, 13);

    if (driverStandingsCount == 0) {
        tft.setTextColor(COLOR_GRID);
        tft.setFreeFont(&FreeSans9pt7b);
        tft.drawString("Standings pending...", SCREEN_WIDTH / 2, 120);
        return;
    }

    // Column headers
    int y = 36;
    tft.setFreeFont(&FreeSansBold9pt7b);
    tft.setTextColor(COLOR_HIGHLIGHT);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("POS", 8, y);
    tft.drawString("DRIVER", 50, y);
    tft.drawString("PTS", 270, y);
    tft.drawFastHLine(0, y + 10, SCREEN_WIDTH, COLOR_GRID);

    // Rows
    tft.setFreeFont(&FreeSans9pt7b);
    y = 60;
    constexpr int rowH = 19;

    for (uint8_t i = 0; i < driverStandingsCount; i++) {
        StandingEntry& e = driverStandings[i];

        // Highlight top 3
        uint16_t color = COLOR_TEXT;
        if (e.position == 1) color = COLOR_PODIUM_GOLD;
        else if (e.position == 2) color = COLOR_PODIUM_SILVER;
        else if (e.position == 3) color = COLOR_PODIUM_BRONZE;

        char posBuf[4];
        snprintf(posBuf, sizeof(posBuf), "%d", e.position);
        char ptsBuf[8];
        snprintf(ptsBuf, sizeof(ptsBuf), "%d", e.points);

        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(color);
        tft.drawString(posBuf, 15, y);
        tft.drawString(e.code, 50, y);
        tft.setTextColor(COLOR_SESSION_TEXT);
        tft.drawString(ptsBuf, 270, y);

        y += rowH;
    }
}

// --- Post-Race: Constructor Standings ---

void drawConstructorStandings() {
    tft.fillScreen(COLOR_BG);

    // Header
    tft.fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_F1_RED);
    tft.setTextColor(COLOR_HEADER_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSansBold9pt7b);
    tft.drawString("CONSTRUCTOR STANDINGS", SCREEN_WIDTH / 2, 13);

    if (constructorStandingsCount == 0) {
        tft.setTextColor(COLOR_GRID);
        tft.setFreeFont(&FreeSans9pt7b);
        tft.drawString("Standings pending...", SCREEN_WIDTH / 2, 120);
        return;
    }

    // Column headers
    int y = 36;
    tft.setFreeFont(&FreeSansBold9pt7b);
    tft.setTextColor(COLOR_HIGHLIGHT);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("POS", 8, y);
    tft.drawString("TEAM", 50, y);
    tft.drawString("PTS", 270, y);
    tft.drawFastHLine(0, y + 10, SCREEN_WIDTH, COLOR_GRID);

    // Rows
    tft.setFreeFont(&FreeSans9pt7b);
    y = 60;
    constexpr int rowH = 19;

    for (uint8_t i = 0; i < constructorStandingsCount; i++) {
        StandingEntry& e = constructorStandings[i];

        uint16_t color = COLOR_TEXT;
        if (e.position == 1) color = COLOR_PODIUM_GOLD;
        else if (e.position == 2) color = COLOR_PODIUM_SILVER;
        else if (e.position == 3) color = COLOR_PODIUM_BRONZE;

        char posBuf[4];
        snprintf(posBuf, sizeof(posBuf), "%d", e.position);
        char ptsBuf[8];
        snprintf(ptsBuf, sizeof(ptsBuf), "%d", e.points);

        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(color);
        tft.drawString(posBuf, 15, y);
        tft.drawString(e.name, 50, y);
        tft.setTextColor(COLOR_SESSION_TEXT);
        tft.drawString(ptsBuf, 270, y);

        y += rowH;
    }
}

// --- Status message overlay ---

void drawStatusMessage(const char* msg) {
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(COLOR_SESSION_TEXT, COLOR_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(msg, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
}

// --- Update brightness from config or LDR ---

void updateBrightness(uint8_t configBrightness) {
    if (configBrightness == 0) {
        // Auto mode - read LDR
        uint16_t ldr = analogRead(PIN_LDR);
        uint8_t mapped = map(ldr, 0, 4095, 20, 255);
        setBacklight(mapped);
    } else {
        setBacklight(configBrightness);
    }
}
