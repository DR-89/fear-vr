# Implementierungsauftrag: F.E.A.R. VR

## 1. Auftrag und Ziel

Implementiere einen quelloffenen, lokal baubaren VR-Mod für die **Singleplayer-Basisversion von F.E.A.R. 1.08**. Arbeite iterativ und liefere nach jedem Meilenstein einen tatsächlich startbaren Stand. Der Mod darf die originale Steam-Installation nicht verändern und darf keine Spieldateien, Archive, Assets oder proprietären SDK-Quellen in Git aufnehmen.

Das Endziel ist:

- OpenXR-Ausgabe über die aktive PC-VR-Runtime;
- stereoskopisches Rendering mit einem eigenen Bild pro Auge;
- Headtracking, zunächst Rotation und IPD, danach vorsichtig begrenzte 6DoF-Translation;
- spielbare Eingabe mit Tastatur/Maus oder Gamepad;
- später OpenXR-Controllerbelegung und optional entkoppeltes Waffen-Zielen;
- VR-taugliches HUD, Menüs, Zwischensequenzen und Komfortoptionen;
- reproduzierbarer x86/x64-Build und eine Installation, die nur lokal vorhandene, legal erworbene F.E.A.R.-Dateien verwendet.

Ein in beiden Augen dupliziertes Monobild oder eine virtuelle Leinwand ist **nur ein früher Bring-up-Meilenstein**, nicht das fertige VR-Ergebnis.

## 2. Verifizierter lokaler Ausgangszustand

Diese Fakten wurden am 24.07.2026 auf dem Zielrechner geprüft:

| Element | Wert |
|---|---|
| Projektwurzel | `C:\Users\<benutzer>\projects\F.E.A.R-VR` |
| Spielwurzel | `C:\Program Files (x86)\Steam\steamapps\common\FEAR Ultimate Shooter Edition` |
| Basis-EXE | `FEAR.exe` |
| Dateiversion | `1.08.282.0` |
| Architektur | PE32 / x86 (`Machine 0x014C`) |
| SHA-256 von `FEAR.exe` | `D5EBC38A4F12B772C9112A2811C290ADB6C5052D3BC2F817302D38CF55BB2CBE` |
| Renderer | Direct3D 9; `FEAR.exe` importiert `d3d9.dll` und `Direct3DCreate9` |
| Eingabe | `DINPUT8.dll` / `DirectInput8Create` |
| Engine | LithTech Jupiter EX |
| Standard-Benutzerdaten | `C:\Users\Public\Documents\Monolith Productions\FEAR` |
| Unterstützte Isolation | `FEAR.exe -userdirectory <path>` ist laut lokalem `readme.txt` vorhanden |
| Public-Tools-Installer | `...\extras\fear_publictools_108.exe` |
| SDK-Installer-Größe | `671441087` Bytes |
| SHA-256 des SDK-Installers | `11AAA4128528403F7BC9EA5119C68051C62B92A99E6411DFD749AF55E9B19DF8` |
| Aktive OpenXR-Runtime | SteamVR, `C:\Program Files (x86)\Steam\steamapps\common\SteamVR\steamxr_win64.json` |
| 32-Bit-OpenXR-Registrierung | `HKLM\SOFTWARE\WOW6432Node\Khronos\OpenXR\1` fehlt |
| Vorhandene Buildtools | Git vorhanden; CMake, Ninja, `cl` und MSBuild wurden nicht gefunden |

Die OpenXR-Loader-Spezifikation verlangt für eine 32-Bit-Anwendung auf 64-Bit-Windows den aktiven Runtime-Eintrag unter `WOW6432Node`. Da dieser auf dem Zielrechner fehlt und SteamVR lokal nur ein x64-Manifest bereitstellt, ist die Standardarchitektur **ein separater x64-OpenXR-Host**. Versuche nicht, OpenXR dauerhaft direkt in `FEAR.exe` einzubauen.

Prüfe diese Werte zu Beginn erneut. Bei einer anderen `FEAR.exe` darf ein versionsabhängiger Hook nicht aktiviert werden. Brich mit einer verständlichen Diagnose ab, statt unbekannte Offsets zu benutzen.

## 3. Nicht verhandelbare Grenzen

