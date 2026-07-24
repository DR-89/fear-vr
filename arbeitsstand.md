# Arbeitsstand F.E.A.R. VR — Übergabe

> Stand: 2026-07-24
> Sprache des Nutzers: Deutsch
> Referenzauftrag: `ANWEISUNG.md`
> Projektwurzel: `C:\Users\<benutzer>\projects\F.E.A.R-VR`

## 0. Kurzfassung

- M0-Umgebung und Projektgerüst sind vorhanden; x86 und x64 bauen sauber.
- Der eigenständige M1-x64-Host besitzt einen vollständigen
  OpenXR-/D3D11-Lebenszyklus und sendet Stereo-Testbilder an SteamVR.
- OpenXR-SDK 1.1.59 ist lokal fest gepinnt; der Host fordert für die
  Kompatibilität mit SteamVR OpenXR 1.0 an.
- Der Live-Test mit Quest 3 reichte zunächst 120 Frames ein und beendete die
  Session sauber. Ein zusätzlicher manueller Lauf überstand Standby und
  Steam-Link-Neuverbindung über rund 20.400 Frames.
- F.E.A.R. 1.08 und die Public Tools 1.08 sind lokal verifiziert.
- Die alte VS-2003-Solution wurde nach VS2022 migriert. Alle drei
  Public-Tools-Spielmodule kompilieren und linken mit v141 erfolgreich.
- Sämtliche bisherigen Quell- und MSBuild-Fixes sind als idempotente Skripte
  im Repository reproduzierbar.
- **Wichtiger Live-Test-Befund:** Die v141-Module sind nicht ABI-kompatibel
  mit der VC7.1-F.E.A.R.-Runtime und dürfen nicht deployt werden.
- Die ursprünglichen Public-Tools-Module sind wiederhergestellt. Die
  Steam-/Retail-Installation wurde nie verändert.
- M2 ist als x86-D3D9/x64-D3D11-Monobrücke implementiert. D3D9Ex läuft
  GPU-direkt; klassisches D3D9 verwendet vorläufig einen markierten
  CPU-D3D9Ex-Kompatibilitätspfad.
- Der synthetische Cross-Bitness-Livetest bestand inklusive Adapter-LUID,
  Ringpuffer, Minimieren/Wiederherstellen, Auflösungs-Reset, Spielende und
  erzwungenem Host-Abbruch.
- Eine Retail-schonende echte F.E.A.R.-Stage über die offizielle
  `-archcfg`-Modulschicht ist vorhanden. Loader, Originalmodul und Bridge
  wurden im laufenden Spiel verifiziert.
- **M2-Live-Nachweis bestanden:** Der Benutzer sah das normale
  F.E.A.R.-Menü in beiden Augen; Maus und Tastatur reagierten normal. Nach dem
  Schließen des SteamVR-Desktop-Overlays blieb das Spielbild sichtbar.
- **M3 abgeschlossen:** Die Welt wird pro Auge nativ mit eigener Kamera
  gerendert. Der Benutzer bestätigte korrekte 3D-Tiefe und akzeptierte den
  15-Minuten-Stabilitätstest.
- **M4 abgeschlossen:** HMD-Rotation, F9-Recenter, optionale begrenzte
  Translation, Stereo-HUD, raumfeste Menüs, automatische Übergänge und
  Komfortmodus funktionieren im echten Spiel. Der Benutzer bestätigte alle
  Achsen, die verbesserte Latenz, HUD-Anordnung, Menüs und Übergänge.
- Der M4-Launcher unterdrückt die verzögert eingeblendete
  SteamVR-Theaterfläche; der Benutzer bestätigte den finalen Lauf.
- Bekannte Restgrenzen: Der klassische D3D9-Pfad und der HUD-Prototyp nutzen
  CPU-Readbacks; ein physischer vollständiger Trackingverlust wurde nicht
  separat provoziert. Beides ist dokumentiert und blockiert das M4-Gate nicht.

## 1. Verifizierte Umgebung

