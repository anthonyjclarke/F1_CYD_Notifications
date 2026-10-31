#pragma once
#include "config.h"
#include "types.h"
#include "time_utils.h"
static RaceData races[MAX_RACES];
struct SeasonRace { time_t gpTimeUtc; };
static SeasonRace seasonRaces[25];
static uint8_t seasonCount = 0;
static RaceResult podium[MAX_PODIUM];
static StandingEntry driverStandings[MAX_STANDINGS], constructorStandings[MAX_STANDINGS];
static uint8_t podiumCount = 0, driverStandingsCount = 0, constructorStandingsCount = 0;
static uint8_t resultsRound = 0;
inline bool hasResultsFor(const RaceData& race) {
    return resultsRound != 0 && resultsRound == race.round && podiumCount > 0;
}
inline time_t gpFinishedUtc(time_t utc) { return utc + getSessionDurationSeconds(SESSION_GP); }
inline RaceData& getCurrentRace() { return races[1]; }
inline RaceData& getPrevRace() { return races[0]; }
inline RaceData& getNextRace() { return races[2]; }
