#pragma once

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "config.h"
#include "types.h"
#include "debug.h"
#include "time_utils.h"
#include "config_manager.h"

// --- Global race data ---
static RaceData races[MAX_RACES];       // prev, current, next
static uint8_t currentRaceIdx = 1;      // Index into races[] for current/next race
static uint8_t totalRacesLoaded = 0;

// --- Compact upcoming race list (all future rounds, populated by parseSchedule) ---
struct UpcomingRace {
    uint8_t  round;
    bool     isSprint;
    char     name[24];
    char     location[16];
    time_t   gpTimeUtc;
};
static UpcomingRace upcomingRaces[25];
static uint8_t upcomingCount = 0;

// --- Post-race data ---
static RaceResult podium[MAX_PODIUM];
static StandingEntry driverStandings[MAX_STANDINGS];
static StandingEntry constructorStandings[MAX_STANDINGS];
static uint8_t podiumCount = 0;
static uint8_t driverStandingsCount = 0;
static uint8_t constructorStandingsCount = 0;
static bool resultsAvailable = false;

// --- Helpers ---

static const char* sessionLabel(SessionType type) {
    switch (type) {
        case SESSION_FP1:               return "Free Practice 1";
        case SESSION_FP2:               return "Free Practice 2";
        case SESSION_FP3:               return "Free Practice 3";
        case SESSION_SPRINT_QUALIFYING: return "Sprint Qualifying";
        case SESSION_SPRINT:            return "Sprint";
        case SESSION_QUALIFYING:        return "Qualifying";
        case SESSION_GP:                return "Race";
        default:                        return "Unknown";
    }
}

// Parse ISO 8601 UTC string to time_t: "2026-03-14T01:30:00Z"
static time_t parseISO8601(const char* iso) {
    if (!iso || strlen(iso) < 19) return 0;
    tmElements_t tm;
    tm.Year   = atoi(iso) - 1970;
    tm.Month  = atoi(iso + 5);
    tm.Day    = atoi(iso + 8);
    tm.Hour   = atoi(iso + 11);
    tm.Minute = atoi(iso + 14);
    tm.Second = atoi(iso + 17);
    return makeTime(tm);
}

// Add a session to a race
static void addSession(RaceData& race, SessionType type, const char* isoTime) {
    if (!isoTime || race.sessionCount >= MAX_SESSIONS) return;
    SessionInfo& s = race.sessions[race.sessionCount];
    s.type = type;
    s.utcTime = parseISO8601(isoTime);
    strlcpy(s.label, sessionLabel(type), sizeof(s.label));
    formatLocalDay(s.utcTime, s.dayAbbrev, sizeof(s.dayAbbrev));
    formatLocalTime(s.utcTime, s.localTime, sizeof(s.localTime));
    DBG_VERBOSE("[F1]   + %s  %s %s", s.label, s.dayAbbrev, s.localTime);
    race.sessionCount++;
}

// Recalculate session times for a race after timezone change
static void updateSessionTimesForRace(RaceData& race) {
    for (uint8_t i = 0; i < race.sessionCount; i++) {
        SessionInfo& s = race.sessions[i];
        formatLocalDay(s.utcTime, s.dayAbbrev, sizeof(s.dayAbbrev));
        formatLocalTime(s.utcTime, s.localTime, sizeof(s.localTime));
    }
    DBG_INFO("[F1] Updated session times for race '%s' after timezone change", race.name);
}