| Element | Wert |
|---|---|
| Spielwurzel | `C:\Program Files (x86)\Steam\steamapps\common\FEAR Ultimate Shooter Edition` |
| `FEAR.exe` | Version `1.08.282.0`, PE32/x86 |
| `FEAR.exe` SHA-256 | `D5EBC38A4F12B772C9112A2811C290ADB6C5052D3BC2F817302D38CF55BB2CBE` |
| Steam-App-ID | `21090` |
| OpenXR | SteamVR x64 aktiv |
| 32-Bit-OpenXR | `WOW6432Node`-Runtime fehlt; separater x64-Host bleibt Pflicht |
| OpenXR-SDK | `release-1.1.59`, Commit `e5df31de6c15b4900aee3092273194e51282000d` |
| M1-Live-HMD | Quest 3 über Steam Link, SteamVR/OpenXR `2.16.7` |
| M1-Runtime-GPU | NVIDIA GeForce RTX 3050 Laptop GPU, LUID `0x0:C91C` |
| Public Tools | `vendor-local\publictools` (proprietär, gitignored) |

Weitere Details: `docs/ENVIRONMENT.md`.

## 2. Installierte Toolchain

- Visual Studio 2022 Community 17.14.37
- MSVC v143 `14.44.35207`
- MSVC v141 `14.16.27023`
- Windows SDK `10.0.26100.0`
- CMake `4.4.0`

Nicht installiert ist Visual C++ 7.1 / Visual Studio .NET 2003. Diese
Toolchain ist für ein laufzeitfähiges, neu gebautes GameClient-Modul nötig.
v141 bleibt für Quellanalyse und Compile-Tests nützlich.

## 3. Public Tools und Installer

Installer:

```text
C:\Program Files (x86)\Steam\steamapps\common\FEAR Ultimate Shooter Edition\extras\fear_publictools_108.exe
```

SHA-256:

```text
11AAA4128528403F7BC9EA5119C68051C62B92A99E6411DFD749AF55E9B19DF8
```

Der Installer akzeptiert die Steam-Ausgabe nur, wenn der Registrywert
`HKLM\SOFTWARE\WOW6432Node\Monolith Productions\FEAR\1.00.0000\Patch`
vorübergehend von `10` auf `8` gesetzt wird. Das wurde einmalig mit Backup
durchgeführt und danach auf `10` zurückgestellt. Das Projekt automatisiert
diese Registryänderung nicht.

## 4. Reproduzierbarer Public-Tools-Build

Wrapper:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\build-game-modules.ps1
```

Nur Quellfixes prüfen:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\build-game-modules.ps1 -VerifyFixesOnly
```

Relevante Dateien:

- `patches/apply-sdk-build-fixes.ps1`
- `patches/gameclient-build.props`
- `tools/build-game-modules.ps1`
- `docs/BUILD-GAMECLIENT.md`

Gelöste Buildblocker:

1. fehlender `ctype.h`-Include;
2. alte Stringliteral-/Makroverkettungen;
3. VS-2003-for-Scope-Verhalten;
4. ungültiges leeres Wide-Character-Literal;
5. MFC-`afxres.h` durch `winres.h` ersetzt;
6. `legacy_stdio_definitions.lib`;
7. falsche `inline`-Deklarationen von
   `CGameModelDecalMgr::GetDecalType`;
8. alte xcopy-`CustomBuildStep`-Items entfernt;
9. moderne TargetName/TargetPath-Werte an die ursprünglichen Ausgaben
   angepasst.

Letzte erfolgreiche v141-Compile-Artefakte:

| Datei | Bytes | SHA-256 |
|---|---:|---|
| `GameClient.dll` | 3.337.216 | `F3573C38C259487C1D67638A3ACB67904A00BD4B5561675968CB85ADF440E723` |
| `GameServer.dll` | 5.593.088 | `B66DCBB5BD0616C0DB0312BEE20F3472D712BD9D897B8E97538D1AF1BE3F23D0` |
| `ClientFx.fxd` | 510.464 | `4EA6AEF3F4FC5F1C16F9583E4C5C033C0250C3A5493D41B841A711926958B38C` |

Alle drei sind PE32/x86. Sie sind dennoch **nicht deploybar**.

## 5. ABI-Befund und Live-Tests

