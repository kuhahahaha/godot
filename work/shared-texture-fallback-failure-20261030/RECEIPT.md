# Shared texture fallback failure ordering — 2026-10-30

## Production repair

Renderer source review found two texture sampling/share paths that allocated fallback state before proving that the regular shared view or the independent fallback texture had been created. On either driver failure, `texture_create_shared()` / `texture_create_shared_from_slice()` returned a null RID while leaving an allocated fallback object (and, on some backends, an invalid driver handle) stranded. The failure also reached `texture_get_allocation_size()` with a null fallback handle.

The patch validates the regular shared view immediately in the fallback branch, creates the independent fallback texture into a temporary `TextureID`, validates it, then allocates/publishes `SharedFallback`. This preserves fail-closed sampling admission and prevents empty fallback state from escaping a failed create. No owner, sampler, source texture, or synthetic fallback is inferred.

## Build and regressions

Build rc 0:

```text
/usr/bin/python3 /Users/macbookair/.local/bin/scons -Q -j4 platform=macos target=editor dev_build=yes tests=yes builtin_glslang=yes
```

Editor SHA-256: `6572725f75e93a6634819c552e1accbc064abf8be70947279e911f965deafc05`. The only linker diagnostic is the existing MoltenVK macOS 12 object versus macOS 11 target warning.

Focused tests (rebuilt editor, all rc 0):

| Selector | Cases | Assertions |
| --- | ---: | ---: |
| `*[Qualification]*` | 102 | 1,896 |
| `*[K03]*` | 44 | 1,153 |
| `*[RenderingDevice]*` | 12 | 214 |
| `*[R01]*` | 16 | 293 |

The RenderingDevice tests include texture/sampling validation and ownership guards. Existing tests do not inject a backend null shared-view/fallback allocation after a valid source; the new checks are source-local fail-closed guards.

## Current editor/DSO pair

- Godot source HEAD before this package: `c2a2a63d09dc1dbebe0ae049fc0d9e1bf5b57462`.
- ANGE source HEAD: `8e2f7d282e96e406b5311b23db74c16328dcc4cf`.
- Explicit DSO: `/Users/macbookair/Documents/Work/ANGE/work/godot-native-chain-long-20261030/libange.macos.template_debug.arm64.dylib`.
- DSO SHA-256: `38df3a53ee04aa6d977b313853a2bb3bd7efafe207a960920f80644535a4fcab`.

Using a disposable fixture whose `.gdextension` points at that exact DSO, VFX/no-VFX/invalid-VFX each returned rc 2 after Metal 4.0 startup. The first boundary remains:

```text
[ANGE] guarded scene initialization refused stage=install_main_storage code=13 field=host.storage_owner_transport owner=E05 expected=1 actual=0
[ANGE] guarded SceneReady materialization refused stage=install_main_storage
U01_EXTENSION=1
```

No world admission, texture sampling, native submit/completion/readback, Ready, G1, or GPU claim is made.

## Dirty baseline

Before this package, the Godot worktree already contained unrelated dirty/deleted/untracked P01/docs/input and generated work paths. Only `servers/rendering/rendering_device.cpp` and this package are staged.
