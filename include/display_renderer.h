#pragma once

#include <TFT_eSPI.h>
#include "config.h"
#include "types.h"
#include "time_utils.h"
#include "f1_logo.h"
#include "f1_race_car.h"

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

    // F1 logo on white card - logo is designed for white background
    constexpr int16_t logoX = (SCREEN_WIDTH - F1_LOGO_WIDTH) / 2;  // 101
    tft.fillRoundRect(logoX - 5, 5, F1_LOGO_WIDTH + 10, F1_LOGO_HEIGHT + 10, 6, TFT_WHITE);
    tft.pushImage(logoX, 10, F1_LOGO_WIDTH, F1_LOGO_HEIGHT, f1_logo);

    // Thin red separator below logo card
    tft.fillRect(0, 83, SCREEN_WIDTH, 3, COLOR_F1_RED);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_TEXT);
    tft.setFreeFont(&FreeSans12pt7b);
    tft.drawString("Race Schedule", SCREEN_WIDTH / 2, 106);

    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(COLOR_SESSION_TEXT);
    tft.drawString("Connecting to WiFi...", SCREEN_WIDTH / 2, 148);
    char verBuf[24];
    snprintf(verBuf, sizeof(verBuf), "v%s", APP_VERSION);
    tft.drawString(verBuf, SCREEN_WIDTH / 2, 220);
}

// --- Schedule Table ---

void drawScheduleTable(RaceData& race) {
    tft.fillScreen(COLOR_BG);

    // Header bar - "F1 RACE SCHEDULE"
    tft.fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_F1_RED);
    tft.setTextColor(COLOR_HEADER_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSansBold9pt7b);
    tft.drawString("F1 RACE SCHEDULE", SCREEN_WIDTH / 2, 12);

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

// Track last-drawn days value to force full redraw on day-boundary transitions
static int16_t _prevCountdownDays = -1;
static bool _prevOnNow = false;

