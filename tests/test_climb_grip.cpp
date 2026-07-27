#include <cassert>
#include <cmath>
#include <cstdint>

#include "climb_grip.h"

namespace {

constexpr std::uint64_t kFrameNs = 11'000'000ULL; // ~90 Hz

// Beide Haende nutzbar; Hoehe und Grabtaste je Hand frei setzbar.
FearVrInputState ClimbInput(
    float leftHeight, float leftSqueeze,
    float rightHeight, float rightSqueeze) {
    FearVrInputState input{};
    input.flags = FEARVR_IF_VALID | FEARVR_IF_FOCUSED;
    input.activeHands =
        FEARVR_HAND_MASK_LEFT | FEARVR_HAND_MASK_RIGHT;
    input.aimPoseValidHands = input.activeHands;
    input.gripPoseValidHands = input.activeHands;
    for (std::uint32_t hand = 0; hand < FEARVR_HAND_COUNT; ++hand) {
        input.handAimPose[hand].qw = 1.0F;
        input.handGripPose[hand].qw = 1.0F;
    }
    input.handGripPose[FEARVR_HAND_LEFT].py = leftHeight;
    input.handGripPose[FEARVR_HAND_RIGHT].py = rightHeight;
    input.squeeze[FEARVR_HAND_LEFT] = leftSqueeze;
    input.squeeze[FEARVR_HAND_RIGHT] = rightSqueeze;
    return input;
}

// Zieht die rechte Hand ueber `frames` Bilder um `meters` nach unten
// (negativ: nach oben) und meldet, in wie vielen Bildern geklettert wurde.
int PullRightHand(
    fearvr::ClimbGripState& state, std::uint64_t& timeNs,
    float& height, int frames, float meters, float direction = 1.0F) {
    const float step = meters / static_cast<float>(frames);
    int moving = 0;
    for (int frame = 0; frame < frames; ++frame) {
        timeNs += kFrameNs;
        height -= step;
        const fearvr::ClimbPull pull = fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 0.0F, height, 0.9F), timeNs, true);
        if (pull.axis * direction > 0.0F) {
            ++moving;
        }
    }
    return moving;
}

// Haelt die Hand still und meldet, in wie vielen Bildern noch geklettert wird.
int HoldRightHand(
    fearvr::ClimbGripState& state, std::uint64_t& timeNs,
    float height, int frames) {
    int moving = 0;
    for (int frame = 0; frame < frames; ++frame) {
        timeNs += kFrameNs;
        const fearvr::ClimbPull pull = fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 0.0F, height, 0.9F), timeNs, true);
        if (pull.axis != 0.0F) {
            ++moving;
        }
    }
    return moving;
}

} // namespace

