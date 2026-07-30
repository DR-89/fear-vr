#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace fearvr {

// Header-only weapon obstruction model. The game-facing hook supplies the
// distance from the grip to the first world hit; this code decides how far the
// virtual weapon should retract and smooths that correction across frames.
//
// Collision belongs after controller/two-handed/weight pose generation and
// before the final weapon, muzzle, laser and shot transforms are committed.

// Small space left between the virtual muzzle and the contacted surface.
constexpr float kWeaponCollisionMarginUnits = 8.0F;
// Retraction must react quickly when the player moves the weapon into a wall.
constexpr float kWeaponCollisionTightenSeconds = 0.025F;
// Release is deliberately slower to avoid a visible pop away from a surface.
constexpr float kWeaponCollisionReleaseSeconds = 0.080F;
// Briefly retain the deepest obstruction when a ray alternates at an edge.
constexpr std::uint64_t kWeaponCollisionHoldNs = 90'000'000ULL;
// After a pause/loading screen, accept the new state immediately.
constexpr std::uint64_t kWeaponCollisionMaxGapNs = 200'000'000ULL;

struct WeaponCollisionState {
    float retractionUnits{0.0F};
    float heldTargetUnits{0.0F};
    std::uint64_t holdUntilNs{0};
    std::uint64_t lastUpdateNs{0};
    bool haveUpdate{false};
};

inline void ResetWeaponCollision(WeaponCollisionState& state) noexcept {
    state = WeaponCollisionState{};
}

inline float ClampWeaponCollisionValue(
    float value, float minimum, float maximum) noexcept {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

// Compute the target backward movement of the weapon.
//
// weaponLengthUnits: distance from the primary grip to the muzzle.
// freeDistanceUnits: distance from the primary grip to the first world hit.
// hit: whether the world query found an obstruction.
// marginUnits: clearance retained in front of the contacted surface.
// maxRetractionUnits: optional per-weapon cap.
inline float WeaponCollisionRetraction(
    float weaponLengthUnits,
    float freeDistanceUnits,
    bool hit,
    float marginUnits = kWeaponCollisionMarginUnits,
    float maxRetractionUnits = std::numeric_limits<float>::infinity()) noexcept {
    if (!std::isfinite(weaponLengthUnits) || weaponLengthUnits <= 0.0F) {
        return 0.0F;
    }
    if (!hit || !std::isfinite(freeDistanceUnits) ||
        freeDistanceUnits < 0.0F) {
        return 0.0F;
    }
    if (!std::isfinite(marginUnits) || marginUnits < 0.0F) {
        marginUnits = 0.0F;
    }
    if (std::isnan(maxRetractionUnits) || maxRetractionUnits <= 0.0F) {
        return 0.0F;
    }

    const float required =
        weaponLengthUnits + marginUnits - freeDistanceUnits;
    if (required <= 0.0F) {
        return 0.0F;
    }

    float maximum = weaponLengthUnits;
    if (std::isfinite(maxRetractionUnits) && maxRetractionUnits < maximum) {
        maximum = maxRetractionUnits;
    }
    return ClampWeaponCollisionValue(required, 0.0F, maximum);
}

// True when the normal muzzle position reaches or crosses the protected
// clearance around a surface. This can be used to suppress firing while the
// weapon is heavily obstructed, independently of visual smoothing.
inline bool WeaponMuzzleObstructed(
    float weaponLengthUnits,
    float freeDistanceUnits,
    bool hit,
    float marginUnits = kWeaponCollisionMarginUnits) noexcept {
    if (!std::isfinite(weaponLengthUnits) || weaponLengthUnits <= 0.0F ||
        !hit || !std::isfinite(freeDistanceUnits) ||
        freeDistanceUnits < 0.0F) {
        return false;
    }
    if (!std::isfinite(marginUnits) || marginUnits < 0.0F) {
        marginUnits = 0.0F;
    }
    return freeDistanceUnits <= weaponLengthUnits + marginUnits;
}

inline float UpdateWeaponCollision(
    WeaponCollisionState& state,
    float targetRetractionUnits,
    std::uint64_t nowNs) noexcept {
    if (!std::isfinite(targetRetractionUnits)) {
        return state.retractionUnits;
    }
    if (targetRetractionUnits < 0.0F) {
        targetRetractionUnits = 0.0F;
    }

    float seconds = 0.0F;
    if (state.haveUpdate && nowNs > state.lastUpdateNs &&
        nowNs - state.lastUpdateNs <= kWeaponCollisionMaxGapNs) {
        seconds = static_cast<float>(nowNs - state.lastUpdateNs) * 1.0e-9F;
    }
    state.lastUpdateNs = nowNs;
    state.haveUpdate = true;

    // More obstruction takes effect immediately as the new held target.
    // Less obstruction is accepted only after a short edge-stability window.
    if (targetRetractionUnits >= state.heldTargetUnits) {
        state.heldTargetUnits = targetRetractionUnits;
        state.holdUntilNs = nowNs + kWeaponCollisionHoldNs;
    } else if (nowNs >= state.holdUntilNs) {
        state.heldTargetUnits = targetRetractionUnits;
    }
    targetRetractionUnits = state.heldTargetUnits;

    if (seconds <= 0.0F) {
        state.retractionUnits = targetRetractionUnits;
        return state.retractionUnits;
    }

    const float timeConstant =
        targetRetractionUnits > state.retractionUnits
            ? kWeaponCollisionTightenSeconds
            : kWeaponCollisionReleaseSeconds;
    float weight = seconds / timeConstant;
    if (weight > 1.0F) {
        weight = 1.0F;
    }
    state.retractionUnits +=
        (targetRetractionUnits - state.retractionUnits) * weight;
    return state.retractionUnits;
}

} // namespace fearvr
