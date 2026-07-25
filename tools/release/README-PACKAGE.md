# F.E.A.R. VR

VR-Mod für die Singleplayer-Basisversion von **F.E.A.R. 1.08**
(LithTech Jupiter EX, Direct3D 9). Natives Stereo-Rendering, Headtracking und
OpenXR-Motion-Controls.

## Voraussetzungen

1. **F.E.A.R. Ultimate Shooter Edition**, legal installiert, Version
   **1.08.282.0**. Der Mod prüft Version und SHA-256; bei Abweichung bleiben
   alle versionsabhängigen Hooks deaktiviert.
2. **F.E.A.R. Public Tools 1.08.** Der offizielle Installer
   `fear_publictools_108.exe` liegt der Ultimate Shooter Edition unter
   `extras\` bei.
3. Ein Headset mit **SteamVR** oder **Virtual Desktop**. Beide Runtimes sind
   bestätigt.
4. Windows 10/11, 64 Bit.

### Hinweis zur Installation der Public Tools

Der Installer prüft
`HKLM\SOFTWARE\WOW6432Node\Monolith Productions\FEAR\1.00.0000\Patch` und
erwartet dort den Wert **8**, während Steam **10** setzt. Für die Installation
muss der Wert vorübergehend auf 8 stehen und danach wieder auf 10. Ohne diesen
Schritt lehnt der Installer die Steam-Fassung ab.

Diese fünf Module sind proprietär und dürfen dem Paket nicht beiliegen. Sie
werden bei der Installation aus **deiner eigenen** Public-Tools-Installation
kopiert:

`GameClient.dll`, `GameServer.dll`, `ClientFx.fxd`, `FEAR.dep`,
`FEARMod.Arch00s`

## Installation

```powershell
powershell -ExecutionPolicy Bypass -File tools\install.ps1
```

Standardziel ist `%USERPROFILE%\FearVR`.

> **Nicht unterhalb von `%LOCALAPPDATA%` installieren.** Die Engine scheitert
> dort beim Laden der Archivkonfiguration mit „Failed to initialize client —
> unable to load game resources". Der Installer lehnt solche Ziele deshalb ab.

Optionen:

```powershell
tools\install.ps1 -InstallDir "D:\Spiele\FearVR"
tools\install.ps1 -InstallDir "C:\Users\<Name>\FearVR"
tools\install.ps1 -RetailRoot "D:\Steam\steamapps\common\FEAR Ultimate Shooter Edition"
tools\install.ps1 -PublicToolsGame "C:\FEAR Public Tools\Dev\Runtime\Game"
tools\install.ps1 -NoShortcut
```

Retail und Public Tools werden automatisch gesucht und über ihre Hashes
verifiziert.

## Spielen

Desktop-Verknüpfung **F.E.A.R. VR** oder:

```powershell
powershell -ExecutionPolicy Bypass -File tools\play.ps1
```

Optionen:

```powershell
tools\play.ps1 -Runtime vdxr      # Virtual Desktop erzwingen
tools\play.ps1 -Runtime steamvr   # SteamVR erzwingen
tools\play.ps1 -Translation       # begrenzte HMD-Translation (opt-in)
tools\play.ps1 -NoHeadBob         # Head-Bob zwingend aus (Standard bereits aus)
tools\play.ps1 -NoStereoHud       # nur zur Fehlersuche
```

`-Runtime` setzt `XR_RUNTIME_JSON` nur für den Hostprozess. Die systemweite
Runtime-Einstellung wird nicht verändert.

Steam wird zum Starten benötigt, weil F.E.A.R. offiziell über
`steam.exe -applaunch 21090` startet. Das ist unabhängig davon, welche
VR-Runtime rendert; SteamVR selbst muss unter Virtual Desktop nicht laufen.

## Steuerung

| Eingabe | Funktion |
|---|---|
| linker Stick | Bewegen |
| linker Grip | Rennen |
| linker Stick-Klick | Pausenmenü |
| rechter Stick links/rechts | Drehen |
| rechter Stick hoch/runter | Waffenwahl |
| rechter Stick-Klick | Blickrichtung zentrieren |
| rechter Grip | Benutzen |
| Trigger | Zielen und Feuern |
| A / B / X / Y | Springen / Nachladen / Ducken / Zeitlupe |
| linke Hand seitlich neigen | um die Ecke lehnen |

Tastatur: **F8** Stereo an/aus, **F9** zentrieren, **F10** raumfester
Komfortbildschirm, **F11** Player-Body-Pieces neu kalibrieren.

Maus, Tastatur und Gamepad bleiben parallel nutzbar. Die VR-Optionen stehen
im ESC-Menü unter **VR SETTINGS**.

Die linke System-/Menütaste ist nicht belegbar: SteamVR fängt sie für sein
eigenes Systemmenü ab.

## Deinstallation

```powershell
powershell -ExecutionPolicy Bypass -File tools\uninstall.ps1          # Trockenlauf
powershell -ExecutionPolicy Bypass -File tools\uninstall.ps1 -Apply   # ausführen
```

**Spielstände bleiben erhalten.** Sie liegen in `<InstallDir>\userdata` und
werden nur mit `-IncludeUserData` entfernt.

Die Retail-Installation wird zu keinem Zeitpunkt beschrieben. Eine
Steam-Dateiprüfung ist nicht nötig; die Installation gilt Steam gegenüber als
unverändert.

## Bekannte Grenzen

- Der klassische D3D9-Pfad braucht ein CPU-Readback pro Frame, ebenso der
  Stereo-HUD-Mischer. Das ist ein Techniknachweis, kein Performancepfad.
- HMD-Translation hat keine Weltkollision und bleibt deshalb opt-in.
- Die Hooks gelten für F.E.A.R. **1.08.282.0**. Bei abweichendem Hash bleiben
  sie aus, und das Spiel läuft flach weiter.
- Der Waffen-Sprung beim Treppensteigen ist nicht abschließend geklärt.
- „Motion-Controlled Aiming" ist über Zielstrahl und Trefferpunkt belegt; eine
  allgemeine „6DoF-Waffe" wird nicht behauptet.

## Lizenz

Die eigenen Bestandteile stehen unter der **MIT-Lizenz** (siehe `LICENSE`).
Für die Abhängigkeiten gilt `THIRD_PARTY_NOTICES.md`.

Dieses Paket enthält **keine** Retail-Dateien, keine proprietären SDK-Quellen
und keine extrahierten Assets. Zum Betrieb sind eine eigene, legal erworbene
F.E.A.R.-Installation und der offizielle Public-Tools-Installer erforderlich.
