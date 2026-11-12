# Instance192 generation selection candidate review

2026-11-07, independent read-only review of `work/instance192-generation-selection-candidate/01-generation-selection.patch` and `02-layout-test.patch`, R2 snapshots. No active source edits, build, tests, or Git operations. The metadata full build is outside this review.

## Verdict

No confirmed new CPU-prefix accounting defect, lease-release loss, root-return dangling access, or cleared-vector read regression was found in the inspected R2 component paths. This is source review only. The patches are not a complete Instance192 Create integration and are not sufficient to remove the public profile gate. The two concrete acceptance blockers are the missing original Create/quote wiring and missing directed lifecycle coverage described below.

## Acceptance blockers

1. **The original Create/metadata receivers do not invoke this component.** `particles_storage_external.cpp::_external_create_received` still rejects every profile except SceneOnly, uses `plan_metadata_layout` with legacy defaults, constructs `ExternalOutputOwner(config, generation, true)` without the selection opt-in, installs the directory without `p_generation_selection`, and never calls `acquire_generation_selection`. Existing query/create layout calls in the same file likewise do not use `plan_generation_selection_layout`. Applying 01/02 alone therefore cannot create an Instance192 generation. This limitation is acknowledged in the candidate README; it is not a hidden regression in the component. Before a gate change, the original typed quote and Create must recompute the same derived binding/storage/selection counts, move the actual metadata lease, install the independently bound generation root, call acquisition, and publish a successful RID only after complete selection. A failed acquisition must leave the unpublished owner reachable through that original root for OBSERVE, rather than destroying it synchronously or losing its RID locator.

2. **02 tests layout arithmetic only.** It verifies row alignment, disjointness, unequal consumer/binding/storage/selection counts, checked overflow, and preservation of the last successful layout on error. It does not exercise actual mesh/material selection leases, partial acquisition, failed release, original OBSERVE dispatch, terminal metadata return, or unpublished RID destruction. Existing material lease tests validate the lease primitive separately; they do not verify this new owner composition. Required before acceptance: fail on the first/later mesh and material; verify exact retained prefixes and blocked mutation/free; one-pass OBSERVE progress and reverse release order; release-error retry with unchanged count; directory destruction only after all leases return; terminal unpublished particles_free with exact metadata return and no later pin/owner access; a successful generation surviving until caller free; and root-source/device-loss refusal preserving custody. These are real missing behavioral checks, not failures observed by this read-only review.

## CPU acquisition and failure state

Candidate `external_output_owner.cpp:725–765` authenticates the exact original request, generation pin, placement metadata context and allocation before the first CPU selection acquire. It obtains original mesh leases first and increments `retained_mesh_count` only after each successful call. It rechecks the actual total flattening while meshes are pinned, then acquires each original material using its captured generation and increments `retained_material_count` only after success.

The failure closure runs `withdraw()` before setting `generation_selection_unpublished` and FAULTED_RETIRING. From CONFIGURED, `withdraw()` closes every view and slot and prevents new admission. Any acquired prefix remains in the placement rows; there is no local destructor rollback. Mesh/material NativeSelectionLease destructors reject active custody, so the retained owner must continue through its original retirement service. Successful mesh acquisition has no later failing increment; material acquisition checks both independent overflow counters before incrementing either. These match the prefix counts maintained by the candidate.

Pre-acquisition authentication/layout/singleton failures return before `generation_selection_attempted` and before CPU holds. The future Create caller must handle this distinct no-selection-prefix case while preserving any generation root/metadata it already installed. The candidate itself supplies no caller for either error class.

In candidate `particles_storage_retire.cpp:171–193`, a charged service slice releases one material from the retained suffix, then meshes after all materials return. Each count decreases only after the original release returns OK. An unavailable singleton or failed original release preserves the count and row. CPU lease returns are charged to `passes`, set the original `controls` usage, and do not claim native physical-return receipts.

## Root return, destruction and budget

