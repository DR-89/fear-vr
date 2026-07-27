#include <cassert>
#include <cstdint>

#include "melee_actions.h"

namespace {

constexpr std::uint64_t kFrameNs = 11'000'000ULL; // ~90 Hz

struct TestMotion {
    std::uint64_t nowNs{1'000'000'000ULL};
    float z[FEARVR_HAND_COUNT]{};
};

struct TestMovement {
    bool available{false};
    bool onGround{false};
    bool airborne{false};
    bool movingForward{false};
    bool sprinting{false};
    bool postureDownWindow{false};
    bool stickCrouch{false};
    bool aimingDownSights{false};
    bool onLadder{false};
    bool headHeightValid{false};
    float headHeightMeters{0.0F};
    bool weaponStrikeEnabled{true};
    bool offHandStrikeEnabled{true};
    bool jumpKickEnabled{true};
    bool slideKickEnabled{true};
};

FearVrInputState HandsAt(const TestMotion& motion) {
    FearVrInputState input{};
    input.flags = FEARVR_IF_VALID | FEARVR_IF_FOCUSED;
    input.activeHands =
        FEARVR_HAND_MASK_LEFT | FEARVR_HAND_MASK_RIGHT;
    input.aimPoseValidHands = input.activeHands;
    input.gripPoseValidHands = input.activeHands;
    for (std::uint32_t hand = 0; hand < FEARVR_HAND_COUNT; ++hand) {
        input.handAimPose[hand].pz = motion.z[hand];
        input.handAimPose[hand].qw = 1.0F;
        input.handGripPose[hand].qw = 1.0F;
    }
    return input;
}

fearvr::MeleeActionRequest Sample(
    fearvr::MeleeActionState& state,
    TestMotion& motion,
    bool offHandHoldingWeapon = false,
    bool weaponDisabled = false,
    float offHandSqueeze = 0.0F,
    TestMovement movement = {}) {
    fearvr::MeleeActionInput frame{};
    frame.input = HandsAt(motion);
    frame.input.squeeze[FEARVR_HAND_LEFT] = offHandSqueeze;
    frame.nowNs = motion.nowNs;
    frame.movementAvailable = movement.available;
    frame.onGround = movement.onGround;
    frame.airborne = movement.airborne;
    frame.movingForward = movement.movingForward;
    frame.sprinting = movement.sprinting;
    frame.postureDownWindow = movement.postureDownWindow;
    frame.stickCrouch = movement.stickCrouch;
    frame.aimingDownSights = movement.aimingDownSights;
    frame.onLadder = movement.onLadder;
    frame.headHeightValid = movement.headHeightValid;
    frame.headHeightMeters = movement.headHeightMeters;
    frame.weaponStrikeEnabled = movement.weaponStrikeEnabled;
    frame.offHandStrikeEnabled = movement.offHandStrikeEnabled;
    frame.jumpKickEnabled = movement.jumpKickEnabled;
    frame.slideKickEnabled = movement.slideKickEnabled;
    frame.weaponDisabled = weaponDisabled;
    frame.offHandHoldingWeapon = offHandHoldingWeapon;
    return fearvr::UpdateMeleeActions(state, frame);
}

fearvr::MeleeActionRequest MoveHand(
    fearvr::MeleeActionState& state,
    TestMotion& motion,
    std::uint32_t hand,
    float speedMps,
    bool offHandHoldingWeapon = false,
    bool weaponDisabled = false,
    float offHandSqueeze = 0.0F,
    TestMovement movement = {}) {
    motion.nowNs += kFrameNs;
    motion.z[hand] -=
        speedMps * static_cast<float>(kFrameNs) * 1.0e-9F;
    return Sample(
        state, motion, offHandHoldingWeapon, weaponDisabled,
        offHandSqueeze, movement);
}

void Prime(
    fearvr::MeleeActionState& state,
    TestMotion& motion,
    bool offHandHoldingWeapon = false,
    bool weaponDisabled = false,
    float offHandSqueeze = 0.0F,
    TestMovement movement = {}) {
    assert(
        Sample(
            state, motion, offHandHoldingWeapon, weaponDisabled,
            offHandSqueeze, movement)
            .action == fearvr::MeleeAction::None);
}

TestMovement SlideReady(float headHeightMeters = 1.70F) {
    TestMovement movement{};
    movement.available = true;
    movement.onGround = true;
    movement.movingForward = true;
    movement.sprinting = true;
    movement.headHeightValid = true;
    movement.headHeightMeters = headHeightMeters;
    return movement;
}

fearvr::MeleeActionRequest PhysicalCrouchThenThrust(
    TestMovement movement,
    std::uint32_t hand = FEARVR_HAND_RIGHT) {
    fearvr::MeleeActionState state{};
    TestMotion motion{};
    movement.headHeightValid = true;
    movement.headHeightMeters = 1.70F;
    Prime(state, motion, false, false, 0.0F, movement);
    motion.nowNs += 100'000'000ULL;
    movement.headHeightMeters = 1.44F;
    assert(
        Sample(state, motion, false, false, 0.0F, movement)
            .action == fearvr::MeleeAction::None);
    return MoveHand(
        state, motion, hand, 3.0F, false, false, 0.0F,
        movement);
}

} // namespace

int main() {
    // Der bekannte Stoss der Waffenhand bleibt ein WeaponStrike.
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        Prime(state, motion);
        const auto request = MoveHand(
            state, motion, FEARVR_HAND_RIGHT, 3.0F);
        assert(request.action == fearvr::MeleeAction::WeaponStrike);
        assert(!request.needsDuckEdge);
        assert(!request.needsForwardHold);
    }

