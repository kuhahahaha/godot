# Metal shader retirement readiness repair — 2026-10-30

## Scope

This package addresses a local Godot/Metal retirement defect found while auditing the `Retain shader while pipeline retirement is unknown` boundary. The qualification reducer already intentionally retains a shader while a dependent pipeline is `Unknown`; the existing focused tests cover that fail-closed behavior. The separate local defect was in `RenderingDeviceDriverMetal::shader_retire_ready_nonblocking()`: it only accepted `MDComputeShader`, so every `MDRenderShader` reported not-ready forever even when both vertex and fragment libraries had completed. That made a render shader with a qualification return remain queued and repeatedly produce `ERR_BUSY` at the frame/free gates.

The production fix keeps unknown or malformed identities fail-closed and adds the missing render path: both `vert` and `frag` must exist and each library must report `retire_ready_nonblocking()`. Compute behavior is unchanged. No synthetic return, owner inference, error suppression, or fallback was added.

## Source change

- `drivers/metal/rendering_device_driver_metal.cpp:1575` now dispatches to `MDComputeShader` or `MDRenderShader`; render readiness requires both libraries.
- Baseline Godot HEAD before this package: `3ea5b8d13e26adb391ba4e92d5a591148640c49b`.
- ANGE DSO paired for runtime: `/Users/macbookair/Documents/Work/ANGE/work/godot-native-chain-long-20261030/libange.macos.template_debug.arm64.dylib`, SHA-256 `38df3a53ee04aa6d977b313853a2bb3bd7efafe207a960920f80644535a4fcab`, ANGE source HEAD `8e2f7d282e96e406b5311b23db74c16328dcc4cf`.

## Verification

Editor build (rc 0):

```text
/usr/bin/python3 /Users/macbookair/.local/bin/scons -Q -j4 platform=macos target=editor dev_build=yes tests=yes builtin_glslang=yes
```

Editor SHA-256: `e4b692205d8bbac62c78937d0f873b2161f73c939f5974df86f7ed9daabaf16b`. The linker emitted only the pre-existing MoltenVK deployment-target warning (object macOS 12, editor target macOS 11).

Focused doctest commands all returned rc 0 with the rebuilt editor:

| Selector | Cases | Assertions |
| --- | ---: | ---: |
| `*[Qualification]*` | 102 | 1,896 |
| `*[K03]*` | 44 | 1,153 |
| `*[P01]*` | 0 | 0 |
| `*[CallQueue]*` | 6 | 92 |
| `*unknown pipeline preserves its original shader*` | 1 | 30 |
| `*pipeline identity failure is not masked by shader pending*` | 1 | 8 |
| `*non-error Unknown remains well-formed*` | 1 | 74 |
| `*service poll error precedes new native shader retirement*` | 1 | 12 |

The P01 selector is retained as a negative control because this source tree has no tests carrying that label; it passed with zero selected cases. Qualification logs contain expected `ERR_UNAUTHORIZED` guard diagnostics while the doctest summary is green.

Fixed editor/DSO Metal pair controls used the existing U01 fixture and returned rc 2 for all three controls (`runtime-vfx`, `runtime-no-vfx`, `runtime-invalid-vfx`). Each reached Metal 4.0 startup and loaded the extension, then stopped at the same fail-closed upstream boundary:

```text
[ANGE] guarded scene initialization refused stage=install_main_storage code=13 field=host.storage_owner_transport owner=E05 expected=1 actual=0
[ANGE] guarded SceneReady materialization refused stage=install_main_storage
U01_EXTENSION=1
```

No world admission, VFX parsing, native submit/completion/readback, Ready, G1, or GPU claim is made. The invalid-VFX control is not evidence of parser behavior because E05 precedes it.

## Classification

The literal `Retain shader while pipeline retirement is unknown` behavior is intentional and locally verified by the existing reducer tests above; it must remain fail-closed until the pipeline owner reports a resolvable identity. The repaired defect was a distinct Metal producer/readiness omission for render shaders. The remaining runtime blocker is the ANGE E05 owner transport input, which is external to this Godot-side patch.
