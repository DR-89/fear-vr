# Arbeitsstand F.E.A.R.-VR

Stand: 25.07.2026  
Letzter Commit: `92a782a M5: OpenXR-Motion-Controls, natives VR-Menü und
Handdarstellung`

Diese Datei ist die aktuelle Übergabe für den nächsten Agenten. Die ältere Datei
`arbeitsstand.md` beschreibt überwiegend M4 und ist für den jetzigen Stand nicht
maßgeblich.

## Wichtig vor dem Weiterarbeiten

- M4 und **M5 sind abgeschlossen und committed**. Der Arbeitsbaum ist sauber.
- Die Retail-Installation wird nicht verändert. Gebaut und getestet wird über
  `stage\m5-game` und `stage\userdata-m5`.
- Nur auf ausdrücklichen Wunsch committen.

## Aktuell vom Benutzer bestätigter Zustand

Folgende Punkte funktionieren:

- Stereo-VR startet nach dem Laden automatisch; F8 ist nicht mehr erforderlich.
- Korrekte 3D-Darstellung, Kopfbewegung und Achsen.
- Theater-/Desktop-Overlay-Problematik ist behoben.
- HUD-Positionen sind brauchbar.
- Roter Zielstrahl und Scope-Ausrichtung wurden als sehr gut bestätigt.
- Das normale Fadenkreuz ist ausgeblendet; der rote Zielstrahl reicht aus.
- Waffenausrichtung fühlt sich nach der letzten Kalibrierung gut an.
- Controller-Steuerung im Spiel funktioniert:
  - linker Stick: Bewegung
  - rechter Stick links/rechts: Drehen
  - rechter Stick hoch/runter: Springen/Ducken, erst ab 80 % Rohausschlag
    (der deadzonebereinigte Wert waere gestaucht)
  - A: Waffenwechsel, zyklisch vorwaerts
  - B kurz: Nachladen; B ab 350 ms gehalten: Granate werfen
    (COMMAND_ID_THROW_GRENADE = 81)
  - X: Zeitlupe
  - Y: Pausenmenü
  - linker Grip: Rennen
  - rechter Grip: Benutzen
  - Trigger: Zielen/Feuern
  - rechter Stick-Klick: Recenter
  - linker Stick-Klick: frei (Ducken liegt allein auf dem rechten Stick)
  - Haptik je Schuss ueber den Fire-Vectors-Aufruf statt ueber die
    Triggerflanke, damit auch Dauerfeuer vibriert
  - HUD-Stauchung: gleichmaessig zur Bildmitte (5/4) statt zonenweiser
    Verschiebungen, die HUD-Elemente an den Zonengrenzen zerschnitten haben
  - Flachbildzustand: Menuefokus UND Weapon-Manager-Frische werden jetzt
    oder-verknuepft. Der Fokus kennt nur Pausenmenues; Vollbildschirme wie das
    Missionsbriefing wurden dadurch als Spiel gewertet und vom Compositor
    verworfen — im Headset blieb die Welt stehen, das Desktopfenster zeigte
    das Briefing.
  - Retail-Taschenlampe entfaellt komplett (Kommandopuls und Kamera-Override
    entfernt). Sie war die zweite, nicht schaltbare Lichtquelle, die nach
    Zwischensequenzen sichtbar neben dem Handscheinwerfer stand.
- Aktivieren und Aufsammeln folgen dem Waffenstrahl statt der Blickrichtung
  (Hooks auf `CTargetMgr::CheckForIntersect` und `ObjectDetector::Update`,
  Reichweite 60 Einheiten). Gebaut, aber noch **nicht im Spiel getestet**.
  Adressen und Layouts: `docs/RETAIL-ACTIVATION.md`; Notausstieg
  `-fearvr-no-interaction`.
- Die linke System-/Menütaste kann nicht für Pause verwendet werden, weil
  SteamVR sie abfängt und sein eigenes Systemmenü öffnet.
- Das native englische VR-Menü im ESC-Menü ist jetzt laut Benutzer völlig in
  Ordnung. Die Auswahl läuft seit dem `SetSelection`-Fix sauber und springt
  nicht mehr (bestätigt am 25.07.2026).
- Ober- und Unterarme sind korrekt ausgeblendet, Hände und Waffe bleiben
  sichtbar (bestätigt am 25.07.2026).
- Lehnen über die Neigung der linken Hand fühlt sich angenehm an; Richtung und
  Schwelle passen (bestätigt am 25.07.2026).

## VR-Menü: aktueller Aufbau

Das Menü ist bewusst kurz und einseitig, damit kein Eintrag über den Rand läuft
und kein unsauberes Scrollen entsteht:

