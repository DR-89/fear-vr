#pragma once

#include <cmath>

namespace fearvr {

constexpr float kPhysicalDuckEnterMeters = 0.26F;
constexpr float kPhysicalDuckReleaseMeters = 0.18F;

inline bool UpdatePhysicalDuck(
    bool enabled, bool trackingValid, float currentHeadHeightMeters,
    float recenteredHeadHeightMeters, bool wasDucking) noexcept {
    if (!enabled || !trackingValid ||
        !std::isfinite(currentHeadHeightMeters) ||
        !std::isfinite(recenteredHeadHeightMeters)) {
        return false;
    }
    const float loweredMeters =
        recenteredHeadHeightMeters - currentHeadHeightMeters;
    return wasDucking
        ? loweredMeters > kPhysicalDuckReleaseMeters
        : loweredMeters >= kPhysicalDuckEnterMeters;
}

} // namespace fearvr
