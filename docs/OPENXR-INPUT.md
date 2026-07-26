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

- linker Stick: Bewegen; linker Grip: Rennen — oder, mit der Hand an der
  Waffe, sie mit der linken Hand mithalten;
- rechter Stick links/rechts: Drehen; hoch: Springen; runter: Ducken
  (jeweils ab 80 % Ausschlag);
- A: Waffenwechsel; B kurz: Nachladen; B gehalten: Granate werfen;
- X: Zeitlupe; Y: Pause;
- rechter Grip: Benutzen; rechter Trigger: Feuern; linker Trigger: Fokus;
- rechter Stick-Klick: Headtracking-Recenter;
- linker Stick-Klick: Medkit benutzen (`COMMAND_ID_MEDKIT` = 70). Retail wertet
  ihn an der steigenden Flanke in `CInterfaceMgr::OnCommandOn` aus, ein
  gehaltener Klick verbraucht deshalb genau einen Medkit. Damit ist keine freie
  Taste mehr übrig — weitere Aktionen bräuchten eine Geste oder einen
  langen Druck;
- linke Hand seitlich neigen: um die Ecke lehnen.

Maus, Tastatur und vorhandenes Gamepad bleiben parallel nutzbar.

### Linkshänderbelegung

`Controls: RIGHT-HANDED / LEFT-HANDED` im VR-Menü spiegelt die komplette
Belegung: Waffe, Feuern, Benutzen und Waffenwechsel wandern in die linke Hand,
Bewegung, Taschenlampe, Lehnen, Zeitlupe und Pause in die rechte.

Gespiegelt wird an genau einer Stelle — `MirrorInputHandedness` tauscht direkt
nach dem Abholen im `FearVrInputState` beide Sticks, Trigger, Grips, Tastenbits,
Handmasken und die verfolgten Posen. Stromabwärts bleibt die Waffenhand
unverändert `FEARVR_HAND_RIGHT`, weshalb Waffenausrichtung, Handknoten,
Zweihandgriff, Lehnen und die Befehlszuordnung ohne eigenen Handfall
mitspiegeln. Zurückgespiegelt werden muss nur die Handmaske der
Haptikanforderung, weil sie wieder einen physischen Controller adressiert.
`tests/test_input_state.cpp` prüft den Tausch und dass zweimaliges Spiegeln den
Ausgangszustand ergibt.

Beim Umschalten wird recentert, weil Handkalibrierung und gepufferte Posen
danach der jeweils anderen Hand gehören. Persistiert als `LeftHanded` in
`fearvr.ini`.

**Grenze:** Das Retail-Spielermodell trägt die Waffe im `RightHand`-Socket.
Sichtbar bleibt deshalb ein rechtes Handmodell an der Waffe, auch wenn es dem
physisch linken Controller folgt. Da Ober- und Unterarme ohnehin ausgeblendet
sind, fällt das wenig auf; das Modell tatsächlich zu spiegeln wäre ein
Eingriff in die Retail-Animationen.

Jeder abgefeuerte Schuss erzeugt einen kurzen Haptikimpuls, auch im
Dauerfeuer. Auslöser ist der Retail-Aufruf der Fire-Vectors, den beide
Feuerpfade genau einmal pro Schuss ausführen — nicht die Triggerflanke. Damit
vibriert der Controller bei leerem Magazin korrekt gar nicht.

### Zeigen statt hinsehen: Aktivieren und Aufnehmen

Retail sucht Schalter und Items entlang der Kamerablickrichtung. In VR ist das
die Kopfrichtung, was weder zum Zeigen noch zum Aufsammeln taugt — Items lagen
bisher nur beim Draufstehen im Zugriff. Beide Suchen folgen deshalb jetzt der
Waffenmündung, also demselben Strahl, den auch die rote Zielhilfe zeichnet:
Der rechte Grip aktiviert, worauf gezeigt wird, und nimmt auf, worauf gezeigt
wird. Die Reichweite beträgt rund 1,5 m.

Herleitung der dafür verwendeten Retail-Adressen, Speicherlayouts und die
Absicherung gegen abweichende Binärversionen: `docs/RETAIL-ACTIVATION.md`.

### Zweihandgriff: die Waffe mit der linken Hand mithalten

Der linke Grip greift die Waffe, sobald die linke Hand tatsächlich dort liegt,
wo ein Vordergriff wäre. Gemessen wird der Versatz der linken Grip-Pose im
Zielrahmen der rechten Aim-Pose (`LeftHandSupportOffset`, alles in Metern in
OpenXR-LOCAL, also unabhängig von Blickrichtung und Spielbasis):

