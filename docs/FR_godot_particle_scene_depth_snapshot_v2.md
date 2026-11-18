# FR: Godot `SceneDepthSnapshotMeta256` (`v2`), GDExtension Lease C ABI, & Pre-Transparent Reverse-Z `R32_SFLOAT` Depth Snapshot Provider

- **Paired Implementation Plan**: [FR_godot_particle_scene_depth_snapshot_v2_PLAN](FR_godot_particle_scene_depth_snapshot_v2_PLAN.md)
- **Suite Overview**: [FR_godot_particle_ue_parity_OVERVIEW](FR_godot_particle_ue_parity_OVERVIEW.md)
- **Planning Status**: `ready` | **Implementation Status**: `not-started` | **Engine Tests**: `not-run`
- **Counterpart Cross-Repo Producer Spec**: `ANGE_agy_core` `FR-PAR-04` Part B (`FR_particle_par_e10_and_scene_depth.md`, tests `P01..P33`)

---

## 1. Problem & Current Fork Behavior

GPU and CPU `engine.scene_depth.collide` particle simulation requires a hazard-free, zero-CPU-readback scene depth texture paired with an exact `256B` `std430` metadata buffer and a GDExtension C ABI lease bridge consumable across the `godot_ANGE` <-> `libange` dylib boundary. Source audit of `godot_ANGE` at `ac2f93dabdca8cc0162294b85f5122125fa5c43e` exposes four concrete gaps:

1. **`256B` Struct Layout Drift (`SceneCurrentDepthMeta256` v1 vs ANGE)**:
   - [particles_collision_scene_rd.h](../servers/rendering/renderer_rd/storage_rd/particles_collision_scene_rd.h:111) defines `SceneCurrentDepthMeta256` (`magic = 0x4b303644u` `"K06D"`, `version = 1`) with `{world_to_view_rows, view_to_world_rows, projection_rows, ...}`, whereas `ANGE_agy_core` (`scene_depth_current.h`) had drifted to a different field order. Both repositories must lock the single canonical **`SceneDepthSnapshotMeta256` (`magic = 0x4b303644u`, `version = 2u`)** frozen in `FR_particle_par_e10_and_scene_depth.md`.
2. **Missing GDExtension C ABI Bridge (`core/extension/` <-> `libange`) for Phase-1 Depth Snapshot Leases**:
   - `libange` is a separate dynamic library and cannot call `RendererRD::ParticlesCollisionSceneRD` C++ methods directly. `core/extension/` already exposes `Registry::scene_phase_borrow` ([gdextension_production_window.cpp](../core/extension/gdextension_production_window.cpp:1489)) internally for `SceneReadContent` retention on `request->actions[ordinal].scene_attempt.content`, and `gdextension_interface.json` ([gdextension_interface.json](../core/extension/gdextension_interface.json:25860)) registers `device_session_scene_phase_*` entrypoints ([gdextension_production_interface.cpp](../core/extension/gdextension_production_interface.cpp:667)), but lacks C ABI functions to arm per-viewport snapshot demand and borrow `{meta_ssbo_rid, depth_texture_rid, snapshot_generation}` during `SCENE_PHASE_BEFORE_PARTICLES` (`phase = 1`).
3. **MSAA 4x Unresolved Depth (Forward+) & TBDR Merged Subpass / Opaque-Only Skip (Forward Mobile)**:
   - `RB_TEX_DEPTH` ([render_scene_buffers_rd.cpp](../servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.cpp:199)) lacks `CAN_COPY_FROM_BIT` and is rejected by `RD::texture_copy` ([rendering_device.cpp](../servers/rendering/rendering_device.cpp:3862)).
   - In **Forward+** ([render_forward_clustered.cpp](../servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.cpp:2733)) and **Forward Mobile** ([render_forward_mobile.cpp](../servers/rendering/renderer_rd/forward_mobile/render_forward_mobile.cpp:1107)), Godot only sets `scene_state.used_depth_texture = true` when a material uses `hint_depth_texture` or `has_active_late_transparent_depth_phase` is true. Without also OR-ing `ParticlesCollisionSceneRD::has_pending_depth_snapshot_request(scene_viewport, frame_number)`:
     - Forward+ under MSAA 2x/4x/8x skips `resolve_effects->resolve_depth` (`:2752-2756`), leaving `rb->get_depth_texture()` unresolved!
     - Forward Mobile keeps `merge_transparent_pass = true` (`:1111-1128`), allowing TBDR transient/discardable depth and skipping `dispatch_late_particle_collision_phase` (`:1513`) on opaque-only frames (`:1455-1472`)!
