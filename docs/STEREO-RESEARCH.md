# STEREO-RESEARCH.md — Zweifacher Welt-Renderpfad

> Ziel (ANWEISUNG.md §5.3): den vorhandenen Welt-Renderpfad **zweimal** (pro
> Auge) aufrufen, ohne die Simulation ein zweites Mal fortzuschreiben.
> Dieses Dokument sammelt die Quellcodebelege und Messungen, bevor der Pfad als
> gültig gilt.

> Status: **M3 abgeschlossen** — native Stereo-Welt funktioniert mit der
> Steam-Retailfassung. Der Benutzer hat den 15-Minuten-Stabilitätstest am
> 24.07.2026 als bestanden akzeptiert; HUD, Menüs und Headtracking wurden
> anschließend in M4 ergänzt.

## 1. Zu findende Stellen in den Clientquellen (§5.3)

- [x] `OnRender` bzw. zentraler Client-Renderpfad
- [x] `RenderCamera`
- [x] `Start3D`, `End3D`, `FlipScreen`
- [x] Player-Camera, CameraFX, `SetCameraFOV`
- [x] HUD-/Interface-Rendering
- [x] Camera Shake, Head Bob und Zwischensequenz-Komfortpfad
- [x] Input- und Waffenrichtungsberechnung bleibt einmal pro Spiel-Frame
- [ ] Lean und Slow-Mo als getrennt protokollierte Detailregression

## 2. Angestrebter Frame-Ablauf (§5.3)

1. Aktuellen XR-Renderauftrag unmittelbar vor dem Rendern lesen.
2. Originale Kamera-Pose und FOV sichern.
3. Linke Augenpose relativ zum kalibrierten Ursprung anwenden.
4. **Nur** die 3D-Welt für links rendern und über die Bridge erfassen.
5. Rechte Augenpose anwenden.
6. **Nur** die 3D-Welt für rechts rendern und erfassen.
7. Kamera wiederherstellen.
8. HUD und Menüs **genau einmal** rendern.
9. Bildschirm **genau einmal** präsentieren.

## 3. Nachweis vor Gültigkeit

Anhand Quellcode **und** Laufzeittest ist zu beweisen, dass der zweite
`RenderCamera`-Aufruf **keine** Simulation, KI, Soundereignisse, Partikel-
alterung oder Eingabe doppelt ausführt (Gate M3).

## 4. Fallback-Reihenfolge, falls RenderCamera nicht doppelt aufrufbar ist

Nicht blind einen kompletten D3D9-Command-Replayer bauen. In dieser Reihenfolge
prüfen und die genaue Ursache hier dokumentieren:

1. Offizielle Render-Target-/Camera-APIs aus dem SDK.
2. Kleiner, **versionsgeprüfter** Hook um den Engine-Kamera-Renderaufruf.
3. Tiefe + Bild-Reprojektion **nur** als klar gekennzeichneter
   Kompatibilitätsmodus.

> D3D9-Depth-Hacks wie `INTZ`/`RESZ` sind herstellerabhängig und dürfen nicht
> die **einzige** Basis des Hauptpfads sein.

## 5. Quellcode- und ABI-Beleg

Der offizielle Public-Tools-Client trennt Aktualisierung und Welt-Render:

- `GameClientShell.cpp:4071-4104` führt PreRender, Spezialeffekt-, Shatter-,
  ClientFX- und Streaming-Updates vor dem eigentlichen Kamera-Render aus.
- `GameClientShell.cpp:4124-4134` ruft zuerst
  `CPlayerCamera::Render()` und zeichnet danach dynamische FX und Interface.
- `PlayerCamera.cpp:417-419` leert den Render-Target und ruft genau einmal
  `ILTRenderer::RenderCamera(m_hCamera)` auf.
- `iltrenderer.h:353-355` definiert diesen Ein-Argument-Aufruf als Alias für
  `RenderCamera(hCamera, NULL)`.

Damit liegt der kleinste sichere Hook **unterhalb** aller Client-Updates und
oberhalb des reinen Engine-Welt-Renders. Nur dieser Engine-Aufruf wird pro Auge
wiederholt; `CGameClientShell::RenderCamera`, PlayerCamera-Pre/Post-Logik,
Simulation, Eingabe, Audio und Interface laufen weiterhin einmal pro Frame.

