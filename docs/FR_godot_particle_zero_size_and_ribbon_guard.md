# FR: Godot External Particle Zero-Size Degenerate Primitive Admission (`O-01`) & Ribbon Sub-Segment History Compatibility

- **Paired Implementation Plan**: [FR_godot_particle_zero_size_and_ribbon_guard_PLAN](FR_godot_particle_zero_size_and_ribbon_guard_PLAN.md)
- **Suite Overview**: [FR_godot_particle_ue_parity_OVERVIEW](FR_godot_particle_ue_parity_OVERVIEW.md)
- **Planning Status**: `ready` | **Implementation Status**: `not-started` | **Engine Tests**: `not-run`
- **Counterpart Cross-Repo Producer Spec**: `ANGE_agy_core` `FR-RND-01` (`FR_particle_rnd_sprite_ribbon_zero_size.md`, tests `R01..R28`)

---

## 1. Problem & Current Fork Behavior

Standard VFX emitters routinely author `0.0 -> peak -> 0.0` scale-over-life curves on `INSTANCE_192` (`Sprite`, `Mesh`), spawn newborn consecutive `Ribbon` (`INDEXED_144`) particles at coincident world positions (`length == 0.0f`), taper `ribbon_width` to `0.0f`, and subdivide sharp ribbon turns into `tessellation_factor: 1..4` Catmull-Rom sub-segments. Source inspection of `godot_ANGE` at `ac2f93dabdca8cc0162294b85f5122125fa5c43e` exposes three concrete engine-side defects/gaps that must be closed for `O-01`:

1. **`particles_external_guard.glsl` Spurious `BOUNDS_FAULT` (`64u`) on Boundary-Seated Zero-Area `Instance192Record` (World & Local Space)**:
   - In [particles_external_guard.glsl](../servers/rendering/renderer_rd/shaders/particles_external_guard.glsl:218), `affine_interval(row, x, y, z, numeric_valid)` unconditionally calls `multiply_interval(vec2(row.x), x, ...)` even when `row.xyz == vec3(0.0)` (canonical zero-area degenerate `Instance192Record`).
   - Because `rounded_interval(0.0, ...)` ([particles_external_guard.glsl](../servers/rendering/renderer_rd/shaders/particles_external_guard.glsl:179)) widens `0.0f` by `ROUNDING_FLOOR = 2^-20` (`9.53674316e-7f`), three zero basis multiplications plus three `add_interval` steps artificially inflate a collapsed point `[tx, tx]` by `~6 * 2^-20` (`~5.72e-6f`). In `WORLD` space (`proof.source_accessor.w == 1u`), a zero-size particle spawned on an authored floor boundary (`ty == current_decl_min.y == cull_min.y`) escapes [particles_external_guard.glsl](../servers/rendering/renderer_rd/shaders/particles_external_guard.glsl:318), raising `BOUNDS_FAULT = 64u` and zeroing `ProtectedCommands.instanceCount` ([particles_external_guard.glsl](../servers/rendering/renderer_rd/shaders/particles_external_guard.glsl:521)) for the **entire emitter batch**, even though IEEE-754 vertex evaluation `0.0 * v + t == t` has zero multiplication error.
   - Additionally, in **`LOCAL` space (`proof.source_accessor.w == 2u`)**, [particles_external_guard.glsl](../servers/rendering/renderer_rd/shaders/particles_external_guard.glsl:307) transforms `world_x/y/z = [tx, tx]` back into emitter-local space via `affine_interval(inverse0..2, world_x, world_y, world_z, numeric_valid)` where `Host^-1` has a **non-zero** basis (`~6 * 2^-20` interval rounding plus F32 `Host * local -> world` roundtrip error). `LOCAL`-space emitters therefore require an explicit cross-repo `>= 1.0e-4f` outward pad on `current_decl_min` / `current_decl_max`.
