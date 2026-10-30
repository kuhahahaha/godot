# Texture initial-data failure cleanup — 2026-10-30

## Production repair

`RenderingDevice::_texture_create_internal()` previously returned a null RID when one initial-data layer failed, while leaving the newly created texture owner and driver texture behind. That was a real local failure-propagation leak. The repair retains unresolved `ERR_BUSY` ownership for the existing fence/shutdown path (an issued worker cannot be freed early), while all other initialization errors queue the failed texture for normal frame retirement. Qualified creates detach the unpublished physical cell before cleanup; the caller only releases the reservation when no owner remains. No completion, owner, or Ready state is fabricated.

## Build and regression

Build command rc 0:

```text
/usr/bin/python3 /Users/macbookair/.local/bin/scons -Q -j4 platform=macos target=editor dev_build=yes tests=yes builtin_glslang=yes
```

Editor SHA-256: `ea64062cec33e44a660c49e7ed168e3c01252c5becbad023ccad8118da08fdc3`.

Focused rebuilt-editor tests, all rc 0:

| Selector | Cases | Assertions |
| --- | ---: | ---: |
| `*[RenderingDevice]*` | 12 | 214 |
| `*[Qualification]*` | 102 | 1,896 |
| `*[K03]*` | 44 | 1,153 |
| `*[R01]*` | 16 | 293 |

The K03 unresolved-completion control remains green and continues to require its failed destination to remain owned until safe shutdown; this contract is preserved.

## Current editor/DSO pair

- Godot source HEAD before this package: `d29e803da6c841f4f3233d7e31c1d3911bbcbcee`.
- ANGE source HEAD: `8e2f7d282e96e406b5311b23db74c16328dcc4cf`.
- Explicit DSO SHA-256: `38df3a53ee04aa6d977b313853a2bb3bd7efafe207a960920f80644535a4fcab`.

With a temporary fixture explicitly bound to that DSO, `runtime-vfx`, `runtime-no-vfx`, and `runtime-invalid-vfx` each returned rc 2 after Metal 4.0 startup. First boundary remained:

```text
[ANGE] guarded scene initialization refused stage=install_main_storage code=13 field=host.storage_owner_transport owner=E05 expected=1 actual=0
[ANGE] guarded SceneReady materialization refused stage=install_main_storage
U01_EXTENSION=1
```

No world admission, native submit/completion/readback, Ready, G1, or GPU claim is made. The upstream E05 owner transport remains unavailable.
