# Native external texture import failure cleanup — 2026-10-30

This package fixes one independently attributable failure-propagation gap in the
Godot RenderingDevice texture import path. Before the fix,
`TextureStorage::texture_create_from_native_handle()` reserved a TextureStorage RID,
called a void initializer, and always returned that RID. Invalid imported format,
type, usage, or shared-view creation therefore could leave an uninitialized RID and
leak the extension-created RD texture. The new bool contract is fail-closed and
frees both the reserved storage cell and source RD RID on every failed import.

## Build and focused regressions

- Module-on editor Debug build: `EDITOR_BUILD_R2.rc=0`.
- Editor SHA256: `95e1c7099dad686a2a3b1cca61e909fbaa9325836cd375280e6e0db8dc06296c`.
- RenderingDevice: rc 0, 12/12 cases, 214/214 assertions.
- Qualification: rc 0, 102/102 cases, 1896/1896 assertions.
- K03: rc 0, 44/44 cases, 1153/1153 assertions.
- R01: rc 0, 16/16 cases, 293/293 assertions.

The logs and rc files are included next to this receipt. No test or fallback owner
was added. No ASAN run is claimed for this package.

## Current pair boundary

The fixed editor was run with the explicit ANGE DSO
`38df3a53ee04aa6d977b313853a2bb3bd7efafe207a960920f80644535a4fcab`. VFX,
no-VFX, and invalid-VFX fixtures each returned rc 2 with the same first error:

```
[ANGE] guarded scene initialization refused stage=install_main_storage code=13 field=host.storage_owner_transport owner=E05 expected=1 actual=0
[ANGE] guarded SceneReady materialization refused stage=install_main_storage
U01_EXTENSION=1
```

The E05 owner transport still prevents world admission; no native operation,
completion, readback, visible output, Ready, or GPU claim is made. The runtime probe
is regression evidence only and does not validate the imported texture path.

All pre-existing unrelated dirty/deleted/untracked files were excluded from the
commit; see `EXCLUDED_DIRTY_BASELINE.txt`.
