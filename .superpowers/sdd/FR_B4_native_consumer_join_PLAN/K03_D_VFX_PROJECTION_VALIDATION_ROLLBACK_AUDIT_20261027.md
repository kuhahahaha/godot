# K03 D-side VFX projection validation/rollback audit — 2026-10-27

## Scope and result

This source audit covers the D-side VFX semantic projection path currently in
the ANGE worktree. It verifies that source ingestion, projection publication,
consumer validation, and world-load rollback remain fail-closed. It also records
the same-path runtime erase guard that protects an unload followed by a reload
within one ECS progress cycle.

This audit is source evidence for the current dirty tree. The final2 runtime
pair receipt remains a separate binary snapshot and is not reattributed to this
newer source HEAD. No renderer packet, operation, completion, readback,
physical disposal, Ready, Verified, or GPU qualification claim is made.

## Current source provenance

- ANGE source HEAD: `e6bb496f4b374e8727470637f7e312b5df1fdf83`
- Godot editor/DSO runtime provenance is intentionally not claimed by this
  source-only audit.
- Exact hashes for the six audited source files are in the adjacent JSON.

## Source publication boundary

`WorldLoader::load_vfx_sources()` establishes the following order:

1. It converts the world-owned runtime file paths to `project:` roots and uses
   the original `VfxRuntimeReader`/engine asset root.
2. It prepares the complete VFX source model and prints authored diagnostics.
3. It refuses any model whose availability is not `Ready`.
4. For each system entry it calls `compile_system_bundle()` and requires a
   bundle, projection, valid registry lease, and exact projection→bundle
   identity.
5. On any failure it preserves the diagnostic classification, clears all
   accumulated projections, and returns false. No empty projection, default
   registry, synthetic lease, or fallback source is installed.
6. Only after all systems succeed does it publish the projection vector and
   emit the `runtime sources ready` marker.

The loader resets `vfx_models_` and `vfx_system_projections_` at the beginning
of every world load, so a failed or replaced world cannot inherit a previous
world's projection.

## Consumer validation and rollback boundary

`EcsWorld::do_world_load()` calls `validate_vfx_system_projections()` immediately
after `WorldLoader::load_world()` and before scene-attribute compilation,
resource preparation, or mount. The transaction rolls back at the explicit
`vfx_semantic_projection` stage when validation fails.

The validator rejects a null projection and calls
`validate_system_registry_projection()` for every non-null projection. A
failure prints `VFX-SEM-014` and returns false. This is semantic ownership and
structure validation; it does not create an operation, native packet, GPU
submission, completion, or readback authority.

## Same-path runtime erase guard

`drain_pending_runtime_erases()` swaps the pending path list and checks the
current ECS lookup before erasing a runtime. A live, valid entity that still
carries `WorldPath` is retained. This prevents a deferred erase from deleting a
newly published same-path runtime after unload→reload interleaving. The guard
is lifecycle protection only; it does not infer an external owner or release
receipt.

## Runtime evidence boundary

The separately recorded final2 pair package
`K03_FINAL2_PAIR_U01_CLOSE_RECHECK_20261027.*` observed VFX/no-VFX source-stage,
world entry, explicit close, and unload with its own editor/DSO/source snapshot.
This audit does not claim that those binary results execute the newer source
HEAD above. It only records the current source contract and its fail-closed
rollback behavior.

## Claims

U08 producer admission, E09 same-request source/operation association,
operation, completion, readback authority/bytes, packet publication, physical
disposal, Released, Ready, Verified, and GPU qualification remain false.
