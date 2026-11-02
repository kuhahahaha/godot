# Manual particle emission input bounds receipt

Date: 2026-11-02. Base before this change: `ae75b14cca`.

`GPUParticles2D::emit_particle`, `GPUParticles3D::emit_particle`, and the RD/GLES3 `particles_emit` storage entry points now reject non-finite transforms, velocities, color and custom-color components before forwarding to RenderingServer or changing backend state. The staged source consists of one guard per entry point; neighboring dirty particle changes remain unstaged.

`python3 tests/servers/rendering/fixtures/particle_emit_input_bounds/run.py --source-ref HEAD` failed at the missing 2D scene guard before staging. The current worktree fixture passed 4/4 source checks and checks that scene guards precede RenderingServer submission. This is source-level evidence; live GPU manual-emission behavior is not claimed by this receipt.
