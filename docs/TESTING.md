# TESTING.md — Test- und Verifikationsplan

Grundlage: ANWEISUNG.md §14 (Tests) und die Gates aus §13.

## 1. Automatisierte Tests

Als CMake/CTest-Ziele unter `tests/` (baubar ohne Headset):

- [x] Protokollgrößen und -Offsets in **x86 und x64** (`static_assert` +
      Laufzeit-Roundtrip)
- [x] Ablehnung ungültiger Magic / Version / Größe
- [x] Quaternion-Normalisierung und Achsenabbildung
      (`head_tracking_math`: OpenXR→LithTech für Yaw und Pitch;
      `input_state`: unnormierte und degenerierte Quaternionen)
- [x] Pose relativ zum Recenter-Ursprung und Yaw-only-Recenter bei geneigtem
      beziehungsweise gesenktem Kopf (`head_tracking_math`)
- [x] bildsynchrone Spielkamera-Bewegungsmessung einschließlich
      Achsenrichtung, Basis-Yaw sowie Fail-closed-Fällen für Head-Bob,
      Teleport, alte Samples und gleichzeitige Drehung
      (`locomotion_reprojection`); die berechnete Layer-Translation bleibt
      ohne Tiefenpuffer bewusst deaktiviert
- [x] FOV-Winkel → gemeinsame symmetrische Projektion (`stereo_math`);
      echte Projektionsmatrizen baut LithTech selbst aus dem gesetzten FOV
- [x] Ringpuffer-Paarung und Generationen
- [x] OpenXR-State-Machine als testbare Logik **ohne** Headset
- [x] EXE-Hashprüfung — `tools/verify-install.ps1`, read-only, kein CTest-Ziel
- [x] Stage-Pfad bleibt unter der Projektwurzel
- [x] Retail-Hash vor/nach M2-Vorbereitung und Startversuch
- [x] semantische Controllerabbildung inklusive Lehnen
      (`input_state`, `controller_mapping`)

## 2. Live-Testmatrix

Stand 25.07.2026. „synthetisch" heißt: über `tools\test-m2-bridge.ps1` oder
einen Unit-Test nachgewiesen, nicht im laufenden Retail-Spiel.

| Achse | Varianten | Stand |
|---|---|---|
| SteamVR | aus / an | **erfüllt** — ohne Runtime klare Diagnose, Exitcode 10 (§6) |
| Headset | aktiv / Standby / Trackingverlust | **erfüllt** — Standby §6, vollständiger Controller-Trackingverlust §10 |
| Startreihenfolge | Host vor Spiel / Host nach Spiel | **erfüllt** — der Launcher startet den Host zuerst; späte Hooks §7 |
| Host-Abbruch | während des Spiels beendet | **synthetisch** — Producer lief fail-open weiter (§7) |
| Fenster | Fenstermodus / Vollbild | **offen** im Retail-Spiel |
| Alt-Tab | dreimal | **offen** im Retail-Spiel |
| Auflösung | Wechsel / D3D9 Reset | **teilweise** — echte Device-Resets in §8 überlebt, gezielter Auflösungswechsel offen |
| Spielzustände | Hauptmenü, neues Spiel, Save/Load, Tod/Respawn | **teilweise** — Menü, neues Spiel und Ladezustände bestätigt; Save/Load und Tod/Respawn nicht gezielt geprüft |
| Effekte | Slow-Mo, viele Partikel | **erfüllt** — Slow-Mo protokolliert (§13), Partikel im Spielverlauf |
| Szenen | Leiter, Lean, Knockdown, erste Zwischensequenz | **teilweise** — Lean bestätigt (§13), Zwischensequenz über Komfortbildschirm (§9); Leiter und Knockdown offen |
| Dauer | mind. ein 15-Minuten-Lauf | **akzeptiert** — 11½ min mit ≥24.900 Stereo-Frames, vom Benutzer als bestanden gewertet (§8) |
| Build | Debug **und** RelWithDebInfo | **teilweise** — alle Läufe in RelWithDebInfo; Debug nicht live geprüft |

