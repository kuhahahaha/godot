# Legacy heightfield native depth regression

This fixture exercises actual per-instance heightfield targets owned by one
base collider. It does not establish E34 acceptance, retained HeightFrame
snapshots, COW, or production-use leases.

### SceneCull source selection (H12)

```sh
python3 tests/servers/rendering/fixtures/particle_heightfield_clear_native/run_source_selection.py --binary /absolute/path/to/godot --display-driver macos --rendering-driver metal
```

This runs four cases on Mobile and Forward+ (eight processes). Each creates a
real SceneCull scenario with a field, source and ordinary receiver, obtains the
original collider-instance identity from its pair snapshot, and invokes
`SceneCull::render_particle_colliders()` through its actual dirty-list/index path.
Two independent targets are first seeded to `.125`. A direct native renderer
control must rasterize a y=0 plane to `.5`; the SceneCull target must yield:

| Source | Expected raw depth |
| --- | --- |
| Opaque mesh, node cast-shadow OFF | `.5` |
| Actual discard-all spatial material | `0` |
| Hidden mesh | `0` |
| Particle source with a real mesh draw pass | `0` |

The particle case seeds a fixed identity instance in the original particle render
buffer. Its positive control renders that same particle draw pass to `.5`, so an
uninitialized or empty particle cannot satisfy the exclusion test. This is a
fixed draw input, not a particle simulation claim. All observations use D32
sampling into a test-owned SSBO and asynchronous readback. The runner requires
real renderer/driver startup, exact case identity, independent control and source
values, clean exit and no errors/warnings. Scene instances are freed before their
bases after readback; the SceneCull-owned collision instance is freed once by its
original owner.

This covers native source selection only. It does not establish HeightFrame
Current, COW, managed source membership, producer/reader completion, retirement
qualification, or E34 acceptance. No source-selection GPU run is claimed merely
by the presence of this fixture.

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

## Scene height resource pair and checked empty-list control

`run_resources.py` adds a separate two-case Mobile/Forward+ mode; the existing
16-case capture runner and legacy modes retain their inputs and expectations.

```sh
python3 tests/servers/rendering/fixtures/particle_heightfield_clear_native/run_resources.py --binary /absolute/path/to/godot --display-driver macos --rendering-driver metal
```

The main RD creates two independent `HeightFrameResources` D32/framebuffer
pairs. An extra retained Ref to A makes retirement return Busy while its state,
RID and allocation serial remain unchanged. Checked clear/end seeds A to raw
0.75 and B to raw 0.25. Their actual begin/clear/end handles, capture flags and
restored list/attachment state are checked. An independent empty-list test cut
begins B at 0.125, explicitly rejects with `ERR_CANT_CREATE`, and verifies the
original checked cleanup end and sticky first error. An ordinary B clear/end
then succeeds in the same session before the final checked B clear.

The original fixture shader samples both depth textures into its own SSBO;
asynchronous readback must return the independent constants 0.75 and 0.25.
The callback releases the actual descriptors and extra A Ref. The script then
requests bounded render-thread retirement steps through a mutex-protected
single pending callback. Each original framebuffer must physically return
before its depth is retired. Passing requires all four exact RID/serial
witnesses to report Returned; elapsed time or a number of frames never stands
in for that observation. The script fails after 30 seconds without success.
The Python runner also verifies the actual driver/renderer, distinct RIDs,
retention result, checked restoration, numeric samples and four return reports.

This mode verifies the resource pair and checked empty-list component. It does
not publish a HeightFrame, mint a Scene sample, connect the original budget or
last-use bridge, or label physical disposal as producer Complete. Retained Ref
count is an additional custody gate; the original native owner still supplies
the producer/reader completion protocol when this component is integrated.

### Immutable candidate capture

`python3 tests/servers/rendering/fixtures/particle_heightfield_clear_native/run_candidate.py --binary <tests-enabled-editor> --display-driver <driver> --rendering-driver <driver>` runs Mobile and Forward+ serially with `ANGE_HEIGHTFIELD_CANDIDATE=1`. The render-thread entry is `run_candidate`; the existing `poll_frame_resources` pumps physical disposal.

This retains every resource-mode check (including the recoverable cut and four exact physical Returned identities). It additionally creates an active real collision instance and two 256×256 targets, freezes a candidate, then changes the live transform, extents, masks and shape revision. An indexed triangle at y=-.5 is captured using that frozen frame; independent GPU sampling must read A=.75 and B=.25 after B was reseeded .125. Required uniform/VA/IA/pipeline/push/draw/end records and once-only capture are checked. A second zero-business-capacity candidate must enter FailedRetiring with no business records and preserve A. Resolver preparation is permitted in that negative; it is not a no-work budget claim. Both candidates remain retained through async readback, then release their resource custody before the existing retirement pump.

The successful state is only `CaptureRecorded`: no Current, operation, budget, native-source membership or Complete is manufactured.

The candidate run also retains a capacity-five frame: actual begin + three global uniform bindings + VA fill its business slots, then IA reservation fails. It requires FailedRetiring, all five saved records, the sticky first error, and a real cleanup end handle with restored inactive list/attachments and no unknown state. The ordinary B=.125 clear and normal B=.25 capture then execute in the same session; this wrapper failure prefix remains retained through async readback. `candidate_postbind_restored` is required by the runner.

The explicit `--geometry-refusal` candidate mode creates a real two-instance MultiMesh. Current RD must refuse its draw with `ERR_UNAVAILABLE` / `NoNewEffect`, leave no draw handle, and complete checked cleanup. The runner allows exactly that one bracketed RD error. It then requires ordinary rendering and independently allocated occurrence 2 capture to recover in the same session (D32 A=.75, B=.25, ordinary=.25). This proves the current refusal branch, not successful instancing, mixed IA, full last-use retention or generation retirement. The four Returned reports cover the original A/B targets only.