1. Stereo rendering
2. Stereo HUD
3. Turn speed
4. Red aim guide
5. Controller vibration
6. Recenter view
7. Reset VR defaults
8. BACK

HMD translation, Head bob und Comfort screen sind weiterhin als Einstellungen
vorhanden und werden aus `fearvr.ini` gelesen/gespeichert, sind aber nicht mehr
auf der sichtbaren Seite.

Im Code werden die alten Controls `MORE SETTINGS >` und `< BASIC SETTINGS` noch
erzeugt, jedoch versteckt und nicht mehr benutzt. Das kann nach Stabilisierung
aufgeräumt werden, hat aber derzeit keine Priorität.

Relevante Retail-Konstanten:

- `CMenuSystem::Init`: RVA `0x0010CA90`
- `CMenuSystem::OnCommand`: RVA `0x0010D480`
- `CMenuSystem::OnFocus`: RVA `0x0010CE00`
- `AddControl(wchar_t*)`-Thunk: RVA `0x00008A58`
- `CLTGUIListCtrl::GetControl`: RVA `0x00251440`
- `CLTGUIListCtrl::SwapItems`: RVA `0x00252CE0`
- `CLTGUIListCtrl::SetSelection`: RVA `0x002527E0`
- Menüliste im Menüobjekt: `+0x6E8`
- Item spacing: `+0x54`
- First shown: `+0x5C`
- Recalc-Byte: `+0x648`
- `CLTGUICtrl::Show`: VTable-Slot 41

## Offener Hauptfehler: Arme ausblenden

Letztes Benutzerfeedback:

> Ober und Unterarm werden nach wie vor angezeigt. VR settings Menü ist völlig
> in Ordnung.

Gewünscht ist: Nur Hände und Waffen anzeigen; Ober- und Unterarme ausblenden.

### Vermessene Fakten (Lauf `logs\m5-fear-20260724-231900`)

Die frühere Meldung `Retail player-body pieces: <none>` stammte aus einem
**alten** Binary; der damalige Retry-Fix war nie gestartet worden. Mit
ausgebauter Diagnose liegen jetzt harte Daten vor:

- Modell: `chars\models\player.Model00p`
- **4 Pieces**, 61 Nodes
- Materialien: nur zwei, `chars\materials\player_new.Mat00` und
  `chars\materials\player_head.Mat00`
- `GetNumPieces` und `GetPiece(index, …)` liefern `LT_OK`, aber
  `GetPieceName` liefert für **alle vier** Pieces `LT_NOTFOUND` (61).
  Retail speichert die Piece-Namen also nicht.
- Nodes (Auszug): `Pelvis, Torso, Upper_torso, Neck, Head, Left_shoulder,
  Left_armu, Left_arml, Left_hand, L_Thumb0…L_Pinkie2, Right_shoulder,
  Right_armu, Right_arml, Right_hand, R_Thumb0…R_Pinkie2, Left_legu,
  Left_legl, Left_foot, Right_legu, Right_legl, Right_foot, trans_cam,
  Pelvis_cam, Torso_cam, Upper_torso_cam, Neck_cam, Head_cam, Eye_cam, aimer`

Folgerung: Namensbasiertes Ausblenden ist unmöglich. Die Pieces existieren
aber und lassen sich **über den Index** ausblenden.

### Aktueller Ansatz: Piece-Index statt Piece-Name

In `src/gameclient_loader/stereo_hook.cpp`:

- `LogRetailPlayerBodyGeometry(...)` gibt einmalig Modelldatei, Piece-Anzahl,
  Piece-Namen bzw. deren Fehlercodes, Materialdateien und alle Node-Namen aus.
- `ApplyPlayerBodyPieceMask(...)` setzt `SetPieceHideStatus` nach der Bitmaske
  `g_hiddenBodyPieceMask` und wird bei jedem Weapon-Update erneut angewandt,
  weil ein Modell-Reload die Hide-Flags zurücksetzt.
- `PollBodyPieceProbeKey()` schaltet mit **F11** durch sechs Zustände: alles
  sichtbar, dann jeweils **nur** Piece 0, 1, 2 oder 3 sichtbar, zuletzt alles
  versteckt. Isolieren benennt ein Piece deutlich besser als Ausblenden. Jeder
  Schritt wird als `vr_body_piece_probe` geloggt und sofort als
  `HiddenBodyPieces` in `fearvr.ini` gespeichert.
- Der gespeicherte Wert wird beim Start automatisch angewandt. F11 ist also
  eine einmalige Kalibrierung, keine Taste für jede Sitzung.