Die offenen Punkte sind allesamt Regressionstests am laufenden Spiel und
brauchen Headset und Benutzer; sie blockieren keinen Codepfad.

## 3. Pro Live-Test zu erfassen (§14)

Alle Kennzahlen stehen in den JSON-Logs eines Laufs und werden mit einem
Aufruf zusammengefasst:

```powershell
pwsh -File tools\collect-perf-report.ps1            # jüngster Lauf
pwsh -File tools\collect-perf-report.ps1 -Run m5-fear-20260725-000655
pwsh -File tools\collect-perf-report.ps1 -AsJson
```

| Kennzahl | Quelle |
|---|---|
| Host-/Proxy-/GameClient-Version | `host_start`, `proxy_start` (mit Git-Hash) |
| EXE-Hash | `stage\<milestone>-deployment.json`, Feld `runtimeSha256` |
| GPU und Adapter-LUIDs | `d3d11_adapter` |
| OpenXR-Runtime-Name/-Version | `runtime` |
| Swapchainformat und -größe | `swapchains`, dazu `shared_resources` im Proxy |
| Game-FPS und XR-Displayrate | `perf_frame`: `game_fps`, `xr_fps` |
| dropped frames | `ring_full`: `dropped` (Proxy, kumuliert) |
| reused frames | `perf_frame`: `reused` |
| Renderzeit links/rechts | `perf_frame`: `render_left_*`, `render_right_*` |
| Host-Copyzeit | `perf_frame`: `copy_avg_us`, `copy_max_us` |
| Handle-Anzahl Anfang/Ende | `host_start` und `host_stop`, Feld `handles` |

`perf_frame` fasst je 300 eingereichte Frames zu einer Zeile zusammen und
setzt die Zähler danach zurück. Ein Frame gilt als *reused*, wenn seit der
letzten Einreichung kein neues Spielbild importiert wurde — das ist regulär,
sobald die XR-Displayrate über der Spiel-FPS liegt.

Der Bericht bildet Game-FPS, Renderzeiten und Copyzeit **nur über Fenster mit
Stereobild**. Menü, Ladebildschirm und Komfortmodus rendern lediglich das
linke Auge als Mono-Quad; würde man sie mitteln, sänken die Werte für das
rechte Auge und die Game-FPS künstlich.

Läufe **vor** dem 25.07.2026 enthalten noch keine `perf_frame`-Zeilen; der
Bericht weist das ausdrücklich aus, statt Nullen zu melden.

Logs: `logs/<milestone>-fear-YYYYMMDD-HHMMSS/` mit je einem Host- und
Proxylog.

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

- ~~Lean und Slow-Mo getrennt protokollieren~~ — in M5 geschlossen, siehe §13;
- ~~physisch vollständigen Trackingverlust provozieren~~ — in M5 nachgeholt,
  siehe §10.

Das 15-Minuten-Stabilitätsgate wurde vom Benutzer am 24.07.2026 auf Basis des
fehlerfreien 11½-Minuten-Laufs mit mindestens 24.900 Stereo-Frames als
bestanden akzeptiert.

## 9. M4-Abnahme vom 2026-07-24

Automatisiert bestanden:

- `head_tracking_math` in x86 und x64;
- OpenXR→LithTech-Achsen- und Quaternionabbildung;
- neutrale Recenter-Pose, Yaw-only-Verhalten für Pitch und Roll, IPD nach
  Recenter und ungültige Pose;
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

- Head-Bob ist standardmäßig aus. `HeadBob=1` in `fearvr.ini` stellt nur die
  Kamera-Amplituden wieder her; die Waffen-Amplituden bleiben für stabiles
  Zielen auf null. `-NoHeadBob` erzwingt beide aus;
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

## 10. M5-Diagnosestart vom 2026-07-24

Automatisiert bestanden:

- Protokoll v3 ist in x86 und x64 layoutidentisch;
- `FearVrInputState` und `FearVrHapticRequest` besitzen feste POD-Größen;
- Fokusverlust neutralisiert Sticks, Trigger, Grip und alle Tasten;
- Deadzone, nicht-endliche Werte und Achsenbegrenzung sind getestet;
- x86 und x64 bauen mit `/W4 /WX`, jeweils 6/6 CTest-Tests grün.