2. **C++ `Instance192Record` & `Float144Vertex` Builders Mask `NaN`/`Inf` and Emit Invalid Singular Records**:
   - `ParticlesMeshLightSortRD::canonicalize_finite_float` ([particles_mesh_light_sort_rd.cpp](../servers/rendering/renderer_rd/storage_rd/particles_mesh_light_sort_rd.cpp:167)) and `ParticlesRibbonRD::canonicalize_finite_float` ([particles_ribbon_rd.cpp](../servers/rendering/renderer_rd/storage_rd/particles_ribbon_rd.cpp:158)) flush `mag >= 0x7f800000u` (`NaN`/`Inf`) to `0.0f`, and `ParticlesRibbonRD::build_ribbon_segment_quad` ([particles_ribbon_rd.cpp](../servers/rendering/renderer_rd/storage_rd/particles_ribbon_rd.cpp:168)) must fail closed on `NaN`/`Inf` inputs *before* clamping negative widths to `0.0f`.
   - `ParticlesMeshLightSortRD::make_instance192_record` ([particles_mesh_light_sort_rd.cpp](../servers/rendering/renderer_rd/storage_rd/particles_mesh_light_sort_rd.cpp:192)) leaves `inverse_transform_rows` all-zero (`{0,0,0,0}`), `color` non-zero, and `flags = 1u` when `|det| <= 1e-12f`, and fails to distinguish canonical zero-scale degeneration (`min(|sx|, |sy|, |sz|) <= 1e-6f` -> zero basis + `{I, -t}` + `flags = 17u`) from an uninverted non-zero singular matrix.
3. **Cross-Repo Wire Drift in `RibbonSegmentEndpointKey32` (`Float144Vertex` `words[28..35]`)**:
   - `ANGE_agy_core` (`ribbon_frame_history.cpp:41-50`) packs `Float144Vertex` `words[28..35]` (`endpoint_key_words[0..7]`) as `{start_id_index (w28), start_id_tag (w29), end_id_index (w30), end_id_tag (w31), group_lo (w32), group_hi (w33), subsegment_index (w34), valid_mask (w35)}`, whereas `godot_ANGE`'s `RibbonSegmentEndpointKey32` ([particles_ribbon_rd.h](../servers/rendering/renderer_rd/storage_rd/particles_ribbon_rd.h:47)) previously stored `{emitter_id, trail_id, head_particle_id, tail_particle_id}`. Storing `subsegment_index` in `word[31]` would corrupt `end_id_tag`! Both repositories must lock `subsegment_index` (`0..3` for `tessellation_factor: 1..4`) in **`endpoint_key_words[6]` (`Float144Vertex` `word[34]`)** and `valid_mask` (`0u` or `1u`) in **`endpoint_key_words[7]` (`word[35]`)**.

---

## 2. User / API Surface

- **Authoring DSL / GDScript / ClassDB Surface**: **N/A** (internal `RendererRD` C++ storage/guard and GLSL compute contracts; no public ClassDB method, `.xml` doc, or GDExtension JSON enum changes).
- **Internal Engine C++ Surface**:
  - `RendererRD::ParticlesMeshLightSortRD` ([particles_mesh_light_sort_rd.h](../servers/rendering/renderer_rd/storage_rd/particles_mesh_light_sort_rd.h:285)):
    - `static bool flush_subnormal_and_neg_zero(float p_value, float &r_out)` (returns `false` on `NaN`/`Inf`; flushes IEEE-754 subnormals and `-0.0f` to `+0.0f`).
    - `static Instance192Record make_canonical_degenerate_instance192(const Vector3 &p_translation_world, const Color &p_uv_rect, uint64_t p_particle_id, bool p_has_previous)`.
    - `static Error build_instance192_record_checked(const Transform3D &p_transform, const Color &p_color, const Color &p_custom_uv_rect, uint64_t p_particle_id, bool p_has_previous, bool p_allow_zero_scale_degenerate, Instance192Record &r_record)`.
  - `RendererRD::RibbonSegmentEndpointKey32` & `Float144Vertex` ([particles_ribbon_rd.h](../servers/rendering/renderer_rd/storage_rd/particles_ribbon_rd.h:47)):
    - Canonical 8-word (`32B`) wire layout matching `Float144Vertex` `words[28..35]`: `{uint32_t start_id_index, start_id_tag, end_id_index, end_id_tag, group_lo, group_hi, subsegment_index, valid_mask}` (`sizeof == 32`), with convenience constructors/helpers preserving `{start_id, end_id, group_id, subsegment_index = 0u, valid_mask = 1u}`, `with_subsegment(uint32_t p_subsegment_ordinal) const` (writing `subsegment_index = p_subsegment_ordinal & 3u` in **`endpoint_key_words[6]` / `word[34]`**), and `uint32_t get_subsegment() const` (reading `endpoint_key_words[6]` / `word[34]`).
    - Enforce `Float144Vertex` `pad0 == 0u` (`word[3]`) and `pad1 == 0u` (`word[7]`) matching [particles_external_guard_indexed144.glsl](../servers/rendering/renderer_rd/shaders/particles_external_guard_indexed144.glsl:279).
    - `ParticlesRibbonRD::build_ribbon_segment_quad_checked(...)` (and fail-closed `NaN`/`Inf` pre-check in `build_ribbon_segment_quad`) rejecting any non-finite input before admitting finite `half_width <= 0.0f` and coincident endpoints (`||tail - head|| <= 1e-6f`) as `[P0, P0, P1, P1]`.