// Parse the sportstimes JSON and find relevant races
bool parseSchedule(const String& json) {
    DBG_INFO("[F1] Parsing schedule JSON (%d bytes)", json.length());

    JsonDocument filter;
    JsonObject rf = filter["races"][0].to<JsonObject>();
    rf["name"]     = true;
    rf["location"] = true;
    rf["round"]    = true;
    rf["slug"]     = true;
    rf["sessions"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json, DeserializationOption::Filter(filter));
    if (err) {
        DBG_ERROR("[F1] JSON parse error: %s", err.c_str());
        return false;
    }

    JsonArray racesArr = doc["races"].as<JsonArray>();
    if (racesArr.isNull() || racesArr.size() == 0) {
        DBG_ERROR("[F1] No races found in JSON");
        return false;
    }

    DBG_VERBOSE("[F1] Total races in feed: %d", racesArr.size());

    // Find the current/next race based on GP time
    time_t now = nowUTC();
    int nextIdx = -1;

    for (int i = 0; i < (int)racesArr.size(); i++) {
        const char* gpTime = racesArr[i]["sessions"]["gp"];
        time_t gp = parseISO8601(gpTime);
        // Find first race where GP hasn't happened yet (or within post-race window)
        if (gp + (POST_RACE_DAYS * 86400L) > now) {
            nextIdx = i;
            break;
        }
    }

    if (nextIdx < 0) {
        // Season is over, use last race
        nextIdx = racesArr.size() - 1;
        DBG_WARN("[F1] Season appears over, defaulting to last race");
    }

    // Load prev, current, next
    int indices[MAX_RACES];
    indices[0] = (nextIdx > 0) ? nextIdx - 1 : 0;           // Previous
    indices[1] = nextIdx;                                      // Current/next
    indices[2] = (nextIdx < (int)racesArr.size() - 1) ? nextIdx + 1 : nextIdx;  // Next after

    totalRacesLoaded = 0;
    for (int r = 0; r < MAX_RACES; r++) {
        JsonObject race = racesArr[indices[r]];
        RaceData& rd = races[r];
        memset(&rd, 0, sizeof(RaceData));

        strlcpy(rd.name,     race["name"]     | "Unknown", sizeof(rd.name));
        strlcpy(rd.location, race["location"] | "",        sizeof(rd.location));
        strlcpy(rd.slug,     race["slug"]     | "",        sizeof(rd.slug));
        rd.round = race["round"] | 0;

        JsonObjectConst sessions = race["sessions"].as<JsonObjectConst>();
        rd.isSprint = !sessions["sprint"].isNull();

        DBG_VERBOSE("[F1] R%d %s (%s%s)", rd.round, rd.name, rd.location,
                    rd.isSprint ? " - Sprint" : "");

        if (rd.isSprint) {
            addSession(rd, SESSION_FP1,               sessions["fp1"]);
            addSession(rd, SESSION_SPRINT_QUALIFYING,  sessions["sprintQualifying"]);
            addSession(rd, SESSION_SPRINT,             sessions["sprint"]);
            addSession(rd, SESSION_QUALIFYING,         sessions["qualifying"]);
            addSession(rd, SESSION_GP,                 sessions["gp"]);
        } else {
            addSession(rd, SESSION_FP1,       sessions["fp1"]);
            addSession(rd, SESSION_FP2,       sessions["fp2"]);
            addSession(rd, SESSION_FP3,       sessions["fp3"]);
            addSession(rd, SESSION_QUALIFYING, sessions["qualifying"]);
            addSession(rd, SESSION_GP,         sessions["gp"]);
        }

        // Set first/last session times
        if (rd.sessionCount > 0) {
            rd.firstSessionUtc = rd.sessions[0].utcTime;
            rd.gpTimeUtc = rd.sessions[rd.sessionCount - 1].utcTime;
        }
        totalRacesLoaded++;
    }

    DBG_INFO("[F1] Loaded %d races. Current: R%d %s (%s)",
             totalRacesLoaded, races[1].round, races[1].name,
             races[1].isSprint ? "Sprint weekend" : "Standard weekend");

    // Log first session timing for countdown debugging.
    if (races[1].sessionCount > 0) {
        char firstLocal[48];
        char gpLocal[48];
        formatLocalFullDate(races[1].firstSessionUtc, firstLocal, sizeof(firstLocal));
        formatLocalFullDate(races[1].gpTimeUtc, gpLocal, sizeof(gpLocal));
        DBG_INFO("[F1] Current race first session (UTC): %ld", (long)races[1].firstSessionUtc);
        DBG_INFO("[F1] Current race first session (local): %s", firstLocal);
        DBG_INFO("[F1] Current race GP (local): %s", gpLocal);
    }

    // Populate compact upcoming-races list (all rounds from current onward)
    upcomingCount = 0;
    for (int i = nextIdx; i < (int)racesArr.size() && upcomingCount < 25; i++) {
        JsonObject r = racesArr[i];
        UpcomingRace& ur = upcomingRaces[upcomingCount];
        ur.round    = r["round"] | 0;
        JsonObjectConst upcomingSessions = r["sessions"].as<JsonObjectConst>();
        ur.isSprint = !upcomingSessions["sprint"].isNull();
        strlcpy(ur.name,     r["name"]     | "Unknown", sizeof(ur.name));
        strlcpy(ur.location, r["location"] | "",        sizeof(ur.location));
        ur.gpTimeUtc = parseISO8601(upcomingSessions["gp"]);
        upcomingCount++;
    }
    DBG_INFO("[F1] Upcoming races: %d (from R%d to end of season)", upcomingCount,
             upcomingCount > 0 ? upcomingRaces[0].round : 0);
    return true;
}