Erster Lauf `logs\m5-fear-20260724-181253`:

- alle fünf vorgeschlagenen Interaction Profiles wurden von SteamVR
  akzeptiert;
- Quest-Controllerzustände und rechte Primärtaste erreichten den Host;
- ein erster Client-Hook verwendete irrtümlich Slot 14 und wurde vor jeder
  Spielbelegung verworfen.

Korrigierter Lauf `logs\m5-fear-20260724-181508`:

- beide Quest-Controller aktiv (`active_hands=0x3`);
- `IClientShell.Default` Version 5 gefunden;
- der aus dem öffentlichen Header belegte `Update`-Slot 20 wird aufgerufen;
- Controllerzustände erreichen damit den x86-Retail-Client;
- Fokuswechsel veröffentlichte einen vollständigen neutralen Zustand.

Benutzerabnahme am 24.07.2026:

- Controller-Recenter über rechten Stick-Klick funktioniert;
- der Haptik-Probeimpuls über die rechte Primärtaste funktioniert;
- die übrigen ausgeführten Diagnosepunkte wurden als passend bestätigt;
- ein zunächst ausgelassener vollständiger Trackingverlust wurde anschließend
  im selben laufenden Test nachgeholt: um 20:32:15 meldete der Host
  `active_hands=0x0`, 229 ms später wieder `active_hands=0x3`;
- beide Controller wurden damit ohne Neustart wieder erkannt.

Die semantische Spielbelegung wurde anschließend nachgereicht und vom Benutzer
im Spiel bestätigt: Bewegen, Drehen, Waffenwahl, Springen, Nachladen, Ducken,
Zeitlupe, Rennen, Benutzen, Zielen/Feuern, Recenter und Pausenmenü. Die linke
System-/Menütaste des getesteten Quest-Touch-Controllers ist nicht nutzbar,
weil SteamVR sie für das eigene Systemmenü abfängt. Das gilt nicht für die
Application-Menu-Tasten der Vive Wands.

### SteamVR-native Controllerabdeckung, Implementierungsprüfung 28.07.2026

- Valve Index: Thumbsticks, Trigger, analoge Grips, A/B, beide Stick-Klicks,
  Aim-/Grip-Posen und Haptik besitzen explizite Bindings.
- HTC Vive Wands: Trackpads, Trigger, digitale Grip-Klicks, beide
  Application-Menu-Tasten, Aim-/Grip-Posen und Haptik besitzen explizite
  Bindings. OpenXR konvertiert den digitalen Grip in die gemeinsame
  Float-Squeeze-Action.
- `XR_KHR_generic_controller` wird optional aktiviert und nur vorgeschlagen,
  wenn die Runtime die Erweiterung meldet.
- Das aktive beziehungsweise von SteamVR emulierte Interaction Profile wird
  nach der Action-Synchronisierung und bei Profilwechsel pro Hand geloggt.
- x86 und x64 bauen mit `/W4 /WX`; jeweils 13/13 CTest-Tests sind grün.
- VDXR 1.0.10 akzeptierte die erweiterten Vive- und WMR-Vorschläge jeweils mit
  `result=0`; die Runtime meldete den generischen KHR-Fallback als nicht
  verfügbar. Ein Headset war bei dieser Initialisierungsprüfung nicht
  verbunden.

Ein physischer Index-/Vive-Wand-Lauf steht weiterhin aus. Bis dahin bedeutet
„unterstützt“ eine vollständige Implementierungsabdeckung, nicht dieselbe
Hardwarebestätigung wie bei Quest 3.

## 11. Native VR-Einstellungen im ESC-Menü

Automatisiert und im Retail-Lauf `logs\m5-fear-20260724-222748` bestätigt:

- x86- und x64-Build erfolgreich; jeweils 7/7 CTest-Tests grün;
- neue Bridge-Exporte für Translation, Stereo-HUD, Komfortmodus und Recenter
  vorhanden;
