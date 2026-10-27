# ANGE lifecycle → Sprite caller gap — 2026-10-28

A source census finds no `particle_lifecycle`, `LifecycleFrame`, or `step_particle_lifecycle` symbol in the current ANGE source/tests. Existing `GPUParticles3D` builder and MultiMesh terrain/grass paths are separate render consumers; no production caller connects a lifecycle frame to attribute update and Sprite/GPUParticles3D output.

This is a precise source gap, not permission to add a synthetic lifecycle service or infer native/GPU completion. Current CPU/native guards and renderer consumers remain bounded by their existing contracts.
