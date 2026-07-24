# ENVIRONMENT.md — Verifizierter Umgebungszustand

> Erzeugt und geprüft am **2026-07-24** auf dem Zielrechner.
> Grundlage: ANWEISUNG.md §2 und §4.
> Diese Werte werden zu Beginn **jeder** Arbeitssitzung erneut geprüft
> (`tools/verify-install.ps1`). Bei abweichender `FEAR.exe` darf **kein**
> versionsabhängiger Hook aktiviert werden — dann Abbruch mit Diagnose.

## 1. Verifizierter Ausgangszustand (§2)

Alle Werte wurden am 2026-07-24 live gegen den Rechner geprüft und stimmen
mit ANWEISUNG.md §2 überein.

| Element | Erwartet (§2) | Gemessen 2026-07-24 | Status |
|---|---|---|---|
| Projektwurzel | `C:\Users\<benutzer>\projects\F.E.A.R-VR` | identisch | ✅ |
| Spielwurzel | `C:\Program Files (x86)\Steam\steamapps\common\FEAR Ultimate Shooter Edition` | identisch | ✅ |
| Basis-EXE | `FEAR.exe` | vorhanden | ✅ |
| Dateiversion | `1.08.282.0` | `1.08.282.0` | ✅ |
| Produktversion | — | `1.08.282.0` | ℹ️ |
| Architektur | PE32 / x86 (`Machine 0x014C`) | PE-Signatur `PE\0\0`, Machine `0x014C` | ✅ |
| SHA-256 `FEAR.exe` | `D5EBC38A4F12B772C9112A2811C290ADB6C5052D3BC2F817302D38CF55BB2CBE` | identisch | ✅ |
| Renderer-Import | `d3d9.dll` / `Direct3DCreate9` | beide im Importstring vorhanden | ✅ |
| Eingabe-Import | `DINPUT8.dll` / `DirectInput8Create` | beide im Importstring vorhanden | ✅ |
| Engine | LithTech Jupiter EX | (Dokumentation) | ℹ️ |
| Standard-Benutzerdaten | `C:\Users\Public\Documents\Monolith Productions\FEAR` | (Standardpfad) | ℹ️ |
| `-userdirectory`-Support | laut lokalem `readme.txt` vorhanden | wird in M0-Stage verifiziert | ⏳ |
| Public-Tools-Installer | `...\extras\fear_publictools_108.exe` | vorhanden | ✅ |
| SDK-Installer-Größe | `671441087` Bytes | `671441087` | ✅ |
| SHA-256 SDK-Installer | `11AAA4128528403F7BC9EA5119C68051C62B92A99E6411DFD749AF55E9B19DF8` | identisch | ✅ |
| Aktive OpenXR-Runtime (x64) | SteamVR `steamxr_win64.json` | `HKLM\SOFTWARE\Khronos\OpenXR\1\ActiveRuntime` → `...\SteamVR\steamxr_win64.json` | ✅ |
| 32-Bit-OpenXR-Registrierung | `HKLM\SOFTWARE\WOW6432Node\Khronos\OpenXR\1` **fehlt** | `ActiveRuntime`-Wert nicht vorhanden | ✅ (fehlt wie erwartet) |
| Git | vorhanden | `C:\Program Files\Git\cmd\git.exe` | ✅ |
| CMake | nicht gefunden | nicht gefunden | ✅ (fehlt) |
| Ninja | nicht gefunden | nicht gefunden | ✅ (fehlt) |
| `cl` (MSVC) | nicht gefunden | nicht gefunden | ✅ (fehlt) |
| MSBuild | nicht gefunden | nicht gefunden | ✅ (fehlt) |

Legende: ✅ bestätigt · ⏳ später zu verifizieren · ℹ️ dokumentiert, nicht messbar

> Hinweis: Die Build-Tools (CMake/`cl`/MSBuild) waren zum Zeitpunkt dieses
> Ausgangs-Snapshots noch nicht installiert. Der aktuelle, installierte Stand
> steht in Abschnitt 2.

### Architektur-Konsequenz (§2 / §5)

Der 32-Bit-OpenXR-Runtime-Eintrag unter `WOW6432Node` **fehlt** und SteamVR
stellt lokal nur ein x64-Manifest (`steamxr_win64.json`) bereit. Der
OpenXR-Loader verlangt für eine 32-Bit-Anwendung auf 64-Bit-Windows genau
diesen `WOW6432Node`-Eintrag. Daraus folgt die vorgeschriebene
Standardarchitektur:

> **Ein separater x64-OpenXR-Host (`fearvr-host.exe`) besitzt die
> OpenXR-Session.** OpenXR wird **nicht** dauerhaft direkt in die x86-`FEAR.exe`
> eingebaut. Siehe `docs/ARCHITECTURE.md`.

## 2. Build-Toolchain (§4)

> **Am 2026-07-24 auf ausdrücklichen Wunsch des Benutzers installiert.** Die
> Installation lief einmalig erhöht (UAC bestätigt), still über winget, ohne
> Retail-Dateien zu berühren. Verifiziert über `vswhere` und einen echten
> x86-/x64-Build des Gerüsts (siehe §4 dieses Dokuments).

### Zwingend erforderlich (Stand nach Installation)