- Byte-Signaturen von `CMenuSystem::Init`, `OnCommand`, `OnFocus`,
  `CBaseMenu::AddControl` und `CLTGUIListCtrl` gegen Retail 1.08 geprüft;
- `vr_settings_menu_hooks_installed` bestätigt alle drei Menü-Hooks;
- `vr_settings_menu_built` bestätigt den Eintrag `VR SETTINGS` direkt
  hinter `Optionen`;
- das Spiel blieb nach Aufbau der erweiterten Menüliste stabil und nahm
  Controllerbefehle weiter an;
- Änderungen werden sofort angewendet und in
  `stage\userdata-m5\fearvr.ini` persistiert.

Die Seite ist bewusst kurz und einseitig, damit kein Eintrag über den Rand des
nativen Rahmens läuft: Stereo rendering, Stereo HUD, Turn speed, Red aim guide,
Controller vibration, Controls, Ladder climbing, Melee, Show arms, Recenter
view, Reset VR defaults, BACK. HMD-Translation, Head-Bob, Komfortbildschirm
und die vier einzelnen Nahkampfaktionen bleiben ohne eigenen Menüeintrag in
`fearvr.ini` einstellbar. Ein zweistufiges Menü wurde verworfen; die
Beschriftungen sind durchgehend englisch. Bedienung: Stick navigiert, A oder
Trigger bestätigt, B geht zurück. Details: `docs/OPENXR-INPUT.md`.

## 12. Arme schaltbar, Hände, Torso und Beine sichtbar

Die Retail-Modellabfrage im Lauf `logs\m5-fear-20260724-231900` ergab:

- Modell `chars\models\player.Model00p` mit **4 Pieces** und 61 Nodes;
- nur zwei Materialien, `player_new.Mat00` und `player_head.Mat00`;
- `GetNumPieces` und `GetPiece(index, …)` liefern `LT_OK`, aber
  `GetPieceName` liefert für alle vier Pieces `LT_NOTFOUND` (61).

Die erste Piece-Kalibrierung war damit zu grob: Piece #1 ist `Body_Group` und
enthält nicht nur Arme, sondern auch Torso und Beine. `HiddenBodyPieces=2`
konnte deshalb Kicks und Körper ebenfalls ausblenden.

Die endgültige Stage-Erzeugung liest stattdessen das lokale Retail-Modell und
die Textur. Eine validierte Mesh-Verzeichnis-Suche findet 4546 Vertices und
6216 Dreiecke. Eine Zusammenhangsanalyse wählt genau sechs Arm-Komponenten
(links/rechts, je drei LODs), rasterisiert deren UV-Dreiecke und setzt nach
einer 2-Pixel-Erweiterung 48.190 DXT3-Alpha-Pixel transparent. Eine getrennte
Hand-/Handgelenkmaske muss überlappungsfrei bleiben; andernfalls bricht das
Stage-Skript ab.

Benutzerabnahme am 27.07.2026:

- `Show arms: OFF` blendet nur Ober- und Unterarme aus;
- Hände, Torso und Beine bleiben sichtbar, einschließlich Kick-Animationen;
- `Show arms: ON` stellt die Retail-Arme sofort wieder her;
- erneutes Ausschalten funktioniert ohne Neustart;
- `ShowArms=0` wurde in `stage\userdata-m5\fearvr.ini` persistiert.

F11 und `HiddenBodyPieces` bleiben reine Entwicklerdiagnosen; der alte Wert
`2` wird beim Laden auf `0` migriert.

## 13. Lehnen über die Neigung der linken Hand

Automatisiert abgedeckt in `tests/test_input_state.cpp` und
`tests/test_controller_mapping.cpp`:

- Rolllage-Extraktion gegen Sollwinkel von -80° bis +80°;
- unnormierte Quaternionen liefern denselben Winkel;
- degenerierte Quaternion ergibt 0;
- Schwelle: ~24° löst aus, ~17° nicht;
- ohne gültige linke Aim-Pose löst sich das Lehnen, statt hängen zu bleiben.

