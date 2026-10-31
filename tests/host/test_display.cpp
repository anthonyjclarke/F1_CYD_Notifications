#include "f1_data.h"
#include "display_states.h"
#include <cassert>
#include <fstream>
#include <iostream>

static RaceData makeRace(time_t gp) {
    RaceData race = {};
    strlcpy(race.name, "Test Grand Prix", sizeof(race.name));
    strlcpy(race.location, "Test circuit", sizeof(race.location));
    race.round = 10;
    race.gpTimeUtc = gp;
    race.firstSessionUtc = gp - 2 * 86400;
    race.sessionCount = 1;
    race.sessions[0].type = SESSION_GP;
    race.sessions[0].utcTime = gp;
    return race;
}

int main(int argc, char** argv) {
    time_t gp = fakeUtc;
    races[1] = makeRace(gp);
    RaceData& race = getCurrentRace();

    // GP starts in race-week mode; only its nominal end starts post-race.
    fakeUtc = gp - 1;
    assert(determinePhase(race) == STATE_RACE_WEEK_COUNTDOWN);
    fakeUtc = gp;
    assert(determinePhase(race) == STATE_RACE_WEEK_COUNTDOWN);
    fakeUtc = gp + 7199;
    assert(determinePhase(race) == STATE_RACE_WEEK_COUNTDOWN);
    fakeUtc = gp + 7200;
    assert(determinePhase(race) == STATE_POST_RACE_WINNER);

    // Every supported session uses its own nominal live window.
    for (int type = SESSION_FP1; type <= SESSION_GP; ++type) {
        race.sessions[0].type = static_cast<SessionType>(type);
        uint16_t duration = getSessionDurationSeconds(race.sessions[0].type);
        fakeUtc = gp + duration - 1;
        assert(getCountdownTarget(race).utcTime == gp);
        assert(getCountdownWithSession(gp, race.sessions[0].type).isOnNow);
        fakeUtc = gp + duration;
        assert(!getCountdownWithSession(gp, race.sessions[0].type).isOnNow);
    }

    // The start date can differ from today's local date while still live.
    fakeLocalOffset = 11 * 3600;
    race = makeRace(gp + 12 * 3600 + 30 * 60);  // 23:30 local
    fakeUtc = race.gpTimeUtc + 3600;            // 00:30 local tomorrow
    assert(!isSameLocalDay(fakeUtc, race.gpTimeUtc));
    assert(getCountdownTarget(race).utcTime == race.gpTimeUtc);
    assert(getCountdownWithSession(race.gpTimeUtc, SESSION_GP).isOnNow);

    race = makeRace(gp);
    fakeLocalOffset = 0;
    fakeUtc = gp + 100;
    fakeMillis = 10000;
    currentDisplayState = STATE_RACE_WEEK_COUNTDOWN;
    lastDisplayChange = fakeMillis;
    requestRedraw();
    updateDisplay(race);
    assert(raceAnimation.isActive());
    assert(tft.imageRows == 56);
    int fullRedraws = tft.fills;

    // A normal one-second update and an explicit redraw cannot restart the pass.
    fakeMillis += 1000;
    updateDisplay(race);
    assert(tft.fills == fullRedraws);
    requestRedraw();
    fakeMillis += 1000;
    updateDisplay(race);
    fakeMillis = 14199;
    updateDisplay(race);
    assert(currentDisplayState == STATE_RACE_WEEK_COUNTDOWN);
    fakeMillis = 14200;
    updateDisplay(race);
    assert(currentDisplayState == STATE_RACE_WEEK_EVENT_DETAILS);
    assert(!raceAnimation.isActive());
    assert(std::find(tft.labels.begin(), tft.labels.end(), "RACE DETAILS") != tft.labels.end());

    // Re-entering starts a fresh pass. A tap cancels it without a late advance.
    currentDisplayState = STATE_RACE_WEEK_COUNTDOWN;
    requestRedraw();
    updateDisplay(race);
    assert(raceAnimation.isActive());
    manualAdvanceDisplay(race);
    updateDisplay(race);
    assert(currentDisplayState == STATE_RACE_WEEK_EVENT_DETAILS);
    assert(!raceAnimation.isActive());

    // A live session ending during a pass cancels it for post-race immediately.
    currentDisplayState = STATE_RACE_WEEK_COUNTDOWN;
    requestRedraw();
    updateDisplay(race);
    fakeUtc = gp + 7200;
    updateDisplay(race);
    assert(currentDisplayState == STATE_POST_RACE_WINNER);
    assert(!raceAnimation.isActive());

    // Countdown -> event details -> schedule still uses normal timed rotation.
    fakeUtc = gp - 3600;
    fakeMillis = 30000;
    updateDisplay(race);
    assert(currentDisplayState == STATE_RACE_WEEK_COUNTDOWN);
    assert(!raceAnimation.isActive());
    fakeMillis += DISPLAY_ROTATE_MS;
    updateDisplay(race);
    assert(currentDisplayState == STATE_RACE_WEEK_EVENT_DETAILS);
    fakeMillis += DISPLAY_ROTATE_MS;
    updateDisplay(race);
    assert(currentDisplayState == STATE_RACE_WEEK_SCHEDULE);

    // A late live transition owns its full duration beyond the old 8s deadline.
    currentDisplayState = STATE_RACE_WEEK_COUNTDOWN;
    lastDisplayChange = fakeMillis;
    requestRedraw();
    updateDisplay(race);
    fakeMillis += DISPLAY_ROTATE_MS - 1000;
    fakeUtc = gp;
    updateDisplay(race);
    assert(raceAnimation.isActive());
    fakeMillis += 1000;
    updateDisplay(race);
    assert(currentDisplayState == STATE_RACE_WEEK_COUNTDOWN);
    manualAdvanceDisplay(race);

    // Real framebuffer composition: clipped edges, cadence, byte-order restore,
    // no leftover car/smoke, single completion, and uint32 millis wraparound.
    TFT_eSPI frame;
    RaceAnimation animation;
    uint32_t start = UINT32_MAX - 100;
    animation.start(start);
    assert(!animation.update(frame, start));
    int rows = frame.imageRows;
    assert(!animation.update(frame, start + 1));
    assert(frame.imageRows == rows);
    assert(!frame.getSwapBytes());
    std::ofstream preview;
    if (argc > 1) preview.open(argv[1], std::ios::binary);
    bool sawCar = false, sawSmoke = false;
    for (uint32_t elapsed = ON_NOW_FRAME_MS; elapsed < ON_NOW_ANIMATION_MS;
         elapsed += ON_NOW_FRAME_MS) {
        assert(!animation.update(frame, start + elapsed));
        for (int y = 94; y < 150; ++y) {
            for (int x = 0; x < SCREEN_WIDTH; ++x) {
                uint16_t pixel = frame.pixels[y * SCREEN_WIDTH + x];
                sawCar |= pixel == 0xD800;  // Source livery accent (220, 0, 0)
                sawSmoke |= pixel == frame.color565(150, 150, 150);
                if (preview.is_open()) {
                    char rgb[] = {char(((pixel >> 11) & 31) * 255 / 31),
                                  char(((pixel >> 5) & 63) * 255 / 63),
                                  char((pixel & 31) * 255 / 31)};
                    preview.write(rgb, 3);
                }
            }
        }
    }
    assert(sawCar && sawSmoke);
    for (int y = 94; y < 150; ++y)
        for (int x = 0; x < SCREEN_WIDTH; ++x)
            assert(frame.pixels[y * SCREEN_WIDTH + x] == (y == 136 ? COLOR_GRID : COLOR_BG));
    assert(animation.update(frame, start + ON_NOW_ANIMATION_MS));
    assert(!animation.update(frame, start + ON_NOW_ANIMATION_MS + 1));
    std::cout << "PASS: GP timing, midnight, redraws, completion, tap, phase change, "
                 "normal rotation, late start, frame composition and millis wraparound\n";
}
