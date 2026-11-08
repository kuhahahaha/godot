# Independent test-source review

Reviewer: independent Sol agent, 2026-11-08. Scope: the two new NativeVertexArrayPlan cases and `TestMeshStorageExternalAccess::prospective_plan` in `tests/servers/rendering/test_mesh_storage_external.cpp` (lines 521–622 at review). Read-only test review; no test edits, build or execution. Production fixes to the prior findings were not assumed present.

## Fixture safety findings

1. **Guard dependent vector indexing.** Lines549/551–553,560–562,592 index plan Vectors after nonfatal CHECK calls. A regressed planner returning error/reset output or wrong sizes produces a Vector bounds abort rather than a useful test failure and cleanup. In this noexceptions/native fixture, do not replace these with early-aborting REQUIRE while native resources remain live. Save the call status and conditionally execute dependent assertions only when the required sizes are present, while always running cleanup. Keep the independent size assertions.
2. **Clean up external borrowed buffers on initial acquisition failure.** Lines611–614 clear the external Surface metadata, which deliberately does not own external current/previous/index buffers. Those handles are only saved/freed inside the acquired==OK branch at603–608. Save these handles immediately after constructing the Surface and free them once after either branch; otherwise a regression in initial acquire leaks live RD owners and trips end_native cleanup checks.

Both findings were sent directly to root. They concern failure-path robustness; the intended passing-path cleanup is sound.

## Verified safety and assertion properties

- The synthetic hold saturation is restored immediately after nonfatal CHECK and before any release. Failed cached VA metadata is restored before disposal. Neither case fabricates a huge Version array; omitting a `version_count=UINT32_MAX` fixture avoids invalid scanning of unreal storage.
- The default-normal RID is a real RD vertex allocation. Temporarily nulling the default slot tests missing-resource rejection without invented RIDs; it is restored before later derive. The minimal MeshStorage destructor does not free defaults, so the explicit free(normal) is correct and not a double free.
- Detachment occurs while the retained hold is live. Old Surface fields are read before final retained release; no subsequent dereference of original occurs after that release. Derived custody is released first. External backings are freed after the retained physical hold returns, and pending frees are drained before owner/dependency checks.
- Global MeshStorage singleton is restored; fake RD format/cache state is cleared through the existing end_native helper. No exception-dependent RAII or REQUIRE was introduced.
- Assertions independently check selected IDs, counts, vertex count, first descriptor location/stride/data format, default binding and offset values, unsupported modes, rejected external mask, frozen existing/missing behavior, source release rejection, and actual no-factory/no-Version/no-hold changes across the initial plan/rejection calls.
- Failed-cached-Version is deliberately synthetic for both kinds of Surface. External factories do not normally publish such a failed entry, but rejection is a useful invariant check and restoring metadata keeps cleanup valid.

## Coverage limits, not claims of runtime acceptance

The initial no-factory snapshots do not enclose the later frozen/thaw plan-only calls. External generation-key mismatch, zero/subset-mask full descriptor details, implicit ordinary binding value, default stride/format, and the full external descriptor table are not independently asserted by these two new cases. Existing neighboring tests cover some actual factory behavior, but this small fixture does not establish broad before/after factory equivalence, motion/instance/point-size preservation, overflow safety, RD eligibility, or actual GPU behavior. The saturation and cached-failure assertions should fail against the uncorrected original candidate and are appropriate targeted regressions.
