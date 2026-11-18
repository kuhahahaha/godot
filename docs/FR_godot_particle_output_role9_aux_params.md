# FR: Godot External Particle Output Role 9 (`GDEXTENSION_OUTPUT_ROLE_AUX_PARAMS_V1`) & `Recipe 2` Soft/Refraction/`depth_test_disabled` Shader Plumbing

- **Paired Implementation Plan**: [FR_godot_particle_output_role9_aux_params_PLAN](FR_godot_particle_output_role9_aux_params_PLAN.md)
- **Suite Overview**: [FR_godot_particle_ue_parity_OVERVIEW](FR_godot_particle_ue_parity_OVERVIEW.md)
- **Planning Status**: `ready` | **Implementation Status**: `not-started` | **Engine Tests**: `not-run`
- **Counterpart Cross-Repo Producer Spec**: `ANGE_agy_core` `FR-MAT-02` (`FR_particle_mat_dynamic_params_soft_distortion.md`, tests `M01..M22`)

---

## 1. Problem & Current Fork Behavior

Per-particle dynamic material modulation (`material_params_0: vec4`, `material_params_1: vec4` — driving soft-particle depth fade, dissolve/erosion, UV phase, screen-refraction pinch/chromatic split, and wall-penetrating UI/icon particles) requires uniform `.gdshader` `CUSTOM0` and `CUSTOM1` reads across all particle geometry profiles (`Sprite`, `SubUV`, `Mesh` under `INSTANCE_192`, and `Ribbon` under `INDEXED_144`) with zero CPU readbacks and zero VRAM overhead on emitters that omit auxiliary parameters.

Source verification of `godot_ANGE` at `ac2f93dabdca8cc0162294b85f5122125fa5c43e` establishes six exact architectural facts:

1. **`INDEXED_144` Already Transports `CUSTOM0` & `CUSTOM1` with Zero Changes**:
   - `Float144Vertex` ([particles_ribbon_rd.h](../servers/rendering/renderer_rd/storage_rd/particles_ribbon_rd.h:72)) already carries `custom0[4]` (`words[20..23]`, offset `80B`) and `aux_params[4]` (`words[24..27]`, offset `96B`), bound at vertex input `location = 6` and `location = 7` in `MeshStorage::_mesh_surface_external_vertex_attributes` ([mesh_storage.cpp](../servers/rendering/renderer_rd/storage_rd/mesh_storage.cpp:768)) and validated as finite normal/zero floats in [particles_external_guard_indexed144.glsl](../servers/rendering/renderer_rd/shaders/particles_external_guard_indexed144.glsl:277).
2. **`INSTANCE_192` Cannot Widen `Instance192Record` (`192B`) or Use Mesh Vertex `location = 6/7`**:
   - `Instance192Record` ([particles_mesh_light_sort_rd.h](../servers/rendering/renderer_rd/storage_rd/particles_mesh_light_sort_rd.h:54)) is frozen at `192B` (`words[40..43]` already occupied by `INSTANCE_CUSTOM` `uv_rect`).
   - Static meshes qualified for `INSTANCE_192` forbid `RSE::ARRAY_FORMAT_CUSTOM0..3` ([gdextension_production_material_qualification.cpp](../core/extension/gdextension_production_material_qualification.cpp:782)), and `SceneShaderForwardClustered` ([scene_shader_forward_clustered.cpp](../servers/rendering/renderer_rd/forward_clustered/scene_shader_forward_clustered.cpp:1183)) / `SceneShaderForwardMobile` ([scene_shader_forward_mobile.cpp](../servers/rendering/renderer_rd/forward_mobile/scene_shader_forward_mobile.cpp:1149)) currently rename `CUSTOM0`/`CUSTOM1` directly to `in vec4 custom0_attrib` / `custom1_attrib` ([scene_forward_clustered.glsl](../servers/rendering/renderer_rd/shaders/forward_clustered/scene_forward_clustered.glsl:49), [scene_forward_mobile.glsl](../servers/rendering/renderer_rd/shaders/forward_mobile/scene_forward_mobile.glsl:61)).
