# FR: Godot Bounded Particle Vertex Displacement Qualification (`Recipe 3`) & Per-Surface `SurfaceProof` Guard Margin

- **Paired Implementation Plan**: [FR_godot_particle_wpo_recipe3_and_bounds_PLAN](FR_godot_particle_wpo_recipe3_and_bounds_PLAN.md)
- **Suite Overview**: [FR_godot_particle_ue_parity_OVERVIEW](FR_godot_particle_ue_parity_OVERVIEW.md)
- **Planning Status**: `ready` | **Implementation Status**: `not-started` | **Engine Tests**: `not-run`
- **Counterpart Cross-Repo Producer Spec**: `ANGE_agy_core` `FR-WPO-06` (`FR_particle_wpo_bounded_displacement.md`, tests `W01..W21`)

---

## 1. Problem & Current Fork Behavior

UE Niagara corpus effects such as undulating organic tentacles (`#30 VEFECTS-TENTACLES`), pulsing energy shields (`#22`), wobbling plasma cores, and vertex-twisted tornado funnels displace local-space `VERTEX` inside spatial `.gdshader` `vertex()` within an authored bound $\|\Delta v\|_\infty \le R$ (`max_vertex_offset` $\in (0.0, 16.0]$ local-space meters). Source audit of `godot_ANGE` at `ac2f93dabdca8cc0162294b85f5122125fa5c43e` reveals both what is **already built into the GPU guard shader** and the exact C++ choke points blocking bounded `INSTANCE_192` WPO:

1. **`ParticlesExternalGuard` Already Evaluates Local-Space `SurfaceProof` Displacement Intervals in GLSL**:
   - `ParticlesExternalGuard::SurfaceProof` ([particles_external_guard.h](../servers/rendering/renderer_rd/effects/particles_external_guard.h:70)) and [particles_external_guard.glsl](../servers/rendering/renderer_rd/shaders/particles_external_guard.glsl:288) (`:288-300`, `:431-432`) **already** store `current_displacement_min[4]`, `current_displacement_max[4]`, `previous_displacement_min[4]`, and `previous_displacement_max[4]`, add `[displacement_min, displacement_max]` to all 8 local mesh AABB corners via `add_interval`, transform each displaced corner through `affine_interval(row0..2, x, y, z, numeric_valid)`, and copy `current_displacement -> previous_displacement` across frames in `ParticlesStorage::_external_guard_copy_history_locked` ([particles_storage_external.cpp](../servers/rendering/renderer_rd/storage_rd/particles_storage_external.cpp:189)).
2. **Hardcoded `!zero_displacement` (`!uses_vertex`) & `Recipe 2`-Only Choke Points Currently Reject All `VERTEX` Writes**:
   - `Registry::production_prepare_material_qualification` & `production_prepare_material_recipe` ([gdextension_production_material_qualification.cpp](../core/extension/gdextension_production_material_qualification.cpp:514), `:663-664`) accept only `Recipe 1` (`OPAQUE_V1 = 1`, gate mask `0`) and `Recipe 2` (`PARTICLE_SURFACE_V1 = 2`, gate mask `412`), rejecting `MATERIAL_RECIPE_VERTEX_WRITE = 1u` (`uses_vertex`), requiring `(compiler_values & ZERO_DISPLACEMENT_V1) != 0`, and checking `recipe == PARTICLE_SURFACE_V1` in `prepare` (`:877`), `record` (`:1414`), and `read` (`:1666`).
   - `RenderForwardClustered::warm_native_material_instance192_shader` ([render_forward_clustered.cpp](../servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.cpp:6379)) and `RenderForwardMobile::warm_native_material_instance192_shader` ([render_forward_mobile.cpp](../servers/rendering/renderer_rd/forward_mobile/render_forward_mobile.cpp:4812)) reject `!features.zero_displacement`.
   - `MaterialStorage::material_recipe_source_acquire` ([material_storage.cpp](../servers/rendering/renderer_rd/storage_rd/material_storage.cpp:3380), `:3386`) and `MaterialData::get_native_recipe_shader_if_ready` ([scene_shader_forward_clustered.cpp](../servers/rendering/renderer_rd/forward_clustered/scene_shader_forward_clustered.cpp:1461), [scene_shader_forward_mobile.cpp](../servers/rendering/renderer_rd/forward_mobile/scene_shader_forward_mobile.cpp:1442)) reject `!zero_displacement`.
   - `ShaderData::get_native_instance192_shader_features` ([scene_shader_forward_clustered.cpp](../servers/rendering/renderer_rd/forward_clustered/scene_shader_forward_clustered.cpp:289), [scene_shader_forward_mobile.cpp](../servers/rendering/renderer_rd/forward_mobile/scene_shader_forward_mobile.cpp:304)) and `MaterialStorage::material_selection_get_shader_observation` ([material_storage.cpp](../servers/rendering/renderer_rd/storage_rd/material_storage.cpp:1232)) lack a WPO-gated `wpo_output_pass_mask` (`0u` until WPO-qualified; `PASS_TRANSPARENT` or `PASS_OPAQUE | PASS_DEPTH` without `MOTION_VECTORS` once qualified).
   - `ParticlesStorage::_external_guard_prepare_owned_prefix` ([particles_storage_external.cpp](../servers/rendering/renderer_rd/storage_rd/particles_storage_external.cpp:335)) rejects `!contact.material.zero_displacement` with `ERR_UNAVAILABLE` and leaves `SurfaceProof` displacement vectors hardcoded to `{0, 0, 0, 0}`.
