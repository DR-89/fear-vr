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
- **Nachtrag 25.07.2026:** Die Prämisse gilt nicht mehr uneingeschränkt. Der
  Virtual Desktop Streamer registriert zusätzlich eine 32-Bit-Runtime
  (`virtualdesktop-openxr-32.json`), sodass OpenXR in `FEAR.exe` technisch
  möglich wäre. Die Zwei-Prozess-Architektur bleibt trotzdem: Sie ist
  runtime-unabhängig, funktioniert auch mit SteamVR, und der ABI-empfindliche
  VC7.1-Client bekäme sonst einen modernen OpenXR-Loader in denselben Prozess.
  Siehe AD-018.

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
- **Produktionsoptimierung:** Ein eigener `d3d9.dll`-Proxy neben `FEAR.exe`
  sieht `CreateDevice` garantiert früh und ergänzt
  `D3DCREATE_MULTITHREADED`. Der Present-Thread kopiert das fertige Auge nur
  noch GPU-intern in einen gepufferten Render-Target-Slot. Ein Worker wartet
  auf dessen GPU-Query, führt `GetRenderTargetData`, Zeilenkopie,
  `UpdateSurface` und die Slot-Veröffentlichung aus. Es wartet höchstens ein
  Bild; ein neueres ersetzt das noch nicht gelesene alte Bild
  („latest frame wins“). Ein vorhandener fremder D3D9-Wrapper wird in der
  Entwicklungsinstallation als `d3d9.fearvr-upstream.dll` gesichert und von
  der Bridge weiterverkettet.
- **Messung/Beleg:** Vor der Umstellung blockierte der Transfer F.E.A.R.s
  Present-Thread im echten 1920×1080-Stereolauf durchschnittlich 16–17,5 ms
  (Spitzen bis etwa 28 ms). Im asynchronen Lauf
  `logs\m5-fear-20260731-003009` sank die Arbeit im Present-Thread auf
  ungefähr 0,17–0,31 ms. Der verbleibende Readback/Upload läuft mit ungefähr
  14 ms im Worker. Der Benutzer bewertete den Lauf als „richtig gut“.
- **Bekannte Nachteile:** Per-Frame-CPU-Readback, zusätzliche Latenz und
  Bandbreite bleiben; die finale Nullkopie-Invariante ist damit nicht erfüllt.
  Seit dem GPU-Kompositor sind es zwei Readbacks pro Bild statt drei — je
  einer pro Auge, und die sind der Preis des klassischen Geräts, nicht des
  HUDs. Weg wären sie erst, wenn das Spielgerät selbst ein D3D9Ex-Gerät
  würde: Dann greift der bereits vorhandene `DirectShared`-Pfad
  (`bridge.cpp`, Abfrage per `QueryInterface` auf `IDirect3DDevice9Ex`) ohne
  jede CPU-Kopie. Der Preis dafür ist ein Wrapper für Texturen, Surfaces und
  Buffer, weil `D3DPOOL_MANAGED` auf Ex-Geräten nicht existiert und über
  `D3DPOOL_DEFAULT` plus SYSTEMMEM-Schattenkopie nachgebaut werden müsste.
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
- **Stabiler Erststart:** Eine Ansicht darf den Kopf-/Körperanker erst setzen,
  wenn OpenXR Position und Orientierung nicht nur als gültig, sondern auch als
  tatsächlich getrackt meldet. Ein angekündigter Wechsel des verwendeten
  `LOCAL`-Reference-Space wird zum angegebenen XR-Zeitpunkt übernommen; die
  erste vollständig getrackte Pose im neuen Ursprung erhöht über Host und
  Bridge die Recenter-Generation. Damit kann kein vorläufiger Erststart-
  Ursprung bis zum Prozessneustart am Körper hängen bleiben.
- **Lokomotionsmessung seit Protokoll v6:** Der x86-Client veröffentlicht
  zusätzlich die Basis-Spielkamera jedes Stereo-Bilds sowie die neueste
  gerenderte Spielkamera. Der Host misst daraus den Weg, den das noch
  angezeigte Bild hinter der aktuellen Spielkamera liegt. Die Quellpose wird
  damit bewusst **nicht** verschoben: Ohne Tiefenpuffer würde eine
  Projektions-Layer-Translation nahe und ferne Objekte gleich behandeln und
  erzeugte im Live-Test kurzzeitige Mehrfachbilder bei Stick-Lokomotion.
  Vertikale Kamerabewegung, mehr als 50 cm Abstand, mehr als 16 Frames Alter
  und eine gleichzeitige Basisdrehung über ungefähr 5 Grad werden für die
  Telemetrie verworfen.
