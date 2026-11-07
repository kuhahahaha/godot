# Legacy heightfield native depth regression

This fixture exercises actual per-instance heightfield targets owned by one
base collider. It does not establish E34 acceptance, retained HeightFrame
snapshots, COW, or production-use leases.

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
both renderers: sixteen GPU cases total. It requires
successful process exit,
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

`run_legacy_native.py` drives the same main-RD D32 capture with a local y=0
plane, then runs the original `ParticlesStorage::_particles_process` for one
ordinary legacy particle. Its ordinary particles material starts the particle
at the field origin, observes the actual `COLLIDED`, `COLLISION_NORMAL` and
`COLLISION_DEPTH` builtins, writes normal into velocity and depth into custom.w,
and corrects position by normal times depth. The fixture asynchronously reads
the actual particle buffer before freeing particle, material, shader and capture
resources. The initial velocity sentinel is -7, so no simulation cannot pass.

Four scenes run on Mobile and Forward+ (eight cases): identity, plus Z rotation
90 degrees with translation (10,20,30) and local scales (1,1,1), (2,2,2), or
(2,3,4). Geometry and the actual collision instance use the same field transform;
the particle itself has identity scale and world collision radius 0.1.
Independent world-plane expectations are normal (0,1,0), depth 0.1 and corrected
position (0,0.1,0) for identity; the other three expect normal (-1,0,0), depth
0.1 and position (9.9,20,30). They are fixed values, not derived through the
production capture or collider matrix code. The runner also requires active,
one actual process frame, one collider, real renderer evidence and no errors or
warnings. This verifies the transformed planar legacy height branch; H25 sloped
geometry and sphere/box/SDF/attractor neighbor coverage remain unverified.
