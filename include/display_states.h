#pragma once

#include "config.h"
#include "types.h"
#include "debug.h"
#include "time_utils.h"
#include "display_renderer.h"
#include "f1_data.h"
#include "track_images.h"

static DisplayState currentDisplayState = STATE_IDLE;
static unsigned long lastDisplayChange = 0;
static bool displayNeedsRedraw = true;
static bool displayPartialUpdate = false;  // true = only countdown digits need refresh

// Human-readable state name for debug output
static const char* stateName(DisplayState s) {
    switch (s) {
        case STATE_IDLE:                   return "IDLE";
        case STATE_RACE_WEEK_COUNTDOWN:    return "RACE_WEEK_COUNTDOWN";
        case STATE_RACE_WEEK_SCHEDULE:     return "RACE_WEEK_SCHEDULE";
        case STATE_RACE_WEEK_TRACK:        return "RACE_WEEK_TRACK";
        case STATE_POST_RACE_WINNER:       return "POST_RACE_WINNER";
        case STATE_POST_RACE_DRIVERS:      return "POST_RACE_DRIVERS";
        case STATE_POST_RACE_CONSTRUCTORS: return "POST_RACE_CONSTRUCTORS";
        default:                           return "UNKNOWN";
    }
}

// Force a full redraw on next update
void requestRedraw() {
    displayNeedsRedraw = true;
    displayPartialUpdate = false;
}

// Determine what phase we're in based on current time and race data
DisplayState determinePhase(RaceData& race) {
#if FORCE_POST_RACE_TEST_DISPLAYS
    (void)race;
    return STATE_POST_RACE_WINNER;
#else
    time_t now = nowUTC();
    long secsAfterGP = now - race.gpTimeUtc;
    int daysToFirst = daysUntilLocalDate(race.firstSessionUtc);
    int daysAfterGP = secsAfterGP / 86400;

    if (secsAfterGP >= 0 && daysAfterGP < POST_RACE_DAYS) {
        // Post-race window
        return STATE_POST_RACE_WINNER;
    } else if (daysToFirst >= 0 && daysToFirst <= COUNTDOWN_WEEK_DAYS) {
        // Race week
        return STATE_RACE_WEEK_COUNTDOWN;
    } else {
        // Idle - between races
        return STATE_IDLE;
    }
#endif
}

// Advance to next display state within the current phase
void advanceDisplayState(DisplayState phase) {
    DisplayState prev = currentDisplayState;

    switch (phase) {
        case STATE_RACE_WEEK_COUNTDOWN:
        case STATE_RACE_WEEK_SCHEDULE:
        case STATE_RACE_WEEK_TRACK:
            // Rotate between countdown, schedule, track
            switch (currentDisplayState) {
                case STATE_RACE_WEEK_COUNTDOWN: currentDisplayState = STATE_RACE_WEEK_SCHEDULE; break;
                case STATE_RACE_WEEK_SCHEDULE:  currentDisplayState = STATE_RACE_WEEK_TRACK; break;
                case STATE_RACE_WEEK_TRACK:     currentDisplayState = STATE_RACE_WEEK_COUNTDOWN; break;
                default:                        currentDisplayState = STATE_RACE_WEEK_COUNTDOWN; break;
            }
            break;

        case STATE_POST_RACE_WINNER:
        case STATE_POST_RACE_DRIVERS:
        case STATE_POST_RACE_CONSTRUCTORS:
            // Rotate between winner, drivers, constructors
            switch (currentDisplayState) {
                case STATE_POST_RACE_WINNER:       currentDisplayState = STATE_POST_RACE_DRIVERS; break;
                case STATE_POST_RACE_DRIVERS:      currentDisplayState = STATE_POST_RACE_CONSTRUCTORS; break;
                case STATE_POST_RACE_CONSTRUCTORS: currentDisplayState = STATE_POST_RACE_WINNER; break;
                default:                           currentDisplayState = STATE_POST_RACE_WINNER; break;
            }
            break;

        default:
            currentDisplayState = STATE_IDLE;
            break;
    }

    DBG_INFO("[Display] Screen change: %s → %s", stateName(prev), stateName(currentDisplayState));
    displayNeedsRedraw = true;
}