- Die Hände stammen vom Player-Body, nicht vom Waffenmodell: Die Waffe ist ein
  eigenes Objekt (`m_RightHandWeapon.m_hObject`), das über den `RightHand`-
  Socket hängt. Den kompletten Body zu verstecken (Maske 0xF) liefert deshalb
  eine schwebende Waffe **ohne** Hände.
- Sollte Retail doch einmal Namen liefern, setzt die Namenserkennung die Maske
  weiterhin automatisch.

### Gelöst und benutzerbestätigt am 25.07.2026

**Piece #1 trägt die Arme.** Mit `HiddenBodyPieces=2` sind Hände und Waffe
sichtbar, Ober- und Unterarm verschwinden. Vom Benutzer im Spiel bestätigt.

Der Wert steht als `kPlayerBodyArmPieceMask` zusätzlich als Code-Default in
`stereo_hook.cpp` und greift damit auch ohne vorhandene `fearvr.ini` und ohne
jeden Tastendruck. F11 bleibt nur als Nachkalibrierung bestehen, falls ein
anderes Modell einmal eine andere Piece-Reihenfolge hat.

```powershell
$log = Get-ChildItem logs\m5-fear-* -Directory |
    Sort-Object Name -Descending | Select-Object -First 1
Get-ChildItem $log.FullName -Filter 'proxy-*.log' |
    Select-String -Pattern 'vr_player_body_|vr_arm_|vr_body_piece_probe' |
    ForEach-Object { $_.Line }
```

Auswertung:

- Trennt ein Index Arme von Händen, diesen Wert als `HiddenBodyPieces` in
  `fearvr.ini` festschreiben und als Standard in den Code übernehmen.
- Entfernt jedes armhaltige Piece zugleich die Hände, sind Arme und Hände im
  selben Mesh. Nur zwei Materialien machen das leider wahrscheinlich. Dann ist
  ein eigener Hand-only-Rendering-Ansatz nötig, kein weiteres Raten.
- Knochen nicht blind weit weg verschieben: Bei geskinnten Meshes entstehen
  sonst gestreckte oder schwebende Geometrien. Ein Kollabieren von `Right_armu`
  und `Right_arml` auf die Handposition erzeugt einen sichtbaren Splitter vom
  Oberkörper zur Hand und ist deshalb keine Lösung.

## Lehnen über die Neigung der linken Hand

Auf Wunsch neu ergänzt: Die linke Hand seitlich neigen lehnt um die Ecke.

- `PoseRollRadians` in `src/common/input_state.h` liest die Rolllage einer Pose
  um deren eigene Vorwärtsachse aus den Welt-Hoch-Anteilen der lokalen X- und
  Y-Achse. Unnormierte Quaternionen liefern denselben Winkel.
- `LeftHandLeanRollRadians` gibt 0 zurück, solange die linke Hand inaktiv oder
  ihre Aim-Pose ungültig ist. Das Lehnen löst sich damit bei Trackingverlust.
- `controller_mapping.h` bildet auf `FEARVR_CMD_LEAN_LEFT` (20) und
  `FEARVR_CMD_LEAN_RIGHT` (21) ab, Schwelle 0,42 rad (~24°). Positive Rolllage,
  also Handoberseite nach links, lehnt nach links.
- Beide Kommandos stehen in `kDigitalCommands` von `InjectSemanticCommandBits`.
- `CLeanMgr` blendet das Lehnen selbst weich ein und aus, deshalb ist das
  Kommando bewusst binär.

Vom Benutzer am 25.07.2026 im Spiel bestätigt.

Zwei Grenzfälle aus `logs\m5-fear-20260724-235013` nachgezogen:

- Rolllagen nahe ±180° (gemessen 177,3°, 136,3°, −169,3°) lösten ein volles
  Lehnen aus. Obergrenze jetzt 1,75 rad (~100°).
- Ursache dieser Ausreißer war eine steil nach unten zeigende Aim-Pose. Dort
  verschwinden die Welt-Hoch-Anteile beider Achsen und die Rolllage wird
  bedeutungslos. `PoseLevelness` liefert `|cos(pitch)|`; unter 0,5 wird nicht
  mehr gelehnt.

Die Entscheidung sitzt jetzt in `LeftHandLeanDirection` (−1, 0, +1).

Lean und Slow-Mo werden getrennt protokolliert: `vr_lean_left_engaged`,
`vr_lean_right_engaged`, `vr_slowmo_engaged` mit laufender Nummer und
Rolllage, dazu `*_released` mit Haltedauer. Damit ist die letzte offene
Detailregression aus der Live-Testmatrix geschlossen.