int main() {
    // Ohne kletterbare Umgebung passiert nichts, auch mit gedrueckter Taste.
    {
        fearvr::ClimbGripState state{};
        const fearvr::ClimbPull pull = fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 1.0F, 1.5F, 1.0F), kFrameNs, false);
        assert(!pull.gripping);
        assert(pull.axis == 0.0F);
    }

    // Der Fehler aus dem Spiel: Nur greifen und die Hand unten halten darf
    // *nicht* weiterklettern. Nach dem Zug laeuft die Bewegung nur kurz aus.
    {
        fearvr::ClimbGripState state{};
        std::uint64_t now = 1'000'000'000ULL;
        float height = 1.5F;
        fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 0.0F, height, 0.9F), now, true);
        // Ein Zug von 30 cm ueber 10 Bilder.
        assert(PullRightHand(state, now, height, 10, 0.30F) > 0);
        // Danach still halten: hoechstens der kurze Nachlauf, dann Schluss.
        const int movingWhileHolding =
            HoldRightHand(state, now, height, 300);
        assert(movingWhileHolding <= 12);
        assert(HoldRightHand(state, now, height, 30) == 0);
    }

    // Ein zweiter, laengerer Zug bewegt auch laenger — die Kletterdauer haengt
    // am Ziehen, nicht an einem nachlaufenden Guthaben.
    {
        fearvr::ClimbGripState state{};
        std::uint64_t now = 1'000'000'000ULL;
        float height = 1.5F;
        fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 0.0F, height, 0.9F), now, true);
        const int shortPull = PullRightHand(state, now, height, 10, 0.20F);
        HoldRightHand(state, now, height, 60);
        const int longPull = PullRightHand(state, now, height, 40, 0.40F);
        assert(longPull > shortPull * 2);
    }

    // Greifen ohne jede Bewegung klettert nie.
    {
        fearvr::ClimbGripState state{};
        std::uint64_t now = 1'000'000'000ULL;
        fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 0.0F, 1.2F, 0.9F), now, true);
        assert(HoldRightHand(state, now, 1.2F, 200) == 0);
    }

    // Wiederholtes Ziehen klettert wieder — Nachgreifen funktioniert.
    {
        fearvr::ClimbGripState state{};
        std::uint64_t now = 1'000'000'000ULL;
        float height = 1.5F;
        fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 0.0F, height, 0.9F), now, true);
        assert(PullRightHand(state, now, height, 10, 0.30F) > 0);
        HoldRightHand(state, now, height, 60);
        // Loslassen, Hand wieder hochsetzen, neu greifen.
        now += kFrameNs;
        fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 0.0F, 1.5F, 0.0F), now, true);
        now += kFrameNs;
        height = 1.5F;
        fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 0.0F, height, 0.9F), now, true);
        assert(PullRightHand(state, now, height, 10, 0.30F) > 0);
    }

    // Die Hand anheben klettert abwaerts.
    {
        fearvr::ClimbGripState state{};
        std::uint64_t now = 1'000'000'000ULL;
        float height = 1.5F;
        fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 0.0F, height, 0.9F), now, true);
        assert(PullRightHand(state, now, height, 10, -0.30F, -1.0F) > 0);
    }

    // Ein neu gegriffener Halt bewegt nichts, obwohl die Hand tief steht.
    {
        fearvr::ClimbGripState state{};
        std::uint64_t now = 1'000'000'000ULL;
        now += kFrameNs;
        const fearvr::ClimbPull pull = fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 0.0F, 0.4F, 0.9F), now, true);
        assert(pull.gripping);
        assert(pull.axis == 0.0F);
    }

    // Hysterese: knapp unter der Greifschwelle bleibt der Griff bestehen,
    // erst unter der Loesschwelle faellt er weg.
    {
        fearvr::ClimbGripState state{};
        std::uint64_t now = 1'000'000'000ULL;
        fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 0.0F, 1.5F, 0.9F), now, true);
        now += kFrameNs;
        fearvr::ClimbPull pull = fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 0.0F, 1.5F, 0.55F), now, true);
        assert(pull.gripping);
        now += kFrameNs;
        pull = fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 0.0F, 1.5F, 0.30F), now, true);
        assert(!pull.gripping);
    }

    // Ein winziges Zittern am Griff bewegt nichts.
    {
        fearvr::ClimbGripState state{};
        std::uint64_t now = 1'000'000'000ULL;
        fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 0.0F, 1.5F, 0.9F), now, true);
        int moving = 0;
        for (int frame = 0; frame < 20; ++frame) {
            now += kFrameNs;
            const float jitter = (frame % 2 == 0) ? 1.502F : 1.498F;
            const fearvr::ClimbPull pull = fearvr::UpdateClimbGrip(
                state, ClimbInput(1.5F, 0.0F, jitter, 0.9F), now, true);
            if (pull.axis != 0.0F) {
                ++moving;
            }
        }
        assert(moving == 0);
    }

    // Beidhaendig: Die ziehende Hand fuehrt, die ruhende stoert nicht.
    {
        fearvr::ClimbGripState state{};
        std::uint64_t now = 1'000'000'000ULL;
        fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 0.9F, 1.5F, 0.9F), now, true);
        int moving = 0;
        float height = 1.5F;
        for (int frame = 0; frame < 10; ++frame) {
            now += kFrameNs;
            height -= 0.03F;
            const fearvr::ClimbPull pull = fearvr::UpdateClimbGrip(
                state, ClimbInput(1.5F, 0.9F, height, 0.9F), now, true);
            if (pull.axis > 0.0F) {
                ++moving;
            }
        }
        assert(moving > 0);
    }

    // Verlorene Handverfolgung loest den Griff, ohne einen Sprung zu machen.
    {
        fearvr::ClimbGripState state{};
        std::uint64_t now = 1'000'000'000ULL;
        fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 0.0F, 1.5F, 0.9F), now, true);
        now += kFrameNs;
        FearVrInputState lost = ClimbInput(1.5F, 0.0F, 1.2F, 0.9F);
        lost.gripPoseValidHands = FEARVR_HAND_MASK_LEFT;
        fearvr::ClimbPull pull =
            fearvr::UpdateClimbGrip(state, lost, now, true);
        assert(!pull.gripping);
        now += kFrameNs;
        pull = fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 0.0F, 1.2F, 0.9F), now, true);
        assert(pull.gripping);
        assert(pull.axis == 0.0F);
    }

    // Das Verlassen der Leiter vergisst jeden Zustand.
    {
        fearvr::ClimbGripState state{};
        std::uint64_t now = 1'000'000'000ULL;
        float height = 1.5F;
        fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 0.0F, height, 0.9F), now, true);
        PullRightHand(state, now, height, 10, 0.30F);
        now += kFrameNs;
        fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 0.0F, height, 0.9F), now, false);
        now += kFrameNs;
        const fearvr::ClimbPull pull = fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 0.0F, height, 0.9F), now, true);
        assert(pull.gripping);
        assert(pull.axis == 0.0F);
    }

    // Ein Sprung nach einer Unterbrechung erzeugt keine Kletterbewegung.
    {
        fearvr::ClimbGripState state{};
        std::uint64_t now = 1'000'000'000ULL;
        fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 0.0F, 1.5F, 0.9F), now, true);
        now += 3'000'000'000ULL;
        const fearvr::ClimbPull pull = fearvr::UpdateClimbGrip(
            state, ClimbInput(1.5F, 0.0F, 0.5F, 0.9F), now, true);
        assert(pull.gripping);
        assert(pull.axis == 0.0F);
    }

    return 0;
}
