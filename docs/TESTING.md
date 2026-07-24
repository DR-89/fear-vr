# TESTING.md — Test- und Verifikationsplan

Grundlage: ANWEISUNG.md §14 (Tests) und die Gates aus §13.

## 1. Automatisierte Tests

Als CMake/CTest-Ziele unter `tests/` (baubar ohne Headset):

- [x] Protokollgrößen und -Offsets in **x86 und x64** (`static_assert` +
      Laufzeit-Roundtrip)
- [x] Ablehnung ungültiger Magic / Version / Größe
- [ ] Quaternion-Normalisierung und Achsenabbildung
- [ ] Pose relativ zum Recenter-Ursprung
- [ ] FOV-Winkel → Projektionsmatrizen
- [x] Ringpuffer-Paarung und Generationen
- [x] OpenXR-State-Machine als testbare Logik **ohne** Headset
- [ ] EXE-Hashprüfung
- [x] Stage-Pfad bleibt unter der Projektwurzel
- [x] Retail-Hash vor/nach M2-Vorbereitung und Startversuch

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

Manuell bestanden:

- Benutzerbestätigung: linkes Auge rot, rechtes Auge blau;
- Absetzen und Wiederaufsetzen: `FOCUSED → VISIBLE → FOCUSED`, Testbild danach
  wieder sichtbar;
- Steam-Link-Unterbrechung und Wiederverbindung: Headset/Controller wurden
  reaktiviert, der Host renderte ohne Absturz weiter;
- längerer Testlauf mit rund 20.400 Frames, danach `host_stop`.

Noch offen:

- einen echten `XR_SESSION_LOSS_PENDING`-Wechsel provozieren und die
  automatische Session-Neuerstellung zusätzlich zum Unit-Test live
  bestätigen. Steam Link hielt beim Verbindungsabbruch dieselbe OpenXR-Session
  am Leben; der normale SteamVR-`-shutdown`-Befehl wurde bei aktiver Anwendung
  nicht ausgeführt.

## 7. M2-Live-Tests vom 2026-07-24

Automatisiert bestanden:

- x86-D3D9-Producer und x64-D3D11/OpenXR-Host über benannte IPC-Objekte;
- exakte NVIDIA-Adapter-LUID `0x0:C91C` auf beiden Seiten;
- GPU-direkte D3D9Ex-Shared-Textures mit drei Slots je Auge, ohne
  CPU-Readback;
- klassischer D3D9-Kompatibilitätstest über den explizit markierten
  CPU-D3D9Ex-Pfad;
- frische Paare für Frame/Generation 1 und 300;
- Minimieren/Wiederherstellen sowie D3D9-Reset von 960×540 auf 800×450;
- normaler Producer-/Host-Abschluss;
- erzwungener Host-Abbruch: Producer lief fail-open bis Frame 600 weiter;
- separater Test für IAT-Hook und späte `Present`-/`Reset`-Detours an einem
  bereits erzeugten realen D3D9-Gerät.

Zusätzlich geprüft:

- Auslaufen des Game-Heartbeats beendet den Host kontrolliert auch aus
  `XR_SESSION_STATE_SYNCHRONIZED`;
- der Host erkennt das echte Spielende anhand des Prozesshandles und beendet
  sich nicht fälschlich während Pause, Ladezustand oder Fokusverlust;
- x86/x64-`RelWithDebInfo`, Protokoll-, Session-State- und Hook-Tests
  bestehen.

Realer F.E.A.R.-Lauf bestanden:

- `tools\launch-m2-fear.ps1` verifizierte `GameClient.dll`, `GameOrig.dll`
  und `fearvr-d3d9.dll` im laufenden x86-Prozess;
- späte Hooks und Adapter-LUID-Abgleich bestanden;
- 1024×768-Spielbilder wurden laufend vom Host importiert;
- Benutzerbestätigung: F.E.A.R.-Menü in beiden Augen sichtbar, Maus und
  Tastatur reagieren normal;
