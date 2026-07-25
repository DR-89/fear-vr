# ARCHITECTURE.md — Architektur & Entscheidungslog

Für jede wesentliche Entscheidung wird hier festgehalten (ANWEISUNG.md §17):
**Problem · getestete Optionen · Messung/Quellcodebeleg · gewählte Lösung ·
bekannte Nachteile · Rückfallpfad.**

## 1. Gesamtarchitektur

```text
SteamVR / OpenXR-Runtime (x64)
             ^
             | OpenXR + XR_KHR_D3D11_enable
    fearvr-host.exe (x64, D3D11)
             ^
             | versioniertes IPC:
             | Posen, FOV, Zustände, Events,
             | D3D9/D3D11-Shared-Texture-Handles
             v
 FEAR.exe (x86, D3D9)
   + d3d9.dll Proxy/Bridge (x86)
   + lokal neu gebautes GameClient-Modul (x86)
             v
      LithTech RenderCamera, zweimal pro Frame
```

Komponenten: `src/host64/`, `src/proxy32/`, `src/launcher/`,
`game-source-overlay/` (GameClient), gemeinsamer Vertrag `src/common/protocol.h`.

## 2. Getroffene Entscheidungen

### AD-001 — Getrennter x64-OpenXR-Host statt OpenXR direkt in FEAR.exe

- **Problem:** `FEAR.exe` ist x86. OpenXR-Loader braucht für 32-Bit-Apps den
  Runtime-Eintrag unter `HKLM\SOFTWARE\WOW6432Node\Khronos\OpenXR\1`.
- **Messung/Beleg:** Eintrag fehlt auf dem Zielrechner; SteamVR liefert lokal
  nur `steamxr_win64.json` (siehe `docs/ENVIRONMENT.md`, 2026-07-24).
- **Getestete Optionen:** (a) OpenXR in x86 einbauen — scheitert an fehlender
  32-Bit-Runtime; (b) separater x64-Host mit IPC — tragfähig.
- **Gewählte Lösung:** separater `fearvr-host.exe` (x64), Kopplung über
  versioniertes IPC + D3D9/D3D11-Shared-Textures.
- **Bekannte Nachteile:** Prozess-/Bitness-Grenze, IPC-Komplexität,
  Shared-Texture-Interop.
- **Rückfallpfad:** Flat-Screen ohne Host bleibt jederzeit möglich.

### AD-002 — Statischer OpenXR-Loader mit festem SDK-Pin

- **Problem:** Der Host braucht reproduzierbare Header und einen Loader, darf
  aber keine zufällige DLL neben dem Spiel bevorzugen.
- **Messung/Beleg:** OpenXR-SDK `release-1.1.59` baut unter Windows einen
  statischen Loader; die Runtime-Auswahl bleibt beim registrierten
  `ActiveRuntime`-Manifest.
- **Gewählte Lösung:** lokaler, gitignorierter Checkout auf Commit
  `e5df31de6c15b4900aee3092273194e51282000d`, Prüfung über
  `tools/prepare-dependencies.ps1`, Link gegen `OpenXR::openxr_loader`.
- **Bekannter Nachteil:** Der Loader wird Bestandteil jedes Host-Builds.
- **Rückfallpfad:** Ein dynamischer Loader kann später als separat gepinnte
  Distributionsdatei gebaut werden.

### AD-003 — Zwei Swapchains und OpenXR-1.0-Anwendungsniveau

- **Problem:** M1 muss Links/Rechts eindeutig unterscheiden und auch mit
  älteren, OpenXR-1.0-kompatiblen Runtimes funktionieren.
- **Messung/Beleg:** SteamVR/OpenXR 2.16.7 lehnte eine 1.1-Anforderung mit
  `XR_ERROR_API_VERSION_UNSUPPORTED` ab. Khronos `hello_xr` fordert ebenfalls
  `XR_API_VERSION_1_0` an. Der Live-Test akzeptierte zwei Swapchains mit je
  `1624x1736`.