- Arbeite zuerst ausschließlich mit `FEAR.exe`, nicht mit `FEARMP.exe`, `FEARServer.exe`, Extraction Point oder Perseus Mandate.
- Starte keinen Multiplayer- oder PunkBuster-Prozess und umgehe keinen Kopierschutz oder Anti-Cheat.
- Schreibe nichts nach `C:\Program Files (x86)\...\FEAR Ultimate Shooter Edition`.
- Überschreibe niemals die originalen Archive, EXE- oder DLL-Dateien.
- Verwende ein isoliertes Benutzerverzeichnis unter dem Projekt, beispielsweise `stage\userdata`.
- Lege keine Retail-Dateien, Public-Tools-Basisquellen, kompilierten proprietären Tools oder extrahierten Assets in Git ab.
- Keine fest codierten Binäroffsets ohne EXE-Hashprüfung und Signaturprüfung. Unbekannter Build bedeutet „deaktiviert“, nicht „trotzdem versuchen“.
- Keine OpenXR-, D3D-, Thread- oder IPC-Initialisierung in `DllMain`. Dort nur minimale, loader-lock-sichere Arbeit.
- Keine CPU-Rücklesung pro Auge und Frame im finalen Pfad.
- Keine Simulation, KI, Partikel oder Spielzeit zweimal fortschreiben. Nur der Welt-Renderdurchlauf darf pro Auge wiederholt werden.
- Das Spiel muss ohne laufenden Host weiterhin normal in Flat-Screen starten können.

## 4. Was auf dem Rechner benötigt wird

Bitte nichts ungefragt systemweit installieren. Erstelle zuerst `docs/ENVIRONMENT.md` mit dem gefundenen Zustand und nenne fehlende Komponenten.

Erforderlich:

1. Visual Studio 2022 mit „Desktopentwicklung mit C++“.
2. MSVC-x86/x64-Tools und ein aktuelles Windows 10/11 SDK.
3. Zusätzlich das Toolset **v141**, falls der offizielle F.E.A.R.-Clientquellcode mit v143 nicht sauber bzw. ABI-kompatibel baut. Ein aktuelles F.E.A.R.-Referenzprojekt hat den x86-Build mit VS 2022 und v141 nachgewiesen.
4. CMake, vorzugsweise die mit Visual Studio gelieferte Version oder eine aktuelle offizielle Version.
5. Git.
6. SteamVR als aktive OpenXR-Runtime und ein angeschlossenes Headset für Live-Tests.
7. Der lokale offizielle Public-Tools-Installer aus dem oben genannten `extras`-Ordner.

Als Entwicklungswerkzeuge sind optional sinnvoll:

- Visual Studio Graphics Diagnostics bzw. ein für den gewählten Renderpfad geeignetes Capture-Werkzeug;
- apitrace für D3D9-Call-Analyse;
- RenderDoc erst auf dem D3D11-Hostpfad;
- Ghidra nur dann, wenn eine Funktion nicht über die offiziellen Clientquellen erreichbar ist.

Abhängigkeiten müssen auf einen Tag oder Commit festgeschrieben und in `THIRD_PARTY_NOTICES.md` dokumentiert werden. Bevorzugte kleine Abhängigkeiten:

- Khronos OpenXR-SDK/Loader für den x64-Host;
- MinHook für gezielte x86-Hooks, falls der D3D9-Proxy nicht vollständig über COM-Wrapper gelöst wird;
- DirectXMath aus dem Windows SDK.

Vermeide zu Beginn dgVoodoo2, ReShade, RTX Remix und weitere D3D-Proxys. Mehrere Wrapper um `d3d9.dll` erschweren die Fehleranalyse. Kompatibilität damit ist ein späterer, eigener Meilenstein.

## 5. Vorgesehene Architektur

```text
SteamVR / OpenXR-Runtime (x64)
             ^
             | OpenXR + XR_KHR_D3D11_enable
             |
    fearvr-host.exe (x64, D3D11)
             ^
             | versioniertes IPC:
             | Posen, FOV, Zustände, Events,
             | D3D9/D3D11-Shared-Texture-Handles
             v
 FEAR.exe (x86, D3D9)
   + d3d9.dll Proxy/Bridge (x86)
   + lokal neu gebautes GameClient-Modul (x86)
             |
             v
      LithTech RenderCamera, zweimal pro Frame
```

### 5.1 `fearvr-host.exe` (x64)

Der Host besitzt die OpenXR-Session und:

- initialisiert Instance, System, Session und `XR_KHR_D3D11_enable`;
- fragt mit `xrGetD3D11GraphicsRequirementsKHR` den vorgeschriebenen Adapter-LUID und das Feature Level ab;
- erstellt das D3D11-Gerät exakt auf diesem Adapter;
- verwaltet Session-State, Reference Space, Actions und Swapchains;
- ruft pro XR-Frame `xrWaitFrame`, `xrBeginFrame`, `xrLocateViews` und `xrEndFrame` in korrekter Reihenfolge auf;
- sendet die vorhergesagten Augenposen, FOV-Werte und den Frame-Zähler möglichst spät an den Spielprozess;
- öffnet die vom x86-Teil erzeugten D3D9-Shared-Textures mit `ID3D11Device::OpenSharedResource`;
- zeichnet die zwei Texturen mit einem kleinen Fullscreen-Shader in die OpenXR-Swapchainbilder;
- behandelt Formatkonvertierung, Gamma, Skalierung und nötigenfalls Orientierung im Shader;
- überlebt Headset-Standby, Session-Restart, Host-Neustart und `XR_SESSION_LOSS_PENDING`;
- schreibt strukturierte Logs nach `logs\host-YYYYMMDD-HHMMSS.log`.

