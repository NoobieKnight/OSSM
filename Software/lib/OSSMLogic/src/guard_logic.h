#pragma once

// Pure predicate logic extracted from src/ossm/state/guards.cpp.
// No hardware dependencies — testable on native platform.

namespace guard_logic {

/// Check if the device is NOT homed.
/// guards.cpp lines 14-15 (ossmIsNotHomed)
inline bool isNotHomedLogic(bool isHomed) { return !isHomed; }

}  // namespace guard_logic
