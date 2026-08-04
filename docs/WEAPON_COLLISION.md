# Weapon world collision

Weapon world collision keeps the equipped VR weapon from visually passing
through solid level geometry. It is enabled by default while this feature is
being tested. Firing remains unchanged: this pass resolves the rendered
weapon, muzzle, and attached hands but does not suppress a shot.

## Live tuning

Open the floating panel with both grips plus B and select **Collide**. The
header shows the equipped weapon. The controls are:

1. `World collision` enables or disables collision response.
2. `Show collision box` keeps the wireframe visible outside the Collide tab.
3. `Edit profile` selects the equipped weapon (`Current`) or global fallback
   (`Default`).
4. `Box length` scales the measured grip-to-muzzle length.
5. `Box width` sets the probe volume width.
6. `Box height` sets the probe volume height.
7. `Forward offset` moves the complete box along the weapon aim direction.

The box is always previewed while the Collide tab is open. It is green when
the volume is clear, red while obstructed or retracting, and blue when world
collision is disabled.

## Collision model

The runtime measures the equipped model's muzzle socket and builds an oriented
box from the final controller, recoil, simulated-weight, and two-handed pose.
Nine longitudinal probes cover the center, corners, and edge midpoints. The
engine query requests world polygons only. Object-level collision and AABB
hits are deliberately excluded because Retail uses many invisible solid helper
objects around the player, weapon, and scripted gameplay objects.

Each hit plane is oriented toward the player's tracking base and converted to
a minimum-clearance correction along the plane normal. A front contact pushes
the weapon backward, a side contact pushes it sideways, and floor or ceiling
contacts resolve vertically. The strongest valid polygon contact is selected
instead of compounding several potentially conflicting normals. Entering and
release corrections are bounded and smoothed, with a short hold to avoid
flicker at doorframes. The corrected pose is used by the visible weapon, hand
sockets, muzzle, aim guide, and fire-vector origin.

When `WeaponWeightDiagnostics=1`, active collision contacts emit a rate-limited
`weapon_collision_contact` entry containing the selected world/poly IDs,
surface flags, profile name, and applied correction vector.

This is an oriented probe volume rather than a general rigid-body simulation.
It is intended to stop the barrel entering surfaces in front of the weapon;
it does not push the player or physically move either controller.

## Configuration

Global values are stored in `[VR]`:

```ini
[VR]
WeaponCollisionEnabled=1
WeaponCollisionDebug=1
WeaponCollisionLengthScale=1.0
WeaponCollisionWidth=0.10
WeaponCollisionHeight=0.18
WeaponCollisionForwardOffset=0.0
```

Dimensions and offsets are metres. Individual weapons use their normalized
model name:

```ini
[WeaponCollision.assaultrifle]
LengthScale=1.25
Width=0.12
Height=0.18
ForwardOffset=-0.05
```

Weapons without a section inherit the global profile. Selecting `Current` in
the live menu creates and persists an override for the equipped weapon.