- **Gewählte Lösung:** OpenXR 1.0 anfordern, `XR_KHR_D3D11_enable` verwenden,
  je Auge eine Swapchain erzeugen und links rot/rechts blau leeren.
- **Bekannte Nachteile:** Zwei Swapchains verursachen mehr Objekte; M1 rendert
  noch keine Spielbilder.
- **Rückfallpfad:** Bei späterem Profiling kann auf eine Array-Swapchain
  gewechselt werden, sofern die Links-/Rechts-Zuordnung gleichwertig testbar
  bleibt.

### AD-004 — Versionierter Drei-Slot-Ring über die Bitnessgrenze

- **Problem:** Das x86-Spiel und der x64-Host dürfen keine rohen Pointer teilen
  und sich im jeweiligen Renderthread nicht gegenseitig blockieren.
- **Messung/Beleg:** Der D3D9Ex-Livetest importierte fortlaufend frische
  Frame-/Generationspaare über dieselbe NVIDIA-Adapter-LUID; Reset,
  Minimieren, Host-Abbruch und Prozessende hingen nicht.
- **Getestete Optionen:** Einzeltextur mit implizitem Timing, CPU-Kopie und
  expliziter Shared-Texture-Ring.
- **Gewählte Lösung:** Drei Slots je Auge mit atomaren Zuständen,
  Frame-ID/Generation, D3D9- und D3D11-Event-Queries sowie privater
  D3D11-Zieltextur im Host. Der IPC-Header ist versioniert und hat in x86/x64
  dieselbe geprüfte Größe.
- **Bekannte Nachteile:** Mehr VRAM und Synchronisationslogik; bei vollem Ring
  werden Frames verworfen.
- **Rückfallpfad:** Ohne Host oder bei Protokoll-/Adapterfehler rendert das
  normale Flat-`Present` weiter.

### AD-005 — Offizielle archcfg-Stage plus spätes D3D9-Hooking

- **Problem:** Eine kopierte Steam-EXE scheitert mit Application Load Error
  `5:0000065434`; `FEARDevSP.exe` fordert ein CD/DVD-Laufwerk. Das offizielle
  GameClient-Modul wird außerdem erst nach Erzeugung des D3D9-Geräts geladen.
- **Messung/Beleg:** Der reale Lauf verifizierte Loader, Originalmodul und
  Bridge aus `stage\m2-game`; ein isolierter Hook-Test fing `Present` und
  `Reset` eines bereits existierenden Geräts ab.
- **Getestete Optionen:** kopierte EXE, Dev-EXE, früher IAT-Hook und offizielle
  `-archcfg`-Modulschicht mit spätem Detour.
- **Gewählte Lösung:** Retail unverändert über Steam starten, eigene
  `-archcfg`-Stage verwenden, originales VC7.1-GameClient über einen
  ABI-neutralen Loader weiterreichen und die bereits vorhandenen
  `Present`-/`Reset`-Ziele mit fest gepinntem MinHook 1.3.4 detouren.
- **Bekannte Nachteile:** Ein später Hook sieht `Direct3DCreate9` und
  `CreateDevice` nicht mehr; Reset-/Present-Ressourcen müssen ohne
  Erzeugungshook sicher erkannt werden.
- **Rückfallpfad:** Stage weglassen und F.E.A.R. unverändert über Steam
  starten. Es gibt keine Remote-Thread-Injection und keinen Retail-Schreibzugriff.

### AD-006 — Temporärer CPU-D3D9Ex-Pfad für klassisches D3D9

- **Problem:** F.E.A.R. verwendet klassisches `IDirect3DDevice9`; eine darauf
  angelegte Shared Texture wurde mit `D3DERR_INVALIDCALL` abgelehnt.