void drawCountdown(RaceData& race, Countdown& cd, bool partialUpdate = false) {
    // Force full redraw when isOnNow state changes
    if (cd.isOnNow != _prevOnNow) {
        partialUpdate = false;
    }
    // Force full redraw when the days value changes (day rolls over, or 1→0 transition)
    if (cd.days != _prevCountdownDays) {
        partialUpdate = false;
    }
    _prevCountdownDays = cd.days;
    _prevOnNow = cd.isOnNow;

    char countBuf[16];

    // --- Handle "On Now" state ---
    if (cd.isOnNow) {
        if (!partialUpdate) {
            tft.fillScreen(COLOR_BG);

            tft.fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_F1_RED);
            tft.setTextColor(COLOR_HEADER_TEXT);
            tft.setTextDatum(MC_DATUM);
            tft.setFreeFont(&FreeSansBold9pt7b);
            tft.drawString("SESSION ON NOW", SCREEN_WIDTH / 2, 12);

            tft.setFreeFont(&FreeSans12pt7b);
            tft.setTextColor(COLOR_TEXT);
            tft.drawString(race.name, SCREEN_WIDTH / 2, 48);
            tft.setFreeFont(&FreeSans9pt7b);
            tft.setTextColor(COLOR_SESSION_TEXT);
            tft.drawString(race.location, SCREEN_WIDTH / 2, 66);
        }

        // Display race car image
        constexpr int16_t carX = (SCREEN_WIDTH - F1_CAR_WIDTH) / 2;
        constexpr int16_t carY = 100;
        tft.fillRect(0, carY - 10, SCREEN_WIDTH, F1_CAR_HEIGHT + 20, COLOR_BG);
        tft.pushImage(carX, carY, F1_CAR_WIDTH, F1_CAR_HEIGHT, f1_race_car);

        // "On Now" text
        tft.setFreeFont(&FreeMonoBold24pt7b);
        tft.setTextColor(COLOR_F1_RED);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("ON", SCREEN_WIDTH / 2, 180);

        tft.setFreeFont(&FreeSans9pt7b);
        tft.setTextColor(COLOR_TEXT);
        tft.drawString("NOW!", SCREEN_WIDTH / 2, 210);

        return;
    }

    // --- Full redraw: background + all static elements ---
    if (!partialUpdate) {
        tft.fillScreen(COLOR_BG);

        tft.fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_F1_RED);
        tft.setTextColor(COLOR_HEADER_TEXT);
        tft.setTextDatum(MC_DATUM);
        tft.setFreeFont(&FreeSansBold9pt7b);
        tft.drawString(cd.days > 0 ? "RACE WEEK COUNTDOWN" : "SESSION STARTING SOON",
                       SCREEN_WIDTH / 2, 12);

        if (cd.days > 0) {
            // Compact name + location above logo
            tft.setFreeFont(&FreeSans9pt7b);
            tft.setTextColor(COLOR_TEXT);
            tft.drawString(race.name, SCREEN_WIDTH / 2, 34);
            tft.setTextColor(COLOR_SESSION_TEXT);
            tft.drawString(race.location, SCREEN_WIDTH / 2, 50);
            // Location center y=50, bottom ~57px

            // F1 logo on white card — pushed down to clear location text
            // Card: y=66 to y=66+74=140; logo: y=71 to y=131
            constexpr int16_t logoX = (SCREEN_WIDTH - F1_LOGO_WIDTH) / 2;  // 101
            tft.fillRoundRect(logoX - 5, 66, F1_LOGO_WIDTH + 10, F1_LOGO_HEIGHT + 10, 6, TFT_WHITE);
            tft.pushImage(logoX, 71, F1_LOGO_WIDTH, F1_LOGO_HEIGHT, f1_logo);
        } else {
            // No logo — compact HH:MM:SS layout with next session info
            tft.setFreeFont(&FreeSans12pt7b);
            tft.setTextColor(COLOR_TEXT);
            tft.drawString(race.name, SCREEN_WIDTH / 2, 50);
            tft.setFreeFont(&FreeSans9pt7b);
            tft.setTextColor(COLOR_SESSION_TEXT);
            tft.drawString(race.location, SCREEN_WIDTH / 2, 72);

            // Next session label (static until session boundary)
            time_t now = nowUTC();
            for (uint8_t i = 0; i < race.sessionCount; i++) {
                if (race.sessions[i].utcTime > now) {
                    char sessionBuf[48];
                    snprintf(sessionBuf, sizeof(sessionBuf), "%s - %s %s",
                             race.sessions[i].label, race.sessions[i].dayAbbrev,
                             race.sessions[i].localTime);
                    tft.drawString(sessionBuf, SCREEN_WIDTH / 2, 218);
                    break;
                }
            }
        }
    }

    // --- Partial-update: erase then redraw only the changing number region ---
    if (cd.days > 0) {
        // Keep clear of logo card (y=66..140) so rounded bottom corners remain visible
        tft.fillRect(0, 142, SCREEN_WIDTH, 76, COLOR_BG);  // y=142 to y=218

        snprintf(countBuf, sizeof(countBuf), "%d", cd.days);
        tft.setFreeFont(&FreeMonoBold24pt7b);
        tft.setTextColor(COLOR_COUNTDOWN);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(countBuf, SCREEN_WIDTH / 2, 172);  // center ~y=143-201

        tft.setFreeFont(&FreeSans9pt7b);
        tft.setTextColor(COLOR_SESSION_TEXT);
        tft.drawString(cd.days == 1 ? "DAY" : "DAYS", SCREEN_WIDTH / 2, 210);
    } else {
        // Erase region covers HH:MM:SS (FreeMonoBold24pt7b centered at y=138, ~±30px)
        tft.fillRect(0, 107, SCREEN_WIDTH, 63, COLOR_BG);  // y=107 to y=170

        snprintf(countBuf, sizeof(countBuf), "%02d:%02d:%02d", cd.hours, cd.minutes, cd.seconds);
        tft.setFreeFont(&FreeMonoBold24pt7b);
        tft.setTextColor(COLOR_COUNTDOWN);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(countBuf, SCREEN_WIDTH / 2, 138);
    }
}

// --- Event Details Display ---

