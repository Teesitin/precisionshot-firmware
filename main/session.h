#pragma once

#include <cstddef>
#include <cstdint>

namespace training {

enum class Mode { Freestyle, Classic };
enum class Unit { Meters, Feet };

// Scores shared by the normal and fullscreen views.
struct Session {
    Mode mode = Mode::Classic;
    uint32_t shots = 0;
    uint32_t total = 0;
    int lastScore = 0;
    bool hasShot = false;
    bool complete = false;

    void reset();
    void setMode(Mode nextMode);
    void recordShot(unsigned score);
    unsigned remaining() const;
    unsigned displayScore() const;
};

struct Settings {
    static constexpr int kMinimumDistanceMillimeters = 1000;
    static constexpr int kMaximumDistanceMillimeters = 100000;
    static constexpr unsigned kMaximumSensitivity = 4095;

    // Changing units does not change the actual distance.
    int distanceMillimeters = 10000;
    unsigned sensitivity = 1000;
    Unit unit = Unit::Meters;

    // Each step changes the distance by one meter or approximately one foot.
    void adjustDistance(int steps);
    // Each step changes the sensor threshold by 100.
    void adjustSensitivity(int steps);
    void toggleUnit();
    void formatDistance(char *buffer, size_t size) const;
};

} // namespace training