- **Messung/Beleg:** Der D3D9Ex-Testproducer funktioniert GPU-direkt. Der
  klassische D3D9-Test und das echte 1024×768-Spielbild funktionieren nur über
  den protokollierten `path=cpu_d3d9ex`.
- **Getestete Optionen:** direkter Shared-Handle auf D3D9, D3D9Ex-Hilfsgerät
  und kontrollierte CPU-Übertragung zwischen den Geräten.
- **Gewählte Lösung:** Für den M2-Integrationsnachweis
  `GetRenderTargetData`, Zeilenkopie und `UpdateSurface` in eine
  D3D9Ex-Shared-Texture. Das Protokoll setzt ausdrücklich
  `FEARVR_BF_CPU_FALLBACK`.
- **Bekannte Nachteile:** Per-Frame-CPU-Readback, zusätzliche Latenz und
  Bandbreite; die finale Produktionsinvariante ist damit nicht erfüllt.
- **Rückfallpfad:** Bridge deaktivieren oder bis zur GPU-direkten Lösung
  Flat-Screen verwenden. Dieser Pfad darf nicht stillschweigend als
  Release-/Performancepfad gelten.

### AD-007 — Versionsgeprüfter Retail-PlayerCamera-Hook für M3

- **Problem:** Die veröffentlichte `ILTRenderer`-Deklarationsreihenfolge bildet
  die Overloads nicht in der Reihenfolge der Retail-VC7.1-VTable ab. Ein Hook
  des vermeintlichen Slots 15 hatte deshalb eine falsche Signatur.
- **Messung/Beleg:** Eine read-only Laufzeitprobe zeigte, dass Slot 17 den
  Kamera-Handle und `nullptr` pusht und über `vtable+0x4c` an Slot 19
  weiterleitet. Der korrigierte Lauf `logs\m3-fear-20260724-162315` erzeugte
  mindestens 24.900 vollständige Stereo-Frames in rund 11½ Minuten; der
  Benutzer bestätigte korrekte 3D-Tiefenwirkung. Details stehen in
  `STEREO-RESEARCH.md`.
- **Getestete Optionen:** Header-Reihenfolge als Slot-Reihenfolge, Hook der
  Technik-Override-Funktion und Laufzeitzuordnung der Retail-Weiterleitung.
- **Gewählte Lösung:** Nur Slot 17 ersetzen, linkes/rechtes Auge jeweils über
  den unveränderten Slot 19 rendern und die exakte 15-Byte-Weiterleitung vor
  jeder Aktivierung prüfen. Kamera-Pose und FOV werden nach beiden Augen sowie
  im SEH-Fehlerpfad wiederhergestellt.
- **Bekannte Nachteile:** Der Nachweis gilt für die geprüfte Steam-
  Retailfassung. M3 erfasst nur die Welt; HUD und Menü werden im Client danach
  gezeichnet und benötigen die M4-HUD-/Layer-Lösung.
- **Rückfallpfad:** Bei Signaturabweichung, fehlendem Host, ungültigem
  Renderauftrag oder F8-Deaktivierung bleibt beziehungsweise wird die originale
  Slot-17-Weiterleitung aktiv.

### AD-008 — Relatives Headtracking mit bildsynchroner OpenXR-Pose

- **Problem:** Absolute Tracking-Space-Posen dürfen nicht direkt die
  Spielkamera ersetzen. Zusätzlich war das konsumierte Spielbild zwei
  OpenXR-Frames älter als die Pose, die der Host zusammen mit dem Bild
  einreichte; das fühlte sich träge an.
- **Messung/Beleg:** Public-Tools-Achsen ergeben die zentrale Z-Spiegelung in
  `COORDINATE-SYSTEM.md`. Der erste M4-Lauf bestätigte alle Achsen und F9,
  wirkte aber leicht langsam. Der zweite Lauf protokollierte
  `image_pose_matched` mit `request_age_frames=2`; nach Einreichen der zum Bild
  gehörenden Renderpose bewertete der Benutzer das Tracking als „deutlich
  besser“.