3. **Companion Role (`GDEXTENSION_OUTPUT_ROLE_AUX_PARAMS_V1 = 9`) Across `PrepareV3`, `AcquireV2`, & `WithoutPrevious` Fallback**:
   - `production_prepare_output_plan_internal` ([gdextension_production_output.cpp](../core/extension/gdextension_production_output.cpp:3104)) rejects `output_role_count != 3` when `plan.acquire.roles` is non-empty, so 4-role `INSTANCE_192` (`{1, 2, 7, 9}`) enters via `plan.acquire.roles = {}` (`all_roles_inactive`) + `production_prepare_output_v3` + `production_output_acquire_v2`.
   - `production_history_fallback_validate_locked` ([gdextension_production_output.cpp](../core/extension/gdextension_production_output.cpp:2166)) and `production_history_choose_locked` ([gdextension_production_output.cpp](../core/extension/gdextension_production_output.cpp:2297)) must strip `role 2` (`ALIGNED_PREVIOUS_RECORDS_V1`) when `WithoutPrevious` fallback is staged/chosen on `output_prepare_version == 3` (`role_count == 4`).
4. **OOB-Safe Guard Prefix Preparation (`_external_guard_prepare_owned_prefix`)**:
   - In [particles_storage_external.cpp](../servers/rendering/renderer_rd/storage_rd/particles_storage_external.cpp:286), `imported` must be upper-bounded to `<= Guard::SOURCE_INSTANCE_GATE` (`11`) and `Guard::INSTANCE_AUX_PARAMS` (`12`) must map explicitly to `ExternalOutputStorage::INSTANCE_AUX_PARAMS` (`9`) to avoid OOB reads of `p_source_reads[5]` and `slot.storage->resources[11]`.
5. **Settled `32B` Zero Sentinel SSBO (`draw_tracker != nullptr`)**:
   - `RenderingDevice::_production_output_owned_uniform_set_create_locked` ([rendering_device_immutable_hold.cpp](../servers/rendering/rendering_device_immutable_hold.cpp:2509)) requires `buffer->draw_tracker != nullptr && buffer->transfer_worker_index < 0` (`:2511`), so `ParticlesStorage::external_aux_zero_sentinel_buffer` (`32B`) must be settled via `_check_transfer_worker_buffer` + `_buffer_make_mutable` ([rendering_device_immutable_hold.cpp](../servers/rendering/rendering_device_immutable_hold.cpp:3076)).
6. **`Recipe 2` Qualification for `Set 2, binding = 2` & Source-Verified `depth_test_disabled` vs `depth_test_inverted` Partition**:
   - `NativeMaterialRecipeRoster::_join_bindings` ([material_storage.cpp](../servers/rendering/renderer_rd/storage_rd/material_storage.cpp:3611)) must admit `set == 2 && reflected.binding == 2` (`Role::AUX_PARAMS`).
   - Source inspection of `ShaderData::native_material_recipe_rejections` in **both** [scene_shader_forward_clustered.cpp](../servers/rendering/renderer_rd/forward_clustered/scene_shader_forward_clustered.cpp:278) (`:278-280`) and [scene_shader_forward_mobile.cpp](../servers/rendering/renderer_rd/forward_mobile/scene_shader_forward_mobile.cpp:293) (`:293-295`) confirms:
     - `render_mode depth_test_disabled;` (`depth_test = DEPTH_TEST_DISABLED` at `:215`) sets **`MATERIAL_RECIPE_DEPTH_MODE = 128u`** (`GDEXTENSION_MATERIAL_QUALIFICATION_SOURCE_GATE_DEPTH_MODE_V1 = 128`, **inside** `PARTICLE_SOURCE_GATE_ALLOWED_MASK_V1 = 412 = 4 | 8 | 16 | 128 | 256`) and sets `uses_alpha_pass() == true` ([scene_shader_forward_clustered.h](../servers/rendering/renderer_rd/forward_clustered/scene_shader_forward_clustered.h:357)), so wall-penetrating UI/icon particles (`REVERSE.md #42 N888_ICON_OCCLUSION`) pass `Recipe 2` cleanly!
     - `render_mode depth_test_inverted;` (`depth_test = DEPTH_TEST_ENABLED_INVERTED` at `:217`) sets **`MATERIAL_RECIPE_DEPTH_WRITE = 32u`** (`SOURCE_GATE_DEPTH_WRITE_V1 = 32`, **outside** `412`) AND is explicitly rejected in `_plan_native_material_recipe` ([render_forward_clustered.cpp](../servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.cpp:6221), [render_forward_mobile.cpp](../servers/rendering/renderer_rd/forward_mobile/render_forward_mobile.cpp:4655)) and `warm_native_material_instance192_shader` (`:6374` / `:4807`) with `ERR_UNAVAILABLE`.