Beginne mit zwei getrennten Color-Swapchains, je eine pro Auge. Ein Texture-Array kann später optimiert werden.

### 5.2 `d3d9.dll` Proxy/Bridge (x86)

Der Proxy liegt nur in einer **isolierten Stage**, nie in der Retail-Installation. Er:

- lädt das echte `%SystemRoot%\SysWOW64\d3d9.dll` über einen absolut aufgelösten Systempfad;
- leitet alle offiziellen D3D9-Exports weiter;
- interceptiert mindestens `Direct3DCreate9`, `IDirect3D9::CreateDevice`, `IDirect3DDevice9::Reset` und `Present`;
- erkennt Geräteverlust und erstellt alle eigenen `D3DPOOL_DEFAULT`-Ressourcen nach `Reset` neu;
- erstellt pro Auge mindestens zwei ringgepufferte, teilbare 2D-Texturen ohne Mipmaps und MSAA;
- kopiert das fertige Augenbild GPU-seitig in den jeweiligen Slot;
- signalisiert erst nach einem `D3DQUERYTYPE_EVENT`, dass die D3D9-GPU-Arbeit abgeschlossen ist;
- blockiert den Spielthread nie unbegrenzt auf den Host;
- stellt eine kleine optionale C-ABI für das neu gebaute GameClient-Modul bereit.

`FEAR.exe` selbst importiert nur `Direct3DCreate9`; leite trotzdem die komplette dokumentierte D3D9-Exportoberfläche weiter, damit indirekte Nutzer und spätere Kombinationen nicht brechen.

Die C-ABI darf nur fest breite POD-Typen enthalten. Keine STL-Typen, C++-Exceptions, CRT-Heapobjekte oder Pointer über Modul-/Prozessgrenzen. Beispielhafte Funktionen:

```cpp
extern "C" __declspec(dllexport) bool FearVr_IsHostConnected();
extern "C" __declspec(dllexport) bool FearVr_GetRenderRequest(
    FearVrRenderRequest* request);
extern "C" __declspec(dllexport) void FearVr_BeginEye(uint32_t eye);
extern "C" __declspec(dllexport) void FearVr_CaptureEye(uint32_t eye);
extern "C" __declspec(dllexport) void FearVr_EndStereoFrame(uint64_t frameId);
```

Das GameClient-Modul muss diese Funktionen dynamisch über `GetModuleHandleW(L"d3d9.dll")` und `GetProcAddress` auflösen. Fehlen sie, bleibt der originale Renderpfad unverändert.

### 5.3 Neu gebautes GameClient-Modul (x86)

Installiere die offiziellen Public Tools nach Möglichkeit in `vendor-local`, oder dokumentiere den externen Installationsort. Untersuche zuerst die vorhandenen Clientquellen und deren Lizenz. Baue den unveränderten Client als **Stock-Referenz**, bevor VR-Code hinzugefügt wird.

Suche in den Quellen insbesondere nach:

- `OnRender` bzw. dem zentralen Client-Renderpfad;
- `RenderCamera`;
- `Start3D`, `End3D` und `FlipScreen`;
- Player-Camera, CameraFX und `SetCameraFOV`;
- HUD-/Interface-Rendering;
- Camera Shake, Head Bob, Lean, Slow-Mo und Zwischensequenzzuständen;
- Input- und Waffenrichtungsberechnung.

Primärziel ist, den vorhandenen Welt-Renderpfad ohne zweiten Simulationsschritt zweimal aufzurufen:

1. aktuellen XR-Renderauftrag unmittelbar vor dem Rendern lesen;
2. originale Kamera-Pose und FOV sichern;
3. linke Augenpose relativ zum kalibrierten Körper-/Kameraursprung anwenden;
4. ausschließlich die 3D-Welt für links rendern und über die Bridge erfassen;
5. rechte Augenpose anwenden;
6. ausschließlich die 3D-Welt für rechts rendern und erfassen;
7. Kamera wiederherstellen;
8. HUD und Menüs genau einmal rendern;
9. Bildschirm genau einmal präsentieren.

Bevor dieser Pfad als gültig gilt, muss anhand des Quellcodes und eines Laufzeittests bewiesen sein, dass der zweite `RenderCamera`-Aufruf keine Simulation, KI, Soundereignisse, Partikelalterung oder Eingabe doppelt ausführt.

