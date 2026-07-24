# ARCHITECTURE.md — Architektur & Entscheidungslog

Für jede wesentliche Entscheidung wird hier festgehalten (ANWEISUNG.md §17):
**Problem · getestete Optionen · Messung/Quellcodebeleg · gewählte Lösung ·
bekannte Nachteile · Rückfallpfad.**

## 1. Gesamtarchitektur

```text
SteamVR / OpenXR-Runtime (x64)
             ^
             | OpenXR + XR_KHR_D3D11_enable
    fearvr-host.exe (x64, D3D11)
             ^
             | versioniertes IPC:
             | Posen, FOV, Zustände, Events,
             | D3D9/D3D11-Shared-Texture-Handles
             v
 FEAR.exe (x86, D3D9)
   + d3d9.dll Proxy/Bridge (x86)
   + lokal neu gebautes GameClient-Modul (x86)
             v
      LithTech RenderCamera, zweimal pro Frame
```

Komponenten: `src/host64/`, `src/proxy32/`, `src/launcher/`,
`game-source-overlay/` (GameClient), gemeinsamer Vertrag `src/common/protocol.h`.

## 2. Getroffene Entscheidungen

### AD-001 — Getrennter x64-OpenXR-Host statt OpenXR direkt in FEAR.exe

- **Problem:** `FEAR.exe` ist x86. OpenXR-Loader braucht für 32-Bit-Apps den
  Runtime-Eintrag unter `HKLM\SOFTWARE\WOW6432Node\Khronos\OpenXR\1`.
- **Messung/Beleg:** Eintrag fehlt auf dem Zielrechner; SteamVR liefert lokal
  nur `steamxr_win64.json` (siehe `docs/ENVIRONMENT.md`, 2026-07-24).
- **Getestete Optionen:** (a) OpenXR in x86 einbauen — scheitert an fehlender
  32-Bit-Runtime; (b) separater x64-Host mit IPC — tragfähig.
- **Gewählte Lösung:** separater `fearvr-host.exe` (x64), Kopplung über
  versioniertes IPC + D3D9/D3D11-Shared-Textures.
- **Bekannte Nachteile:** Prozess-/Bitness-Grenze, IPC-Komplexität,
  Shared-Texture-Interop.
- **Rückfallpfad:** Flat-Screen ohne Host bleibt jederzeit möglich.

_Weitere Entscheidungen (AD-002 …) werden pro Meilenstein ergänzt._

## 3. Noch zu dokumentieren (Pflicht laut §17)

- [ ] ob und wie `RenderCamera` zweimal **sicher** aufgerufen wird → `STEREO-RESEARCH.md`
- [ ] HUD-Trennung (Welt-HUD vs. Menü-HUD, Quad-/Cylinder-Layer)
- [ ] symmetrische vs. asymmetrische Projektion
- [ ] D3D9/D3D11-Format und Synchronisation (Query-Event, Ringpuffer)
- [ ] Koordinatenkonversion → `COORDINATE-SYSTEM.md`
- [ ] Verhalten bei CameraFX / Zwischensequenzen (Komfortmodus)
- [ ] Abgrenzung zu Motion-Controlled Aiming (erst ab M5, mit Nachweis)

## 4. Nicht verhandelbare Invarianten (§3)

- Keine Schreibzugriffe auf das Retail-Verzeichnis; Originaldateien nie
  überschreiben.
- Keine OpenXR-/D3D-/Thread-/IPC-Initialisierung in `DllMain`.
- Keine fest codierten Binäroffsets ohne EXE-Hash-/Signaturprüfung
  (unbekannter Build ⇒ Feature deaktiviert).
- Nur der **Welt-Renderdurchlauf** darf pro Auge wiederholt werden — niemals
  Simulation, KI, Partikel, Sound, Eingabe oder Spielzeit doppelt.
- Kein per-Frame-CPU-Readback im finalen Pfad.
- Flat-Screen-Start ohne Host muss immer funktionieren.