Importvergleich:

| Modulset | C++-/CRT-Imports |
|---|---|
| originale Public-Tools-Module | `MSVCP71.dll`, `MSVCR71.dll` |
| VS2022/v141-Build | `MSVCP140.dll`, `VCRUNTIME140.dll`, UCRT |

F.E.A.R. tauscht C++-/CRT-Objekte über DLL-Grenzen aus. Die MSVC-
Binärkompatibilitätsgarantie reicht nicht bis VC7.1 zurück. Am 24.07.2026
wurden folgende kontrollierte Starts ausgeführt:

1. Retail über `steam.exe -applaunch 21090` mit isoliertem
   `-userdirectory`: nach 15 Sekunden stabil.
2. Retail plus originale `Default.archcfg`: nach 15 Sekunden stabil.
3. Retail plus lose Public-Tools-`Game`-Schicht und originale Module:
   nach 15 Sekunden stabil.
4. Derselbe Lauf mit ausschließlich neu gebautem `GameClient.dll`:
   reproduzierbarer Abbruch. Windows meldete `MSVCR71.dll`,
   Ausnahmecode `0xc0000005`; ein nachfolgender WER-Eintrag nannte
   `CoreUIComponents.dll`.

Frühere Startprobleme sind ebenfalls erklärt:

- `FEARDevSP.exe` verlangt noch ein physisches CD/DVD-Laufwerk.
- Eine kopierte Steam-`FEAR.exe` endet mit Steam Application Load Error
  `5:0000065434`.
- Richtig ist der offizielle Steam-Start mit App-ID `21090`.

`tools/deploy-stock-game-modules.ps1` prüft deshalb vor jedem Kopieren, ob
ein Modul `MSVCR71.dll` und `MSVCP71.dll` importiert. Der aktuelle v141-Build
wird mit `ABI-SICHERHEITSABBRUCH` abgelehnt. Der Test bestätigt außerdem, dass
bei der Ablehnung keine Runtime-Datei verändert wird.

## 6. Sicherer aktueller Dateizustand

- Retail-`FEAR.exe` ist unverändert und hat weiterhin den erwarteten Hash.
- Unter `vendor-local\publictools\Dev\Runtime\Game` liegen wieder die
  ursprünglichen Public-Tools-Module.
- Original-`GameClient.dll` SHA-256:
  `B5F1F1976227FD0E6F1C32BD2BEEDFB117E68A87A07BB42D06BE489DD08A63BA`.
- Das einmalige Backup liegt unter
  `stage\m0-stock-module-backup` und ist gitignored.
- Testbenutzerdaten und Deploymentmanifeste liegen ausschließlich unter
  `stage`.

## 7. Start-/Deploymenttechnik

`tools/deploy-stock-game-modules.ps1`:

- verifiziert Retail-EXE, PE32/x86 und VC7.1-CRT-Imports;
- sichert die Originalmodule genau einmal;
- erzeugt `stage\m0-stock.archcfg` aus Retail-`Default.archcfg` plus loser
  Public-Tools-`Game`-Schicht;
- schreibt ein hashgebundenes Deploymentmanifest;
- verändert das Retail-Verzeichnis nicht.

`tools/launch-m0-stock.ps1`:

- verifiziert EXE, Archivkonfiguration und Module;
- startet offiziell über `steam.exe -applaunch 21090`;
- setzt ein isoliertes `-userdirectory`;
- erkennt den tatsächlichen `FEAR.exe`-Prozess.

Der Launcher ist erst wieder für einen vollständigen M0-Lauf zu verwenden,
wenn ein VC7.1-kompatibles Modulset vorliegt.

## 8. M1 — OpenXR-/D3D11-Host

Implementiert:

- fest gepinntes Khronos OpenXR-SDK `release-1.1.59`;
- statischer offizieller OpenXR-Loader;
- `XR_KHR_D3D11_enable`, Graphics-Requirements und exakte Adapter-LUID;
- D3D11-Gerät, OpenXR-Session, Local-Space und zwei getrennte Swapchains;
- Frame-Lebenszyklus mit `wait/begin/locate/end`;
- links rotes, rechts blaues Stereo-Testbild;
- Zustandsautomat für READY, STOPPING, EXITING und LOSS_PENDING;
- JSON-Lines-Logs und klare Diagnose ohne Runtime;
- reproduzierbarer Abhängigkeitsprüfer
  `tools/prepare-dependencies.ps1`.

