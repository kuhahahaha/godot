# Particle collision scene setter bounds receipt

Date: 2026-11-02. Base before this change: `68ef9271c1`.

The scene setters for collision sphere, box, SDF, and heightfield geometry and attractor sphere/box/vector-field geometry now reject non-finite or negative values before mutating state or submitting to RenderingServer. SDF thickness remains within 0..2. SDF and heightfield resolution, heightfield update mode, and attractor strength/attenuation/directionality have explicit bounds in the same file.

`python3 tests/servers/rendering/fixtures/particle_collision_scene_bounds/run.py --source-ref HEAD` failed on the missing sphere guard before staging; the worktree version passed 9/9. The mixed-worktree editor binary SHA256 `b87ce88c36142d213b2ecc6e79e22995e4de5fa544ca3971dab89f7cc46e409d` ran `./bin/godot.macos.editor.arm64 --headless --test --test-case='*collision scene*'` with 1 case and 14 assertions passed. This doctest checks state preservation and is not live GPU evidence.

The source fixture and scene setter change are committed together. The larger dirty `tests/scene/test_gpu_particles.cpp` remains outside this commit; the doctest result is qualified as an integrated mixed-worktree check.