- **Gewählte Lösung:** Beim Stereo-Aktivieren und mit F9 wird die
  Augenmittelpose als Recenter gespeichert. Pro Auge werden relative Position
  und Rotation berechnet, nach LithTech konvertiert und lokal auf die
  bestehende Spielkamera komponiert. Ein 256 Einträge großer Host-Ring ordnet
  jeder Renderanforderung ihre OpenXR-Pose/FOV zu; der Compositor erhält
  zusammen mit dem importierten Bild exakt dessen Renderpose und kann korrekt
  zeitwarpen.
- **Bekannte Nachteile:** Der klassische D3D9-CPU-Transfer bleibt teuer.
  Translation besitzt noch keine Weltkollision und bleibt deshalb
  standardmäßig aus.
- **Rückfallpfad:** Bei 250 ms ohne frische Pose wird Mono verwendet; die erste
  wieder gültige Pose wird neu zentriert. F8 stellt den originalen
  Kamera-Renderpfad wieder her.

### AD-009 — Raumfestes Menüpanel und Stereo-HUD-Prototyp

- **Problem:** Das Hauptmenü besitzt keinen Stereo-Weltdurchlauf. Als
  Projektionstextur war es lesbar, folgte in `XR_REFERENCE_SPACE_TYPE_VIEW`
  jedoch störend jeder Kopfbewegung. Im Spiel werden HUD und Pausemenü erst
  nach den beiden Weltbildern gezeichnet und fehlten deshalb im Stereo-Frame.
- **Messung/Beleg:** Die Läufe `170417` und `170754` bestätigten ein lesbares,
  raumfestes Menü sowie funktionierende Übergänge zur Stereo-Spielansicht.
- **Gewählte Lösung:** Mono-Menüs werden beim Eintritt einmal relativ zur
  aktuellen Augenmittelpose verankert und danach als Quad im lokalen
  Referenzraum eingereicht. Ein CPU-Prototyp vergleicht das endgültige
  Present-Bild mit dem rechten Weltbild und kopiert nur nachträglich geänderte
  Pixel identisch in beide Augen. Deltas über 65 Prozent gelten als Menü oder
  Vollbildeffekt und wechseln automatisch auf das raumfeste Panel, damit die
  Welt nicht versehentlich überwiegend mono bleibt.
- **Bekannte Nachteile:** Der HUD-Prototyp benötigt ein zusätzliches
  D3D9-Readback pro Frame und ist kein zulässiger finaler Pfad. Transparente
  UI-Kanten enthalten den Hintergrund des rechten Auges. Nach dem visuellen
  Nachweis muss die Trennung GPU-seitig oder über einen nativen UI-Layer
  erfolgen.
- **Rückfallpfad:** `-NoStereoHud` am M4-Launcher lässt den bestätigten
  Weltstereo-/Menü-Quad-Pfad unverändert; der direkte Shared-Texture-Pfad
  mischt kein HUD.

### AD-010 — SteamVR-Desktop-Theater beim Retail-Start unterdrücken

- **Problem:** F.E.A.R. muss offiziell mit `steam.exe -applaunch 21090`
  gestartet werden. SteamVR erkennt es trotzdem als Desktopspiel und kann
  verzögert eine Theaterfläche über der bereits aktiven OpenXR-Szene öffnen.
- **Messung/Beleg:** `steamvr.vrsettings` nannte
  `valve.steam.desktopgame.21090` als zuletzt verwendete externe Fläche. Der
  Benutzer musste sie bei mehreren Läufen manuell schließen. Selbst der
  dokumentierte Benutzerwert `steamvr.autoShowGameTheater=false` verhinderte
  die verspätete Einblendung in einem Lauf nicht zuverlässig.
