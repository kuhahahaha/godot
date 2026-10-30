# Mesh external surface admission / retirement audit — 2026-10-30

Audited current Godot HEAD `f7cacfa60a9b5c97032de78b2a438a01ff469e50` with the
explicit ANGE DSO recorded in `DSO_SHA256.txt`. No production source was modified.

The descriptor admission path already validates RID ownership, exact 144-byte
vertex ranges, UINT32 index ranges, usage flags, index-array identity, primitive,
mesh state, and distinct nonzero generations before publishing. Partial allocation
failure deletes the unpublished surface; successful external surfaces borrow caller
buffers, and teardown frees generated vertex arrays without freeing borrowed buffers.

There is no production caller of `mesh_add_external_surface`; only tests cover the
private cache. A live external owner/generation producer is therefore upstream of
this repository. This package records that boundary without adding a synthetic
caller or owner.

## Fresh validation

- `[B4][K05]`: rc 0, 4/4 cases, 101/101 assertions.
- RenderingDevice: rc 0, 12/12 cases, 214/214 assertions.
- Qualification: rc 0, 102/102 cases, 1896/1896 assertions.
- K03: rc 0, 44/44 cases, 1153/1153 assertions.
- R01: rc 0, 16/16 cases, 293/293 assertions.

The current Metal VFX/no-VFX/invalid-VFX pair each returns rc 2 at E05
`host.storage_owner_transport` UNAVAILABLE before world admission. No operation,
completion, readback, visible output, Ready, or GPU claim is made.
