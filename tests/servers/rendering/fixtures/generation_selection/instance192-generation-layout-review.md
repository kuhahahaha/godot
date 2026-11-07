# Generation selection R2 layout/config independent review

Read-only reviewer `/root/review_vertex`; scope 01/02 R2 candidate, native layout/config and adjacent active call sites. No active/candidate source edits, build/test or Git operations. Lease release/auto-drain detail is assigned to the other reviewer. All ten base/candidate entries in IDENTITY-R2.json matched at read time.

## P1 — appended free test cannot access private owner layout API

`candidate/tests/servers/rendering/test_external_output_owner_state.cpp:831` introduces a free TEST_CASE that names `Owner::MetadataLayout`, `Owner::GenerationMeshSelection`, `Owner::GenerationMaterialSelection` and invokes `Owner::plan_metadata_layout`. All remain in ExternalOutputOwner's default private section; its first public label is immediately before its destructor. A free TEST_CASE has no friend access. The existing metadata-layout assertions are correctly inside the friend `TestExternalOutputOwnerStateAccess::metadata_layout`. Move the new assertions into a similar friend helper and let the test case call it; do not widen production visibility for this test. Source compilation is blocked until corrected. Root and Astra notified; reviewer did not modify the candidate.

## Arithmetic, alignment and slab placement

The primitive planner starts at native sizeof(ExternalOutputOwner), keeps the cursor in uint64_t, aligns each nonempty slice using its native power-of-two alignof, and proves count <= (UINT32_MAX-aligned)/stride before multiplication/addition. Cursor is <= UINT32_MAX after each successful append, so cursor+align-1 is safe for the actual small compile-time alignments. All strides are nonzero native sizeof values. Result assignment happens only after every slice and the max_align_t bound succeeds, preserving the caller's output on failure. Zero-count offsets remain zero. The final SurfaceCell slice remains last, after the two new selection slices.

The generation planner first resolves actual mesh/material rows through validate_native_selection. That bounds mesh count by MAX_MESH_RIDS and flattened material count by MAX_FLATTENED_SURFACES, checks positive native surface counts and exact material vector length, then bounds V<=8/I<=4. Slots V*I and bindings slots*flattened are widened before multiplication; bindings must fit uint32_t. The new storage count is actual slots, binding count actual slots*flattened, and SurfaceCell count remains the independent consumer capacity. Placement construction of each selection row uses the authenticated slab base plus checked native row offset/stride. Mesh/material count comes from the same synchronous config preflight; no native callback occurs between these scans. Registry metadata validation proves actual original context/lease, minimum byte length and base alignment before any row construction. No separate selection heap allocation is added.

Output acquire recomputes the full same layout using stored selection/binding/storage capacities, uses flattened generation_material_count after config migration, and verifies selected-slot bindings fit binding_capacity. Slot index comes from the bounded original V*I selection, matching generation planner storage_count. Stored private capacities are derived by install rather than caller DTOs. No arithmetic or slice-overlap defect was found along this intended internal opt-in path.

## SceneOnly/default compatibility

Default planner arguments preserve legacy meanings: no selection rows, binding_count=p_surfaces, storage_count=MAX_SLOTS when p_surfaces is nonzero, otherwise zero. The old view/read/storage/binding/SurfaceCell order is retained when new row counts are zero. Existing constructor callers keep p_inline_selection=false; inline SceneOnly gates, frozen tag storage and zero consumer query behavior remain intact. Native sizeof(Owner) necessarily grows, so raw numeric byte sizes/offsets are not binary stable across this patch. Query/create replan from the same current native type; no persisted quote handle is promised. The expected compatibility is semantics and recomputed sizing, not equality to a prior binary's byte count. Existing V2 oversized-metadata behavior is unchanged here.

## Config migration and retirement gates

The opted-in constructor copies the five fixed 32-byte tags into owner bytes and clears its PackedByteArray handles. Successful generation directory installation copies original mesh RIDs and flattened material RID/generation into placement rows, then clears config.surfaces and config.surface_materials and marks generation_config_inline. Clearing removes retained owner references to parser vector backing; the synchronous caller may still own its own input copies. Acquisition reads the row arrays and exact stored material generations, not the cleared vectors.