- **Gewählte Lösung:** Der Launcher setzt den Benutzerwert weiterhin auf
  `false` und sichert die vorherige Konfiguration im Projekt. Zusätzlich
  beobachtet ein versteckter, auf den neuen F.E.A.R.-Prozess begrenzter
  Wächter höchstens fünf Minuten gezielt
  `valve.steam.desktopgame.21090`. Wird diese Fläche erstmals sichtbar,
  beendet er zunächst den Theatermodus mit dem Compositor-Befehl
  `disable_theater_mode` und schließt anschließend das verbleibende Dashboard
  mit `vrcmd --hidedashboard`.
- **Bekannte Nachteile:** Der Wächter hängt von den SteamVR-internen
  Bezeichnungen `valve.steam.desktopgame.21090` und `vrcmd` ab und muss nach
  größeren SteamVR-Änderungen erneut geprüft werden. Das normale Dashboard
  wird nicht allein aufgrund seiner Sichtbarkeit geschlossen; Auslöser ist
  ausschließlich die F.E.A.R.-Theaterfläche.
- **Rückfallpfad:** Der Wächter endet bei Spielende oder Timeout. F.E.A.R.,
  SteamVR und der OpenXR-Host werden nicht beendet oder umgangen.

### AD-011 — M4-Komfortoptionen für Bob, Shake und Zwischensequenzen

- **Problem:** Head-Bob, Camera-Shakes und geskriptete Kamerafahrten können in
  VR unangenehm sein; globale Abschaltung aller ClientFX würde dagegen
  Partikel und Spielwirkung beschädigen.
- **Gewählte Lösung:** `-NoHeadBob` setzt ausschließlich die offiziellen
  `HeadBobDebugMode`-/Amplitude-Konsolenvariablen für Kamera und Waffe.
  F10 setzt im Renderauftrag `FEARVR_RF_FLATSCREEN`, verwendet den normalen
  einmaligen Welt-Render und zeigt ihn raumfest als Quad. Beim Verlassen wird
  neu zentriert. Fehlt in einem Zustand ein vollständiges Stereo-Weltbild,
  wird der Stereo-Status automatisch gelöscht und derselbe Panelpfad genutzt.
- **Bekannte Nachteile:** F10 stabilisiert das gesamte Bild, statt einzelne
  CameraShakeFX selektiv herauszufiltern. Der Benutzer entscheidet daher
  bewusst pro problematischer Szene.
- **Rückfallpfad:** Beide Funktionen sind optional; ohne `-NoHeadBob` bleibt
  das Original-Bob aktiv, F10 kann jederzeit zurückgeschaltet werden.

### AD-012 — OpenXR-Actions über bidirektionales IPC

- **Problem:** Der OpenXR-Host ist x64, der Retail-Client x86. Betriebssystem-
  Tastensynthese würde Benutzerbelegungen umgehen und könnte bei Fokusverlust
  Tasten festhalten.
- **Gewählte Lösung:** Protokoll v3 ergänzt einen separaten
  `FearVrInputState`-Seqlock vom Host zum Spiel und einen
  `FearVrHapticRequest`-Seqlock zurück. Der Host synchronisiert ein
  profilübergreifendes OpenXR-Action-Set nur bei Fokus. Der Loader pollt den
  Zustand direkt aus dem belegten `IClientShell::Update`-Slot 20.
- **Sicherheit:** Fehlender Fokus, ungültige Daten oder eine länger als 250 ms
  unveränderte Probe werden vollständig neutralisiert. Maus, Tastatur und
  bestehende Gamepadpfade bleiben unangetastet.
- **Einführungsstufe:** Zunächst waren nur rechter Stick-Klick für Recenter und
  die rechte Primärtaste für einen Haptik-Probeimpuls aktiv. Nach bestätigter
  Rohdatenabnahme wurde die vollständige semantische Spielbelegung ergänzt.
- **Details:** `docs/OPENXR-INPUT.md`.

### AD-013 — Motion-Controlled Aiming und Handdarstellung

