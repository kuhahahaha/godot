# CPU3D local view-depth sorting regression

This fixed fixture creates public `CPUParticles3D` and `Camera3D` nodes, emits 64
particles at two fixed points, and reads the actual uploaded MultiMesh buffer
after `frame_post_draw`. It independently projects each uploaded position into
world camera depth. Cases cover emitter translation, rotation, nonuniform scale
and shear. No private state or test hook is used. The fixture's exact-buffer flag
requires 1280 floats (64 instances × 20 floats), and both fixed points must appear.

Use a separately pinned host outside the mutable build-tree `bin` directory:

```sh
python3 tests/servers/rendering/fixtures/cpu_particles_depth_sort/run.py \
  --godot /absolute/pinned/editor.arm64 --sha256 <actual-host-sha256> \
  --out /absolute/new/evidence-directory
```

The default GREEN oracle requires all four cases to pass, process exit 0 and no
failures. On the unmodified CPU3D source, add `--expect-red`: it strictly requires
exit 43, failures exactly `translation`, `nonuniform_scale`, `shear`, and a passing
rotation control. Every case must still contain the exact buffer, both emission
points and 64 finite world-depth values. The runner independently validates the
expected depth values and sorted order or targeted inversion. Missing points,
malformed/duplicate JSON, parser/startup errors, extra result markers, engine
errors, exit leaks and the 90-second timeout always fail. An expected RED run has
runner exit 0 only when these targeted negative assertions hold; it does not mean
the engine behavior passes.

The runner uses the actual main Metal/Mobile host with a fixed 64fps schedule and
safe render thread. It launches no build. Host SHA256 is required before launch;
host and every recorded input hash must remain stable through process exit.
Inputs include the CPU3D source/header and fixture script/project/runner. A new
output directory contains exact command, source hashes, host hash, process code,
checks, complete parsed result, raw stdout/stderr and a file hash manifest.
Source hashes are observations, not evidence that a host was built from them;
the build owner separately supplies provenance for the pinned executable.

This is a public CPU producer and native MultiMesh ordering test. It does not
assert screenshots, alpha-compositing pixels, GPU synchronization, another
renderer/platform or an ANGE producer/product. It does not alter the separate
390-check native retry/retirement fixture.
