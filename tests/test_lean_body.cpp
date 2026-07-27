#include <cassert>
#include <cmath>

#include "lean_body.h"

namespace {

constexpr float kFrameSeconds = 1.0F / 90.0F;

// Grobes Modell der Spielerbewegung: Die Achse beschleunigt, Reibung bremst.
// Ohne Traegheit waere der Test wertlos — genau sie laesst einen ungedaempften
// Regler ueber das Ziel schiessen.
struct BodySim {
    float position{0.0F};
    float velocity{0.0F};

    void Step(float axis) {
        constexpr float kAccelPerAxis = 1400.0F;
        constexpr float kFriction = 1.5F;
        velocity += axis * kAccelPerAxis * kFrameSeconds;
        velocity -= velocity * kFriction * kFrameSeconds;
        position += velocity * kFrameSeconds;
    }
};

// Faehrt den Regler, bis er zur Ruhe kommt. Liefert die Zahl der Bilder.
int Settle(
    fearvr::LeanBodyState& state, float desiredRight, BodySim& body,
    int maxFrames = 600) {
    for (int frame = 0; frame < maxFrames; ++frame) {
        const fearvr::LeanBodyOutput output = fearvr::UpdateLeanBody(
            state, desiredRight, 0.0F, body.position, 0.0F, kFrameSeconds,
            false, true);
        if (output.resetAnchor) {
            body.position = 0.0F;
            body.velocity = 0.0F;
        }
        if (!output.moving && std::fabs(body.velocity) < 1.0F) {
            return frame;
        }
        body.Step(output.strafeAxis);
    }
    return maxFrames;
}

} // namespace