- nach Schließen des SteamVR-Desktop-Overlays blieb das Spielbild sichtbar.

Relevante Logs:

```text
logs\m2-20260724-121708
logs\m2-20260724-121736
logs\m2-20260724-131526
logs\m2-20260724-133323
logs\m2-fear-20260724-133432
```

Bewertung des M2-Gates:

- Ringpuffer, frische Frames, Reset und Ausfallverhalten sind nachgewiesen.
- Der GPU-direkte D3D9Ex-Test erfüllt „kein per-Frame-CPU-Readback“.
- Das echte klassische D3D9-Spiel benötigt derzeit den Diagnoseflag
  `FEARVR_BF_CPU_FALLBACK` und erfüllt diese Produktionsinvariante noch
  **nicht**.
- Alt-Tab und ein echter Auflösungswechsel im realen Spiel bleiben als
  längerer Regressionstest offen; die entsprechenden synthetischen Tests
  bestehen.
- M2 wird als funktionaler Monobrücken-Techniknachweis angenommen, nicht als
  spielbarer oder komfortabler VR-Stand.

## 8. M3-Live-Test vom 2026-07-24

Automatisiert bestanden:

- x86- und x64-`RelWithDebInfo` mit `/W4 /WX`;
- Protokoll-, OpenXR-Session-State- und Stereo-Math-Tests, jeweils 3/3;
- isolierter Stereo-Transport mit getrennten Augenbildern;
- symmetrisches FOV und IPD-Grenzfälle im Unit-Test;
- Kamera-Restore und SEH-geschützter Mono-Rückfallpfad.

Realer F.E.A.R.-Lauf `logs\m3-fear-20260724-162315`:

- Retail-PlayerCamera-Alias Slot 17 wurde nach Laufzeit-Signaturprüfung
  aktiviert und über den originalen Slot 19 zweimal gerendert;
- rund 11½ Minuten Laufzeit und mindestens 24.900 vollständige Stereo-Frames
  ohne `stereo_render_exception`;
- mehrere echte Auflösungs-/Device-Resets wurden überlebt;
- F8 deaktiviert und reaktiviert den Hook ohne Absturz;
- Benutzerbestätigung: Ego-Steuerung funktioniert und die Welt erscheint
  korrekt in 3D; der weitere Spieltest lief fehlerfrei;
- Spiel und Host beendeten sich kontrolliert.

Finaler Smoke-Test `logs\m3-fear-20260724-163558`:

- bereinigte, bytegleich ins M3-Stage kopierte `GameClient.dll`;
- Retail-Laufzeitsignatur akzeptiert, kein `stereo_hook_layout_mismatch`;
- mehrfaches F8 installierte und entfernte Slot 17 jeweils kontrolliert;
- temporäre VTable-Byteausgabe ist entfernt;
- Spiel beendet, Host durch Game-Heartbeat beendet und OpenXR-Zustände
  `STOPPING → IDLE → EXITING` sauber durchlaufen.

In M4 geschlossene M3-Grenze:

- HUD und Menüs werden nach dem Welt-Render gezeichnet. M4 übernimmt die
  HUD-Differenz identisch in beide Augen und zeigt Menüs beziehungsweise große
  Vollbildänderungen als raumfestes, lesbares OpenXR-Panel.

Verbleibende Detailregressionen:

- Lean und Slow-Mo getrennt protokollieren;
- physisch vollständigen Trackingverlust in einem gezielten Hardwaretest
  provozieren.

Das 15-Minuten-Stabilitätsgate wurde vom Benutzer am 24.07.2026 auf Basis des
fehlerfreien 11½-Minuten-Laufs mit mindestens 24.900 Stereo-Frames als
bestanden akzeptiert.

## 9. M4-Abnahme vom 2026-07-24

Automatisiert bestanden:

