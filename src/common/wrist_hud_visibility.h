#pragma once

#include <cmath>
#include <cstdint>

namespace fearvr {

constexpr std::uint64_t kDualPistolWristDwellMilliseconds = 90;

// Das Display ist eine bewusst ausgefuehrte "Uhr ablesen"-Geste: Man muss
// wirklich zum Handgelenk schauen. Der weitere Haltebereich verhindert
// Flackern am Rand, ohne das HUD beim Wegschauen festzuhalten.
inline bool IsLookingAtWrist(
    float gazeAlignment, bool alreadyVisible = false) noexcept {
    if (!(gazeAlignment >= -1.0F && gazeAlignment <= 1.0F)) {
        return false;
    }
    const float minimumGazeAlignment =
        alreadyVisible ? 0.84F : 0.88F;
    return gazeAlignment >= minimumGazeAlignment;
}

// Gleichzeitig muss die Uhr-/Handgelenkflaeche deutlich zum Gesicht zeigen.
// Die alte 0.45-Grenze akzeptierte fast seitlich gehaltene Haende; 0.72
// verlangt eine echte Ablesepose. Nur fuer ein bereits sichtbares HUD ist die
// Grenze etwas weiter, damit normales Trackingrauschen nicht flackert.
inline bool IsWristBackTilted(
    float surfaceFacing, bool alreadyVisible = false) noexcept {
    if (!(surfaceFacing >= 0.0F && surfaceFacing <= 1.0F)) {
        return false;
    }
    const float minimumSurfaceFacing =
        alreadyVisible ? 0.58F : 0.72F;
    return surfaceFacing >= minimumSurfaceFacing;
}

// Mit einer zweiten Pistole reicht "Hand im Blick" nicht als HUD-Geste: Das
// passiert beim Zielen aus Schulter und Huefte. Entscheidend ist stattdessen
// die Absicht: Die Pistole muss deutlich aus der Blick-/Schussrichtung
// herausgedreht sein, die Wrist-Flaeche muss zum Gesicht zeigen und der linke
// Abzug darf nicht feuern. Fuer ein bereits sichtbares HUD gelten leicht
// weitere Grenzen, damit Trackingrauschen nicht flackert; zur Schussrichtung
// zurueckdrehen oder feuern blendet es trotzdem sofort aus.
inline bool IsDualPistolWristReadingPose(
    float weaponHeadAlignment,
    float surfaceFacing,
    float supportTrigger,
    bool alreadyVisible = false) noexcept {
    if (supportTrigger >= 0.55F) {
        return false;
    }
    const float maximumAimAlignment =
        alreadyVisible ? 0.60F : 0.50F;
    return std::fabs(weaponHeadAlignment) <= maximumAimAlignment &&
           IsWristBackTilted(surfaceFacing, alreadyVisible);
}

inline bool UpdateDualPistolWristDwell(
    bool candidate,
    std::uint64_t nowMilliseconds,
    std::uint64_t& candidateSinceMilliseconds) noexcept {
    if (!candidate) {
        candidateSinceMilliseconds = 0;
        return false;
    }
    if (candidateSinceMilliseconds == 0) {
        candidateSinceMilliseconds = nowMilliseconds;
        return false;
    }
    return nowMilliseconds >= candidateSinceMilliseconds &&
           nowMilliseconds - candidateSinceMilliseconds >=
               kDualPistolWristDwellMilliseconds;
}

} // namespace fearvr
