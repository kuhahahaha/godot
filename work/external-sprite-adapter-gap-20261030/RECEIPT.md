# Sprite/ExternalOutput typed adapter gap audit — 2026-10-30

## Conclusion

No production Godot entry currently connects a Sprite/ExternalOutput draw packet to typed Godot texture/sampler/resource handles. The existing external seams are separate:

- `RenderDataRD::PacketIdentity` admits camera/source tuples and material generation/capabilities, but carries no texture or sampler descriptor.
- `MeshStorage::ExternalSurfaceDescriptor` validates borrowed vertex/index/index-array RIDs, allocation generations, capacities, and a material RID. `mesh_add_external_surface()` has no production caller in this tree; only `test_mesh_storage_external.cpp` references the descriptor.
- `TextureStorage::texture_external_initialize/update()` are the existing external texture path, but do not consume a draw packet or typed sampler/material generation.
- `RenderingDevice::validate_external_indexed_surface_buffers()` validates the Indexed144 buffer tuple only; it is not a Sprite/ExternalOutput adapter.

Adding an adapter would require a new authenticated owner/schema contract for texture, sampler, material generation, resource generations, range validation, and retirement. Inferring those from a packet, ObjectID, or fixture would create unowned GPU authority, so no production code was changed.

## Pair and regression evidence

- Godot source HEAD: `ce71f75814b9c705e67fa0eab7818328cfeb7516`.
- Editor SHA-256: `b39543b6c5b8d96e3acabae8b3212710cc60ffc0248fa4f8261eab9491bb1be9`.
- ANGE source HEAD: `8e2f7d282e96e406b5311b23db74c16328dcc4cf`.
- DSO SHA-256: `38df3a53ee04aa6d977b313853a2bb3bd7efafe207a960920f80644535a4fcab`.

Focused tests all rc 0:

| Selector | Cases | Assertions |
| --- | ---: | ---: |
| `*[RenderingDevice]*` | 12 | 214 |
| `*[Qualification]*` | 102 | 1,896 |
| `*[K03]*` | 44 | 1,153 |
| `*[R01]*` | 16 | 293 |

Current Metal pair VFX/no-VFX/invalid-VFX controls each returned rc 2 after Metal startup and stopped at E05 `host.storage_owner_transport`; no packet admission, texture/sampler binding, native completion/readback, visible output, Ready, or GPU claim is made.