---

## 3. Behavioral Requirements & Invariants

1. **Canonical Zero-Area Degenerate `Instance192Record` (`48 x u32 = 192B`)**:
   - Given finite translation $t = (t_x, t_y, t_z)$ (with subnormals/`-0.0f` flushed to `+0.0f`):
     - `words[0..11]` (`transform_rows`): `{+0, +0, +0, tx}`, `{+0, +0, +0, ty}`, `{+0, +0, +0, tz}`.
     - `words[12..23]` (`normal_row0..2` + `pad0..2`): `{1, +0, +0, 0u}`, `{+0, 1, +0, 0u}`, `{+0, +0, 1, 0u}`.
     - `words[24..35]` (`inverse_transform_rows`): `{1, +0, +0, -tx}`, `{+0, 1, +0, -ty}`, `{+0, +0, 1, -tz}` (exact finite translation inverse safe for `INV_MODEL_MATRIX`).
     - `words[36..39]` (`color`): `{+0, +0, +0, +0}`.
     - `words[40..43]` (`custom0`): finite normal/zero `uv_rect` (default `{+0, +0, 1, 1}`).
     - `words[44..45]`: `{id_index, id_acquire_tag}` preserved unchanged so `A05` motion-vector history remains continuous across `0 -> peak -> 0` scale transitions.
     - `word[46]` (`flags`): `17u` (`FLAG_VALID_RECORD | FLAG_INVERSE_AVAILABLE`), or `19u` (`17u | FLAG_HAS_PREVIOUS_RECORD`) when valid previous record exists; `word[47] == 0u`.
2. **Exact Zero-Basis Affine Interval (`WORLD`) & `>= 1e-4f` Declaration Margin (`LOCAL`)**:
   - In `affine_interval(vec4 row, vec2 x, vec2 y, vec2 z, inout bool numeric_valid)` ([particles_external_guard.glsl](../servers/rendering/renderer_rd/shaders/particles_external_guard.glsl:218)), after verifying `x`, `y`, `z`, and `row.w` are finite (propagating `numeric_valid = false` on any `NaN`/`Inf`), if `(floatBitsToUint(row.x) & 0x7fffffffu) == 0u && (floatBitsToUint(row.y) & 0x7fffffffu) == 0u && (floatBitsToUint(row.z) & 0x7fffffffu) == 0u`, return `vec2(row.w, row.w)` directly without adding `ROUNDING_FLOOR` zero-product noise.
   - In `LOCAL` space (`proof.source_accessor.w == 2u`, [particles_external_guard.glsl](../servers/rendering/renderer_rd/shaders/particles_external_guard.glsl:307)), `Host^-1` has a non-zero rotated/scaled basis, so `affine_interval(inverse0..2, [tx, tx], [ty, ty], [tz, tz], ...)` widens the reconstructed local interval by `~6 * 2^-20` (`~5.72e-6f`) plus F32 `Host * local -> world` roundtrip error. Cross-repo contract (`FR_particle_rnd_sprite_ribbon_zero_size.md`): ANGE pads `LOCAL`-space `current_decl_min` / `current_decl_max` outward by `>= 1.0e-4f` (verified in `T01` with both `1e-4f`-padded `PASS` and unpadded boundary `BOUNDS_FAULT = 64u`).
3. **Fail-Closed Rejection of Corrupt / Non-Finite / Non-Zero Pad / Uninverted Singular Inputs**:
   - Any `NaN` or `Inf` word in `Instance192Record` (`words[0..43]`) or `Float144Vertex` (`words[0..27]`), any subnormal float word reaching the GPU guard, any non-zero pad word (`words[15, 19, 23, 47]` in `Instance192`; **`pad0 (word[3]) == 0u && pad1 (word[7]) == 0u`** in `Float144Vertex` enforced at [particles_external_guard_indexed144.glsl](../servers/rendering/renderer_rd/shaders/particles_external_guard_indexed144.glsl:279)), or invalid `flags` (`(flags & 1u) == 0u || (flags & ~31u) != 0u`) MUST raise `NUMERIC_FAULT = 16u` and zero all `ProtectedCommands`.
   - In `ParticlesMeshLightSortRD::build_instance192_record_checked`, when `p_allow_zero_scale_degenerate == true`, uniform/per-axis singular scale (`min_col_len <= 1e-6f`) emits the canonical zero-area degenerate record, whereas a non-zero collinear/coplanar rank-deficient matrix (`min_col_len > 1e-6f` with `|det| <= 1e-12f * l0 * l1 * l2`) returns `ERR_INVALID_PARAMETER`.
   - In `ParticlesRibbonRD::build_ribbon_segment_quad` and `build_ribbon_segment_quad_checked`, non-finite `p_half_width`, `p_head_world`, `p_tail_world`, or `p_camera_world` MUST fail closed (zeroing output vertices/indices) *before* checking `p_half_width <= 0.0f`.
