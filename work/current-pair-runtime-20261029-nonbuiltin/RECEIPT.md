# Current nonbuiltin fixed-pair U01 runtime receipt — 2026-10-29

This is a bounded, non-headless Metal probe after the explicit macOS
`builtin_glslang=no` executable/shared builds. It records the current pair and
the first owner boundary; it is not a VFX, GPU, completion, readback, Ready or
G1 success receipt.

## Pair and command

- Godot executable: `bin/godot.macos.editor.dev.arm64`
- Godot SHA256: `b747ac4dbc93e7d2a8ad00496fecfb83f709acf9ed1d0b924c16a82d1abfc476`
- ANGE DSO: `/Users/macbookair/Documents/Work/ANGE/bin/libange.macos.template_debug.arm64.dylib`
- ANGE DSO SHA256: `cde5edb63d7102ac449d7d9e1217871ed890b60e3664b7dfe9a793bf54469e1a`
- Renderer: non-headless macOS Metal 4.0, Forward Mobile, Apple M3 Pro, safe render thread
- Command shape: `godot --path <fixture> --display-driver macos --rendering-driver metal --rendering-method mobile --render-thread safe --script res://probe.gd`

## Results

All three controls stop before world admission because the DSO's guarded
initializer cannot install the E05 storage-owner transport:

```text
[ANGE] guarded scene initialization refused stage=install_main_storage code=13 field=host.storage_owner_transport owner=E05 expected=1 actual=0
[ANGE] guarded SceneReady materialization refused stage=install_main_storage
U01_EXTENSION=1
```

| Fixture | Return code | Observed boundary |
|---|---:|---|
| `runtime-vfx` | 2 | extension load failed (`U01_EXTENSION=1`); no VFX/world execution |
| `runtime-no-vfx` | 2 | same E05 owner boundary; no control-world admission |
| `runtime-invalid-vfx` | 2 | same E05 owner boundary precedes invalid-source control |

Raw logs are `runtime-vfx.log`, `runtime-no-vfx.log`, and
`runtime-invalid-vfx.log`. Because the probe exits immediately after a failed
extension load, there is no `U01_ENTER_WORLD`, `U01_UNLOAD`, operation,
completion, readback, Ready or G1 result to count.

## Disposition

The first current-pair failure is the precise external owner seam
`host.storage_owner_transport/E05` during guarded scene initialization. This
receipt supersedes no prior positive runtime claim; it only establishes that
the new pair does not reach world admission. No fallback, synthetic owner,
ObjectID/RID inference or G1 weakening was added. Other native queue work
continues independently.
