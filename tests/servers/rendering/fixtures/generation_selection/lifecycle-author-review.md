# Generation selection lifecycle author review

Source review of applied generation-selection production code and the new original accepted-request fixtures. This reviewer authored these changes; this record is an author audit, not a substitute for independent review. No build/test/git run by this agent. Root's first fixture build exposed private Registry allocation access in the bridge; that was repaired as described below, with failure evidence retained by root. Subsequent verification is pending.

## Findings and corrections

1. Actual compile failure: ExternalLinkedOwnerTest is not a Registry friend and cannot call production_output_metadata_create. Allocation now occurs in existing friend TestProductionRequest::Access's callback; the resulting actual lease is passed by reference into the owner bridge. The native planner supplies exact bytes/alignment. No production friend/public access was added.
2. False-positive gap: the outer test transaction error guard originally masked deletion of acquire_generation_selection's in-loop fail transition. The stale-second-material path now asserts attempted=true, FAULTED_RETIRING, unpublished=true, admission closed and every view/slot closure BEFORE the outer guard runs. The pre-first-lease invalid-context path separately asserts CONFIGURED/unattempted before explicitly exercising the caller guard. These are different responsibilities.
3. Aggregate-only material counts did not prove which original material lease returned first. Per-turn checks now assert each original material RID's selection count: material1 before material0, then mesh. Shared shader count and original mesh count must match the remaining prefix at each step.

## Terminal UAF audit

Production generation service authenticates its original root/context/budget and native retirement facts, returns one CPU lease per pass, then destroys empty directories in another pass. Root-ready refuses selection pointers/custody. It snapshots free_unpublished before unpin, preflights terminal budget, calls original generation_return, marks RELEASED, invokes particles_free and immediately returns. No owner or p_pin access follows particles_free in this branch.

particles_free moves the placement metadata lease out before owner destruction and frees the allocation through the original Registry. RenderingDevice forwarding returns directly. Registry::production_output_owners_service_locked had advanced the owner cursor before callback, retained capture_inflight, then reacquires locks and re-resolves the original finite request row; it does not dereference its saved owner/pin pointer after the callback. The unlinked row branch handles same-callback owner destruction. No production UAF found in this path by source inspection.

The test bridge temporarily retains an old address after auto-free. exists() looks up the actual particles RID and only compares the address if an owner still exists; it does not dereference the stale address. forget_auto_returned requires the actual RID to be absent, then clears the address. Successful owners remain live until the explicit caller free. This is not a synthetic Released transition.

## What the fixture proves if it passes

The metadata bytes originate from native generation selection planning against actual original CPU mesh and material owners. An original accepted request declares the exact allocation; protected RECORD allocates metadata, places rows, installs/binds an independent generation root, moves the lease and invokes actual selection acquisition. Selected identities are inside the slab; owner config vectors are empty, binding count is separate from7 consumer cells, producer tag bytes match inline storage. Parser vectors/tags are dropped before retirement.

The source fixture intentionally has CPU mesh/surface owners without VA/buffer factories and ShaderData/MaterialData without ShaderRD variants. Materials still use original initialize, set_shader and queued service. This validates CPU identity leases and owner composition only. It cannot attest physical geometry, compiled shaders, guard readiness, source qualification, public Instance192 creation or GPU work. Zero graphs/native memberships/driver issues are expected, not missing simulated completion.

The success path retains mesh+two materials, then returns one lease per bounded OBSERVE, destroys directories, returns root and leaves metadata/RID for caller free. The stale second-material path retains the mesh+first-material prefix and auto-frees metadata/RID in the terminal callback. The invalid-context post-root path acquires no lease but exercises the explicit outer transaction failure guard and actual later auto-drain.

## Outer transaction boundary remains explicit

acquire_generation_selection has pre-attempt errors that leave CONFIGURED and do not set unpublished. The fixture deliberately demonstrates that fact before its own caller guard runs. Public Create remains gated and does not gain this behavior merely from the fixture. Any future actual receiver must apply withdrawal/unpublished/FAULTED_RETIRING to every post-root failure, including metadata/context/singleton failures before the first lease. The separate private receiver integration candidate requires its own review and runtime tests.

## Remaining coverage limitations

- Only one mesh and two material rows; later-mesh/first-material failure prefixes not covered.
- No release-error retry, zero/deadline-exhausted budget, source-device-loss or nonterminal request-frontier scenario in these new cases.
- Public typed quote/Create receiver is not the fixture's entry point. Internal bridge uses fail-closed assertions for supposedly infallible setup and does not test allocation/root/directory setup failure rollback.
- The inline check compares only producer_tag among five tags; it does not validate all material generation snapshots independently, although stale-generation behavior exercises the second snapshot.
- No actual output acquire is exercised after clearing vectors; production output uses the new count in source, but this test does not prove that downstream path.
- No sanitizer/UAF runtime evidence yet; source reasoning above is not sanitizer qualification.

These limits should remain in acceptance reports. No additional production counting or terminal-order defect was confirmed by this author audit.
