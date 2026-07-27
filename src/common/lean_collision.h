#pragma once

#include <cmath>
#include <cstdint>

namespace fearvr {

// Physisches Lehnen mit Weltkollision.
//
// F.E.A.R. selbst kennt kein Lehnen, das den Koerper versetzt:
// `CLeanMgr` liefert nur einen Winkel, der als Rollen auf die Kamera und die
// Koerperknoten geht. Der Blickpunkt bleibt dabei genau dort, wo der Spieler
// steht — um eine Ecke sehen kann man damit nicht.
//
// Der Kopfversatz aus dem Headtracking liefert diese Bewegung. Ungebremst
// wuerde er den Blickpunkt allerdings in die Wand schieben, weil die
// Kollisionskapsel des Spielers stehen bleibt. Diese Datei entscheidet
// deshalb, wie viel von einem gewuenschten Versatz uebrig bleibt, wenn die
// Welt im Weg ist. Sie kennt die Welt nicht selbst — sie bekommt nur die
// freie Strecke, die ein Strahl entlang des Versatzes gemessen hat.

// Sicherheitsabstand zur getroffenen Flaeche, in Spieleinheiten. Ohne ihn
// klebte die Nahebene direkt auf der Wand, und man saehe hindurch.
constexpr float kLeanCollisionMarginUnits = 12.0F;
// Wie schnell sich die Begrenzung an eine neue Lage anpasst. Ein hartes
// Umschalten waere im Headset als Ruck zu spueren, gerade beim Streifen einer
// Kante; ueber diese Zeitkonstante geglaettet bleibt es ruhig.
constexpr float kLeanCollisionSmoothingSeconds = 0.06F;
// Auch das Enger-Werden wird geglaettet, nur schneller. An einer Kante
// wechselt der Strahl bildweise zwischen Treffer und freier Sicht; wurde die
// Begrenzung dabei sofort zugezogen, sprang der Blickpunkt im selben Takt hin
// und her. Der Blick steht dadurch fuer wenige Hundertstel naeher an der
// Wand, als die Marge vorsieht — dafuer steht das Bild ruhig.
constexpr float kLeanCollisionTightenSeconds = 0.035F;
// Wie lange eine einmal erkannte Enge gehalten wird, bevor sich die
// Begrenzung wieder oeffnen darf. An einer Kante trifft der Strahl bildweise
// mal und mal nicht; ohne diese Haltezeit wechselte die Begrenzung im selben
// Takt und das Bild zitterte. Gehalten wird immer der engste Wert des
// Fensters — lieber kurz zu vorsichtig als flackernd.
constexpr std::uint64_t kLeanCollisionHoldNs = 220000000ULL;
// Groesster Bildabstand, aus dem noch geglaettet wird. Nach einem
// Ladebildschirm gilt der neue Wert sofort.
constexpr std::uint64_t kLeanCollisionMaxGapNs = 200000000ULL;

struct LeanCollisionState {
    float scale{1.0F};
    // Engster Zielwert des laufenden Haltefensters und dessen Ablauf.
    float heldTarget{1.0F};
    std::uint64_t holdUntilNs{0};
    std::uint64_t lastUpdateNs{0};
    bool haveUpdate{false};
};

inline void ResetLeanCollision(LeanCollisionState& state) noexcept {
    state = LeanCollisionState{};
}

// Anteil des gewuenschten Versatzes, der frei bleibt.
//
// `desiredUnits` ist die Laenge des gewuenschten Kopfversatzes,
// `freeUnits` die Strecke bis zur ersten Wand entlang derselben Richtung.
// Ein negativer oder unendlicher Messwert bedeutet „nichts getroffen".
inline float LeanCollisionScale(
    float desiredUnits, float freeUnits, bool hit) noexcept {
    if (!std::isfinite(desiredUnits) || desiredUnits <= 0.0F) {
        return 1.0F;
    }
    if (!hit || !std::isfinite(freeUnits)) {
        return 1.0F;
    }
    const float allowed = freeUnits - kLeanCollisionMarginUnits;
    if (allowed <= 0.0F) {
        return 0.0F;
    }
    if (allowed >= desiredUnits) {
        return 1.0F;
    }
    return allowed / desiredUnits;
}

// Denselben Wert geglaettet fortschreiben.
inline float UpdateLeanCollision(
    LeanCollisionState& state, float targetScale,
    std::uint64_t nowNs) noexcept {
    if (!std::isfinite(targetScale)) {
        return state.scale;
    }
    if (targetScale < 0.0F) {
        targetScale = 0.0F;
    } else if (targetScale > 1.0F) {
        targetScale = 1.0F;
    }

    float seconds = 0.0F;
    if (state.haveUpdate && nowNs > state.lastUpdateNs &&
        nowNs - state.lastUpdateNs <= kLeanCollisionMaxGapNs) {
        seconds = static_cast<float>(nowNs - state.lastUpdateNs) * 1.0e-9F;
    }
    state.lastUpdateNs = nowNs;
    state.haveUpdate = true;

    // Haltefenster: Eine erkannte Enge gilt weiter, auch wenn der Strahl im
    // naechsten Bild daneben trifft.
    if (targetScale <= state.heldTarget || nowNs >= state.holdUntilNs) {
        if (targetScale <= state.heldTarget) {
            state.holdUntilNs = nowNs + kLeanCollisionHoldNs;
        }
        state.heldTarget = targetScale;
    }
    targetScale = state.heldTarget;

    if (seconds <= 0.0F) {
        state.scale = targetScale;
        return state.scale;
    }
    float weight = seconds / kLeanCollisionSmoothingSeconds;
    if (weight > 1.0F) {
        weight = 1.0F;
    }
    if (targetScale < state.scale) {
        float tighten = seconds / kLeanCollisionTightenSeconds;
        if (tighten > 1.0F) {
            tighten = 1.0F;
        }
        state.scale += (targetScale - state.scale) * tighten;
    } else {
        state.scale += (targetScale - state.scale) * weight;
    }
    return state.scale;
}

} // namespace fearvr
