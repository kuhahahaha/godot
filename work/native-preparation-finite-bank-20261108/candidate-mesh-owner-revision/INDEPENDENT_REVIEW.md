# Independent review — original mesh owner revision

Decision: **not ready to integrate as the complete native mesh revision producer**. One concrete mutation-tracking defect remains. A separate ordinary dependency-notification behavior change should be removed or explicitly validated before integration. The candidate's existing tests are safe boundary extensions in the inspected fixture, but do not yet exercise most of its new production mutation tracking.

Default Sol independent source review, 2026-11-08. Read AGENTS, local godot-fr skill, the candidate patch, README, MUTATION_MATRIX, TEST_PLAN, generator and pinned originals. No production/test edits, build, syntax compilation, executable or GPU run performed by this review. Only this review document is written.

## Fixed review inputs

Base `b220d7467b43ac4e921f5fda6944394083b60997`. Verified all three original file hashes from `SOURCE_PINS.json` using `git show` at the pinned base. Verified patch SHA256 `c084dfefa6ee8cab78f26831127e906806c15a764abfd24119e2e0852c83e144`. The candidate contains no after/ copies; independently reconstructed all three full after-files by applying its patch to pinned originals in a temporary directory, then read the changed paths against their original owner/mutation code. Temporary reconstruction was removed. All line references explicitly marked “after” below refer to that reconstruction.

Reconstructed after SHA256:

- `servers/rendering/renderer_rd/storage_rd/mesh_storage.h`: `c0a752a7dae94c8ccdf9b9464a4da36ba79d24bc40d770470abb1b30e39d891e`.
- `servers/rendering/renderer_rd/storage_rd/mesh_storage.cpp`: `e95d097774fa7312bad3663bac36a3068f67c7fab2b37944c75ad431462a34b0`.
- `tests/servers/rendering/test_mesh_storage_external.cpp`: `69209cf9f1cbc6a43f26f64b0788d91267b5a0789d07299a2924d693ed8ba6fa`.

Root concurrently integrated the separate material observation slice; those moving live files were not substituted for these pinned mesh inputs.

## Findings requiring changes

### [P1] Failed add can mutate original owner state without advancing revision

After `mesh_storage.cpp:514`, `mesh_add_surface` sets `mesh->has_bone_weights = true` when the incoming format has BONES. After line 539, the same call rejects `!new_surface.index_count && !new_surface.vertex_count`. The new revision advance is only after actual directory publication, after line 602. Consequently a rejected call can change the original Mesh's state and later selection eligibility/renderer behavior while preserving its old revision.

Concrete source-level trigger: begin with an ordinary nonempty mesh whose `has_bone_weights` is false. Supply a `SurfaceData` with current format version plus BONES, zero vertex/index counts and empty vertex/index/skin/blend/attribute arrays. The debug byte-size validation permits these zero counts; no earlier index/vertex presence guard runs. The call sets the owner flag and then rejects for missing both arrays. `mesh_needs_instance(mesh, true)` changes from false to true (`mesh_storage.cpp` original lines 2031–2035), but `(RID, revision)` remains identical. New selection still accepts because its predicate checks actual surface skin buffers/instances, not the owner's changed `has_bone_weights` flag.

The preexisting premature owner write is not introduced by this patch; the new revision producer's claim to track original owner state makes overlooking that write a defect in this feature. The matrix's statement that all content validation precedes publication overlooks this failed prefix. The candidate already treats region API metadata invalidation before RD failure as real state mutation; apply the same consistency here.

Minimal fix choices: move the owner `has_bone_weights` update after all ordinary validation that can reject before publication, or advance at its actual first existing owner-state change and define rejected-prefix semantics accordingly. Prefer avoiding a premature owner write where possible; account for the preexisting temporary Surface/buffer cleanup separately if touching its validation ordering. Add a regression that compares actual `mesh_needs_instance` and held/reacquired revision around this rejected add. It must fail if an owner flag changes while revision is preserved; merely checking surface_count is insufficient.

### [P2] Shadow consolidation drops existing ordinary dependency notifications

After `mesh_storage.cpp:248`, `_mesh_invalidate_shadow_owners` skips the entire body when a reverse entry already has a null `shadow_mesh`. Each replaced original loop unconditionally called that reverse owner's `dependency.changed_notify(DEPENDENCY_CHANGED_MESH)`, including such stale entries. `Dependency::changed_notify` invokes registered callbacks synchronously (`servers/rendering/storage/utilities.cpp:33`). This is an observable behavior change to ordinary meshes, not only a revision bookkeeping change.

I have not established an actual rendered-image regression from those skipped callbacks: after invalidation an already-null owner no longer logically uses this shadow source. Nevertheless the candidate does not supply an equivalence argument or a regression for changing that existing notification protocol, and revision correctness does not require it. Preserve the old notification edge while conditionalizing only the actual nulling/revision advance: an already-null stale target must keep the same revision and receive the prior notification. If intentionally optimizing those notifications instead, separate that behavior change and test the actual dependency/cull rebuild route.