`test_controller_mapping` prüft ausschließlich über `assert` und wurde bis
dahin unter RelWithDebInfo wegen `NDEBUG` wirkungslos übersetzt. Das Ziel baut
jetzt mit `/UNDEBUG`; die Kommandozeilenwarnung `D9025` ist beabsichtigt.

Benutzerabnahme am 25.07.2026: Das Lehnen im Spiel passt.

Lean und Slow-Mo werden getrennt protokolliert. `vr_lean_left_engaged`,
`vr_lean_right_engaged` und `vr_slowmo_engaged` melden die laufende Nummer,
die Lean-Ereignisse zusätzlich die gemessene Rolllage; die zugehörigen
`*_released`-Ereignisse melden die Haltedauer. Damit ist die letzte offene
Detailregression aus der Live-Testmatrix geschlossen.

Der Lauf `logs\m5-fear-20260724-235013` deckte dabei zwei echte Grenzfälle
auf: Rolllagen von 177,3°, 136,3° und −169,3° lösten ein volles Lehnen aus.
Ursache war eine steil nach unten zeigende Aim-Pose, deren Rolllage numerisch
bedeutungslos ist. Obergrenze und Levelness-Prüfung fangen das jetzt ab.

## 14. VR-Menü: Sprünge bei der Auswahl

Benutzermeldung am 25.07.2026: Das Auswählen des nächsten Eintrags war nicht
immer korrekt und sprang.

Ursache im Public-Tools-Quelltext nachgewiesen:
`CLTGUIListCtrl::SetSelection` summiert beim Herunterscrollen rückwärts die
`GetBaseHeight()` aller Controls, ohne `IsVisible()` zu prüfen, während
`CalculatePositions()` unsichtbare Controls überspringt. Da jeder Umschalter
ein verstecktes Geschwister-Control besitzt, wird `m_nFirstShown` falsch
gesetzt.

Nicht die Ursache und deshalb verworfen: ein zusätzliches `Enable(false)` auf
versteckten Controls. `CLTGUICtrl::IsEnabled()` ist bereits als
`m_bEnabled && IsVisible()` definiert, die Navigation überspringt unsichtbare
Einträge also ohnehin. Ein Enable/Disable-Paar hätte beim Wiedereinblenden
zusätzlich statische Controls auswählbar gemacht.

Behoben, indem der Listenanfang festgehalten wird, solange die VR-Seite aktiv
ist — in jedem Client-Update, weil Tastatur, Maus und Controller alle direkt
über `NextSelection` navigieren. Der Schreibzugriff erfolgt nur, wenn der Wert
tatsächlich abweicht.

Ebenfalls entfernt: die toten Controls `MORE SETTINGS >` und
`< BASIC SETTINGS` samt Seitenzustand. Sie wurden noch erzeugt und nur
versteckt, belegten aber weiterhin Listenindizes.

Benutzerabnahme am 25.07.2026: Die Auswahl läuft sauber und springt nicht mehr.

## 15. M5-Abnahme vom 2026-07-25

Automatisiert bestanden:

- x86 und x64 bauen mit `/W4 /WX`;
- je 7/7 CTest-Tests grün: `protocol`, `xr_session_state`, `stereo_math`,
  `head_tracking_math`, `stereo_hud_math`, `input_state`, `controller_mapping`.

Im Spiel bestätigt:

- vollständige semantische Controllerbelegung (§10);
- natives VR-Menü im ESC-Menü, Auswahl sauber (§11, §14);
- Arme schaltbar; Hände, Torso, Beine und Waffe sichtbar (§12);
- Lehnen über die Neigung der linken Hand, Richtung und Schwelle passend (§13).

M5-Gate:

- kein Stuck Input nach Fokusverlust — automatisiert getestet und im Lauf
  bestätigt;
- Controller trenn- und wieder verbindbar — vollständiger Trackingverlust in
  §10 nachgewiesen;
- die Behauptung „Motion-Controlled Aiming“ ist über den roten Zielstrahl und
  die bestätigte Übereinstimmung von Waffen- und Projektilrichtung belegt
  (AD-013). Eine allgemeine „6DoF-Waffe“ wird weiterhin nicht behauptet.

