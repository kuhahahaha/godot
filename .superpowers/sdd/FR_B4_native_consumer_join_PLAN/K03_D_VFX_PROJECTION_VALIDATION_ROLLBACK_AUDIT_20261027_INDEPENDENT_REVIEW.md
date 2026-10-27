# Independent review — D-side VFX projection validation/rollback (2026-10-27)

## Verdict

**Limited-scope accept; no P0–P2 findings.** The audit is correctly framed as
source-only evidence for ANGE HEAD `e6bb496f…` and does not reattribute the
older final2 runtime receipt to this newer source.

## Review findings

- The six source hashes are consistently recorded against the stated source
  HEAD in JSON. The current Godot worktree does not contain the separate ANGE
  source tree at those paths, so this review treats the hashes as
  provenance-level rather than freshly rehashing the six files.
- The described `WorldLoader::load_vfx_sources()` ordering is fail-closed:
  reset state, require Ready model, require bundle/projection/lease and exact
  identity, clear accumulated projections on failure, and publish only after
  all systems succeed. No fallback/default registry/synthetic lease is
  introduced.
- `EcsWorld::do_world_load()` validation is placed before scene compilation,
  resource preparation, or mount, with explicit rollback at
  `vfx_semantic_projection` on failure. Null and registry-invalid projections
  are rejected.
- The same-path deferred erase guard retains a live valid `WorldPath` runtime,
  protecting unload→reload interleaving from deleting the newly published
  runtime.
- The runtime reference is explicitly scoped to the separate
  `K03_FINAL2_PAIR_U01_CLOSE_RECHECK_20261027` binary snapshot and is not
  attributed to the newer source HEAD.

## Claim boundary

The all-false claims for U08/E09, operation, completion, readback, packet,
physical disposal, Released, Ready, Verified, and GPU are appropriate. This
audit establishes source publication/validation/rollback and lifecycle guards
only; it provides no renderer packet or operation authority.

No source changes were made and no tests were rerun.