The generation service first runs original native/source retirement observation and `generation_returnable`, which refuses live slots, retained slot resources/pins, open view histories, or unclosed admission. Selection release precedes placement directory destruction. `return_reserved_cells` additionally refuses CPU selection custody; `_production_output_generation_ready` refuses any remaining row pointer or custody before root return.

The unpublished terminal branch (`particles_storage_retire.cpp:206–219`) checks the original control/pass/receipt/deadline availability before asking Core to return the root. Core independently authenticates the exact executing OBSERVE budget/usage identity and capture pin, checks the original source/physical obligations and request frontier, records its terminal result, unpins the generation and charges one pass. The extra preflight does not reserve a second pass; root return and immediate CPU destruction are one terminal component phase.

After successful root return, the candidate sets RELEASED and calls `particles_free`, then returns immediately. `particles_free` moves the metadata lease to a stack owner before destroying the placement owner and returning its backing allocation. It rejects any active generation/slot state through the existing destructor guards. The metadata return increments the original request revision; Core's service entry already reserves its resource/revision margin. No separate GPU completion is inferred from that CPU destruction.

The callback path was followed through `RenderingDevice::_production_output_owner_service` and `Registry::production_output_owners_service_locked` (`gdextension_production_request.cpp:2035–2094`). The RenderingDevice wrapper directly returns the callback result. Core saves scalar locator identities before dispatch, retains the original request via `capture_inflight`, reacquires RD/Registry locks, and re-resolves `request.uses[index]`. If the row was unpinned, it returns without reading the old pin pointer. The candidate likewise does not read `owner` or `p_pin` after `particles_free`. No dangling dereference was found on this path. Its actual same-callback metadata-free behavior still needs the directed test above.

## Count and projection audit

The R2 layout distinguishes:

| Quantity | Meaning / derivation |
| --- | --- |
| `generation_mesh_count` | Number of original configured mesh RID rows. |
| `generation_material_count` | Authenticated flattened surface/material rows M. |
| `slot_count` / native `storage_count` | P = max_views * max_inflight. |
| Native `binding_count` / `binding_capacity` | P * M SurfaceBinding placement rows, checked before narrowing. |
| `surface_capacity` / layout `surface_count` | Independent consumer SurfaceCell lease directory capacity. |
| `view_capacity`, `read_capacity` | Independent consumer directories, not inferred from P or M. |

`plan_generation_selection_layout` checks the config's V/I limits, validates real mesh/material selection, derives P and P*M with uint64 arithmetic, and feeds separate counts into the common checked planner. SurfaceCell remains the final slice. The legacy common planner defaults remain compatible with existing callers; they do not automatically produce this native generation layout.

After placement install, mesh/material identities and material generations reside in quoted rows, and the two config vectors are cleared. The candidate's acquisition uses those rows. The only active post-Create config-vector read found in the owner/output paths was output acquisition's flattened material count; 01 replaces it with `generation_material_count` when `generation_config_inline`, rejects incomplete generation selection, uses `binding_capacity` for P*M, and reconstructs metadata offsets with all independent counts. Actual native storage and binding placement use slot_index and that projected M. `storage_capacity` is derived as P at install; no unrelated consumer count is used for it.

Other config readers found in `particles_storage.cpp`, output selection, and retirement read scalar bounds, query flags, profile, capacity, V/I, and inline digest bytes. They do not depend on the cleared mesh/material vectors. `validate_native_selection` and the native planner still expect parser vectors and are used before clearing; they must not be called on the owner's cleared config in future integration. Existing public surface selection/view/resolve endpoints remain unavailable and make no generation-row read claim. No missed current vector read was found; renderer integration remains outside these patches.

## Scope and identity

Reviewed the R2 candidate snapshots named by `IDENTITY-R2.json`, including output acquisition changes in its fifth entry. No CPU fixture or source inspection here establishes GPU/native completion, raster readiness, successful Instance192 creation, or full qualification. The component structure is suitable for the next original-caller integration review once the two acceptance blockers are addressed.
