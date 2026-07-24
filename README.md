# F.E.A.R. VR

Quelloffener, lokal baubarer VR-Mod für die **Singleplayer-Basisversion von
F.E.A.R. 1.08** (`FEAR.exe`, LithTech Jupiter EX, Direct3D 9).

> **Status:** M2 — die x86-D3D9/x64-D3D11-Monobrücke ist implementiert und
> mit dem echten F.E.A.R. im Headset bestätigt. Der D3D9Ex-Test läuft
> GPU-direkt; das klassische D3D9 des Spiels verwendet vorläufig einen
> markierten CPU-Kompatibilitätspfad. Beide Augen zeigen noch dasselbe flache
> Bild, ohne Kopfsteuerung. Dies ist ein Techniknachweis, kein angenehm
> spielbarer VR-Stand. Details: `docs/M2-D3D9-BRIDGE.md`.

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
| `tools/` | `prepare-stage.ps1`, `verify-install.ps1`, `launch-vr.ps1` |
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

x86 (Proxy) und x64 (Host) werden **getrennt** gebaut:

```powershell
pwsh -File tools\prepare-dependencies.ps1

cmake -S . -B build\x86 -A Win32 -DFEARVR_BUILD_PROXY=ON -DFEARVR_BUILD_HOST=OFF
cmake --build build\x86 --config RelWithDebInfo

cmake -S . -B build\x64 -A x64 -DFEARVR_BUILD_PROXY=OFF -DFEARVR_BUILD_HOST=ON
cmake --build build\x64 --config RelWithDebInfo
```

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

Im echten Spiel muss das SteamVR-Desktop-Overlay gegebenenfalls mit der
System-/Menütaste des linken Controllers geschlossen werden. Es ist nicht Teil
des Mods.

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
