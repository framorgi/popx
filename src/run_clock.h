#pragma once
#include <chrono>
#include <cmath>
#include <cstdint>

/// @brief Biological time is fixed; speed changes only wall-clock scheduling.
struct RunClock {
    static uint64_t duration_ticks(double minutes) {
        return static_cast<uint64_t>(std::ceil(minutes * 6000.0));
    }
    static bool expired(uint64_t tick, double minutes) {
        return minutes > 0 && tick >= duration_ticks(minutes);
    }
    static std::chrono::microseconds interval(unsigned speed) {
        return std::chrono::microseconds(speed ? 10000 / speed : 0);
    }
};
