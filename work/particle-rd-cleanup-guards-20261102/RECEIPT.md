# RD particle cleanup null guards

Date: 2026-11-02

particles_free and particles_collision_free now reject a null owner lookup before dereferencing particle state. This covers invalid and uninitialized RID lookups at the direct native storage API. It does not reclaim uninitialized reservations or demonstrate a failure in the normal RenderingServer create/free queue.

Verification:
- HEAD baseline source fixture: rc 1, missing guards in both entries.
- Current source fixture: rc 0, 2/2 checks.
- Editor build completed successfully (existing MoltenVK deployment-target warning).
- [ParticlesProcessRetry]*: 3 cases / 47 assertions passed, rc 0.
- [Particles]*: 29 cases / 154 assertions passed, rc 0.
- Editor SHA-256: `f761d5729218d2c5e794b4d948f53e95ab6f856bafd99f2de2f02af0832f1b98`.

The source fixture verifies guard placement. No live RD invalid-RID test was executed; neighboring doctests are not GPU qualification evidence. Independent Astra audit confirmed the RID-owner uninitialized-bit lookup distinction, but inspected a separate worktree; local source locations and guard placement were checked here before editing.