void drawEventDetails(RaceData& race) {
    tft.fillScreen(COLOR_BG);

    // Header
    tft.fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_F1_RED);
    tft.setTextColor(COLOR_HEADER_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSansBold9pt7b);
    tft.drawString("RACE DETAILS", SCREEN_WIDTH / 2, 12);

    // Round number
    char roundBuf[16];
    snprintf(roundBuf, sizeof(roundBuf), "Round %d", race.round);
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(COLOR_SESSION_TEXT);
    tft.drawString(roundBuf, SCREEN_WIDTH / 2, 38);

    // Race name
    tft.setFreeFont(&FreeSans12pt7b);
    tft.setTextColor(COLOR_TEXT);
    tft.drawString(race.name, SCREEN_WIDTH / 2, 60);

    // Location
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(COLOR_SESSION_TEXT);
    tft.drawString(race.location, SCREEN_WIDTH / 2, 82);

    tft.drawFastHLine(10, 96, SCREEN_WIDTH - 20, COLOR_GRID);

    // Date range: "15 - 17 Mar 2026" or cross-month "30 Nov - 2 Dec 2026"
    tmElements_t tmFirst, tmGP;
    breakTime(myTZ.tzTime(race.firstSessionUtc, UTC_TIME), tmFirst);
    breakTime(myTZ.tzTime(race.gpTimeUtc, UTC_TIME), tmGP);
    const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                            "Jul","Aug","Sep","Oct","Nov","Dec"};
    char dateBuf[32];
    if (tmFirst.Month == tmGP.Month) {
        snprintf(dateBuf, sizeof(dateBuf), "%d - %d %s %d",
                 tmFirst.Day, tmGP.Day, months[tmGP.Month - 1], tmGP.Year + 1970);
    } else {
        snprintf(dateBuf, sizeof(dateBuf), "%d %s - %d %s %d",
                 tmFirst.Day, months[tmFirst.Month - 1],
                 tmGP.Day, months[tmGP.Month - 1], tmGP.Year + 1970);
    }
    tft.setFreeFont(&FreeSans12pt7b);
    tft.setTextColor(COLOR_HIGHLIGHT);
    tft.drawString(dateBuf, SCREEN_WIDTH / 2, 118);

    tft.drawFastHLine(10, 133, SCREEN_WIDTH - 20, COLOR_GRID);

    // Sprint / Standard weekend badge
    tft.setFreeFont(&FreeSansBold9pt7b);
    if (race.isSprint) {
        tft.setTextColor(COLOR_HIGHLIGHT);
        tft.drawString("SPRINT WEEKEND", SCREEN_WIDTH / 2, 153);
    } else {
        tft.setTextColor(COLOR_SESSION_TEXT);
        tft.drawString("STANDARD WEEKEND", SCREEN_WIDTH / 2, 153);
    }

    // First session day + time
    char firstDayBuf[8], firstTimeBuf[8];
    formatLocalDay(race.firstSessionUtc, firstDayBuf, sizeof(firstDayBuf));
    formatLocalTime(race.firstSessionUtc, firstTimeBuf, sizeof(firstTimeBuf));
    char firstBuf[40];
    snprintf(firstBuf, sizeof(firstBuf), "First Session: %s %s", firstDayBuf, firstTimeBuf);
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(COLOR_SESSION_TEXT);
    tft.drawString(firstBuf, SCREEN_WIDTH / 2, 177);

    // Grand Prix day + time
    char gpDayBuf[8], gpTimeBuf[8];
    formatLocalDay(race.gpTimeUtc, gpDayBuf, sizeof(gpDayBuf));
    formatLocalTime(race.gpTimeUtc, gpTimeBuf, sizeof(gpTimeBuf));
    char gpBuf[32];
    snprintf(gpBuf, sizeof(gpBuf), "Grand Prix: %s %s", gpDayBuf, gpTimeBuf);
    tft.setTextColor(COLOR_TEXT);
    tft.drawString(gpBuf, SCREEN_WIDTH / 2, 200);
}

// --- Post-Race: Winner Display ---

