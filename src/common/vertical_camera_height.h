#pragma once

#include <cmath>

namespace fearvr {

// Richtungsabhaengige Wahl der visuellen Kamerahoehe.
//
// Retails 275-ms-Filter wirkt treppauf angenehm, treppab aber wie kurzes
// Schweben mit anschliessendem Nachfallen. Beim Sprung kann ein bereits
// laufender Filter ebenfalls noch auslaufen. Deshalb darf die Darstellung in
// diesen beiden Faellen voruebergehend die bereits berechnete Rohhoehe nutzen,
// waehrend Retail intern unveraendert weiterrechnet. Erst wenn beide Hoehen
// wieder zusammenliegen, wird ohne Sprung zur finalen Kamera zurueckgeschaltet.
constexpr float kVerticalHeightDownStepEpsilon = 0.05F;
constexpr float kVerticalHeightCatchupTolerance = 0.25F;
// Eine groessere Differenz kann eine echte Decken-/Bodenkollision sein. Dann
// hat Retails finale Anti-Clipping-Position Vorrang fuer das ganze Ereignis.
constexpr float kVerticalHeightCollisionGuardDistance = 35.0F;

struct VerticalCameraHeightState {
    float lastRawHeight{0.0F};
    bool haveRawHeight{false};
    bool bypassActive{false};
    bool collisionGuardActive{false};
};

struct VerticalCameraHeightOutput {
    float visualHeight{0.0F};
    bool bypassActive{false};
    bool descending{false};
};

inline void ResetVerticalCameraHeight(
    VerticalCameraHeightState& state) noexcept {
    state = VerticalCameraHeightState{};
}

inline VerticalCameraHeightOutput UpdateVerticalCameraHeight(
    VerticalCameraHeightState& state,
    float rawHeight,
    float finalHeight,
    bool airborne,
    bool ducking,
    bool movementStateAvailable) noexcept {
    VerticalCameraHeightOutput output;
    output.visualHeight = finalHeight;
    if (!std::isfinite(rawHeight) || !std::isfinite(finalHeight) ||
        !movementStateAvailable) {
        ResetVerticalCameraHeight(state);
        return output;
    }

    const bool descending =
        state.haveRawHeight &&
        rawHeight <
            state.lastRawHeight - kVerticalHeightDownStepEpsilon &&
        !ducking;
    const bool bypassRequested = airborne || descending;
    const float difference = std::fabs(rawHeight - finalHeight);

    if (bypassRequested &&
        difference > kVerticalHeightCollisionGuardDistance) {
        state.collisionGuardActive = true;
        state.bypassActive = false;
    }

    if (state.collisionGuardActive) {
        if (!bypassRequested &&
            difference <= kVerticalHeightCatchupTolerance) {
            state.collisionGuardActive = false;
        }
    } else {
        if (bypassRequested) {
            state.bypassActive = true;
        } else if (
            state.bypassActive &&
            difference <= kVerticalHeightCatchupTolerance) {
            state.bypassActive = false;
        }
    }

    state.lastRawHeight = rawHeight;
    state.haveRawHeight = true;
    output.descending = descending;
    output.bypassActive = state.bypassActive;
    output.visualHeight =
        state.bypassActive ? rawHeight : finalHeight;
    return output;
}

} // namespace fearvr
