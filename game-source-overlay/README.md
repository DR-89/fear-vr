# game-source-overlay/

Enthält **ausschließlich neu geschriebene** Projektdateien für das lokal
gebaute GameClient-Modul (ANWEISUNG.md §5.3, §10).

**Nicht** hierher gehören:

- offizielle Public-Tools-/SDK-Basisquellen (liegen lokal in `vendor-local/`,
  nicht in Git);
- Retail-Dateien oder extrahierte Assets.

Vorgehen (M0/M3):

1. Public Tools nach `vendor-local/` installieren (nur lokal).
2. Unveränderten Client als **Stock-Referenz** als x86 bauen (v141 falls nötig).
3. Renderpfad untersuchen (`docs/STEREO-RESEARCH.md`).
4. Erst danach hier neue Dateien/Overlays hinzufügen, die den vorhandenen
   Welt-Renderpfad zweimal pro Frame aufrufen — ohne zweite Simulation.

Lizenzgrenzen der offiziellen Bestandteile beachten
(`THIRD_PARTY_NOTICES.md`). Noch leer (M0).