---

## 2. User / API Surface

- **GDExtension C Interface (`core/extension/gdextension_interface.json`)**:
  - Append `GDEXTENSION_OUTPUT_ROLE_AUX_PARAMS_V1 = 9` to `GDExtensionOutputConstantsV1` (strictly preserving values `0..8`).
  - `GDExtensionOutputPrepareV3`: for `INSTANCE_192` (`roles[0].role == GDEXTENSION_OUTPUT_ROLE_CURRENT_RECORDS_V1`), accept existing 2-role `{1, 7}` and 3-role `{1, 2, 7}` shapes unchanged (`role_count = 3`), plus 3-role `{1, 7, 9}` and 4-role `{1, 2, 7, 9}`, normalized to `role_count = 4` (`canonical_roles[0..3] = {1, 2, 7, 9}`).
  - `GDExtensionOutputAcquireResultV2` & `WithoutPrevious` Fallback: project 4-role `INSTANCE_192` slots (`writables` up to 4 on primary `{1, 2, 7, 9}`, or 3 `{1, 7, 9}` when `WithoutPrevious` fallback clears `role 2` at runtime; `stride = 32u` for `role 9`). Legacy V1 `production_output_acquire` (`writables[3]`) refuses `role_count != 3` with `ERR_INVALID_DATA`.
- **GDShader Spatial Built-in Surface & Digest Behavior**:
  - Spatial shaders compiled for `INSTANCE_192` read `CUSTOM0` (`vec4`, `words[0..3]`) and `CUSTOM1` (`vec4`, `words[4..7]`) in `vertex()` without requiring `ARRAY_FORMAT_CUSTOM0/1` on the mesh surface.
  - Action-7 `GDExtensionMaterialQualificationResultMetadataV1` (`oracle.recipe`, `oracle.verifier`, `oracle.vector_table`) digests remain **bit-identical** for existing non-`CUSTOM0/1` shaders (`Set 2` keeps only `bindings 0..1`), while `ShaderRD`'s internal shader-cache `base_sha256` updates as expected when `scene_forward_*.glsl` is edited.
- **ClassDB / `.xml` Surface**: **N/A** (no public GDScript/ClassDB additions).

---

## 3. Behavioral Requirements & Invariants

1. **Canonical `32B` `InstanceAuxParamsRecord32` Wire Layout (`role 9`)**:
   - `words[0..3]` (`0..15B`): `material_params_0` (`f32[4]`) -> `CUSTOM0.xyzw`.
   - `words[4..7]` (`16..31B`): `material_params_1` (`f32[4]`) -> `CUSTOM1.xyzw`.
   - Active byte extent: `capacity == 0 ? 0u : (aux_enabled ? capacity * 32u : 0u)`. Non-multiples of `32` or `aux_bytes / 32 != curr_bytes / 192` fail closed in `PrepareV3` / `AcquireV2` with `ERR_INVALID_DATA`. At `capacity == 0`, both `_external_guard_copy_history_locked` and `particles_external_resolve` verify `INSTANCE_AUX_PARAMS` is absent (`!rid.is_valid() && !bytes`).
