# Independent re-review — mesh revision findings P1/P2

Decision: **both previously reported source findings are resolved in the revised candidate; no new implementation blocker found in this follow-up source review.** The producer is still pending behavioral acceptance and B/public transport integration. This is not a full mutation-matrix or GPU pass.

Default Sol, 2026-11-08. Follow-up to historical `INDEPENDENT_REVIEW.md`, which remains accurate for its older patch. This review did not modify live production/tests, build, compile or execute tests. Only this document is written.

## Exact reviewed version

Verified patch SHA256 `4c410fc8ede43c1c8c24bcc2c1fa2f4e181b335103e5c02a78f8336df4e4ffeb`. Verified all three originals against `SOURCE_PINS.json` using pinned base `b220d7467b43ac4e921f5fda6944394083b60997`, independently replayed the patch in a temporary directory and compared every resulting byte with the retained after-files. All three match. Removed temporary reconstruction afterward.

Retained after hashes:

- `mesh_storage.h`: `c0a752a7dae94c8ccdf9b9464a4da36ba79d24bc40d770470abb1b30e39d891e`.
- `mesh_storage.cpp`: `4f792980b354226c6581caaa326ba9dc250066f3ed42f30036067a08936dd2ba`.
- `test_mesh_storage_external.cpp`: `f86d0420223fc85342cd283f5fb410da6a32ded173c47a2c248108cb92b03061`.

Read revised full patch, relevant retained after-functions/tests, REVIEW_FIXES and SYNTAX_RESULTS. Source line numbers below refer to retained after-files.

## Closed findings

**P1 — failed BONES add no longer mutates the owner under its old revision.** `mesh_storage.cpp:458` rejects absent vertex/index counts before `memnew(Mesh::Surface)` at 460 and before any corresponding new buffers or owner state writes. `has_bone_weights` is now assigned at actual surface publication (597), immediately preceding surface-count increment and original revision advance. Subsequent instance/notification work still sees the same published flag. The old failure path therefore cannot silently change `mesh_needs_instance` or leak that temporary Surface. Ordinary valid publication retains the original owner semantics.

The new failed-BONES fixture (`test_mesh_storage_external.cpp:428`) uses the actual current format-version value plus BONES. That constant is already shifted in `rendering_server_enums.h`; the input does not accidentally take the compatibility-conversion branch. Zero vectors/counts pass original debug size checks and reach the reviewed presence rejection. The checks cover actual `mesh_needs_instance`, the original bone flag, surface count/pointer, original revision and unchanged driver allocation count. It preserves the preexisting real Surface rather than trying to clean up an unreachable failed temporary allocation. The driver counter covers GPU allocations; absence of the CPU Surface allocation is established by the inspected guard order, not by that driver counter alone. These tests have not been executed in this review.

**P2 — original notifications are preserved independently of revision changes.** `_mesh_invalidate_shadow_owners` (248) now conditions only the actual valid→null shadow change and revision advance. `dependency.changed_notify` remains unconditional for every original reverse entry, matching all five replaced loops. A selected stale-null target is not revision-invalidated and remains releasable; it still receives the old notification. There is no remaining early continue that drops that edge.

The added real DependencyTracker test (509 onward) checks one callback for actual shadow assignment, another for actual source invalidation and a third for repeated source clear while the now-null target is selected. It checks actual target revision advance for the true clear, stable held revision for the stale-null edge and successful release. The callback's userdata points to an in-scope counter. Explicit `shadow_tracker.clear()` (535) removes tracker dependency entries before owner free, and explicit reverse-edge erase removes the test's stale pointer before shadow source free. This avoids turning fixture teardown into a dangling reverse-owner regression. Existing singleton/original-surface cleanup continues afterward.

## Other follow-up observations

The new MAX test now acquires an actual selection with snapshot UINT64_MAX, checks the value, explicitly returns it, and only then mutates the owner to permanent zero. This covers the release boundary missing from the prior fixture source. A selected owner's legitimate mutation paths remain refused; the saturation model is set only after ordinary selections have returned.

No changes were made to the earlier-reviewed acquire/release validation, zero permanence, region metadata-first revision semantics, derived bounds cache policy or the public ABI. The new tests use existing fixture access and original APIs rather than new production flags. The revision is still a per-owner source/state token paired with full native RID lifetime identity; it is not a globally unique ID or GPU completion fact.

## Precise remaining gaps

1. **Execution is pending.** SYNTAX_RESULTS reports the author's two translation-unit syntax checks exited 0 under the reviewed patch hash. This re-review inspected that receipt but did not rerun it. No behavioral red/green, linked test or actual backend acceptance follows from syntax checks. Execute the focused native selection/mesh fixture after integration before claiming the producer works.
2. **Broader mutation coverage is still pending.** The revised executable test source now covers the two discovered cases and MAX release, but still lacks an executed compact matrix for real same-RID structural remove/clear/readd; successful region update plus metadata mutation followed by actual RD range/failure rejection; custom/derived skeleton bounds; successful instance attach/detach; cache-only VA/material-array operations preserving revision; and release snapshot-mismatch refusal preserving custody. TEST_PLAN records most of this honestly. Do not promote a written inventory into acceptance evidence.
3. **COW and higher-level ownership remain separate.** This patch does not run Host resource COW, extend pre-Ready request preparation, expose a checked public generation reader, or wire B's native mesh-generation contract. It provides an internal held revision source only. Source/account/currentness and original finite-bank retirement must still be checked by the eventual consumer/transport.
4. **Full profile material evidence remains separate.** This does not qualify material/profile compatibility or replace inverse/compiler facts with a boolean success.

No additional source defect was established in the revised delta. The P1/P2 fixes can proceed to integration and focused behavioral validation without waiting for another speculative implementation change; complete producer/renderer acceptance still requires the outstanding evidence above.