- **Bekannte Nachteile:** Der klassische D3D9-CPU-Transfer bleibt teuer.
  Korrekte positionelle Nachprojektion benötigt einen zum Bild gehörenden
  Tiefenpuffer; bis dahin wird Stick-Latenz durch frischere Bildübertragung
  statt durch tiefenlose Layer-Translation reduziert.
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
- **Seit 26.07.2026 auf der GPU:** Der Vergleich läuft als `ps_2_0`-Shader auf
  dem Gerät des Spiels (`GpuHudCompositor` in `src/proxy32/bridge.cpp`). Der
  Deckungsgrad, der Vollbildeffekte vom HUD trennt, entsteht über eine
  Reduktionskette (4× je Durchlauf, bis beide Kanten ≤ 128) und wird um genau
  ein Bild verzögert gelesen — einige Kilobyte statt eines Vollbildes, ohne
  Synchronisationspunkt. Damit entfällt ein Readback pro Bild und die gesamte
  Pixelarbeit auf der CPU. Der alte CPU-Mischer bleibt als automatischer
  Rückfall erhalten; `-fearvr-no-gpu-hud` erzwingt ihn.
- **Bekannte Nachteile:** Transparente UI-Kanten enthalten weiterhin den
  Hintergrund des rechten Auges — das ist dem Verfahren inhärent und braucht
  einen nativen UI-Layer. Der verbleibende Transfer-Readback ist **nicht** dem
  HUD anzulasten, sondern dem klassischen D3D9-Gerät des Spiels (siehe AD-004).
- **Rückfallpfad:** `-NoStereoHud` am M4-Launcher lässt den bestätigten
  Weltstereo-/Menü-Quad-Pfad unverändert; der direkte Shared-Texture-Pfad
  mischt kein HUD.

### AD-010 — SteamVR-Desktop-Theater beim Retail-Start unterdrücken

- **Status 31.07.2026:** Wieder aktiv, nachdem das Problem auf zwei Rechnern
  erneut auftrat. Die beiden separaten Theater-Hilfsskripte bleiben entfernt;
  Einstellung und kurzer Wächter sind direkt in `play.ps1` integriert.
- **Problem:** F.E.A.R. muss offiziell mit `steam.exe -applaunch 21090`
  gestartet werden. SteamVR erkennt es trotzdem als Desktopspiel und kann
  verzögert eine Theaterfläche über der bereits aktiven OpenXR-Szene öffnen.
- **Messung/Beleg:** `steamvr.vrsettings` nannte
  `valve.steam.desktopgame.21090` als zuletzt verwendete externe Fläche. Der
  Benutzer musste sie bei mehreren Läufen manuell schließen. Selbst der
  dokumentierte Benutzerwert `steamvr.autoShowGameTheater=false` verhinderte
  die verspätete Einblendung in einem Lauf nicht zuverlässig.
- **Gewählte Lösung:** Der Launcher setzt den Benutzerwert auf `false` und
  sichert eine tatsächlich geänderte Konfiguration im jeweiligen Lauf-Log.
  Zusätzlich beobachtet eine versteckte Instanz desselben Launchers, begrenzt
  auf den neuen F.E.A.R.-Prozess und höchstens 20 Sekunden, gezielt
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
- **Gewählte Lösung:** Head-Bob ist standardmäßig aus. `HeadBob=1` kann die
  offiziellen Kamera-Amplituden wiederherstellen, während die
  Waffen-Amplituden für stabiles VR-Zielen immer null bleiben. `-NoHeadBob`
  erzwingt Kamera und Waffe aus.
  F10 setzt im Renderauftrag `FEARVR_RF_FLATSCREEN`, verwendet den normalen
  einmaligen Welt-Render und zeigt ihn raumfest als Quad. Beim Verlassen wird
  neu zentriert. Fehlt in einem Zustand ein vollständiges Stereo-Weltbild,
  wird der Stereo-Status automatisch gelöscht und derselbe Panelpfad genutzt.