A repository-wide config.surfaces/config.surface_materials search found the only post-install retained-owner vector-size read in production_output acquire; R2 changes it to generation_material_count for inline owners. Other vector reads are config parsing/decoding, pre-install validation/planning or constructor gating. Public surface-count/acquire/resolve remain unavailable/zero and are not enabled by this candidate. Thus this candidate does not yet provide generation-row publication to those APIs; it must not be described as enabling the full Instance192 transport/profile.

`generation_returnable` intentionally remains a slot/view preflight, so the service can reach selection release. Both of its production call sites were checked: the root-ready callback separately refuses selection pointers/custody; generation service drains custody before directory destruction and root return. return_reserved_cells refuses retained selection custody before destructing any row and destroys both arrays before clearing pointers/counts. The owner destructor independently refuses remaining pointers/custody. Scalar binding/storage capacities and flags survive cleanup but cannot admit work in the withdrawn/retiring terminal state. No missing production gate was found for the new row storage; the separate reviewer owns correctness of charged release/terminal service mechanics.

## P2 coverage limit — planner checks do not exercise placement/config migration

02 presently tests only the pure generic planner, not plan_generation_selection_layout, actual accepted slab placement, config vector/tag retention, changed output acquire counts, or selection-aware cleanup. Deleting the production row placement/copy/clear block or restoring the post-install vector-size read would leave every newly added assertion unchanged. Those regressions would respectively lose generation identity storage/retain parser allocations or reject inline output acquisition; none is covered by 02.

Even the unequal `(surface_consumers=7, bindings=12, storage=4)` assertion checks returned counts only. It does not verify actual storage/binding slice extents, row-to-SurfaceCell disjointness or total bytes for those independent counts. A planner regression that records binding_count=12 but appends only7 binding rows can evade the new unequal-count assertions. Strengthen the friend helper to check all adjacent slice ends/alignment and accepted/first-rejected endpoints for both new row types, with whole-result preservation on failure. Then add an original accepted-request fixture that proves vectors are cleared, identities/generations remain correct in slab rows, parser inputs can change/die, and production acquire uses the row count. Lease-held cleanup/direct partial-prefix retirement cases remain with the other reviewer. These are directed missing tests, not tests run or runtime failures observed here.

## Disposition

BLOCKED on private-access test compilation correction. Within the intended internal opt-in layout/config source path, no additional demonstrated production blocker found. Placement/config and actual generation planner coverage remain pending before accepting the new behavior. No GPU conclusion follows from source review or a future CPU fixture pass.

Read time: 2026-11-07T02:22:19.251630+00:00

- `work/instance192-generation-selection-candidate/01-generation-selection.patch`: `353b54273c14da595dc031d99a3deb36ebcc26a6bd55fb8567a67bf8e5edceb7`
- `work/instance192-generation-selection-candidate/02-layout-test.patch`: `61d71656f6d5a0f4eafd8c964a7ef8887e3618893b508a5285f505dead427bb9`
- `work/instance192-generation-selection-candidate/IDENTITY-R2.json`: `fbbf82f1e67992b0580b55d127b0bd83e0589753fdf36560dc7d35bc5cab1ac3`
- `work/instance192-generation-selection-candidate/candidate/servers/rendering/renderer_rd/storage_rd/external_output_owner.h`: `bc1505c901009fe625b95dc42566120356b0948626033b416d09695c89e5dbdf`
- `work/instance192-generation-selection-candidate/candidate/servers/rendering/renderer_rd/storage_rd/external_output_owner.cpp`: `93d4b2ff502c1be2c1da10611ccd5b0fbff682d4b4f11dfd99bf5f3fa94b9005`
- `work/instance192-generation-selection-candidate/candidate/servers/rendering/renderer_rd/storage_rd/particles_storage_retire.cpp`: `a1dae41ca6d15ed6f8694cd70548280f90d6f56bd5c7e470de1ab45301202137`
- `work/instance192-generation-selection-candidate/candidate/tests/servers/rendering/test_external_output_owner_state.cpp`: `2c780ad18649841ca0f7fe70a3f8904fd33cf48eb52b829dec4ee94ff583f2f0`
- `work/instance192-generation-selection-candidate/candidate/core/extension/gdextension_production_output.cpp`: `5304332bc4f9024f342b01dde6787cb4f27f7af29e1183b67020f0e981a35331`
