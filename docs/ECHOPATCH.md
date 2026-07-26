# EchoPatch neben F.E.A.R. VR

> **Stand 26.07.2026: wieder entfernt.** Zwei Startversuche, zwei Blockaden —
> beide unten unter „Warum es am Ende nicht zusammenpasst" belegt. Die
> Installation ist über `tools\install-echopatch.ps1 -Apply` jederzeit
> reproduzierbar wiederherstellbar; alles Weitere in diesem Dokument gilt
> unverändert für den Fall, dass jemand es erneut versuchen will.

[EchoPatch](https://github.com/Wemino/EchoPatch) von Wemino ist ein
Modernisierungspatch für F.E.A.R. Er wird hier bewusst mitbetrieben: seine
Bugfixes, High-FPS-Korrekturen und Startbeschleunigungen sind für den VR-Betrieb
ein Gewinn, seine Bild-, HUD- und Controllerfunktionen dagegen nicht.

Installiert: **4.2.1**, gepinnt über Größe und SHA-256 in `_fearvr-env.ps1`.

```bash
pwsh -File tools\install-echopatch.ps1            # Trockenlauf, zeigt nur den Stand
```

```bash
pwsh -File tools\install-echopatch.ps1 -Apply     # installieren
```

```bash
pwsh -File tools\install-echopatch.ps1 -Apply -Remove
```

## Warum das die einzige Ausnahme von „Retail bleibt unverändert" ist

EchoPatch ist ein `dinput8.dll`-Wrapper. Er muss neben `FEAR.exe` liegen, und
`FEAR.exe` startet bei uns aus dem Retail-Verzeichnis — über
`steam.exe -applaunch 21090`, weil das der offizielle Weg ist und die Steam-DRM
das so erwartet. Über `-archcfg` kommen nur die *Spielmodule* aus `stage\`; für
eine EXE-nahe Wrapper-DLL gibt es diesen Weg nicht.

Installiert werden deshalb genau zwei Dateien in die Retail-Installation:

| Datei | Herkunft |
|---|---|
| `dinput8.dll` | unverändert aus dem gepinnten Release |
| `EchoPatch.ini` | `patches\echopatch\EchoPatch.ini` aus diesem Repo |

`FEAR.exe` selbst wird **nicht** angefasst. Installer, Launcher und
Deinstaller prüfen ihren SHA-256 weiterhin vor und nach jedem Lauf und brechen
bei jeder Abweichung ab. `tools\uninstall-fearvr.ps1` entfernt beide Dateien
wieder; eine fremde `dinput8.dll` (Hash passt nicht zum Release) bleibt dabei
unangetastet.

## Die Einstellungen, die nicht verstellt werden dürfen

Diese vier Werte prüft `verify-install.ps1` bei jedem Lauf, und
`install-echopatch.ps1` verweigert die Installation, wenn die Vorlage davon
abweicht:

- **`CheckLAAPatch = 0`** — der einzige EchoPatch-Eingriff, der `FEAR.exe`
  *überschreibt* (Large Address Aware). Danach passt der SHA-256 nicht mehr und
  keine unserer Startprüfungen lässt das Spiel noch an.
- **`SDLGamepadSupport = 0`** — die wichtigste inhaltliche Abweichung. EchoPatch
  schreibt seine Gamepadbefehle in denselben `CBindMgr`-Kommandoraum, in den
  unser GameClient die OpenXR-Eingaben injiziert. Beide gleichzeitig wären zwei
  Schreiber auf denselben Kommandobits. Nebenbei bestätigt EchoPatchs
  Standardbelegung unsere Wahl: dort liegt `GAMEPAD_LEFT_STICK = 70`, also
  ebenfalls Medkit auf dem linken Stick-Klick.
- **`HUDScaling = 0`** — `stereo_hud_math.h` ist auf das unveränderte
  Retail-HUD-Layout kalibriert; jede Skalierung verschiebt die Stereo-Ablage.
- **`CustomFOV = 0`** und `SSAAScale = 1.0` — Projektion und Auflösung pro Auge
  setzt der VR-Pfad selbst.

Weitere Abweichungen vom EchoPatch-Standard, jeweils in der INI mit `FEARVR:`
begründet:

- `DynamicVsync = 0` — Vsync auf den Flachbildschirm würde das Spiel an dessen
  Bildrate koppeln und Latenz in die VR-Pipeline tragen.
- `AutoResolution = 0` — die Backbuffergröße bestimmt unsere Augentexturen und
  darf nicht unbemerkt beim Start wechseln.
- `NoLODBias = 0`, `EnablePersistentWorldState = 0` — beides kostet Bildrate,
  und der Weltrender läuft pro Bild zweimal (gemessen rund 50 fps). Wer Reserve
  hat, darf beides einschalten.
- `SmallTextCustomScalingFactor = 1.0` statt 1.2 — Untertitel bleiben im
  Retail-Layout.
- `DisableLetterbox = 1` — schwarze Balken sind auf einer Leinwand ein
  Stilmittel, im Headset zwei Blenden vor den Augen.
- `SkipAllIntro = 1` — jeder Testlauf begänne sonst mit Videos.

Aktiv bleiben die Gründe, aus denen EchoPatch hier überhaupt mitläuft:
`HighFPSFixes`, `WeaponFixes`, `FixScriptedAnimationCrash`,
`FixNvidiaShadowCorruption`, `OptimizeSaveSpeed`, `FastVRAMDetection`,
`DisableRedundantHIDInit`, `EnableCrashHandler`, `DisablePunkBuster`.

## Warum es am Ende nicht zusammenpasst

Zwei Startversuche am 26.07.2026, beide gescheitert — und beide Ursachen sind
strukturell, nicht durch eine weitere Einstellung zu beheben.

**1. Der Crash-Handler tötet absichtlich abgefangene Ausnahmen.**
`EnableCrashHandler` installiert einen *Vectored* Exception Handler, und der
läuft vor jedem SEH-Handler. Dieses Projekt sondiert Retail-Interna
grundsätzlich mit `__try/__except`: `ApplyHeadBobEnabled` etwa löst beim Start
eine Zugriffsverletzung aus, fängt sie ab und meldet
`headbob_configuration_failed` — nachweisbar in jedem früheren Lauf. Mit dem
VEH wurde daraus ein tödlicher Absturz beim Laden, `EIP=0`, `Source: VEH`,
exakt zwischen den Logzeilen `vr_features_disabled` und
`headbob_configuration_failed`. Mit `EnableCrashHandler = 0` startete das Spiel
wieder bis `stereo_hook_armed`.

**2. Signaturen, die es bei uns nicht gibt.** Danach brach EchoPatch mit
„Unable to find signature for patch: **SurfaceJumpImpulse**" ab — ein modaler
Dialog, der den Start blockiert. Der Quelltext ist nach Zielmodulen gegliedert
(`src/Engine`, `src/Client`, `src/Server`), EchoPatch patcht also sehr wohl die
Spielmodule und nicht nur die EXE. Genau die haben wir umgebaut: Der
Retail-Client heißt bei uns `GameOrig.dll`, weil unser Shim den Namen
`GameClient.dll` belegt. Welcher Schalter diese eine Signatur abschaltet, ließe
sich durch Ausprobieren finden — nur wäre die nächste fehlende Signatur eine
Frage der Zeit, und jeder Versuch kostet einen Spielstart mit Headset.

**Die Abwägung.** Fast alles, wofür EchoPatch bekannt ist — FOV, HUD-Skalierung,
SSAA, Gamepad, Gyro, Framerate-Limiter — mussten wir ohnehin abschalten, weil es
gegen den VR-Pfad arbeitet. Übrig blieben Bugfixes, die dieser Mod nie gebraucht
hat. Dem stand entgegen, dass die gesamte Hookstrategie auf einer byteweise
verifizierten `FEAR.exe` und festen RVAs beruht; ein zweiter Patcher, der
denselben Code zur Laufzeit umschreibt, untergräbt diese Prämisse und macht
jeden künftigen Absturz zu einer Untersuchung mit zwei Verdächtigen.

## Falls es jemand erneut versucht

- `EnableCrashHandler = 0` ist zwingend, sonst startet das Spiel gar nicht.
- Der Weg um Punkt 2 herum wäre, die Spielmodul-Patches gruppenweise
  abzuschalten (`WeaponFixes`, `HighFPSFixes`, `EnablePersistentWorldState`,
  `EnableCustomMaxWeaponCapacity`) und nur die reinen Engine-Patches zu
  behalten.
- **`FixAspectRatioBlur = 1` ist ungeprüft.** Der Fix rechnet mit einem
  Vollbild-Seitenverhältnis; wir rendern pro Auge.
- Der Launcher meldet nach dem Start `EchoPatch: aktiv`, sobald `dinput8.dll`
  aus dem Retail-Verzeichnis geladen wurde. Ohne EchoPatch lädt das Spiel die
  System-DLL; das ist der Normalfall und wird nicht gemeldet.
