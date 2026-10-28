# U01/VFX current-pair runtime result — 2026-10-30

The fixed three-control run used the current Godot editor executable and current ANGE Debug DSO pinned in `RECEIPT.json`. All controls reached non-headless Metal startup and loaded the extension library, then stopped at the same guarded owner boundary:

```text
[ANGE] guarded scene initialization refused stage=install_main_storage code=13 field=host.storage_owner_transport owner=E05 expected=1 actual=0
```

Observed return code is `2` for `runtime-vfx`, `runtime-no-vfx`, and `runtime-invalid-vfx`. The invalid-VFX source rejection was not reached because the owner-unavailable boundary precedes world admission. No Vulkan/driver error or crash marker occurred.

This package is runtime-boundary evidence only. It does not claim world admission, VFX parsing, native/GPU execution, completion/readback, visible output, PCK, Ready, or G1.