- **Bekannte Nachteile:** F10 stabilisiert das gesamte Bild, statt einzelne
  CameraShakeFX selektiv herauszufiltern. Der Benutzer entscheidet daher
  bewusst pro problematischer Szene.
- **Rückfallpfad:** Kamera-Bob kann mit `HeadBob=1` in `fearvr.ini` bewusst
  aktiviert werden; F10 kann jederzeit zurückgeschaltet werden.

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
  Namen; Piece #1 ist `Body_Group` und enthält Arme, Torso und Beine gemeinsam.
  Der Stage-Generator liest deshalb die Rendermesh-Geometrie des lokal
  installierten Modells, bestimmt die sechs Arm-Komponenten (beide Seiten und
  drei LODs) und rasterisiert deren UV-Dreiecke in die Alphaebene einer lokal
  erzeugten DXT3-Textur. Eine Kontrollmaske stellt sicher, dass keine Hand-UVs
  getroffen werden. Der Generator baut die Alphaebene aller elf DDS-Mipmaps
  neu auf; die kleineren Mips der undurchsichtigen Retail-Textur dürfen nicht
  übernommen werden, weil ihre ursprünglich ungenutzten Alpha-Werte Körper und
  Hände beim Alpha-Test verwerfen würden. Das Material übernimmt
  `SurfaceFlags=0` von Retails funktionierenden Alpha-Test-Körpermaterialien.
- **Arm-IK:** Ober- und Unterarm verwenden einen analytischen Zwei-Knochen-
  Solver mit gemessenen Retail-Knochenlaengen. Ein koerperrelativer Polvektor
  fuehrt beide Ellenbogen nach aussen, unten und leicht hinter den Brustkorb;
  die zuletzt gueltige Beugehemisphaere bleibt an gestreckten oder zum
  Polvektor parallelen Posen erhalten. Der Hand-Socket wird anschliessend
  weiterhin exakt auf die OpenXR-Grip-Pose gesetzt.
- **Live-Kalibrierung:** Das schwebende IK-Menue kann die gemeinsamen
  koerperrelativen Polkomponenten und den linken Handversatz zur Laufzeit
  veraendern. Die linke sichtbare Hand verwendet fuer Position und Rotation
  dieselbe OpenXR-Grip-Pose; ein lokaler Sechs-Achsen-Versatz wird erst danach
  angewendet und veraendert weder Waffensteuerung noch Schussachse.
- **Bekannte Nachteile:** Knochen zu skalieren oder zu verschieben scheidet
  aus. Node-Control liefert nur einen `LTRigidTransform`, und ein Kollabieren
  der Armknochen erzeugt bei geskinnten Meshes einen sichtbaren Splitter vom
  Oberkörper zur Hand.
- **Arm-Sichtbarkeit:** Standard ist `Show arms: OFF` bzw. `ShowArms=0`.
  Das erzeugte Alpha-Test-Material entfernt nur Ober- und Unterarme aus dem
  gemeinsamen Atlas; Hände, Torso und Beine bleiben sichtbar. `ON` setzt das
  unveränderte Retail-Material ein. Die Wahl wird sofort gespeichert.
  `HiddenBodyPieces` und F11 bleiben nur als Entwicklerdiagnose erhalten.
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
- **Gewählte Lösung:** ein englisch beschrifteter Eintrag `VR Settings` direkt
  hinter „Optionen“, als **eine** kompakte Seite mit elf Einträgen, die
  vollständig in den nativen Rahmen passt. Solange die Seite aktiv ist, wird
  der Listenanfang in jedem Client-Update auf 0 festgehalten — nicht nur im
  eigenen Hook, weil Tastatur, Maus und Controller alle direkt über
  `NextSelection` navigieren. Geschrieben wird nur bei tatsächlicher Abweichung.
- **Bekannte Nachteile:** Selten benutzte Optionen (HMD-Translation, Head-Bob,
  Komfortbildschirm und einzelne Nahkampfaktionen) sind nur über `fearvr.ini`
  erreichbar. Die Lösung hängt an den geprüften Retail-Offsets und ist damit
  buildgebunden.