3. **Unscaled `Recipe 2` `[-0.5, +0.5]` Oracle Quad Cannot Empirically Bound $R \in (0.0, 16.0]$**:
   - In `Recipe 2` ([gdextension_production_material_qualification.cpp](../core/extension/gdextension_production_material_qualification.cpp:222)), the `[-0.5, +0.5]` quad at `scale = 1.0` already spans the full allowed `[-0.50, +0.50]` interior inside the `32x16` orthographic `[-1, +1]^2` target ([render_scene_data_rd.cpp](../servers/rendering/renderer_rd/storage_rd/render_scene_data_rd.cpp:344)). Leaving `scale = 1.0` would make any $R \ge 0.50$ (`vfx_mesh_wpo.material` uses $R = 0.50$) cover `[-1, +1]^2` and collapse `exterior_mask` to `0u`. Scaling `fixed_instance192` uniformly by $s = \frac{0.4375f}{0.5f + R}$ with wide orthographic depth `[z_near = 0.0625f, z_far = 64.0f]` keeps all **384 `exterior_mask` pixels** active across the entire $R \in (0.0, 16.0]$ domain without premature Z clipping on diagonal escapes.

---

## 2. User / API Surface

- **GDExtension Material Qualification C ABI (`core/extension/gdextension_interface.json`) — Zero Struct Growth (`784B` Preserved)**:
  - Append to `GDExtensionMaterialQualificationConstantsV1` ([gdextension_interface.json](../core/extension/gdextension_interface.json:13043)):
    - `GDEXTENSION_MATERIAL_QUALIFICATION_RECIPE_CURRENT_RESET_F32_WORLD_PARTICLE_WPO_V1 = 3`
    - `GDEXTENSION_MATERIAL_QUALIFICATION_PARTICLE_WPO_SOURCE_GATE_ALLOWED_MASK_V1 = 413` (`0x0000019du = PARTICLE_SOURCE_GATE_ALLOWED_MASK_V1 (412) | SOURCE_GATE_VERTEX_WRITE_V1 (1)`)
  - `GDExtensionMaterialQualificationPrepareV1::reserved` (`bytes 780..783` of `784B`, [gdextension_interface.json](../core/extension/gdextension_interface.json:13531)):
    - `0u` mandatory for `recipe == 1` and `recipe == 2` (`ERR_INVALID_PARAMETER` if non-zero, preserving all existing tests).
    - IEEE-754 binary32 bits of finite normal `max_vertex_offset` $R \in (0.0f, 16.0f]$ (`0x00800000u <= bits <= 0x41800000u`) for `recipe == 3` (`ERR_INVALID_DATA`, reason `3` if `<= 0.0f`, `> 16.0f`, subnormal/`-0.0f`, or `NaN`/`Inf`).
- **Zero-New-Function `CreateV4` Integration via Generation-Bound `MaterialStorage` & Frozen `ExternalOutputOwner` WPO Custody**:
  - No new `device_session_output_create` C ABI function is required: when `Registry::production_read_material_qualification` ([gdextension_production_material_qualification.cpp](../core/extension/gdextension_production_material_qualification.cpp:1600)) delivers a `Recipe 3` result whose `32x16` `R16G16B16A16_SFLOAT` readback satisfies `all_finite && exterior_zero`, Core stamps `(qualified_wpo_generation = material_generation, qualified_wpo_max_offset_m = R)` onto `MaterialStorage::Material`. During `device_session_output_create_v4` (`ExternalOutputOwner::acquire_generation_selection`, [external_output_owner.cpp](../servers/rendering/renderer_rd/storage_rd/external_output_owner.cpp:2122)), each `GenerationMaterialSelection` row freezes `float wpo_max_offset_m` under its held `NativeSelectionLease` for the owner's entire generation lifetime.
