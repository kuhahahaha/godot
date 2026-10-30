# Async texture readback map-failure propagation — 2026-10-30

## Production repair

The asynchronous texture readback flush path dereferenced `driver->buffer_map()` without checking for a null result. A backend staging-map failure could therefore crash the render thread and expose partially copied bytes through the callback. The path now records the map failure, stops copying, and skips the success callback. Staging buffers are still cleared by the normal frame cleanup. No empty/synthetic payload, completion, or readback provenance is published; callback delivery remains success-only, matching async buffer readback.

## Build and focused regressions

Build rc 0:

```text
/usr/bin/python3 /Users/macbookair/.local/bin/scons -Q -j4 platform=macos target=editor dev_build=yes tests=yes builtin_glslang=yes
```

Editor SHA-256: `b39543b6c5b8d96e3acabae8b3212710cc60ffc0248fa4f8261eab9491bb1be9`.

Focused tests, all rc 0:

| Selector | Cases | Assertions |
| --- | ---: | ---: |
| `*[RenderingDevice]*` | 12 | 214 |
| `*[Qualification]*` | 102 | 1,896 |
| `*[K03]*` | 44 | 1,153 |
| `*[R01]*` | 16 | 293 |

## Current pair

Godot HEAD: `c20d73f2a76fde39e63209cd8cd3e9af7ea34c0d`.
ANGE HEAD: `8e2f7d282e96e406b5311b23db74c16328dcc4cf`.
DSO SHA-256: `38df3a53ee04aa6d977b313853a2bb3bd7efafe207a960920f80644535a4fcab`.

With the explicit current DSO, VFX/no-VFX/invalid-VFX controls each returned rc 2 after Metal 4.0 startup and stopped at:

```text
[ANGE] guarded scene initialization refused stage=install_main_storage code=13 field=host.storage_owner_transport owner=E05 expected=1 actual=0
[ANGE] guarded SceneReady materialization refused stage=install_main_storage
U01_EXTENSION=1
```

No world admission, native submit/completion/readback, Ready, G1, or GPU claim is made. E05 owner transport remains upstream and fail-closed.
