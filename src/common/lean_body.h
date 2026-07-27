#pragma once

#include <cmath>
#include <cstdint>

namespace fearvr {

// Den Koerper dem physischen Lehnen nachziehen.
//
// Bewegt sich nur der Blickpunkt, bleibt alles andere an der Spielerposition
// stehen: Die Waffe wandert seltsam am Bildrand entlang, und um eine Ecke
// schiessen kann man nicht, weil Schussursprung und Kollisionskapsel weiter
// hinter der Wand stecken. Deshalb muss der Koerper folgen.
//
// Geschrieben wird dafuer nichts in die Spielerphysik. Der Regler hier gibt
// nur Achsenwerte aus, wie sie sonst der Stick liefert — die Engine bewegt
// den Spieler damit selbst, mitsamt Kollision. Was der Koerper bereits
// zurueckgelegt hat, zieht der Aufrufer vom Blickpunktversatz ab; beides
// zusammen ergibt immer den gewuenschten Versatz, egal wie weit der Koerper
// gerade gekommen ist.
//
// Alle Laengen in Spieleinheiten, alle Vektoren zweidimensional in der
// Blickbasis: x nach rechts, y nach vorne.

// Schaltschwellen mit Abstand: Der Regler laeuft erst ab dem groesseren Wert
// an und haelt erst unter dem kleineren wieder an. Ohne diesen Abstand
// schaltete er am Zielpunkt bildweise ein und aus.
constexpr float kLeanBodyStartUnits = 11.0F;
constexpr float kLeanBodyToleranceUnits = 6.0F;
// Beim Zurueckkehren in die Neutralstellung darf der Anker erst nahezu am
// Ausgangspunkt einrasten. Die normale sechs-Einheiten-Toleranz waere in VR
// als letzter seitlicher Sprung sichtbar.
constexpr float kLeanBodyNeutralToleranceUnits = 1.0F;
constexpr float kLeanBodyNeutralSpeedUnitsPerSecond = 2.0F;
// Ab diesem Abstand gibt der Regler vollen Ausschlag.
constexpr float kLeanBodyFullAxisUnits = 30.0F;
// Groesster Achsenwert. Lehnen ist eine kurze, ruhige Bewegung; voller
// Ausschlag waere ein Sprint zur Seite.
constexpr float kLeanBodyMaxAxis = 0.25F;
// Gegenanteil aus der gemessenen Geschwindigkeit. Die Spielerbewegung hat
// Traegheit: Stellt der Regler nur nach Restweg, gibt er bis zuletzt Vollgas,
// der Koerper gleitet ueber das Ziel, und der Regler dreht um — das ist das
// Hin und Her nach dem Lehnen um eine Ecke. Der Wert ist so gewaehlt, dass
// eine Geschwindigkeit von rund hundert Einheiten pro Sekunde den vollen
// Stellwert aufhebt.
constexpr float kLeanBodyDampingPerUnitPerSecond = 0.006F;
// Kommt der Koerper ueber diese Zeit nicht nennenswert voran, steht etwas im
// Weg. Der Regler hoert dann auf zu stellen: Gegen eine Wand zu druecken
// bringt den Koerper nicht ans Ziel, laesst ihn aber an der Flaeche entlang
// rutschen — und genau dieses Rutschen ist das Wackeln in Wandnaehe.
constexpr float kLeanBodyStallSeconds = 0.30F;
// Wie viel Fortschritt in dieser Zeit als „kommt voran" zaehlt.
constexpr float kLeanBodyStallProgressUnits = 2.0F;
// Aendert sich der Abstand zum Ziel um mehr als das, ist die Lage neu und der
// Regler versucht es wieder — etwa wenn der Spieler sich zurueckbewegt.
constexpr float kLeanBodyStallReleaseUnits = 6.0F;
// Ein gemessener Versatz ueber dieser Groesse stammt nicht vom Lehnen,
// sondern von einem Teleport, einer Zwischensequenz oder einem
// Ladebildschirm. Dann faengt die Nachfuehrung von vorne an.
constexpr float kLeanBodyTeleportUnits = 150.0F;

struct LeanBodyState {
    // Letzte Messung, fuer die Geschwindigkeit der Daempfung.
    float lastMeasuredRight{0.0F};
    float lastMeasuredForward{0.0F};
    // Fortschrittsueberwachung gegen das Druecken an Waenden.
    float bestRestMagnitude{0.0F};
    float stallRestMagnitude{0.0F};
    float stallSeconds{0.0F};
    bool stalled{false};
    bool haveMeasured{false};
    bool active{false};
    bool neutralReturnActive{false};
};

struct LeanBodyOutput {
    // Wie der Stick: -1 bis +1, rechts beziehungsweise vorwaerts positiv.
    float strafeAxis{0.0F};
    float forwardAxis{0.0F};
    // Gemessener Koerperversatz, den der Aufrufer noch im selben Bild vom
    // Blickpunkt abzieht.
    float appliedRight{0.0F};
    float appliedForward{0.0F};
    bool moving{false};
    // Der Aufrufer soll den Ankerpunkt auf die aktuelle Spielerposition
    // setzen: Es wird gerade nicht nachgefuehrt.
    bool resetAnchor{false};
};

inline void ResetLeanBody(LeanBodyState& state) noexcept {
    state = LeanBodyState{};
}

// Ein Bild weiterrechnen.
//
// `desiredRight`/`desiredForward`: der gewuenschte Versatz aus dem
// Kopftracking. `movedRight`/`movedForward`: was sich die Spielerposition
// seit dem letzten Bild tatsaechlich bewegt hat, in derselben Basis.
// `playerControlled` meldet, dass der Spieler gerade selbst laeuft — dann
// gehoert die Fortbewegung ihm, und die Nachfuehrung haelt sich vollstaendig
// heraus.
// Ein Bild weiterrechnen.
//
// `measuredRight`/`measuredForward`: wo der Koerper *jetzt* steht, gemessen
// gegenueber dem Ankerpunkt, den der Aufrufer haelt. Bewusst eine absolute
// Messung und keine aufsummierte Bewegung: Die Spielerkamera schwankt im
// Betrieb staendig ein wenig, und aufsummiert ergaben diese Schwankungen
// einen Versatz, gegen den der Regler ansteuerte — er erzeugte damit genau
// die Bewegung, die er gleich darauf wieder als Versatz mass. Das Bild
// wackelte dauerhaft, auch ohne jedes Lehnen.
//
// `playerControlled` meldet, dass der Spieler gerade selbst laeuft — dann
// gehoert die Fortbewegung ihm, und die Nachfuehrung haelt sich heraus.
inline LeanBodyOutput UpdateLeanBody(
    LeanBodyState& state,
    float desiredRight, float desiredForward,
    float measuredRight, float measuredForward,
    float seconds,
    bool playerControlled, bool enabled) noexcept {
    LeanBodyOutput output;
    if (!enabled || !std::isfinite(desiredRight) ||
        !std::isfinite(desiredForward) || !std::isfinite(measuredRight) ||
        !std::isfinite(measuredForward) || !std::isfinite(seconds) ||
        seconds < 0.0F) {
        ResetLeanBody(state);
        output.resetAnchor = true;
        return output;
    }

    const float measuredMagnitude = std::sqrt(
        measuredRight * measuredRight +
        measuredForward * measuredForward);
    if (measuredMagnitude > kLeanBodyTeleportUnits || playerControlled) {
        ResetLeanBody(state);
        output.resetAnchor = true;
        return output;
    }

    float restRight = desiredRight - measuredRight;
    float restForward = desiredForward - measuredForward;
    const float restMagnitude = std::sqrt(
        restRight * restRight + restForward * restForward);
    const float desiredMagnitude = std::sqrt(
        desiredRight * desiredRight + desiredForward * desiredForward);

    // Geschwindigkeit des Koerpers aus zwei Messungen.
    float velocityRight = 0.0F;
    float velocityForward = 0.0F;
    if (state.haveMeasured && seconds > 0.0F) {
        velocityRight =
            (measuredRight - state.lastMeasuredRight) / seconds;
        velocityForward =
            (measuredForward - state.lastMeasuredForward) / seconds;
    }
    state.lastMeasuredRight = measuredRight;
    state.lastMeasuredForward = measuredForward;
    state.haveMeasured = true;
    const float measuredSpeed = std::sqrt(
        velocityRight * velocityRight +
        velocityForward * velocityForward);
    const bool returningToNeutral =
        desiredMagnitude <= kLeanBodyToleranceUnits;
    if (!returningToNeutral) {
        state.neutralReturnActive = false;
    } else if (measuredMagnitude > kLeanBodyNeutralToleranceUnits) {
        state.neutralReturnActive = true;
    }

    // Erst neu verankern, wenn der Rueckweg fast vollstaendig und fast ohne
    // Restgeschwindigkeit beendet ist. Sonst folgt der neue Anker der noch
    // ausrollenden Retail-Kamera und macht ihre letzten Bilder wieder sichtbar.
    if (returningToNeutral &&
        measuredMagnitude <= kLeanBodyNeutralToleranceUnits &&
        (!state.neutralReturnActive ||
         measuredSpeed <= kLeanBodyNeutralSpeedUnitsPerSecond)) {
        state.active = false;
        output.resetAnchor = true;
        return output;
    }

    // Steht etwas im Weg? Erst wieder versuchen, wenn sich die Lage
    // wesentlich geaendert hat.
    if (returningToNeutral) {
        // Der Weg zur alten Fussposition war vor dem Lehnen frei. Beim
        // Rueckweg darf die Wanderkennung nicht kurz vor dem Ziel abschalten.
        state.stalled = false;
        state.stallSeconds = 0.0F;
    } else if (state.stalled) {
        if (std::fabs(restMagnitude - state.stallRestMagnitude) >
            kLeanBodyStallReleaseUnits) {
            state.stalled = false;
            state.stallSeconds = 0.0F;
            state.bestRestMagnitude = restMagnitude;
        }
    }

    state.active = !state.stalled &&
        (returningToNeutral
            ? restMagnitude > kLeanBodyNeutralToleranceUnits
            : (state.active ? restMagnitude > kLeanBodyToleranceUnits
                            : restMagnitude > kLeanBodyStartUnits));

    if (state.active && !returningToNeutral) {
        if (restMagnitude < state.bestRestMagnitude -
                kLeanBodyStallProgressUnits ||
            state.bestRestMagnitude == 0.0F) {
            state.bestRestMagnitude = restMagnitude;
            state.stallSeconds = 0.0F;
        } else {
            state.stallSeconds += seconds;
            if (state.stallSeconds >= kLeanBodyStallSeconds) {
                state.stalled = true;
                state.stallRestMagnitude = restMagnitude;
                state.active = false;
            }
        }
    } else {
        state.bestRestMagnitude = restMagnitude;
        state.stallSeconds = 0.0F;
    }

    if (state.active) {
        float strength = restMagnitude / kLeanBodyFullAxisUnits;
        if (strength > 1.0F) {
            strength = 1.0F;
        }
        const float axis = strength * kLeanBodyMaxAxis;
        float strafe = (restRight / restMagnitude) * axis;
        float forward = (restForward / restMagnitude) * axis;
        // Was der Koerper schon an Geschwindigkeit hat, wird abgezogen.
        strafe -= velocityRight * kLeanBodyDampingPerUnitPerSecond;
        forward -= velocityForward * kLeanBodyDampingPerUnitPerSecond;
        const auto Clamp = [](float value) {
            if (value > kLeanBodyMaxAxis) {
                return kLeanBodyMaxAxis;
            }
            if (value < -kLeanBodyMaxAxis) {
                return -kLeanBodyMaxAxis;
            }
            return value;
        };
        output.strafeAxis = Clamp(strafe);
        output.forwardAxis = Clamp(forward);
        output.moving =
            output.strafeAxis != 0.0F || output.forwardAxis != 0.0F;
    }

    // Die Retail-Basiskamera enthaelt die gemessene Koerperbewegung bereits in
    // diesem Bild. Deshalb muss derselbe Wert sofort und vollstaendig
    // kompensiert werden, auch beim Ueberschwingen und auf dem Rueckweg. Eine
    // Begrenzung auf den aktuellen Kopfversatz machte das Pendeln des Koerpers
    // wieder in der Kamera sichtbar, sobald der Kopf die Neutralstellung
    // schneller erreichte als der Koerper.
    output.appliedRight = measuredRight;
    output.appliedForward = measuredForward;
    return output;
}

} // namespace fearvr