4. **Point-Sampled Silhouette Fidelity & Combined Depth-Stencil (`D32_SFLOAT_S8_UINT`) Safety**:
   - Bilinear depth copying smears foreground/background depths into phantom mid-air collision walls along silhouettes (`CopyEffects::copy_to_fb_rect` defaults `p_bilinear_filtering = true` at [copy_effects.h](../servers/rendering/renderer_rd/effects/copy_effects.h:373)), and sampling a `D32_SFLOAT_S8_UINT` texture without a `TEXTURE_SLICE_DEPTH` view violates Vulkan/Metal depth-only aspect rules.

---

## 2. User / API Surface

- **GDExtension C Interface (`core/extension/gdextension_interface.json` + `gdextension_production_interface.cpp`)**:
  - `GDExtensionSceneDepthSnapshotArmV1`: `{ GDExtensionDeviceExitAbi abi; uint64_t viewport_rid; float slab_thickness_m; float silhouette_jump_m; }`
  - `GDExtensionSceneDepthSnapshotBorrowResultV1`: `{ GDExtensionDeviceExitAbi abi; GDExtensionSceneReadOutcomeV1 outcome; GDExtensionSceneOpaque32V1 sample_token; uint64_t meta_ssbo_rid; uint64_t depth_texture_rid; uint64_t snapshot_generation; uint32_t width; uint32_t height; uint32_t flags; uint32_t reserved; }`
  - `device_session_scene_depth_snapshot_arm_v1(GDExtensionProductionAttachmentRefV1 attachment, const GDExtensionSceneDepthSnapshotArmV1 *input) -> GDExtensionDeviceSessionResultV1`: arms per-viewport post-opaque depth snapshot capture on main RD (`SCENE_PHASE_BEFORE_PARTICLES` attachment).
  - `device_session_scene_depth_snapshot_borrow_v1(GDExtensionProductionPhysicalContextRefV1 context, GDExtensionProductionRequestRefV1 request, uint64_t action_ordinal, uint64_t viewport_rid, const GDExtensionSceneOperationKeyV1 *key, GDExtensionSceneDepthSnapshotBorrowResultV1 *out) -> GDExtensionDeviceSessionResultV1`: acquires `Ref<DepthReadLease>` for `viewport_rid` inside `SCENE_PHASE_BEFORE_PARTICLES`, binds it to `request->actions[action_ordinal].scene_attempt.content` via `Registry::scene_phase_borrow` (guaranteeing automatic `return_after_reads()` upon GPU completion / request retirement), and returns `{meta_ssbo_rid, depth_texture_rid, snapshot_generation, width, height, flags}`.
- **Cross-Repo Texture & Descriptor Lock (`godot_ANGE` <-> `ANGE_agy_core`)**:
  - Snapshot texture format: **`RD::DATA_FORMAT_R32_SFLOAT`** (color-aspect 2D single-sample texture storing Reverse-Z depth `[0.0 .. 1.0]`).
  - Set-1 descriptor binding: **`RD::UNIFORM_TYPE_TEXTURE`** (`1` RID, consumed in GLSL 450 via `#extension GL_EXT_samplerless_texture_functions : enable` + `uniform texture2D` + `texelFetch(tex, tc, 0)` — zero extra sampler RID!).
- **ClassDB / `.xml` Surface**: **N/A** (no public GDScript/ClassDB additions).