Falls `RenderCamera` nicht zweimal auf den Backbuffer oder einen kontrollierbaren Render-Target-Pfad gerufen werden kann, dokumentiere die genaue Ursache in `docs/STEREO-RESEARCH.md`. Implementiere dann nicht blind einen kompletten D3D9-Command-Replayer. Prüfe in dieser Reihenfolge:

1. offizielle Render-Target-/Camera-APIs aus dem SDK;
2. ein kleiner, versionsgeprüfter Hook um den Engine-Kamera-Renderaufruf;
3. Tiefe plus Bild-Reprojektion nur als klar gekennzeichneter Kompatibilitätsmodus.

D3D9-Depth-Hacks wie `INTZ`/`RESZ` sind herstellerabhängig und dürfen nicht die einzige Basis des Hauptpfads sein.

## 6. IPC- und Synchronisationsvorgaben

Lege den gemeinsamen Vertrag in `src/common/protocol.h` ab und kompiliere ihn in x86 und x64. Anforderungen:

- Magic, Protokollversion und Strukturgröße in jedem Header;
- `uint32_t`/`uint64_t`, keine `size_t`, Handles oder Pointer in nativer Breite;
- explizite Ausrichtung und `static_assert(sizeof(...))` in beiden Architekturen;
- Matrizen nicht als undokumentierter Speicherblock übertragen: Pose als Position plus normalisiertes Quaternion, FOV als vier Winkel;
- Named File Mapping und Named Events nur im `Local\`-Namespace und mit PID/Sitzungs-ID im Namen;
- Timeouts für alle Warteoperationen;
- Ringpuffer mit mindestens zwei, besser drei Slots pro Auge;
- monotone `frameId`/`generation` statt impliziter „letzter Frame“-Annahmen;
- Host-Heartbeat und Game-Heartbeat;
- saubere Protokoll-Ablehnung bei Versionsunterschied;
- Shared-Texture-Handle als `uint64_t` serialisieren und auf der Empfängerseite validieren;
- keine ungeprüften Datenlängen aus dem anderen Prozess verwenden.

Für den ersten korrekten Shared-Texture-Test darf synchron gearbeitet werden:

1. D3D9 kopiert in einen Slot.
2. `D3DQUERYTYPE_EVENT` signalisiert GPU-Fertigstellung.
3. Game setzt `FrameReady`.
4. Host kopiert/rendered die Textur in das XR-Swapchainbild.
5. Host setzt `SlotConsumed`.

Danach auf Ringpuffer umstellen, ohne Datenrennen einzuführen. Der Game-Prozess darf bei Host-Ausfall einen Slot überspringen, aber nie hängen.

Der Host ist Taktgeber für XR. Der Spielprozess verwendet jeweils den neuesten vollständig veröffentlichten Renderauftrag. Wenn bis zum Renderzeitpunkt kein neuer Auftrag vorliegt, darf er die letzte gültige Pose verwenden oder Flat-Screen rendern. Es darf keinen zyklischen Wait geben, bei dem Host und Spiel gegenseitig unbegrenzt aufeinander warten.

## 7. Koordinaten, Projektion und Kamera

OpenXR und LithTech benutzen voraussichtlich unterschiedliche Händigkeit und Vorwärtsachsen. **Nicht raten.**

Erstelle:

- `docs/COORDINATE-SYSTEM.md`;
- Unit-Tests für Achsenabbildung, Quaternion-Konvertierung und Pose-Komposition;
- einen Debugmodus mit farbigen Achsen bzw. eindeutigem „links/rechts/oben/vorne“-Test;
- eine konfigurierbare, zentral definierte Konversion.

Vorgehen:

1. Beim Recenter die HMD-Pose als neutralen lokalen Ursprung speichern.
2. Relative Headpose berechnen, nicht die absolute Tracking-Space-Pose direkt in die Welt schreiben.
3. Körper-Yaw und Head-Yaw getrennt halten.
4. IPD aus den beiden von `xrLocateViews` gelieferten Posen übernehmen, nicht fest auf 64 mm setzen.
5. Zunächst Translation deaktivierbar machen; danach lokale Translation auf einen komfortablen Bereich begrenzen und gegen Wanddurchdringung absichern.
6. Roll der Spielkamera nur aus dem HMD übernehmen, nicht aus künstlichem Weapon Sway oder Camera Shake.

Wenn LithTech nur symmetrische FOV-Werte akzeptiert, verwende im ersten Stereo-MVP ein konservatives symmetrisches FOV und reiche genau dieses FOV in `XrCompositionLayerProjectionView` ein. Dokumentiere die reduzierte Abdeckung. Später kann ein gezielter Projektionsmatrix-Hook asymmetrische OpenXR-Frusta umsetzen.

Nah- und Fern-Clipping, Waffenmodell und Partikeleffekte müssen pro Auge geprüft werden. Vermeide ein zu großes Near Plane, durch das Waffe/Hände verschwinden.

## 8. HUD, Menüs, Videos und Komfort

Erste spielbare Stufe:

- HUD identisch in beide Augen rendern;
- HUD-Größe und scheinbare Tiefe konfigurierbar;
- Menüs vollständig bedienbar;
- vorgerenderte Videos und stark geskriptete Zwischensequenzen auf einer stabilen virtuellen Leinwand anzeigen.

Zielstufe:

- HUD/Menu einmal in eine eigene Textur rendern;
- als OpenXR-Quad- oder Cylinder-Layer in komfortabler Distanz einreichen;
- Welt-HUD und Menü-HUD getrennt skalieren;
- Fadenkreuz wahlweise blick-, kamera- oder waffenrichtungsbezogen.

Komfortoptionen:

- Recenter;
- seated/standing;
- Snap-Turn 30° und 45°;
- Smooth Turn optional;
- dynamische Vignette optional;
- Head Bob, Camera Shake, erzwungenen Roll und starke Lean-Bewegung abschwächen oder deaktivieren;
- Zwischensequenz-/Ladder-/Knockdown-Kamera automatisch in einen Komfortmodus schalten.

## 9. Eingabe und Motion Controller

Motion Controller sind **nicht Teil des ersten Stereo-MVP**.

Reihenfolge:

1. Headtracking plus vorhandene Tastatur/Maus- oder Gamepadsteuerung.
2. OpenXR Actions für Trigger, Grip, Thumbsticks, Menü, A/B/X/Y und Recenter.
3. Diese Actions zunächst als direkte Spielaktionen im Clientcode abbilden; kein globales `SendInput`.
4. Linke Hand: Fortbewegung und Snap-/Smooth-Turn.
5. Rechte Hand: zunächst Buttons; erst später Waffenpose und Schussrichtung.
6. Haptik über OpenXR mit begrenzter Amplitude/Dauer.

Für echtes entkoppeltes Waffen-Zielen müssen Client **und möglicherweise Server-/Gameplaymodul** geprüft werden. Die visuelle Waffenpose, Hitscan-/Projektilrichtung, Mündungsblitz, Rückstoß, Interaktionen und Netzwerknachrichten müssen dieselbe Richtung verwenden. Implementiere dies erst nach stabilem Stereo und nur für Singleplayer. Ein bloß gedrehtes Waffenmodell bei unveränderter Schussrichtung ist nicht akzeptabel.

## 10. Projektstruktur

Lege mindestens diese Struktur an:

```text
F.E.A.R-VR/
  ANWEISUNG.md
  README.md
  LICENSE
  THIRD_PARTY_NOTICES.md
  CMakeLists.txt
  cmake/
  docs/
    ENVIRONMENT.md
    ARCHITECTURE.md
    COORDINATE-SYSTEM.md
    STEREO-RESEARCH.md
    TESTING.md
  src/
    common/
      protocol.h
      math/
    host64/
    proxy32/
    launcher/
  game-source-overlay/
  patches/
  shaders/
  tests/
  tools/
    prepare-stage.ps1
    verify-install.ps1
    launch-vr.ps1
  vendor-local/       # ignorieren
  build/              # ignorieren
  stage/              # ignorieren
  logs/               # ignorieren