2. **Guard Validation (`ParticlesExternalGuard` + `particles_external_guard.glsl` + CPU Harness)**:
   - Add `Binding::INSTANCE_AUX_PARAMS = 12` (`BINDING_COUNT = 13`) and 6 `AUX` shader modes (`INIT_AUX`, `VALIDATE_AUX`, `FINALIZE_AUX`, `INIT_NO_MOTION_AUX`, `VALIDATE_NO_MOTION_AUX`, `FINALIZE_NO_MOTION_AUX`); expand `ExternalOutputOwner::GUARD_MODE_CAPACITY` from `8` to `14` ([external_output_owner.h](../servers/rendering/renderer_rd/storage_rd/external_output_owner.h:62)).
   - Gate `layout(set = 0, binding = 12, std430) restrict readonly buffer InstanceAuxParams` under `#if !defined(MODE_EMPTY) && defined(AUX_PARAMS_ENABLED)`. When `options[1] == 0u`, `binding 12` is omitted (`{RID(), 0}`), preserving `12` uniforms (`10` at `capacity == 0`). When `options[1] == 1u`, `MODE_VALIDATE` checks all 8 float words (`ordinal * 8u + [0..7]`) via `finite_normal_or_zero()`, raising `NUMERIC_FAULT = 16u` and zeroing all `ProtectedCommands` on `NaN`, `Inf`, or subnormal.
   - `ParticlesStorage::_external_guard_prepare_owned_prefix` bounds `imported` strictly to `binding == Guard::SOURCE_COUNT || (binding >= Guard::SOURCE_COUNT_VALIDITY && binding <= Guard::SOURCE_INSTANCE_GATE)` and maps `Guard::INSTANCE_AUX_PARAMS (12)` explicitly to `ExternalOutputStorage::INSTANCE_AUX_PARAMS (9)`.
3. **Settled `32B` Zero Sentinel & Set-2 Bounds Safety**:
   - On main-RD `ParticlesStorage` init, allocate `external_aux_zero_sentinel_buffer` (`32` zero bytes) and settle it via `RenderingDevice::_production_prepare_static_sentinel_buffer(external_aux_zero_sentinel_buffer, 32)` (`_check_transfer_worker_buffer` + `_buffer_make_mutable`, guaranteeing `draw_tracker != nullptr && transfer_worker_index < 0`). Guard `if (external_aux_zero_sentinel_buffer.is_valid())` in `~ParticlesStorage()` so test-only `ParticlesStorage(ExternalLinkedOwnerTest *)` instances ([particles_storage.cpp](../servers/rendering/renderer_rd/storage_rd/particles_storage.cpp:56)) never free a null RID.
   - In `instance192_inc.glsl`, under `#if defined(CUSTOM0_USED) || defined(CUSTOM1_USED)`, `instance192_read_aux(uint index, out vec4 params0, out vec4 params1)` checks `uint pair_count = uint(instance192_aux_params.data.length()) >> 1u;` and returns `(vec4(0.0), vec4(0.0))` whenever `index >= pair_count`, preventing OOB reads on the `32B` (`pair_count == 1`) zero sentinel for any `gl_InstanceIndex >= 1`.
4. **Vertex Attribute Isolation & `MOTION_VECTORS` Dual `vertex_shader()` Semantics**:
   - Guard `layout(location = 6) in vec4 custom0_attrib;` and `layout(location = 7) in vec4 custom1_attrib;` in `scene_forward_clustered.glsl` and `scene_forward_mobile.glsl` with `&& !defined(USE_INSTANCE192_OUTPUT)`, and rename `CUSTOM0`/`CUSTOM1` to local `read_custom0`/`read_custom1`.
   - Under `MOTION_VECTORS` (`instance192_previous_record == true` and `false`), both vertex invocations intentionally read current-frame `instance_aux_params` (matching `Instance192Record.color`; `Recipe 2` enforces `VERTEX_WRITE == 0` and `POSITION` write `== 0`).
