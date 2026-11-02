# CPU particle geometry range receipt

Date: 2026-11-02

`CPUParticles2D/3D` now enforce the same declared finite ranges as `ParticleProcessMaterial` for spread, flatness, emission sphere/box/rect extents, ring height/radii, and ring cone angle. Invalid values preserve the prior state before CPU-to-GPU conversion. The source fixture is RED at `HEAD` (0/20) and GREEN on the current source (20/20); the rebuilt `[CPUParticles]*` selection passes 10 cases / 43 assertions.