Live-Nachweis am 24.07.2026:

1. `--validate-only` bestand gegen SteamVR/OpenXR 2.16.7 und Quest 3.
2. Exakter Runtime-Adapter:
   NVIDIA GeForce RTX 3050 Laptop GPU, LUID `0x0:C91C`.
3. Zwei Swapchains mit je `1624x1736`,
   `DXGI_FORMAT_R8G8B8A8_UNORM_SRGB`.
4. 120 Stereo-Frames und sauberer Übergang über STOPPING bis EXITING.
5. Fehlende Runtime künstlich geprüft: verständliche Diagnose, Exitcode 10.
6. x86 und x64 bauen mit `/W4 /WX`; je zwei CTest-Tests bestehen.

Wichtiger Live-Test-Fund: Das aktuelle OpenXR-SDK definiert
`XR_CURRENT_API_VERSION` als 1.1. SteamVR 2.16.7 beantwortete diese
Anforderung mit `XR_ERROR_API_VERSION_UNSUPPORTED`. Wie das aktuelle
Khronos-`hello_xr` fordert der Host deshalb explizit `XR_API_VERSION_1_0` an.

Noch offen für das vollständige M1-Gate:

- echten `XR_SESSION_LOSS_PENDING`-Wechsel live provozieren und die bereits per
  Unit-Test geprüfte Session-Neuerstellung beobachten. Steam Link hielt beim
  manuellen Verbindungsabbruch dieselbe OpenXR-Session am Leben.

Zusätzlicher manueller Nachweis am 24.07.2026:

- Benutzer bestätigt: linkes Auge rot, rechtes Auge blau;
- Absetzen/Wiederaufsetzen:
  `FOCUSED → VISIBLE → FOCUSED`, Bilder danach wieder sichtbar;
- Steam-Link-Unterbrechung/Wiederverbindung ohne Hostabsturz;
- rund 20.400 Frames, anschließend sauberer `host_stop`.

Details: `docs/M1-OPENXR-HOST.md`.

## 9. Empfohlene nächste Schritte

1. M5 mit Motion-Controller-Eingabe beginnen, ohne die bestehende
   Maus-/Tastatursteuerung zu beeinträchtigen.
2. Den HUD-Differenzpfad und den klassischen D3D9-Kompatibilitätspfad für M6
   GPU-seitig umsetzen und die markierten CPU-Readbacks entfernen.
3. Lean, Slow-Mo und einen physisch erzwungenen Trackingverlust als gezielte
   Regressionen protokollieren.
4. Die M4-Komfortoptionen anhand längerer geskripteter Kamerafahrten weiter
   abstimmen.

## 10. Repository-Regeln

- `vendor-local/` niemals committen.
- Retail-Verzeichnis immer read-only behandeln.
- Keine DRM-/CD-Prüfung umgehen; ausschließlich Steam und offizielle
  Mod-/Archivmechanismen nutzen.
- PowerShell-Dateien mit Umlauten als UTF-8 mit BOM speichern.
- „VR spielbar“ erst ab M4, „Motion Controls“ erst ab M5 behaupten.

## 11. M2 – D3D9-/OpenXR-Monobrücke

Implementiert:

- vollständige dokumentierte D3D9-Weiterleitung aus absolut geladenem
  `%SystemRoot%\SysWOW64\d3d9.dll`;
- Hooks für `Direct3DCreate9[Ex]`, `CreateDevice[Ex]`, `Reset` und `Present`;
- früher Proxy-/VTable-Pfad sowie späte MinHook-Detours für ein bereits vor
  dem GameClient-Loader erzeugtes D3D9-Gerät;
- drei Shared-Texture-Slots pro Auge, GPU-`StretchRect` und
  `D3DQUERYTYPE_EVENT`;