Bewusst zurückgestellt:

- Der gelegentliche Sprung der Waffe beim Treppensteigen ist nicht abschließend
  geklärt. Der Benutzer hat den Punkt am 25.07.2026 ausdrücklich zurückgestellt;
  er ist kein M5-Blocker.

Unverändert aus M4 übernommene Grenzen:

- Translation bleibt ohne Weltkollision opt-in;
- der Stereo-HUD-Mischer benötigt im klassischen D3D9-Pfad weiterhin ein
  CPU-Readback und muss vor M6 ersetzt werden.

## 16. M6 — Verpackung, Deinstallation und Regression

### Ein-Schritt-Build

`tools\build-all.ps1` prüft die gepinnten Abhängigkeiten, konfiguriert und
baut x86 und x64, führt beide CTest-Suiten aus und schreibt
`stage\build-manifest.json`. Verifiziert am 25.07.2026: beide Architekturen
mit `/W4 /WX`, je 7/7 Tests grün.

Die Artefakte sind **prozessreproduzierbar, nicht bitgleich**. Im
Kontrollversuch wurde `build\x86` zweimal gelöscht und auf demselben Commit
neu gebaut; `GameClient.dll` und `fearvr-d3d9.dll` hatten danach jeweils
unterschiedliche SHA-256-Summen, weil MSVC Zeitstempel und PDB-GUIDs
einbettet. Das Manifest hält deshalb den Git-Commit fest und markiert einen
unsauberen Arbeitsbaum ausdrücklich (AD-017).

### Deinstallation

`tools\uninstall-fearvr.ps1` ist ohne `-Apply` ein reiner Trockenlauf.

Aktuelle Launcher schreiben keinen SteamVR-Wert mehr. Der Deinstaller behält
für alte Projektstände die gezielte Wiederherstellung von
`steamvr.autoShowGameTheater`; beide Rückstellzweige wurden gegen eine Kopie
der echten Konfiguration getestet:

- Schlüssel war ursprünglich **nicht vorhanden** → die eingefügte Zeile wird
  entfernt, die Datei parst danach weiterhin als JSON;
- Schlüssel war ursprünglich **vorhanden** → der ursprüngliche Wert wird
  wiederhergestellt (Testsicherung mit `true`, Ergebnis `true`).

Zurückgesetzt wird gezielt dieser eine Schlüssel. Die ganze Sicherung
zurückzukopieren würde alle SteamVR-Einstellungen verwerfen, die der Benutzer
seither geändert hat.

### M6-Gate, nachgewiesen am 25.07.2026