void drawRaceWinner(RaceData& race) {
    tft.fillScreen(COLOR_BG);

    // Header
    tft.fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_F1_RED);
    tft.setTextColor(COLOR_HEADER_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSansBold9pt7b);
    tft.drawString("RACE RESULT", SCREEN_WIDTH / 2, 12);

    // Race name
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(COLOR_SESSION_TEXT);
    tft.drawString(race.name, SCREEN_WIDTH / 2,38);

    if (podiumCount == 0) {
        tft.setTextColor(COLOR_GRID);
        tft.drawString("Results pending...", SCREEN_WIDTH / 2, 120);
        return;
    }

    // Top 5 results — 9pt throughout, 36px per entry (driver line + constructor line + gap)
    // yPos: 58, 94, 130, 166, 202 → last constructor at 202+16=218, within 240px
    const char* posLabels[] = {"1st", "2nd", "3rd", "4th", "5th"};

    for (uint8_t i = 0; i < podiumCount; i++) {
        int y = 58 + i * 36;
        tft.setTextDatum(ML_DATUM);

        // Position label — yellow for top 3, white for 4th/5th
        tft.setFreeFont(&FreeSansBold9pt7b);
        tft.setTextColor(i < 3 ? COLOR_HIGHLIGHT : COLOR_TEXT);
        tft.drawString(posLabels[i], 15, y);

        // Driver name — bold white
        tft.setTextColor(COLOR_TEXT);
        tft.drawString(podium[i].driverName, 70, y);

        // Constructor — light gray
        tft.setFreeFont(&FreeSans9pt7b);
        tft.setTextColor(COLOR_SESSION_TEXT);
        tft.drawString(podium[i].constructor, 70, y + 16);
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
    tft.drawString("DRIVER STANDINGS", SCREEN_WIDTH / 2, 12);

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
    tft.drawString("PTS", 255, y);
    tft.drawFastHLine(0, y + 10, SCREEN_WIDTH, COLOR_GRID);

    // Rows
    tft.setFreeFont(&FreeSans9pt7b);
    y = 60;
    constexpr int rowH = 19;
    uint8_t rows = driverStandingsCount < STANDINGS_TOP_N ? driverStandingsCount : STANDINGS_TOP_N;

    for (uint8_t i = 0; i < rows; i++) {
        StandingEntry& e = driverStandings[i];

        // Highlight top 3
        uint16_t color = COLOR_TEXT;
        if (e.position == 1) color = COLOR_PODIUM_GOLD;
        else if (e.position == 2) color = COLOR_PODIUM_SILVER;
        else if (e.position == 3) color = COLOR_PODIUM_BRONZE;

        char posBuf[4];
        snprintf(posBuf, sizeof(posBuf), "%d", e.position);
        char ptsBuf[12];
        snprintf(ptsBuf, sizeof(ptsBuf), "%d pts", e.points);

        // Full name if it fits in the column (x=50 to x=245), else surname only
        constexpr int nameColWidth = 195;
        const char* displayName = e.name;
        char surname[32];
        if (tft.textWidth(e.name) > nameColWidth) {
            const char* lastSpace = strrchr(e.name, ' ');
            if (lastSpace) {
                strlcpy(surname, lastSpace + 1, sizeof(surname));
                displayName = surname;
            }
        }

        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(color);
        tft.drawString(posBuf, 15, y);
        tft.drawString(displayName, 50, y);
        tft.setTextColor(COLOR_SESSION_TEXT);
        tft.drawString(ptsBuf, 248, y);

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
    tft.drawString("CONSTRUCTOR STANDINGS", SCREEN_WIDTH / 2, 12);

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
    tft.drawString("PTS", 255, y);
    tft.drawFastHLine(0, y + 10, SCREEN_WIDTH, COLOR_GRID);

    // Rows
    tft.setFreeFont(&FreeSans9pt7b);
    y = 60;
    constexpr int rowH = 19;
    uint8_t rows = constructorStandingsCount < STANDINGS_TOP_N ? constructorStandingsCount : STANDINGS_TOP_N;

    for (uint8_t i = 0; i < rows; i++) {
        StandingEntry& e = constructorStandings[i];

        uint16_t color = COLOR_TEXT;
        if (e.position == 1) color = COLOR_PODIUM_GOLD;
        else if (e.position == 2) color = COLOR_PODIUM_SILVER;
        else if (e.position == 3) color = COLOR_PODIUM_BRONZE;

        char posBuf[4];
        snprintf(posBuf, sizeof(posBuf), "%d", e.position);
        char ptsBuf[12];
        snprintf(ptsBuf, sizeof(ptsBuf), "%d pts", e.points);

        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(color);
        tft.drawString(posBuf, 15, y);
        tft.drawString(e.name, 50, y);
        tft.setTextColor(COLOR_SESSION_TEXT);
        tft.drawString(ptsBuf, 248, y);

        y += rowH;
    }
}

// --- Status message overlay ---

// yOffset=28 → y=148, matching "Connecting to WiFi..." on the splash screen
void drawStatusMessage(const char* msg, int yOffset = 28) {
    tft.setFreeFont(&FreeSans9pt7b);
    int y = (SCREEN_HEIGHT / 2) + yOffset;
    // Clear using actual font height + generous margin so descenders (g, j, p, y)
    // and any previous longer message don't bleed through.
    int fh = tft.fontHeight();  // FreeSans9pt7b yAdvance ≈ 17
    tft.fillRect(0, y - fh / 2 - 4, SCREEN_WIDTH, fh + 10, COLOR_BG);
    tft.setTextColor(COLOR_SESSION_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(msg, SCREEN_WIDTH / 2, y);
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
