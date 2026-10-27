# Godot real particle caller boundary — 2026-10-27

The production tree does have a real particle consumer: `sys_visual_bridge.cpp:305-328` observes ECS `Particle` plus `Transform`, then calls `create_particles_node(p.data, p.one_shot, "Particles")` to create and attach a Godot `GPUParticles3D` node.

That caller consumes `ParticleData` through the ordinary Godot particle API. It does not carry an authenticated U08 producer admission, same-request E09 association, `ParticleProgramLoader` request, `StageExecutor::start_native_qualification`, or `MeshStorage::mesh_add_external_surface` producer call. The current census therefore narrows the gap: a real runtime particle consumer exists, while the native qualification/external-surface producer path remains absent.

No synthetic association, ObjectID/RID, test helper, serial, or consumer guard is promoted to operation/completion/readback/Ready/GPU authority.

Evidence: `K03_GODOT_REAL_PARTICLE_CALLER_BOUNDARY_20261027_REPORT.json` and `/tmp/k03-real-particle-caller-boundary-20261027.log`.
