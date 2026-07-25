# OpenXR-Eingabe — M5

> Status: **M5-Spielsteuerung aktiv**. OpenXR-Controllerzustände erreichen den
> Retail-Client über einen versionierten, fail-open IPC-Pfad. Bewegung,
> Waffensteuerung, haptisches Feedback und die native Menübedienung sind aktiv.

## Architektur

```text
OpenXR Actions (x64 Host)
  → FearVrInputState / Protokoll v3
  → Shared-Memory-Seqlock
  → x86-D3D9-Bridge
  → FearVr_GetInputState
  → GameClient-Loader, IClientShell::Update Slot 20
```

Haptik läuft getrennt in Gegenrichtung:

```text
GameClient → FearVrHapticRequest → Bridge → Host → xrApplyHapticFeedback
```

Maus, Tastatur und bestehendes Gamepad werden weder entfernt noch umgebunden.
Fehlt der Host, ist die Probe älter als 250 ms oder verliert OpenXR den Fokus,
neutralisiert der Client alle VR-Zustände.

## Action Set und Profile

Das Action Set `gameplay` enthält:

- linken Bewegungs-Stick beziehungsweise Trackpad;
- rechten Dreh-Stick beziehungsweise Trackpad;
- Trigger und Grip je Hand;
- Primär-, Sekundär-, Menü- und Stick-Tasten je Hand;
- Haptikausgabe je Hand.

Vorgeschlagene Interaction Profiles:

- Oculus Touch Controller;
- Valve Index Controller;
- Microsoft Motion Controller;
- HTC Vive Controller;
- KHR Simple Controller.

Die Zustände bleiben im Transport zunächst physisch und werden erst im
GameClient semantisch auf F.E.A.R.-Befehle abgebildet. Dadurch kann kein
fehlerhaftes Profil unbemerkt Bewegung oder Feuer auslösen.

## Spielbelegung

- linker Stick: Bewegen; linker Grip: Rennen; linker Stick-Klick: Pause;
- rechter Stick links/rechts: Drehen; hoch/runter: Waffenwechsel;
- A: Springen; B: Nachladen; X: Ducken; Y: Zeitlupe;
- rechter Grip: Benutzen; rechter Trigger: Feuern; linker Trigger: Fokus;
- rechter Stick-Klick: Headtracking-Recenter;
- linke Hand seitlich neigen: um die Ecke lehnen.

Maus, Tastatur und vorhandenes Gamepad bleiben parallel nutzbar. Der rechte
Feuer-Trigger erzeugt einen kurzen Haptikimpuls.

### Lehnen über die Handneigung

`COMMAND_ID_LEAN_LEFT` (20) und `COMMAND_ID_LEAN_RIGHT` (21) sind digitale
Retail-Kommandos, die `CLeanMgr` selbst weich ein- und ausblendet. Die Auswahl
kommt aus der Rolllage der linken **Aim**-Pose um deren eigene Vorwärtsachse:
`PoseRollRadians` liest dazu die Welt-Hoch-Anteile der lokalen X- und Y-Achse,
was unabhängig von Blickrichtung und Handhöhe funktioniert.

- Schwelle: 0,42 rad (~24°), deutlich über der Neigung beim normalen Zielen.
- Positive Rolllage (Handoberseite nach links) lehnt nach links.
- Ohne gültige linke Aim-Pose ist die Rolllage 0, das Lehnen löst sich also
  bei Trackingverlust, statt hängen zu bleiben.

Zwei Grenzfälle mussten zusätzlich abgefangen werden, beide im echten Lauf
`logs\m5-fear-20260724-235013` aufgetreten:

- **Umgedrehte Hand.** Eine Rolllage nahe ±180° erfüllt einen reinen
  Schwellenvergleich ebenfalls. Im Log lösten 177,3°, 136,3° und −169,3°
  fälschlich ein volles Lehnen aus. Es gilt deshalb zusätzlich eine Obergrenze
  von 1,75 rad (~100°): Die Hand darf geneigt, aber nicht gedreht sein.
