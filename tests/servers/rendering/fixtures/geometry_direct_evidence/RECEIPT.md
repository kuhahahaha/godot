# Original GeometryDirect capture evidence — 2026-11-06

Local source commits 601fe9c8c9, 7886755bcb and 353b6684c1 add a private no-descriptor GeometryDirect profile through the original draw body and original factory/setter tests. Public ABI and ordinary capture=false drawing remain unchanged. Original pipeline metadata is retained in all builds. Exact owner, shader identity, framebuffer/vertex format, primitive and cached VA/IA counts are checked before effects. Indexed VA4/IA6 is accepted. After append, a lost capture handle retains Recorded and draw_count.

Build: `scons -Q tests=yes platform=macos target=editor module_particles_runtime_tests_enabled=yes -j3` exit 0. Focused GeometryDirect: 1 case, 84 assertions pass. Configured full suite: 2145 cases, 546787 assertions pass, 3 skipped. Environment: GODOT_TEST_PHYSICS_3D_BACKEND=GodotPhysics3D GODOT_TEST_PHYSICS_3D_THREADED=0. Commands use `bin/godot.macos.editor.arm64 --headless --test`, with focused `--test-case='*[NativeDisposal]*[GeometryDirect]*'`.

The tests use original shader/pipeline/vertex-format/buffer/VA/IA factories and original bind setters. Negative fixtures cover aligned stale VA/IA counts and a RID from the wrong owner; they assert no new draw effect. No effective production mutation RED has been run yet. Early fixture failures were missing driver methods, invalid synthetic shader ownership, static constant reference linking and omitted vertex-format teardown; the final factory fixture fixes those and passes.

Limits: graph mapping remains Pending; actual native payload execution and Recorded mapping are not yet tested here. No non-DEBUG local build, source/read-use authority, GPU completion, physical retirement, Scene HEIGHT_RASTER output or no-next-draw-close acceptance is claimed. Prepared descriptors and deferred copy/clear prefixes remain unfinished. Goal stays active. PAYLOAD_PLAN.txt is a read-only plan, not execution evidence.

## Recorded/native payload follow-up

Commit-local test extension executes the original graph with `begin(true)` and `end()`. The strict driver records render pass, pipeline, vertex/index binds, indexed and nonindexed draws, and end. Focused GeometryDirect now passes 1 case / 176 assertions; configured full suite passes 2145 cases / 546879 assertions, 3 skipped. Assertions cover pipeline 400, vertex buffer 10, index buffer 11, UINT32 index byte offset 8 (element offset 2), indexed draw 6/1/0, triangle draw 3/1/0, and Recorded mapping for all original handles to primary command buffer 11, slot 0.

This is original RDG/native payload and graph mapping evidence only. It does not establish source/read-use identity, native completion, physical retirement, Scene output or no-next-draw closure.
