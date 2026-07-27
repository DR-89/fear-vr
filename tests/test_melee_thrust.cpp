#include <cassert>
#include <cmath>
#include <cstdint>

#include "melee_thrust.h"

namespace {

constexpr std::uint64_t kFrameNs = 11'000'000ULL; // ~90 Hz

// Waffenhand an `position`, Blick entlang -Z (Identitaetsquaternion), sofern
// keine andere Ausrichtung gesetzt wird.
FearVrInputState HandAt(
    std::uint64_t timeNs, float x, float y, float z) {
    FearVrInputState input{};
    input.flags = FEARVR_IF_VALID | FEARVR_IF_FOCUSED;
    input.activeHands =
        FEARVR_HAND_MASK_LEFT | FEARVR_HAND_MASK_RIGHT;
    input.aimPoseValidHands = input.activeHands;
    input.gripPoseValidHands = input.activeHands;
    input.predictedDisplayTimeNs = timeNs;
    for (std::uint32_t hand = 0; hand < FEARVR_HAND_COUNT; ++hand) {
        input.handAimPose[hand].qw = 1.0F;
        input.handGripPose[hand].qw = 1.0F;
    }
    input.handAimPose[FEARVR_HAND_RIGHT].px = x;
    input.handAimPose[FEARVR_HAND_RIGHT].py = y;
    input.handAimPose[FEARVR_HAND_RIGHT].pz = z;
    return input;
}

// Bewegt die Hand `frames` Bilder lang mit `speed` m/s entlang der Achse
// (dx, dy, dz) und meldet, wie oft die Geste dabei ausgeloest hat.
int RunMotion(
    fearvr::MeleeThrustDetector& detector, std::uint64_t& timeNs,
    float& x, float& y, float& z, int frames, float speed,
    float dx, float dy, float dz, bool suppressed = false) {
    const float seconds = static_cast<float>(kFrameNs) * 1.0e-9F;
    int hits = 0;
    for (int frame = 0; frame < frames; ++frame) {
        timeNs += kFrameNs;
        x += dx * speed * seconds;
        y += dy * speed * seconds;
        z += dz * speed * seconds;
        if (fearvr::UpdateMeleeThrust(
                detector, HandAt(timeNs, x, y, z), timeNs,
                FEARVR_HAND_RIGHT, suppressed)) {
            ++hits;
        }
    }
    return hits;
}

} // namespace