- **Problem:** §13 verbietet die Behauptung „6DoF-Waffe“, solange Schuss- und
  Waffenrichtung nicht nachweislich übereinstimmen. Zusätzlich zeigte der
  Ego-Blick Ober- und Unterarm, was die schwebende Waffenhaltung zerstörte.
- **Gewählte Lösung:** Der `RightHand`-Socket des Retail-Player-Body folgt der
  OpenXR-Grip-Pose, und die sichtbare Waffe erhält nach dem Retail-Update
  denselben Transform. Beides nutzt dieselbe Aim-Rotation wie die Projektile.
  Der rote Zielstrahl macht diese Übereinstimmung im Spiel sichtbar und
  prüfbar.
- **Nachweis:** Benutzerabnahme am 24.07.2026 — Zielstrahl und
  Scope-Ausrichtung stimmen mit dem Trefferpunkt überein.
- **Handdarstellung:** `chars\models\player.Model00p` liefert vier Pieces ohne
  Namen. Sichtbarkeit läuft deshalb über den Piece-Index. Piece #1 trägt die
  Arme und wird ausgeblendet; Hände und Waffe bleiben sichtbar.
- **Bekannte Nachteile:** Knochen zu skalieren oder zu verschieben scheidet
  aus. Node-Control liefert nur einen `LTRigidTransform`, und ein Kollabieren
  der Armknochen erzeugt bei geskinnten Meshes einen sichtbaren Splitter vom
  Oberkörper zur Hand.
- **Rückfallpfad:** `HiddenBodyPieces` in `fearvr.ini` und die F11-Probe
  erlauben eine Neukalibrierung, falls ein Build eine andere Piece-Reihenfolge
  liefert.
- **Details:** `docs/OPENXR-INPUT.md`, `docs/TESTING.md` §12.

### AD-014 — VR-Einstellungen als kurze Seite in der nativen Menüliste

- **Problem:** Die VR-Optionen brauchen eine im Headset bedienbare Oberfläche.
  Ein eigenes Overlay hätte Fokus, Eingabe und Pausenzustand doppelt verwalten
  müssen. In einer längeren Liste sprang die Auswahl zudem sichtbar.
- **Messung/Beleg:** Byte-Signaturen von `CMenuSystem::Init`, `OnCommand`,
  `OnFocus`, `CBaseMenu::AddControl` und `CLTGUIListCtrl` wurden gegen Retail
  1.08 geprüft; `logs\m5-fear-20260724-222748` bestätigt Hooks und Menüaufbau.
  Der Sprung ließ sich im Public-Tools-Quelltext festnageln:
  `CLTGUIListCtrl::SetSelection` summiert beim Herunterscrollen rückwärts die
  `GetBaseHeight()` **aller** Controls, ohne `IsVisible()` zu prüfen, während
  `CalculatePositions()` unsichtbare Controls überspringt. Jeder Umschalter
  besitzt ein verstecktes Geschwister-Control, also wird `m_nFirstShown` falsch
  gesetzt.
- **Getestete Optionen:** eigenes VR-Overlay, zweistufiges Menü mit
  `MORE SETTINGS >`, kleinere Fonthöhe über `SetFontHeight`, zusätzliches
  `Enable(false)` auf versteckten Controls.
- **Gewählte Lösung:** ein englisch beschrifteter Eintrag `VR SETTINGS` direkt
  hinter „Optionen“, als **eine** kurze Seite mit acht Einträgen, die
  vollständig in den nativen Rahmen passt. Solange die Seite aktiv ist, wird
  der Listenanfang in jedem Client-Update auf 0 festgehalten — nicht nur im
  eigenen Hook, weil Tastatur, Maus und Controller alle direkt über
  `NextSelection` navigieren. Geschrieben wird nur bei tatsächlicher Abweichung.
- **Bekannte Nachteile:** Selten benutzte Optionen (HMD-Translation, Head-Bob,
  Komfortbildschirm) sind nur über `fearvr.ini` erreichbar. Die Lösung hängt an
  den geprüften Retail-Offsets und ist damit buildgebunden.
