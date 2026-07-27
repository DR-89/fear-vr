#pragma once

#include <cmath>
#include <cstdint>

#include "protocol.h"

namespace fearvr {

// Nahkampf per Geste: Die dominante Hand wird schnell nach vorne gestossen.
// Ausgewertet wird ausschliesslich die Zielpose der Waffenhand — nach
// `MirrorInputHandedness` ist das immer FEARVR_HAND_RIGHT, auch im
// Linkshaendermodus.
//
// Die Bewegung muss zwei Dinge zugleich erfuellen: schnell sein *und* entlang
// der eigenen Blickrichtung der Hand laufen. Nur die Geschwindigkeit zu messen
// wuerde jedes Zurueckziehen, jedes Schwenken und jeden Ruck beim Nachladen
// mitzaehlen.

// Vorwaertsanteil der Handgeschwindigkeit, ab dem der Stoss gilt. Ein
// beilaeufiges Anheben der Waffe bleibt deutlich darunter, ein bewusster Stoss
// erreicht ein Mehrfaches.
constexpr float kMeleeThrustSpeedMps = 2.0F;
// cos(50 Grad): Der Stoss muss grob dorthin gehen, wohin die Hand zeigt.
constexpr float kMeleeThrustMinAlignment = 0.64F;
// Erst unter diesem Vorwaertsanteil ist die Geste wieder scharf. Verhindert,
// dass ein einziger langer Stoss mehrfach zaehlt.
constexpr float kMeleeThrustRearmSpeedMps = 0.8F;
// Sperre nach einem Treffer. Retail spielt die Nahkampfanimation ab; schneller
// nachzusetzen ergibt im Spiel ohnehin keinen zweiten Schlag.
constexpr std::uint64_t kMeleeThrustCooldownNs = 700000000ULL;
// Groesster Abstand zweier Abtastungen, aus dem noch eine Geschwindigkeit
// gebildet werden darf. Nach einem Ladebildschirm oder einem Frameeinbruch
// waere die Differenz sonst ein Sprung und kein Stoss.
constexpr std::uint64_t kMeleeThrustMaxSampleGapNs = 100000000ULL;

struct MeleeThrustDetector {
    std::uint64_t lastSampleTimeNs{0};
    std::uint64_t cooldownUntilNs{0};
    float lastX{0.0F};
    float lastY{0.0F};
    float lastZ{0.0F};
    // Groesster Vorwaertsanteil seit dem letzten Auslesen, und wie viele
    // Abtastungen ueberhaupt zu einer Geschwindigkeit gefuehrt haben. Nur
    // Diagnose, aber die entscheidende: Bleibt `evaluatedSamples` null,
    // scheitert es an der Zeitbasis oder den Posen; ist nur
    // `peakForwardSpeed` zu klein, ist die Schwelle zu hoch gesetzt.
    float peakForwardSpeed{0.0F};
    std::uint32_t evaluatedSamples{0};
    bool haveSample{false};
    // Nach einem erkannten Stoss erst wieder scharf, wenn die Hand langsamer
    // geworden ist.
    bool armed{true};
};

struct MeleeThrustVector {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
};

// Eigene Blickrichtung einer Pose: das gedrehte -Z. OpenXR ist rechtshaendig
// mit -Z nach vorne.
inline MeleeThrustVector PoseForwardAxis(
    const FearVrPose& pose) noexcept {
    const float x = pose.qx;
    const float y = pose.qy;
    const float z = pose.qz;
    const float w = pose.qw;
    const float lengthSqr = x * x + y * y + z * z + w * w;
    if (!std::isfinite(lengthSqr) || lengthSqr < 1.0e-6F) {
        return {};
    }
    // Die dritte Spalte der Rotationsmatrix, negiert. Beide Terme tragen
    // denselben Faktor |q|^2, deshalb ist die Division die Normalisierung
    // einer nicht exakt normierten Quaternion.
    const MeleeThrustVector forward{
        -2.0F * (x * z + w * y) / lengthSqr,
        -2.0F * (y * z - w * x) / lengthSqr,
        -(lengthSqr - 2.0F * (x * x + y * y)) / lengthSqr};
    return forward;
}

inline void ResetMeleeThrust(MeleeThrustDetector& detector) noexcept {
    detector = MeleeThrustDetector{};
}

// Eine Abtastung einspeisen. Liefert genau in dem Aufruf `true`, in dem der
// Stoss erkannt wird.
//
// `nowNs` ist die Uhr des Aufrufers, nicht der Zeitstempel aus dem
// Eingabezustand: Dessen `predictedDisplayTimeNs` ist eine *vorhergesagte*
// Anzeigezeit, die zwischen zwei Abholungen gleich bleiben oder springen
// kann. Beides macht die Geschwindigkeit unbrauchbar — mit gleichem
// Zeitstempel wird jede Abtastung verworfen, und genau daran ist die erste
// Fassung im Spiel gescheitert.
//
// `hand` ist die dominante Hand im bereits gespiegelten Zustand.
inline bool UpdateMeleeThrust(
    MeleeThrustDetector& detector,
    const FearVrInputState& input,
    std::uint64_t nowNs,
    std::uint32_t hand = FEARVR_HAND_RIGHT,
    bool suppressed = false) noexcept {
    if (hand >= FEARVR_HAND_COUNT) {
        return false;
    }
    const std::uint32_t handMask =
        hand == FEARVR_HAND_LEFT ? static_cast<std::uint32_t>(
                                       FEARVR_HAND_MASK_LEFT)
                                 : static_cast<std::uint32_t>(
                                       FEARVR_HAND_MASK_RIGHT);
    if ((input.flags & FEARVR_IF_VALID) == 0 ||
        (input.flags & FEARVR_IF_FOCUSED) == 0 ||
        (input.activeHands & handMask) == 0 ||
        (input.aimPoseValidHands & handMask) == 0) {
        // Ohne verwertbare Pose bleibt keine Historie stehen, aus der beim
        // Wiederauftauchen eine Scheingeschwindigkeit entstehen koennte.
        detector.haveSample = false;
        return false;
    }

    const FearVrPose& pose = input.handAimPose[hand];
    if (!std::isfinite(pose.px) || !std::isfinite(pose.py) ||
        !std::isfinite(pose.pz)) {
        detector.haveSample = false;
        return false;
    }

    const bool hadSample = detector.haveSample;
    const std::uint64_t previousNs = detector.lastSampleTimeNs;
    const float previousX = detector.lastX;
    const float previousY = detector.lastY;
    const float previousZ = detector.lastZ;

    detector.haveSample = true;
    detector.lastSampleTimeNs = nowNs;
    detector.lastX = pose.px;
    detector.lastY = pose.py;
    detector.lastZ = pose.pz;

    if (!hadSample || nowNs <= previousNs ||
        nowNs - previousNs > kMeleeThrustMaxSampleGapNs) {
        return false;
    }

    const float seconds =
        static_cast<float>(nowNs - previousNs) * 1.0e-9F;
    const float dx = (pose.px - previousX) / seconds;
    const float dy = (pose.py - previousY) / seconds;
    const float dz = (pose.pz - previousZ) / seconds;
    const float speedSqr = dx * dx + dy * dy + dz * dz;
    if (!std::isfinite(speedSqr)) {
        return false;
    }

    const MeleeThrustVector forward = PoseForwardAxis(pose);
    const float forwardSpeed =
        dx * forward.x + dy * forward.y + dz * forward.z;
    ++detector.evaluatedSamples;
    if (forwardSpeed > detector.peakForwardSpeed) {
        detector.peakForwardSpeed = forwardSpeed;
    }

    if (forwardSpeed < kMeleeThrustRearmSpeedMps &&
        nowNs >= detector.cooldownUntilNs) {
        detector.armed = true;
    }

    if (!detector.armed || suppressed ||
        nowNs < detector.cooldownUntilNs) {
        return false;
    }
    if (forwardSpeed < kMeleeThrustSpeedMps) {
        return false;
    }
    // Der Vorwaertsanteil an der Gesamtbewegung. Ein seitlicher Schwenk mit
    // gleicher Geschwindigkeit faellt hier heraus.
    const float speed = std::sqrt(speedSqr);
    if (speed <= 0.0F ||
        forwardSpeed / speed < kMeleeThrustMinAlignment) {
        return false;
    }

    detector.armed = false;
    detector.cooldownUntilNs = nowNs + kMeleeThrustCooldownNs;
    return true;
}

} // namespace fearvr
