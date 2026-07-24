# COORDINATE-SYSTEM.md — Koordinaten, Projektion, Kamera

> **Nicht raten.** OpenXR und LithTech benutzen voraussichtlich
> unterschiedliche Händigkeit und Vorwärtsachsen. Jede Abbildung wird belegt,
> zentral definiert und mit Unit-Tests abgesichert (ANWEISUNG.md §7).

## 1. Konventionen

| System | Händigkeit | Vorwärts | Oben | Rechts | Einheit | Beleg |
|---|---|---|---|---|---|---|
| OpenXR (`XrSpace`, view space) | rechtshändig | −Z | +Y | +X | Meter | OpenXR-Spezifikation |
| LithTech Jupiter EX (Welt/Kamera) | linkshändig | +Z | +Y | +X | 100 Game-Units/Meter | `ltrotation.h`, `ltvector.h`, M3-IPD-Livetest |

`LTRotation::Right/Up/Forward` liefert für die Identität +X/+Y/+Z.
`LTVector::Cross` berechnet die Operandenreihenfolge gegenüber dem üblichen
rechtshändigen Kreuzprodukt umgekehrt. Die zentrale Abbildung lautet daher:

```text
Position OpenXR → LithTech:   ( x,  y, -z)
Quaternion OpenXR → LithTech: (-x, -y,  z, w)
```

Der Quaternionvektor erhält bei der Z-Spiegelung zusätzlich das Vorzeichen
eines axialen Vektors. Die Abbildung ist in `head_tracking_math.h` zentral
implementiert und wird in x86 und x64 getestet.

## 2. Zentrale, konfigurierbare Konversion

- Genau **eine** Stelle im Code definiert `OpenXR → LithTech` (Position,
  Rotation): `src/common/head_tracking_math.h`.
- Die Skalierung liegt zentral in `src/common/stereo_math.h`.
- Tests: `tests/test_head_tracking_math.cpp` und
  `tests/test_stereo_math.cpp`.

## 3. Vorgehen (§7)

1. **Recenter:** beim Aktivieren aktuelle HMD-Pose als neutralen lokalen
   Ursprung speichern; F9 erhöht `recenterGeneration` und setzt sie neu.
2. **Relative Headpose** berechnen — nicht die absolute Tracking-Space-Pose
   direkt in die Welt schreiben.
3. **Körper-Yaw** und **Head-Yaw** getrennt halten.
4. **IPD** aus den beiden `xrLocateViews`-Posen übernehmen, **nicht** fest auf
   64 mm setzen.
5. **Translation** standardmäßig deaktiviert; opt-in lokal auf 25 cm vom
   Recenter-Punkt begrenzt. Eine echte Kollisionsabfrage gegen Wände bleibt
   vor einer allgemeinen Aktivierung erforderlich.
6. **Roll** der Spielkamera nur aus dem HMD, nicht aus Weapon Sway/Camera Shake.

## 4. Projektion / FOV

- Falls LithTech nur **symmetrisches** FOV akzeptiert: im ersten Stereo-MVP
  konservatives symmetrisches FOV verwenden und **genau dieses** FOV in
  `XrCompositionLayerProjectionView` einreichen (reduzierte Abdeckung
  dokumentieren).
- Später: gezielter Projektionsmatrix-Hook für asymmetrische OpenXR-Frusta.
- Near/Far-Clipping, Waffenmodell und Partikel **pro Auge** prüfen; Near Plane
  nicht zu groß wählen (Waffe/Hände dürfen nicht verschwinden).

## 5. Verifikation (Debug)

- Debugmodus mit farbigen Achsen bzw. eindeutigem
  „links/rechts/oben/vorne"-Test.
- Unit-Tests: Achsenabbildung, Quaternion-Konvertierung, Pose-Komposition,
  Pose relativ zum Recenter-Ursprung, FOV-Winkel → Projektionsmatrix
  (siehe `docs/TESTING.md`).

## 6. Gate (M4)

Kopf links/rechts/oben/unten bewegt die Ansicht in der **erwarteten** Richtung;
kein künstliches Rollen beim normalen Laufen; Trackingverlust ohne Kamerasprung.

**Gate abgeschlossen.** Live am 24.07.2026 bestätigt:

- alle Rotationsachsen bewegen sich in die richtige Richtung;
- F9 setzt erfolgreich die Neutralpose;
- das erste Bild lag zwei OpenXR-Frames hinter der Renderanforderung;
- Zuordnung der tatsächlich gerenderten Pose zum Bild erlaubt dem OpenXR-
  Compositor korrektes Timewarp; Benutzerbewertung danach „deutlich besser“.
- opt-in Translation bewegt sich seitlich und vor/zurück korrekt, kehrt sauber
  zum Recenter-Punkt zurück und blieb im begrenzten Live-Test stabil.

Der Host veröffentlicht nur gültige, aktuelle Posen. Bei fehlender gültiger
Pose bleibt die letzte Spielkamera unverändert; es wird keine ungültige
Transformation eingespeist. Ein physisch erzwungener vollständiger
Trackingverlust wurde im M4-Abnahmelauf nicht separat provoziert und bleibt als
gezielte Hardware-Regression dokumentiert.
