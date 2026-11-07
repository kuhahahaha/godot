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
field extents_y = 2 and transform_y = -0.5: ten GPU cases total. It requires
successful process exit,
renderer startup evidence, no errors/warnings, the measured D32 samples and
agreement between renderers. Failure to instantiate the native module is a
failure (exit 77), not a skip accepted as GPU evidence.

Each case creates one base collider and two collision instances at resolution
256. Extents are (1,1,1), except the scaled case uses (1,2,1). It explicitly
seeds both independent instance depth targets to 0.75, then invokes the actual
scene renderer on only the second instance. A compute shader samples the two
production D32 textures into a fixture-owned SSBO; asynchronous readback on the
main RD observes GPU completion. Expected values are 0.75 for the untouched
sentinel and either 0 for empty capture or
`(plane_y - field_transform_y + extents_y) / (2 * extents_y)` for planar
geometry: 0.25, 0.5, 0.75, and 0.6875 in the four geometry cases.
The fixture frees its uniform set, sampler, SSBO, pipeline, shader, geometry,
mesh, both collision instances and their base collider after readback, and on
synchronous setup failure.

Counterexample: change only Mobile's heightfield draw-list flag from
`RD::DRAW_CLEAR_DEPTH` to `RD::DRAW_DEFAULT_ALL`, rebuild and rerun. The empty
Mobile case must report second_raw = 0.75 and fail the numeric assertion;
restore the flag before the final build. This mutation is a test command, not
a production change included with this fixture. No assertion is based solely
on resource identity or renderer metadata.