5. **Lifecycle, `WithoutPrevious` Fallback, Retirement, `Recipe 2` Qualification, & `depth_test_disabled` Admission**:
   - `ExternalOutputStorage::INSTANCE_AUX_PARAMS` (`index 9`) participates in `allocate_instance192_owned`, `production_output_records_written_locked`, `WithoutPrevious` fallback (`roles {1, 7, 9}`), `request_retirement`, `observe`, and `destroy_if_returned`.
   - `Recipe 2` (`allowed_gate_mask = 412 = ALPHA (4) | DISCARD (8) | TIME (16) | DEPTH_MODE (128) | FRAMEBUFFER_READ (256)`) qualifies spatial particle shaders reading `CUSTOM0`, `CUSTOM1`, `hint_depth_texture`, `hint_screen_texture`, and **`render_mode depth_test_disabled;`** (`MATERIAL_RECIPE_DEPTH_MODE = 128`), while strictly rejecting `VERTEX` writes (`1`, handled separately by bounded WPO `Recipe 3` in [FR_godot_particle_wpo_recipe3_and_bounds](FR_godot_particle_wpo_recipe3_and_bounds.md)), **`depth_test_inverted`** / `DEPTH` writes (`MATERIAL_RECIPE_DEPTH_WRITE = 32`), `POSITION` / `Z_CLIP_SCALE` writes (`2` / `64`), and `STENCIL` (`512`).

---

## 4. Boundaries, Non-Goals & Backend Support

- **Non-Goals**: Authoring `.gdshader` / `.material` preset files (`vfx_soft_smoke*`, `vfx_refract_distortion*`) and `.vfx` `material_params_0/1` bindings live in `ANGE_agy_core` (`FR_particle_mat_dynamic_params_soft_distortion.md`). Vertex displacement (`VERTEX` write) is out of scope for `Recipe 2` and owned by [FR_godot_particle_wpo_recipe3_and_bounds](FR_godot_particle_wpo_recipe3_and_bounds.md) (`Recipe 3`). `CUSTOM2` and `CUSTOM3` remain mesh-only attributes.
- **Renderer Backends**: Supported on `RendererRD` (`RenderForwardClustered` and `RenderForwardMobile`). Compatibility (`GLES3`) and Dummy (`RendererDummy`) refuse external RD output creation and `GDExtensionMaterialQualification` (`get_native_material_recipe_backend() == 0` -> `ERR_UNAVAILABLE` at [gdextension_production_material_qualification.cpp](../core/extension/gdextension_production_material_qualification.cpp:901)) without silent fallback.

---

## 5. Acceptance Criteria & Relationship to Adjacent Capabilities

1. `PrepareV3` + `AcquireV2` + `Guard` + `Seal` succeeds for `{1, 2, 7, 9}`, `{1, 7, 9}`, and `WithoutPrevious` history fallback (`stage_output_plan_with_history_guard_v2` + `stage_output_history_fallback_v2`) on live Metal RD, delivering exact per-instance `CUSTOM0` and `CUSTOM1` values to Clustered and Mobile color/transparent/motion-vector passes.
2. Legacy 2-role `{1, 7}` and 3-role `{1, 2, 7}` `INSTANCE_192` packets allocate `0` bytes for `INSTANCE_AUX_PARAMS`, keep `100%` bit-identical Action-7 oracle/verifier/vector digests for non-`CUSTOM0/1` shaders, and read `(vec4(0.0), vec4(0.0))` via the settled `32B` sentinel when bound to a `CUSTOM0/1` material.
3. Soft-smoke (`hint_depth_texture` + `CUSTOM0/1`), screen-refraction (`hint_screen_texture` + `hint_depth_texture` + `CUSTOM0/1`), and wall-penetrating icon (`render_mode depth_test_disabled;` + `CUSTOM0/1`) shaders pass `Recipe 2` qualification (`COMPLETE_V1 = 2`), while `render_mode depth_test_inverted;` (`DEPTH_WRITE = 32`) and `VERTEX` writes (`VERTEX_WRITE = 1`) are rejected under `Recipe 2`, and `NaN`/subnormal aux words trigger `NUMERIC_FAULT = 16u`.
4. Relationship to adjacent FRs: Sequences after [FR_godot_particle_zero_size_and_ribbon_guard](FR_godot_particle_zero_size_and_ribbon_guard.md) and before [FR_godot_particle_wpo_recipe3_and_bounds](FR_godot_particle_wpo_recipe3_and_bounds.md); independent of [FR_godot_particle_scene_depth_snapshot_v2](FR_godot_particle_scene_depth_snapshot_v2.md).
