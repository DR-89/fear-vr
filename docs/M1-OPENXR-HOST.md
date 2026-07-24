# M1 — OpenXR-/D3D11-Host

## Ergebnis

`fearvr-host.exe` ist ein eigenständiger x64-Prozess. Er:

- lädt die systemweit registrierte OpenXR-Runtime über den statisch gelinkten
  Khronos-Loader;
- fordert `XR_KHR_D3D11_enable` und OpenXR 1.0 an;
- liest über `xrGetD3D11GraphicsRequirementsKHR` die benötigte Adapter-LUID;
- erzeugt das D3D11-Gerät ausschließlich auf diesem Adapter;
- verwaltet Instance, HMD-System, Session, Local-Space und Session-States;
- erzeugt zwei getrennte Stereo-Swapchains;
- führt pro Frame `xrWaitFrame`, `xrBeginFrame`, `xrLocateViews` und
  `xrEndFrame` aus;
- rendert für M1 links rot und rechts blau;
- schreibt JSON-Lines nach `logs/host-YYYYMMDD-HHMMSS.log`.

Das ist bewusst noch keine F.E.A.R.-Integration. Spielbilder und IPC folgen
ab M2/M3.

## Abhängigkeiten

```powershell
pwsh -File tools\prepare-dependencies.ps1
pwsh -File tools\prepare-dependencies.ps1 -VerifyOnly
```

Das Skript prüft:

- OpenXR-SDK `release-1.1.59`,
  Commit `e5df31de6c15b4900aee3092273194e51282000d`;
- OpenXR-SDK-Source `release-1.1.59` als `hello_xr`-Referenz,
  Commit `04e92820192a6eec490e5eb8ffbd8211bafb0551`.

Beide Checkouts liegen unter `vendor-local/` und werden nicht committet.

## Build und Tests

```powershell
cmake -S . -B build\x64 -G "Visual Studio 17 2022" -A x64 `
  -DFEARVR_BUILD_HOST=ON -DFEARVR_BUILD_PROXY=OFF `
  -DFEARVR_BUILD_TESTS=ON -DFEARVR_WARNINGS_AS_ERRORS=ON
cmake --build build\x64 --config RelWithDebInfo
ctest --test-dir build\x64 -C RelWithDebInfo --output-on-failure
```

## Verwendung

SteamVR und das Headset zuerst vollständig verbinden:

```powershell
# Nur Initialisierung bis einschließlich beider Swapchains
build\x64\src\host64\RelWithDebInfo\fearvr-host.exe --validate-only

# 120 Stereo-Testframes, danach geordneter Session-Abbau
build\x64\src\host64\RelWithDebInfo\fearvr-host.exe --max-frames 120
```

Weitere Optionen:

- `--log-dir <Pfad>`
- `--d3d-debug`
- `--help`

Exitcodes:

- `0`: erfolgreich;
- `2`: ungültige Kommandozeile;
- `10`: keine OpenXR-Runtime erreichbar;
- `11`: kein betriebsbereites HMD;
- `12`: anderer OpenXR-Fehler;
- `13`: D3D11-/Hostinitialisierungsfehler.

## Live-Nachweis vom 2026-07-24

- Runtime: SteamVR/OpenXR `2.16.7`
- HMD: Quest 3 / `oculus`, Positions- und Orientierungstracking aktiv
- GPU: NVIDIA GeForce RTX 3050 Laptop GPU, LUID `0x0:C91C`
- Swapchains: links und rechts je `1624x1736`,
  `DXGI_FORMAT_R8G8B8A8_UNORM_SRGB`
- 120 Stereo-Frames erfolgreich eingereicht
- Zustände:
  `IDLE → READY → SYNCHRONIZED → VISIBLE → FOCUSED → VISIBLE →`
  `SYNCHRONIZED → STOPPING → IDLE → EXITING`

## Manueller Test vom 2026-07-24

Der Benutzer bestätigte im Headset:

- linkes Auge rot, rechtes Auge blau;
- beide Bilder kehren nach Absetzen/Wiederaufsetzen zurück;
- eine Steam-Link-Unterbrechung und Wiederverbindung wird ohne Absturz
  überstanden.

Das Hostlog `host-20260724-113657.log` belegt zweimal
`FOCUSED → VISIBLE → FOCUSED`, rund 20.400 eingereichte Frames und einen
sauberen `host_stop`.

Der Unit-Test `test_xr_session_state` deckt außerdem
`XR_SESSION_LOSS_PENDING` und die erneute READY-/BeginSession-Sequenz ohne
Headset ab. SteamVR hielt bei der Link-Unterbrechung dieselbe OpenXR-Session
am Leben und sein normaler `-shutdown`-Befehl wurde bei aktiver Anwendung
nicht ausgeführt. Ein echter, von der Runtime gemeldeter
`XR_SESSION_LOSS_PENDING`-Wechsel bleibt deshalb als letzter Live-Test offen.
