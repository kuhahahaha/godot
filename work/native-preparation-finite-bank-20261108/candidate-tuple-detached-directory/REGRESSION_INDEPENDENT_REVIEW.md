# Regression independent review: detached Surface tests

Scope: source-only review of the changed continuation tests and test-only access seams. No build or runtime claim.

## Finding (P2): helper failure paths leak already-created vertex arrays

`install_detached_geometry` creates `detached_vertex_array` at `tests/core/extension/test_production_request.cpp:8262-8266`, then creates `detached_other_vertex_array` at `8270-8274`. `MeshStorage::_mesh_surface_dispose(surface)` is called on either failure, but at both points `surface->versions` has not yet been assigned (assignment occurs only at `8277`), and the two VAs are held only in the probe fields. `_mesh_surface_dispose` therefore cannot discover/free either VA. The first-VA failure can leak that VA; second-VA failure leaks the first VA (and potentially leaves the failed second RID according to the driver contract). The helper then returns false and the continuation exits, while the outer cleanup only frees detached RIDs when `detached_geometry_installed` is true (`~9302-9315`), so these partial-construction failures are not reclaimed by the test harness.

This does not weaken the normal success-path invariant, but it makes setup failure non-clean and can contaminate later tests. A test-only cleanup guard should free every already-created VA explicitly (and the buffer) before `_mesh_surface_dispose`, or assign temporary Version entries before any fallible creation and clear them on rollback. Keep cleanup exception-free and preserve the existing `check_step`/`CHECK` semantics.

## Invariants that are meaningfully exercised

* Modes 3/4/5 construct an actual retired `Mesh::Surface`, two genuine Version VAs sharing one VB, acquire/derive a selected hold, clear the mesh surface, release the source hold, and leave the row as the final retained metadata holder (`~8242-8327`). This exercises detached status and the selected-versus-unselected directory distinction.
* The test asserts tuple-key reuse and distinct allocation for changed subpass/backend, then probes all key fields against a bounded P=3 ledger (`~8342-8399`). The comment explicitly labels this as a private ledger probe and says it does not issue production ViewCell or surface authority (`~8339-8341`), so the mock scope is truthful.
* Modes 3/4/5 assert finish completion, physical transfer, retirement-directory presence, and that both the selected and previously unselected VA plus VB are members of the original accepted hold (`~8532-8549`). Final checks require all detached owners and alias storage to be gone (`~9302-9315`).
* Mode 4 closes inside the original scope; mode 5 combines detached geometry with a failed raster factory. Their assertions distinguish `closed_inside_scope` and `failed_pipeline`, and the common bounded observe loop checks eventual owner-consumed/row-bank restoration rather than claiming one-turn cleanup.
* `ExternalPipelinePreparation`'s TESTS_ENABLED friend and the bridge's forward declaration are narrowly scoped to the test accessors. The `operator==` compares all tuple fields used by the key (including specialization array and booleans), avoiding a hash-only test oracle.

## Limits

These tests provide source-level regression coverage for the detached tuple seam and cleanup accounting. They do not establish GPU execution, production issuer authority, or native runtime fence behavior.
