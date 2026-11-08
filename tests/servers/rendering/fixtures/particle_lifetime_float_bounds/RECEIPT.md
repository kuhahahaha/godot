# Particle lifetime CPU/GPU float boundary

The RD shader push constant stores lifetime as float. Commit 2438aa0698 rejects finite doubles that convert to zero or infinity. The helper test now rejects DBL_MIN/DBL_MAX and accepts FLT_MIN/FLT_MAX alongside existing ordinary, zero, negative and nonfinite controls.

Incremental module-on build: `scons -Q tests=yes platform=macos target=editor debug_symbols=no module_particles_runtime_tests_enabled=yes -j2`, completed successfully in 12.37s. Built editor SHA256: `408091853383967ff4dea755e80b77c79ec803b8557b8956afa636229c0f207e`. Focused ParticlesReadback/ParticlesProcessRetry/K03 MetalCompletion: 18 cases, 475 assertions, exit 0. No baseline RED engine run was executed; the previous test explicitly accepted DBL_MAX. This verifies the RD helper boundary, not scene-node setter parity or live GPU lifetime output. Those remain follow-up work.
