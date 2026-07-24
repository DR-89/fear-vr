# M2 – D3D9-zu-OpenXR-Monobrücke

Stand: 24.07.2026

## Ergebnis

Der funktionale M2-Techniknachweis ist erbracht:

- Ein x86-D3D9-Producer übergibt laufend Bilder an den x64-D3D11/OpenXR-Host.
- Das echte F.E.A.R.-Hauptmenü erscheint in Quest 3/SteamVR gleichzeitig in
  beiden Augen.
- Maus und Tastatur funktionieren unverändert.
- Die sichere Stage wird über die offizielle `-archcfg`-Modulschicht geladen;
  das Retail-Verzeichnis bleibt unverändert.
- Spiel, Bridge und Host hängen weder beim normalen Ende noch bei einem
  erzwungenen Host-Abbruch.

M2 ist trotzdem **noch kein angenehm spielbarer VR-Mod**. Beide Augen sehen
dasselbe flache Monobild; Kopfbewegung steuert die Spielkamera noch nicht. Der
klassische D3D9-Renderer des echten Spiels benötigt außerdem vorläufig einen
langsamen, klar markierten CPU-Kompatibilitätspfad. Native Stereosicht folgt in
M3, Kopfsteuerung und Komfortfunktionen in M4.

## Zwei geprüfte Bildpfade

### Direkter GPU-Pfad für D3D9Ex

```text
D3D9Ex Present (x86)
  -> StretchRect in drei Shared-Texture-Slots je Auge
  -> D3DQUERYTYPE_EVENT
  -> versioniertes Local\-IPC mit Frame-ID und Generation
  -> OpenSharedResource + CopyResource (D3D11/x64)
  -> private Host-Textur je Auge
  -> Fullscreen-Shader in beide OpenXR-Swapchains
```

Dieser Pfad enthält keinen CPU-Readback. Der isolierte D3D9Ex-Testproducer
beweist den GPU-Interop-Pfad einschließlich Ringpuffer, Reset und
Cross-Bitness-Import.

### Kompatibilitätspfad für klassisches D3D9

F.E.A.R. erzeugt ein klassisches `IDirect3DDevice9`. Eine direkt auf diesem
Gerät angelegte Shared Texture wird auf dem geprüften System mit
`D3DERR_INVALIDCALL` abgelehnt. Für den realen M2-Integrationsnachweis arbeitet
die Bridge deshalb so:

```text
Spiel-Backbuffer
  -> StretchRect in D3D9-Render-Target
  -> GetRenderTargetData in Systemspeicher
  -> Zeilenkopie in D3D9Ex-Systemmem-Surface
  -> UpdateSurface in D3D9Ex-Shared-Texture
  -> unveränderter D3D11/OpenXR-Hostpfad
```

Die Bridge veröffentlicht dafür das Diagnoseflag `FEARVR_BF_CPU_FALLBACK`
und protokolliert `path=cpu_d3d9ex`. Dieser Weg ist eine bewusste
M2-Kompatibilitätslösung, **nicht** der endgültige Renderpfad. Er widerspricht
der Produktionsinvariante „kein per-Frame-CPU-Readback“ und ist ein
Performance-/Komfortproblem, das vor einer spielbaren Veröffentlichung
beseitigt werden muss.

## Gemeinsames Protokoll

`src/common/protocol.h` verwendet Protokollversion 2. Der Header ist in x86 und
x64 exakt 432 Byte groß. Enthalten sind:

- getrennte Host-/Game-Heartbeats;
- Host- und Game-Adapter-LUID;
- Prozess-IDs und Diagnoseflags;
- ein Seqlock für den aktuellen OpenXR-Renderauftrag;
- drei Slots je Auge mit `EMPTY`, `WRITING`, `READY`, `CONSUMING`;
- 64-Bit-Shared-Handles, Frame-ID und Generation.

Alle Kernelobjekte tragen eine zufällige Sitzung-ID:

```text
Local\FearVr.M2.<session>.Mapping
Local\FearVr.M2.<session>.FrameReady
Local\FearVr.M2.<session>.SlotConsumed
```

