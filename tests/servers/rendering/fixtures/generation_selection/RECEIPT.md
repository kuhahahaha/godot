# Native generation selection custody

This batch adds an internal opt-in Instance192 generation selection substrate, preserving public SceneOnly query/Create gates. Native metadata contains separate original mesh/material identity and lease rows, per-slot storage and flattened binding rows, and independent consumer capacity. Parser vectors and fixed tag handles are not retained by inline owners. Original MeshStorage/MaterialStorage leases retain the acquired prefix on failure. Original OBSERVE returns one CPU lease per admitted pass, destroys directories in a separate pass, then returns the original generation root. Unpublished failed owners free their original RID and metadata in the same terminal callback.

## Verification

Build command: `scons -Q platform=macos target=editor tests=yes debug_symbols=no module_particles_runtime_tests_enabled=yes -j2`.

Final directed `[GenerationSelection]`: 3 cases,381 assertions,exit0,no diagnostics. Final combined:214 cases,91659 assertions,exit0. Combined diagnostics are exactly three deliberate pipeline allocation failures,three foreign-camera refusals and one deliberate invalid UTF-8 diagnostic, matching the existing negative fixtures. Both runs use binary SHA256 `05d0d26604829e72b90e3f0f4a781d0ca5e63a1d27d67b23c7d44d8eb168297e`,156961640 bytes; full commands and logs are adjacent.

The three actual accepted-request cases cover successful retained CPU selection, a stale second material retaining the first material and mesh prefix, and post-root invalid context before the first acquired lease. They check original hold counts, exact reverse material return order, one-pass progress, delayed root return, successful caller-owned free and failed unpublished auto-free. The third case explicitly supplies the transaction caller guard in the test helper; it does not claim production Create integration is implemented.

Counterfactual: removing the component failure lambda's withdraw/unpublished/FAULTED_RETIRING transition causes one of three cases to fail with6 target assertion failures out of381,exit1,without a crash. Restoring the source produces the final green result. This demonstrates the outer test cleanup does not conceal that production failure transition.

## Corrections during verification

First build failed because a test bridge called a private Registry allocator. Allocation moved into the existing friend Access callback; no production access was widened. The first runtime fixture then crashed before lifecycle execution: MeshStorage's embedded SkeletonShaderRD constructs ShaderRD, which requires an already initialized RenderingDevice singleton. The fixture now prepares its original device before constructing MeshStorage. Initial build and runtime/trace failure evidence is retained. Neither failure is represented as an intended RED behavior test.

## Limits

CPU/dummy component evidence only. No actual GPU shader compilation, vertex-array/pipeline retention, public Instance192 Create, seal/consumer path, visible draw or physical GPU retirement is certified. Existing public profile gates remain. Inline selection is an internal opt-in with no public caller in this batch. Source reviews and their original limitations are included; later receipt facts supersede pending-runtime statements in older review snapshots. The entire long work queue remains active.
