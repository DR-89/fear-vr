#pragma once

#include <cmath>
#include <cstdint>

#include "head_tracking_math.h"
#include "protocol.h"

namespace fearvr {

struct LocomotionReprojection {
    // Zusatzversatz fuer die Pose des alten Quellbilds in OpenXR-LOCAL.
    TrackingVector sourcePoseOffsetMeters{};
    std::uint64_t ageFrames{0};
    float distanceMeters{0.0F};
    bool valid{false};
};

inline bool IsValidGameCameraSample(
    const FearVrGameCameraSample& sample) noexcept {
    return (sample.flags & FEARVR_GCF_VALID) != 0 &&
           sample.frameId != 0 && IsValidPose(sample.pose);
}

// F.E.A.R. bewegt die Kamera unabhaengig vom HMD im Spielraum. Ein altes
// Bild muss deshalb fuer OpenXR so beschrieben werden, als waere seine
// Quellpose um die seitdem erfolgte Spielbewegung zurueckversetzt. Der
// Compositor kann dann sowohl diese kuenstliche Translation als auch die
// echte, neuere HMD-Pose in einem Schritt nachprojizieren.
//
// Nur horizontale Bewegung wird beruecksichtigt: Head-Bob, Treppen und
// Kamerahoehenkorrekturen sollen den XR-Layer nicht vertikal springen lassen.
// Teleports, Levelwechsel und sehr alte Bilder werden fail-closed verworfen.
inline LocomotionReprojection ComputeLocomotionReprojection(
    const FearVrGameCameraSample& imageCamera,
    const FearVrGameCameraSample& latestCamera,
    float maximumDistanceMeters = 0.5F,
    std::uint64_t maximumAgeFrames = 16) noexcept {
    LocomotionReprojection result{};
    if (!IsValidGameCameraSample(imageCamera) ||
        !IsValidGameCameraSample(latestCamera) ||
        latestCamera.frameId < imageCamera.frameId ||
        !std::isfinite(maximumDistanceMeters) ||
        maximumDistanceMeters <= 0.0F) {
        return result;
    }

    result.ageFrames = latestCamera.frameId - imageCamera.frameId;
    if (result.ageFrames > maximumAgeFrames) {
        return result;
    }

    // Einen kuenstlichen Snap-/Smooth-Turn kann eine reine Translation nicht
    // korrekt repraesentieren. War die Basisrotation inzwischen mehr als
    // ungefaehr fuenf Grad weitergedreht, warten wir auf das neue Bild.
    const float rotationAlignment = std::fabs(Dot(
        PoseRotation(imageCamera.pose),
        PoseRotation(latestCamera.pose)));
    constexpr float kMinimumRotationAlignment = 0.99904822F;
    if (!std::isfinite(rotationAlignment) ||
        rotationAlignment < kMinimumRotationAlignment) {
        return result;
    }

    const TrackingVector planarWorldDelta{
        latestCamera.pose.px - imageCamera.pose.px,
        0.0F,
        latestCamera.pose.pz - imageCamera.pose.pz};
    const float distanceSquared =
        planarWorldDelta.x * planarWorldDelta.x +
        planarWorldDelta.z * planarWorldDelta.z;
    if (!IsFinite(planarWorldDelta) || !std::isfinite(distanceSquared)) {
        return result;
    }
    result.distanceMeters = std::sqrt(distanceSquared);
    if (!std::isfinite(result.distanceMeters) ||
        result.distanceMeters > maximumDistanceMeters) {
        return result;
    }

    // Welt -> damalige Spielkamera -> OpenXR-Achsen. Die Achsenabbildung
    // OpenXrToLithTech ist ihr eigenes Inverses.
    const TrackingVector localLithTechDelta = Rotate(
        Conjugate(PoseRotation(imageCamera.pose)), planarWorldDelta);
    TrackingVector localOpenXrDelta =
        OpenXrToLithTech(localLithTechDelta);
    localOpenXrDelta.y = 0.0F;
    if (!IsFinite(localOpenXrDelta)) {
        return result;
    }

    result.sourcePoseOffsetMeters = {
        -localOpenXrDelta.x, 0.0F, -localOpenXrDelta.z};
    result.valid = IsFinite(result.sourcePoseOffsetMeters);
    return result;
}

} // namespace fearvr