Die Deklarationsreihenfolge der Overloads entspricht in der Retail-VC7.1-VTable
nicht der Reihenfolge im Header. Eine read-only Laufzeitprobe am 24.07.2026
ergab:

```text
slot 17:
8B542404 8B01 6A00 52 FF504C C20400
mov edx,[esp+4]; mov eax,[ecx]; push 0; push edx;
call [eax+0x4c]; ret 4

slot 19:
8B442408 8B542404 50 6A00 6A00 6A00 52 ...
```

`0x4c / 4 = 19`: Retail-Slot 17 ist der Ein-Argument-PlayerCamera-Alias und
leitet mit `techniqueOverride=nullptr` an Slot 19 weiter. Der M3-Loader prüft
diese 15-Byte-Weiterleitung vor dem Patch. Bei einer abweichenden EXE bleibt
die VTable unangetastet.

## 6. Gewählte M3-Lösung

1. F8 ersetzt ausschließlich Retail-VTable-Slot 17.
2. Pose und FOV der Spielkamera werden gesichert.
3. Die Kamera wird um die halbe OpenXR-IPD nach links verschoben, Slot 19 wird
   aufgerufen und der Backbuffer als linkes Auge erfasst.
4. Dasselbe geschieht nach rechts für das rechte Auge.
5. Pose und FOV werden auch bei einer strukturierten Ausnahme wiederhergestellt.
6. Erst `Present` übergibt das vollständige Augenpaar an den Host.
7. F8 stellt den originalen Slot atomar wieder her; ohne Host oder gültigen
   Renderauftrag bleibt der Mono-/Flat-Pfad aktiv.

## 7. Live-Nachweis vom 24.07.2026

Lauf `logs\m3-fear-20260724-162315`:

- Quest 3 / SteamVR-OpenXR, Swapchains je `1624x1736`;
- D3D9- und OpenXR-Adapter-LUID stimmen überein;
- `stereo_hook_installed`, `player_rendercamera_called`,
  `stereo_render_active` und fortlaufende `stereo_frame_staged`-Ereignisse;
- rund 11½ Minuten Prozesslaufzeit und mindestens 24.900 vollständige
  Stereo-Frames;
- mehrere Auflösungs-/Device-Resets und wiederholtes F8-Umschalten ohne
  Absturz;
- keine `stereo_render_exception` und kein Mono-Fallback während des
  kontinuierlichen Welt-Renders;
- Benutzerbestätigung: Ego-Steuerung funktioniert, Welt ist klar erkennbar und
  hat korrekte 3D-Tiefenwirkung; der weitere Spieltest lief ohne erkennbaren
  Fehler;
- normales Spielende, anschließend kontrolliertes
  `game_disconnected`-Timeout im Host.

Der erste gemessene stabile Lauf wurde nach rund 11½ Minuten beendet. Der
anschließende Spieltest verlief laut Benutzer vollständig gut; er hat den
15-Minuten-Stabilitätsnachweis am 24.07.2026 ausdrücklich als bestanden
akzeptiert. Lean und Slow-Mo bleiben als getrennt protokollierbare
Detailregressionen erhalten, blockieren das abgeschlossene M3-Gate aber nicht.

## 8. In M4 geschlossene Grenze: HUD und Menü

Die Augenbilder werden unmittelbar nach dem Welt-`RenderCamera` erfasst.
`GameClientShell.cpp:4131-4149` zeichnet dynamische FX, Interface, Debug-/HUD-
Elemente und Konsole erst danach. M4 vergleicht deshalb das endgültige
Present-Bild mit dem rechten Weltbild und übernimmt nachträglich gezeichnete
HUD-Pixel identisch in beide Augen. Größere Vollbildänderungen — insbesondere
Menüs und Zwischensequenzen — werden automatisch als lesbares, raumfestes
OpenXR-Quad dargestellt.

Der Benutzer bestätigte Fadenkreuz und HUD in Stereo, das raumfeste Hauptmenü
und funktionierende Übergänge. Der derzeitige CPU-Readback des HUD-Prototyps
bleibt eine bekannte M6-Optimierungsaufgabe; `-NoStereoHud` ist der
Rückfallpfad.
