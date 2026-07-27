#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

#include "lean_collision.h"

namespace {

constexpr std::uint64_t kFrameNs = 11'000'000ULL; // ~90 Hz

} // namespace

int main() {
    // Freie Sicht: Der Versatz bleibt vollstaendig erhalten.
    assert(fearvr::LeanCollisionScale(30.0F, 0.0F, false) == 1.0F);
    // Wand weit genug entfernt.
    assert(fearvr::LeanCollisionScale(30.0F, 200.0F, true) == 1.0F);
    // Wand innerhalb des Versatzes: anteilig gekuerzt, abzueglich Abstand.
    {
        const float scale = fearvr::LeanCollisionScale(40.0F, 32.0F, true);
        assert(scale > 0.0F && scale < 1.0F);
        assert(std::fabs(scale - (32.0F - 12.0F) / 40.0F) < 0.001F);
    }
    // Wand direkt am Kopf: kein Versatz.
    assert(fearvr::LeanCollisionScale(40.0F, 5.0F, true) == 0.0F);
    // Kein gewuenschter Versatz: nichts zu kuerzen.
    assert(fearvr::LeanCollisionScale(0.0F, 5.0F, true) == 1.0F);
    // Unsinnige Messwerte lassen den Versatz unangetastet.
    assert(fearvr::LeanCollisionScale(
               30.0F, std::numeric_limits<float>::infinity(), true) == 1.0F);

    // Enger werden geht schnell, aber nicht sprunghaft: An einer Kante
    // wechselt der Strahl bildweise, und ein harter Sprung waere sichtbar.
    {
        fearvr::LeanCollisionState state{};
        std::uint64_t now = 1'000'000'000ULL;
        fearvr::UpdateLeanCollision(state, 1.0F, now);
        now += kFrameNs;
        const float first = fearvr::UpdateLeanCollision(state, 0.0F, now);
        assert(first < 1.0F);
        assert(first > 0.0F);
        // Nach wenigen Bildern ist die Begrenzung dennoch zu.
        for (int frame = 0; frame < 12; ++frame) {
            now += kFrameNs;
            fearvr::UpdateLeanCollision(state, 0.0F, now);
        }
        assert(state.scale < 0.05F);
        // Deutlich schneller als das Oeffnen.
        assert(fearvr::kLeanCollisionTightenSeconds <
               fearvr::kLeanCollisionSmoothingSeconds);
    }

    // Weiter werden geschieht erst nach dem Haltefenster, und dann
    // geglaettet — nie sprunghaft.
    {
        fearvr::LeanCollisionState state{};
        std::uint64_t now = 1'000'000'000ULL;
        fearvr::UpdateLeanCollision(state, 0.0F, now);
        // Innerhalb des Haltefensters bleibt es zu.
        for (int frame = 0; frame < 10; ++frame) {
            now += kFrameNs;
            assert(fearvr::UpdateLeanCollision(state, 1.0F, now) == 0.0F);
        }
        // Nach Ablauf oeffnet es, aber ueber mehrere Bilder verteilt.
        int partiallyOpenFrames = 0;
        for (int frame = 0; frame < 80; ++frame) {
            now += kFrameNs;
            const float scale =
                fearvr::UpdateLeanCollision(state, 1.0F, now);
            if (scale > 0.0F && scale < 0.99F) {
                ++partiallyOpenFrames;
            }
        }
        assert(partiallyOpenFrames > 3);
        assert(state.scale > 0.99F);
    }

    // Ein Sprung in der Zeit (Ladebildschirm) uebernimmt sofort.
    {
        fearvr::LeanCollisionState state{};
        std::uint64_t now = 1'000'000'000ULL;
        fearvr::UpdateLeanCollision(state, 0.0F, now);
        now += 5'000'000'000ULL;
        assert(fearvr::UpdateLeanCollision(state, 1.0F, now) == 1.0F);
    }

    // Kantenflackern: Der Strahl trifft abwechselnd. Die Begrenzung darf
    // nicht im selben Takt mitwechseln, sondern haelt den engen Wert.
    {
        fearvr::LeanCollisionState state{};
        std::uint64_t now = 1'000'000'000ULL;
        fearvr::UpdateLeanCollision(state, 1.0F, now);
        float highest = 0.0F;
        float lowest = 1.0F;
        for (int frame = 0; frame < 60; ++frame) {
            now += kFrameNs;
            const float target = (frame % 2 == 0) ? 0.2F : 1.0F;
            const float scale =
                fearvr::UpdateLeanCollision(state, target, now);
            if (frame > 10) {
                if (scale > highest) { highest = scale; }
                if (scale < lowest) { lowest = scale; }
            }
        }
        // Die Schwankung bleibt klein, statt zwischen 0.2 und 1.0 zu springen.
        assert(highest - lowest < 0.1F);
        assert(highest < 0.45F);
    }

    // Ist der Weg dauerhaft frei, oeffnet die Begrenzung nach dem
    // Haltefenster wieder ganz.
    {
        fearvr::LeanCollisionState state{};
        std::uint64_t now = 1'000'000'000ULL;
        fearvr::UpdateLeanCollision(state, 0.2F, now);
        for (int frame = 0; frame < 120; ++frame) {
            now += kFrameNs;
            fearvr::UpdateLeanCollision(state, 1.0F, now);
        }
        assert(state.scale > 0.95F);
    }

    // Zuruecksetzen gibt den Versatz wieder frei.
    {
        fearvr::LeanCollisionState state{};
        fearvr::UpdateLeanCollision(state, 0.0F, kFrameNs);
        fearvr::ResetLeanCollision(state);
        assert(state.scale == 1.0F);
    }

    return 0;
}