int main() {
    // Ein zuegiger Stoss nach vorne loest genau einmal aus, auch wenn die
    // Bewegung ueber mehrere Bilder laeuft.
    {
        fearvr::MeleeThrustDetector detector{};
        std::uint64_t now = 1'000'000'000ULL;
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
        // Erste Abtastung baut nur die Historie auf.
        assert(!fearvr::UpdateMeleeThrust(
            detector, HandAt(now, x, y, z), now));
        assert(RunMotion(
                   detector, now, x, y, z, 8, 3.0F,
                   0.0F, 0.0F, -1.0F) == 1);
    }

    // Ruhige Bewegung derselben Richtung bleibt unter der Schwelle.
    {
        fearvr::MeleeThrustDetector detector{};
        std::uint64_t now = 1'000'000'000ULL;
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
        assert(!fearvr::UpdateMeleeThrust(
            detector, HandAt(now, x, y, z), now));
        assert(RunMotion(
                   detector, now, x, y, z, 30, 1.2F,
                   0.0F, 0.0F, -1.0F) == 0);
    }

    // Ein schneller Schwenk quer zur Zielrichtung ist kein Stoss.
    {
        fearvr::MeleeThrustDetector detector{};
        std::uint64_t now = 1'000'000'000ULL;
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
        assert(!fearvr::UpdateMeleeThrust(
            detector, HandAt(now, x, y, z), now));
        assert(RunMotion(
                   detector, now, x, y, z, 10, 4.0F,
                   1.0F, 0.0F, 0.0F) == 0);
        // Ebenso das schnelle Zurueckziehen.
        assert(RunMotion(
                   detector, now, x, y, z, 10, 4.0F,
                   0.0F, 0.0F, 1.0F) == 0);
    }

    // Stoss, Ruecknahme, Stoss: zwei Schlaege, keiner davon doppelt.
    {
        fearvr::MeleeThrustDetector detector{};
        std::uint64_t now = 1'000'000'000ULL;
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
        assert(!fearvr::UpdateMeleeThrust(
            detector, HandAt(now, x, y, z), now));
        int hits = RunMotion(
            detector, now, x, y, z, 8, 3.0F, 0.0F, 0.0F, -1.0F);
        // Ruecknahme und Pause, laenger als die Sperre.
        hits += RunMotion(
            detector, now, x, y, z, 8, 1.0F, 0.0F, 0.0F, 1.0F);
        hits += RunMotion(
            detector, now, x, y, z, 70, 0.0F, 0.0F, 0.0F, 0.0F);
        hits += RunMotion(
            detector, now, x, y, z, 8, 3.0F, 0.0F, 0.0F, -1.0F);
        assert(hits == 2);
    }

    // Innerhalb der Sperre bleibt der zweite Stoss aus.
    {
        fearvr::MeleeThrustDetector detector{};
        std::uint64_t now = 1'000'000'000ULL;
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
        assert(!fearvr::UpdateMeleeThrust(
            detector, HandAt(now, x, y, z), now));
        int hits = RunMotion(
            detector, now, x, y, z, 8, 3.0F, 0.0F, 0.0F, -1.0F);
        hits += RunMotion(
            detector, now, x, y, z, 4, 1.0F, 0.0F, 0.0F, 1.0F);
        hits += RunMotion(
            detector, now, x, y, z, 8, 3.0F, 0.0F, 0.0F, -1.0F);
        assert(hits == 1);
    }

    // Der Sperrparameter haelt die Geste zurueck, ohne die Historie zu
    // verlieren. Der GameClient nutzt ihn derzeit nicht: Nahkampf gilt auch
    // fuer beidhaendig gefuehrte Waffen.
    {
        fearvr::MeleeThrustDetector detector{};
        std::uint64_t now = 1'000'000'000ULL;
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
        assert(!fearvr::UpdateMeleeThrust(
            detector, HandAt(now, x, y, z), now, FEARVR_HAND_RIGHT,
            true));
        assert(RunMotion(
                   detector, now, x, y, z, 8, 3.0F,
                   0.0F, 0.0F, -1.0F, true) == 0);
    }

    // Ein Sprung nach einer Unterbrechung (Ladebildschirm) zaehlt nicht.
    {
        fearvr::MeleeThrustDetector detector{};
        std::uint64_t now = 1'000'000'000ULL;
        assert(!fearvr::UpdateMeleeThrust(
            detector, HandAt(now, 0.0F, 0.0F, 0.0F), now));
        now += 2'000'000'000ULL;
        assert(!fearvr::UpdateMeleeThrust(
            detector, HandAt(now, 0.0F, 0.0F, -1.5F), now));
    }

    // Verlorene Handverfolgung verwirft die Historie, statt beim Wiedersehen
    // eine Scheingeschwindigkeit zu erzeugen.
    {
        fearvr::MeleeThrustDetector detector{};
        std::uint64_t now = 1'000'000'000ULL;
        assert(!fearvr::UpdateMeleeThrust(
            detector, HandAt(now, 0.0F, 0.0F, 0.0F), now));
        now += kFrameNs;
        FearVrInputState lost = HandAt(now, 0.0F, 0.0F, 0.0F);
        lost.aimPoseValidHands = FEARVR_HAND_MASK_LEFT;
        assert(!fearvr::UpdateMeleeThrust(detector, lost, now));
        now += kFrameNs;
        assert(!fearvr::UpdateMeleeThrust(
            detector, HandAt(now, 0.0F, 0.0F, -0.1F), now));
    }

    // Die Richtung folgt der Hand, nicht der Welt: um 90 Grad nach links
    // gedreht zeigt die Hand nach -X, und ein Stoss dorthin zaehlt.
    {
        fearvr::MeleeThrustDetector detector{};
        std::uint64_t now = 1'000'000'000ULL;
        const float half = std::sqrt(0.5F);
        float x = 0.0F;
        const auto TurnedHand = [&](std::uint64_t timeNs, float px) {
            FearVrInputState input = HandAt(timeNs, px, 0.0F, 0.0F);
            // Drehung um +Y um +90 Grad: -Z wird zu -X.
            input.handAimPose[FEARVR_HAND_RIGHT].qy = half;
            input.handAimPose[FEARVR_HAND_RIGHT].qw = half;
            return input;
        };
        assert(!fearvr::UpdateMeleeThrust(
            detector, TurnedHand(now, x), now));
        const float seconds = static_cast<float>(kFrameNs) * 1.0e-9F;
        int hits = 0;
        for (int frame = 0; frame < 8; ++frame) {
            now += kFrameNs;
            x -= 3.0F * seconds;
            if (fearvr::UpdateMeleeThrust(
                    detector, TurnedHand(now, x), now)) {
                ++hits;
            }
        }
        assert(hits == 1);
    }

    return 0;
}
