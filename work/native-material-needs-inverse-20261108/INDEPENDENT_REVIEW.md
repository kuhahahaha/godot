# Independent implementation review

Reviewed commit `86a59ee22e` against `AGENTS.md`, `RECEIPT.md`, and `SOURCE_MAP.md`. The review was read-only; no build or live edit was performed.

## Findings

No material implementation defects found in the requested scope.

The clustered and mobile compilers both register independent usage pointers for `INV_MODEL_MATRIX` and `ALPHA_HASH_SCALE`, then derive the inverse requirement as their OR. The existing alpha-clip union is preserved by OR-ing the hash flag into `uses_alpha_clip` after a successful compile. Both source paths clear the borrowed observation before empty-source and compile-failure exits, and only publish it after the version is successfully installed.

`MaterialStorage::material_selection_get_shader_features()` leaves its caller output untouched on every refusal path: it validates the lease owner/device/render thread, material RID and exact generation, selection holds, 3D shader/data, clean serviced state, no next pass, and the compiler/original shader-version association before assigning a local result. The existing selection lease prevents shader/material replacement and recompile while held; material mutations advance generation after release, so an old lease cannot authenticate a later source. Empty source invalidates the observation while retaining the old version RID, and the getter still refuses because the feature-version marker is cleared. Failed compile frees and clears the version, likewise preventing stale reuse.

The native fixture's unissued, released, generation/version, empty-source, and both-backend cases are consistent with these guards. No source evidence justified a material defect report.

## Source evidence

- Compiler observations and invalidation: `servers/rendering/renderer_rd/forward_clustered/scene_shader_forward_clustered.cpp:42–278` and `servers/rendering/renderer_rd/forward_mobile/scene_shader_forward_mobile.cpp:44–291`.
- Both usage-discovery node paths set usage flags independently of define discovery: `servers/rendering/shader_compiler.cpp:915–926,1049–1060`.
- Selection acquisition/release and output assignment: `servers/rendering/renderer_rd/storage_rd/material_storage.cpp:1156–1229`.
- Shader mutation retention gate: `servers/rendering/renderer_rd/storage_rd/material_storage.cpp:1247–1255,2552–2556`; source changes advance each owning material generation at `2627–2634`.
- Replacement/free gates and preparation checks: `servers/rendering/renderer_rd/storage_rd/material_storage.cpp:2522–2525,2826–2829,2850–2857,2993–3010`.
- Lease fields are private, copying is deleted, and destruction requires return: `servers/rendering/renderer_rd/storage_rd/material_storage.h:123–135`.

This is source review of the compiler-observation constituent. Recorded validation was inspected as supplied evidence; no tests were rerun, and full qualification/public transport were outside the review.
