#pragma once

#include "config.h"
#include "types.h"
#include "time_utils.h"
#include "display_renderer.h"
#include "f1_data.h"
#include "track_images.h"

static DisplayState currentDisplayState = STATE_IDLE;
static unsigned long lastDisplayChange = 0;
static bool displayNeedsRedraw = true;

// Force a redraw on next update
void requestRedraw() {
    displayNeedsRedraw = true;
}

// Determine what phase we're in based on current time and race data
DisplayState determinePhase(RaceData& race) {
    time_t now = nowUTC();
    long secsToFirst = race.firstSessionUtc - now;
    long secsAfterGP = now - race.gpTimeUtc;
    int daysToFirst = secsToFirst / 86400;
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
}

// Advance to next display state within the current phase
void advanceDisplayState(DisplayState phase) {
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
    displayNeedsRedraw = true;
}

// Render the current display state
void renderCurrentState(RaceData& race) {
    if (!displayNeedsRedraw) return;
    displayNeedsRedraw = false;

    switch (currentDisplayState) {
        case STATE_IDLE: {
            Countdown cd = getCountdown(race.firstSessionUtc);
            drawCountdown(race, cd);
            break;
        }
        case STATE_RACE_WEEK_COUNTDOWN: {
            Countdown cd = getCountdown(race.firstSessionUtc);
            drawCountdown(race, cd);
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
        currentDisplayState = phase;
        displayNeedsRedraw = true;
        lastDisplayChange = millis();
    }

    // Countdown state needs frequent updates for the timer
    if (currentDisplayState == STATE_IDLE ||
        currentDisplayState == STATE_RACE_WEEK_COUNTDOWN) {
        static unsigned long lastCountdownUpdate = 0;
        if (millis() - lastCountdownUpdate >= COUNTDOWN_UPDATE_MS) {
            lastCountdownUpdate = millis();
            displayNeedsRedraw = true;
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
    advanceDisplayState(phase);
    lastDisplayChange = millis();
}