// Render the current display state
void renderCurrentState(RaceData& race) {
    if (!displayNeedsRedraw) return;
    bool partial = displayPartialUpdate;
    displayNeedsRedraw = false;
    displayPartialUpdate = false;

    switch (currentDisplayState) {
        case STATE_IDLE: {
            Countdown cd = getCountdown(race.firstSessionUtc);
            drawCountdown(race, cd, partial);
            break;
        }
        case STATE_RACE_WEEK_COUNTDOWN: {
            Countdown cd = getCountdown(race.firstSessionUtc);
            drawCountdown(race, cd, partial);
            break;
        }
        case STATE_RACE_WEEK_SCHEDULE:
            drawScheduleTable(race);
            break;

        case STATE_RACE_WEEK_TRACK: {
            const uint8_t* trackImg = getTrackImage(race.slug);
            uint16_t tw, th;
            getTrackImageSize(race.slug, tw, th);
            drawTrackLayout(trackImg, tw, th, race);
            break;
        }
        case STATE_POST_RACE_WINNER:
            drawRaceWinner(race);
            break;

        case STATE_POST_RACE_DRIVERS:
            drawDriverStandings();
            break;

        case STATE_POST_RACE_CONSTRUCTORS:
            drawConstructorStandings();
            break;
    }
}

// Main display update - call from loop()
void updateDisplay(RaceData& race) {
    DisplayState phase = determinePhase(race);
    unsigned long rotateInterval = (phase >= STATE_POST_RACE_WINNER)
                                   ? POST_RACE_ROTATE_MS
                                   : DISPLAY_ROTATE_MS;

    // Check if phase changed (e.g., transition from race-week to post-race)
    bool phaseChanged = false;
    if (phase == STATE_IDLE && currentDisplayState != STATE_IDLE) {
        phaseChanged = true;
    } else if (phase == STATE_RACE_WEEK_COUNTDOWN &&
               currentDisplayState != STATE_RACE_WEEK_COUNTDOWN &&
               currentDisplayState != STATE_RACE_WEEK_SCHEDULE &&
               currentDisplayState != STATE_RACE_WEEK_TRACK) {
        phaseChanged = true;
    } else if (phase == STATE_POST_RACE_WINNER &&
               currentDisplayState != STATE_POST_RACE_WINNER &&
               currentDisplayState != STATE_POST_RACE_DRIVERS &&
               currentDisplayState != STATE_POST_RACE_CONSTRUCTORS) {
        phaseChanged = true;
    }

    if (phaseChanged) {
        DBG_INFO("[Display] Phase change: %s → %s", stateName(currentDisplayState), stateName(phase));
        currentDisplayState = phase;
        displayNeedsRedraw = true;
        lastDisplayChange = millis();
    }

    // Countdown state: update digits every second without full-screen flicker
    if (currentDisplayState == STATE_IDLE ||
        currentDisplayState == STATE_RACE_WEEK_COUNTDOWN) {
        static unsigned long lastCountdownUpdate = 0;
        if (millis() - lastCountdownUpdate >= COUNTDOWN_UPDATE_MS) {
            lastCountdownUpdate = millis();
            if (!displayNeedsRedraw) {          // don't override a pending full redraw
                displayNeedsRedraw = true;
                displayPartialUpdate = true;    // digits only — no fillScreen
            }
        }
    }

    // Rotate between screens
    if (currentDisplayState != STATE_IDLE &&
        millis() - lastDisplayChange >= rotateInterval) {
        lastDisplayChange = millis();
        advanceDisplayState(phase);
    }

    renderCurrentState(race);
}

// Manual advance (touch input)
void manualAdvanceDisplay(RaceData& race) {
    DisplayState phase = determinePhase(race);
    DBG_INFO("[Display] Manual advance from %s", stateName(currentDisplayState));
    advanceDisplayState(phase);
    lastDisplayChange = millis();
}