int main() {
    // Abgeschaltet passiert nichts.
    {
        fearvr::LeanBodyState state{};
        const fearvr::LeanBodyOutput output = fearvr::UpdateLeanBody(
            state, 30.0F, 0.0F, 0.0F, 0.0F, kFrameSeconds, false, false);
        assert(!output.moving);
        assert(output.strafeAxis == 0.0F);
    }

    // Ein Lehnen nach rechts schiebt den Koerper nach rechts.
    {
        fearvr::LeanBodyState state{};
        const fearvr::LeanBodyOutput output = fearvr::UpdateLeanBody(
            state, 30.0F, 0.0F, 0.0F, 0.0F, kFrameSeconds, false, true);
        assert(output.moving);
        assert(output.strafeAxis > 0.0F);
        assert(output.forwardAxis == 0.0F);
        assert(output.strafeAxis <= fearvr::kLeanBodyMaxAxis);
    }

    // Der Koerper kommt an und bleibt dann stehen.
    {
        fearvr::LeanBodyState state{};
        BodySim body;
        const int frames = Settle(state, 30.0F, body);
        assert(frames > 0);
        assert(frames < 500);
        assert(std::fabs(body.position - 30.0F) <
               fearvr::kLeanBodyStartUnits);
        const fearvr::LeanBodyOutput idle = fearvr::UpdateLeanBody(
            state, 30.0F, 0.0F, body.position, 0.0F, kFrameSeconds, false,
            true);
        assert(!idle.moving);
    }

    // Der entscheidende Fall: Ohne Lehnen und ohne Versatz stellt der Regler
    // nichts — und der Anker wandert mit, damit gewoehnliche Spielbewegung
    // nicht als Lehnen gilt. Genau hier entstand das Dauerwackeln.
    {
        fearvr::LeanBodyState state{};
        for (int frame = 0; frame < 200; ++frame) {
            // Die Spielerkamera schwankt bildweise ein wenig.
            const float jitter = (frame % 2 == 0) ? 0.6F : -0.6F;
            const fearvr::LeanBodyOutput output = fearvr::UpdateLeanBody(
                state, 0.0F, 0.0F, jitter, 0.0F, kFrameSeconds, false, true);
            assert(!output.moving);
            assert(output.strafeAxis == 0.0F);
            assert(output.resetAnchor);
        }
    }

    // Zurueckkommen zieht den Koerper wieder zurueck.
    {
        fearvr::LeanBodyState state{};
        float right = 30.0F;
        float forward = 0.0F;
        const fearvr::LeanBodyOutput output = fearvr::UpdateLeanBody(
            state, 0.0F, 0.0F, right, forward, kFrameSeconds, false, true);
        assert(output.moving);
        assert(output.strafeAxis < 0.0F);
    }

    // Diagonales Lehnen bedient beide Achsen.
    {
        fearvr::LeanBodyState state{};
        const fearvr::LeanBodyOutput output = fearvr::UpdateLeanBody(
            state, 20.0F, 20.0F, 0.0F, 0.0F, kFrameSeconds, false, true);
        assert(output.strafeAxis > 0.0F);
        assert(output.forwardAxis > 0.0F);
        assert(std::fabs(output.strafeAxis - output.forwardAxis) < 0.01F);
    }

    // Laeuft der Spieler selbst, haelt sich die Nachfuehrung heraus und der
    // Anker wandert mit.
    {
        fearvr::LeanBodyState state{};
        const fearvr::LeanBodyOutput output = fearvr::UpdateLeanBody(
            state, 30.0F, 0.0F, 10.0F, 0.0F, kFrameSeconds, true, true);
        assert(!output.moving);
        assert(output.strafeAxis == 0.0F);
        assert(output.resetAnchor);
        assert(output.appliedRight == 0.0F);
    }

    // Eine Wand: Der Koerper kommt nicht voran. Der Regler hoert dann auf zu
    // stellen, statt dauerhaft dagegen zu druecken — sonst rutscht der
    // Koerper an der Flaeche entlang, und genau das wackelt in Wandnaehe.
    {
        fearvr::LeanBodyState state{};
        bool stoppedPushing = false;
        for (int frame = 0; frame < 100; ++frame) {
            const fearvr::LeanBodyOutput output = fearvr::UpdateLeanBody(
                state, 40.0F, 0.0F, 0.0F, 0.0F, kFrameSeconds, false, true);
            assert(std::fabs(output.strafeAxis) <=
                   fearvr::kLeanBodyMaxAxis);
            if (!output.moving && frame > 5) {
                stoppedPushing = true;
                break;
            }
        }
        assert(stoppedPushing);
        // Und bleibt ruhig, solange sich nichts aendert.
        for (int frame = 0; frame < 200; ++frame) {
            const fearvr::LeanBodyOutput output = fearvr::UpdateLeanBody(
                state, 40.0F, 0.0F, 0.0F, 0.0F, kFrameSeconds, false, true);
            assert(!output.moving);
        }
        // Bewegt sich der Spieler zurueck, versucht der Regler es wieder.
        const fearvr::LeanBodyOutput retry = fearvr::UpdateLeanBody(
            state, 15.0F, 0.0F, 0.0F, 0.0F, kFrameSeconds, false, true);
        assert(retry.moving);
    }

    // Ueberschwinger werden vollstaendig kompensiert. Die Sicht bleibt am
    // physischen Kopf, auch wenn der Koerper kurz weiter als dieser laeuft.
    {
        fearvr::LeanBodyState state{};
        // Wenige Bilder: Die Wanderkennung greift erst nach 300 ms.
        for (int frame = 0; frame < 8; ++frame) {
            fearvr::UpdateLeanBody(
                state, 20.0F, 0.0F, 40.0F, 0.0F, kFrameSeconds, false, true);
        }
        const fearvr::LeanBodyOutput output = fearvr::UpdateLeanBody(
            state, 20.0F, 0.0F, 40.0F, 0.0F, kFrameSeconds, false, true);
        assert(std::fabs(output.appliedRight - 40.0F) < 0.001F);
        assert(output.strafeAxis < 0.0F);
    }

    // Dasselbe nach links.
    {
        fearvr::LeanBodyState state{};
        for (int frame = 0; frame < 8; ++frame) {
            fearvr::UpdateLeanBody(
                state, -20.0F, 0.0F, -40.0F, 0.0F, kFrameSeconds, false,
                true);
        }
        const fearvr::LeanBodyOutput output = fearvr::UpdateLeanBody(
            state, -20.0F, 0.0F, -40.0F, 0.0F, kFrameSeconds, false, true);
        assert(std::fabs(output.appliedRight + 40.0F) < 0.001F);
    }

    // Die Retail-Basiskamera traegt die Koerperbewegung sofort. Der davon
    // abgezogene Anteil muss deshalb im selben Bild folgen; sonst springt der
    // sichtbare Blickpunkt beim Uebergang vom geraden Stand ins Lehnen erst
    // mit dem Koerper und wird anschliessend mehrfach zurueckgezogen.
    {
        fearvr::LeanBodyState state{};
        constexpr float desired = 30.0F;
        constexpr float measuredValues[] = {
            0.0F, 2.0F, 5.0F, 9.0F, 14.0F};
        for (const float measured : measuredValues) {
            const fearvr::LeanBodyOutput output = fearvr::UpdateLeanBody(
                state, desired, 0.0F, measured, 0.0F, kFrameSeconds, false,
                true);
            assert(std::fabs(output.appliedRight - measured) < 0.001F);
            const float visiblePosition =
                measured + desired - output.appliedRight;
            assert(std::fabs(visiblePosition - desired) < 0.001F);
        }
    }

    // Beim Verlassen des Leans bleibt der sichtbare Blickpunkt ruhig, bis der
    // traege Retail-Koerper wirklich wieder am Ausgangspunkt angekommen ist.
    {
        fearvr::LeanBodyState state{};
        BodySim body;
        body.position = 30.0F;
        bool reset = false;
        for (int frame = 0; frame < 900; ++frame) {
            const fearvr::LeanBodyOutput output = fearvr::UpdateLeanBody(
                state, 0.0F, 0.0F, body.position, 0.0F, kFrameSeconds, false,
                true);
            if (output.resetAnchor) {
                assert(std::fabs(body.position) <=
                       fearvr::kLeanBodyNeutralToleranceUnits);
                assert(std::fabs(body.velocity) <=
                       fearvr::kLeanBodyNeutralSpeedUnitsPerSecond);
                reset = true;
                break;
            }
            const float visiblePosition =
                body.position - output.appliedRight;
            assert(std::fabs(visiblePosition) < 0.001F);
            body.Step(output.strafeAxis);
        }
        assert(reset);
    }

    // Ein Teleport setzt die Nachfuehrung zurueck.
    {
        fearvr::LeanBodyState state{};
        const fearvr::LeanBodyOutput output = fearvr::UpdateLeanBody(
            state, 30.0F, 0.0F, 5000.0F, 0.0F, kFrameSeconds, false, true);
        assert(!output.moving);
        assert(output.resetAnchor);
    }

    // Der eigentliche Fall aus dem Spiel: eine Spielerbewegung *mit
    // Traegheit*. Ohne Daempfung gibt der Regler bis zuletzt Vollgas, der
    // Koerper gleitet ueber das Ziel, und alles schwingt in der X-Achse hin
    // und her. Hier laeuft ein grobes Modell der Engine mit: Die Achse
    // beschleunigt, Reibung bremst.
    {
        fearvr::LeanBodyState state{};
        float position = 0.0F;
        float velocity = 0.0F;
        const float accelPerAxis = 1400.0F; // Einheiten/s^2 bei Vollausschlag
        const float friction = 1.5F;        // 1/s, traege wie im Spiel
        int reversals = 0;
        float previousAxis = 0.0F;
        float worstOvershoot = 0.0F;
        for (int frame = 0; frame < 600; ++frame) {
            const fearvr::LeanBodyOutput output = fearvr::UpdateLeanBody(
                state, 30.0F, 0.0F, position, 0.0F, kFrameSeconds, false,
                true);
            if (previousAxis != 0.0F && output.strafeAxis != 0.0F &&
                (previousAxis > 0.0F) != (output.strafeAxis > 0.0F)) {
                ++reversals;
            }
            previousAxis = output.strafeAxis;
            velocity += output.strafeAxis * accelPerAxis * kFrameSeconds;
            velocity -= velocity * friction * kFrameSeconds;
            position += velocity * kFrameSeconds;
            if (frame > 200) {
                const float error = std::fabs(position - 30.0F);
                if (error > worstOvershoot) {
                    worstOvershoot = error;
                }
            }
        }
        // Es laeuft ein und bleibt eingelaufen ...
        assert(worstOvershoot < fearvr::kLeanBodyStartUnits);
        // ... ohne dauerhaftes Hin und Her.
        assert(reversals < 6);
    }

    return 0;
}