## VR-Menü: Sprünge bei der Auswahl behoben

Benutzermeldung: Das Auswählen des nächsten Eintrags war nicht immer korrekt
und sprang.

Ursache im Public-Tools-Quelltext nachgewiesen:
`CLTGUIListCtrl::SetSelection` summiert beim Herunterscrollen rückwärts die
`GetBaseHeight()` **aller** Controls, ohne `IsVisible()` zu prüfen, während
`CalculatePositions()` unsichtbare Controls überspringt. Jeder Umschalter hat
ein verstecktes Geschwister-Control, also wird `m_nFirstShown` falsch gesetzt.

Behoben, indem der Listenanfang festgehalten wird, solange die VR-Seite aktiv
ist — in jedem Client-Update, weil Tastatur, Maus und Controller alle direkt
über `NextSelection` navigieren und nicht über den eigenen Hook.

Die toten Controls `MORE SETTINGS >` und `< BASIC SETTINGS` sind samt
Seitenzustand `g_vrSettingsPage` entfernt.

Vom Benutzer am 25.07.2026 im Spiel bestätigt: Die Auswahl läuft sauber.

## Waffe, Hände und Treppen-Sprung

Es gab weiterhin gelegentlich einen sichtbaren Sprung der Waffe beim
Treppensteigen. Um die animierte/stale Socket-Position nicht für einen Frame
durchkommen zu lassen, wird nach dem Retail-Weapon-Update derselbe
Controller-Transform auf die sichtbare Waffe und den korrigierten
`RightHand`-Socket angewendet.

Dabei wurde ein wichtiger ABI-Fehler gefunden und behoben:

- Falsch war:
  `CClientWeapon::SetWeaponTransform(const LTRigidTransform&)`
- Korrekt ist:
  `CClientWeapon::SetWeaponTransform(const LTTransform&)`
- Jetzt wird eine gültige Skalierung von `1.0F` mitgegeben.

Der falsche Typ las ungültige Scale-Daten und ließ die Waffe verschwinden. Die
korrigierte ABI-Fassung ist gebaut und läuft.

**Zurückgestellt (25.07.2026):** Der Benutzer verfolgt den Treppen-Sprung
derzeit nicht weiter. Der Punkt ist bewusst offen und kein Blocker. Nichts an
Waffen-Transform, Socket-Synchronisierung oder Handpose-Cache ohne neuen
Auftrag ändern.

Kurze Tracking-Lücken der Handpose werden bis 150 ms aus dem letzten gültigen
Pose-Cache überbrückt. Das allein hat den Treppen-Sprung nicht gelöst, ist aber
als Schutz gegen einzelne ungültige Frames weiterhin aktiv.

## Gelöst: Absturz an einer geskripteten Zwischensequenz

Benutzermeldung am 25.07.2026: Das Spiel stürzt reproduzierbar an **einer
bestimmten** Zwischensequenz ab. Andere Sequenzen laufen in 3D durch. Pures
Retail mit denselben Spielständen läuft an derselben Stelle sauber.

### Zwei Signaturen auseinanderhalten

| Signatur | Bedeutung |
|---|---|
| `unknown` + Offset `0x00000000` | **die Zwischensequenz** — Sprung auf Adresse 0 |
| `FEAR.exe` + Offset `0x0004abd6` | Absturz beim **Beenden** des Spiels, eigenes Thema |

Wer das verwechselt, hält einen erfolgreichen Lauf für einen Absturz. Genau
das ist während der Fehlersuche einmal passiert.

### Absturzmelder

Der Loader installiert einen `SetUnhandledExceptionFilter`. Er schreibt
`crash_caught` mit Ausnahmecode, Adresse, Modulzuordnung, aktuellem
Renderschritt und Menüzustand, dazu `crash_frame` je Rücksprungadresse, die im
Stack in unseren Modulen liegt. Das Modulabbild wird über den PE-Header
vermessen, damit keine Abhängigkeit auf `psapi` entsteht.

Befund über alle Läufe: `code=0xC0000005 address=0x00000000
module=retail_or_system`, und **keine** Rücksprungadresse aus dem VR-Loader im
Stack. Der Schaden entsteht also früher als der Absturz.

### Diagnoseschalter

`tools\release\play.ps1` reicht sie durch; im Log erscheint
`vr_features_disabled` mit dem Stand aller Schalter.

