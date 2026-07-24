# STEREO-RESEARCH.md — Zweifacher Welt-Renderpfad

> Ziel (ANWEISUNG.md §5.3): den vorhandenen Welt-Renderpfad **zweimal** (pro
> Auge) aufrufen, ohne die Simulation ein zweites Mal fortzuschreiben.
> Dieses Dokument sammelt die Quellcodebelege und Messungen, bevor der Pfad als
> gültig gilt.

> Status: **offen** — wird gefüllt, sobald die Public-Tools-Clientquellen lokal
> vorliegen (M0) und der Renderpfad analysiert ist.

## 1. Zu findende Stellen in den Clientquellen (§5.3)

- [ ] `OnRender` bzw. zentraler Client-Renderpfad
- [ ] `RenderCamera`
- [ ] `Start3D`, `End3D`, `FlipScreen`
- [ ] Player-Camera, CameraFX, `SetCameraFOV`
- [ ] HUD-/Interface-Rendering
- [ ] Camera Shake, Head Bob, Lean, Slow-Mo, Zwischensequenzzustände
- [ ] Input- und Waffenrichtungsberechnung

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

## 5. Ergebnis (auszufüllen)

- Gewählter Ansatz: _TBD_
- Beleg (Quellzeilen / Messung): _TBD_
- Nachweis „Simulation nur einmal": _TBD_
- Bekannte Nachteile / Rückfallpfad: _TBD_