    // Derselbe Stoss der freien Hand wird als OffHandStrike eingeordnet.
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        Prime(state, motion);
        const auto request = MoveHand(
            state, motion, FEARVR_HAND_LEFT, 3.0F);
        assert(request.action == fearvr::MeleeAction::OffHandStrike);
        assert(!request.needsDuckEdge);
        assert(!request.needsForwardHold);
    }

    // Die freie Hand schlaegt weder aus dem Zweihandgriff noch mit
    // gedruecktem Grabknopf heraus.
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        Prime(state, motion, true);
        assert(
            MoveHand(
                state, motion, FEARVR_HAND_LEFT, 3.0F, true)
                .action == fearvr::MeleeAction::None);
    }
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        Prime(
            state, motion, false, false,
            fearvr::kMeleeOffHandGrabThreshold);
        assert(
            MoveHand(
                state, motion, FEARVR_HAND_LEFT, 3.0F, false,
                false, fearvr::kMeleeOffHandGrabThreshold)
                .action == fearvr::MeleeAction::None);
    }

    // Retail hat die Waffe abgeschaltet: Beide Haende bleiben wirkungslos.
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        Prime(state, motion, false, true);
        assert(
            MoveHand(
                state, motion, FEARVR_HAND_RIGHT, 3.0F, false,
                true)
                .action == fearvr::MeleeAction::None);
    }

    // Die Sperre gehoert der gesamten Aktion, nicht einer Hand: Direkt nach
    // dem Waffenstoss bleibt auch ein Stoss der freien Hand aus.
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        Prime(state, motion);
        assert(
            MoveHand(state, motion, FEARVR_HAND_RIGHT, 3.0F)
                .action == fearvr::MeleeAction::WeaponStrike);
        assert(
            MoveHand(state, motion, FEARVR_HAND_LEFT, 3.0F)
                .action == fearvr::MeleeAction::None);
    }

    // Kreuzen beide Haende im selben Bild die Schwelle, entsteht genau der
    // priorisierte Waffenstoss.
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        Prime(state, motion);
        motion.nowNs += kFrameNs;
        const float distance =
            3.0F * static_cast<float>(kFrameNs) * 1.0e-9F;
        motion.z[FEARVR_HAND_LEFT] -= distance;
        motion.z[FEARVR_HAND_RIGHT] -= distance;
        assert(
            Sample(state, motion).action ==
            fearvr::MeleeAction::WeaponStrike);
    }

    // Wiederholte Zeitstempel und grosse Bildluecken sind keine Bewegung.
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        Prime(state, motion);
        motion.z[FEARVR_HAND_LEFT] = -1.0F;
        assert(
            Sample(state, motion).action ==
            fearvr::MeleeAction::None);
        motion.nowNs += 200'000'000ULL;
        motion.z[FEARVR_HAND_LEFT] = -2.0F;
        assert(
            Sample(state, motion).action ==
            fearvr::MeleeAction::None);
    }

    // Mit verlaesslichem Retail-Bodenzustand wartet der Schlag 250 ms. Ein
    // echter, vom Spieler ausgeloester Sprung in diesem Fenster wertet ihn
    // zum Jump Kick auf.
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        const TestMovement ground{true, true, false};
        Prime(state, motion, false, false, 0.0F, ground);
        assert(
            MoveHand(
                state, motion, FEARVR_HAND_RIGHT, 3.0F, false,
                false, 0.0F, ground)
                .action == fearvr::MeleeAction::None);
        assert(
            state.pendingGroundStrike ==
            fearvr::MeleeAction::WeaponStrike);

        motion.nowNs += 100'000'000ULL;
        const auto request = Sample(
            state, motion, false, false, 0.0F,
            TestMovement{true, false, true});
        assert(request.action == fearvr::MeleeAction::JumpKick);
        assert(!request.needsDuckEdge);
        assert(!request.needsForwardHold);
        assert(
            state.pendingGroundStrike == fearvr::MeleeAction::None);
    }

    // Ohne Sprung faellt die Warteschlange nach genau 250 ms auf den
    // urspruenglichen Schlag zurueck; auch die freie Hand behaelt ihren Typ.
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        const TestMovement ground{true, true, false};
        Prime(state, motion, false, false, 0.0F, ground);
        assert(
            MoveHand(
                state, motion, FEARVR_HAND_LEFT, 3.0F, false,
                false, 0.0F, ground)
                .action == fearvr::MeleeAction::None);
        motion.nowNs += fearvr::kMeleeJumpQueueNs - 1;
        assert(
            Sample(state, motion, false, false, 0.0F, ground)
                .action == fearvr::MeleeAction::None);
        ++motion.nowNs;
        assert(
            Sample(state, motion, false, false, 0.0F, ground)
                .action == fearvr::MeleeAction::OffHandStrike);
    }

    // Erfolgt der Stoss bereits in der Luft, gibt es kein zusaetzliches
    // Wartefenster.
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        const TestMovement air{true, false, true};
        Prime(state, motion, false, false, 0.0F, air);
        assert(
            MoveHand(
                state, motion, FEARVR_HAND_RIGHT, 3.0F, false,
                false, 0.0F, air)
                .action == fearvr::MeleeAction::JumpKick);
    }

    // Wird die Waffe waehrend des Fensters deaktiviert, darf kein
    // verspaeteter Angriff uebrig bleiben.
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        const TestMovement ground{true, true, false};
        Prime(state, motion, false, false, 0.0F, ground);
        assert(
            MoveHand(
                state, motion, FEARVR_HAND_RIGHT, 3.0F, false,
                false, 0.0F, ground)
                .action == fearvr::MeleeAction::None);
        motion.nowNs += 10'000'000ULL;
        assert(
            Sample(state, motion, false, true, 0.0F, ground)
                .action == fearvr::MeleeAction::None);
        motion.nowNs += fearvr::kMeleeJumpQueueNs;
        assert(
            Sample(state, motion, false, false, 0.0F, ground)
                .action == fearvr::MeleeAction::None);
    }

    // Eine schnelle physische Absenkung um mindestens 25 cm, waehrend Retail
    // Sprint vorwaerts am Boden meldet, macht den naechsten Stoss beider
    // Haende zum Slide Kick.
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        TestMovement movement = SlideReady();
        Prime(state, motion, false, false, 0.0F, movement);
        motion.nowNs += 100'000'000ULL;
        movement.headHeightMeters = 1.44F;
        assert(
            Sample(state, motion, false, false, 0.0F, movement)
                .action == fearvr::MeleeAction::None);
        const auto request = MoveHand(
            state, motion, FEARVR_HAND_RIGHT, 3.0F, false,
            false, 0.0F, movement);
        assert(request.action == fearvr::MeleeAction::SlideKick);
        assert(request.needsDuckEdge);
        assert(request.needsForwardHold);
        assert(
            state.sharedCooldownUntilNs ==
            motion.nowNs + fearvr::kMeleeSlideCooldownNs);
    }
    {
        const auto request = PhysicalCrouchThenThrust(
            SlideReady(), FEARVR_HAND_LEFT);
        assert(request.action == fearvr::MeleeAction::SlideKick);
        assert(request.needsDuckEdge);
        assert(request.needsForwardHold);
    }

    // Stick-DUCK benutzt Retails bereits geoeffnetes PostureDown-Fenster und
    // braucht deshalb keinen zusaetzlichen DUCK-Puls.
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        TestMovement movement = SlideReady();
        Prime(state, motion, false, false, 0.0F, movement);
        motion.nowNs += kFrameNs;
        movement.stickCrouch = true;
        assert(
            Sample(state, motion, false, false, 0.0F, movement)
                .action == fearvr::MeleeAction::None);
        movement.postureDownWindow = true;
        const auto request = MoveHand(
            state, motion, FEARVR_HAND_RIGHT, 3.0F, false,
            false, 0.0F, movement);
        assert(request.action == fearvr::MeleeAction::SlideKick);
        assert(!request.needsDuckEdge);
        assert(request.needsForwardHold);
    }
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        TestMovement movement = SlideReady();
        Prime(state, motion, false, false, 0.0F, movement);
        motion.nowNs += kFrameNs;
        movement.stickCrouch = true;
        assert(
            Sample(state, motion, false, false, 0.0F, movement)
                .action == fearvr::MeleeAction::None);
        // Die Erleichterung ist begrenzt: Nach mehr als 400 ms darf ein
        // spaeter, unabhaengiger Stoss nicht mehr zur Slide Kick werden.
        for (int i = 0; i < 9; ++i) {
            motion.nowNs += 50'000'000ULL;
            assert(
                Sample(state, motion, false, false, 0.0F, movement)
                    .action == fearvr::MeleeAction::None);
        }
        assert(
            MoveHand(
                state, motion, FEARVR_HAND_RIGHT, 3.0F, false,
                false, 0.0F, movement)
                .action != fearvr::MeleeAction::SlideKick);
    }
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        TestMovement movement = SlideReady();
        Prime(state, motion, false, false, 0.0F, movement);
        movement.stickCrouch = true;
        movement.onGround = false;
        movement.postureDownWindow = false;
        // Retail wurde live genau so beobachtet: In dem Stoessframe ist der
        // Spieler weder onGround noch airborne. Der Stoss darf in diesem
        // Ein-Frame-Zwischenzustand nicht als normaler Schlag entweichen.
        assert(
            MoveHand(
                state, motion, FEARVR_HAND_RIGHT, 3.0F, false,
                false, 0.0F, movement)
                .action == fearvr::MeleeAction::None);
        assert(
            state.pendingGroundStrike ==
            fearvr::MeleeAction::WeaponStrike);
        motion.nowNs += kFrameNs;
        movement.onGround = true;
        movement.postureDownWindow = true;
        const auto request =
            Sample(state, motion, false, false, 0.0F, movement);
        assert(request.action == fearvr::MeleeAction::SlideKick);
        assert(!request.needsDuckEdge);
        assert(request.needsForwardHold);
    }
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        TestMovement movement = SlideReady();
        Prime(state, motion, false, false, 0.0F, movement);
        movement.stickCrouch = true;
        // Menschlich naheliegend: DUCK und Stoss kommen zusammen. Variante A
        // darf schon die gueltige OpenXR-Flanke verwenden und muss nicht auf
        // Retails erst im Folgeframe sichtbares Fenster warten.
        const auto request = MoveHand(
            state, motion, FEARVR_HAND_RIGHT, 3.0F, false,
            false, 0.0F, movement);
        assert(request.action == fearvr::MeleeAction::SlideKick);
        assert(!request.needsDuckEdge);
        assert(request.needsForwardHold);
    }
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        TestMovement movement = SlideReady();
        Prime(state, motion, false, false, 0.0F, movement);
        motion.nowNs += kFrameNs;
        movement.stickCrouch = true;
        assert(
            Sample(state, motion, false, false, 0.0F, movement)
                .action == fearvr::MeleeAction::None);
        // Designvariante A: Die gueltige Stick-DUCK-Flanke bleibt 400 ms
        // erhalten; der Stoss muss nicht in Retails nur 100 ms langem
        // PostureDown-Fenster landen.
        const auto request = MoveHand(
            state, motion, FEARVR_HAND_RIGHT, 3.0F, false,
            false, 0.0F, movement);
        assert(request.action == fearvr::MeleeAction::SlideKick);
        assert(!request.needsDuckEdge);
        assert(request.needsForwardHold);
    }

    // Jeder harte Schutz ist einzeln wirksam. Ohne Vorwaertsbewegung, Sprint
    // oder Bodenkontakt sowie beim Zielen oder an einer Leiter gibt es keinen
    // Slide Kick.
    {
        TestMovement movement = SlideReady();
        movement.movingForward = false;
        assert(
            PhysicalCrouchThenThrust(movement).action !=
            fearvr::MeleeAction::SlideKick);
    }
    {
        TestMovement movement = SlideReady();
        movement.sprinting = false;
        assert(
            PhysicalCrouchThenThrust(movement).action !=
            fearvr::MeleeAction::SlideKick);
    }
    {
        TestMovement movement = SlideReady();
        movement.onGround = false;
        movement.airborne = true;
        assert(
            PhysicalCrouchThenThrust(movement).action ==
            fearvr::MeleeAction::JumpKick);
    }
    {
        TestMovement movement = SlideReady();
        movement.aimingDownSights = true;
        assert(
            PhysicalCrouchThenThrust(movement).action !=
            fearvr::MeleeAction::SlideKick);
    }
    {
        TestMovement movement = SlideReady();
        movement.onLadder = true;
        assert(
            PhysicalCrouchThenThrust(movement).action !=
            fearvr::MeleeAction::SlideKick);
    }
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        TestMovement movement = SlideReady();
        Prime(state, motion, false, true, 0.0F, movement);
        motion.nowNs += 100'000'000ULL;
        movement.headHeightMeters = 1.44F;
        assert(
            Sample(state, motion, false, true, 0.0F, movement)
                .action == fearvr::MeleeAction::None);
        assert(
            MoveHand(
                state, motion, FEARVR_HAND_RIGHT, 3.0F, false,
                true, 0.0F, movement)
                .action == fearvr::MeleeAction::None);
    }

    // Zu langsame oder ungueltige Kopfbewegung ist keine physische Hocke.
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        TestMovement movement = SlideReady();
        Prime(state, motion, false, false, 0.0F, movement);
        motion.nowNs +=
            fearvr::kMeleePhysicalCrouchWindowNs + 1;
        movement.headHeightMeters = 1.44F;
        assert(
            Sample(state, motion, false, false, 0.0F, movement)
                .action == fearvr::MeleeAction::None);
        assert(
            MoveHand(
                state, motion, FEARVR_HAND_RIGHT, 3.0F, false,
                false, 0.0F, movement)
                .action != fearvr::MeleeAction::SlideKick);
    }
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        TestMovement movement = SlideReady();
        movement.headHeightValid = false;
        Prime(state, motion, false, false, 0.0F, movement);
        motion.nowNs += 100'000'000ULL;
        movement.headHeightMeters = 1.30F;
        assert(
            Sample(state, motion, false, false, 0.0F, movement)
                .action == fearvr::MeleeAction::None);
        assert(
            MoveHand(
                state, motion, FEARVR_HAND_RIGHT, 3.0F, false,
                false, 0.0F, movement)
                .action != fearvr::MeleeAction::SlideKick);
    }

    // Die vier Einzeloptionen sperren nur ihre jeweilige Aktion. Ein
    // deaktivierter normaler Waffenstoss verhindert also keinen ausdruecklich
    // aktiv gelassenen Kick.
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        TestMovement movement{};
        movement.weaponStrikeEnabled = false;
        Prime(state, motion, false, false, 0.0F, movement);
        assert(
            MoveHand(
                state, motion, FEARVR_HAND_RIGHT, 3.0F, false,
                false, 0.0F, movement)
                .action == fearvr::MeleeAction::None);
    }
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        TestMovement movement{};
        movement.offHandStrikeEnabled = false;
        Prime(state, motion, false, false, 0.0F, movement);
        assert(
            MoveHand(
                state, motion, FEARVR_HAND_LEFT, 3.0F, false,
                false, 0.0F, movement)
                .action == fearvr::MeleeAction::None);
    }
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        TestMovement movement{
            true, false, true};
        movement.weaponStrikeEnabled = false;
        Prime(state, motion, false, false, 0.0F, movement);
        assert(
            MoveHand(
                state, motion, FEARVR_HAND_RIGHT, 3.0F, false,
                false, 0.0F, movement)
                .action == fearvr::MeleeAction::JumpKick);
    }
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        TestMovement movement{
            true, false, true};
        movement.jumpKickEnabled = false;
        Prime(state, motion, false, false, 0.0F, movement);
        assert(
            MoveHand(
                state, motion, FEARVR_HAND_RIGHT, 3.0F, false,
                false, 0.0F, movement)
                .action == fearvr::MeleeAction::None);
    }
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        TestMovement movement{
            true, true, false};
        movement.jumpKickEnabled = false;
        Prime(state, motion, false, false, 0.0F, movement);
        // Ist Jump Kick abgeschaltet, braucht der Bodenschlag kein
        // Vorabsprungfenster und bleibt unmittelbar.
        assert(
            MoveHand(
                state, motion, FEARVR_HAND_RIGHT, 3.0F, false,
                false, 0.0F, movement)
                .action == fearvr::MeleeAction::WeaponStrike);
    }
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        TestMovement movement{
            true, true, false};
        movement.weaponStrikeEnabled = false;
        Prime(state, motion, false, false, 0.0F, movement);
        assert(
            MoveHand(
                state, motion, FEARVR_HAND_RIGHT, 3.0F, false,
                false, 0.0F, movement)
                .action == fearvr::MeleeAction::None);
        motion.nowNs += 100'000'000ULL;
        movement.onGround = false;
        movement.airborne = true;
        // Jump Kick bleibt trotz einzeln abgeschaltetem Weapon Strike
        // innerhalb des Vorabsprungfensters erreichbar.
        assert(
            Sample(state, motion, false, false, 0.0F, movement)
                .action == fearvr::MeleeAction::JumpKick);
    }
    {
        TestMovement movement = SlideReady();
        movement.slideKickEnabled = false;
        assert(
            PhysicalCrouchThenThrust(movement).action !=
            fearvr::MeleeAction::SlideKick);
    }
    {
        fearvr::MeleeActionState state{};
        TestMotion motion{};
        TestMovement movement = SlideReady();
        movement.slideKickEnabled = false;
        Prime(state, motion, false, false, 0.0F, movement);
        motion.nowNs += kFrameNs;
        movement.stickCrouch = true;
        assert(
            Sample(state, motion, false, false, 0.0F, movement)
                .action == fearvr::MeleeAction::None);
        movement.postureDownWindow = true;
        assert(
            MoveHand(
                state, motion, FEARVR_HAND_RIGHT, 3.0F, false,
                false, 0.0F, movement)
                .action == fearvr::MeleeAction::None);
    }

    return 0;
}