- **ClassDB / `.xml` Surface**: **N/A** (no public GDScript/ClassDB additions).

---

## 3. Behavioral Requirements & Invariants

1. **Source Gate Partition (`413` Allowed vs Unbounded Modes Strictly Rejected)**:
   - `Recipe 3` admits `MATERIAL_RECIPE_VERTEX_WRITE = 1u` (`uses_vertex`) on top of `412u` (`413u = 0x19du`).
   - **Unbounded / Non-Local Modes Strictly Rejected**:
     - `MATERIAL_RECIPE_VERTEX_MODE = 2u` (`!instance192_shader_supported()` at [scene_shader_forward_clustered.cpp](../servers/rendering/renderer_rd/forward_clustered/scene_shader_forward_clustered.cpp:820) and [scene_shader_forward_mobile.cpp](../servers/rendering/renderer_rd/forward_mobile/scene_shader_forward_mobile.cpp:801)): clip-space `POSITION` override (`uses_position`), `render_mode skip_vertex_transform;`, `render_mode world_vertex_coords;`, writes to `MODELVIEW_MATRIX` / `PROJECTION_MATRIX`, `POINT_SIZE`, and `particle_trails` remain **unconditionally rejected**.
     - `MATERIAL_RECIPE_DEPTH_WRITE = 32u` (`DEPTH` write and `render_mode depth_test_inverted;`), `MATERIAL_RECIPE_CLIP_WRITE = 64u` (`Z_CLIP_SCALE`), `MATERIAL_RECIPE_STENCIL = 512u`, `MATERIAL_RECIPE_WIREFRAME = 1024u`, and `MATERIAL_RECIPE_CULL_SCOPE = 2048u` (`cull_front`) remain **unconditionally rejected**.
2. **Empirical GPU `32x16` `R16G16B16A16_SFLOAT` Scaled Oracle (`all_finite && exterior_zero`)**:
   - Given declared $R \in (0.0, 16.0]$ (`max_vertex_offset_bits = prepare->reserved`), let `const float s = 0.4375f / (0.5f + R); const float inv_s = 1.0f / s;`:
     - **Fixture geometry**: standard `[-0.5, +0.5]^2 x {0}` quad (`fixture_quad_desc[18]`).
     - **Wide-Z `fixed_camera_desc`**: `{ 2.0f, 0.0625f, 64.0f, 0.0f, 0.0f, 0.0f, 32.0f, 16.0f }` (`z_near = 0.0625f, z_far = 64.0f` — wide Z frustum prevents diagonal XY+Z escapes such as `VERTEX += vec3(0.6, 0.0, 1.3)` or `vec3(2.0, 0.0, 2.0)` from near/far-clipping out before hitting the 384 exterior pixels).
     - **Exact 48-Word `fixed_instance192(s)` Layout** (shared bit-for-bit with ANGE `FR_particle_wpo_bounded_displacement_PLAN.md`):
       - `words[0] = words[5] = words[10] = float_bits(s)`, `words[11] = float_bits(-2.0f)`
       - `words[12] = words[17] = words[22] = float_bits(1.0f)`
       - `words[24] = words[29] = words[34] = float_bits(inv_s)`, `words[35] = float_bits(2.0f * inv_s)`
       - `words[36] = words[37] = words[38] = words[39] = float_bits(1.0f)`, `words[46] = 0u` (`Recipe2FlagsWord == 0u`), all other words `0u`.
     - **Verifier (`all_finite && exterior_zero` over `384` exterior pixels)**: `interior_mask[5..10] = 0x007ffe00u`, `exterior_mask[0..3, 12..15] = 0xffffffffu`, `exterior_mask[4..11] = 0xff0000ffu`. Verifier checks **only `all_finite && exterior_zero`** (`interior_nonzero` is NOT required because valid `ALPHA = sin(TIME)` shaders evaluate to `0.0` at `TIME = 0.0`, while pure Z-only displacement without XY extent is bounded at runtime by `SurfaceProof` `[-R_s, +R_s]^3` + `ParticlesExternalGuard` `BOUNDS_FAULT`).
     - **Oracle Digest Binding (`132B`)**: `mq->oracle.recipe` hashes `"godot.material_qualification.recipe.current_reset_f32_world_particle_wpo.v1"` over `vec_bytes[96] + verifier[32] + max_vertex_offset_bits_le[4]` (`132` bytes).
