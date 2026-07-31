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

Wenn die Runtime `XR_KHR_generic_controller` anbietet, aktiviert der Host die
Erweiterung unter Beibehaltung des OpenXR-1.0-Anwendungsniveaus und schlägt
zusätzlich das KHR Generic Controller Profile vor. SteamVR kann damit
Controller aus neueren Treibern automatisch auf Sticks, Trigger, Grip,
Primär-/Sekundärtasten, Posen und Haptik abbilden.

Nach `xrSyncActions` und bei jedem
`XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED` fragt der Host
`xrGetCurrentInteractionProfile` für beide Hände ab. Das Ereignis
`input_interaction_profile` im Hostlog zeigt daher nicht nur akzeptierte
Vorschläge, sondern das tatsächlich aktive beziehungsweise emulierte Profil.

Die Zustände bleiben im Transport zunächst physisch und werden erst im
GameClient semantisch auf F.E.A.R.-Befehle abgebildet. Dadurch kann kein
fehlerhaftes Profil unbemerkt Bewegung oder Feuer auslösen.

## Spielbelegung

- linker Stick: Bewegen; linker Grip: Rennen — oder, mit der Hand an der
  Waffe, sie mit der linken Hand mithalten;
- rechter Stick links/rechts: Drehen; hoch: Springen; runter: Ducken
  (jeweils ab 80 % Ausschlag);
- A: Waffenwechsel; B kurz: Nachladen; B gehalten: Granate werfen;
- X: Taschenlampe; bei ausgerüsteten Dual Pistols stattdessen Zeitlupe, ohne
  den aktuellen Lichtzustand zu ändern; Y: Pause;
- rechter Grip: Benutzen; rechter Trigger: Feuern; linker Trigger:
  Zeitlupe — bei Dual Pistols feuert er stattdessen die linke Pistole;
- rechter Stick-Klick: in der 3D-Welt manueller Nahkampfangriff, in Menüs und
  anderen 2D-Ansichten den Flachbildschirm neu verankern;
- linker Stick-Klick: Medkit benutzen (`COMMAND_ID_MEDKIT` = 70). Retail wertet
  ihn an der steigenden Flanke in `CInterfaceMgr::OnCommandOn` aus, ein
  gehaltener Klick verbraucht deshalb genau einen Medkit. Damit ist keine freie
  Taste mehr übrig — weitere Aktionen bräuchten eine Geste oder einen
  langen Druck;
