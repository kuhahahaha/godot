# Native external texture import failure propagation audit

Baseline Godot HEAD before this package: `98a205264f2d1be15d501e631aaecdb13d48ebbd`.
The pre-fix source had a void `TextureStorage::texture_rd_initialize()` called after
reserving a TextureStorage RID in `texture_create_from_native_handle()`. Validation
failures returned through `ERR_FAIL_*` with no status, leaving the reserved RID
uninitialized. A failed shared or sRGB view creation also had no way to tell the
caller to release the extension-created source RID.

The production fix changes the virtual contract to `bool` across the common,
RenderingDevice, GLES3, and dummy storage interfaces. The RD implementation now:

- uses value-returning fail-closed checks;
- rejects a null primary shared view;
- frees a primary shared view if the optional sRGB view fails; and
- returns `true` only after `texture_owner.initialize_rid()` succeeds.

`texture_create_from_native_handle()` initializes and frees its reserved RID on
failure, frees the extension-created source RID, and returns a null RID. This keeps
failure cleanup local to the import transaction and does not invent an owner or
completion. The GLES3 implementation remains unsupported and returns `false`,
matching its previous empty implementation while satisfying the typed contract.

The source change is limited to:
- `servers/rendering/storage/texture_storage.h`
- `servers/rendering/dummy/storage/texture_storage.h`
- `servers/rendering/renderer_rd/storage_rd/texture_storage.h/.cpp`
- `drivers/gles3/storage/texture_storage.h/.cpp`