- **Verworfen mit Begründung:** `SetFontHeight` verringert die Basishöhe der
  Listeneinträge nicht. `Enable(false)` ist wirkungslos, weil
  `CLTGUICtrl::IsEnabled()` bereits `m_bEnabled && IsVisible()` ist, und würde
  beim Wiedereinblenden statische Controls auswählbar machen.
- **Rückfallpfad:** Bei Signaturabweichung wird kein Menü-Hook installiert; das
  ESC-Menü bleibt unverändert und alle Werte weiterhin über `fearvr.ini`
  einstellbar.
- **Details:** `docs/OPENXR-INPUT.md`, `docs/TESTING.md` §11 und §14.

### AD-015 — Lehnen über die Rolllage der linken Hand

- **Problem:** Für `CLeanMgr` fehlte nach der vollständigen Belegung eine freie
  Taste. Die linke Menütaste scheidet aus, weil SteamVR sie für das eigene
  Systemmenü abfängt.
- **Messung/Beleg:** `logs\m5-fear-20260724-235013` zeigte zwei echte
  Grenzfälle: Rolllagen von 177,3°, 136,3° und −169,3° lösten ein volles Lehnen
  aus, jeweils bei steil nach unten zeigender Aim-Pose, deren Rolllage
  numerisch bedeutungslos ist.
- **Gewählte Lösung:** `PoseRollRadians` liest die Rolllage der linken
  Aim-Pose um deren eigene Vorwärtsachse aus den Welt-Hoch-Anteilen der lokalen
  X- und Y-Achse. `LeftHandLeanDirection` liefert −1, 0 oder +1 und bildet auf
  die digitalen Retail-Kommandos `LEAN_LEFT` (20) und `LEAN_RIGHT` (21) ab;
  `CLeanMgr` blendet selbst weich ein und aus. Untergrenze 0,42 rad (~24°),
  Obergrenze 1,75 rad (~100°) gegen gedrehte Hände, zusätzlich `PoseLevelness`
  ≥ 0,5 gegen hängende Hände.
- **Bekannte Nachteile:** Beim Zielen in steilem Winkel ist bewusst kein Lehnen
  möglich. Die Schwellen sind empirisch und nicht einstellbar.
- **Rückfallpfad:** Ohne gültige linke Aim-Pose ist die Rolllage 0; das Lehnen
  löst sich bei Trackingverlust, statt hängen zu bleiben.
- **Details:** `docs/OPENXR-INPUT.md`, `docs/TESTING.md` §13.

### AD-016 — Deinstallation entfernt Moddateien, keine Benutzerdaten