- linke Hand seitlich neigen: um die Ecke lehnen;
- Waffenhand schnell nach vorne stoßen: Nahkampf (`COMMAND_ID_ALT_FIRING`
  = 19 — die Sekundärattacke, die Retail im Optionsmenü „Melee Attack" nennt).

### SteamVR-native Controller

Valve Index Controller besitzen für alle obigen Aktionen eigene Eingaben:
Thumbsticks und Klicks, A/B, analoge Trigger und Grips sowie Posen und Haptik.

HTC Vive Wands verwenden stattdessen:

- linkes/rechtes Trackpad für Bewegung und Drehung sowie hoch/runter für
  Springen und Ducken;
- Trackpad-Klick als physische Primärtaste;
- den digitalen Grip-Klick als Float-Squeeze (OpenXR konvertiert `click` zu
  `0.0` beziehungsweise `1.0`) für Rennen/Zweihandgriff und Benutzen;
- linke Menütaste für Pause;
- rechte Menütaste kurz für Nachladen, gehalten für Granate;
- Trigger, Aim-/Grip-Pose und Haptik unverändert.

Vive Wands besitzen keine Stick-Klicks. Medkit bleibt deshalb über die
Tastatur erreichbar; der manuelle Stick-Klick-Nahkampf wird durch die bereits
vorhandene Waffenhand-/Off-Hand-Geste ersetzt. Maus, Tastatur und Gamepad
bleiben parallel aktiv.

### Physisches Lehnen (`Physical lean`, Standard an)

F.E.A.R.s eigenes Lehnen ist ausschließlich eine Drehung: `CLeanMgr` berechnet
einen Winkel, den `CPlayerCamera` als Rollen auf die Kamera legt
(`PlayerCamera.cpp:953`) und der `CLeanNodeController` auf die Körperknoten.
Der Blickpunkt bleibt dabei exakt in der Spielerposition — um eine Ecke sehen
kann man damit nicht, es kippt nur das Bild.

Deshalb bewegt jetzt der physische Kopfversatz aus dem Headtracking den
Blickpunkt mit. Das ist dieselbe Größe, die bisher nur mit `-Translation`
verfügbar war, nur wird sie an der Weltgeometrie begrenzt: Ein Strahl von der
Spielerkameraposition entlang des gewünschten Versatzes misst die freie
Strecke, und `src/common/lean_collision.h` macht daraus den Anteil, der übrig
bleibt (12 Einheiten Sicherheitsabstand zur Fläche). Enger werden gilt sofort,
weiter werden nur geglättet — sonst steckte der Blickpunkt für Sekundenbruch-
teile in der Wand oder ruckelte beim Streifen einer Kante.

Beide Augenposen enthalten den Kopfversatz als denselben Summanden, deshalb
genügt **ein** Strahl pro Bild statt einem pro Auge; der gesperrte Anteil wird
anschließend von beiden Posen abgezogen.

Das sichtbare Körpermodell folgt beim Rendern dem horizontalen Anteil dieses
Versatzes. Dadurch bleibt der Kopf natürlich über dem Torso, statt bei realer
Raumbewegung vor, hinter oder neben das eigene Modell zu gelangen. Retails
Spielerobjekt und Kollisionskapsel bleiben dennoch an der normalen
Spielerposition; es werden keinerlei Lean-Bewegungsachsen injiziert. So
entsteht weder die Rückkopplung noch das Pendeln früherer Nachführversuche.
Blickpunkt, Hände, Waffe, Mündung und Schussursprung verwenden denselben
weltbegrenzten Versatz.

Der gemessene Kopfversatz wird verstärkt, bevor Blickpunkt und Hände ihm
folgen: `LeanScale` in `fearvr.ini`, Standard 200 Prozent, erlaubt sind 100
bis 400. Zehn Zentimeter physisches Lehnen wirken damit wie zwanzig — man muss
sich also nur halb so weit beugen, um hinter einer Deckung hervorzusehen. Der
gemessene Versatz bleibt vorher auf 25 cm begrenzt, die Wirkung damit auf
einen halben Meter. In den Augenposen steckt der *unverstärkte* Versatz, und
genau der wird beim Verrechnen wieder herausgenommen; verstärkt ist nur der
weltbegrenzte physische Lean-Versatz.

Solange physisches Lehnen aktiv ist, entfällt Retails Kameraneigung über die
linke Handneigung (`COMMAND_ID_LEAN_LEFT`/`_RIGHT` werden dann nicht mehr
injiziert). Beides zusammen kippte das Bild zusätzlich zu einer Bewegung, die
der Spieler ohnehin selbst macht. Mit `Physical lean: Off` ist die Handneigung
unverändert da.

Umschaltbar im VR-Menü unter `Physical lean: On / Off`, gespeichert als
`PhysicalLean` in `fearvr.ini`.

### Physisches Ducken (`Physical duck`, Standard an)

Eine Absenkung des HMD um 26 cm gegenueber der Recenter-Hoehe aktiviert Retails
normales DUCK-Kommando. Erst oberhalb von 18 cm wird es wieder freigegeben;
diese Hysterese verhindert Flattern an der Grenze. Der Stick bleibt in beiden
Betriebsarten unveraendert nutzbar. Umschaltbar im VR-Menue, gespeichert als
`PhysicalDuck` in `fearvr.ini`.

### Klettern an Leitern (abschaltbar, Standard aus)

Umschaltbar im VR-Menü unter `Ladder climbing: HANDS / CLASSIC`, gespeichert
als `Climbing` in `fearvr.ini`. Standard ist `Classic`, also die
Retail-Steuerung über den Stick. An einer Leiter
greift dann ein Grabknopf die Sprosse, und die Bewegung dieser Hand **pro
Bild** treibt das Klettern: Ziehen nach unten klettert aufwärts, beim
beidhändigen Greifen führt die stärker ziehende Hand.

Bewegt wird nur, *während* die Hand zieht. Zwei Fassungen sind daran zuvor
gescheitert, beide am selben Punkt: Die Handpose ist raumbezogen, also bleibt
die Hand im Zimmer stehen, während der Spielkörper hochfährt. Die Auslenkung
gegenüber dem Griffpunkt verschwand deshalb nie (einmal greifen = endlos
hochfahren), und ein Zugguthaben lief ohne Kenntnis der echten
Klettergeschwindigkeit über mehrere Sprossen nach. Jetzt zählt die geglättete
Handgeschwindigkeit des aktuellen Bildes, mit langsamem Anstieg gegen
Trackingzittern und schnellem Abklingen gegen Nachlauf.

Solange der Spieler an der Leiter hängt, gehört die Vorwärtsachse ganz den
Händen — auch ohne Griff. Sonst kletterte der Stick weiter mit, und `HANDS`
wäre nur eine zusätzliche Möglichkeit statt einer Entscheidung. Die Auswertung steht in
`src/common/climb_grip.h` und ist über `tests/test_climb_grip.cpp` ohne
Headset geprüft — einschließlich genau dieses Falls.

Bewegt wird über die Kommandos, die Retail an der Leiter selbst auswertet.
`CMoveMgr::UpdateControlFlags` prüft dort ausschließlich `COMMAND_ID_FORWARD`
und `COMMAND_ID_REVERSE` (bzw. das Vorzeichen der Bewegungsachse) — die
Klettergeschwindigkeit ist fest, der Zug entscheidet nur über die Richtung.
Ein Schreibzugriff auf die Spielerphysik ist deshalb nicht nötig. Beide
Kommandos werden **nur** während des Kletterns injiziert; sonst bliebe von der
analogen Stickbewegung nur noch volle Geschwindigkeit übrig.

Ob der Spieler an einer Leiter hängt, liefert `LadderMgr`. Herleitung der
Adressen in `GameOrig.dll`:

- `LadderMgr::Instance()` liegt bei RVA `0x27B50` (erreichbar auch über den
  Inkrementell-Link-Thunk bei `0x7BBC`) und ist ein Magic-Static-Accessor: Er
  endet mit `mov eax, <Objekt>` / `ret`.
- Das statische Objekt liegt bei RVA `0x2D7AA8`, in `.data`.
- `m_pLadder` ist sein erster Member. Doppelt belegt: Die Aufrufstelle in
  `CMoveMgr`, die `"%s - jump from ladder"` protokolliert, liest direkt nach
  dem Accessor-Aufruf `cmp dword ptr [eax], 0`, und `CanReleaseLadder`
  beginnt mit demselben `cmp dword ptr [ecx], 0`.
- Gegenprobe: 46 direkte Aufrufe im Modul, 36 davon verwenden das Ergebnis
  sofort als Objektzeiger — die erwartete Streuung von `IsClimbing()`.

Die Waffe verschwindet an der Leiter von selbst: `LadderMgr` ruft beim
Aufstieg `CClientWeaponMgr::DisableWeapons`, und `CClientWeapon::SetVisible`
kehrt bei gesetztem `m_bDisabled` folgenlos zurück — unser erzwungenes
Sichtbarschalten kann daran nichts ändern. Der eigene Zielstrahl kannte diesen
Zustand jedoch nicht und hing sonst in der leeren Hand; er folgt jetzt
demselben Flag (`CClientWeapon+0x223`, belegt aus `SetVisible`, das dort
`m_bVisible` setzt und bei gesetztem Flag sofort zurückkehrt). Damit ist er
auch in Zwischensequenzen und am Geschütz aus.

Zur Laufzeit wird das Bytemuster des Accessors geprüft und die im Code
stehende (bereits relozierte) Objektadresse gegen die erwartete RVA gehalten.
Stimmt etwas nicht, bleibt das Klettern aus und das Spiel unverändert
(`ladder_manager_pattern_mismatch`). Überall außerhalb einer Leiter behalten
die Grabknöpfe ihre gewohnte Bedeutung.

### Nahkampfgesten

Gemessen wird die Geschwindigkeit der Zielpose beider Hände aus zwei
aufeinanderfolgenden Abtastungen, projiziert auf die eigene Blickrichtung der
jeweiligen Hand. Ausgelöst wird ab 2 m/s Vorwärtsanteil, sofern die Bewegung
höchstens 50 Grad neben der Zeigerichtung liegt; ein schneller Schwenk oder
das Zurückziehen fallen dadurch heraus. Danach sperrt die Geste 700 ms und
wird erst wieder scharf, wenn die Hand unter 0,8 m/s fällt — ein Stoß zählt
so genau einmal.

Die Waffenhand erzeugt den bekannten Waffenstoß. Ein Stoß der freien Hand
erzeugt als **Off-Hand Strike** denselben Retail-Sekundärangriff; F.E.A.R. hat
keinen eigenen unbewaffneten Faustangriff. Die freie Hand ist gesperrt, solange
sie die Waffe im Zweihandgriff hält oder ihr Grabknopf gedrückt ist. Beide
Hände teilen sich einen Cooldown, damit eine Bewegung nicht zwei Angriffe
auslöst.

Bei verfügbarem Retail-Bewegungszustand wartet ein Stoß am Boden höchstens
250 ms. Meldet `CMoveMgr` in diesem Fenster `jumped` oder `falling`, wird der
wartende Sekundärangriff zum Jump Kick; andernfalls folgt danach der normale
Waffen- beziehungsweise Off-Hand Strike. Ein Stoß, der bereits in der Luft
beginnt, löst den Jump Kick sofort aus. Die Geste setzt **niemals** `JUMP` —
der Sprung muss immer vom Spieler kommen.

Ein Slide Kick braucht zusätzlich einen eindeutigen Duck-Impuls: entweder
senkt sich die frische HMD-Pose innerhalb 400 ms um mindestens 25 cm, oder der
rechte Stick wird nach unten gedrückt. Danach darf ein Stoß der Waffen- oder
freien Hand nur dann auslösen, wenn Retail zugleich **RUN**, **FORWARD** und
Bodenkontakt meldet. ADS, Leiterzustand und eine deaktivierte Waffe sperren die
Aktion. Bei der physischen Hocke erzeugt der Sequencer für 200 ms `DUCK`,
`FORWARD` und `ALT_FIRING`; Stick-DUCK muss Retails echtes
`PostureDownTime`-Fenster bereits geöffnet haben und braucht deshalb keine
zweite DUCK-Flanke. Slide Kicks haben mindestens eine Sekunde gemeinsamen
Cooldown.

Retails `CPlayerCamera` übernimmt beim Ende einer Kameraanimation deren letzte
Socket-Drehung als neue lokale Blickrichtung. Beim Slide Kick kann dadurch eine
Abwärtsneigung dauerhaft im Blick verbleiben. Für VR wird deshalb die lokale
Kameradrehung unmittelbar vor dem Tritt festgehalten und während der
Kick-Animation als Renderbasis verwendet. Nur die animierte Kameradrehung wird
unterdrückt: Körperanimation, Absenken, Position und Körper-Yaw bleiben
erhalten, die Blickrichtung kommt vollständig vom HMD. Sobald Retails Kamera
wieder mehrere Frames mit der festgehaltenen Basis übereinstimmt, endet der
Eingriff automatisch.

Die reine Einordnung steht in `src/common/melee_actions.h` und ist über
`tests/test_melee_actions.cpp` ohne Headset geprüft. Der GameClient erzeugt
daraus einen 200-ms-Puls.

Die Zeitbasis ist die eigene Uhr des GameClients (`QueryPerformanceCounter`),
nicht `predictedDisplayTimeNs` aus dem Eingabezustand. Dessen vorhergesagte
Anzeigezeit bleibt zwischen zwei Abholungen gleich, wenn der Client häufiger
pollt als der Host liefert — die erste Fassung verwarf deshalb jede Abtastung
und löste im Spiel nie aus. Alle drei Sekunden protokolliert der Client unter
`melee_thrust_peak` die schnellste gemessene Handgeschwindigkeit und die Zahl
der ausgewerteten Abtastungen; damit lässt sich eine ausbleibende Geste sofort
einordnen.

Jump und Slide Kick sowie ihre sichtbaren Körperanimationen wurden im Headset
bestätigt. M2 liest die Zustände zusätzlich für `melee_retail_state`:

- `g_pMoveMgr` liegt bei RVA `0x2D8D8C`. Zwei Zugriffe in
  `CMoveMgr::PlayerLeashFn` müssen auf dieselbe relozierte Adresse zeigen.
- `m_bOnGround`, `m_bFalling` und `m_bJumped` liegen bei `CMoveMgr+0x64`,
  `+0x66` und `+0x78`. Drei getrennte Codestellen sichern die Offsets ab,
  darunter die originale Jump-Kick-Auswahl in `CPlayerBodyMgr`.
- `CMoveMgr::UpdateControlFlags` startet bei der DUCK-Flanke den
  PostureDown-Timer bei `CMoveMgr+0x4B0`. Dessen Dauer wird nicht geraten,
  sondern aus Retails bereits initialisierter Konsolenvariable
  `PostureDownTime` gelesen.

Die Diagnose protokolliert Zustandswechsel und die gemessene Fensterdauer.
Der Jump Kick liest davon nur `jumped` und `falling`; er injiziert weder
`JUMP` noch andere Bewegung. Der Slide Kick verwendet zusätzlich den echten
Bodenzustand und das gemessene Posture-Down-Fenster. Passt eine der Byteproben
nicht zur geladenen `GameOrig.dll`, bleibt der Bewegungszustand unverfügbar:
normale Schläge funktionieren dann weiterhin sofort, Kicks bleiben aus.

Maus, Tastatur und vorhandenes Gamepad bleiben parallel nutzbar.

### 2D-Panel-Recenter

Die 3D-Welt besitzt bewusst keinen manuellen Headtracking-Recenter mehr. Ihre
stabile Kamerabasis wird beim Betreten automatisch initialisiert. In Menüs,
Ladebildern, Briefings und anderen 2D-Ansichten verankern F9, rechter
Stick-Klick und `Recenter 2D panel` das raumfeste Panel neu an der aktuellen
Blickrichtung. Derselbe Stickklick fordert in der 3D-Welt stattdessen Retails
Sekundärangriff an; der aktuelle Bewegungszustand wählt normalen Schlag, Jump
Kick oder Slide Kick.

### Linkshänderbelegung

`Controls: Right-handed / Left-handed` im VR-Menü spiegelt die komplette
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
vibriert der Controller bei leerem Magazin korrekt gar nicht. Bei Dual Pistols
vibriert jeweils die Hand der tatsächlich schießenden Pistole.

### Dual Pistols: eine Pistole pro Hand

Retails Dual-Pistol-Waffe bringt bereits zwei getrennte Handmodelle,
Magazinhälften, Feueranimationen und die Wahl `m_bFireLeftHand` mit. Der alte
VR-Pfad löschte das linke Modell und ersetzte anschließend jeden Schuss durch
die rechte Controllerpose. Der neue Pfad erhält das `LDPistol`-Modell und setzt
es nach Retails gemeinsamem `SetWeaponTransform` auf die linke Grip-/Aim-Pose.

- linker Trigger: linke Pistole; rechter Trigger: rechte Pistole;
- beide Trigger: Retails vorhandene Dual-Pistol-Kadenz darf die Seiten
  abwechseln;
- X: Zeitlupe, solange X gedrückt ist; die Taschenlampe bleibt an bzw. aus;
- zwei unabhängige rote Zielstrahlen und Mündungsursprünge;
- Haptik auf dem Controller der tatsächlich feuernden Pistole;
- der normale Zweihandgriff bleibt für diese Waffe aus, weil die Stützhand
  bereits ihre eigene Pistole führt.

Die Triggervorgabe wird vor dem Retail-Waffenupdate in das verifizierte
`m_bFireLeftHand`-Byte bei `CClientWeapon+0x1E3` geschrieben. Dadurch sehen
Munitionsabzug, Mündungsblitz, Servernachricht und Fire-Vectors dieselbe
Feuerhand. `GetFireVectors` wird zusätzlich an der Instruktion
`mov al,[edi+0x1E3]` geprüft; bei einer unbekannten GameOrig-Version wird der
Hook nicht installiert. Der linke Lauf wird über den in der ausgelieferten
Dual-Pistols-Datenbank eingetragenen Socket `Flash` gemessen und anschließend
aus der eigenen Support-Hand-Transformation rekonstruiert. Die vollständige
Socketrotation einschließlich Roll wird auf die OpenXR-Aim-Pose gelegt, nicht
nur der Vorwärtsvektor. Die sichtbare linke Hand erhält dieselbe korrigierte
Rotation wie das Waffenobjekt. Bei Dual Pistols entfällt außerdem die sonstige
Recenter-Neutralkalibrierung der freien Support-Hand, weil eine Pistole direkt
der kanonischen OpenXR-Feuerachse folgen muss.

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
Die Rampe läuft von 0 bei 0,20 m auf 0,75 ab 0,35 m (`TwoHandedAimBlend`).

Gedreht wird über die kürzeste Drehung zwischen alter und neuer Vorwärtsachse.
Damit bleibt die Neigung der rechten Hand erhalten — die Waffe kippt in der
Hand, statt sich um die eigene Achse zu verdrehen. Zwei Sicherungen begrenzen
das: unter 0,12 m Handabstand ist die Linie nur noch Trackingrauschen. Und der
Lenkwinkel wird weich gedämpft (`SoftLimitedSteerAngle`): bis 55° folgt die
Waffe eins zu eins, darüber wächst der Winkel nur noch gedämpft weiter und
läuft asymptotisch gegen 90°, ohne sie je zu erreichen. Die Steigung am
Übergang ist 1, der Verlauf also knickfrei.

Zwei Fehler dieser Begrenzung sind im echten Lauf aufgefallen:

- Früher setzte die Zweihandkorrektur jenseits der Grenze für ein Bild
  vollständig aus; die frisch berechnete Einhandpose ließ die Waffe dadurch
  sichtbar in die Bildmitte springen.
- Danach hielt sie die Richtung am Rand eines harten 50°-Kegels fest — und
  benutzte dafür `forward.Cross(target)` als Drehachse. `LTVector::Cross`
  dreht die Operandenreihenfolge aber um (COORDINATE-SYSTEM.md §1), die Achse
  zeigte also in die Gegenrichtung: Sobald die Stützhand den Kegel verließ,
  kippte die Waffe schlagartig auf die **falsche** Seite der Zielachse
  (Benutzerbefund vom 28.07.2026, „plötzlich nach links"). Das Kreuzprodukt
  wird jetzt von Hand gerechnet, wie in `RotationBetweenDirections`.

Die Korrektur sitzt in `ApplyTwoHandedAimSupport` und läuft im
Weapon-Manager-Update **nach** der Mündungskorrektur, also auf der fertigen
Feuerachse. Sichtbare Waffe, rechter Handknoten, Zielhilfe und Geschossbahn
benutzen dieselbe Transformation und bleiben deshalb deckungsgleich.

**Sichtbarer Originalgriff.** Beim Einrasten wird die sichtbare Stützhand nicht
mehr an der aktuellen Controllerposition eingefroren. Für genau eine
Animationsauswertung bleiben beide Armketten unangetastet; unmittelbar vor
unserem Hand-Node-Override werden Retails animierte `RightHand`- und
`LeftHand`-Sockets gelesen. Ihr relativer Transform ist der originale,
waffenspezifische Griffpunkt und wird anschließend mit der VR-bewegten Waffe
mitgeführt. Dadurch sitzen Handfläche und Handrotation beispielsweise bei
Shotgun, SMG und Gewehr wieder an dem vom Spiel vorgesehenen Vordergriff. Die
physische Stützhand steuert weiterhin die Feuerachse und wird nur für die
Darstellung vom animierten Griff entkoppelt.

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

`VR Settings` wird in der verifizierten Retail-1.08-Klasse
`CMenuSystem` direkt nach „Optionen“ ergänzt. Die Seite verwendet dieselbe
native Menüliste und ist deshalb mit Tastatur und VR-Controller bedienbar:
Stick navigiert, A oder Trigger bestätigt, B geht zurück.

Die native Liste bildet eine kurze Kategorieübersicht mit sechs Unterseiten.
Jede Unterseite bleibt klein genug für den nativen 320px-Rahmen. `Back`
beziehungsweise Controller-B kehrt von einer Unterseite zuerst zur
Kategorieübersicht zurück und verlässt `VR Settings` erst von dort. Werte
werden sofort angewendet und in `stage/userdata-m5/fearvr.ini` gespeichert.
Numerische Einstellungen verwenden sichere Presets; exakte Zwischenwerte
können weiterhin direkt in der INI gesetzt werden.

1. **Display & HUD:** Stereo rendering, Stereo HUD, FOV scale.
2. **Movement & Comfort:** HMD translation, head bob, comfort screen,
   turn speed, physical leaning, physical duck and lean strength.
3. **Controls:** handedness, controller vibration, ladder climbing,
   flashlight mount, two-handed grip and Recenter 2D panel.
4. **Weapons:** red aim guide, show arms, simulated weapon-weight preset
   (None/Light/Medium/Heavy), plus
   Weight, Position follow, Rotation follow and Catch-up strength.
5. **Melee:** master toggle and individual weapon strike, off-hand strike,
   jump kick and slide kick toggles.
6. **Advanced:** weapon diagnostics and Reset VR defaults.

When simulated weapon weight is enabled, every shot confirmed by Retail adds
a short backward kick and muzzle rise. Profile weight scales the impulse,
while position and rotation follow rates control recovery. The recoil uses the
same successful fire-vector call as controller vibration, so an empty or
otherwise rejected trigger pull does not move the weapon.

`Show arms` ist standardmäßig `Off`: Nur Ober- und Unterarme werden über ein
lokal erzeugtes Alpha-Test-Material ausgeblendet; Hände, Torso und Beine
bleiben sichtbar. `On` setzt sofort das unveränderte Retail-Material ein.
Die Auswahl wird als `ShowArms` gespeichert.

Die vier Gewichtsstufen skalieren das konfigurierte Waffenprofil mit `0`,
`0,5`, `1` oder `2`. Die Auswahl wird als `WeaponWeightPreset` gespeichert;
eine vorhandene Konfiguration mit `WeaponWeightEnabled=1` und ohne Preset wird
aus Kompatibilitätsgründen als `Medium` geladen.

`FOV scale` erweitert das symmetrische Sichtfeld in Tangentenraum und wirkt
gleichzeitig auf Retails Stereokamera und die an OpenXR übermittelte
Projektionsschicht. Dadurch bleiben Bild und Headset-Projektion deckungsgleich.
`130%` ist der VR-Standard; die Auswahl wird als `FovScale`
gespeichert.

Die Taschenlampe ist ein eigener Spot-Projektor ohne Batterieverbrauch und
über X schaltbar. `Flashlight mount` wählt zwischen linker Hand, Kopf und
Waffe; Standard ist `WEAPON` (`FlashlightMount=2`). Die Auswahl wird als
`FlashlightMount=0/1/2` gespeichert. An der Hand
folgt der Ursprung der linken Grip-Pose und die Strahlrichtung der linken
Aim-Pose. Am Kopf folgt beides der HMD-Pose, an der Waffe der aktuellen
Mündung und Feuerachse. Beim Kopfmodus bleibt das sichtbare Lampenmodell
ausgeblendet, damit es nicht in die Augen-Frusta ragt. Außerdem deaktiviert
nur dieser Modus die Objektschatten des Projektors: Player-Body, Hände und
Waffe können den kopffesten Lichtkegel damit nicht mehr verdecken,
Weltgeometrie wirft weiterhin Schatten. Beim Wechsel zurück auf Hand oder
Waffe wird die volle Schattenstufe wiederhergestellt.

Die **Retail**-Taschenlampe wird dafür nicht mehr benutzt. Sie wurde früher per
Kommandopuls dauerhaft eingeschaltet und folgte der Kamera, die währenddessen
auf die Handpose gesetzt wurde. Im Normalfall lagen beide Lampen übereinander
und fielen nicht auf — nach Zwischensequenzen aber ruht der Kameraeingriff,
weil die Kamera dann der Engine gehört. Die Retail-Lampe leuchtete in diesem
Moment wieder vom Kopf aus, während der VR-Scheinwerfer weiterlief: zwei
getrennte Kegel, deren Lichtfelder sich addierten, und nur einer davon ließ
sich ausschalten. Der VR-Scheinwerfer allein deckt denselben Zweck ab und
spart zugleich einen Kameraeingriff in geskripteten Szenen.

## Support-hand Wrist-HUD

Das Status-HUD wird als eigenes World-Space-Fenster nach jedem Stereo-
Weltdurchlauf und vor dem Eye-Capture gerendert. Es sitzt am Handgelenk rund
fünf Zentimeter vor der dem Gesicht zugewandten Handfläche. Sein Z-Test ist
deaktiviert, damit die Hand das Informationsfenster nicht mehr verdeckt; die
World-Space-Position und Stereo-Parallaxe bleiben erhalten. Weil die komplette
Eingabe bei
`Controls: LEFT-HANDED` vorher gespiegelt wird, bleibt diese logische linke
Hand auch dann automatisch die physische Stützhand.

Das Fenster erscheint nur, wenn

- Retail `GS_PLAYING` meldet;
- ein aktueller Weapon-Manager-Frame und gültige Grip-/Aim-Posen vorliegen;
- keine Zwischensequenz aktiv ist; und
- die Stützhand die Waffe nicht beidhändig gegriffen hält; und
- die HMD-Vorwärtsachse innerhalb eines engen Blickkegels auf das Handgelenk
  zeigt.

Eine Haltezeit von 140 ms verhindert Flackern an der Kegelgrenze. Das
15,2 × 9,2 cm große, zur HMD-Pose ausgerichtete Vektorfenster zeigt HP, Armor,
Gesamtmunition, Splittergranaten, Näherungsmine, Fernladung, Medkits und einen
Luftbalken. Dünne Segmentzeichen, umrandete Rasterbalken und feste Trennlinien
ersetzen die frühere 3 × 5-Blockschrift. Es verwendet keine Retail-Texturen
oder Fonts.

Die Daten kommen aus dem versionsgeprüften Retail-1.08-`CPlayerStats`, aus
`CWeaponDB::GetPlayerGrenade` für alle drei Wurfwaffen-Slots und aus
`CPlayerStats::GetGearCount` für das Medkit. Die alten Status-Variablen werden
erst nach Retails Client-Update auf null gesetzt. Sobald Retail Pause, ESC,
Menü oder einen anderen Vollbildzustand meldet, werden ihre ursprünglichen
Werte noch vor demselben Renderframe wiederhergestellt. Menü-Layout,
Menüsteuerung und der Flachbildschirm-Pfad bleiben dadurch unverändert.

Größe, Abstand, Blickwinkel und die endgültige Lesbarkeit werden nach jeder
visuellen Iteration im Headset erneut abgenommen.

### Warum die Auswahl gesprungen ist

Jeder Umschalter besteht aus zwei Controls (`… : On` und `… : Off`), von denen
immer eines versteckt ist. Das ist für die Navigation unkritisch, weil
`CLTGUICtrl::IsEnabled()` als `m_bEnabled && IsVisible()` definiert ist und
`CLTGUIListCtrl::NextSelection` unsichtbare Einträge damit korrekt überspringt.

Der Fehler steckt in `CLTGUIListCtrl::SetSelection`: Beim Herunterscrollen
bestimmt es den neuen Listenanfang, indem es rückwärts die `GetBaseHeight()`
aller Controls aufsummiert — **ohne** `IsVisible()` zu prüfen.
`CalculatePositions()` überspringt unsichtbare Controls dagegen sehr wohl.
Beide Rechnungen widersprechen sich, sobald versteckte Geschwister-Controls
dazwischenliegen, und `m_nFirstShown` wird falsch gesetzt.

Jede Kategorieunterseite passt vollständig in den nativen Rahmen. Der
Listenanfang wird deshalb bei 0 festgehalten, solange irgendeine VR-Seite
aktiv ist — und zwar in jedem Client-Update, weil Tastatur, Maus und Controller
alle direkt über `NextSelection` navigieren und nicht über den eigenen Hook.

`MeleeGestures=1` ist der Master-Schalter. `MeleeWeaponStrike`,
`MeleeOffHandStrike`, `MeleeJumpKick` und `MeleeSlideKick` bleiben getrennte
persistierte Werte, sind nun aber ebenfalls auf der Melee-Unterseite
erreichbar. Untergeordnete Schalter werden ausgeblendet, wenn der
Master-Schalter auf `Classic` steht.

- `ShowArms=0` — vom Menü gespeicherter Schalter; `0` ist der Standard und
  verwendet die VR-Armmaske, `1` das vollständig sichtbare Retail-Material.
- `HiddenBodyPieces=0` — reine Entwicklerdiagnose für die F11-Piece-Probe.
  Der frühere Wert `2` wird auf `0` migriert, weil Retail-Piece #1
  `Body_Group` mit Armen, Torso und Beinen gemeinsam enthält.
