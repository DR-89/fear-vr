#pragma once

#include <cmath>
#include <cstdint>

#include "melee_thrust.h"
#include "protocol.h"

namespace fearvr {

// Reine, vom GameClient unabhaengige Einordnung der Nahkampfgesten. Die
// Eingabe ist bereits auf die eingestellte Haendigkeit gespiegelt:
// FEARVR_HAND_RIGHT ist die Waffenhand, FEARVR_HAND_LEFT die freie Hand.
//
// Die Bewegungsfelder stammen aus Retails CMoveMgr. Ist der Zustand nicht
// verfuegbar, bleiben die beiden M1-Schlaege ohne Verzoegerung erhalten.
struct MeleeActionInput {
    FearVrInputState input{};
    std::uint64_t nowNs{0};
    float headHeightMeters{0.0F};
    bool headHeightValid{false};
    bool movementAvailable{false};
    bool airborne{false};
    bool onGround{false};
    bool movingForward{false};
    bool sprinting{false};
    bool postureDownWindow{false};
    bool stickCrouch{false};
    bool aimingDownSights{false};
    bool onLadder{false};
    bool weaponDisabled{false};
    bool offHandHoldingWeapon{false};
    bool weaponStrikeEnabled{true};
    bool offHandStrikeEnabled{true};
    bool jumpKickEnabled{true};
    bool slideKickEnabled{true};
};

enum class MeleeAction {
    None,
    WeaponStrike,
    OffHandStrike,
    JumpKick,
    SlideKick,
};

struct MeleeActionRequest {
    MeleeAction action{MeleeAction::None};
    bool needsDuckEdge{false};
    bool needsForwardHold{false};
};

struct MeleeActionState {
    MeleeThrustDetector weaponHand{};
    MeleeThrustDetector offHand{};
    std::uint64_t sharedCooldownUntilNs{0};
    MeleeAction pendingGroundStrike{MeleeAction::None};
    std::uint64_t pendingGroundStrikeUntilNs{0};
    float headReferenceHeightMeters{0.0F};
    std::uint64_t headReferenceTimeNs{0};
    std::uint64_t physicalCrouchUntilNs{0};
    std::uint64_t stickCrouchUntilNs{0};
    bool haveHeadReference{false};
    bool stickCrouchWasDown{false};
};

// Dieselbe Druckschwelle wie Sprint, Benutzen, Zweihandgriff und
// Leitergreifen. Mit gehaltenem Grabknopf gehoert die freie Hand einer anderen
// Handlung und darf keinen Schlag ausloesen.
constexpr float kMeleeOffHandGrabThreshold = 0.65F;
// Ein Stoss am Boden wartet kurz auf einen vom Spieler selbst ausgeloesten
// Sprung. Wird CMoveMgr in diesem Fenster airborne, wird derselbe Stoss zum
// Jump Kick; andernfalls folgt der normale Schlag.
constexpr std::uint64_t kMeleeJumpQueueNs = 250000000ULL;
constexpr float kMeleePhysicalCrouchDropMeters = 0.25F;
constexpr std::uint64_t kMeleePhysicalCrouchWindowNs = 400000000ULL;
constexpr std::uint64_t kMeleeCrouchIntentNs = 400000000ULL;
constexpr std::uint64_t kMeleeSlideCooldownNs = 1000000000ULL;

inline bool UpdatePhysicalCrouch(
    MeleeActionState& state,
    const MeleeActionInput& frame) noexcept {
    if (!frame.headHeightValid ||
        !std::isfinite(frame.headHeightMeters)) {
        state.haveHeadReference = false;
        return false;
    }
    if (!state.haveHeadReference ||
        frame.nowNs <= state.headReferenceTimeNs ||
        frame.nowNs - state.headReferenceTimeNs >
            kMeleePhysicalCrouchWindowNs) {
        state.headReferenceHeightMeters = frame.headHeightMeters;
        state.headReferenceTimeNs = frame.nowNs;
        state.haveHeadReference = true;
        return false;
    }
    if (frame.headHeightMeters >
        state.headReferenceHeightMeters) {
        state.headReferenceHeightMeters = frame.headHeightMeters;
        state.headReferenceTimeNs = frame.nowNs;
        return false;
    }
    if (state.headReferenceHeightMeters - frame.headHeightMeters <
        kMeleePhysicalCrouchDropMeters) {
        return false;
    }

    // Von der neuen, tieferen Position aus neu messen. So erzeugt ein
    // gehaltenes Hocken keine Folge von Crouch-Gesten.
    state.headReferenceHeightMeters = frame.headHeightMeters;
    state.headReferenceTimeNs = frame.nowNs;
    return true;
}

inline bool SlideMovementAllowed(
    const MeleeActionInput& frame) noexcept {
    return frame.movementAvailable && frame.onGround &&
           !frame.airborne && frame.movingForward &&
           frame.sprinting && !frame.aimingDownSights &&
           !frame.onLadder && !frame.weaponDisabled;
}

inline bool StrikeEnabled(
    MeleeAction action, const MeleeActionInput& frame) noexcept {
    if (action == MeleeAction::WeaponStrike) {
        return frame.weaponStrikeEnabled;
    }
    if (action == MeleeAction::OffHandStrike) {
        return frame.offHandStrikeEnabled;
    }
    return false;
}

inline void ResetMeleeActions(MeleeActionState& state) noexcept {
    state = MeleeActionState{};
}

inline MeleeActionRequest UpdateMeleeActions(
    MeleeActionState& state,
    const MeleeActionInput& frame) noexcept {
    // Beide Detektoren werden auch waehrend einer gemeinsamen Sperre
    // aktualisiert. Ein zweiter Stoss in diesem Fenster wird dadurch
    // vollstaendig verbraucht und kann nicht nach Ablauf der Sperre verspaetet
    // ausloesen.
    const bool weaponThrust = UpdateMeleeThrust(
        state.weaponHand, frame.input, frame.nowNs,
        FEARVR_HAND_RIGHT);
    const bool offHandThrust = UpdateMeleeThrust(
        state.offHand, frame.input, frame.nowNs,
        FEARVR_HAND_LEFT);

    const bool slideMovementAllowed = SlideMovementAllowed(frame);
    if (UpdatePhysicalCrouch(state, frame) &&
        slideMovementAllowed) {
        state.physicalCrouchUntilNs =
            frame.nowNs + kMeleeCrouchIntentNs;
    }
    if (frame.stickCrouch && !state.stickCrouchWasDown &&
        slideMovementAllowed) {
        state.stickCrouchUntilNs =
            frame.nowNs + kMeleeCrouchIntentNs;
    }
    state.stickCrouchWasDown = frame.stickCrouch;

    if (frame.weaponDisabled) {
        state.pendingGroundStrike = MeleeAction::None;
        state.pendingGroundStrikeUntilNs = 0;
        state.physicalCrouchUntilNs = 0;
        state.stickCrouchUntilNs = 0;
        return {};
    }

    // Variante A: Eine beim Vorwaertssprint gesehene Stick-DUCK-Flanke bleibt
    // 400 ms als Absicht erhalten. Retails echtes, nur 100 ms langes Fenster
    // zaehlt ebenfalls, selbst wenn CMoveMgr `onGround` beim Uebergang fuer
    // einen Update-Schritt auf 0 setzt. So duerfen Hocke und Handstoss in
    // natuerlicher Reihenfolge kommen, ohne gleichzeitig sein zu muessen.
    const bool stickCrouchReady =
        frame.postureDownWindow ||
        (state.stickCrouchUntilNs != 0 &&
         frame.nowNs <= state.stickCrouchUntilNs);
    const bool physicalCrouchReady =
        state.physicalCrouchUntilNs != 0 &&
        frame.nowNs <= state.physicalCrouchUntilNs;

    // Eine bereits eingeordnete Bodengeste wird vor neuen Stoessen
    // abgearbeitet. Dabei wird niemals JUMP angefordert: Nur Retails echter
    // Luftzustand darf den wartenden Schlag zum Tritt aufwerten.
    if (state.pendingGroundStrike != MeleeAction::None) {
        if (state.pendingGroundStrike == MeleeAction::OffHandStrike &&
            (frame.offHandHoldingWeapon ||
             frame.input.squeeze[FEARVR_HAND_LEFT] >=
                 kMeleeOffHandGrabThreshold)) {
            state.pendingGroundStrike = MeleeAction::None;
            state.pendingGroundStrikeUntilNs = 0;
            return {};
        }
        // Stick-DUCK und Handstoss koennen im selben Retail-Update eintreffen.
        // Das echte PostureDown-Fenster ist fuer uns dann erst im Folgeframe
        // sichtbar. Deshalb darf auch der wartende Bodenschlag noch zum Slide
        // Kick aufgewertet werden.
        if (slideMovementAllowed &&
            (stickCrouchReady || physicalCrouchReady)) {
            if (frame.slideKickEnabled) {
                state.pendingGroundStrike = MeleeAction::None;
                state.pendingGroundStrikeUntilNs = 0;
                state.physicalCrouchUntilNs = 0;
                state.stickCrouchUntilNs = 0;
                state.sharedCooldownUntilNs =
                    frame.nowNs + kMeleeSlideCooldownNs;
                return {
                    MeleeAction::SlideKick,
                    !stickCrouchReady,
                    true};
            }
            if (stickCrouchReady) {
                // Ein normaler Sekundaerangriff waere im echten Fenster
                // ebenfalls ein Slide Kick und muss bei deaktivierter Aktion
                // verschwinden.
                state.pendingGroundStrike = MeleeAction::None;
                state.pendingGroundStrikeUntilNs = 0;
                state.physicalCrouchUntilNs = 0;
                state.stickCrouchUntilNs = 0;
                return {};
            }
        }
        if (frame.movementAvailable && frame.airborne) {
            state.pendingGroundStrike = MeleeAction::None;
            state.pendingGroundStrikeUntilNs = 0;
            return frame.jumpKickEnabled
                ? MeleeActionRequest{
                      MeleeAction::JumpKick, false, false}
                : MeleeActionRequest{};
        }
        if (frame.nowNs >= state.pendingGroundStrikeUntilNs) {
            const MeleeAction action = state.pendingGroundStrike;
            state.pendingGroundStrike = MeleeAction::None;
            state.pendingGroundStrikeUntilNs = 0;
            return StrikeEnabled(action, frame)
                ? MeleeActionRequest{action, false, false}
                : MeleeActionRequest{};
        }
        return {};
    }

    if (frame.nowNs < state.sharedCooldownUntilNs) {
        return {};
    }

    MeleeActionRequest request;
    // Falls beide Haende im selben Bild die Schwelle kreuzen, hat der
    // ausdrueckliche Waffenstoss Vorrang. Es wird trotzdem nur ein Retail-
    // Sekundaerangriff angefordert.
    if (weaponThrust) {
        request.action = MeleeAction::WeaponStrike;
    } else if (
        offHandThrust && !frame.offHandHoldingWeapon &&
        frame.input.squeeze[FEARVR_HAND_LEFT] <
            kMeleeOffHandGrabThreshold) {
        request.action = MeleeAction::OffHandStrike;
    }

    if (request.action == MeleeAction::None) {
        return request;
    }

    state.sharedCooldownUntilNs =
        frame.nowNs + kMeleeThrustCooldownNs;
    if (frame.movementAvailable && frame.airborne) {
        return frame.jumpKickEnabled
            ? MeleeActionRequest{
                  MeleeAction::JumpKick, false, false}
            : MeleeActionRequest{};
    }
    if (slideMovementAllowed &&
        (stickCrouchReady || physicalCrouchReady)) {
        if (!frame.slideKickEnabled) {
            state.physicalCrouchUntilNs = 0;
            state.stickCrouchUntilNs = 0;
            // Stick-DUCK hat Retail bereits in den Slide-Zustand versetzt.
            // Auch ein "normaler" ALT_FIRING-Puls waere hier ein Slide Kick
            // und muss deshalb vollstaendig unterdrueckt werden. Eine rein
            // physische Hocke veraendert Retail dagegen noch nicht; dort darf
            // der normale Schlag weiterlaufen.
            if (stickCrouchReady) {
                return {};
            }
        } else {
            request.action = MeleeAction::SlideKick;
            // Stick-DUCK hat Retails echte Flanke bereits erzeugt. Nur eine
            // physische Hocke braucht einen synthetischen DUCK-Puls.
            request.needsDuckEdge = !stickCrouchReady;
            request.needsForwardHold = true;
            state.physicalCrouchUntilNs = 0;
            state.stickCrouchUntilNs = 0;
            state.sharedCooldownUntilNs =
                frame.nowNs + kMeleeSlideCooldownNs;
            return request;
        }
    }
    if (frame.movementAvailable && !frame.airborne) {
        if (frame.jumpKickEnabled) {
            // CMoveMgr kann waehrend einer verarbeiteten DUCK-Flanke fuer
            // genau einen Update-Schritt zugleich onGround=0 und airborne=0
            // melden. Dieser Zwischenzustand ist weiterhin bodennah und muss
            // in dieselbe Warteschlange, damit das im Folgeframe sichtbare
            // PostureDown-Fenster den Stoss noch zum Slide Kick aufwertet.
            // Auch bei abgeschaltetem normalem Schlag vormerken: So kann der
            // Spieler Jump Kick einzeln aktiv lassen. Laeuft das Fenster ab,
            // prueft der Pending-Pfad den Strike-Schalter erneut und bleibt
            // gegebenenfalls still.
            state.pendingGroundStrike = request.action;
            state.pendingGroundStrikeUntilNs =
                frame.nowNs + kMeleeJumpQueueNs;
            return {};
        }
        // Ohne Jump Kick gibt es nichts abzuwarten; normale Schlaege bleiben
        // unmittelbar und werden nicht grundlos um 250 ms verzoegert.
        return StrikeEnabled(request.action, frame)
            ? request
            : MeleeActionRequest{};
    }

    return StrikeEnabled(request.action, frame)
        ? request
        : MeleeActionRequest{};
}

} // namespace fearvr
