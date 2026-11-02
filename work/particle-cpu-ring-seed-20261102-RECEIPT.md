# CPU ring emission fixed-seed receipt, 2026-11-02

`CPUParticles2D::_particles_process` used process-global `Math::randf()` for both ring angle and radius after each particle's `rng` had been seeded from its fixed seed. The ring path now uses that particle RNG for both draws. The existing point-path control was extended to exercise a real 2D ring emitter and verify that processing does not advance the global RNG stream.

RED, before source repair: `./bin/godot.macos.editor.arm64 --headless --path tests/scene/fixtures/cpu_particle_point_seed --script run.gd` exited 1 with `CPU_POINT_SEED={"failures":["2D ring emission consumed global RNG"]}`. Then `scons platform=macos target=editor tests=yes -j4` exited 0 and produced editor SHA-256 `ae40e9f012d38695dd30e1f07d2a39882b6f50297889949daec8ede245def7dd`. GREEN, with the same command: exit 0 and `CPU_POINT_SEED={"failures":[]}`.

This establishes the real SceneTree ring path no longer consumes the global RNG for angle or radius. It does not establish rendered pixel equality, cross-process exact positions, GPU behavior, or an ANGE product request. Other concurrent edits in `scene/2d/cpu_particles_2d.cpp` are outside this change.
