# Reinterpretation buffer failure propagation — 2026-10-30

`_texture_create_reinterpret_buffer()` previously returned void and unconditionally accounted/finalized a driver buffer after `buffer_create()`. A backend allocation failure left an invalid buffer handle, called allocation-size accounting on it, and still allowed the shared texture create path to continue. The helper now returns `bool`, checks its fallback/input, returns false on allocation failure, and publishes buffer/tracker state only after a valid handle. Existing callers fail closed and clean the temporary fallback/view; a newly-created empty source fallback is removed on the shared alias failure. No owner, sampler, fallback authority, or synthetic resource is introduced.

The same shared/slice branches also now clean the regular view if independent fallback texture creation fails.

Build rc 0:

```text
/usr/bin/python3 /Users/macbookair/.local/bin/scons -Q -j4 platform=macos target=editor dev_build=yes tests=yes builtin_glslang=yes
```

Focused rebuilt-editor regressions, all rc 0:

| Selector | Cases | Assertions |
| --- | ---: | ---: |
| `*[RenderingDevice]*` | 12 | 214 |
| `*[Qualification]*` | 102 | 1,896 |
| `*[K03]*` | 44 | 1,153 |
| `*[R01]*` | 16 | 293 |

Editor SHA-256: `1b1515c4283fbb5c9e9eb66323abf1ad96a37010d0d44920da6e6985403560b5` (current binary was rebuilt before the subsequent commit). DSO SHA-256: `38df3a53ee04aa6d977b313853a2bb3bd7efafe207a960920f80644535a4fcab`.

The explicit current DSO Metal pair (`runtime-vfx`, `runtime-no-vfx`, `runtime-invalid-vfx`) returned rc 2 for each control and stopped at the same first boundary:

```text
[ANGE] guarded scene initialization refused stage=install_main_storage code=13 field=host.storage_owner_transport owner=E05 expected=1 actual=0
[ANGE] guarded SceneReady materialization refused stage=install_main_storage
U01_EXTENSION=1
```

No world admission, native submit/completion/readback, Ready, G1, or GPU claim is made. The current pair remains blocked on upstream E05 owner transport.
