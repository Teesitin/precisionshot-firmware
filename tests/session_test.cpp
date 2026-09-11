#include "session.h"

// Keep checks active in both optimized host builds and device diagnostics.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstring>
#include <limits>

using training::Mode;
using training::Session;
using training::Settings;
using training::Unit;

static void classicRounds() {
    Session session;
    assert(session.mode == Mode::Classic);
    assert(!session.hasShot && !session.complete && session.remaining() == 10);
    for (unsigned i = 1; i <= 9; ++i) {
        session.recordShot(i);
        assert(session.shots == i && session.lastScore == static_cast<int>(i));
        assert(session.displayScore() == i && session.remaining() == 10 - i);
        assert(session.hasShot && !session.complete);
    }
    session.recordShot(10);
    assert(session.shots == 10 && session.total == 55);
    assert(session.complete && session.remaining() == 0);
    assert(session.lastScore == 10 && session.displayScore() == 55);

    // Re-selecting the current mode must not clear a completed round.
    session.setMode(Mode::Classic);
    assert(session.complete && session.displayScore() == 55);
    session.recordShot(7);
    assert(!session.complete && session.shots == 1 && session.total == 7);
    assert(session.displayScore() == 7 && session.remaining() == 9);
    session.reset();
    assert(!session.hasShot && session.shots == 0 && session.total == 0);
    assert(session.displayScore() == 0 && session.remaining() == 10);
}

static void missesAndLimits() {
    Session session;
    session.recordShot(0);
    assert(session.hasShot && session.shots == 1 && session.lastScore == 0);
    assert(session.remaining() == 9);
    session.recordShot(std::numeric_limits<unsigned>::max());
    assert(session.lastScore == 10 && session.total == 10);
    session.reset();
    for (unsigned i = 0; i < 10; ++i) session.recordShot(10);
    assert(session.complete && session.displayScore() == 100);
    session.recordShot(0);
    assert(!session.complete && session.shots == 1 && session.total == 0);
    session.reset();
    for (unsigned i = 0; i < 10; ++i) session.recordShot(0);
    assert(session.complete && session.displayScore() == 0);
}

static void freestyleAndModeChanges() {
    Session session;
    session.recordShot(8);
    session.setMode(Mode::Freestyle);
    assert(session.shots == 0 && !session.hasShot && !session.complete);
    for (unsigned i = 1; i <= 25; ++i) {
        session.recordShot(8);
        assert(session.shots == i && session.total == i * 8);
        assert(!session.complete && session.displayScore() == 8);
        assert(session.remaining() == 0);
    }
    session.setMode(Mode::Freestyle);
    assert(session.shots == 25);
    session.reset();
    assert(session.mode == Mode::Freestyle && session.shots == 0);

    // Very long Freestyle sessions keep their last shot without wrapping counts.
    session.shots = std::numeric_limits<uint32_t>::max();
    session.total = std::numeric_limits<uint32_t>::max() - 1;
    session.recordShot(10);
    assert(session.shots == std::numeric_limits<uint32_t>::max());
    assert(session.total == std::numeric_limits<uint32_t>::max());
    assert(session.displayScore() == 10 && !session.complete);
    session.setMode(Mode::Classic);
    assert(session.mode == Mode::Classic && session.remaining() == 10);
    assert(session.shots == 0 && session.total == 0 && !session.hasShot);
}

static void distanceAndSensitivity() {
    Settings settings;
    char buffer[24];
    settings.formatDistance(buffer, sizeof(buffer));
    assert(std::strcmp(buffer, "10.0 m") == 0);
    settings.toggleUnit();
    assert(settings.unit == Unit::Feet && settings.distanceMillimeters == 10000);
    settings.formatDistance(buffer, sizeof(buffer));
    assert(std::strcmp(buffer, "32.8 ft") == 0);
    settings.adjustDistance(1);
    settings.formatDistance(buffer, sizeof(buffer));
    assert(std::strcmp(buffer, "33.8 ft") == 0);
    settings.adjustDistance(-1);
    assert(settings.distanceMillimeters == 10000);
    settings.toggleUnit();
    assert(settings.unit == Unit::Meters && settings.distanceMillimeters == 10000);
    settings.adjustDistance(1);
    assert(settings.distanceMillimeters == 11000);
    settings.adjustDistance(INT_MIN);
    assert(settings.distanceMillimeters == 1000);
    settings.adjustDistance(INT_MAX);
    assert(settings.distanceMillimeters == 100000);
    settings.formatDistance(buffer, sizeof(buffer));
    assert(std::strcmp(buffer, "100.0 m") == 0);
    settings.toggleUnit();
    settings.adjustDistance(INT_MIN);
    assert(settings.distanceMillimeters == 1000);
    settings.adjustDistance(INT_MAX);
    assert(settings.distanceMillimeters == 100000);
    settings.formatDistance(buffer, sizeof(buffer));
    assert(std::strcmp(buffer, "328.1 ft") == 0);

    settings.adjustSensitivity(1);
    assert(settings.sensitivity == 1100);
    settings.adjustSensitivity(INT_MIN);
    assert(settings.sensitivity == 0);
    settings.adjustSensitivity(INT_MAX);
    assert(settings.sensitivity == 4095);

    // Bounded formatting is safe for truncated and absent output buffers.
    char tiny[2] = {'x', 'x'};
    settings.formatDistance(tiny, sizeof(tiny));
    assert(tiny[1] == '\0');
    settings.formatDistance(nullptr, 0);
}

#ifdef PRECISIONSHOT_EMBEDDED_TEST
int runSessionTests() {
#else
int main() {
#endif
    classicRounds();
    missesAndLimits();
    freestyleAndModeChanges();
    distanceAndSensitivity();
    std::puts("session_test: all session and settings checks passed");
    return 0;
}