| Komponente | Zweck | Status | Benötigt ab |
|---|---|---|---|
| Visual Studio 2022 Community `17.14.37` | IDE, Compiler/Linker/MSBuild | ✅ installiert | M0 |
| MSVC x86- und x64-Toolset (v143 `14.44.35207`) | Proxy (x86) + Host (x64) | ✅ (cl.exe Host x64/x86) | M0 |
| Windows 10/11 SDK `10.0.26100.0` | D3D9/D3D11, DirectXMath, DXGI | ✅ installiert | M1/M2 |
| Toolset **v141** `14.16.27023` | offizieller Clientquellcode, nur Compile-/Quellanalyse | ✅ installiert; Live-Test nicht ABI-kompatibel | M0-Diagnose |
| Toolset **VC7.1** (VS .NET 2003) | laufzeitfähiger GameClient mit MSVCP71/MSVCR71 | ❌ nicht installiert | spätere GameClient-Änderungen |
| CMake (Kitware `4.4.0`) | Buildsystem (Win32 + x64 getrennt) | ✅ `C:\Program Files\CMake\bin` | M0 |
| Public Tools 1.08 (aus lokalem Installer, nach `vendor-local`) | offizielle Client-APIs / GameClient-Quellen | ✅ installiert und Buildquellen verifiziert | M0-Diagnose |

> Hinweis: `cl.exe`/`MSBuild.exe` liegen bewusst **nicht** auf dem globalen PATH;
> sie werden über die VS-Umgebung bzw. den CMake-„Visual Studio 17 2022"-Generator
> gefunden. `verify-install.ps1` erkennt VS/MSVC/v141 daher über `vswhere`.

### Bereits vorhanden

| Komponente | Status |
|---|---|
| Git | ✅ `C:\Program Files\Git\cmd\git.exe` |
| SteamVR als aktive OpenXR-Runtime (x64) | ✅ |
| Angeschlossenes Headset für Live-Tests | ⏳ zur Live-Test-Zeit zu prüfen |
| Lokaler offizieller Public-Tools-Installer | ✅ (Hash bestätigt) |

### Als kleine, festgeschriebene Abhängigkeiten vorgesehen (§4)

Werden nach `vendor-local/` bzw. via CMake-`FetchContent` mit **festem
Commit/Tag** eingebunden und in `THIRD_PARTY_NOTICES.md` dokumentiert:

| Abhängigkeit | Zweck | Pin |
|---|---|---|
| Khronos OpenXR-SDK / statischer Loader | x64-Host | `release-1.1.59` / `e5df31de6c15b4900aee3092273194e51282000d` |
| MinHook | gezielte x86-Hooks (falls COM-Wrapper nicht reicht) | TBD |
| DirectXMath | Mathe (aus Windows SDK) | via Windows SDK |

### Optionale Entwicklungswerkzeuge (§4)

Nicht installiert; bei Bedarf und nur nach Rückfrage:
VS Graphics Diagnostics, apitrace (D3D9), RenderDoc (erst auf D3D11-Hostpfad),
Ghidra (nur wenn eine Funktion nicht über offizielle Clientquellen erreichbar ist).

**Bewusst vermieden (§4):** dgVoodoo2, ReShade, RTX Remix und weitere
`d3d9.dll`-Proxys zu Beginn. Mehrere Wrapper erschweren die Fehleranalyse.

## 3. Prüfbefehle (Reproduktion)

```powershell
# EXE-Hash + Version
$exe = "C:\Program Files (x86)\Steam\steamapps\common\FEAR Ultimate Shooter Edition\FEAR.exe"
(Get-FileHash -Algorithm SHA256 $exe).Hash
(Get-Item $exe).VersionInfo.FileVersion

# Aktive OpenXR-Runtime (x64) und fehlender 32-Bit-Eintrag
(Get-ItemProperty "HKLM:\SOFTWARE\Khronos\OpenXR\1").ActiveRuntime
Get-ItemProperty "HKLM:\SOFTWARE\WOW6432Node\Khronos\OpenXR\1" -ErrorAction SilentlyContinue

# Build-Tools
foreach ($t in "git","cmake","ninja","cl","msbuild") { Get-Command $t -ErrorAction SilentlyContinue }
```

Bequemer über den mitgelieferten Prüfer:

```powershell
pwsh -File tools\verify-install.ps1
```

## 4. Verifikation durch echten Build (2026-07-24)

Die installierte Toolchain wurde nicht nur über `vswhere` geprüft, sondern durch
einen tatsächlichen Build des Gerüsts in **beiden** Architekturen:

```powershell
cmake -S . -B build\x64 -G "Visual Studio 17 2022" -A x64   -DFEARVR_BUILD_HOST=ON  -DFEARVR_BUILD_PROXY=OFF
cmake --build build\x64 --config RelWithDebInfo      # -> fearvr-host.exe, test_protocol.exe
cmake -S . -B build\x86 -G "Visual Studio 17 2022" -A Win32 -DFEARVR_BUILD_PROXY=ON -DFEARVR_BUILD_HOST=OFF
cmake --build build\x86 --config RelWithDebInfo      # -> d3d9.dll, fearvr-launcher.exe, test_protocol.exe
```

Ergebnis: beide Builds **warning-clean** (`/W4`), `test_protocol` besteht in
x64 (`ptr=64 bit`) und x86 (`ptr=32 bit`) → `protocol.h` ist in beiden
Architekturen layout-identisch.

## 5. OpenXR-Livezustand (M1, 2026-07-24)

- SteamVR/OpenXR `2.16.7`
- Headset: Quest 3 über Steam Link (`oculus`, Vendor-ID `10462`)
- Runtime-GPU: NVIDIA GeForce RTX 3050 Laptop GPU, LUID `0x0:C91C`
- Stereo-Swapchains: zweimal `1624x1736`, Format `DXGI_FORMAT_R8G8B8A8_UNORM_SRGB`

Der x64-Host initialisiert Instance, HMD-System, den exakt von der Runtime
verlangten D3D11-Adapter, Session, Local-Space und beide Swapchains erfolgreich.
Ein Live-Lauf reichte 120 Stereo-Frames ein und durchlief anschließend
`FOCUSED → VISIBLE → SYNCHRONIZED → STOPPING → IDLE → EXITING` ohne Fehler.