Magic, Version, Headergröße, Slotgröße und Slotzahl werden beim Verbinden
geprüft. Inkompatible Protokolle werden abgelehnt.

## x86-Bridge und spätes Anhängen

`src/proxy32`:

- lädt `%SystemRoot%\SysWOW64\d3d9.dll` über einen absoluten Pfad;
- exportiert die dokumentierte D3D9-Oberfläche mit den originalen Ordinalen;
- hookt `Direct3DCreate9[Ex]`, `CreateDevice[Ex]`, `Reset` und `Present`;
- behält den frühen Proxy-/VTable-Pfad für Testproducer;
- installiert zusätzlich außerhalb von `DllMain` MinHook-Detours für bereits
  erzeugte D3D9-Geräte;
- kopiert das Monobild für M2 identisch in beide Augen;
- gibt `D3DPOOL_DEFAULT`-Ressourcen vor `Reset` frei;
- verwirft Frames bei vollem Ring, statt den Spielthread zu blockieren;
- erkennt einen ausgefallenen Host und lässt das normale D3D9-`Present`
  fail-open weiterlaufen.

Das späte Anhängen ist für F.E.A.R. nötig: Das über `-archcfg` geladene
GameClient-Modul startet erst, nachdem die Engine ihr D3D9-Gerät erzeugt hat.
Ein reiner IAT-Hook auf `Direct3DCreate9` käme daher zu spät. Die Bridge
erzeugt kontrolliert ein verstecktes Hilfsgerät, ermittelt daraus die
`Present`-/`Reset`-Ziele und installiert die Detours mit dem fest gepinnten
MinHook 1.3.4.

Auf Hybrid-GPUs wird die Adapter-LUID in dieser Reihenfolge ermittelt:

1. `IDirect3D9Ex::GetAdapterLUID`;
2. exakter Vendor-/Device-/SubSys-/Revision-Abgleich mit DXGI;
3. Monitor-zu-DXGI-Abgleich für klassische D3D9-Geräte.

## x64-Host

`src/host64` akzeptiert:

```text
fearvr-host --ipc-session <ID> [--max-frames N] [--log-dir PFAD]
            [--exit-on-game-disconnect]
```

Der Host:

- schreibt Pose, FOV und predicted display time per Seqlock;
- prüft vor dem Import die exakte Adapter-LUID;
- beansprucht nur zusammengehörige READY-Slots beider Augen;
- validiert Handles, Dimension, Format, Mips, Array- und Sample-Eigenschaften;
- öffnet die Shared Textures mit `ID3D11Device::OpenSharedResource`;
- kopiert sie GPU-seitig in private D3D11-Texturen;
- gibt einen Slot erst nach einer D3D11-Event-Query wieder frei;
- rendert die letzte vollständige Textur per Fullscreen-Triangle;
- konsumiert Frames auch im OpenXR-Zustand `SYNCHRONIZED`, damit beim
  Wiedererlangen des Fokus sofort ein aktuelles Bild bereitsteht;
- beendet sich anhand des tatsächlichen Spielprozesses und nicht fälschlich
  wegen eines pausierten Heartbeats.

Die private Kopie verhindert, dass eine bereits wiederverwendete
D3D9-Shared-Texture noch von einer OpenXR-Swapchain gelesen wird.

## Automatisierte und isolierte Tests

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\test-m2-bridge.ps1

powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\test-m2-bridge.ps1 -ClassicD3D9

powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\test-m2-bridge.ps1 -AbortHost
```

Nachgewiesen am 24.07.2026 mit Quest 3, Steam Link und
SteamVR/OpenXR 2.16.7:

- x86-D3D9 und x64-D3D11 verwendeten die NVIDIA-LUID `0x0:C91C`;
- der D3D9Ex-Test lief GPU-direkt ohne CPU-Readback;
- der klassische D3D9-Test lief über den markierten CPU-D3D9Ex-Pfad;
- beide Augen importierten 960×540 und nach Reset 800×450;
- Frame-ID und Generation wurden laufend frisch konsumiert;
- Minimieren, Wiederherstellen und Device Reset hingen nicht;
- normaler Producer-/Host-Abschluss hing nicht;
- nach erzwungenem Host-Abbruch lief der Producer flat weiter;
- ein separater Hook-Test belegt sowohl IAT- als auch späte
  `Present`-/`Reset`-Hooks an einem realen D3D9-Gerät.

Relevante gitignorierte Logs:

```text
logs\m2-20260724-121708
logs\m2-20260724-121736
logs\m2-20260724-131526
logs\m2-20260724-133323
```

## Echter F.E.A.R.-Lauf

Eine kopierte Steam-`FEAR.exe` ist wegen Steam Application Load Error
`5:0000065434` keine nutzbare Proxy-Stage. `FEARDevSP.exe` verlangt ein
physisches CD/DVD-Laufwerk. M2 umgeht keine dieser Prüfungen.

Stattdessen verwendet die echte Stage die verifizierte offizielle
`-archcfg`-Modulschicht:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\prepare-m2-stage.ps1

powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\launch-m2-fear.ps1
```

`stage\m2-game` enthält lokal:

- `GameClient.dll`: kleiner ABI-neutraler Loader;
- `GameOrig.dll`: unverändertes originales VC7.1-GameClient-Modul;
- `fearvr-d3d9.dll`: M2-Bridge unter eindeutigem Modulnamen;
- die übrigen unveränderten Public-Tools-Modulset-Dateien.

Der Loader dereferenziert keine Engine-/C++-Objekte. Er reicht den
Master-Database-Pointer unverändert an das originale Modul weiter und lädt die
Bridge über einen absoluten Stage-Pfad. Vor jedem Start werden Manifest,
Stage-Dateien, `Default.archcfg` und Retail-EXE-Hash geprüft. Ein x86-Probe
verifiziert anschließend, dass Loader, Originalmodul und Bridge wirklich aus
der Stage geladen wurden. Es gibt keine Remote-Thread-Injection und keinen
Schreibzugriff auf Retail.

Der abschließende reale Lauf liegt unter:

```text
logs\m2-fear-20260724-133432
```

Er belegt:

- Loader, Originalmodul und Bridge wurden aus `stage\m2-game` geladen;
- die späten `Present`-/`Reset`-Hooks wurden installiert;
- Spiel und Host meldeten dieselbe NVIDIA-LUID `0x0:C91C`;
- der Host importierte beide 1024×768-Monotexturen;
- laufende Frames wurden konsumiert;
- OpenXR erreichte `VISIBLE` und `FOCUSED`;
- der Benutzer bestätigte das sichtbare F.E.A.R.-Menü in beiden Augen sowie
  normale Maus-/Tastaturbedienung;
- nach Schließen des SteamVR-Desktop-Overlays blieb das Spielbild sichtbar.

## Gate-Bewertung und nächster Schritt

M2 ist als **funktionaler Monobrücken-Techniknachweis** angenommen. Der
GPU-direkte D3D9Ex-Pfad erfüllt die Synchronisations- und Ringpufferziele. Der
reale klassische D3D9-Pfad erfüllt dagegen noch nicht die strikte
Zero-CPU-Readback-Invariante und ist deshalb kein finaler Produktionspfad.

Die subjektiv unangenehme Nutzung ist im aktuellen Stand erwartbar:

- keine echte Links-/Rechts-Parallaxe;
- keine Kopplung der Spielkamera an die HMD-Pose;
- flaches Spiel-HUD und Menü im gesamten Sichtfeld;
- zusätzlicher CPU-/GPU-Transfer pro Frame.

Als Nächstes folgt M3: nur den Welt-Renderdurchlauf sicher zweimal pro Frame
ausführen und echte Augenprojektionen einspeisen, ohne Simulation, KI,
Partikel, Audio oder Spielzeit zu verdoppeln. M4 ergänzt anschließend
Headtracking, Recenter und Komfortverhalten.
