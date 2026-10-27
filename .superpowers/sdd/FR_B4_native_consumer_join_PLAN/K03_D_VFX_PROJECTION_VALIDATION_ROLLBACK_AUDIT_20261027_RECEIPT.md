# K03 D-side VFX projection validation/rollback receipt — 2026-10-27

- Audited current ANGE HEAD `e6bb496f4b374e8727470637f7e312b5df1fdf83` source only.
- WorldLoader resets projection state, requires Ready models and exact bundle/projection/lease identity, clears projections on failure, and publishes only after all systems succeed.
- EcsWorld validates every projection before scene compilation/mount and rolls back at `vfx_semantic_projection` on failure.
- Same-path deferred runtime erases retain a live valid `WorldPath` runtime during reload interleaving.
- This source audit does not reattribute final2 binary results to the newer HEAD.
- No U08/E09, operation, completion, readback, packet, physical disposal, Released, Ready, Verified, or GPU claim.