// Fetch schedule from GitHub
bool fetchSchedule() {
    DBG_INFO("[F1] Fetching schedule from GitHub...");
    WiFiClientSecure client;
    client.setInsecure();  // Skip cert verification for GitHub raw

    HTTPClient http;
    http.begin(client, SCHEDULE_URL);
    http.setTimeout(15000);
    int code = http.GET();

    if (code != 200) {
        DBG_WARN("[F1] Schedule fetch HTTP %d", code);
        http.end();
        return false;
    }

    String json = http.getString();
    http.end();
    DBG_INFO("[F1] Schedule received (%d bytes)", json.length());

    // Cache and parse
    cacheSchedule(json);
    return parseSchedule(json);
}

// Load schedule from cache
bool loadScheduleFromCache() {
    DBG_INFO("[F1] Loading schedule from cache");
    String json = loadCachedSchedule();
    if (json.isEmpty()) {
        DBG_WARN("[F1] No cached schedule available");
        return false;
    }
    return parseSchedule(json);
}

// Fetch race results from Jolpica API
bool fetchRaceResults(uint8_t round) {
    DBG_INFO("[F1] Fetching results for R%d", round);
    HTTPClient http;
    char url[128];
    snprintf(url, sizeof(url), "%s/%d/results.json?limit=3", JOLPICA_BASE_URL, round);
    http.begin(url);
    http.setTimeout(10000);
    int code = http.GET();

    if (code != 200) {
        DBG_WARN("[F1] Race results HTTP %d", code);
        http.end();
        return false;
    }

    String json = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        DBG_ERROR("[F1] Results parse error: %s", err.c_str());
        return false;
    }

    JsonArray results = doc["MRData"]["RaceTable"]["Races"][0]["Results"];
    if (results.isNull() || results.size() == 0) {
        DBG_WARN("[F1] No results data in response (race may still be running)");
        return false;
    }

    podiumCount = 0;
    for (JsonObject r : results) {
        if (podiumCount >= MAX_PODIUM) break;
        RaceResult& p = podium[podiumCount];
        p.position = r["position"].as<int>();
        strlcpy(p.driverCode, r["Driver"]["code"] | "???", sizeof(p.driverCode));
        snprintf(p.driverName, sizeof(p.driverName), "%s %s",
                 r["Driver"]["givenName"] | "",
                 r["Driver"]["familyName"] | "");
        strlcpy(p.constructor, r["Constructor"]["name"] | "", sizeof(p.constructor));
        podiumCount++;
    }

    DBG_INFO("[F1] Podium: P1=%s  P2=%s  P3=%s",
             podium[0].driverCode,
             podiumCount > 1 ? podium[1].driverCode : "-",
             podiumCount > 2 ? podium[2].driverCode : "-");
    return podiumCount > 0;
}