```

`game-source-overlay` darf nur neu geschriebene Projektdateien enthalten. Falls Patches gegen das offizielle SDK nötig sind, halte sie minimal und prüfe vor jeder Veröffentlichung, ob die SDK-Lizenz das Verteilen des konkreten Diffs erlaubt. Eine sichere Alternative ist ein lokales Transformationsskript, das auf eine vom Benutzer bereitgestellte SDK-Quelle angewendet wird.

Erstelle früh eine `.gitignore`, die mindestens folgende Inhalte ausschließt:

```gitignore
/vendor-local/
/build/
/stage/
/logs/
/dist/
/local-runtime/
*.Arch00
*.exe
*.dll
*.pdb
```

Erlaube gezielt selbst erzeugte kleine Testprogramme, falls die pauschalen Binärregeln dafür angepasst werden müssen. Committe niemals eine Datei nur deshalb, weil Git sie nicht automatisch ignoriert.

## 11. Buildsystem

Ein CMake-Buildverzeichnis kann nicht gleichzeitig Win32 und x64 sein. Baue getrennt:

```powershell
cmake -S . -B build\x86 -A Win32 -DFEARVR_BUILD_PROXY=ON -DFEARVR_BUILD_HOST=OFF
cmake --build build\x86 --config RelWithDebInfo

