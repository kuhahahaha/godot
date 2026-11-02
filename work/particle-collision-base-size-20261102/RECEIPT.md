# Particle collision base-size receipt

Date: 2026-11-02

The scene-node and RD/GLES3 storage collision base-size setters now reject negative and non-finite values. The source-ref baseline is RED; the current harness passes 6/6 behavior checks and 4/4 source checks. The native `[ParticlesProcessRetry]*` selection also passes with the RD validator's negative-value assertion.

The `GPUParticles2D` node already had its non-negative guard in HEAD; this commit adds the missing `GPUParticles3D`, RD and GLES3 guards only. The staged source is limited to those setter/validator hunks; neighboring dirty particle work remains unstaged. Against the pre-fix HEAD, `run.py --source-ref HEAD` fails at the 3D node. The current worktree run passes 6/6 extracted C++ behavior checks and 4/4 source checks. The current mixed-worktree editor binary ran `[ParticlesProcessRetry]*` with 3 cases and 47 assertions passed; this is native unit evidence, not a live GPU collision run.
