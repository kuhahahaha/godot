# Particle emission texture changed RED/GREEN receipt

Root ran the same frozen fixture on the actual main RenderingDevice: Metal 4.0, Forward Mobile, Apple M3 Pro (Apple9). Clean RED returned 1 with 185 checks and 48 failures. GREEN returned 0 with 185/185 passing. Each run contains exactly one result record and 28 observations; all report a main-RD callback and three actual material parameters. The copied runtime logs contain no engine/script error diagnostics. Return codes are root-reported; the logs have no exit-code trailers.

Source baseline: `67be378a87798185a0ff209a5c8f50308c303601`. GREEN changes only `scene/resources/particle_process_material.cpp` and `.h` in the production scope. Per-file baseline/current/diff SHA-256 values and the two-source-path diff hash (`8469f27fa91c57b6bdf58a7184ecf067657d49d00c653ca48efca6e8fb01123e`) are in [RECEIPT.json](RECEIPT.json). The frozen script is SHA-256 `bdf97f700ea6ce812bc4f10c12cd21af40c904ef6d00043b3dde462522f8f744`.

The initial draft failed parsing because `weakref()` inferred Variant under warnings-as-errors. The explicit `WeakRef` declaration fixed the script before clean RED; that parse failure is not counted as behavior RED and its overwritten log was not retained. Product code was changed only after the clean target RED.

The three emission slots now use independent deferred callbacks that read their current members. Reference replacement disconnects only the relevant callback, preserving other slots on a shared resource. Same-reference setters still refresh. Point validity retains the original resource-reference semantics. Color resource notifications update the RID without queuing shader variants; the color setter retains its original queue operation.

Build command:

```sh
scons -Q platform=macos target=editor tests=yes debug_symbols=no module_particles_runtime_tests_enabled=yes -j3
```

Runtime command (both stages):

```sh
bin/godot.macos.editor.arm64 --display-driver macos --rendering-method mobile --rendering-driver metal --audio-driver Dummy --path tests/scene/fixtures/particle_emission_texture_changed --script run.gd
```

The root-owned neighboring regression returned 0: 18 test cases and 90 assertions passed using:

```sh
bin/godot.macos.editor.arm64 --headless --test '--test-case=*[GPUParticles]*,*[CPUParticles]*,*[Particles]*'
```

That headless CPU regression is neighbor coverage, not GPU qualification.

The fixture proves actual material parameter association. Capture preserves RID values and normalizes any non-RID value, including Variant NIL, to empty RID before comparing; null/detached comparisons therefore do not dynamically distinguish the raw Variant type. Exact Variant NIL behavior and shader-variant semantics were checked by agent `/root/review_collision_fixture` through direct read-only source/fixture review, reported by messages to root; no separate review file exists and no static blocker was reported. They are not dynamic oracles of this fixture. No shader sampling, particle motion, or physical-effect claim is made.

The RED executable hash reported by root is `2dde15a04a877b1c93e5cbe92147b0a985dce61a1805232efef75c1df4c313e6` (156531640 bytes). Its matching sidecar was copied from the prior collision fixture's GREEN receipt; no independent RED executable copy was retained. Its runtime banner retains the earlier `60d14b9c6` pre-commit revision. GREEN's sidecar records `67199631ee6edc968a0f52d2a0af709d2c14735ab28e9051c0c3754ea5b5a281` (156531656 bytes). These are source-tree runtime builds, not newly preserved fixed-artifact exports.

Scope remains the three emission textures. Bind-once curve/ramp setters (`set_param_texture`, `set_color_ramp`, `set_color_initial_ramp`, `set_alpha_curve`, `set_emission_curve`, `set_velocity_limit_curve`, `set_rotation_velocity_3d_curve`) are follow-up source evidence, outside this patch and its runtime acceptance.

| Raw log | SHA-256 |
| --- | --- |
| [red_run.log](red_run.log) | `6a8da77e3d03971e53421f332a7bcfdfd8f2b335a145b0088102792fe74ea5d2` |
| [green_run.log](green_run.log) | `340fe9777d631257fb45a930bf5e39d25f358a8867c0889c6a79ab02b305bc6e` |
| [green_build.log](green_build.log) | `2dba6355f7a25ef88f8442d9212f121dcedc0a2d441eaa9979e04dc23ee9cd9e` |
| [neighbors.log](neighbors.log) | `a5c906dc38a68f3da4299870dca64f8b85bb2c4bbc00de9a3799d1f0e2985699` |