- **Verworfen mit Begründung:** `SetFontHeight` verringert die Basishöhe der
  Listeneinträge nicht. `Enable(false)` ist wirkungslos, weil
  `CLTGUICtrl::IsEnabled()` bereits `m_bEnabled && IsVisible()` ist, und würde
  beim Wiedereinblenden statische Controls auswählbar machen.
- **Rückfallpfad:** Bei Signaturabweichung wird kein Menü-Hook installiert; das
  ESC-Menü bleibt unverändert und alle Werte weiterhin über `fearvr.ini`
  einstellbar.
- **Nachtrag 29.07.2026:** Die einzelne Seite wurde durch eine
  Kategorieübersicht mit sechs kurzen Unterseiten ersetzt. Damit sind die
  bisher nur in `fearvr.ini` erreichbaren Komfort- und Melee-Schalter sowie
  sichere Presets für globale oder waffenspezifische Weight-Profile direkt im
  Spiel einstellbar. Jede Unterseite passt weiterhin vollständig in den
  nativen Rahmen; Root-/Back-Zustand und Presetwahl liegen als unabhängig
  testbares Modell in `src/common/vr_menu_model.h`.
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
- **Externe Änderungen:** Der aktuelle Entwicklungsstart schreibt weder in die
  SteamVR-Konfiguration noch in die Registry oder Retail-Installation. Der
  Deinstaller behält nur die gezielte Wiederherstellung alter
  `autoShowGameTheater`-Sicherungen für frühere Revisionen.
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

### AD-018 — Runtime-unabhängiger Betrieb, Umschaltung per XR_RUNTIME_JSON

- **Problem:** Der Mod soll auch über Virtual Desktop laufen und dabei nicht
  von SteamVR abhängen. Der Host bleibt reines OpenXR. Die Theaterbehandlung
  wird nur aktiv, wenn SteamVR beim Spielstart tatsächlich läuft; ein reiner
  VDXR-Lauf startet keinen SteamVR-Prozess.
- **Messung/Beleg:** Mit `ActiveRuntime` auf VDXR meldete
  `fearvr-host.exe --validate-only` am 25.07.2026 `VirtualDesktopXR 1.0.10`,
  erkannte `Meta Quest 3`, wählte dieselbe Adapter-LUID `0x0:D57B` und
  erzeugte zwei Swapchains mit `2688x2880` — gegenüber `2064x2208` unter
  SteamVR. Exitcode 0. Ein vollständiger Spielstart mit `-Runtime vdxr` lief
  ohne SteamVR.
- **Getestete Optionen:** (a) `HKLM\...\Khronos\OpenXR\1\ActiveRuntime`
  umschreiben — verworfen, das ist eine systemweite Einstellung und erfordert
  Administratorrechte; (b) `XR_RUNTIME_JSON` nur für den Hostprozess setzen.
- **Gewählte Lösung:** `-Runtime active|steamvr|vdxr|<Manifestpfad>` an den
  Launchern. Die Auflösung sitzt in `Resolve-OpenXrRuntime`
  (`tools\_fearvr-env.ps1`); die Umgebungsvariable wird nur um den
  `Start-Process`-Aufruf herum gesetzt und danach zurückgestellt. Die
  SteamVR-Schritte laufen ausschließlich, wenn die effektive Runtime als
  `steamvr` erkannt wurde.
- **Bekannte Nachteile:** Die Erkennung liest den `name` aus dem
  Runtime-Manifest; benennt ein Hersteller seine Runtime um, greift der
  SteamVR-Zweig nicht mehr. Das ist harmlos — er ist reine Kosmetik.
- **Abgrenzung:** Steam bleibt als **Store** nötig, weil F.E.A.R. offiziell
  über `steam.exe -applaunch 21090` startet. Das ist unabhängig von der
  VR-Runtime; SteamVR selbst muss nicht laufen.
- **Rückfallpfad:** Ohne `-Runtime` gilt unverändert die systemweite
  Einstellung.

### AD-019 — Direkt entpackbares Retail-Overlay

- **Problem:** Der Release soll ohne separaten Mod-Installer direkt über eine
  vorhandene F.E.A.R.-1.08-Installation entpackt werden. Fünf Laufzeitdateien
  stammen jedoch aus den Public Tools und deren EULA erlaubt keine öffentliche
  Weitergabe.