// Fetch driver standings from Jolpica API
bool fetchDriverStandings() {
    DBG_INFO("[F1] Fetching driver standings");
    HTTPClient http;
    char url[128];
    snprintf(url, sizeof(url), "%s/driverstandings.json?limit=%d", JOLPICA_BASE_URL, STANDINGS_TOP_N);
    http.begin(url);
    http.setTimeout(10000);
    int code = http.GET();

    if (code != 200) {
        DBG_WARN("[F1] Driver standings HTTP %d", code);
        http.end();
        return false;
    }

    String json = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, json)) {
        DBG_ERROR("[F1] Driver standings parse error");
        return false;
    }

    JsonArray standings = doc["MRData"]["StandingsTable"]["StandingsLists"][0]["DriverStandings"];
    if (standings.isNull()) {
        DBG_WARN("[F1] No driver standings in response");
        return false;
    }

    driverStandingsCount = 0;
    for (JsonObject s : standings) {
        if (driverStandingsCount >= STANDINGS_TOP_N || driverStandingsCount >= MAX_STANDINGS) break;
        StandingEntry& e = driverStandings[driverStandingsCount];
        e.position = s["position"].as<int>();
        e.points   = (uint16_t)(s["points"].as<float>());
        e.wins     = s["wins"].as<int>();
        strlcpy(e.code, s["Driver"]["code"] | "???", sizeof(e.code));
        snprintf(e.name, sizeof(e.name), "%s %s",
                 s["Driver"]["givenName"] | "",
                 s["Driver"]["familyName"] | "");
        driverStandingsCount++;
    }

    DBG_INFO("[F1] Driver standings Top %d: %d entries. P1: %s (%d pts)",
             STANDINGS_TOP_N,
             driverStandingsCount,
             driverStandingsCount > 0 ? driverStandings[0].code : "?",
             driverStandingsCount > 0 ? driverStandings[0].points : 0);
    return driverStandingsCount > 0;
}

// Fetch constructor standings from Jolpica API
bool fetchConstructorStandings() {
    DBG_INFO("[F1] Fetching constructor standings");
    HTTPClient http;
    char url[128];
    snprintf(url, sizeof(url), "%s/constructorstandings.json?limit=%d", JOLPICA_BASE_URL, STANDINGS_TOP_N);
    http.begin(url);
    http.setTimeout(10000);
    int code = http.GET();

    if (code != 200) {
        DBG_WARN("[F1] Constructor standings HTTP %d", code);
        http.end();
        return false;
    }

    String json = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, json)) {
        DBG_ERROR("[F1] Constructor standings parse error");
        return false;
    }

    JsonArray standings = doc["MRData"]["StandingsTable"]["StandingsLists"][0]["ConstructorStandings"];
    if (standings.isNull()) {
        DBG_WARN("[F1] No constructor standings in response");
        return false;
    }

    constructorStandingsCount = 0;
    for (JsonObject s : standings) {
        if (constructorStandingsCount >= STANDINGS_TOP_N || constructorStandingsCount >= MAX_STANDINGS) break;
        StandingEntry& e = constructorStandings[constructorStandingsCount];
        e.position = s["position"].as<int>();
        e.points   = (uint16_t)(s["points"].as<float>());
        e.wins     = s["wins"].as<int>();
        strlcpy(e.name, s["Constructor"]["name"] | "???", sizeof(e.name));
        strlcpy(e.code, "", sizeof(e.code));  // No code for constructors
        constructorStandingsCount++;
    }

    DBG_INFO("[F1] Constructor standings Top %d: %d entries. P1: %s (%d pts)",
             STANDINGS_TOP_N,
             constructorStandingsCount,
             constructorStandingsCount > 0 ? constructorStandings[0].name : "?",
             constructorStandingsCount > 0 ? constructorStandings[0].points : 0);
    return constructorStandingsCount > 0;
}

// Fetch all post-race data
bool fetchPostRaceData(uint8_t round) {
    DBG_INFO("[F1] Fetching all post-race data for R%d", round);
    bool resultsOk = fetchRaceResults(round);
    bool driversOk = fetchDriverStandings();
    bool constructorsOk = fetchConstructorStandings();
    bool ok = resultsOk && driversOk && constructorsOk;
    resultsAvailable = ok;
    DBG_INFO("[F1] Post-race data fetch: podium=%s, drivers=%s, constructors=%s",
             resultsOk ? "ok" : "fail",
             driversOk ? "ok" : "fail",
             constructorsOk ? "ok" : "fail");
    DBG_INFO("[F1] Post-race data fetch: %s", ok ? "complete" : "PARTIAL/FAILED (will retry)");
    return ok;
}

// Get the current race data
RaceData& getCurrentRace() {
    return races[currentRaceIdx];
}

// Get the next race data
RaceData& getNextRace() {
    return races[2];
}
