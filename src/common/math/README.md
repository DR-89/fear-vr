# src/common/math/

Zentrale, konfigurierbare Mathe- und Konversionsroutinen (ANWEISUNG.md §7),
gemeinsam von Host (x64) und GameClient/Proxy (x86) genutzt.

Geplant (ab M3/M4), jeweils mit Unit-Tests in `tests/`:

- Achsenabbildung OpenXR ↔ LithTech (eine einzige Definitionsstelle);
- Quaternion-Normalisierung und -Konvertierung;
- Pose-Komposition und Pose relativ zum Recenter-Ursprung;
- Trennung von Körper-Yaw und Head-Yaw;
- IPD aus den beiden `xrLocateViews`-Posen;
- FOV-Winkel → Projektionsmatrix (symmetrisch, später asymmetrisch);
- begrenzte lokale Translation.

Header-only (DirectXMath aus dem Windows SDK). Noch leer (M0).
