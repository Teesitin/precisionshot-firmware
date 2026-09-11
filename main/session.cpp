#include "session.h"

#include <cstdio>
#include <limits>

namespace training {

void Session::reset() {
    shots = 0;
    total = 0;
    lastScore = 0;
    hasShot = false;
    complete = false;
}

void Session::setMode(Mode nextMode) {
    if (mode == nextMode) return;
    mode = nextMode;
    reset();
}

void Session::recordShot(unsigned score) {
    // The shot after a completed Classic round starts the next round and counts
    // as its first shot. Keep the completed total visible until that shot.
    if (mode == Mode::Classic && complete) reset();
    if (score > 10) score = 10;
    lastScore = static_cast<int>(score);
    hasShot = true;

    constexpr uint32_t maximum = std::numeric_limits<uint32_t>::max();
    if (shots < maximum) ++shots;
    total = total > maximum - score ? maximum : total + score;
    complete = mode == Mode::Classic && shots >= 10;
}

unsigned Session::remaining() const {
    return mode == Mode::Classic && shots < 10 ? 10 - shots : 0;
}

unsigned Session::displayScore() const {
    return complete ? total : static_cast<unsigned>(lastScore);
}

void Settings::adjustDistance(int steps) {
    // A foot is 304.8 mm, rounded to the nearest millimeter for storage. Use
    // wide arithmetic so very large input increments cannot overflow.
    const int64_t increment = unit == Unit::Meters
        ? static_cast<int64_t>(steps) * 1000
        : (static_cast<int64_t>(steps) * 3048 + (steps < 0 ? -5 : 5)) / 10;
    const int64_t next = static_cast<int64_t>(distanceMillimeters) + increment;
    distanceMillimeters = static_cast<int>(
        next < kMinimumDistanceMillimeters ? kMinimumDistanceMillimeters :
        next > kMaximumDistanceMillimeters ? kMaximumDistanceMillimeters : next);
}

void Settings::adjustSensitivity(int steps) {
    const int64_t next = static_cast<int64_t>(sensitivity) +
                         static_cast<int64_t>(steps) * 100;
    sensitivity = static_cast<unsigned>(
        next < 0 ? 0 : next > kMaximumSensitivity ? kMaximumSensitivity : next);
}

void Settings::toggleUnit() {
    unit = unit == Unit::Meters ? Unit::Feet : Unit::Meters;
}

void Settings::formatDistance(char* buffer, size_t size) const {
    if (buffer == nullptr || size == 0) return;
    const int64_t tenths = unit == Unit::Meters
        ? (static_cast<int64_t>(distanceMillimeters) + 50) / 100
        : (static_cast<int64_t>(distanceMillimeters) * 100 + 1524) / 3048;
    std::snprintf(buffer, size, "%ld.%ld %s", static_cast<long>(tenths / 10),
                  static_cast<long>(tenths % 10), unit == Unit::Meters ? "m" : "ft");
}

}  // namespace training
