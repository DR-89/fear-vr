# Public-Tools-Spielmodule bauen

## Ergebnis und wichtige ABI-Grenze

Der unveränderte F.E.A.R.-1.08-Clientquellstand aus den offiziellen Public
Tools baut auf diesem Rechner mit Visual Studio 2022, dem v141-Toolset und dem
Windows SDK 10.0.26100.0 erfolgreich als PE32/x86:

- `GameClient.dll`, Version `1.08.282.0`
- `GameServer.dll`, Version `1.08.282.0`
- `ClientFx.fxd`

Die proprietären SDK-Quellen und erzeugten Binärdateien bleiben unter
`vendor-local/` und werden nicht committet.

**Diese v141-Dateien sind nur Compile-Artefakte und dürfen nicht in F.E.A.R.
geladen werden.** Der Original-`GameClient.dll` importiert
`MSVCP71.dll`/`MSVCR71.dll` (Visual C++ 7.1), der v141-Build dagegen
`MSVCP140.dll`/`VCRUNTIME140.dll` und die UCRT. Ein isolierter Live-Test mit
nur dem neu gebauten `GameClient.dll` beendet F.E.A.R. reproduzierbar mit
einem Zugriffsschutzfehler in `MSVCR71.dll`. Die Originalmodule laufen mit
derselben Archivkonfiguration stabil.

F.E.A.R. tauscht C++-/CRT-Objekte über die Modulgrenze aus. Dafür reicht
Quellkompatibilität nicht; für ein auslieferbares GameClient-Modul ist eine
echte VC7.1-kompatible Toolchain einschließlich passender Header und
Bibliotheken erforderlich. Auf diesem Rechner ist sie nicht installiert.

## Voraussetzungen

- Public Tools 1.08 unter `vendor-local\publictools`
- Visual Studio 2022 mit C++-Workload
- MSVC v141 für Compile-/Quellanalyse
- Windows SDK 10.0.26100.0
- PowerShell 5.1 oder neuer
- für ein laufzeitfähiges Spielmodul: MSVC/CRT 7.1 (Visual Studio .NET 2003)

Die geprüften lokalen Versionen stehen in [ENVIRONMENT.md](ENVIRONMENT.md).

## Public Tools installieren

Der offizielle Installer liegt in der Steam-Installation:

```text
C:\Program Files (x86)\Steam\steamapps\common\FEAR Ultimate Shooter Edition\extras\fear_publictools_108.exe
```

SHA-256:

```text
11AAA4128528403F7BC9EA5119C68051C62B92A99E6411DFD749AF55E9B19DF8
```

Der alte Installer prüft nicht die tatsächliche Steam-EXE, sondern den
Registrywert:

```text
HKLM\SOFTWARE\WOW6432Node\Monolith Productions\FEAR\1.00.0000\Patch
```

Die Steam Ultimate Shooter Edition verwendet `Patch=10`; der Public-Tools-1.08-
Installer erwartet `Patch=8`. Falls der Installer die verifizierte
`FEAR.exe 1.08.282.0` deshalb ablehnt:

1. Registry-Schlüssel exportieren und aktuellen Wert notieren.
2. `Patch` nur für die Dauer der Installation auf `8` setzen.
3. nach `vendor-local\publictools` installieren;
4. den ursprünglichen Wert in einem `finally`-artigen Ablauf unbedingt
   wiederherstellen;
5. anschließend kontrollieren, dass `Patch=10` gesetzt ist.

Dieser Workaround wurde einmalig angewandt. Das Projekt automatisiert keine
Registryänderung und benötigt sie für spätere Builds nicht.

## Einmalige Solution-Migration

Der Installer liefert VS-2003-`.vcproj`-Dateien. Einmalig mit Visual Studio
2022 migrieren:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.com" `
  "C:\Users\<benutzer>\projects\F.E.A.R-VR\vendor-local\publictools\Source\Game\Game.sln" `
  /Upgrade
```

Die migrierten `.vcxproj`-Dateien bleiben lokal unter `vendor-local`.

## Reproduzierbarer Compile-Test

Der Wrapper wendet alle kleinen Kompatibilitätstransformationen idempotent an,
injiziert die modernen MSBuild-Einstellungen und prüft die drei Ergebnisse auf
PE32/x86 sowie ihre CRT-Imports. Der Build endet erfolgreich, kennzeichnet
v141-Ausgaben aber ausdrücklich als nicht deploybar:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\build-game-modules.ps1
```

Nur die Transformationen prüfen:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\build-game-modules.ps1 -VerifyFixesOnly
```

