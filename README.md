# F.E.A.R. VR

Quelloffener, lokal baubarer VR-Mod für die **Singleplayer-Basisversion von
F.E.A.R. 1.08** (`FEAR.exe`, LithTech Jupiter EX, Direct3D 9).

> **Status:** M6 (Verpackung und Regression). M5 ist abgeschlossen und im Spiel
> bestätigt: nativer Stereo-Weltrender, relatives HMD-Headtracking,
> F9-Recenter, lesbares raumfestes Menü und Stereo-HUD sind mit dem echten
> F.E.A.R. auf Quest 3/SteamVR bestätigt. Die OpenXR-Controller steuern das
> Spiel vollständig: Bewegen, Drehen, Waffenwahl, Springen, Nachladen, Ducken,
> Zeitlupe, Rennen, Benutzen, Zielen/Feuern, Recenter und Pausenmenü; Lehnen
> läuft über die Neigung der linken Hand. Im Ego-Blick sind nur Hände und Waffe
> sichtbar, und das ESC-Menü enthält eine native VR-Einstellungsseite. Der
> klassische D3D9-Pfad verwendet weiterhin einen markierten
> CPU-Kompatibilitätspfad; Translation bleibt ohne Weltkollision opt-in.
> Details: `docs/TESTING.md`.

## Funktionsumfang

Alles hier Aufgeführte ist implementiert und im echten Spiel gelaufen. Wo
etwas gebaut, aber noch nicht im Spiel abgenommen ist, steht es dabei.

### Darstellung

- **Nativer Stereo-Weltrender.** Die LithTech-Kamera rendert zweimal pro
  Frame mit eigener Augenmatrix — kein nachträgliches Verdoppeln eines
  Monobildes. Startet nach dem Laden automatisch; F8 schaltet jederzeit um.
- **Relatives Headtracking.** Die HMD-Rotation dreht die Spielkamera relativ
  zur Neutralpose; Nick und Roll bleiben erhalten. F9 oder rechter Stick-Klick
  setzt die aktuelle Blickrichtung als Neutralpose.
- **Optionale HMD-Translation** bis 25 cm (`-Translation`). Ohne
  Weltkollision, deshalb bewusst opt-in.
- **Stereo-HUD.** Munition, Gesundheit und Hinweise werden als Overlay in
  beide Augen gehoben statt flach über das linke Bild gelegt. Das HUD wird
  dabei gleichmäßig zur Bildmitte gestaucht (5/4), damit die Randblöcke im
  bequemen Sichtfeld liegen.
- **Flachbildmodus für Vollbild-UI.** Menüs, Ladebildschirme, Movies und das
  Missionsbriefing erscheinen als raumfestes 2,4 × 1,8 m großes Panel in 2 m
  Abstand. Erkannt wird das am Retail-Spielzustand
  `CInterfaceMgr::m_eGameState`, nicht an Pixelheuristiken. Rechter
  Stick-Klick verankert das Panel neu in Blickrichtung.
- **Komfortbildschirm (F10).** Raumfeste Darstellung für Camera-Shakes und
  Zwischensequenzen, damit erzwungene Kamerabewegung nicht am Kopf zerrt.
- **Ruhige Kamera.** Waffen-Bob und Kamera-Rückstoß sind abgeschaltet;
  Head-Bob ist standardmäßig aus und nur über `fearvr.ini` zuschaltbar.
- **Ego-Körper korrigiert.** Ober- und Unterarme sind ausgeblendet, Hände und
  Waffe bleiben sichtbar. Das klassische Fadenkreuz ist aus, weil der rote
  Zielstrahl seine Aufgabe übernimmt.

### Motion Controls

- **Vollständige Spielsteuerung über OpenXR-Controller** — Bewegen, Drehen,
  Springen, Ducken, Rennen, Waffenwechsel, Nachladen, Granate, Zeitlupe,
  Benutzen, Zielen, Feuern, Pause und Recenter. Maus, Tastatur und Gamepad
  bleiben parallel nutzbar.
- **Waffe folgt der rechten Hand.** Schussursprung und Fire-Vectors kommen
  aus der Mündungstransformation, nicht aus der Blickrichtung.