| Schalter | schaltet ab |
|---|---|
| `-NoFlashlight` | Taschenlampenmodell, Lichtobjekt, Kamera-Override |
| `-NoHandNodes` | Node-Controls auf Arm- und Handknochen |
| `-NoWeaponTransform` | Überschreiben der Waffentransformation |
| `-NoBodyHide` | Ausblenden der Körper-Pieces |
| `-Safe` | die vier oben zusammen |
| `-NoStereo` | Stereo-Doppelrender |
| `-NoInject` | Schreiben der Kommandobits in `CBindMgr` |
| `-NoBindingHook` | Hook auf `CBindMgr::GetBindingValue` |
| `-NoClientUpdate` | Arbeit im `IClientShell::Update`-Hook |
| `-NoInput` | den kompletten Client-Input-Hook |

### Gemessen ausgeschlossen

1. **Zeitversatz Waffe/Render** — 79 von 79 Messungen `drift_units=0.00`.
2. **Retail-Waffen-Bob und -Lag** — `headbob_disabled` bestätigt.
3. **Taschenlampen-Kamera-Override** — Absturz mit `flashlight=0`.
4. **Node-Controls, Waffentransform, Piece-Hiding** — Absturz mit `-Safe`.
5. **Stale Handles nach Levelwechsel** — `world_objects_forgotten` erscheint an
   dieser Stelle nie, das Weapon-Manager-Update pausiert dort nicht.
6. **Stereo-Doppelrender** — Absturz mit `stereo=0`.
7. **Modulkombination** — mit geladenen Modulen, aber ohne VR-Flags läuft die
   Szene durch. Loader, `GameOrig.dll` und `d3d9`-Proxy sind unbedenklich.
8. **Kommando-Injektion** — Absturz mit `inject=0`.
9. **Bindungs-Hook** — Absturz mit `binding_hook=0`.

### Ursache: der Detour auf `SetTrackedTarget`

Die Eingrenzung lief über `-NoInput` → `-NoAimHooks` → `-NoAimAt`. Wichtig für
das Verständnis: `-NoInput` lässt nicht nur den `IClientShell::Update`-Hook
weg, an diesem Pfad hängt auch `InstallWeaponAimHooks`. Im überlebenden Lauf
fehlt `weapon_aim_hooks_installed` im Log, in jedem abgestürzten ist es
vorhanden. Diese Verwechslung hat unterwegs zu einer falschen
Zwischenschlussfolgerung geführt.

Entscheidend war dann `-fearvr-aimat-passthrough`:

| Konfiguration | Ergebnis |
|---|---|
| AimAt-Hook aktiv, Ziel überschrieben | Absturz |
| AimAt-Hook aktiv, **reicht nur durch** | **Absturz** |
| AimAt-Hook nicht installiert | läuft durch |

Der Hook war installiert und tat nachweislich nichts — und es stürzte
trotzdem ab. **Ursache ist das Patchen dieser Funktion selbst**, nicht unser
Zielwert und nicht der Inhalt des Hooks.

Erwähnenswert, weil es zwei naheliegende Erklärungen ausschließt:

- `EnsureHandNodeControls(nullptr)` kehrt sofort zurück. Für fremde Kontexte —
  also den gespawnten NPC — tat der Hook ohnehin nichts.
- `IsStaticPlayerBodyTrackerContext` prüft, ob der Kontext im statischen
  Datenbereich von `GameOrig.dll` liegt (`0x002D0000`–`0x002E9900`). NPCs sind
  Heap-Objekte und fallen korrekt durch; eine Verwechslung fand nicht statt.

### Lösung

`HookRetailSetTrackedTarget` wird **nicht mehr installiert**. Mit
`-fearvr-aimat` lässt es sich zu Vergleichszwecken wieder zuschalten.

Kosten: Oberkörper und Kopf des Spielerkörpers drehen nicht mehr in die
Zielrichtung nach. Waffenhaltung, roter Zielstrahl, Fire-Vectors und
Trefferpunkt sind nicht betroffen. Vom Benutzer am 25.07.2026 im Spiel
bestätigt: Die Szene läuft durch, der Rest fühlt sich normal an.

### Lehre für weitere Retail-Hooks

Ein MinHook-Detour auf eine Retail-Funktion kann stabil aussehen und trotzdem
in einer einzelnen Spielsituation tödlich sein — hier ohne jede
Rücksprungadresse aus unserem Code im Stack. Wenn ein Absturz auf einen
unserer Hooks zeigt, ist der aussagekräftigste Test **nicht**, den Hookinhalt
zu entschärfen, sondern ihn als reines Durchreichen zu betreiben. Erst das
trennt „unser Code" von „unser Patch".

**Vom Benutzer am 25.07.2026 zurückgestellt.** An dem Hook hängt die gesamte
VR-Controllerunterstützung; ihn zu entfernen hieße, das Feature zu entfernen.

