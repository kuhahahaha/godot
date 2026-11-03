# Particle readback/process retry focused recheck — 2026-11-04

The fixed Godot editor reran the native particle readback and process-retry controls:

`bin/godot.macos.editor.arm64 --headless --test --source-file='*test_particles_readback.cpp,*test_particles_process_retry.cpp' --no-colors`

Result: exit 0, 15/15 cases, 105/105 assertions. The production Vulkan upload retry case was explicitly skipped because `GODOT_TEST_PARTICLES_PROCESS_RD=1` was not set; this receipt makes no GPU claim. The focused controls cover bounded particle buffer sizing, trail multiplication overflow, lifetime split arithmetic, finite setter domains, and invalid RID readback fail-closed behavior.
