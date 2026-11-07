# GeometryPrepared capture evidence — 2026-11-06

Production changes add the private `DrawCaptureProfile::GeometryPrepared` path and `_draw_list_draw_geometry_prepared_recorded(...)`. It reuses the original prepare/bind loops while validating descriptor count/format, live owner identity, qualification and pending-clear state, attachment aliasing, tracker sizes/usages, cross-set conflicts, graph-frame/list/kind identity, strict prefix ordering, and the final draw handle. Capture is read-only and refuses before native effects on invalid input; `GeometryDirect` behavior is unchanged.

Build: `scons -Q tests=yes platform=macos target=editor module_particles_runtime_tests_enabled=yes -j3` exit 0. Focused results from `bin/godot.macos.editor.arm64 --headless --test`:

- `--test-case='*[GeometryPrepared]*'`: 2 cases, 1,568 assertions, 0 failures.
- `--test-case='*[GeometryDirect]*'`: 1 case, 176 assertions, 0 failures.
- `--test-case='*[ExternalOutput]*,*[ProductionRequest]*'`: 56 cases, 36,706 assertions, 0 failures.

The GeometryPrepared fixtures use original shader/pipeline/VA/IA/uniform-buffer/uniform-set factories, batching and barrier variants, compatible shader origins, sparse and contiguous multi-set layouts, and refusal checks for qualification dependencies, pending clear, tracker mismatch, cross-set usage conflict, and stale cached IDs. Prefix handles and the draw handle map from Pending to Recorded with strict graph identity and ordering.

Limits: this covers CPU-side recording and graph mapping only. It does not establish deferred copy/clear/get-data transfer drain, real GPU completion, physical retirement, Sprite/LE192 output, or no-next-draw close acceptance. Those remain dependent on the source/Host/read-use/completion interfaces.

Post-commit full regression for `f847ccd2dd` and `a5ed5d777e`: `GODOT_TEST_PHYSICS_3D_BACKEND=GodotPhysics3D GODOT_TEST_PHYSICS_3D_THREADED=0 bin/godot.macos.editor.arm64 --headless --test` exited 0 with 2,147 cases / 548,716 assertions / 0 failures / 3 skipped. Log: `/tmp/b4-prepared-withdraw-full.log`. Opt-in Vulkan particle upload/trail tests explicitly reported UNRUN; this headless suite is not real GPU acceptance.
