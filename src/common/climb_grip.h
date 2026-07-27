#pragma once

#include <cmath>
#include <cstdint>

#include "protocol.h"

namespace fearvr {

// Klettern an Leitern mit den Haenden: Man greift eine Sprosse und zieht sich
// daran hoch, statt vorwaerts zu druecken.
//
// Diese Datei enthaelt ausschliesslich die Bewegungsauswertung — greift eine
// Hand, und wie weit hat sie seit dem letzten Bild gezogen. Sie kennt weder
// die Spielwelt noch die Leiter selbst. Das ist Absicht: So laesst sich die
// Kletterbewegung ohne Headset pruefen, und der Eingriff ins Spiel bleibt auf
// eine Stelle beschraenkt.
//
// Bewegt wird ausschliesslich, *waehrend* die Hand zieht. Zwei Fassungen sind
// vorher gescheitert, beide am selben Punkt: Die Handpose ist raumbezogen.
// Faehrt der Spielkoerper hoch, bleibt die Hand im Zimmer stehen, wo sie ist.
//
//  1. Auslenkung gegenueber dem Griffpunkt: Einmal greifen, Hand unten
//     lassen — der Spieler fuhr endlos weiter.
//  2. Zugstrecke als Guthaben, das die Bewegung abbaut: Ohne die echte
//     Klettergeschwindigkeit des Spiels zu kennen, lief ein einzelner Zug
//     ueber mehrere Sprossen nach.
//
// Jetzt zaehlt allein die Geschwindigkeit der Hand im aktuellen Bild. Ein
// kurzer Nachlauf glaettet das Tracking, mehr nicht: Klettern dauert so lange
// wie das Ziehen und keinen Zug laenger.

// Greifen und Loslassen der Grabtaste, mit Abstand dazwischen, damit ein am
// Druckpunkt ruhender Finger nicht flattert. Dieselben Werte wie beim
// Zweihandgriff, damit sich beide Griffe gleich anfuehlen.
constexpr float kClimbEngageSqueeze = 0.65F;
constexpr float kClimbReleaseSqueeze = 0.45F;
// Ab dieser geglaetteten Handgeschwindigkeit gilt die Hand als ziehend.
constexpr float kClimbMinPullSpeedMps = 0.15F;
// Zeitkonstanten der Glaettung, bewusst unterschiedlich. Ohne Glaettung waere
// ein Zittern von zwei Millimetern pro Bild bereits ein Drittel Meter pro
// Sekunde und vom Ziehen kaum zu unterscheiden; gemittelt hebt es sich auf,
// weil es das Vorzeichen wechselt. Das Abklingen muss dagegen schnell gehen:
// Jede Millisekunde, die die geglaettete Geschwindigkeit nach dem Zug noch
// ueber der Schwelle steht, klettert der Spieler weiter — daher kommt das
// beobachtete Nachlaufen ueber mehrere Sprossen.
constexpr float kClimbRiseSeconds = 0.08F;
constexpr float kClimbFallSeconds = 0.02F;
// Nachlauf nach der letzten Zugbewegung. Nur zur Glaettung: Ohne ihn wuerde
// die Bewegung bei jedem Zittern zwischen an und aus springen.
constexpr std::uint64_t kClimbCoastNs = 60000000ULL;
// Groesster Bildabstand, aus dem noch ein Zug gebildet werden darf. Nach einem
// Ladebildschirm waere die Differenz ein Sprung und keine Handbewegung.
constexpr std::uint64_t kClimbMaxSampleGapNs = 200000000ULL;

struct ClimbHandState {
    float lastHeight{0.0F};
    bool gripping{false};
    bool haveHeight{false};
};

struct ClimbGripState {
    ClimbHandState hand[FEARVR_HAND_COUNT];
    // Geglaettete Zuggeschwindigkeit in m/s, positiv nach unten gezogen.
    float smoothedPullSpeed{0.0F};
    // Bis wann die zuletzt gemessene Zugrichtung noch nachlaeuft.
    std::uint64_t coastUntilNs{0};
    float coastAxis{0.0F};
    std::uint64_t lastUpdateNs{0};
    bool haveUpdate{false};
};

struct ClimbPull {
    // Richtung der Kletterbewegung: +1 aufwaerts, -1 abwaerts, 0 steht.
    float axis{0.0F};
    // Mindestens eine Hand greift.
    bool gripping{false};
};

inline void ResetClimbGrip(ClimbGripState& state) noexcept {
    state = ClimbGripState{};
}

// Ist die Hand nutzbar und ihre Greifposition bekannt?
inline bool ClimbHandUsable(
    const FearVrInputState& input, std::uint32_t hand) noexcept {
    if (hand >= FEARVR_HAND_COUNT) {
        return false;
    }
    const std::uint32_t mask =
        hand == FEARVR_HAND_LEFT
            ? static_cast<std::uint32_t>(FEARVR_HAND_MASK_LEFT)
            : static_cast<std::uint32_t>(FEARVR_HAND_MASK_RIGHT);
    if ((input.flags & FEARVR_IF_VALID) == 0 ||
        (input.flags & FEARVR_IF_FOCUSED) == 0 ||
        (input.activeHands & mask) == 0 ||
        (input.gripPoseValidHands & mask) == 0) {
        return false;
    }
    return std::isfinite(input.handGripPose[hand].py);
}

// Eine Abtastung einspeisen und die Kletterrichtung zurueckgeben.
//
// `climbable` meldet, ob ueberhaupt geklettert werden darf — im Spiel also,
// ob der Spieler an einer Leiter haengt. Ist es false, wird nichts gegriffen
// und jeder gemerkte Zustand faellt weg; die Grabtasten behalten dann ihre
// gewohnte Bedeutung.
//
// `nowNs` ist die Uhr des Aufrufers.
inline ClimbPull UpdateClimbGrip(
    ClimbGripState& state,
    const FearVrInputState& input,
    std::uint64_t nowNs,
    bool climbable) noexcept {
    ClimbPull pull;
    if (!climbable) {
        ResetClimbGrip(state);
        return pull;
    }

    float seconds = 0.0F;
    if (state.haveUpdate && nowNs > state.lastUpdateNs &&
        nowNs - state.lastUpdateNs <= kClimbMaxSampleGapNs) {
        seconds = static_cast<float>(nowNs - state.lastUpdateNs) * 1.0e-9F;
    }
    state.lastUpdateNs = nowNs;
    state.haveUpdate = true;

    // Schnellste Zugbewegung dieses Bildes bestimmen.
    float pullSpeed = 0.0F;
    for (std::uint32_t hand = 0; hand < FEARVR_HAND_COUNT; ++hand) {
        ClimbHandState& handState = state.hand[hand];
        if (!ClimbHandUsable(input, hand)) {
            handState = ClimbHandState{};
            continue;
        }
        const float squeeze = input.squeeze[hand];
        const float height = input.handGripPose[hand].py;
        const bool wasGripping = handState.gripping;
        handState.gripping = wasGripping
            ? squeeze > kClimbReleaseSqueeze
            : squeeze >= kClimbEngageSqueeze;
        if (!handState.gripping) {
            handState.haveHeight = false;
            continue;
        }

        pull.gripping = true;
        if (wasGripping && handState.haveHeight && seconds > 0.0F) {
            // Nach unten gezogen ist positiv: Der Koerper geht nach oben.
            const float speed =
                (handState.lastHeight - height) / seconds;
            if (std::isfinite(speed) &&
                std::fabs(speed) > std::fabs(pullSpeed)) {
                pullSpeed = speed;
            }
        }
        handState.lastHeight = height;
        handState.haveHeight = true;
    }

    if (!pull.gripping) {
        state.coastUntilNs = 0;
        state.coastAxis = 0.0F;
        state.smoothedPullSpeed = 0.0F;
        return pull;
    }

    if (seconds > 0.0F) {
        const bool rising =
            std::fabs(pullSpeed) >= std::fabs(state.smoothedPullSpeed);
        float weight = seconds /
            (rising ? kClimbRiseSeconds : kClimbFallSeconds);
        if (weight > 1.0F) {
            weight = 1.0F;
        }
        state.smoothedPullSpeed +=
            (pullSpeed - state.smoothedPullSpeed) * weight;
    }
    pullSpeed = state.smoothedPullSpeed;

    if (pullSpeed >= kClimbMinPullSpeedMps) {
        state.coastAxis = 1.0F;
        state.coastUntilNs = nowNs + kClimbCoastNs;
    } else if (pullSpeed <= -kClimbMinPullSpeedMps) {
        state.coastAxis = -1.0F;
        state.coastUntilNs = nowNs + kClimbCoastNs;
    }

    if (nowNs < state.coastUntilNs) {
        pull.axis = state.coastAxis;
    } else {
        state.coastAxis = 0.0F;
    }
    return pull;
}

} // namespace fearvr
