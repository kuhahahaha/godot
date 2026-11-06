# External output late custody refusal

This receipt covers stale and late consumer calls after generation withdrawal.

- Source: `tests/servers/rendering/test_external_output_owner_state.cpp`
- Build: `scons -Q tests=yes platform=macos target=editor module_particles_runtime_tests_enabled=yes -j3`
- Test binary: `bin/godot.macos.editor.arm64`
- Focused command: `GODOT_TEST_PHYSICS_3D_BACKEND=GodotPhysics3D GODOT_TEST_PHYSICS_3D_THREADED=0 bin/godot.macos.editor.arm64 --headless --test --test-case='*[ExternalOutput]*,*[ProductionRequest]*'`
- Result: 56 cases / 36,529 assertions / 0 failures.

The added checks mutate only wrapper serial, slot, or generation fields and verify that
`view_return`, `surface_return`, `cancel_before_draw`, `read_return`, and late
`record_checked` reject stale or unavailable calls while preserving the live consumer,
Registry pin, cell serial, and custody. The restored wrapper then follows the existing
successful return path. This is custody/state evidence; it does not claim typed frame
consumption, native draw completion, GPU retirement, or Sprite visible output.
