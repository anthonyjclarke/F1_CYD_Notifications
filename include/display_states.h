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
        case STATE_POST_RACE_NEXT_RACE:    return "POST_RACE_NEXT_RACE";
        default:                           return "UNKNOWN";
    }
}

// Force a full redraw on next update
void requestRedraw() {
    displayNeedsRedraw = true;
    displayPartialUpdate = false;
}

// Helper: compare local dates (return true if same day in local timezone)
static bool isSameLocalDay(time_t utc1, time_t utc2) {
    tmElements_t tm1, tm2;
    breakTime(myTZ.tzTime(utc1, UTC_TIME), tm1);
    breakTime(myTZ.tzTime(utc2, UTC_TIME), tm2);
    return (tm1.Year == tm2.Year && tm1.Month == tm2.Month && tm1.Day == tm2.Day);
}

// Structure to hold both countdown target and session type
struct CountdownTarget {
    time_t utcTime;
    SessionType sessionType;
};

// Determine the countdown target: if today is an event day in the race weekend,
// count down to the next/current session on that day; otherwise to first session
static CountdownTarget getCountdownTarget(RaceData& race) {
    time_t now = nowUTC();

    // Check if any session is scheduled for today (local date)
    for (uint8_t i = 0; i < race.sessionCount; i++) {
        time_t sessionTime = race.sessions[i].utcTime;
        SessionType sessionType = race.sessions[i].type;

        // If this session is on today's date
        if (isSameLocalDay(now, sessionTime)) {
            // Check if session is currently running
            uint16_t durationSecs = getSessionDurationSeconds(sessionType);
            time_t sessionEndTime = sessionTime + durationSecs;

            if (now >= sessionTime && now < sessionEndTime) {
                // Session is currently running - return it
                return {sessionTime, sessionType};
            } else if (sessionTime > now) {
                // Session hasn't started yet - return it
                return {sessionTime, sessionType};
            }
            // Session has ended - continue looking for other sessions today
        }
    }

    // No event today - find the next upcoming session in the season
    // First check if there are future sessions in the current race
    for (uint8_t i = 0; i < race.sessionCount; i++) {
        if (race.sessions[i].utcTime > now) {
            return {race.sessions[i].utcTime, race.sessions[i].type};
        }
    }

    // No future sessions in current race - check upcoming races
    for (uint8_t i = 0; i < upcomingCount; i++) {
        if (upcomingRaces[i].gpTimeUtc > now) {
            // For upcoming races, return the GP time as the target
            return {upcomingRaces[i].gpTimeUtc, SESSION_GP};
        }
    }

    // Fallback to first session of current race (shouldn't happen in normal operation)
    return {race.firstSessionUtc, race.sessions[0].type};
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
            if (resultsAvailable) {
                // Combined rotation: countdown → schedule → track → winner → drivers → constructors → countdown
                switch (currentDisplayState) {
                    case STATE_RACE_WEEK_COUNTDOWN:    currentDisplayState = STATE_RACE_WEEK_SCHEDULE; break;
                    case STATE_RACE_WEEK_SCHEDULE:     currentDisplayState = STATE_RACE_WEEK_TRACK; break;
                    case STATE_RACE_WEEK_TRACK:        currentDisplayState = STATE_POST_RACE_WINNER; break;
                    case STATE_POST_RACE_WINNER:       currentDisplayState = STATE_POST_RACE_DRIVERS; break;
                    case STATE_POST_RACE_DRIVERS:      currentDisplayState = STATE_POST_RACE_CONSTRUCTORS; break;
                    case STATE_POST_RACE_CONSTRUCTORS: currentDisplayState = STATE_RACE_WEEK_COUNTDOWN; break;
                    default:                           currentDisplayState = STATE_RACE_WEEK_COUNTDOWN; break;
                }
            } else {
                // Standard rotation: countdown → schedule → track → countdown
                switch (currentDisplayState) {
                    case STATE_RACE_WEEK_COUNTDOWN: currentDisplayState = STATE_RACE_WEEK_SCHEDULE; break;
                    case STATE_RACE_WEEK_SCHEDULE:  currentDisplayState = STATE_RACE_WEEK_TRACK; break;
                    case STATE_RACE_WEEK_TRACK:     currentDisplayState = STATE_RACE_WEEK_COUNTDOWN; break;
                    default:                        currentDisplayState = STATE_RACE_WEEK_COUNTDOWN; break;
                }
            }
            break;

        case STATE_POST_RACE_WINNER:
        case STATE_POST_RACE_DRIVERS:
        case STATE_POST_RACE_CONSTRUCTORS:
        case STATE_POST_RACE_NEXT_RACE:
            // Rotate: winner → drivers → constructors → next race → winner
            switch (currentDisplayState) {
                case STATE_POST_RACE_WINNER:       currentDisplayState = STATE_POST_RACE_DRIVERS; break;
                case STATE_POST_RACE_DRIVERS:      currentDisplayState = STATE_POST_RACE_CONSTRUCTORS; break;
                case STATE_POST_RACE_CONSTRUCTORS: currentDisplayState = STATE_POST_RACE_NEXT_RACE; break;
                case STATE_POST_RACE_NEXT_RACE:    currentDisplayState = STATE_POST_RACE_WINNER; break;
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

// Render the current display state.
// phase is passed in to distinguish combined race-week+results mode from pure post-race.
void renderCurrentState(RaceData& race, DisplayState phase) {
    if (!displayNeedsRedraw) return;
    bool partial = displayPartialUpdate;
    displayNeedsRedraw = false;
    displayPartialUpdate = false;

    switch (currentDisplayState) {
        case STATE_IDLE: {
            CountdownTarget target = getCountdownTarget(race);
            Countdown cd = getCountdownWithSession(target.utcTime, target.sessionType);
            drawCountdown(race, cd, partial);
            break;
        }
        case STATE_RACE_WEEK_COUNTDOWN: {
            CountdownTarget target = getCountdownTarget(race);
            Countdown cd = getCountdownWithSession(target.utcTime, target.sessionType);
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
            // In combined race-week mode races[1] is the new race; use races[0] for previous race title
            drawRaceWinner(phase == STATE_RACE_WEEK_COUNTDOWN ? getPrevRace() : race);
            break;

        case STATE_POST_RACE_DRIVERS:
            drawDriverStandings();
            break;

        case STATE_POST_RACE_CONSTRUCTORS:
            drawConstructorStandings();
            break;

        case STATE_POST_RACE_NEXT_RACE: {
            RaceData& nextRace = getNextRace();
            CountdownTarget target = getCountdownTarget(nextRace);
            Countdown cd = getCountdownWithSession(target.utcTime, target.sessionType);
            drawCountdown(nextRace, cd, partial);
            break;
        }
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
               currentDisplayState != STATE_RACE_WEEK_TRACK &&
               // Don't reset if showing previous race results in combined mode
               !(resultsAvailable && (
                   currentDisplayState == STATE_POST_RACE_WINNER ||
                   currentDisplayState == STATE_POST_RACE_DRIVERS ||
                   currentDisplayState == STATE_POST_RACE_CONSTRUCTORS))) {
        phaseChanged = true;
    } else if (phase == STATE_POST_RACE_WINNER &&
               currentDisplayState != STATE_POST_RACE_WINNER &&
               currentDisplayState != STATE_POST_RACE_DRIVERS &&
               currentDisplayState != STATE_POST_RACE_CONSTRUCTORS &&
               currentDisplayState != STATE_POST_RACE_NEXT_RACE) {
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
        currentDisplayState == STATE_RACE_WEEK_COUNTDOWN ||
        currentDisplayState == STATE_POST_RACE_NEXT_RACE) {
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

    renderCurrentState(race, phase);
}

// Manual advance (touch input)
void manualAdvanceDisplay(RaceData& race) {
    DisplayState phase = determinePhase(race);
    DBG_INFO("[Display] Manual advance from %s", stateName(currentDisplayState));
    advanceDisplayState(phase);
    lastDisplayChange = millis();
}
