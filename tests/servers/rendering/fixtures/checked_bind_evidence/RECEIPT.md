# Original checked draw binds — 2026-11-06

Core-private uniform/VA/IA wrappers share the original public setter bodies. Uniform acceptance remains deferred CPU state; VA/IA report actual original graph instructions and bounded complete transfer pairs. Early refusal preserves state and transfer fields. Late uniform attachment-alias errors preserve the original state mutation and report it explicitly. Ordinary entry points retain their original operation order.

Validation on macOS arm64: tests-enabled editor build exit 0. CheckedBind: 3 cases, 209 assertions pass. Graph/RecordSource/D32/NativeDisposal/Qualification/CheckedBind neighbors: 175 cases, 50,024 assertions pass. After restoring the negative control and rebuilding, full suite: 2,137 cases, 545,873 assertions pass, 3 skipped. Full run uses GODOT_TEST_PHYSICS_3D_BACKEND=GodotPhysics3D and GODOT_TEST_PHYSICS_3D_THREADED=0. Initial test-only doctest compound assertions were split before successful validation.

Effective negative: retain actual VA transfer processing but set captured transfer_count to zero. New fixture fails exactly its expected count assertion (1 case / 1 assertion); mutation reversed before restored build and full suite. Independent source review found no actionable issue and records final source hashes. New TU copyright header normalized with repository script; git diff --check passed.

Commands: `scons -Q tests=yes platform=macos target=editor module_particles_runtime_tests_enabled=yes -j3`; `bin/godot.macos.editor.arm64 --headless --test --test-case='*[CheckedBind]*'`; full suite uses `--headless --test` and the environment above.

Limits: fixture uses actual RD owner entries/graph begins through private test plumbing, not real Scene factories. This is setter capture only: no descriptor prepare/draw completion, geometry profile, source authority, physical GPU completion, HEIGHT_RASTER or end-to-end particle acceptance claim. Detailed limits are retained in IMPLEMENTATION.txt and REVIEW.txt. Goal remains active.