- **Problem:** Das M6-Gate verlangt, dass eine Deinstallation nur Projekt- und
  Moddateien entfernt. Der naheliegende Weg — `stage\` komplett löschen — wäre
  falsch.
- **Messung/Beleg:** `stage\userdata-*` ist das `-userdirectory`, das der
  Launcher an `FEAR.exe` übergibt. Eine Bestandsaufnahme am 25.07.2026 fand
  dort Spielstände (`Quick.sav`, `Reload.sav`), `Profile000.gdb`, Screenshots
  und `fearvr.ini` — zusammen rund 310 MB über zehn Verzeichnisse.
- **Gewählte Lösung:** `stage\` wird eintragsweise geleert;
  `userdata-*`-Verzeichnisse bleiben erhalten und verschwinden nur mit
  `-IncludeUserData`. Ohne `-Apply` ist der Lauf ein Trockenlauf.
- **Externe Änderungen:** Außerhalb der Projektwurzel schreibt der Mod genau
  `steamvr.autoShowGameTheater`. Zurückgesetzt wird gezielt dieser Schlüssel
  aus der ältesten Sicherung, nicht die ganze Datei — sonst gingen alle
  SteamVR-Einstellungen verloren, die seither entstanden sind. War der
  Schlüssel ursprünglich nicht vorhanden, wird die eingefügte Zeile entfernt.
  Es gibt keine Registry-Änderung und keinen Retail-Schreibzugriff.
- **Bekannte Nachteile:** Läuft SteamVR noch, überschreibt es seine
  Konfiguration beim Beenden. Das Skript warnt und bietet
  `-Scope ProjectOnly` für genau diesen Fall.
- **Rückfallpfad:** Der Trockenlauf ist der Standard; jeder Schritt ist einzeln
  über `-Scope`, `-KeepLogs`, `-IncludeVendor` und `-IncludeUserData` steuerbar.
- **Details:** `docs/TESTING.md` §16.

### AD-017 — Prozessreproduzierbarer Build statt bitgleicher Artefakte

- **Problem:** §13 fordert für M6 „reproduzierbare x86-/x64-Artefakte“.
- **Messung/Beleg:** Kontrollversuch am 25.07.2026: `build\x86` zweimal
  vollständig gelöscht, neu konfiguriert und gebaut, beide Male auf demselben
  Commit und mit identischen Quellen. `GameClient.dll` ergab `9AD461AE…` und
  `4FF34D75…`, `fearvr-d3d9.dll` `FAD56D7E…` und `91DB685B…`. MSVC bettet
  Zeitstempel und PDB-GUIDs ein; `/Brepro` ist für diese Toolchain und den
  v141-/VC7.1-Mischbetrieb nicht durchgängig verfügbar.
- **Gewählte Lösung:** `tools\build-all.ps1` als einziger Einstiegspunkt.
  Reproduzierbar sind Vorgang und Eingangsgrößen, nicht die Bytes: Das
  Manifest hält Git-Commit, Konfiguration, CMake-Version, Retail-Hash und die
  Hashes der erzeugten Dateien fest und meldet einen unsauberen Arbeitsbaum.
- **Bekannter Nachteil:** Ein Artefakt lässt sich nicht allein über seinen Hash
  einem Commit zuordnen; dafür ist das Manifest nötig.
- **Rückfallpfad:** Die getrennten CMake-Aufrufe bleiben dokumentiert und
  funktionsfähig.
- **Details:** `README.md`, `docs/TESTING.md` §16.

## 3. Noch zu dokumentieren (Pflicht laut §17)

- [x] ob und wie `RenderCamera` zweimal **sicher** aufgerufen wird → `STEREO-RESEARCH.md`
- [x] HUD-Trennung (Welt-HUD vs. Menü-HUD, Quad-Layer) → AD-009
- [x] symmetrische vs. asymmetrische Projektion → `COORDINATE-SYSTEM.md`
- [x] D3D9/D3D11-Format und Synchronisation (Query-Event, Ringpuffer)
      → `M2-D3D9-BRIDGE.md`, AD-004 bis AD-006
- [x] Koordinatenkonversion → `COORDINATE-SYSTEM.md`
- [x] Verhalten bei CameraFX / Zwischensequenzen (Komfortmodus) → AD-011
- [x] Abgrenzung zu Motion-Controlled Aiming (erst ab M5, mit Nachweis)
      → AD-013

## 4. Nicht verhandelbare Invarianten (§3)

- Keine Schreibzugriffe auf das Retail-Verzeichnis; Originaldateien nie
  überschreiben.
- Keine OpenXR-/D3D-/Thread-/IPC-Initialisierung in `DllMain`.
- Keine fest codierten Binäroffsets ohne EXE-Hash-/Signaturprüfung
  (unbekannter Build ⇒ Feature deaktiviert).
- Nur der **Welt-Renderdurchlauf** darf pro Auge wiederholt werden — niemals
  Simulation, KI, Partikel, Sound, Eingabe oder Spielzeit doppelt.
- Kein per-Frame-CPU-Readback im finalen Pfad.
- Flat-Screen-Start ohne Host muss immer funktionieren.