- **Roter Zielstrahl** aus der Mündung, ein- und ausschaltbar.
- **Zeigen statt hinsehen.** Aktivieren und Aufsammeln folgen dem
  Waffenstrahl mit rund 1,5 m Reichweite, statt der Kopfrichtung.
- **Handlampe in der linken Hand.** Sie folgt Position und Zielrichtung der
  Hand und schaltet mit einem Klick auf den linken Trigger. Die zweite,
  nicht schaltbare Retail-Taschenlampe ist entfernt.
- **Lehnen über die Handneigung.** Die linke Hand seitlich neigen lehnt um
  die Ecke; umgedrehte und hängende Hände sind abgefangen.
- **Haptik pro Schuss**, auch im Dauerfeuer — ausgelöst am Retail-Schuss,
  nicht an der Triggerflanke. Bei leerem Magazin vibriert nichts.

### Menü und Einstellungen

- **Native VR-Einstellungsseite** im ESC-Menü („VR SETTINGS"), mit dem
  Controller bedienbar: Stereo rendering, Stereo HUD, Turn speed, Red aim
  guide, Controller vibration, Recenter view, Reset VR defaults.
- **Persistenz in `fearvr.ini`**, inklusive der nicht im Menü sichtbaren
  Einstellungen HMD-Translation, Head-Bob und Komfortbildschirm.
- **F11-Kalibrierung** für die Player-Body-Pieces, falls der Standard für das
  Arm-Piece einmal nicht passt. Das Ergebnis wird sofort gespeichert.

### Betrieb

- **Zwei Prozesse nach Bitness:** x64-OpenXR-Host und x86-D3D9-Bridge über
  ein versioniertes Shared-Memory-Protokoll mit Frame-Ring und
  Heartbeat-Überwachung. Fällt der Host aus, läuft das Spiel flach weiter.
- **Runtime-Wahl zur Laufzeit:** SteamVR und VirtualDesktopXR sind bestätigt;
  `-Runtime` setzt `XR_RUNTIME_JSON` nur für den Hostprozess.
- **SteamVR-Desktop-Theater** wird automatisch abgeschaltet und überwacht,
  unter VDXR unterbleibt das vollständig.
- **Strukturierte JSON-Logs** für Host und Bridge samt Perf-Zählern; jeder
  Lauf schreibt in ein eigenes Verzeichnis unter `logs\`.
- **Versionsbindung mit Notausstieg.** Alle Retail-Hooks prüfen Zeitstempel,
  Abbildgröße und die erwarteten Bytefolgen. Passt etwas nicht, bleibt der
  jeweilige Hook aus und das Spiel läuft weiter. Zusätzlich gibt es
  Diagnoseschalter wie `-fearvr-safe`, `-fearvr-no-interaction` oder
  `-fearvr-no-gamestate`.
- **Weitergebbares Paket** (`tools\make-release.ps1`) mit Installer,
  Desktop-Verknüpfung und Deinstallation. Es enthält ausschließlich eigene,
  MIT-lizenzierte Binaries; die proprietären Module holt der Installer aus
  der lokalen Public-Tools-Installation.

## Grundprinzipien

- **Retail bleibt unangetastet.** Es wird nichts in die Steam-Installation
  geschrieben und keine originale EXE/DLL/Archivdatei überschrieben. Gearbeitet
  wird ausschließlich in einer isolierten Stage unter der Projektwurzel
  (`stage/`) mit eigenem `-userdirectory`.
- **Keine Retail-/SDK-/Asset-Dateien in Git.** Siehe `.gitignore`.
- **Getrennte Prozesse nach Bitness:** ein x64-OpenXR-Host besitzt die
  OpenXR-Session; die x86-`FEAR.exe` rendert über eine `d3d9.dll`-Bridge und ein
  lokal neu gebautes GameClient-Modul. Grund: Der 32-Bit-OpenXR-Runtime-Eintrag
  fehlt auf diesem Rechner (siehe `docs/ENVIRONMENT.md`).

## Architektur (Kurzform)

```text
SteamVR / OpenXR (x64)
   ^  OpenXR + XR_KHR_D3D11_enable
fearvr-host.exe (x64, D3D11)
   ^  versioniertes IPC (Posen, FOV, Shared-Texture-Handles)
FEAR.exe (x86, D3D9) + d3d9.dll-Bridge + GameClient-Modul (x86)
   v  LithTech RenderCamera, zweimal pro Frame
```

Details: `docs/ARCHITECTURE.md`.

## Repository-Struktur

| Pfad | Inhalt |
|---|---|
| `docs/` | Umgebung, Architektur, Koordinaten, Stereo-Research, Tests |
| `src/common/` | geteilter IPC-Vertrag (`protocol.h`) + Mathe |
| `src/host64/` | x64-OpenXR-Host (`fearvr-host.exe`) |
| `src/proxy32/` | x86-D3D9-Proxy/Bridge |
| `src/gameclient_loader/` | ABI-neutraler Loader für die echte `archcfg`-Stage |
| `src/launcher/` | Launcher (startet Host, dann isolierte `FEAR.exe`) |
| `game-source-overlay/` | nur **neu geschriebene** GameClient-Projektdateien |
| `patches/` | minimale, lizenzgeprüfte Diffs / Transformationsskripte |
| `shaders/` | Host-Fullscreen-/Composite-Shader |
| `tests/` | automatisierte Tests (Protokoll, Mathe, State-Machine …) |
| `tools/` | `verify-install.ps1`, `prepare-m5-stage.ps1`, `launch-m5-fear.ps1` … |
| `vendor-local/`, `build/`, `stage/`, `logs/` | lokal, **nicht** in Git |

## Voraussetzungen

Siehe `docs/ENVIRONMENT.md` für den geprüften Ist-Zustand und die noch
fehlenden Komponenten. Kurz:

- F.E.A.R. 1.08 (Ultimate Shooter Edition), legal installiert
- Visual Studio 2022 mit „Desktopentwicklung mit C++" (+ Toolset v141 für
  Compile-/Quellanalyse; laufzeitfähige Public-Tools-Module benötigen VC7.1)
- CMake, Git
- SteamVR als aktive OpenXR-Runtime + Headset
- lokaler offizieller Public-Tools-Installer 1.08

## Schnellstart (Environment prüfen)

```bash
pwsh -File tools/verify-install.ps1
```

Prüft Retail-Pfad, `FEAR.exe`-Hash/-Version, OpenXR-Runtime, Registry und
vorhandene Build-Tools und meldet fehlende Komponenten — ohne etwas zu ändern.

## Build

Ein Aufruf prüft die gepinnten Abhängigkeiten, baut x86 und x64, führt beide
Testsuiten aus und schreibt `stage\build-manifest.json` mit den SHA-256-Summen
aller Artefakte:

```powershell
pwsh -File tools\build-all.ps1
```

Einzeln geht es weiterhin; x86 (Proxy) und x64 (Host) werden **getrennt**
gebaut:

```powershell
pwsh -File tools\prepare-dependencies.ps1

cmake -S . -B build\x86 -A Win32 -DFEARVR_BUILD_PROXY=ON -DFEARVR_BUILD_HOST=OFF
cmake --build build\x86 --config RelWithDebInfo

cmake -S . -B build\x64 -A x64 -DFEARVR_BUILD_PROXY=OFF -DFEARVR_BUILD_HOST=ON
cmake --build build\x64 --config RelWithDebInfo
```

`-G "Visual Studio 17 2022"` gehört dazu: Ohne `-G` wählt CMake das neueste
installierte Visual Studio, und die x86-Module müssen v141-/VC7.1-kompatibel
bleiben. `build-all.ps1` erkennt einen mit fremdem Generator angelegten
Buildbaum und erzeugt ihn neu.

Die Artefakte sind **prozessreproduzierbar, nicht bitgleich**: MSVC bettet
Zeitstempel und PDB-GUIDs ein, sodass zwei Builds derselben Quellen
unterschiedliche Hashes ergeben. Das Manifest hält den Git-Stand fest und
warnt, wenn der Arbeitsbaum nicht sauber ist.

M1-Host gegen die aktive OpenXR-Runtime prüfen:

```powershell
build\x64\src\host64\RelWithDebInfo\fearvr-host.exe --validate-only
build\x64\src\host64\RelWithDebInfo\fearvr-host.exe --max-frames 120
```

M2-Brücke und echte Stage:

```powershell
pwsh -File tools\test-m2-bridge.ps1
pwsh -File tools\test-m2-bridge.ps1 -ClassicD3D9
pwsh -File tools\test-m2-bridge.ps1 -AbortHost
pwsh -File tools\prepare-m2-stage.ps1
pwsh -File tools\launch-m2-fear.ps1
```

Spielbarer M4-Stand:

```powershell
pwsh -File tools\prepare-m4-stage.ps1
pwsh -File tools\launch-m4-fear.ps1
```

M5 mit Motion Controls:

```powershell
pwsh -File tools\prepare-m5-stage.ps1
pwsh -File tools\launch-m5-fear.ps1
```

## VR-Runtime: SteamVR oder Virtual Desktop

Der Mod ist an keine bestimmte Runtime gebunden — der x64-Host spricht nur
OpenXR. Bestätigt sind **SteamVR** und **VirtualDesktopXR (VDXR)**.

```powershell
pwsh -File tools\launch-m5-fear.ps1                    # aktive Runtime
pwsh -File tools\launch-m5-fear.ps1 -Runtime vdxr      # Virtual Desktop
pwsh -File tools\launch-m5-fear.ps1 -Runtime steamvr   # SteamVR
```

`-Runtime` setzt `XR_RUNTIME_JSON` **nur für den Hostprozess**. Die
systemweite Einstellung unter
`HKLM\SOFTWARE\Khronos\OpenXR\1\ActiveRuntime` wird nicht verändert; wer sie
dauerhaft umstellen will, tut das im Virtual Desktop Streamer
beziehungsweise in SteamVR. `tools\verify-install.ps1` zeigt die aktive
Runtime und welche installiert sind.

Nur bei SteamVR werden die SteamVR-spezifischen Schritte ausgeführt
(`autoShowGameTheater` abschalten, Theaterwächter). Unter VDXR unterbleiben
sie vollständig, und es wird keine SteamVR-Datei angefasst.

**Steam bleibt trotzdem nötig** — aber nur als Store: F.E.A.R. wird offiziell
über `steam.exe -applaunch 21090` gestartet. Das ist unabhängig davon, welche
VR-Runtime rendert. SteamVR selbst muss unter VDXR nicht laufen.

Belegung: linker Stick bewegt, linker Grip rennt; der linke Stick-Klick ist
frei. Rechter Stick dreht; ab 80 % Ausschlag springt er nach oben und duckt
nach unten, Stick-Klick zentriert die Blickrichtung. A wechselt die Waffe, B lädt
kurz gedrückt nach und wirft gehalten eine Granate, X schaltet Zeitlupe,
Y öffnet die Pause. Rechter Grip benutzt, die Trigger zielen und feuern. Die linke Hand
seitlich zu neigen lehnt um die Ecke. Die Taschenlampe sitzt in der linken
Hand, folgt deren Position und Zielrichtung und wird mit einem Klick auf den
linken Trigger geschaltet. Jeder Schuss vibriert. Maus, Tastatur und
Gamepad bleiben parallel nutzbar. Details: `docs/OPENXR-INPUT.md`.

Im Ego-Blick sind nur Hände und Waffe zu sehen; Ober- und Unterarm sind
ausgeblendet.

Der M5-Start aktiviert das bestätigte Stereo-HUD standardmäßig und schließt
SteamVRs verzögertes F.E.A.R.-Desktop-Theater automatisch. Optionen:

- `-Translation`: begrenzte HMD-Translation bis 25 cm, ohne Weltkollision;
- Head-Bob ist standardmäßig aus; `HeadBob=1` in `fearvr.ini` aktiviert nur
  die Kamerabewegung, während die Waffe zum stabilen Zielen ruhig bleibt;
- `-NoHeadBob`: erzwingt Head-Bob aus, auch wenn die INI ihn aktiviert;
- `-NoStereoHud`: nur für Vergleich/Fehlersuche.

Tasten im Spiel:

- F8: nativen Stereo-Weltrender ein-/ausschalten;
- F9: aktuelle HMD-Ausrichtung zentrieren;
- F10: raumfesten Komfortbildschirm für Camera-Shakes und Zwischensequenzen
  ein-/ausschalten;
- F11: Player-Body-Pieces einzeln isolieren, um das Arm-Piece neu zu
  kalibrieren. Nur nötig, wenn der Standard einmal nicht passt.

Das ESC-Menü enthält in M5 direkt hinter „Optionen“ den englisch beschrifteten
Eintrag „VR SETTINGS“. Die Seite ist bewusst kurz und einseitig: Stereo
rendering, Stereo HUD, Turn speed, Red aim guide, Controller vibration,
Recenter view, Reset VR defaults und BACK. HMD-Translation, Head-Bob und
Komfortbildschirm bleiben in `fearvr.ini` einstellbar, ohne die native
Menüliste zu überfüllen. Die Auswahl wird unter
`stage/userdata-m5/fearvr.ini` gespeichert. Stick navigiert, A oder Trigger
bestätigt und B geht zurück.

## Deinstallation

Der Mod schreibt außerhalb der Projektwurzel genau **eine** Datei:
`steamvr.vrsettings`, und dort ausschließlich den Schlüssel
`steamvr.autoShowGameTheater`. Es gibt keine Registry-Änderung, keinen
Schreibzugriff auf die Retail-Installation und keine Datei außerhalb des
Projektordners.

```powershell
pwsh -File tools\uninstall-fearvr.ps1          # Trockenlauf, ändert nichts
pwsh -File tools\uninstall-fearvr.ps1 -Apply   # tatsächlich entfernen
```

Entfernt werden `stage\`, `build\`, `dist\`, `local-runtime\` und `logs\`.
Zuvor wird `autoShowGameTheater` aus der ältesten Sicherung gezielt
zurückgesetzt — nur dieser eine Schlüssel, damit spätere eigene
SteamVR-Einstellungen erhalten bleiben.

**Nicht entfernt werden Spielstände.** `stage\userdata-*` ist das
`-userdirectory` des Spiels und enthält Saves, Profile und Screenshots. Das
sind Benutzerdaten, keine Moddateien; sie verschwinden nur mit
`-IncludeUserData`. Weitere Schalter: `-KeepLogs`, `-IncludeVendor` und
`-Scope SteamVrOnly|ProjectOnly`.

Eine Steam-Dateiprüfung ist nicht nötig, weil Retail nie beschrieben wurde.
Das Skript prüft den SHA-256 der `FEAR.exe` vor und nach dem Lauf.

SteamVR sollte dabei geschlossen sein: Es schreibt seine Konfiguration beim
Beenden neu und würde die Rückstellung sonst überschreiben. Das Skript warnt,
wenn es SteamVR laufen sieht.

## Bekannte Grenzen

- Der klassische D3D9-Pfad braucht ein CPU-Readback pro Frame
  (`FEARVR_BF_CPU_FALLBACK`), ebenso der Stereo-HUD-Mischer. Beides ist als
  Techniknachweis markiert und kein Release-Performancepfad.
- HMD-Translation hat keine Weltkollision und bleibt deshalb opt-in
  (`-Translation`).
- Die versionsabhängigen Hooks gelten für **F.E.A.R. 1.08.282.0**. Bei
  abweichendem Hash oder abweichender Signatur bleiben sie deaktiviert und das
  Spiel läuft flach weiter.
- Die linke System-/Menütaste ist nicht belegbar: SteamVR fängt sie für sein
  eigenes Systemmenü ab.
- Der Waffen-Sprung beim Treppensteigen ist nicht abschließend geklärt und
  bewusst zurückgestellt.
- „Motion-Controlled Aiming" ist über Zielstrahl und Trefferpunkt belegt; eine
  allgemeine „6DoF-Waffe" wird nicht behauptet.

## Lizenz

Die selbst geschriebenen Bestandteile stehen unter der **MIT-Lizenz** (siehe
`LICENSE`). Für die Lizenzgrenzen der Abhängigkeiten und der offiziellen
F.E.A.R.-Client- und Public-Tools-Bestandteile gilt `THIRD_PARTY_NOTICES.md`.

## Rechtlicher Hinweis

Dieser Mod enthält **keine** Retail-Dateien, keine proprietären SDK-Quellen und
keine extrahierten Assets. Zum Bauen und Betreiben ist eine eigene, legal
erworbene F.E.A.R.-Installation sowie der offizielle Public-Tools-Installer
erforderlich. „VR spielbar" wird frühestens ab M4 behauptet, „Motion Controls"
ab M5.
