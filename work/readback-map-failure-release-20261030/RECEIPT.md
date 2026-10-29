# Native readback temporary-buffer cleanup — 2026-10-30

## Production repair

Source review found a local resource-release hole in both synchronous readback paths. `RenderingDevice::buffer_get_data()` and `RenderingDevice::texture_get_data()` allocate a temporary CPU transfer buffer, wait for all frames, and then return through `ERR_FAIL_NULL_V` when `driver->buffer_map()` returns null. That early return leaked the temporary driver allocation after GPU work had already been stalled.

The patch replaces both early returns with explicit fail-closed branches that call `driver->buffer_free(tmp_buffer)` before returning an empty result. No owner, completion event, readback bytes, or success state is fabricated. A failed map remains a failed readback. The existing wait-error path still queues the temporary buffer for deferred disposal.

## Build and focused regressions

Build command returned rc 0:

```text
/usr/bin/python3 /Users/macbookair/.local/bin/scons -Q -j4 platform=macos target=editor dev_build=yes tests=yes builtin_glslang=yes
```

Editor SHA-256: `270baaac82e2a8e517b30c31b53a32c6b9e3a88b5193aed7c802e83245d2c491`. The linker emitted only the existing MoltenVK deployment-target warning (object macOS 12, target macOS 11).

Rebuilt editor focused tests, all rc 0:

| Selector | Cases | Assertions |
| --- | ---: | ---: |
| `*[CallQueue]*` | 6 | 92 |
| `*[Qualification]*` | 102 | 1,896 |
| `*[K03]*` | 44 | 1,153 |
| `*[R01]*` | 16 | 293 |

K03/R01 include existing map-failure and readback-fence negative controls. No fabricated GPU readback is claimed.

## Current editor/DSO pair

- Godot source HEAD before this package: `7aed2b4845c1b8c0c418f4edc50bde354314e62a`.
- ANGE source HEAD: `8e2f7d282e96e406b5311b23db74c16328dcc4cf`.
- Explicit DSO: `/Users/macbookair/Documents/Work/ANGE/work/godot-native-chain-long-20261030/libange.macos.template_debug.arm64.dylib`.
- DSO SHA-256: `38df3a53ee04aa6d977b313853a2bb3bd7efafe207a960920f80644535a4fcab`.

Using a disposable fixture copy whose `.gdextension` points at that exact DSO, `runtime-vfx`, `runtime-no-vfx`, and `runtime-invalid-vfx` all returned rc 2. Every run reached Metal 4.0 / Apple M3 Pro startup and stopped at the same first external boundary:

```text
[ANGE] guarded scene initialization refused stage=install_main_storage code=13 field=host.storage_owner_transport owner=E05 expected=1 actual=0
[ANGE] guarded SceneReady materialization refused stage=install_main_storage
U01_EXTENSION=1
```

No world admission, native submit, completion, readback, Ready, G1, or GPU claim is made.

## Boundary and dirty baseline

The E05 owner transport declaration remains unavailable upstream; Godot cannot safely infer it from fixture metadata or install a fallback owner. Before this package, the working tree already contained 8 modified, 120 deleted, and 206 untracked paths, including unrelated P01/docs/input and generated work artifacts. Only `servers/rendering/rendering_device.cpp` and this package were staged.
