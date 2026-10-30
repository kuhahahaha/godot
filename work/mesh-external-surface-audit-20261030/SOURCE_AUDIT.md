# Mesh external surface descriptor / retirement audit — 2026-10-30

## Admission and validation

`MeshStorage::mesh_add_external_surface()` (`servers/rendering/renderer_rd/storage_rd/mesh_storage.cpp:537-594`) is fail-closed before allocating a `Mesh::Surface`: it rejects missing mesh, full mesh, zero counts, non-triangle/instanced/blend-shape meshes, null buffer/index/material RIDs, zero generations, equal current/previous generations, undersized 144-byte vertex or UINT32 index ranges, and any failure from `RenderingDevice::validate_external_indexed_surface_buffers()`.

The RD validator (`servers/rendering/rendering_device.cpp:4425-4460`) resolves each RID in the owning tables, checks exact byte sizes, distinct current/previous vertex buffers, vertex+storage usage, UINT32 index format/count/size, index-array offset/count/format, and index-array driver identity. No synthetic RID or owner is accepted.

After validation, the surface stores borrowed RIDs and generation values. `memrealloc` failure deletes the newly allocated `Mesh::Surface` before returning `ERR_OUT_OF_MEMORY`; the surface is only published after reallocation succeeds. External surface cleanup (`mesh_storage.cpp:_mesh_surface_clear`) frees generated vertex arrays but deliberately does not free borrowed vertex/index buffers.

## Production reachability gap

A repository call census finds only the declaration and definition of
`mesh_add_external_surface`; no production caller publishes an external surface.
The existing `[B4][K05]` tests exercise the private Indexed144 cache and its failed
vertex-array construction, but do not drive the public admission function with a
live `MeshStorage`/`RenderingDevice` owner pair. Consequently there is no current
production Sprite/ExternalOutput packet source, owner transport, or generation
producer to validate end-to-end.

No source change is safe in this slice: adding a caller would invent upstream owner
and packet authority, while changing borrowed-resource retirement would risk freeing
resources owned by the caller. This is a precise current-tree gap audit, not a GPU
or Ready claim.
