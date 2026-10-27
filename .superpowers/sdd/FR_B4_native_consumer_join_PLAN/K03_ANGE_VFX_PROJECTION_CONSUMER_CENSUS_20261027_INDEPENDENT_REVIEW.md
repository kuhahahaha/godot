# Independent review — K03 ANGE VFX projection consumer census — 2026-10-27

- Reviewer: `vfx_projection_consumer_review` (non-author)
- Result: **limited-scope accept**, no actionable P0/P1/P2/P3.
- Tests: not rerun; static source and recorded evidence only.

All five source hashes match the current dirty ANGE tree at source HEAD
`b2038e9a5be40624f6b19613f209422da49b4355`. The call chain is accurate:
`ff_vfx` runtime batch callback → `WorldLoader::load_vfx_sources()` →
`compile_system_bundle()` and lease/bundle checks →
`EcsWorld::validate_vfx_system_projections()` with rollback on invalid data.

The only production projection-to-Godot visual consumer found is the
`BridgeParticle` observer over ECS `Particle + Transform`, which calls
`create_particles_node()`. No `SystemRegistryProjection`, `vfx_models`, or
operation consumer exists in that bridge. The census correctly separates
semantic source/projection evidence from actual GPU production.

The recorded Metal/mobile Apple M3 Pro marker logs match: VFX has
`entries=2 systems=1`, no-VFX has `entries=2 systems=0`, and both have explicit
extension/world/unload success booleans. No GPU, operation, completion,
readback, Ready/Verified, packet, or physical-disposal claim is made.
