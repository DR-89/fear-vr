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

### AD-002 — Statischer OpenXR-Loader mit festem SDK-Pin

- **Problem:** Der Host braucht reproduzierbare Header und einen Loader, darf
  aber keine zufällige DLL neben dem Spiel bevorzugen.
- **Messung/Beleg:** OpenXR-SDK `release-1.1.59` baut unter Windows einen
  statischen Loader; die Runtime-Auswahl bleibt beim registrierten
  `ActiveRuntime`-Manifest.
- **Gewählte Lösung:** lokaler, gitignorierter Checkout auf Commit
  `e5df31de6c15b4900aee3092273194e51282000d`, Prüfung über
  `tools/prepare-dependencies.ps1`, Link gegen `OpenXR::openxr_loader`.
- **Bekannter Nachteil:** Der Loader wird Bestandteil jedes Host-Builds.
- **Rückfallpfad:** Ein dynamischer Loader kann später als separat gepinnte
  Distributionsdatei gebaut werden.

### AD-003 — Zwei Swapchains und OpenXR-1.0-Anwendungsniveau

- **Problem:** M1 muss Links/Rechts eindeutig unterscheiden und auch mit
  älteren, OpenXR-1.0-kompatiblen Runtimes funktionieren.
- **Messung/Beleg:** SteamVR/OpenXR 2.16.7 lehnte eine 1.1-Anforderung mit
  `XR_ERROR_API_VERSION_UNSUPPORTED` ab. Khronos `hello_xr` fordert ebenfalls
  `XR_API_VERSION_1_0` an. Der Live-Test akzeptierte zwei Swapchains mit je
  `1624x1736`.
- **Gewählte Lösung:** OpenXR 1.0 anfordern, `XR_KHR_D3D11_enable` verwenden,
  je Auge eine Swapchain erzeugen und links rot/rechts blau leeren.
- **Bekannte Nachteile:** Zwei Swapchains verursachen mehr Objekte; M1 rendert
  noch keine Spielbilder.
- **Rückfallpfad:** Bei späterem Profiling kann auf eine Array-Swapchain
  gewechselt werden, sofern die Links-/Rechts-Zuordnung gleichwertig testbar
  bleibt.

### AD-004 — Versionierter Drei-Slot-Ring über die Bitnessgrenze

- **Problem:** Das x86-Spiel und der x64-Host dürfen keine rohen Pointer teilen
  und sich im jeweiligen Renderthread nicht gegenseitig blockieren.
- **Messung/Beleg:** Der D3D9Ex-Livetest importierte fortlaufend frische
  Frame-/Generationspaare über dieselbe NVIDIA-Adapter-LUID; Reset,
  Minimieren, Host-Abbruch und Prozessende hingen nicht.
- **Getestete Optionen:** Einzeltextur mit implizitem Timing, CPU-Kopie und
  expliziter Shared-Texture-Ring.
- **Gewählte Lösung:** Drei Slots je Auge mit atomaren Zuständen,
  Frame-ID/Generation, D3D9- und D3D11-Event-Queries sowie privater
  D3D11-Zieltextur im Host. Der IPC-Header ist versioniert und hat in x86/x64
  dieselbe geprüfte Größe.
- **Bekannte Nachteile:** Mehr VRAM und Synchronisationslogik; bei vollem Ring
  werden Frames verworfen.
- **Rückfallpfad:** Ohne Host oder bei Protokoll-/Adapterfehler rendert das
  normale Flat-`Present` weiter.

### AD-005 — Offizielle archcfg-Stage plus spätes D3D9-Hooking

- **Problem:** Eine kopierte Steam-EXE scheitert mit Application Load Error
  `5:0000065434`; `FEARDevSP.exe` fordert ein CD/DVD-Laufwerk. Das offizielle
  GameClient-Modul wird außerdem erst nach Erzeugung des D3D9-Geräts geladen.
- **Messung/Beleg:** Der reale Lauf verifizierte Loader, Originalmodul und
  Bridge aus `stage\m2-game`; ein isolierter Hook-Test fing `Present` und
  `Reset` eines bereits existierenden Geräts ab.
- **Getestete Optionen:** kopierte EXE, Dev-EXE, früher IAT-Hook und offizielle
  `-archcfg`-Modulschicht mit spätem Detour.
- **Gewählte Lösung:** Retail unverändert über Steam starten, eigene
  `-archcfg`-Stage verwenden, originales VC7.1-GameClient über einen
  ABI-neutralen Loader weiterreichen und die bereits vorhandenen
  `Present`-/`Reset`-Ziele mit fest gepinntem MinHook 1.3.4 detouren.
- **Bekannte Nachteile:** Ein später Hook sieht `Direct3DCreate9` und
  `CreateDevice` nicht mehr; Reset-/Present-Ressourcen müssen ohne
  Erzeugungshook sicher erkannt werden.
- **Rückfallpfad:** Stage weglassen und F.E.A.R. unverändert über Steam
  starten. Es gibt keine Remote-Thread-Injection und keinen Retail-Schreibzugriff.

### AD-006 — Temporärer CPU-D3D9Ex-Pfad für klassisches D3D9

- **Problem:** F.E.A.R. verwendet klassisches `IDirect3DDevice9`; eine darauf
  angelegte Shared Texture wurde mit `D3DERR_INVALIDCALL` abgelehnt.
- **Messung/Beleg:** Der D3D9Ex-Testproducer funktioniert GPU-direkt. Der
  klassische D3D9-Test und das echte 1024×768-Spielbild funktionieren nur über
  den protokollierten `path=cpu_d3d9ex`.
- **Getestete Optionen:** direkter Shared-Handle auf D3D9, D3D9Ex-Hilfsgerät
  und kontrollierte CPU-Übertragung zwischen den Geräten.
- **Gewählte Lösung:** Für den M2-Integrationsnachweis
  `GetRenderTargetData`, Zeilenkopie und `UpdateSurface` in eine
  D3D9Ex-Shared-Texture. Das Protokoll setzt ausdrücklich
  `FEARVR_BF_CPU_FALLBACK`.
- **Bekannte Nachteile:** Per-Frame-CPU-Readback, zusätzliche Latenz und
  Bandbreite; die finale Produktionsinvariante ist damit nicht erfüllt.
- **Rückfallpfad:** Bridge deaktivieren oder bis zur GPU-direkten Lösung
  Flat-Screen verwenden. Dieser Pfad darf nicht stillschweigend als
  Release-/Performancepfad gelten.

## 3. Noch zu dokumentieren (Pflicht laut §17)

- [ ] ob und wie `RenderCamera` zweimal **sicher** aufgerufen wird → `STEREO-RESEARCH.md`
- [ ] HUD-Trennung (Welt-HUD vs. Menü-HUD, Quad-/Cylinder-Layer)
- [ ] symmetrische vs. asymmetrische Projektion
- [x] D3D9/D3D11-Format und Synchronisation (Query-Event, Ringpuffer)
      → `M2-D3D9-BRIDGE.md`, AD-004 bis AD-006
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