- **Deinstallation entfernt nur Projekt-/Moddateien.** Lauf mit
  `-Scope ProjectOnly -KeepLogs -Apply`: `stage\m0-stock-module-backup`,
  `stage\m2-game` bis `stage\m5-game`, alle Stage-Manifeste und `build\`
  entfernt; die Public-Tools-Stockmodule wurden vorher aus dem Backup
  zurückgestellt.
- **Spielstände bleiben erhalten.** `stage\userdata-*` enthält Saves, Profile
  und Screenshots — das sind Benutzerdaten, keine Moddateien. Alle zehn
  `userdata-*`-Verzeichnisse blieben unangetastet, `fearvr.ini` inklusive
  `fearvr.ini` ebenfalls. Entfernt wird das nur mit
  `-IncludeUserData`.
- **Retail unverändert.** Der SHA-256 der `FEAR.exe` wird vor und nach jedem
  Lauf geprüft; eine Steam-Dateiprüfung ist nicht nötig.
- **Frische Stage allein aus Repo erzeugbar.** Direkt nach der Deinstallation:
  `build-all.ps1` baute beide Architekturen neu (je 7/7 Tests grün),
  `prepare-m5-stage.ps1` erzeugte `stage\m5-game` neu. Die einzigen
  Voraussetzungen waren das Repo, die legal installierte F.E.A.R.-Kopie, die
  lokalen Public Tools und die gepinnten Abhängigkeiten.

### Gemessene Performancezahlen, Lauf `m5-fear-20260725-004241`

RTX 3050 Laptop, SteamVR/OpenXR 2.16.7, Quest 3, Swapchains 2×2064x2208
(Format 29), Shared-Texture 1024x768 B8G8R8A8 über `path=cpu_d3d9ex`.
Laufzeit 1,1 min, 20 Messfenster, davon 11 mit Stereobild. Die Spielzahlen
sind ausschließlich über die Stereofenster gebildet; Menü- und Ladefenster
rendern nur das linke Auge und würden die Mittelwerte verfälschen.

| Kennzahl | Wert |
|---|---|
| XR-Displayrate | Ø 88,8 fps, max 90,1 — stabil auf der 90-Hz-Rate |
| Game-FPS (Stereo) | Ø 49,7, Spanne 15,6–72,3 |
| Renderzeit links | Ø 147,5 µs, max 162 µs |
| Renderzeit rechts | Ø 81,3 µs, max 98 µs |
| Host-Copyzeit | Ø 293,7 µs, max 316 µs (Einzelspitzen bis 2439 µs beim Laden) |
| eingereichte XR-Frames | 6000 |
| konsumierte Spielbilder | 2400 |
| dropped (Ring voll) | 1 |
| reused | 2871 |
| Handles eingeschwungen | 486–518 |

Einordnung:

- Die 2871 reused frames sind **kein Fehler**: Der Host läuft mit 90 Hz, das
  Spiel liefert rund 50 Bilder pro Sekunde. Die Differenz muss zwangsläufig
  aus wiederholt eingereichten Bildern bestehen. Das Verhältnis passt zu den
  gemessenen Raten.
- Genau ein verworfener Frame über den ganzen Lauf. Der Drei-Slot-Ring läuft
  also praktisch nie voll.
- Die Host-Copyzeit von rund 0,3 ms liegt deutlich unter dem 11-ms-Budget
  eines 90-Hz-Frames. Der teure Anteil des `cpu_d3d9ex`-Pfads sitzt auf der
  x86-Seite im Spielprozess, nicht im Host.
- Die Handle-Anzahl bleibt über den ganzen Lauf zwischen 486 und 518, ohne
  Trend. Kein Handle-Leck. Der Wert aus `host_start` (70) ist als Referenz
  ungeeignet, weil er vor der OpenXR-/D3D-Initialisierung entsteht.
- Die linke Renderzeit liegt rund 66 µs über der rechten. Das linke Auge wird
  zuerst gerendert und trägt den Zustandswechsel der Pipeline; in Mono-Phasen
  ist ohnehin nur das linke Auge aktiv.

Damit ist die Kennzahlenpflicht aus §14 erfüllt.

### Asynchroner Latest-Frame-Transport, geprüft am 31.07.2026

Der Lauf `m5-fear-20260731-003753` nutzte VDXR und den frühen
Root-`d3d9.dll`-Proxy. Die Logfolge
`game_device_multithreaded`, `async_cpu_transfer_ready` und
`frame_ready ... path=async` bestätigt, dass nicht der synchrone Fallback
gemessen wurde.

| Kennzahl | Vorher synchron | Latest-Frame-Worker |
|---|---:|---:|
| Blockierzeit im Present-Thread | Ø 16–17,5 ms, max. ca. 28 ms | meist Ø 0,18–0,28 ms |
| Readback und Upload | Bestandteil der Blockierzeit | Ø ca. 14–15 ms im Worker |
| Gesamtes Transportalter | direkt, aber spielthread-blockierend | Ø ca. 22,7–24,1 ms |
| Neue Spielbilder im Host | häufig ca. 45 fps | ca. 44–49 fps im Gameplay |

Es ist höchstens ein noch nicht begonnener Frame vorgemerkt. Trifft vorher ein
neuerer Frame ein, wird der alte GPU-Frame nur noch abgeerntet und nicht mehr
über CPU und D3D9Ex transportiert. Dadurch wächst keine Warteschlange an und
der Host erhält nach einer Überlastung wieder das aktuellste verfügbare Bild.
Der Live-Lauf blieb fehlerfrei; der subjektive Test meldete während des Laufs
ein sehr gutes Ergebnis.

### Runtime-Unabhängigkeit, geprüft am 25.07.2026

| Prüfung | Ergebnis |
|---|---|
| `--validate-only` unter VDXR | `VirtualDesktopXR 1.0.10`, `Meta Quest 3`, Adapter-LUID `0x0:D57B`, Swapchains 2×`2688x2880`, Exitcode 0 |
| `--validate-only` unter SteamVR | `SteamVR/OpenXR 2.16.7`, Swapchains 2×`2064x2208` |
| Spielstart `-Runtime vdxr` | `logs\m5-fear-20260725-005345`: Runtime VDXR, Bridge verbunden, `ipc_frame` importiert |
| SteamVR-Theater-Hilfen | entfernt; weder unter VDXR noch SteamVR wird ein Wächter gestartet oder `steamvr.vrsettings` geändert |

VDXR liefert mit `2688x2880` je Auge eine deutlich höhere Swapchain-Auflösung
als SteamVR. Das Spielbild bleibt davon unberührt: Es kommt weiterhin als
1024x768-Textur über die Bridge und wird im Host hochskaliert.

Umgeschaltet wird über `-Runtime`, das `XR_RUNTIME_JSON` nur für den
Hostprozess setzt. Die systemweite Runtime-Einstellung wird nicht verändert
(AD-018).

## 17. Weitergebbares Overlay

`tools\make-release.ps1` erzeugt unter `dist\` einen Ordner und ein ZIP, deren
Inhalt direkt in den Ordner mit `FEAR.exe` entpackt wird.

| Prüfung | Erwartetes Ergebnis |
|---|---|
| Archivwurzel | `FEARVR\`, Standardstarter und SteamVR-Starter; kein zusätzlicher Paketordner |
| Öffentlicher Modus | keine Retail- oder Public-Tools-Datei, Manifest `redistributable=true` |
| Privater Modus | sieben Laufzeitmodule plus Body-Assets, Manifest `redistributable=false` und Warnhinweis |
| Paketintegrität | `prepare-overlay.ps1` prüft ausgelieferte Dateien gegen `release-manifest.json` |
| Erststart öffentlich | Public-Tools-Erkennung über den Hash des unveränderten VC7.1-`GameClient.dll` |
| SteamVR | Starter setzt für den Hostprozess Valves OpenXR-Manifest; systemweite Runtime bleibt unverändert |
| Update | neues ZIP über denselben Ordner; `FEARVR\userdata` bleibt erhalten |
| Entfernung | `FEARVR\` und beide Starter löschen; keine Retail-Datei muss repariert werden |

### Fehlschlag beim ersten Paketstand

Standardziel war `%LOCALAPPDATA%\FearVR`. Dort brach das Spiel mit
„Failed to initialize client - unable to load game resources" ab. Im Prozess
waren nur 48 statt 152 Module geladen, es entstand kein Proxy-Log, und der
Host zeigte mangels Bildern seinen roten Ersatzbildschirm.

Eingegrenzt wurde es durch Kreuztests mit byteweise identischen Dateien: Nur
der **Ort der Archivkonfiguration** entscheidet, das Modulverzeichnis darf
unter `%LOCALAPPDATA%` liegen. Details und Messtabelle in AD-020.

Behoben durch Standardziel `%USERPROFILE%\FearVR` und eine Sperre in
`install.ps1` gegen Ziele unterhalb von `%LOCALAPPDATA%`.

### Offen

- Ein Lauf über volle 15 Minuten **mit** Instrumentierung. Der Messlauf war
  1,1 min lang; das Dauergate selbst wurde bereits in §8 auf Basis des
  11½-Minuten-Laufs abgenommen.
- Die in §2 als offen markierten Live-Regressionen (Fenstermodus/Vollbild,
  Alt-Tab, gezielter Auflösungswechsel, Save/Load, Tod/Respawn, Leiter,
  Knockdown, Debug-Build).