cmake -S . -B build\x64 -A x64 -DFEARVR_BUILD_PROXY=OFF -DFEARVR_BUILD_HOST=ON
cmake --build build\x64 --config RelWithDebInfo
```

Das Public-Tools-GameClient-Projekt kann einen separaten x86-Build mit v141 benötigen. Kapsle es so, dass moderne Hostquellen nicht auf alte Compiler-/CRT-Annahmen herabgesetzt werden.

Pflichten:

- Warnings auf hohem Niveau; neue Projektquellen warning-clean;
- `RelWithDebInfo` als primärer Testbuild;
- PDBs lokal erzeugen;
- keine Downloads zur Laufzeit des Spiels;
- FetchContent nur mit festem Commit/Tag;
- reproduzierbarer Offline-Rebuild, sobald Abhängigkeiten vorhanden sind;
- Versionsinformationen in Host, Proxy, Protokoll und Logs.

## 12. Sichere Stage und Launcher

Erstelle `tools/prepare-stage.ps1` so, dass es:

1. Retail-Pfad, Dateiversion und SHA-256 prüft;
2. `stage` ausschließlich unter der Projektwurzel auflöst und validiert;
3. niemals Retail-Dateien löscht, verschiebt oder überschreibt;
4. nur die für den Start nötigen beschreibbaren Binärdateien kopiert;
5. Datenarchive entweder über einen vom SDK dokumentierten Mod-/`archcfg`-Pfad referenziert oder, falls nötig, als unveränderte lokale Kopie nutzt;
6. keine Hardlinks verwendet, wenn dadurch ein Schreibzugriff die Retail-Datei verändern könnte;
7. `-userdirectory "...\stage\userdata"` setzt;
8. vor und nach Tests die Hashes der Retail-EXE und zentralen DLLs vergleicht.

Prüfe als ersten Staging-Spike, ob eine kopierte `FEAR.exe` aus `stage` mit dem Retail-Verzeichnis als Working Directory startet und Daten/DLLs korrekt findet. Wenn nicht, verwende die offiziell dokumentierte Public-Tools-Runtime-/`archcfg`-Methode. Führe keine DLL-Injection per Remote Thread ein, solange eine normale lokale Proxy-/Mod-Stage möglich ist.

Der Launcher:

- startet zuerst `fearvr-host.exe`;
- wartet mit Timeout auf „XR ready“;
- startet dann die isolierte `FEAR.exe` mit getrenntem Benutzerverzeichnis;
- übergibt nur benötigte Pfade/Session-ID;
- beendet bei normalem Spielende den zugehörigen Host;
- lässt das Spiel auf Wunsch ohne VR-Host flat starten;
- gibt bei fehlender Runtime, falschem Adapter oder falscher EXE eine klare Meldung aus.

## 13. Meilensteine und harte Gates

Arbeite strikt in dieser Reihenfolge. Nach jedem Gate: Build, Tests, kurzer Live-Test, Dokumentation und sauberer Commit.

### M0 – Umgebung und unveränderter Quellbuild

Lieferumfang:

- Git-Repository, Grundstruktur, `.gitignore`;
- `docs/ENVIRONMENT.md`;
- Hash-/Versionsprüfer;
- installierte Public Tools nur lokal;
- unverändertes GameClient-Modul erfolgreich als x86 gebaut;
- isolierte Flat-Screen-Stage startet.

Gate:

- Keine Retail-Datei geändert.
- Der Stock-Quellbuild verhält sich in Menü und erstem Spielabschnitt wie Retail.

### M1 – Eigenständiger x64-OpenXR-Host

Lieferumfang:

- x64-Host nach dem offiziellen `hello_xr`-Lebenszyklus;
- zwei verschiedenfarbige Testbilder, eindeutig links/rechts;
- D3D11-Gerät auf dem von OpenXR verlangten Adapter;
- korrekte Session-State-Behandlung und Logs.

Gate:

- Headset zeigt beide Augen korrekt.
- Standby und Session-Neustart verursachen keinen Crash.
- Ohne Runtime endet der Host mit verständlicher Diagnose.

### M2 – x86-D3D9-Proxy und Mono-Brücke

Lieferumfang:

- sicher weiterleitender D3D9-Proxy;
- Reset-/Present-Hooks;
- D3D9-zu-D3D11-Shared-Texture-Test über Prozess- und Bitnessgrenze;
- zunächst das normale F.E.A.R.-Bild in beiden Augen;
- Adapter-LUID-Prüfung und CPU-Fallback nur für Diagnose.

Gate:

- Kein per-Frame-CPU-Readback im normalen Pfad.
- Alt-Tab, Auflösungswechsel, Device Reset, Host-Abbruch und Spielende hängen nicht.
- Framefarben und -zähler beweisen, dass keine alten/recycelten Slots gelesen werden.

### M3 – Native Stereo-Welt

Lieferumfang:

- dokumentierter `RenderCamera`-/Renderpfad;
- Welt zweimal, Simulation einmal;
- per-eye Pose und IPD;
- zunächst symmetrische, danach wenn möglich asymmetrische Projektion;
- Kamera-Restore und Flat-Screen-Fallback.

Gate:

- Ein nahes und ein fernes Objekt zeigen korrekte Parallaxe.
- Linkes/rechtes Auge sind nicht vertauscht.
- Keine sichtbare doppelte Simulationsgeschwindigkeit.
- Save/Load, Slow-Mo, Partikel, Gegner-KI und Audio bleiben zeitlich korrekt.
- 15 Minuten Spiel ohne Deadlock oder stetigen Ressourcenanstieg.

### M4 – Headtracking und Komfort

Lieferumfang:

- Recenter und dokumentierte Achsenkonvertierung;
- HMD-Rotation;
- opt-in, begrenzte Translation;
- Head-Bob/Shake-Optionen;
- Cutscene-/Video-Komfortmodus;
- Stereo-HUD als erste Stufe.

Gate:

- Kopf links/rechts/oben/unten bewegt die Ansicht in der erwarteten Richtung.
- Kein künstliches Rollen bei normalem Laufen.
- Trackingverlust wird ohne Kamerasprung behandelt.

### M5 – OpenXR-Eingabe

Lieferumfang:

- Action Set und Bindings für verbreitete Interaction Profiles;
- Thumbsticks, Trigger, Grip, Tasten, Recenter und Haptik;
- Eingabe direkt im Clientpfad;
- Tastatur/Maus und Gamepad bleiben nutzbar.

Gate:

- Kein Stuck Input nach Fokusverlust.
- Controller können während einer Session getrennt/verbunden werden.
- Noch keine Behauptung „6DoF-Waffe“, solange Schuss- und Waffenrichtung nicht nachweislich übereinstimmen.

### M6 – Verpackung und Regression

Lieferumfang:

- lokaler Builder/Installer ohne Retail-Inhalte;
- `README.md` mit Voraussetzungen, Start, Deinstallation und bekannten Grenzen;
- `THIRD_PARTY_NOTICES.md`;
- Testmatrix und Performancezahlen;
- reproduzierbare x86-/x64-Artefakte.

Gate:

- Deinstallation entfernt nur Projekt-/Moddateien.
- Steam-Dateiprüfung ist nicht nötig, weil Retail unverändert blieb.
- Frische lokale Stage lässt sich allein aus Repo, legal installierter F.E.A.R.-Kopie, Public Tools und dokumentierten Abhängigkeiten erzeugen.

## 14. Tests

Automatisierte Tests:

- Protokollgrößen und -Offsets in x86 und x64;
- ungültige Magic/Version/Größe;
- Quaternionnormalisierung und Achsenabbildung;
- Pose relativ zum Recenter-Ursprung;
- FOV-Winkel und Projektionsmatrizen;
- Ringpuffer-Generationen und Timeoutpfade;
- OpenXR-State-Machine als testbare Logik ohne Headset;
- EXE-Hashprüfung;
- Stage-Pfad bleibt unter Projektwurzel;
- Retail-Hash vor/nach Vorbereitung und Live-Test.

Live-Testmatrix:

- SteamVR aus / an;
- Headset aktiv / Standby / Trackingverlust;
- Host startet vor/nach dem Spiel;
- Host wird während des Spiels beendet;
- Fenstermodus und Vollbild;
- Alt-Tab dreimal;
- Auflösungswechsel / D3D9 Reset;
- Hauptmenü, neues Spiel, Save/Load, Tod/Respawn;
- Slow-Mo und viele Partikel;
- Leiter, Lean, Knockdown und erste Zwischensequenz;
- mindestens ein 15-Minuten-Lauf;
- Debug und RelWithDebInfo.

Pro Live-Test erfassen:

- Host-/Proxy-/GameClient-Version;
- EXE-Hash;
- GPU und Adapter-LUIDs;
- OpenXR-Runtime-Name/-Version;
- Swapchainformat und -größe;
- Game-FPS und XR-Displayrate;
- dropped/reused frames;
- Renderzeit links/rechts und Host-Copyzeit;
- Ressourcen-/Handle-Anzahl am Anfang und Ende.

## 15. Performance- und Qualitätsregeln

- Zwei Welt-Renderdurchläufe kosten fast doppelte GPU-Zeit. Starte mit reduzierter Render Scale und deaktiviertem MSAA/Soft Shadows, bevor du optimierst.
- Nutze die von OpenXR empfohlenen View-Größen als Ziel, aber mache Render Scale konfigurierbar.
- Keine synchrone CPU-Rücklesung außer in einem expliziten Diagnosemodus.
- Ressourcen pro Reset/Session erzeugen, nicht pro Frame.
- Kein unbeschränktes Wachstum von Logs oder Captures.
- Host und Game dürfen unterschiedliche Frameraten haben; wiederverwende notfalls das letzte vollständige Stereopaar, niemals ein linkes Auge aus Frame N mit einem rechten aus Frame N+1.
- Profile erst nach korrektem Stereo. Keine Optimierung, die Parallaxe, Synchronisation oder Kamerakorrektheit verschlechtert.

## 16. Fehlerbehandlung

Jede Komponente muss klar melden:

- falsche F.E.A.R.-Version/Hash;
- fehlende oder nicht aktive OpenXR-Runtime;
- fehlendes `XR_KHR_D3D11_enable`;
- fehlgeschlagene XR-Session;
- unterschiedlicher XR-/D3D9-Adapter;
- nicht unterstütztes Shared-Texture-Format;
- Host-/Protokollversionskonflikt;
- Device Lost/Reset;
- Timeout bzw. Host-Abbruch.

Bei jedem Fehler gilt: VR deaktivieren, Ressourcen freigeben und nach Möglichkeit Flat-Screen fortsetzen. Kein Crash als normales Fallback.

## 17. Dokumentation der Entscheidungen

Halte in `docs/ARCHITECTURE.md` für jede wesentliche Entscheidung fest:

- Problem;
- getestete Optionen;
- Messung oder Quellcodebeleg;
- gewählte Lösung;
- bekannte Nachteile;
- Rückfallpfad.

Besonders zu dokumentieren:

- ob und wie `RenderCamera` zweimal sicher aufgerufen wird;
- HUD-Trennung;
- symmetrische/asymmetrische Projektion;
- D3D9/D3D11-Format und Synchronisation;
- Koordinatenkonversion;
- Verhalten bei CameraFX/Zwischensequenzen;
- Abgrenzung zu Motion-Controlled Aiming.

## 18. Referenzen

Primärquellen für die Implementierung:

- [Khronos OpenXR SDK](https://github.com/KhronosGroup/OpenXR-SDK)
- [Khronos OpenXR SDK Source und `hello_xr`](https://github.com/KhronosGroup/OpenXR-SDK-Source)
- [OpenXR 1.1 Specification](https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html)
- [OpenXR Loader: Windows- und 32/64-Bit-Runtime-Erkennung](https://registry.khronos.org/OpenXR/specs/1.1/loader.html)
- [Microsoft: `ID3D11Device::OpenSharedResource`, einschließlich D3D9-Interopbedingungen](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-opensharedresource)
- [Microsoft: Direct3D-9Ex-Ressourcenfreigabe](https://learn.microsoft.com/en-us/windows/win32/direct3d9/dx9lh)
- [Microsoft: Direct3D-9-Queries und GPU-Fertigstellung](https://learn.microsoft.com/en-us/windows/win32/direct3d9/queries)
- [MinHook](https://github.com/TsudaKageyu/minhook)
- [Microsoft: Visual C++ Build Tools](https://learn.microsoft.com/en-us/cpp/build/vscpp-step-0-installation)

Sekundäre Referenz, nicht ungeprüft übernehmen:

- [FEAR-MORE](https://github.com/SendoTarget/FEAR-MORE) zeigt einen aktuellen VS-2022/v141-x86-Build der offiziellen F.E.A.R.-1.08-Clientmodule und eine lokale, Retail-schonende Stagingstrategie. Beachte dessen komponentenspezifische MIT-/GPL-/proprietäre Lizenzgrenzen. Kopiere weder Code noch Binärdateien ohne konkrete Lizenzprüfung.

Die lokal vorhandenen offiziellen Public Tools 1.08 sind für F.E.A.R.-spezifische Client-APIs maßgeblicher als Vermutungen oder Binäranalyse. Lies deren Dokumentation und Quellcode zuerst.

## 19. Arbeitsweise und Abschlussbericht

Implementiere nicht alle Meilensteine in einem unprüfbaren Großschritt. Für jeden Meilenstein:

1. kurze Ausgangshypothese dokumentieren;
2. kleinsten beweisenden Spike bauen;
3. automatisierte Tests ergänzen;
4. Live-Test mit Log durchführen;
5. Ergebnis und offene Risiken dokumentieren;
6. nur selbst geschriebene bzw. zulässig verteilbare Dateien committen.

Der Abschlussbericht jedes Meilensteins nennt:

- was jetzt nachweislich funktioniert;
- welche Tests gelaufen sind;
- relevante Logs/Messwerte;
- bekannte Grenzen;
- den nächsten kleinsten Implementierungsschritt.

Behaupte „VR spielbar“ erst ab M4. Behaupte „Motion Controls“ erst ab M5. Behaupte „6DoF-Waffensteuerung“ erst, wenn sichtbare Waffenpose und tatsächliche Schuss-/Interaktionsrichtung in mehreren Waffen- und Save/Load-Tests übereinstimmen.