3. **`wpo_output_pass_mask` Eligibility (`OPAQUE`/`TRANSPARENT` Admitted; Un-Qualified & `MOTION_VECTORS` Rejected)**:
   - When `!features.zero_displacement`: `ShaderData::get_native_instance192_shader_features` sets `features.output_pass_mask = 0u` and sets `features.wpo_output_pass_mask = transparent ? Output::PASS_TRANSPARENT : (Output::PASS_OPAQUE | Output::PASS_DEPTH)` iff `instance192_shader_supported() && (features.material_recipe_rejections & ~413u) == 0u` (omitting `PASS_SHADOW | PASS_PROBE`).
   - `MaterialStorage::material_selection_get_shader_observation` exposes `wpo_output_pass_mask` as `observation.output_pass_mask` **only** when `qualified_wpo_generation == p_lease.generation && qualified_wpo_max_offset_m > 0.0f` (else `0u`), and `ExternalOutputOwner` rejects `config.motion == ExternalOutputConfig::REQUIRED` (`ExternalPacketPass::MOTION` / `MOTION_VECTORS`) whenever any surface has `!zero_displacement`.
4. **Frozen Per-Owner `wpo_max_offset_m` & `SurfaceProof` `[-R_s, +R_s]^3` Inflation**:
   - `ExternalOutputOwner::acquire_generation_selection` snapshots `row.wpo_max_offset_m` under each surface's `NativeSelectionLease` at creation time so subsequent re-qualification of the same `Material` RID for another emitter (`R_A = 2.0f` vs `R_B = 0.5f`) never mutates `Output A`'s `SurfaceProof` (`particles_storage_external.cpp:349` `memcmp` invariant preserved!).
   - In [particles_external_guard.glsl](../servers/rendering/renderer_rd/shaders/particles_external_guard.glsl:295), `add_interval(point, [-R_s, +R_s])` + `affine_interval(row0..2, x, y, z)` scales $R_s$ automatically by `Instance192Record` `model_3x4` (`host * particle`, collapsing to `0` on G1 `O-01` zero-basis degenerate records).

---

## 4. Boundaries, Non-Goals & Backend Support

- **Non-Goals**: `INDEXED_144` (`Ribbon`) WPO (`!material_obs.zero_displacement -> ERR_UNAVAILABLE` at [particles_storage_external.cpp](../servers/rendering/renderer_rd/storage_rd/particles_storage_external.cpp:576)) and WPO `MOTION_VECTORS` (`ExternalOutputConfig::Motion::REQUIRED -> ERR_UNAVAILABLE`) stay disallowed. `.material` `max_vertex_offset` DSL parsing, `VFX-MAT-WPO-001` static lint, `compute_wpo_cull_margin`, and `vfx_mesh_wpo*` assets live in `ANGE_agy_core` (`FR_particle_wpo_bounded_displacement.md`). `Mesh Array` and `GPU->Host Readback` are explicitly excluded by suite ROI ruling.
- **Renderer Backends**: Supported on `RendererRD` (`RenderForwardClustered` and `RenderForwardMobile`). Compatibility (`GLES3`) and Dummy (`RendererDummy`) return `ERR_UNAVAILABLE` with zero silent fallback.

---

## 5. Acceptance Criteria & Relationship to Adjacent Capabilities

1. `Recipe 3` (`WPO_V1 = 3`, `784B` `PrepareV1::reserved = f32_bits(R)`) qualifies spatial particle shaders writing local `VERTEX` within `<= R` across `R in (0.0, 16.0]` on Forward+ and Mobile (`prepare`, `record`, and `read` all routing through `32x16` `R16G16B16A16_SFLOAT`), empirically fails adversarial XY and diagonal XY+Z over-displacement via the `384`-pixel `exterior_mask` (`all_finite && exterior_zero` under wide Z `[0.0625f, 64.0f]`), and statically rejects `POSITION` / `skip_vertex_transform` / `world_vertex_coords` / `depth_test_inverted` / `MOTION_VECTORS`.
2. Frozen `GenerationMaterialSelection::wpo_max_offset_m` populates per-surface `SurfaceProof` `[-R_s, +R_s]^3` under existing `CreateV4` (including multi-surface `Recipe 2 + Recipe 3` meshes, shared-material dual emitters `R_A != R_B`, and `O-01` zero-size collapse), while un-qualified or mutated `VERTEX`-writing materials expose `output_pass_mask == 0u` and fail closed with `ERR_UNAVAILABLE`.
3. Relationship to adjacent FRs: Builds on [FR_godot_particle_zero_size_and_ribbon_guard](FR_godot_particle_zero_size_and_ribbon_guard.md) (G1) and [FR_godot_particle_output_role9_aux_params](FR_godot_particle_output_role9_aux_params.md) (G2); independent of [FR_godot_particle_scene_depth_snapshot_v2](FR_godot_particle_scene_depth_snapshot_v2.md) (G3).