- 0,05 m bis 0,60 m vor der Waffenhand — dahinter ist es kein Vordergriff,
  davor hat keine F.E.A.R.-Waffe mehr etwas zum Anfassen;
- höchstens 0,22 m seitlich von der Zielachse;
- Grip ab 0,65 gedrückt, Lösen erst unter 0,45. Die Hysterese verhindert das
  Flattern zwischen Rennen und Mithalten am Druckpunkt.

Greift die Hand daneben oder ins Leere, bleibt derselbe Knopf das Rennen. Ist
die Waffe gegriffen, gehören linker Grip **und** linke Handneigung der Waffe:
Rennen und Lehnen sind für die Dauer des Griffs gelöst, sonst liefen beide
dauerhaft mit, sobald man beidhaendig zielt. Zum Sprinten die Stützhand also
abnehmen.

**Wirkung auf das Zielen.** Die Linie zwischen beiden Händen übernimmt die
Feuerachse anteilig, gewichtet nach Waffenlänge — eine Pistole bleibt allein an
der rechten Hand, ein Gewehr liegt in beiden. Als Länge dient der gemessene
Abstand zwischen Waffenursprung und Mündungssockel (`muzzleOffsetInWeapon`,
derselbe Wert, aus dem auch die Mündungskorrektur entsteht); er ist der
einzige Waffenkennwert, den wir ohne Retail-Waffendatenbank verlässlich haben.
Die Rampe läuft von 0 bei 0,28 m auf 0,85 ab 0,55 m (`TwoHandedAimBlend`).

Gedreht wird über die kürzeste Drehung zwischen alter und neuer Vorwärtsachse.
Damit bleibt die Neigung der rechten Hand erhalten — die Waffe kippt in der
Hand, statt sich um die eigene Achse zu verdrehen. Zwei Sicherungen begrenzen
das: unter 0,12 m Handabstand ist die Linie nur noch Trackingrauschen, und
weicht sie mehr als 50° von der Zielachse ab, hält die Hand offensichtlich
nicht dieselbe Waffe. In beiden Fällen bleibt die Feuerachse unverändert.

Die Korrektur sitzt in `ApplyTwoHandedAimSupport` und läuft im
Weapon-Manager-Update **nach** der Mündungskorrektur, also auf der fertigen
Feuerachse. Sichtbare Waffe, rechter Handknoten, Zielhilfe und Geschossbahn
benutzen dieselbe Transformation und bleiben deshalb deckungsgleich.
Abschaltbar über `TwoHandGrip=0` in `fearvr.ini`.
`tests/test_two_handed_grip.cpp` deckt Erkennung, Hysterese und Gewichtung ab.

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
6. Controls: RIGHT-HANDED / LEFT-HANDED
7. Recenter view
8. Reset VR defaults
9. BACK

Mit dem neunten Eintrag ist die Seite ausgereizt; weitere Einträge gehören in
`fearvr.ini` statt auf die Seite (so wie `TwoHandGrip`).

HMD-Translation, Head-Bob und Komfortbildschirm bleiben als Einstellungen
erhalten und werden weiterhin aus `fearvr.ini` gelesen und dorthin
geschrieben, stehen aber nicht auf der sichtbaren Seite. Ein zweistufiges Menü
wurde verworfen.

Die Taschenlampe ist ein eigener Spot-Projektor an der linken Hand, ohne
Batterieverbrauch und über einen Klick auf den linken Trigger schaltbar. Ihr
Ursprung folgt der linken Grip-Pose, ihre Strahlrichtung der linken Aim-Pose.

Die **Retail**-Taschenlampe wird dafür nicht mehr benutzt. Sie wurde früher per
Kommandopuls dauerhaft eingeschaltet und folgte der Kamera, die währenddessen
auf die Handpose gesetzt wurde. Im Normalfall lagen beide Lampen übereinander
und fielen nicht auf — nach Zwischensequenzen aber ruht der Kameraeingriff,
weil die Kamera dann der Engine gehört. Die Retail-Lampe leuchtete in diesem
Moment wieder vom Kopf aus, während der Handscheinwerfer weiterlief: zwei
getrennte Kegel, deren Lichtfelder sich addierten, und nur einer davon ließ
sich ausschalten. Der Handscheinwerfer allein deckt denselben Zweck ab und
spart zugleich einen Kameraeingriff in geskripteten Szenen.

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
