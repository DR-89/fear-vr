# Simulated weapon weight

Simulated weapon weight adds configurable positional and rotational lag to the VR weapon pose, making heavier weapons feel less immediate when the controller moves. The feature is optional and disabled by default.

## Configuration file location

Edit `fearvr.ini` in the installed F.E.A.R. VR mod directory, not in the retail F.E.A.R. game directory or the source repository.

The default installer location is:

```text
%USERPROFILE%\FearVR\fearvr.ini
```

For example:

```text
C:\Users\YourName\FearVR\fearvr.ini
```

If the mod was installed with a custom `-InstallDir`, use:

```text
<InstallDir>\fearvr.ini
```

## Enable the feature

Close the game, open `fearvr.ini`, find the existing `[VR]` section, and add the following settings. Do not create a second `[VR]` section if one already exists.

```ini
[VR]
WeaponWeightEnabled=1
WeaponWeight=1.0
WeaponPositionalFollow=18.0
WeaponRotationalFollow=20.0
WeaponCatchUpStrength=1.5
```

Restart the game after saving the file.

Set `WeaponWeightEnabled=0`, or remove the setting, to disable simulated weapon weight.

## Settings

- `WeaponWeight` controls the overall sense of inertia. Higher values feel heavier and respond more slowly.
- `WeaponPositionalFollow` controls how quickly the weapon catches up to hand movement. Higher values follow the hand more closely.
- `WeaponRotationalFollow` controls how quickly the weapon catches up to hand rotation. Higher values follow the hand more closely.
- `WeaponCatchUpStrength` increases catch-up when the filtered weapon falls farther behind, reducing excessive separation during quick movements.

Recommended ranges:

| Setting | Recommended range | Default |
|---|---:|---:|
| `WeaponWeight` | `0.10`–`4.00` | `1.0` |
| `WeaponPositionalFollow` | `2.0`–`40.0` | `18.0` |
| `WeaponRotationalFollow` | `2.0`–`40.0` | `20.0` |
| `WeaponCatchUpStrength` | `0.0`–`4.0` | `1.5` |

## Per-weapon overrides

Individual weapon models can override the global values with a section named after the model:

```ini
[WeaponWeight.<model-name>]
Weight=1.0
PositionalFollow=18.0
RotationalFollow=20.0
CatchUpStrength=1.5
```

Weapons without a matching section use the global `[VR]` settings.

Recoil uses the same equipped-model identity and can also be overridden from
the floating Recoil tab or native recoil page:

```ini
[WeaponRecoil.<model-name>]
Strength=1.5
MuzzleRise=1.0
Recovery=1.0
```

The tool panel shows the equipped weapon in the Recoil, Weight, and Weapon tab
header. Selecting `Current` edits that model's section; selecting `Default`
edits the global `[VR]` values. Weapons without a recoil section inherit the
global values.

## Diagnostics

For troubleshooting, enable rate-limited weapon-weight telemetry under `[VR]`:

```ini
WeaponWeightDiagnostics=1
```

Set it back to `0` after collecting the required logs.