### Nicht erneut versuchen

- Den Bindungs-Hook abschalten, ohne die Kommando-Injektion umzuhängen: Sie
  wird **aus ihm heraus** aufgerufen. Ohne ihn kommt gar keine
  Controllereingabe mehr an, und die Steuerung ist tot.

## Offen: Mündungsfeuer und Leuchtspur versetzt beim Strafen

Benutzermeldung am 25.07.2026: Beim schnellen Strafen mit Dauerfeuer sitzen
Mündungsfeuer und Leuchtspur neben der Waffe. **Die Waffe selbst sitzt
richtig, Zielstrahl und Trefferpunkt stimmen.** Der Fehler ist rein
kosmetisch.

Zwei Hypothesen wurden gemessen und **widerlegt**:

1. *Zeitlicher Versatz zwischen Waffensetzen und Rendern.* Eine temporäre
   Diagnose verglich die Kameraposition im Weapon-Manager-Update mit der zur
   Renderzeit: 79 von 79 Messungen `drift_units=0.00` bei `age_ms=0`. Es gibt
   keinen Versatz. Die Diagnose wurde wieder entfernt.
2. *Retail-Waffen-Bob und Waffen-Lag.* Nach der Korrektur des
   Maskierungsfehlers meldet der Lauf `headbob_disabled`, also sind alle zwölf
   Variablen samt `HeadBobWeapon*Amp` und `WeaponLagEnabled` gesetzt. Der
   Versatz bleibt trotzdem.

Verbleibende Erklärung: Die Mündung hängt an einem Knoten **innerhalb** des
Waffenmodells. Wir sperren das Waffen*objekt* auf die Controllerpose, die
Modellanimation bewegt den Mündungsknoten aber weiterhin relativ dazu. Die
Effekte hängen an diesem Knoten.

Ein Fix müsste eine Node-Control auf den Mündungsknoten des Waffenmodells
legen, analog zu `EnsureHandNodeControls` für Ober-, Unterarm und Hand. Vom
Benutzer am 25.07.2026 bewusst zurückgestellt: Der Eingriff träfe genau das
bestätigt funktionierende Waffen- und Zielsystem, der Fehler ist kosmetisch.

Relevante Nodes/Sockets:

- rechts: `Right_armu`, `Right_arml`, `Right_hand`, Socket `RightHand`
- links: `Left_armu`, `Left_arml`, `Left_hand`, Socket `LeftHand`

Relevante Codebereiche/Funktionen:

- `src/gameclient_loader/stereo_hook.cpp`
  - `ConfigureRetailArmPieceVisibility`
  - `EnsureHandNodeControls`
  - `RemoveHandNodeControls`
  - `RetailSetWeaponTransformFunction`
  - Weapon-/Socket-Synchronisierung und Handpose-Cache
- `src/host64/xr_input.cpp`: OpenXR-Controller-Input
- `src/common/controller_mapping.h`: logische Tastenbelegung
- `src/common/input_state.h`: Input-Zustand/Übergänge

Im vorherigen funktionierenden Log waren unter anderem vorhanden:

- `weapon_hand_tracking_installed`
- `left_hand_tracking_installed`
- `right_forearm_tracking_active`
- `left_forearm_tracking_active`
- `weapon_socket_sync_active`

## Nicht erneut verfolgen: bekannte fehlgeschlagene Ansätze

- Eine kleinere Menü-Fonthöhe über `SetFontHeight` hat die Basishöhe der
  Listeneinträge nicht reduziert. Dieser Ansatz wurde entfernt.
- Ein langes bzw. zweistufiges VR-Menü führte zu unsauberen Sprüngen beim
  Scrollen. Das kurze Einseitenmenü ist bestätigt und soll beibehalten werden.
  Die eigentliche Ursache war nicht der Menüaufbau, sondern der oben
  beschriebene `SetSelection`-Fehler.
- Ein zusätzliches `Enable(false)` auf versteckten Menü-Controls bringt nichts:
  `CLTGUICtrl::IsEnabled()` ist bereits `m_bEnabled && IsVisible()`. Beim
  Wiedereinblenden würde es außerdem statische Controls auswählbar machen.
- Nur das Überbrücken kurzer Pose-Lücken beseitigte den Waffen-Sprung auf
  Treppen nicht.
- Der falsche `LTRigidTransform`-Aufruf von `SetWeaponTransform` ließ die Waffe
  verschwinden; nicht wieder einführen.
- Bone-Scaling über ein angenommenes `NodeControlData::m_fScale` existiert in
  der Retail-Schnittstelle nicht und kompiliert nicht. Node-Control stellt hier
  nur einen `LTRigidTransform*` bereit.