- Protokollversion 2 mit 432-Byte-x86/x64-Header, Heartbeats, Prozess-IDs,
  Adapter-LUID, Seqlock, Frame-ID und Generation;
- D3D11-`OpenSharedResource`, private Textur je Auge, D3D11-Event-Query und
  Fullscreen-Shader in die OpenXR-Swapchains;
- GPU-direkter D3D9Ex-Pfad ohne CPU-Kopie;
- explizit markierter CPU-D3D9Ex-Kompatibilitätspfad für das klassische
  D3D9-Gerät des echten Spiels;
- Adapter-Mismatch und Host-Ausfall bleiben fail-open;
- isolierter Farbgenerator und reproduzierbare Normal-/Host-Abbruch-Tests;
- sichere echte `-archcfg`-Stage ohne Retail-Schreibzugriff und ohne
  Remote-Thread-Injection.

Live-Nachweis des isolierten Bridgepfads am 24.07.2026:

- NVIDIA GeForce RTX 3050 Laptop GPU, LUID `0x0:C91C` auf beiden Seiten;
- 960×540 vor und 800×450 nach erfolgreichem Reset;
- Frame/Generation 1 sowie 300 frisch importiert;
- Producer lief nach erzwungenem Host-Abbruch bis Frame 600 weiter;
- keine Hänger bei Minimieren, Reset, Host-Abbruch oder Prozessende.

Live-Nachweis mit echtem F.E.A.R. am 24.07.2026:

- Stage-Loader, originales VC7.1-GameClient-Modul und Bridge im laufenden
  Prozess verifiziert;
- späte `Present`-/`Reset`-Hooks aktiv;
- 1024×768-Texturen beider Augen über dieselbe NVIDIA-LUID importiert;
- Benutzer bestätigte sichtbares F.E.A.R.-Menü und normale
  Maus-/Tastaturbedienung;
- SteamVR-Desktop-Overlay erfolgreich geschlossen.

Gate-Einschränkung:

- Der echte klassische D3D9-Pfad enthält weiterhin einen per-Frame-CPU-Readback
  und erfüllt damit noch nicht die finale Produktionsinvariante.
- Zum damaligen M2-Stand erklärten Monobild, fehlendes Headtracking und flaches
  HUD, warum er noch nicht angenehm nutzbar war.
- M2 gilt als funktionaler Techniknachweis; der inzwischen abgeschlossene
  M4-Stand erfüllt die Bezeichnung „VR spielbar“.

Relevante Befehle:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\test-m2-bridge.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\test-m2-bridge.ps1 -AbortHost
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\prepare-m2-stage.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\launch-m2-fear.ps1
```

Details: `docs/M2-D3D9-BRIDGE.md`.

## 12. M3/M4 — natives Stereo und spielbarer VR-Modus

M3 ruft ausschließlich den reinen Retail-`RenderCamera`-Slot zweimal auf.
Simulation, Eingabe, Audio und Interface laufen weiterhin genau einmal pro
Spiel-Frame. F8 schaltet den Hook fail-open; Pose und Kamera werden auch bei
Fehlern wiederhergestellt. Der Benutzer bestätigte die native 3D-Wirkung und
den Stabilitätstest.

M4 ergänzt:

- HMD-Rotation auf allen Achsen und F9-Recenter;
- optionale, auf 25 cm begrenzte Translation;
- zur tatsächlich gerenderten Pose passende OpenXR-Views für besseres
  Timewarp;
- Stereo-Fadenkreuz und Stereo-HUD mit angepasster ergonomischer Position;
- raumfestes Menü-/Vollbild-Quad und automatische Übergänge;
- `-NoHeadBob` sowie F10-Flatscreen-Komfortmodus;
- einen pro Spielstart begrenzten Wächter gegen die
  SteamVR-Desktop-Theaterfläche.

Der Benutzer bestätigte den M4-Gesamtstand am 24.07.2026 mit „top, passt“.
Build-, Protokoll- und Mathematiktests sind in x86 und x64 grün. Details:
`docs/TESTING.md`, `docs/ARCHITECTURE.md`, `docs/STEREO-RESEARCH.md` und
`docs/COORDINATE-SYSTEM.md`.
