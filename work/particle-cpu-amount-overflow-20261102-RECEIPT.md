# CPU particle amount overflow receipt

Date: 2026-11-02. Base before this change: `4fc3175c57`.

`CPUParticles2D::set_amount` and `CPUParticles3D::set_amount` now reject counts whose 16-float or 20-float particle-data multiplication exceeds `INT_MAX`. The guard executes before particle arrays or the multimesh are changed.

`python3 tests/scene/fixtures/particle_cpu_amount_overflow/run.py --source-ref HEAD` failed on the missing 2D guard before staging; the worktree run passed 2/2. The mixed-worktree editor binary SHA256 `b87ce88c36142d213b2ecc6e79e22995e4de5fa544ca3971dab89f7cc46e409d` ran `./bin/godot.macos.editor.arm64 --headless --test --test-case='*amount rejects storage multiplication overflow*'` with 1 case and 2 assertions passed. This doctest checks that `INT_MAX` leaves the previous amount unchanged; it is not a GPU allocation test.

Only the two `set_amount` guard hunks are staged from the otherwise dirty CPU particle files. The doctest lives in the larger dirty `tests/scene/test_gpu_particles.cpp`, so its result qualifies the integrated mixed-worktree binary rather than a standalone source tree.