- `head_tracking_math` in x86 und x64;
- OpenXR→LithTech-Achsen- und Quaternionabbildung;
- neutrale Recenter-Pose, IPD nach Recenter und ungültige Pose;
- Translation standardmäßig aus und bei opt-in auf 25 cm begrenzt;
- insgesamt je 5/5 CTest-Tests in x86 und x64, einschließlich
  `stereo_hud_math`.

Live-Läufe:

- `logs\m4-fear-20260724-164546`: Links/Rechts, Hoch/Runter und Rollrichtung
  korrekt; F9-Recenter erfolgreich; Tracking subjektiv noch leicht träge.
- `logs\m4-fear-20260724-165149`: Bildpose über `frameId` zugeordnet,
  gemessener Abstand zwei OpenXR-Frames; mit korrekter Timewarp-Pose ist das
  Tracking laut Benutzer „deutlich besser“, F9 funktioniert weiterhin.
- `logs\m4-fear-20260724-165538`: opt-in Translation aktiv, Bildpose nur einen
  Frame alt; seitliche und Vor-/Rückbewegung sowie Rückkehr zur Neutralposition
  laut Benutzer korrekt und stabil.
- `logs\m4-fear-20260724-170417`: Menübild als 2,4 × 1,8 m großer
  OpenXR-Quad-Layer zwei Meter vor dem Benutzer verankert; Hauptmenü lesbar und
  bleibt bei Kopfbewegung im Raum stehen.
- `logs\m4-fear-20260724-170754`: wiederholte Übergänge zwischen festem
  Menüpanel und Stereo-Spielansicht laut Benutzer funktionsfähig.
- `logs\m4-fear-20260724-172137`: normales HUD als 1-%-Delta in beide Augen
  übernommen; ESC-Menü mit 82-%-Delta automatisch als festes Panel gezeigt.
- `logs\m4-fear-20260724-173150`: angehobene und horizontal eingerückte
  HUD-Anordnung laut Benutzer gut. SteamVR blendete trotz
  `autoShowGameTheater=false` verzögert sein Desktop-Theater ein.
- `logs\m4-fear-20260724-173706`: automatischer Theater-Wächter aktiv; er
  identifizierte die tatsächlich sichtbare Fläche als
  `valve.steam.desktopgame.21090`.
- `logs\m4-fear-20260724-174632`: korrigierter Wächter erkannte die verzögert
  erzeugte F.E.A.R.-Theaterfläche nach rund 3,5 Sekunden, führte
  `disable_theater_mode` und `hidedashboard` aus und protokollierte
  `delayed_theater_hidden`. Der Benutzer bestätigte anschließend „top, passt“.

M4-Gate:

- Kopfbewegungen links/rechts/oben/unten und Rollrichtung korrekt;
- F9-Recenter und bildsynchrone Timewarp-Pose bestätigt;
- kein künstliches Rollen beim normalen Lauf gemeldet;
- bei mehr als 250 ms ohne neue Pose Wechsel auf Mono und Recenter bei
  Wiederkehr; ungültige Posen sind automatisiert getestet;
- Haupt-/Pausemenü, Stereo-HUD, Übergänge und Theater-Unterdrückung bestätigt;
- Benutzerabnahme: M4 darf abgeschlossen werden.

Komfortfunktionen:

- `-NoHeadBob` setzt die offiziellen LithTech-HeadBob-Variablen für Kamera und
  Waffe auf null;
- F10 erzwingt einen raumfesten Komfortbildschirm und zentriert beim Rückweg
  neu;
- Zustände ohne vollständiges Stereo-Weltbild wechseln automatisch auf das
  raumfeste Panel.

Bekannte Grenzen nach M4:

- Translation vor allgemeiner Aktivierung mit Weltkollision absichern;
- Trackingverlust wurde im M4-Retailpfad nicht noch einmal physisch provoziert;
- der Stereo-HUD-Mischer benötigt im klassischen D3D9-Kompatibilitätspfad ein
  zusätzliches CPU-Readback und muss vor dem finalen M6-Pfad GPU-seitig oder
  als nativer UI-Layer ersetzt werden.
