# Shader/pipeline retirement audit — 2026-10-30

## Conclusion

No additional safe Godot production change was identified in the current shader/pipeline retirement path. The source already keeps the shader owner while pipeline retirement is Unknown: `qualification_retire_step()` checks pipeline identity/possible state before shader enqueue, and `_free_internal()` plus `_begin_frame()` refuse shader disposal while `shader_retire_ready_nonblocking()` is false. A current Unknown report cannot publish Returned, and a missing/empty pipeline cell is treated as identity failure. The existing Metal implementation also checks both render shader libraries and compute kernel readiness.

No completion, owner, fallback, or Ready authority was added. This package is an audit receipt; no source diff is included.

## Source controls reviewed

- `core/extension/gdextension_qualification.cpp`: shader identity/readiness gate and report reducer preserve Unknown/error precedence.
- `servers/rendering/rendering_device.cpp`: shader `_free_internal()` readiness gate, frame nonblocking checks, and pipeline-before-shader disposal order.
- `drivers/metal/rendering_device_driver_metal.cpp`: compute kernel and render vertex+fragment library readiness.
- Existing tests cover pipeline identity failure, unresolved possible pipeline, non-error Unknown with dependent blocker, and shader retention before physical free.

## Build and focused regression

The current editor/DSO pair was based on Godot HEAD `62d59813959b3bc74b488ccae194306ffaa57a14` and ANGE HEAD `8e2f7d282e96e406b5311b23db74c16328dcc4cf`.

Editor SHA-256: `ea64062cec33e44a660c49e7ed168e3c01252c5becbad023ccad8118da08fdc3`.
DSO SHA-256: `38df3a53ee04aa6d977b313853a2bb3bd7efafe207a960920f80644535a4fcab`.

Focused tests, all rc 0:

| Selector | Cases | Assertions |
| --- | ---: | ---: |
| `*[RenderingDevice]*` | 12 | 214 |
| `*[Qualification]*` | 102 | 1,896 |
| `*[K03]*` | 44 | 1,153 |
| `*[R01]*` | 16 | 293 |
| `*unknown pipeline preserves its original shader*` | 1 | 30 |
| `*pipeline identity failure is not masked by shader pending*` | 1 | 8 |
| `*non-error Unknown remains well-formed*` | 1 | 74 |

The focused tests show no shader enqueue/free and no Returned state while pipeline identity/retirement is unresolved.

## Runtime boundary

Using a temporary fixture explicitly bound to the DSO above, VFX/no-VFX/invalid-VFX each returned rc 2 after Metal 4.0 startup. All stopped at the same upstream owner boundary:

```text
[ANGE] guarded scene initialization refused stage=install_main_storage code=13 field=host.storage_owner_transport owner=E05 expected=1 actual=0
[ANGE] guarded SceneReady materialization refused stage=install_main_storage
U01_EXTENSION=1
```

No world admission, native submit/completion/readback, Ready, G1, or GPU claim is made. E05 owner transport remains unavailable outside the Godot-side audit scope.