Der äquivalente rohe Buildbefehl lautet:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "C:\Users\<benutzer>\projects\F.E.A.R-VR\vendor-local\publictools\Source\Game\Game.sln" `
  /p:Configuration=Release `
  /p:Platform=Win32 `
  /p:PlatformToolset=v141 `
  /p:WindowsTargetPlatformVersion=10.0.26100.0 `
  /p:ForceImportAfterCppTargets="C:\Users\<benutzer>\projects\F.E.A.R-VR\patches\gameclient-build.props" `
  /m /nologo /verbosity:minimal
```

Artefakte:

```text
vendor-local\publictools\Source\built\Release\GameClient.dll
vendor-local\publictools\Source\built\Release\GameServer.dll
vendor-local\publictools\Source\built\Release\ClientFx.fxd
```

## Warum zwei Projektdateien nötig sind

`patches\apply-sdk-build-fixes.ps1` verändert nur eine lokal bereitgestellte
SDK-Quelle. Es:

- ergänzt den fehlenden direkten `ctype.h`-Include;
- trennt Makro-/Stringtokens, die moderne C++-Lexer sonst als ungültige
  Literal-Suffixe interpretieren;
- korrigiert einen alten for-Scope-Leak;
- ersetzt ein ungültiges leeres Wide-Character-Literal;
- verwendet `winres.h` statt des nicht benötigten MFC-Headers;
- entfernt zwei falsche `inline`-Deklarationen, damit `GetDecalType` als
  externes Symbol emittiert wird.

Das Skript akzeptiert nur den exakt erwarteten Original- oder Zielzustand.
Unbekannte Quellstände werden nicht bearbeitet.

`patches\gameclient-build.props` wird nach den C++-Targets importiert und:

- aktiviert das alte for-Scope-Verhalten für die übrigen SDK-Stellen;
- setzt die nötigen Legacy-CRT-/STL-Defines;
- bindet `legacy_stdio_definitions.lib` ein;
- gleicht moderne `TargetName`/`TargetPath`-Werte an die originalen
  Linkerausgaben an;
- entfernt ausschließlich die alten xcopy-`CustomBuildStep`-Items. Deployment
  erfolgt bewusst separat.

## M0-Stock-Lauftest und Deployment-Sperre

Die Public Tools enthalten lose Game-Ressourcen. `FEARDevSP.exe` kann mit der
Steam-Ausgabe nicht verwendet werden, da sie noch die alte CD/DVD-Prüfung
ausführt. Eine kopierte Steam-`FEAR.exe` scheitert an Steams Pfadprüfung.

Der Launcher verwendet deshalb den offiziellen Aufruf
`steam.exe -applaunch 21090`. Eine projektlokale `stage\m0-stock.archcfg`
enthält die unveränderte Retail-Archivliste und zuletzt das lose
Public-Tools-`Game`-Verzeichnis. Das Retail-Verzeichnis wird nur gelesen und
gehasht, nie beschrieben.

Ein VC7.1-kompatibles Modulset wird mit einmaligem, verifiziertem Backup
bereitgestellt:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\deploy-stock-game-modules.ps1
```

Der derzeitige v141-Build wird hier absichtlich mit
`ABI-SICHERHEITSABBRUCH` abgelehnt, bevor eine Datei kopiert wird.

Lauftest mit isoliertem Benutzerverzeichnis:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\launch-m0-stock.ps1
```

Originale Public-Tools-Module wiederherstellen:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\deploy-stock-game-modules.ps1 -Restore
```

Backup und Deploymentmanifest liegen unter `stage/` und sind gitignored.

Am 24.07.2026 wurden drei Kontrollläufe durchgeführt:

- Retail über Steam, isoliertes Benutzerverzeichnis: nach 15 Sekunden stabil;
- originale Public-Tools-Module über die lose Archivschicht: nach 15 Sekunden
  stabil;
- nur v141-`GameClient.dll`, die beiden anderen Module original:
  reproduzierbarer Prozessabbruch; Windows meldet `MSVCR71.dll`,
  Ausnahmecode `0xc0000005`.

Bis eine VC7.1-Toolchain verfügbar ist, muss die erste VR-Ausgabe über den
D3D9-Proxy und den separaten x64-OpenXR-Host erfolgen. GameClient-Änderungen
werden nicht mit v141 ausgeliefert.

## Bekannte Compilerwarnungen

Der alte SDK-Code erzeugt mit dem modernen Compiler weiterhin unter anderem
Konvertierungswarnungen, Warnungen zu veralteten for-Scope-Optionen und
fehlende historische `vc70.pdb`-Hinweise. Sie blockieren den Stock-Build nicht.
Neu geschriebener F.E.A.R.-VR-Code muss weiterhin warning-clean bleiben.