- **Gewählte Lösung:** Das ZIP enthält `FEARVR\` sowie zwei Doppelklick-Starter
  und ersetzt keine Retail-Datei. `prepare-overlay.ps1` erzeugt `archcfg` und
  Deployment-Metadaten am endgültigen Ort. Das öffentliche Paket ergänzt die
  fünf Public-Tools-Module beim ersten Start aus der lokalen Installation des
  Besitzers.
- **Privater Komplettmodus:** `tools\make-release.ps1 -PrivateBundle` nimmt
  lokale Public-Tools-Module und abgeleitete Body-Assets auf. Damit ist das ZIP
  nach dem Entpacken sofort startbar; Manifest und Hinweisdatei markieren es
  ausdrücklich als nicht weiterverteilbar.
- **SteamVR:** Es gibt keinen getrennten SteamVR-Renderer. Derselbe x64-
  OpenXR-Host wird über Valves `steamxr_win64.json` an die SteamVR-Runtime
  gebunden; ein eigener Starter erzwingt diesen Pfad.
- **Details:** `tools\release\README-PACKAGE.md`, `docs/TESTING.md` §17.

### AD-020 — Installationsziel nicht unterhalb von %LOCALAPPDATA%

- **Problem:** Der erste Paketstand installierte nach `%LOCALAPPDATA%\FearVR`.
  Das Spiel brach dort mit „Failed to initialize client - unable to load game
  resources" ab; im Headset blieb der rote Ersatzbildschirm des Hosts stehen.
- **Messung/Beleg:** Der Fehler haengt allein am Ort der Archivkonfiguration.
  Byteweiser Vergleich: Die Dateien sind bis auf den Modulpfad identisch, beide
  608 Bytes, gleiche Zeilenenden, gleiche ACLs, keine alternativen Datenstroeme.
  Kreuztests am 25.07.2026:

  | Ort der archcfg | Ergebnis |
  |---|---|
  | `%LOCALAPPDATA%\FearVR\` | Error |
  | `%LOCALAPPDATA%\FearVrCfgTest\` (leerer Ordner) | Error |
  | `%LOCALAPPDATA%\Temp\` | startet |
  | `%USERPROFILE%\FearVR\` | startet |
  | Projektordner | startet |

  Dieselbe Datei startet aus `Temp` und scheitert aus `FearVR`; das
  Modulverzeichnis darf dagegen unter `%LOCALAPPDATA%` liegen.
- **Gewählte Lösung:** Standardziel `%USERPROFILE%\FearVR`. `install.ps1` lehnt
  Ziele unterhalb von `%LOCALAPPDATA%` mit einer erklaerenden Meldung ab.
- **Offen:** Die Ursache im Retail-Binary ist nicht geklaert. Dokumentiert ist
  die gemessene Regel, keine Erklaerung.

### AD-021 — OpenXR-Auftrag als Frame-Takt, neuestes fertiges Bild gewinnt

- **Problem:** VDXR nahm weiterhin 90 Bilder pro Sekunde entgegen, während
  schnelle Controllerbewegungen sichtbare Doppelbilder erzeugten. Der
  Classic-D3D9-CPU/D3D9Ex-Pfad behielt bei einem belegten Ausgabeslot das
  älteste fertige Bild am Kopf der Queue. FEAR renderte außerdem mehrere
  Stereopaare mit derselben OpenXR-Auftrags-ID.
- **Messung/Beleg:** Lauf `fearvr-20260731-063123` zeigte bei 90 XR-fps
  45–87 als `reused` bezeichnete Einreichungen je 300 Frames und beim ersten
  passenden Stereobild sieben Auftragsframes Alter. Der damalige Zähler
  verglich allerdings nur `frameId` und vermischte dadurch eine neue
  Texturgeneration mit derselben Pose mit einem wirklich wiederverwendeten
  Bild.
- **Getestete Optionen:** (a) ein festes 90-fps-Limit — verworfen, weil die
  Runtime auch 72, 80 oder 120 Hz liefern kann und ein freilaufendes Limit
  nicht phasengleich zu `xrWaitFrame` ist; (b) synchroner Readback in
  `Present` — nur Diagnose, weil er den Spielthread blockiert; (c) den
  neuesten OpenXR-Auftrag als Taktgeber verwenden und die ohnehin anfallende
  Transferarbeit in dessen begrenztes Wartefenster legen.
- **Gewählte Lösung:** Der Host signalisiert nach jedem vollständig
  veröffentlichten Renderauftrag ein eigenes Auto-Reset-Event. Will FEAR
  dieselbe Auftrags-ID erneut rendern, wartet die Bridge höchstens 20 ms.
  Während dieses begrenzten Fensters werden fertige D3D9-Stagingkopien
  gelesen, nach D3D9Ex hochgeladen und abgeschlossene Slots freigegeben.
  Mehrere fertige Capture-Einträge werden auf den neuesten reduziert; bei
  belegtem Ausgabering wird das Bild verworfen statt später veraltet
  ausgeliefert. Doppelte Stereo-Auftrags-IDs werden nicht erneut gecaptured.
- **Messung:** Im stabilen Stereoteil von
  `fearvr-20260731-065041` liefen XR und importierte Spielbilder beide mit
  89,1–90,1 fps. Echte Wiederverwendung lag typischerweise bei 0–3 von
  300 Frames, das mittlere Auftragsalter bei einem und das Maximum meist bei
  ein bis zwei Frames. Nach der Startphase gab es keine neuen
  Queue-/Slot-Drops; vereinzelte begrenzte Pacing-Timeouts traten nur bei
  Runtime-/EndFrame-Einbrüchen auf.
- **Bekannte Nachteile:** Der Classic-D3D9-Kompatibilitätspfad benötigt
  weiterhin einen GPU-zu-CPU-Readback. Seine Arbeit liegt nun im
  Frame-Pacing-Fenster, ist aber nicht kostenlos. 2D-Startmenüs vor dem
  ersten nativen Stereoframe bleiben bewusst freilaufend.
- **Rückfallpfad:** `-fearvr-no-xr-frame-pacing` lässt FEAR für einen
  A/B-Test wieder mehrere Renderdurchläufe pro OpenXR-Auftrag ausführen.
  Jeder Wait ist auch ohne diesen Schalter hart begrenzt; bei Hostverlust
  rendert FEAR mit dem letzten Auftrag weiter und kann nicht zyklisch auf den
  Host warten.

### AD-022 — Stereo supersampling without changing the Retail display mode

- **Problem:** The VDXR run `fearvr-20260731-065041` used 3072×3264 OpenXR
  swapchains per eye, but the imported game images were only 1280×1024.
  Jupiter EX rejected a manually configured 2560×1440 Retail mode and reset
  it to 640×480.
- **Source evidence/tested option:** The earlier attempt to force the entire
  Retail backbuffer to a 1920×1080 window in `CreateDevice`/`Reset` broke the
  flat-menu projection and was removed by `9fd783e`. Increasing only the
  OpenXR swapchain resolution cannot create additional scene detail.
- **Selected probe:** `-fearvr-render-scale 100..200` scales only the render
  targets used by the two native stereo world passes. Before each eye, the
  bridge saves render target 0, the depth-stencil surface, and the viewport;
  selects a matching larger off-screen target; then restores every saved
  state. With MSAA, the eye first renders into an equivalently multisampled
  target and resolves into the sampleable single-sample capture texture.
  Menus, videos, and the Retail window retain their original backbuffer.
- **Failure path:** If an off-screen target cannot be created or activated,
  the existing backbuffer capture remains active and a structured log event
  records the HRESULT. Failed or interrupted eye renders also restore D3D9
  state and cannot publish an incomplete stereo pair.
- **Viewport correction:** Jupiter resets the viewport and scissor rectangle
  to the cached Retail dimensions from inside `RenderCamera`. The bridge
  intercepts those calls and scales them only when render-target 0 is the
  active supersampled eye surface. It restores both values after the eye;
  internal post-processing targets, menus, and transfer devices are not
  modified.
- **Known cost/open measurement:** The Classic D3D9 path still reads through
  the CPU; 150 percent linear scaling produces 2.25 times as many pixels.
  Live testing must also establish whether Jupiter's internal post-processing
  targets follow the active target size or remain at the Retail resolution.
- **Rollback:** `-RenderScale 100` or `-fearvr-render-scale 100` uses the
  original rendering path.

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