## Build- und Teststand

Der letzte x86-Build war erfolgreich, mit `/W4 /WX`. Alle sieben x86-Tests
bestanden:

1. protocol
2. xr_session_state
3. stereo_math
4. head_tracking_math
5. stereo_hud_math
6. input_state
7. controller_mapping

x64 war während der M5-Menüarbeit ebenfalls zuletzt mit 7/7 Tests grün. Die
neuesten Änderungen betreffen hauptsächlich den x86-Gameclient-Loader; nach
weiteren Änderungen trotzdem beide Architekturen erneut bauen.

`tests/CMakeLists.txt` übersetzt `test_controller_mapping` jetzt mit
`/UNDEBUG`. Der Test prüft ausschließlich über `assert`, und RelWithDebInfo
definierte sonst `NDEBUG`, sodass er wirkungslos durchlief. Die dabei
entstehende Kommandozeilenwarnung `D9025` ist beabsichtigt und harmlos.

x86 bauen und testen:

```powershell
& 'C:\Program Files\CMake\bin\cmake.exe' --build build\x86 --config RelWithDebInfo
& 'C:\Program Files\CMake\bin\ctest.exe' --test-dir build\x86 -C RelWithDebInfo --output-on-failure
```

x64 bauen und testen:

```powershell
& 'C:\Program Files\CMake\bin\cmake.exe' --build build\x64 --config RelWithDebInfo
& 'C:\Program Files\CMake\bin\ctest.exe' --test-dir build\x64 -C RelWithDebInfo --output-on-failure
```

Stage aktualisieren:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\prepare-m5-stage.ps1
```

Spiel starten:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\launch-m5-fear.ps1
```

Ein GUI-Start kann eine Sandbox-/Berechtigungsfreigabe benötigen.

## Geänderte und neue M5-Dateien

Geändert:

- `README.md`
- `docs/ARCHITECTURE.md`
- `docs/TESTING.md`
- `src/common/head_tracking_math.h`
- `src/common/protocol.h`
- `src/common/stereo_hud_math.h`
- `src/gameclient_loader/CMakeLists.txt`
- `src/gameclient_loader/stereo_hook.cpp`
- `src/host64/CMakeLists.txt`
- `src/host64/ipc_bridge.cpp`
- `src/host64/ipc_bridge.h`
- `src/host64/openxr_host.cpp`
- `src/proxy32/bridge.cpp`
- `src/proxy32/bridge.h`
- `src/proxy32/d3d9.def`
- `src/proxy32/proxy_exports.cpp`
- `tests/CMakeLists.txt`
- `tests/test_head_tracking_math.cpp`
- `tests/test_protocol.cpp`
- `tests/test_stereo_hud_math.cpp`
- `tools/launch-m2-fear.ps1`
- `tools/prepare-m2-stage.ps1`

Neu und untracked:

- `docs/OPENXR-INPUT.md`
- `src/common/controller_mapping.h`
- `src/common/input_state.h`
- `src/host64/xr_input.cpp`
- `src/host64/xr_input.h`
- `tests/test_controller_mapping.cpp`
- `tests/test_input_state.cpp`
- `tools/launch-m5-fear.ps1`
- `tools/prepare-m5-stage.ps1`

Die Dokumentation ist teilweise etwas älter als das inzwischen verkürzte
VR-Menü. Erst nach funktionalem Abschluss angleichen.

## Empfohlene nächste Schritte

Alle vom Benutzer angeforderten Spielprüfungen sind bestätigt: Arme
ausgeblendet, VR-Menü sauber, Lehnen angenehm. Der Treppen-Sprung der Waffe ist
ausdrücklich zurückgestellt.

M5 ist committed. M6 ist weitgehend umgesetzt (siehe unten).

1. Scope, roten Strahl, Projektilrichtung und die bestätigte natürliche
   Waffenkalibrierung unverändert lassen.
2. Offene Live-Regressionen aus `docs/TESTING.md` §2 abarbeiten
   (Fenstermodus/Vollbild, Alt-Tab, Auflösungswechsel, Save/Load,
   Tod/Respawn, Leiter, Knockdown, Debug-Build).
3. Weiterhin offen aus M4/M5: der CPU-Readback im Stereo-HUD-Mischer muss
   GPU-seitig oder als nativer UI-Layer ersetzt werden, und Translation
   braucht Weltkollision, bevor sie Standard werden darf.

## M6: Stand vom 25.07.2026

### Runtime-Unabhängigkeit

