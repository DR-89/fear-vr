# COORDINATE-SYSTEM.md — Koordinaten, Projektion, Kamera

> **Nicht raten.** OpenXR und LithTech benutzen voraussichtlich
> unterschiedliche Händigkeit und Vorwärtsachsen. Jede Abbildung wird belegt,
> zentral definiert und mit Unit-Tests abgesichert (ANWEISUNG.md §7).

## 1. Konventionen (zu belegen)

| System | Händigkeit | Vorwärts | Oben | Rechts | Einheit | Beleg |
|---|---|---|---|---|---|---|
| OpenXR (`XrSpace`, view space) | rechtshändig | −Z | +Y | +X | Meter | OpenXR-Spec (zu zitieren) |
| LithTech Jupiter EX (Welt/Kamera) | _TBD_ | _TBD_ | _TBD_ | _TBD_ | _TBD_ (Game-Units) | Public-Tools-Quellen (zu prüfen) |

→ Die tatsächliche LithTech-Konvention wird aus den offiziellen
Public-Tools-Quellen abgeleitet (`RenderCamera`, Kamera-/Rotationstypen),
**nicht** vermutet.

## 2. Zentrale, konfigurierbare Konversion

- Genau **eine** Stelle im Code definiert `OpenXR → LithTech` (Position,
  Rotation) und die Skalierung Meter ↔ Game-Units.
- Implementierung: `src/common/math/` (+ Tests in `tests/`).

## 3. Vorgehen (§7)

1. **Recenter:** aktuelle HMD-Pose als neutralen lokalen Ursprung speichern.
2. **Relative Headpose** berechnen — nicht die absolute Tracking-Space-Pose
   direkt in die Welt schreiben.
3. **Körper-Yaw** und **Head-Yaw** getrennt halten.
4. **IPD** aus den beiden `xrLocateViews`-Posen übernehmen, **nicht** fest auf
   64 mm setzen.
5. **Translation** zunächst deaktivierbar; danach lokal auf einen komfortablen
   Bereich begrenzen und gegen Wanddurchdringung absichern.
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
