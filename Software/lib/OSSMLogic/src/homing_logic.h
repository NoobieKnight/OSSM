#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

// Pure logic extracted from src/ossm/homing/homing.cpp.
// No hardware dependencies — testable on native platform.

namespace homing_logic {

/// Check if measured current exceeds the sensorless homing threshold.
/// homing.cpp lines 98-99
inline bool isCurrentOverLimit(float currentReading, float offset,
                               float threshold) {
    return (currentReading - offset) > threshold;
}

/// Check if homing has exceeded the timeout.
/// homing.cpp line 81
inline bool isHomingTimedOut(uint32_t elapsedMs, uint32_t timeoutMs) {
    return elapsedMs > timeoutMs;
}

}  // namespace homing_logic
