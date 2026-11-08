# Factory review: ready-only native pipeline caller

## Scope

Reviewed the live two-file diff at `HEAD 7997baa62e159383d7d997b5b66ef6074e7916dc`:

- `servers/rendering/renderer_rd/forward_clustered/scene_shader_forward_clustered.cpp`
- `servers/rendering/renderer_rd/forward_mobile/scene_shader_forward_mobile.cpp`

No source edits or builds were made.

## Verdict

**Ready / no blocking findings.** The `p_output` path now calls `get_native_shader_if_ready`; the ordinary rendering path still calls the effectful `get_shader_variant` unchanged. On readiness failure, the exact returned `Error` is stored in `p_output->last_error` and the function returns a null `RID`, allowing the surrounding preparation callback to fail and preserve the admitted row's failure state.

## Lock and deadlock review

`get_native_shader_if_ready` takes the renderer singleton mutex and then performs the ShaderRD readiness query. Its existing nested order is singleton mutex → shader-version mutex → RD mutex for the read-only vertex mask query, matching the caller's already existing readiness query. These locks are released when the helper returns. The backend callback runs after the row-selection registry lock block has ended; beginning the preparation scope does not retain that registry lock across the callback. `create_native_pipeline` invokes `_create_pipeline` only after that helper returns, so the external-output/registry locks used by `_create_pipeline` are not nested under `singleton_mutex`. The helper's `RenderingDevice` render-thread guard is also appropriate for this native-only path.

## Exact-key and replay review

Clustered and mobile pass the same version/flags/ubershader/indexed144/instance192 tuple to the ready helper that the old getter used for shader selection. Their callers already perform the same ready query before deriving the vertex format; the second query in `create_native_pipeline` is side-effect free and revalidates the same key immediately before owned pipeline creation. No key fields are changed between the caller query and pipeline creation. On `ERR_BUSY`, `ERR_UNAVAILABLE`, authorization, or other helper failure, the caller records `last_error`, returns null, and `particles_external_prepare_pipeline` records `backend_error`; replay sees the existing selected row and returns its stored terminal result rather than allocating a fresh row.

This static review does not claim runtime contention or GPU validation.

## Error and ordinary-rendering safety

`p_output` is checked before writing `last_error`, so the ordinary path cannot dereference it. A null shader RID still trips the existing guard. The ordinary path retains the original `get_shader_variant` behavior and direct `render_pipeline_create` call. `git diff --check` passes.