The same helper is used by free/add/external-add/clear/remove, so this affects all five original sites. Do not fix it by advancing a selected stale-null owner's revision: its source was not changed, and doing so would invalidate the live lease and make its strict release fail.

## Source conclusions without a new defect found

- The new revision has a real owner and mutation semantics rather than rebranding RID, caller allocation generation or the E25 constant-1 classification fields. Starting a newly initialized owner's revision at 1 is consistent when paired with the complete original RID lifetime identity. It is not globally unique and must not be treated as such by B.
- Direct tracked successful changes appear covered: surface publication/removal/clear, external publication, material RID, blend settings, custom bounds, path, shadow assignment and instance attach/detach. Region APIs advance at their preexisting capture-classification invalidation before potentially partial RD upload; the documentation correctly avoids claiming successful GPU write count/completion.
- New acquire uses the real original Mesh revision, refuses permanent zero and hold-count overflow before increasing custody, and stores the snapshot in the existing private lease. Release validates owner/device/render thread/RID/nonzero original snapshot/current revision/hold count before decrementing and clearing. Failed release leaves custody intact. This is appropriate fail-closed behavior, provided mutation coverage really prevents revision change under a valid selection.
- Direct relevant owner mutators are blocked by selection_holds. An actually valid shadow association also prevents selection acquisition, so the helper's assertion that an actual valid→null target cannot be selected is supported by current original APIs. A stale already-null target may be selected; it must not get a revision advance on a no-op invalidation. This reasoning does not turn stale reverse pointers into a new lifetime scheme: the candidate inherits the old reverse-set behavior and should not claim to repair it.
- Skeleton-dependent `mesh_get_aabb` previously wrote owner cache fields even while selected. The proposed change returns the computed value while skipping all those cache writes during selection. Unselected actual AABB change advances revision; skeleton key-only refresh does not. Source inspection found no additional selected owner write in this path. A focused test must still check both returned AABB and stored owner fields, including different skeleton keys.
- Material-array cache rebuild, Surface versions/VA directory preparation and render-pass/index bookkeeping derive from retained source and are distinct from source content. Leaving those cache operations outside revision tracking avoids invalidating every draw/preparation. No additional source assignment in the inspected inline mesh getters was found. Material/shader internal mutation remains the independent MaterialStorage contract.
- Revision UINT64_MAX→0 and permanent zero is fail-closed; zero does not block ordinary owner mutations or free. Because selected source mutation is refused, ordinary tracked calls cannot exhaust the owner while a valid lease is held. The exposed `get_revision()` returns zero after release. No new public API or ABI was invented here; B transport/currentness wiring remains separate.

## Tests and remaining acceptance requirements

The candidate changes `ordinary_directory` using the real original MeshStorage owner/lease fixture and existing CPU RD driver. The new path mutation/refusal, material reassignment/reacquisition and modeled exhaustion controls are reasonable. The UINT64_MAX owner assignment is a disclosed boundary setup, not fake production evidence. Temporary manual surface enrollment/detachment and singleton restoration follow the existing fixture and preserve later native-directory tests. Changing the original Surface's material to 998 does not conflict with a later inspected assertion. No new production test flags are present.

Those changes do not test the new structural add/remove/clear increments, region metadata/error prefixes, custom/derived bounds, successful shadow association/indirect invalidation, path notification interactions or instance lifecycle increments. The tests model overflow by jumping to MAX but do not yet prove a live MAX-valued selection can return before the later mutation exhausts its owner. The release snapshot-mismatch path is also new and currently untested. These are substantive producer/lifecycle branches, not a request for mirror tests of every increment.

Before calling this native mesh producer accepted, add and execute a compact owner-state matrix covering:

1. Real same-RID surface remove/clear/readd and one actual data/metadata change; old/nonzero revisions cannot match fresh state. Include the failed-add case above.
2. Region update accepted metadata mutation followed by RD range/failure rejection, and selected region mutation refusal with original release still succeeding.
3. A true shadow-owner relation being invalidated advances that owner; stale-null selected target keeps revision and remains releasable; original dependency notification behavior remains covered.
4. Selected skeleton bounds query returns correct result without writing the owner/cache or revision; unselected changed bounds advances; cache-only material/VA operations preserve revision.
5. A real MAX snapshot is acquired and explicitly returned; the subsequent real mutation goes to zero; new acquire refuses without custody; ordinary mutation/free still works. A mismatched release must leave hold count and lease unchanged until the original test boundary is restored.

No test execution or build was performed in this review, as requested. Existing TEST_PLAN correctly records most of these as pending; a written plan is not evidence that the mutation producer is covered. This review establishes one concrete tracking blocker and one bounded compatibility change to resolve. It does not establish B Ready, public native transport, retained Host COW, profile qualification or GPU success.
