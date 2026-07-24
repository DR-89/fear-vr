# patches/

Minimale, **lizenzgeprüfte** Diffs gegen das offizielle F.E.A.R.-SDK
(ANWEISUNG.md §10).

Regeln:

- Patches so klein wie möglich halten.
- Vor **jeder** Veröffentlichung prüfen, ob die SDK-Lizenz das Verteilen des
  konkreten Diffs erlaubt.
- Sichere Alternative: ein lokales **Transformationsskript**, das auf eine vom
  Benutzer bereitgestellte SDK-Quelle angewendet wird (kein SDK-Code im Repo).

Neu geschriebene GameClient-Dateien gehören nicht hierher, sondern nach
`game-source-overlay/`.

Vorhandene Buildhilfen:

- `apply-sdk-build-fixes.ps1`: idempotente, fail-closed Transformationen einer
  lokal bereitgestellten Public-Tools-1.08-Quelle. Enthält keine SDK-Dateien.
- `gameclient-build.props`: moderne MSBuild-Overrides für v141-Compile-Tests,
  korrekte Ausgabenamen und das Abschalten der defekten alten xcopy-Schritte.
  v141-Ausgaben sind nicht VC7.1-ABI-kompatibel und werden vom
  Deploymentskript abgelehnt.

Anwendung und Begründung stehen in
[`docs/BUILD-GAMECLIENT.md`](../docs/BUILD-GAMECLIENT.md).