Der Mod läuft mit **SteamVR und Virtual Desktop (VDXR)**. Umgeschaltet wird
mit `-Runtime active|steamvr|vdxr|<Manifestpfad>` an den Launchern; gesetzt
wird dabei `XR_RUNTIME_JSON` nur für den Hostprozess. Die systemweite
Einstellung unter `HKLM\SOFTWARE\Khronos\OpenXR\1\ActiveRuntime` wird
bewusst nicht angefasst.

Die SteamVR-Sonderbehandlung (`autoShowGameTheater`, Theaterwächter) läuft
nur noch, wenn die effektive Runtime als SteamVR erkannt wird.

Steam bleibt als **Store** nötig (`-applaunch 21090`); SteamVR selbst nicht.

Nebenbefund: Virtual Desktop registriert auch eine **32-Bit**-OpenXR-Runtime
(`virtualdesktop-openxr-32.json`). Die Prämisse von AD-001 — kein
32-Bit-Runtime-Eintrag vorhanden — gilt damit nicht mehr uneingeschränkt. Die
Zwei-Prozess-Architektur bleibt trotzdem richtig, weil sie runtime-unabhängig
ist und dem ABI-empfindlichen VC7.1-Client keinen modernen OpenXR-Loader in
denselben Prozess lädt.

### Neue Werkzeuge:

- `tools\build-all.ps1` — prüft Abhängigkeiten, baut x86 und x64, führt beide
  Testsuiten aus, schreibt `stage\build-manifest.json`. Der Generator ist auf
  `Visual Studio 17 2022` festgenagelt; ein mit fremdem Generator angelegter
  Buildbaum wird erkannt und neu erzeugt.
- `tools\uninstall-fearvr.ps1` — Trockenlauf als Standard, `-Apply` führt aus.
  `-Scope All|SteamVrOnly|ProjectOnly`, `-KeepLogs`, `-IncludeVendor`,
  `-IncludeUserData`.
- `tools\collect-perf-report.ps1` — wertet ein Laufverzeichnis zu den in
  §14 geforderten Kennzahlen aus, optional `-AsJson`.

Neue Instrumentierung im Host (`perf_frame`, je 300 eingereichte Frames):
XR-Displayrate, Game-FPS, reused frames, Renderzeit links/rechts,
Host-Copyzeit und Handle-Anzahl. `host_start` und `host_stop` tragen
zusätzlich `handles=`.

Nachgewiesenes M6-Gate: deinstalliert, danach allein aus dem Repo neu gebaut
und frisch gestaged; Retail unverändert, Spielstände erhalten. Details in
`docs/TESTING.md` §16.

Gemessen im Lauf `m5-fear-20260725-004241`: XR stabil auf 90 Hz, Spiel rund
50 fps im Stereo, Renderzeit links Ø 147,5 µs und rechts Ø 81,3 µs,
Host-Copyzeit Ø 293,7 µs, ein einziger verworfener Frame, Handles ohne Trend
zwischen 486 und 518. Die 2871 reused frames sind die erwartete Differenz
zwischen 90 Hz Display und ~50 fps Spiel, kein Fehler.

**Wichtig:** `stage\userdata-*` enthält Spielstände, Profile und
Screenshots. Diese Verzeichnisse sind Benutzerdaten und werden von der
Deinstallation nur mit `-IncludeUserData` entfernt. Nicht versehentlich
pauschal `stage\` löschen.

Zwei Fallen, die dabei aufgefallen sind:

- Neue PowerShell-Skripte brauchen eine **UTF-8-BOM**. Ohne sie liest
  PowerShell 5.1 die Datei als ANSI; ein Gedankenstrich zerfällt dann in ein
  typografisches Anführungszeichen, das PowerShell als Stringbegrenzer
  akzeptiert, und das Skript ist nicht mehr parsebar.
- `$LASTEXITCODE` wird von `&` auf ein `.ps1` **nicht** gesetzt und steht noch
  auf dem Ergebnis des letzten nativen Aufrufs. Gerufene Skripte werfen
  selbst; kein Exitcode-Test.

## Einordnung eines früheren Absturzes

Ein einzelner Start unter `logs\m5-fear-20260724-225411` stürzte während eines
Device-Resets in `ClientFx.fxd +0x4ddcf` mit `c0000005` ab. Gleichzeitig lagen
GPU-Watchdog-Ereignisse `LiveKernelEvent 141/117` vor. Ein unveränderter
Wiederholungsstart lief. Solange das nicht reproduzierbar wird, als transienten
GPU-/ClientFx-Fehler behandeln und nicht automatisch dem VR-Menü-Hook
zuschreiben.
