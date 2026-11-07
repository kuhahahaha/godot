# Legacy heightfield native depth regression

This fixture samples the current instance-owned heightfield capture targets.
It does not accept E34, HeightFrame snapshots, or production-use leases.

Build an editor with `tests=yes module_particles_runtime_tests_enabled=yes`.
Run with an actual display and main RenderingDevice (never `--test` or dummy):

```sh
python3 tests/servers/rendering/fixtures/particle_heightfield_clear_native/run_native.py --binary /absolute/path/to/godot --display-driver macos --rendering-driver metal
```

For Linux use `--display-driver x11 --rendering-driver vulkan`.
The runner executes both Mobile and Forward+ for an empty field and planar
geometry at world y = -0.5, 0, +0.5, plus a plane at y = 0.25 with
field extents_y = 2 and transform_y = -0.5. Three further cases transform
both field and local y=0.5 geometry by a 90-degree Z rotation and translation
(10,20,30), with local scales (1,1,1), (2,2,2), and (2,3,4). Each is run on
both renderers: sixteen GPU cases total. It requires successful process exit,
renderer startup evidence, no errors/warnings, the measured D32 samples and
agreement between renderers. Failure to instantiate the native module is a
failure (exit 77), not a skip accepted as GPU evidence.

Each case creates one base collider and two collision instances at resolution
256. Extents are (1,1,1), except the scaled case uses (1,2,1). It explicitly
seeds the first independent instance depth target to 0.75. The second target
is seeded to 0.125 for transformed cases and 0.75 for the original cases,
then the actual scene renderer is invoked on only the second instance.
The transformed seed differs from the expected 0.75 so a renderer no-op fails.
A compute shader samples the two
production D32 textures into a fixture-owned SSBO; asynchronous readback on the
main RD observes GPU completion. Expected values are 0.75 for the untouched
sentinel and either 0 for empty capture or
`(plane_y - field_transform_y + extents_y) / (2 * extents_y)` for planar
geometry: 0.25, 0.5, 0.75, and 0.6875 in the four geometry cases.
The three transformed cases all have the independent expected raw depth 0.75,
specified from local y=0.5 and half-height 1, without calling the production
camera/projection helper. They validate real transformed capture numerics;
they do not certify legacy collision normals (H25), COW or managed admission.
The fixture frees its uniform set, sampler, SSBO, pipeline, shader, geometry,
mesh, both collision instances and their base collider after readback, and on
synchronous setup failure.

Counterexample: change only Mobile's heightfield draw-list flag from
`RD::DRAW_CLEAR_DEPTH` to `RD::DRAW_DEFAULT_ALL`, rebuild and rerun. The empty
Mobile case must report second_raw = 0.75 and fail the numeric assertion;
restore the flag before the final build. This mutation is a test command, not
a production change included with this fixture. No assertion is based solely
on resource identity or renderer metadata.


## Actual legacy height collision control

`run_legacy_native.py` drives the same main-RD D32 capture with four flat local
y=0 planes and one tilted local plane, then runs the original
`ParticlesStorage::_particles_process` for one ordinary legacy particle. Its
ordinary particles material starts the particle at a fixed per-case position,
observes the actual `COLLIDED`, `COLLISION_NORMAL` and
`COLLISION_DEPTH` builtins, writes normal into velocity and depth into custom.w,
and corrects position by normal times depth. The fixture asynchronously reads
the actual particle buffer before freeing particle, material, shader and capture
resources. The initial velocity sentinel is -7, so no simulation cannot pass.

Five exact CASESET scenes run on Mobile and Forward+ (ten cases): identity,
rotation, uniform, nonuniform, and tilt_nonuniform_rotation. All have origin 0
and a world-space particle radius of 0.25 without collision scale. Identity,
uniform scale (2,2,2), and nonuniform scale (2,1,3) start at (0,0.125,0) over
local plane y=0: expected normal (0,1,0), depth 0.125 and corrected position
(0,0.25,0). Rotation is Z90 with particle (-0.125,0,0): expected normal (-1,0,0),
depth 0.125 and position (-0.25,0,0).

The tilted case uses raw extents (2,2,2), local plane y=0.5*x+0.25*z over
x/z in [-2,2], field Rz90*diag(2,1,3), and particle (-0.125,0,0). Independent
CASESET expectations are normal
(-0.9669875568304565,-0.2417468892076141,-0.08058229640253803), depth
0.12087344460380706 and corrected position (-149/616,-9/308,-3/308).
Geometry and actual collider use the same field transform. Fixed world-space
oracles do not call production capture or collider matrix helpers. Legacy
normal/depth/position tolerances are 2^-12; actual COLLIDED, active, one process
frame and one collider are also required. These are unconnected direct numerical
controls; source/record/owner-use evidence is still required for complete H25
acceptance. The separate 16-case D32 capture matrix retains its original
transformed geometry cases and parameters.


## Actual legacy neighbor controls

`run_legacy_neighbors.py` runs six independent single-source scenes on both
Mobile and Forward+ (12 cases), through the same original legacy particle
process and actual particle-buffer asynchronous readback. No height capture is
created for these controls, and no source shares a HashSet with another source.
The inputs and independent raw oracles match the six `H25.numeric_*` rows in
`SCENE_HEIGHTFIELD_CASESET.json`. Sphere center 0/radius 1 and particle
(0,1,0)/radius 0.25 give normal (0,1,0), depth 0.25 and corrected position
(0,1.25,0). Box center 0/extents 1 and particle (1.125,0,0)/radius 0.25 give
normal (1,0,0), depth 0.125 and position (1.25,0,0). SDF extents 2 uses a real
16×16×16 RF volume containing world x at texel centers; particle (0.125,0,0)
and radius 0.25 give normal (1,0,0), depth 0.125 and position (0.25,0,0).

Sphere/box attractors have center 0, radius/extents 2, strength 2, attenuation 1,
directionality 0, and particle (1,0,0): expected force (-1,0,0). The vector
attractor has extents 2, strength 2 and a real 2×2×2 RGBF volume filled with
(0,0.5,0.5), with particle at the origin: expected force (-2,0,0). Their ordinary
process shader writes actual ATTRACTOR_FORCE to CUSTOM and adds force times
DELTA to velocity. First-frame velocities are (-1/60,0,0) and (-2/60,0,0), with
position unchanged because native automatic integration precedes start/process.

Every scene verifies active and exactly one frame, the actual collider/attractor
counts, and actual cached 3D texture count. Textured scenes additionally verify
TYPE_3D, a valid non-white RD texture, exact cached field RID and the original
frame texture index 0. All resources remain alive until readback; particles are
freed before their associated material, collider and texture. These are
**unconnected direct legacy controls**: the CASESET's required external Scene
connection, original record dependency and owner-use join remain absent. Numeric
agreement cannot certify complete H25 acceptance, multi-source interaction order
or the remaining sloped-height coverage.