---

## 3. Behavioral Requirements & Invariants

1. **Canonical `256B` `SceneDepthSnapshotMeta256` Layout (`std430`, Little-Endian)**:
   - `word 0` (`0..3B`): `magic = 0x4b303644u` (`"K06D"`).
   - `word 1` (`4..7B`): `version = 2u`.
   - `words 2..3` (`8..15B`): `width`, `height` (`>= 2u` when `flags & 1u`, else `0u`).
   - `words 4..5` (`16..23B`): `uint64_t snapshot_generation` (`> 0` when `present`, `0` when unpopulated).
   - `words 6..7` (`24..31B`): `uint64_t view_id` (`p_viewport.get_id()`).
   - `words 8..23` (`32..95B`): `float world_to_clip[16]` (column-major `p_cam_projection * Projection(p_cam_transform.affine_inverse())`).
   - `words 24..39` (`96..159B`): `float clip_to_world[16]` (column-major `Projection(p_cam_transform) * p_cam_projection.inverse()` — full projective 4x4 `Projection::inverse()`, **never** `affine_inverse()`!).
   - `words 40..55` (`160..223B`): `float view_to_world[16]` (column-major `Projection(p_cam_transform)`, camera eye in `col3.xyz`).
   - `word 56` (`224..227B`): `float z_near` (`> 0.0f`; if `!Math::is_finite(z_near) || !Math::is_finite(z_far) || z_near <= 0.0f || z_far <= z_near`, e.g. orthographic camera with `z_near <= 0.0f`, `FLAG_PRESENT` MUST be cleared to `0u` -> `flags = 2u`!).
   - `words 57..58` (`228..235B`): `float slab_thickness_m = 0.50f`, `float silhouette_jump_m = 0.50f` (**viewport diagnostic defaults**; per-emitter `.vfx` overrides `[0.01 .. 10.0]` m travel in ANGE `C05` uniforms so multiple emitters sharing one viewport never clobber each other).
   - `word 59` (`236..239B`): `uint32_t flags` (`FLAG_PRESENT = 1u`, `FLAG_REVERSE_Z = 2u`, `(flags & ~3u) == 0u`).
   - `words 60..63` (`240..255B`): `uint32_t reserved[4] = {0u, 0u, 0u, 0u}`.
2. **MSAA Resolve, Mobile Non-Merged Pass, & Point-Sampled Pre-Transparent Copy (`p_bilinear_filtering = false`)**:
   - At **both** [render_forward_clustered.cpp](../servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.cpp:2733) and [render_forward_mobile.cpp](../servers/rendering/renderer_rd/forward_mobile/render_forward_mobile.cpp:1107), OR-in `collision_scene_rd->has_pending_depth_snapshot_request(scene_viewport, frame_number)` to set `scene_state.used_depth_texture = true`:
     - In **Forward+**, this triggers MSAA 2x/4x/8x `resolve_effects->resolve_depth` (`:2752-2756`) and `_render_buffers_copy_depth_texture` (`:2814`) prior to `dispatch_late_particle_collision_phase` (`:2841`).
     - In **Forward Mobile**, this disables `merge_transparent_pass` and `using_subpass_post_process` and sets `resolve_depth_buffer = true` (`:1111-1128`), preventing TBDR discardable depth loss and routing **every** demanded frame (even with `0` transparent elements!) through `_render_buffers_copy_depth_texture` (`:1509`) and `dispatch_late_particle_collision_phase` (`:1513`) strictly pre-transparent (with an additional guard in `:1464`).
   - Depth copy into `dst_gen->framebuffer_rid` (`R32_SFLOAT` color attachment) MUST call `copy_effects->copy_to_fb_rect` ([copy_effects.h](../servers/rendering/renderer_rd/effects/copy_effects.h:373)) with **`p_linear = false` AND `p_bilinear_filtering = false`** (`RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST` at [copy_effects.cpp](../servers/rendering/renderer_rd/effects/copy_effects.cpp:652)) — **never** bilinear across silhouettes! When reading a raw depth texture whose format is `D32_SFLOAT_S8_UINT` / `D24_UNORM_S8_UINT`, bind a depth-only slice view (`RD::TEXTURE_SLICE_DEPTH`) or read `RB_TEX_BACK_DEPTH` (`R32_SFLOAT`).