- **Hängende Hand.** Zeigt die Aim-Pose steil nach unten, verschwinden die
  Welt-Hoch-Anteile beider Achsen und die Rolllage wird numerisch bedeutungslos
  — genau der Grund für die obigen Ausreißer. `PoseLevelness` liefert
  `|cos(pitch)|`; unterhalb von 0,5 (mehr als 60° aus der Waagerechten) wird
  gar nicht mehr gelehnt.

Die Erkennung sitzt in `LeftHandLeanDirection` und liefert −1, 0 oder +1.
`tests/test_controller_mapping.cpp` deckt beide Grenzfälle ab.

Benutzerabnahme am 24.07.2026: Recenter und Haptik funktionieren; die übrigen
ausgeführten Diagnosepunkte passen. Der zunächst ausgelassene Trackingverlust
wurde anschließend nachgeholt: Der Host meldete `active_hands=0x0` und 229 ms
später wieder `active_hands=0x3`. Beide Controller wurden ohne Neustart wieder
erkannt.

## VR-Einstellungen im Pausenmenü

`VR SETTINGS` wird in der verifizierten Retail-1.08-Klasse
`CMenuSystem` direkt nach „Optionen“ ergänzt. Die Seite verwendet dieselbe
native Menüliste und ist deshalb mit Tastatur und VR-Controller bedienbar:
Stick navigiert, A oder Trigger bestätigt, B geht zurück.

Die Seite ist bewusst kurz und einseitig, damit kein Eintrag über den Rand des
nativen 320px-Rahmens läuft und kein unsauberes Scrollen entsteht:

1. Stereo rendering
2. Stereo HUD
3. Turn speed
4. Red aim guide
5. Controller vibration
6. Recenter view
7. Reset VR defaults
8. BACK

HMD-Translation, Head-Bob und Komfortbildschirm bleiben als Einstellungen
erhalten und werden weiterhin aus `fearvr.ini` gelesen und dorthin
geschrieben, stehen aber nicht auf der sichtbaren Seite. Ein zweistufiges Menü
wurde verworfen.

### Warum die Auswahl gesprungen ist

Jeder Umschalter besteht aus zwei Controls (`… : ON` und `… : OFF`), von denen
immer eines versteckt ist. Das ist für die Navigation unkritisch, weil
`CLTGUICtrl::IsEnabled()` als `m_bEnabled && IsVisible()` definiert ist und
`CLTGUIListCtrl::NextSelection` unsichtbare Einträge damit korrekt überspringt.

Der Fehler steckt in `CLTGUIListCtrl::SetSelection`: Beim Herunterscrollen
bestimmt es den neuen Listenanfang, indem es rückwärts die `GetBaseHeight()`
aller Controls aufsummiert — **ohne** `IsVisible()` zu prüfen.
`CalculatePositions()` überspringt unsichtbare Controls dagegen sehr wohl.
Beide Rechnungen widersprechen sich, sobald versteckte Geschwister-Controls
dazwischenliegen, und `m_nFirstShown` wird falsch gesetzt.

Die verkürzte VR-Seite passt vollständig in den nativen Rahmen. Der
Listenanfang wird deshalb bei 0 festgehalten, solange die Seite aktiv ist —
und zwar in jedem Client-Update, weil Tastatur, Maus und Controller alle
direkt über `NextSelection` navigieren und nicht über den eigenen Hook.

Änderungen werden sofort angewendet und in `stage/userdata-m5/fearvr.ini`
gespeichert.

Zusätzlich in `fearvr.ini`, ohne Menüeintrag:

- `HiddenBodyPieces` — Bitmaske der ausgeblendeten Player-Body-Pieces.
  Standard `2`: Piece #1 trägt die Arme, Hände und Waffe bleiben sichtbar.
  **F11** kalibriert den Wert im Spiel neu, indem es die Pieces einzeln
  isoliert.
