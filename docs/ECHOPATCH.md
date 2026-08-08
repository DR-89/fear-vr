# EchoPatch-Abgrenzung und übernommener HID-Fix

Der vollständige [EchoPatch](https://github.com/Wemino/EchoPatch) wird nicht
mit F.E.A.R. VR installiert. Frühere Tests zeigten zwei strukturelle Konflikte:

- Sein Vectored Exception Handler beendet Ausnahmen, die unser Loader beim
  vorsichtigen Sondieren von Retail-Interna per `__try/__except` abfängt.
- Mehrere Signaturen erwarten das originale Spielmodul unter
  `GameClient.dll`. Bei F.E.A.R. VR heißt dieses Modul `GameOrig.dll`, weil
  `GameClient.dll` unser ABI-neutraler Loader ist.

## Warum trotzdem ein eigener `dinput8.dll`-Proxy existiert

Das von Spielern beschriebene Muster – zunächst normale Leistung, nach etwa
5–15 Minuten dauerhaft niedrige Bildrate bis zum Neustart – ist ein bekannter
Fehler des Basisspiels. EchoPatch beschreibt als Ursache, dass F.E.A.R.
allgemeine HID-Geräte fälschlich als Controller initialisiert.

F.E.A.R. VR implementiert ausschließlich diese kleine, frühe Korrektur selbst:

1. Der eigene x86-Proxy wird als `dinput8.dll` direkt neben `FEAR.exe` geladen.
2. Er prüft PE-Zeitstempel, Architektur, Imagegröße und jeden Originalbyte der
   beiden bekannten F.E.A.R.-1.08-Codeblöcke.
3. Nur bei vollständiger Übereinstimmung werden der redundante
   HID-Initialisierungsblock und der alte Windows-Input-Hook im
   **Prozessspeicher** übersprungen.
4. Danach werden `DirectInput8Create` und die übrigen Standardexporte an die
   echte Windows-`dinput8.dll` weitergereicht.
5. `FEAR.exe` auf dem Datenträger bleibt bytegleich.

Bei einer unbekannten EXE oder abweichenden Signatur bleibt der Patch aus und
das Spiel startet unverändert weiter. Das Laufprotokoll enthält dafür das
Ereignis `fear_hid_fix` mit einem der Ergebnisse `applied`,
`already_applied`, `not_fear_108`, `image_too_small`,
`signature_mismatch` oder `protection_failed`.

## Steam CEG und GOG Preservation Program

Die Steam- und die am 20. März 2025 aktualisierte GOG-Fassung sind beide
verifizierte 1.08-Builds:

| Edition | SHA-256 von `FEAR.exe` | `SizeOfImage` | Startmodus |
|---|---|---|---|
| Steam Ultimate Shooter Edition | `D5EBC38A4F12B772C9112A2811C290ADB6C5052D3BC2F817302D38CF55BB2CBE` | `0x001F3000` | Steam App 21090 |
| GOG Preservation Program | `C1678AA4DF37E87C097F45D8EB68A7C379D99AD12D8DA2771CF6235D9493D0B2` | `0x0019D000` | `FEAR.exe` direkt |

Beide melden Dateiversion `1.08.282.0` und PE-Zeitstempel `0x44EF6AE6`.

### Fehlerbild und Ursache

Vor der Korrektur startete die VR-Mod in beiden Editionen normal, aber jedes
Steam-Protokoll und das erste GOG-Protokoll meldeten
`fear_hid_fix result=signature_mismatch`. Die Bildrate war anfangs gut und nahm
anschließend während derselben Sitzung stetig ab. Das entsprach genau dem
ungepatchten HID-Fehler des Basisspiels.

Die beiden relevanten Codeblöcke liegen in beiden Builds an denselben RVAs:

- Der 22 Byte lange redundante HID-Block bei `0x840DD` ist bytegleich mit dem
  bereits unterstützten 1.08-Build.
- Der 29 Byte lange Legacy-Input-Block bei `0x84057` verwendet im GOG-Build
  auf dem Datenträger und im von CEG entpackten Steam-Prozess dieselben IAT-
  Operanden: `FF 15 70 C0 54 00` und `FF 15 3C C4 54 00`.

Beim Laden des frühen Steam-`dinput8.dll`-Proxys sind diese Steam-Codepages
noch CEG-verschlüsselt. Die erste Prüfung muss daher fehlschlagen. Nachdem CEG
das Image entpackt hat, stimmen die Live-Bytes dagegen mit der vollständig
verifizierten gemeinsamen Signatur überein. Die frühere Implementierung
ordnete diese Bytes nur der kleineren GOG-Imagegröße zu und lehnte Steam auch
nach dem Entpacken ab. Eine pauschal gelockerte Signatur wäre unsicher gewesen.

### Abgesicherte Korrektur

Die gemeinsame entpackte 29-Byte-Signatur wird ausschließlich für die beiden
exakten Imagegrößen `0x001F3000` (Steam) und `0x0019D000` (GOG) akzeptiert.
Danach muss weiterhin der gesamte gemeinsame 22-Byte-HID-Block passen.

GOG kann bereits beim Laden des `dinput8.dll`-Proxys gepatcht werden. Bei
Steam wiederholt der GameClient-Loader dieselbe vollständig abgesicherte
Prüfung nach dem CEG-Entpacken, aber vor der DirectInput-Geräteinitialisierung.
Der exportierte `DirectInput8Create`-Proxy führt unmittelbar davor eine letzte
abgesicherte Wiederholungsprüfung aus. Erst wenn beide Blöcke vollständig
verifiziert oder bereits vollständig genoppt sind, werden die noch
ursprünglichen Bytes im Prozessspeicher durch NOPs ersetzt. `FEAR.exe` auf dem
Datenträger bleibt unverändert.

Die exakten Steam- und GOG-SHA-256-Werte sind in der Entwicklungs- und
Release-Erkennung eingetragen. Eine zukünftige Revision wird dadurch nicht
automatisch akzeptiert.

### Verifikation am 8. August 2026

- Vor der Korrektur: Steam und GOG jeweils
  `fear_hid_fix result=signature_mismatch`.
- GOG nach der Korrektur: `fear_hid_fix result=applied`.
- Steam nach der Korrektur: `fear_hid_fix result=already_applied`; der
  GameClient-Fallback hatte beide Regionen nach dem CEG-Entpacken bereits
  erfolgreich gepatcht, bevor der DirectInput-Proxy sie erneut prüfte.
- Read-only-Verifikation im laufenden Steam-Prozess: alle 29 beziehungsweise
  22 Bytes der beiden Regionen waren `0x90`.
- Vollständige x86-Testsuite: 24 von 24 Tests bestanden.
- Reale GOG- und Steam-VR-Sitzungen: derselbe Spielstand über längere Zeit
  stabil und ohne den zuvor beobachteten fortschreitenden Leistungseinbruch.
- Benutzerprofil und elf vorhandene Spielstände blieben beim Overlay-Update
  erhalten.

Der neueste HID-Status lässt sich so prüfen:

```powershell
$log = Get-ChildItem <FEAR>\FEARVR\logs -Recurse -Filter dinput-*.log |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1
Get-Content -LiteralPath $log.FullName
```

Für GOG ist normalerweise `result=applied` zu erwarten. Steam meldet
normalerweise `result=already_applied`, weil der Loader nach dem CEG-Entpacken
vor dem DirectInput-Proxy zum Zug kommt. `signature_mismatch` als endgültiges
Protokollergebnis bedeutet, dass eine andere EXE-Revision oder veränderte
Prozessbytes vorliegen; in diesem Fall bleibt der Patch absichtlich aus.

## Was ausdrücklich nicht übernommen wurde

HUD-Skalierung, FOV, SSAA, Controller-/Gyro-Unterstützung, Crash-Handler,
High-FPS-Spielphysikpatches und alle GameClient-/GameServer-Hooks von EchoPatch
sind nicht Teil dieses Ports. Dadurch gibt es keine zweite Hook-Suite und
keinen zusätzlichen Eingabeschreiber neben OpenXR.
