# TESTING.md — Test- und Verifikationsplan

Grundlage: ANWEISUNG.md §14 (Tests) und die Gates aus §13.

## 1. Automatisierte Tests

Als CMake/CTest-Ziele unter `tests/` (baubar ohne Headset):

- [x] Protokollgrößen und -Offsets in **x86 und x64** (`static_assert` +
      Laufzeit-Roundtrip)
- [ ] Ablehnung ungültiger Magic / Version / Größe
- [ ] Quaternion-Normalisierung und Achsenabbildung
- [ ] Pose relativ zum Recenter-Ursprung
- [ ] FOV-Winkel → Projektionsmatrizen
- [ ] Ringpuffer-Generationen und Timeout-Pfade
- [x] OpenXR-State-Machine als testbare Logik **ohne** Headset
- [ ] EXE-Hashprüfung
- [ ] Stage-Pfad bleibt unter der Projektwurzel
- [ ] Retail-Hash vor/nach Vorbereitung und Live-Test

## 2. Live-Testmatrix

| Achse | Varianten |
|---|---|
| SteamVR | aus / an |
| Headset | aktiv / Standby / Trackingverlust |
| Startreihenfolge | Host vor Spiel / Host nach Spiel |
| Host-Abbruch | während des Spiels beendet |
| Fenster | Fenstermodus / Vollbild |
| Alt-Tab | dreimal |
| Auflösung | Wechsel / D3D9 Reset |
| Spielzustände | Hauptmenü, neues Spiel, Save/Load, Tod/Respawn |
| Effekte | Slow-Mo, viele Partikel |
| Szenen | Leiter, Lean, Knockdown, erste Zwischensequenz |
| Dauer | mind. ein 15-Minuten-Lauf |
| Build | Debug **und** RelWithDebInfo |

## 3. Pro Live-Test zu erfassen (§14)

- Host-/Proxy-/GameClient-Version
- EXE-Hash
- GPU und Adapter-LUIDs
- OpenXR-Runtime-Name/-Version
- Swapchainformat und -größe
- Game-FPS und XR-Displayrate
- dropped / reused frames
- Renderzeit links/rechts und Host-Copyzeit
- Ressourcen-/Handle-Anzahl am Anfang und Ende

Logs: `logs/host-YYYYMMDD-HHMMSS.log` (Host) und entsprechende Proxy-Logs.

## 4. Gate-Checklisten (§13)

- **M0:** keine Retail-Datei geändert; Stock-Quellbuild verhält sich in Menü und
  erstem Abschnitt wie Retail.
- **M1:** Headset zeigt beide Augen; Standby/Session-Neustart ohne Crash; ohne
  Runtime verständliche Diagnose.
- **M2:** kein per-Frame-CPU-Readback; Alt-Tab/Auflösung/Reset/Host-Abbruch/
  Spielende hängen nicht; Framefarben/-zähler beweisen frische Slots.
- **M3:** korrekte Parallaxe nah/fern; L/R nicht vertauscht; keine doppelte
  Simulationsgeschwindigkeit; Save/Load/Slow-Mo/Partikel/KI/Audio zeitlich
  korrekt; 15 min ohne Deadlock/Leck.
- **M4:** Blickrichtung korrekt; kein künstliches Rollen; Trackingverlust ohne
  Kamerasprung.
- **M5:** kein Stuck Input nach Fokusverlust; Controller trenn-/verbindbar; keine
  „6DoF-Waffe"-Behauptung ohne Richtungsnachweis.
- **M6:** Deinstallation entfernt nur Mod-Dateien; Retail unverändert; frische
  Stage allein aus Repo + legaler Kopie + Public Tools + Abhängigkeiten erzeugbar.

## 5. Ausführung

```bash
# Automatisierte Tests (sobald Toolchain + Build vorhanden)
ctest --test-dir build/x64 -C RelWithDebInfo --output-on-failure
ctest --test-dir build/x86 -C RelWithDebInfo --output-on-failure

# Umgebungs-/Retail-Integritätsprüfung (jederzeit, read-only)
pwsh -File tools/verify-install.ps1
```

## 6. M1-Live-Test vom 2026-07-24

Bestanden:

- fehlende Runtime künstlich über `XR_RUNTIME_JSON` geprüft: klare Diagnose
  und Exitcode `10`;
- `--validate-only`: SteamVR/OpenXR 2.16.7, Quest 3, exakte Adapter-LUID und
  zwei `1624x1736`-Swapchains erfolgreich;
- `--max-frames 120`: 120 echte Stereo-Frames, links rot/rechts blau, danach
  sauberer STOPPING-/EXITING-Lebenszyklus;
- x64 und x86 mit `/W4 /WX`; je zwei CTest-Tests bestanden.

Noch manuell offen:

- die visuelle Zuordnung „linkes Auge rot / rechtes Auge blau“ im Headset
  ausdrücklich bestätigen;
- Headset während eines längeren Hostlaufs gezielt in Standby versetzen und
  wieder aufwecken;
- einen echten Runtime-/Session-Verlust provozieren und die automatische
  Session-Neuerstellung zusätzlich zum Unit-Test live bestätigen.
