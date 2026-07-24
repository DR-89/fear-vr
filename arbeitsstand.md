# Arbeitsstand F.E.A.R. VR — Übergabe

> Stand: 2026-07-24
> Sprache des Nutzers: Deutsch
> Referenzauftrag: `ANWEISUNG.md`
> Projektwurzel: `C:\Users\<benutzer>\projects\F.E.A.R-VR`

## 0. Kurzfassung

- M0-Umgebung und Projektgerüst sind vorhanden; x86- und x64-Gerüst bauen.
- F.E.A.R. 1.08 und die Public Tools 1.08 sind lokal verifiziert.
- Die alte VS-2003-Solution wurde nach VS2022 migriert. Alle drei
  Public-Tools-Spielmodule kompilieren und linken mit v141 erfolgreich.
- Sämtliche bisherigen Quell- und MSBuild-Fixes sind als idempotente Skripte
  im Repository reproduzierbar.
- **Wichtiger Live-Test-Befund:** Die v141-Module sind nicht ABI-kompatibel
  mit der VC7.1-F.E.A.R.-Runtime und dürfen nicht deployt werden.
- Die ursprünglichen Public-Tools-Module sind wiederhergestellt. Die
  Steam-/Retail-Installation wurde nie verändert.
- Der nächste unabhängige Implementierungsschritt ist M1 über den
  D3D9-Proxy und den separaten x64-OpenXR-Host. Änderungen am GameClient
  warten auf eine echte VC7.1-Toolchain.

## 1. Verifizierte Umgebung

| Element | Wert |
|---|---|
| Spielwurzel | `C:\Program Files (x86)\Steam\steamapps\common\FEAR Ultimate Shooter Edition` |
| `FEAR.exe` | Version `1.08.282.0`, PE32/x86 |
| `FEAR.exe` SHA-256 | `D5EBC38A4F12B772C9112A2811C290ADB6C5052D3BC2F817302D38CF55BB2CBE` |
| Steam-App-ID | `21090` |
| OpenXR | SteamVR x64 aktiv |
| 32-Bit-OpenXR | `WOW6432Node`-Runtime fehlt; separater x64-Host bleibt Pflicht |
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

## 8. Empfohlene nächste Schritte

1. **M1 unabhängig fortsetzen:** x64-OpenXR-Host implementieren und den
   OpenXR-Instanz-/System-/Session-/Frame-Lebenszyklus gegen SteamVR testen.
2. **M2 vorbereiten:** bestehenden x86-`d3d9.dll`-Proxy erweitern,
   Retail-D3D9 vollständig weiterleiten und zunächst nur Diagnose/IPC
   ergänzen. Dafür wird kein neu gebauter GameClient benötigt.
3. GPU-Adapter-LUID, IPC-Protokoll und Fail-open-Verhalten zwischen Proxy
   und Host verifizieren.
4. Vor GameClient-Änderungen eine rechtmäßig verfügbare VC7.1-Toolchain in
   einer isolierten historischen Buildumgebung einrichten. Keine
   inoffiziellen Compilerarchive ungeprüft herunterladen.
5. Erst ein VC7.1-Stockmodul durch den vorhandenen ABI-Guard und M0-Lauftest
   bringen; danach VR-Änderungen in den Clientquellen beginnen.

## 9. Repository-Regeln

- `vendor-local/` niemals committen.
- Retail-Verzeichnis immer read-only behandeln.
- Keine DRM-/CD-Prüfung umgehen; ausschließlich Steam und offizielle
  Mod-/Archivmechanismen nutzen.
- PowerShell-Dateien mit Umlauten als UTF-8 mit BOM speichern.
- „VR spielbar“ erst ab M4, „Motion Controls“ erst ab M5 behaupten.
