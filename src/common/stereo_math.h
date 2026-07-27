#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "protocol.h"

namespace fearvr {

inline constexpr float kGameUnitsPerMeter = 100.0F;

struct SymmetricFov {
    float halfHorizontal{0.0F};
    float halfVertical{0.0F};
    bool valid{false};
};

inline SymmetricFov SharedSymmetricFov(
    const FearVrFov& left, const FearVrFov& right) noexcept {
    const float horizontal = std::min(
        std::min(-left.angleLeft, left.angleRight),
        std::min(-right.angleLeft, right.angleRight));
    const float vertical = std::min(
        std::min(left.angleUp, -left.angleDown),
        std::min(right.angleUp, -right.angleDown));
    const bool valid =
        std::isfinite(horizontal) && std::isfinite(vertical) &&
        horizontal > 0.01F && vertical > 0.01F;
    return {valid ? horizontal : 0.0F,
            valid ? vertical : 0.0F, valid};
}

inline SymmetricFov ScaleSymmetricFov(
    const SymmetricFov& fov, float scale) noexcept {
    if (!fov.valid || !std::isfinite(scale) || scale <= 0.0F) {
        return {};
    }
    // FOV-Skalierung ist eine Projektionsskalierung: tan(halfFov) wird
    // skaliert, nicht der Winkel selbst. So bleiben Kamera und Compositor
    // geometrisch deckungsgleich.
    const float horizontal =
        std::atan(std::tan(fov.halfHorizontal) * scale);
    const float vertical =
        std::atan(std::tan(fov.halfVertical) * scale);
    const bool valid =
        std::isfinite(horizontal) && std::isfinite(vertical) &&
        horizontal > 0.01F && vertical > 0.01F &&
        horizontal < 1.55F && vertical < 1.55F;
    return {valid ? horizontal : 0.0F,
            valid ? vertical : 0.0F, valid};
}

inline FearVrFov ToProtocolFov(const SymmetricFov& fov) noexcept {
    if (!fov.valid) {
        return {};
    }
    return {-fov.halfHorizontal, fov.halfHorizontal,
            fov.halfVertical, -fov.halfVertical};
}

inline float InterpupillaryDistanceMeters(
    const FearVrRenderRequest& request) noexcept {
    const FearVrPose& left =
        request.eye[FEARVR_EYE_LEFT].pose;
    const FearVrPose& right =
        request.eye[FEARVR_EYE_RIGHT].pose;
    const float dx = right.px - left.px;
    const float dy = right.py - left.py;
    const float dz = right.pz - left.pz;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

inline float EyeOffsetGameUnits(const FearVrRenderRequest& request,
                                std::uint32_t eye,
                                float unitsPerMeter =
                                    kGameUnitsPerMeter) noexcept {
    if (eye >= FEARVR_EYE_COUNT || !std::isfinite(unitsPerMeter) ||
        unitsPerMeter <= 0.0F) {
        return 0.0F;
    }
    const float halfIpd =
        InterpupillaryDistanceMeters(request) * 0.5F;
    if (!std::isfinite(halfIpd)) {
        return 0.0F;
    }
    return (eye == FEARVR_EYE_LEFT ? -halfIpd : halfIpd) *
           unitsPerMeter;
}

} // namespace fearvr
