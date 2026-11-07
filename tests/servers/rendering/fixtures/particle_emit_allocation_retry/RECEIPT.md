# Particle emission allocation retry

`particles_emit()` now allocates its emission buffer before changing an already-emitting particle to `clear=true, emitting=false`. If allocation fails, the request returns with the previous emission state intact; a later successful request performs the state transition and records the emission.

Red/green source regression: `python3 .../run.py --source-ref cf417e2fdd` fails on the old ordering; current `python3 .../run.py` passes 4/4. Existing finite-input guard passes 4/4. The module-on editor rebuild (`scons -Q tests=yes platform=macos target=editor debug_symbols=no module_particles_runtime_tests_enabled=yes -j2`) returned 0, SHA256 `ea41f0112f0ed89ae57dbaf5831fbfb1709246c14d3909fcb2e781a41fd0a15f`; particle readback/process/trail plus Metal completion focused regression passed 22 cases / 540 assertions.