3. **`RefCounted` `DepthSnapshotGeneration` UAF Prevention, Free Order, & Viewport Eviction**:
   - `DepthSnapshotGeneration` inherits `RefCounted` (`Ref<DepthSnapshotGeneration>`, matching `SphereSnapshot` at [particles_collision_scene_rd.h](../servers/rendering/renderer_rd/storage_rd/particles_collision_scene_rd.h:184)) and owns its own `SafeNumeric<uint32_t> active_leases{ 0 }` alongside `ParticlesCollisionSceneRD::active_lease_counter`, so `DepthReadLease` holds `Ref<DepthSnapshotGeneration>` and never dangles on COW swap or `retired_depth_snapshots` vector growth.
   - **Strict RD Free Order & Transactional Rollback**: `release_gpu_resources()` frees owned `framebuffer_rid` **before** `depth_texture` and `meta_buffer`; if `framebuffer_create` fails during allocation, `depth_texture` and `meta_buffer` are rolled back immediately with `0` leaked RIDs.
   - **Closed-Viewport Eviction**: `RenderSceneBuffersRD::cleanup()` ([render_scene_buffers_rd.cpp](../servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.cpp:134), invoked on resize and `free_data()`) and `RendererViewport::viewport_free` call `invalidate_viewport_depth_snapshot(viewport)`, retiring any leased generation into `retired_depth_snapshots` until `active_leases.get() == 0` and erasing `depth_snapshots_by_viewport[vp_id]`.

---

## 4. Boundaries, Non-Goals & Backend Support

- **Non-Goals**: Cross-emitter `E10` `ParticleAttributeReader` (`FR-PAR-04` Part A) and `.vfx` `engine.scene_depth.collide` IR lowering live exclusively in `ANGE_agy_core` (`FR_particle_par_e10_and_scene_depth.md`). Phase-2 `LateTransparentDepthContent` ([gdextension_scene_late_depth.h](../core/extension/gdextension_scene_late_depth.h:59)) remains intact.
- **Renderer Backends**: Supported on `RendererRD` (`RenderForwardClustered` and `RenderForwardMobile`, single-view `view_count == 1`, `z_near > 0`). Compatibility (`GLES3`) and Dummy (`RendererDummy`) return `ERR_UNAVAILABLE` from `device_session_scene_depth_snapshot_{arm,borrow}_v1` without silent CPU depth readback.

---

## 5. Acceptance Criteria & Relationship to Adjacent Capabilities

1. `SceneDepthSnapshotMeta256` (`sizeof == 256`, `magic == 0x4b303644u`, `version == 2u`, `Projection::inverse()`, `z_near <= 0 -> flags = 2u`) and `GDExtensionSceneDepthSnapshot{Arm,BorrowResult}V1` match `ANGE_agy_core` byte-for-byte.
2. Live Metal RD tests verify `core/extension` C ABI `arm` + `borrow` + automatic `return_after_reads()`, `RefCounted` `DepthSnapshotGeneration` COW safety (resize-while-leased, viewport-free eviction, framebuffer-first free order, transactional rollback), MSAA 4x Clustered + Mobile pre-transparent capture, and point-sampled (`p_bilinear_filtering = false`) silhouette preservation.
3. Relationship to adjacent FRs: Coordinates append order on [gdextension_interface.json](../core/extension/gdextension_interface.json:25860) and disjoint methods in `render_forward_{clustered,mobile}.cpp` with [FR_godot_particle_output_role9_aux_params](FR_godot_particle_output_role9_aux_params.md); independent of [FR_godot_particle_zero_size_and_ribbon_guard](FR_godot_particle_zero_size_and_ribbon_guard.md).
