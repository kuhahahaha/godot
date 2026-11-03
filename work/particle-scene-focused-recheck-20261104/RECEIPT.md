# GPU particle scene headless audit — 2026-11-04

The fixed editor ran the complete `*test_gpu_particles.cpp` source suite and exposed an existing test-harness defect: Vulkan-only production cases use `REQUIRE` for renderer availability and then continue into renderer-dependent code when the headless editor has no compositor/RenderingDevice. The first run crashed in `[GPUParticlesFieldRD]`; after a local uncommitted diagnostic guard, the next run exposed the same issue in `[GPUParticlesSubEmitterRD]`.

The source file already contains other owner changes, so no guard was committed from this turn. The reproducible raw output is retained in `FOCUSED_FINAL.log` (78 cases reached, 1 crash, 383 assertions before abort). This is an owner handoff for a safe skip guard; it is not a GPU pass and no production source behavior was changed.