4. **Ribbon Zero-Width / Coincident-Endpoint Survival & Canonical `word[34]` Sub-Segment Key**:
   - Finite `p_half_width <= 0.0f` and coincident ribbon endpoints (`||tail - head|| <= 1e-6f`) produce finite unit fallback tangent/normal, `pad0 == 0u`, `pad1 == 0u`, and collapsed quad vertices `[head, head, tail, tail]` that pass `ParticlesExternalGuardIndexed144` (`status.words[0] == 0u`, `ProtectedCommands[0] == active_segments * 6u`).
   - Encoding sub-segment ordinal $m \in [0..3]$ into `RibbonSegmentEndpointKey32::subsegment_index` (**`endpoint_key_words[6]` / `Float144Vertex` `word[34]`**) with `valid_mask = 1u` (`endpoint_key_words[7]` / `word[35]`) matches ANGE's `ribbon_frame_history.cpp` wire format word-for-word so `align_previous_ribbon_vertices` ([particles_ribbon_rd.cpp](../servers/rendering/renderer_rd/storage_rd/particles_ribbon_rd.cpp:336)) and [particles_external_guard_indexed144.glsl](../servers/rendering/renderer_rd/shaders/particles_external_guard_indexed144.glsl:293) (`words[28..35]`) match exact sub-segment predecessors across frames without `HISTORY_FAULT = 32u` or `end_id_tag` (`word[31]`) corruption.

---

## 4. Boundaries, Non-Goals & Backend Support

- **Non-Goals**: Does not implement Catmull-Rom spline subdivision or `.vfx` `SpriteFacingMode` math inside Godot Core (those execute in `ANGE_agy_core` export kernels per `FR_particle_rnd_sprite_ribbon_zero_size.md`). Does not alter `Instance192Record` (`192B`) or `Float144Vertex` (`144B`) byte sizes or GDExtension C ABI structs.
- **Renderer Backends**: Applies to `RendererRD` (Forward+ Clustered and Forward Mobile on Metal/Vulkan/D3D12). Compatibility (`GLES3::ParticlesStorage`, [particles_storage.h](../drivers/gles3/storage/particles_storage.h:51)) and Dummy (`RendererDummy::ParticlesStorage`, [particles_storage.h](../servers/rendering/dummy/storage/particles_storage.h:39)) do not host external RD output packets (`particles_is_external` is `false` / `ERR_UNAVAILABLE`), with no silent CPU fallback.

---

## 5. Acceptance Criteria & Relationship to Adjacent Capabilities

1. Canonical zero-area `Instance192Record` (`flags == 17u` and `19u` with history) seated on exact `WORLD` bounds (`current_decl_min == cull_min`) and inside `1e-4f`-padded `LOCAL` bounds (`source_accessor.w == 2u` under rotated+translated `Host^-1`) passes `ParticlesExternalGuard` on CPU fixture (`particle_external_guard/run.py`) and live Metal RD (`status.words[0] == 0u`, `commands[1] == active_count`), while `NaN`/`Inf`, subnormals, non-zero pads, bad flags, and bounds escapes still zero all commands.
2. Mixed batches (visible + zero-size degenerate instances; visible + coincident/zero-width ribbon segments + `K = 4` `word[34]` sub-segment keys) preserve batch `ProtectedCommands` counts and `A05` motion-vector history validity, while `Float144Vertex` `pad0 != 0u` (`word[3]`) or `pad1 != 0u` (`word[7]`) triggers `NUMERIC_FAULT = 16u`.
3. Relationship to adjacent FRs: Prerequisite before [FR_godot_particle_output_role9_aux_params](FR_godot_particle_output_role9_aux_params.md) on [particles_external_guard.glsl](../servers/rendering/renderer_rd/shaders/particles_external_guard.glsl:218); independent of [FR_godot_particle_scene_depth_snapshot_v2](FR_godot_particle_scene_depth_snapshot_v2.md).
