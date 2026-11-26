/**************************************************************************/
/*  particles_decal_bridge_rd.h                                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/error/error_list.h"
#include "core/math/aabb.h"
#include "core/math/rect2.h"
#include "core/math/transform_3d.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "core/templates/rid.h"
#include "servers/rendering/rendering_device.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

class ClusterBuilderRD;

namespace RendererRD {

class ExternalOutputOwner;

struct alignas(16) ParticleDecalRecord128 {
	static constexpr uint32_t FLAG_ACTIVE = 1u;
	static constexpr uint32_t FLAG_HAS_SUBUV = 2u;
	static constexpr uint32_t FLAG_KNOWN_MASK = 3u;

	float world_position[3] = {}; // words 0..2  (0..11B)
	float albedo_mix = 1.0f; // word  3     (12..15B, [0..1])
	float orientation_quat[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // words 4..7  (16..31B, unit xyzw, rotated +Y = projection normal)
	float size_world[3] = { 1.0f, 1.0f, 1.0f }; // words 8..10 (32..43B, full box extents matching Godot Decal.size)
	float emission_energy = 1.0f; // word  11    (44..47B, >= 0)
	float color_rgba[4] = { 1.0f, 1.0f, 1.0f, 1.0f }; // words 12..15 (48..63B, linear RGBA modulate)
	float upper_fade = 0.3f; // word  16    (64..67B, >= 1e-5)
	float lower_fade = 0.3f; // word  17    (68..71B, >= 1e-5)
	float normal_fade = 0.0f; // word  18    (72..75B, [0..1])
	uint32_t cull_mask = 0x000FFFFFu; // word  19    (76..79B, 20-bit visual layer mask)
	uint32_t id_index = 0u; // word  20    (80..83B)
	uint32_t id_acquire_tag = 0u; // word  21    (84..87B)
	uint32_t flags = FLAG_ACTIVE; // word  22    (88..91B, bit0=active, bit1=has_subuv)
	uint32_t screen_importance_q16 = 0u; // word  23    (92..95B, [1..65535])
	float uv_subrect[4] = { 0.0f, 0.0f, 1.0f, 1.0f }; // words 24..27 (96..111B, u_off, v_off, u_scale, v_scale)
	float distance_fade_begin = 0.0f; // word  28    (112..115B, 0 = off)
	float distance_fade_length = 0.0f; // word  29    (116..119B)
	uint32_t sort_order_key = 0u; // word  30    (120..123B)
	uint32_t reserved0 = 0u; // word  31    (124..127B, must be 0u)
};

static_assert(std::is_standard_layout_v<ParticleDecalRecord128> && sizeof(ParticleDecalRecord128) == 128 && alignof(ParticleDecalRecord128) == 16);
static_assert(offsetof(ParticleDecalRecord128, world_position) == 0);
static_assert(offsetof(ParticleDecalRecord128, albedo_mix) == 12);
static_assert(offsetof(ParticleDecalRecord128, orientation_quat) == 16);
static_assert(offsetof(ParticleDecalRecord128, size_world) == 32);
static_assert(offsetof(ParticleDecalRecord128, emission_energy) == 44);
static_assert(offsetof(ParticleDecalRecord128, color_rgba) == 48);
static_assert(offsetof(ParticleDecalRecord128, upper_fade) == 64);
static_assert(offsetof(ParticleDecalRecord128, lower_fade) == 68);
static_assert(offsetof(ParticleDecalRecord128, normal_fade) == 72);
static_assert(offsetof(ParticleDecalRecord128, cull_mask) == 76);
static_assert(offsetof(ParticleDecalRecord128, id_index) == 80);
static_assert(offsetof(ParticleDecalRecord128, id_acquire_tag) == 84);
static_assert(offsetof(ParticleDecalRecord128, flags) == 88);
static_assert(offsetof(ParticleDecalRecord128, screen_importance_q16) == 92);
static_assert(offsetof(ParticleDecalRecord128, uv_subrect) == 96);
static_assert(offsetof(ParticleDecalRecord128, distance_fade_begin) == 112);
static_assert(offsetof(ParticleDecalRecord128, distance_fade_length) == 116);
static_assert(offsetof(ParticleDecalRecord128, sort_order_key) == 120);
static_assert(offsetof(ParticleDecalRecord128, reserved0) == 124);

class ParticlesDecalBridgeRD {
public:
	static constexpr uint32_t MAX_PARTICLE_DECALS = 256u;
	static constexpr uint32_t MAX_STAGED_DECAL_CANDIDATES = 8192u;
	static constexpr uint32_t MAX_DISPATCH_EMITTERS = 256u;
	static constexpr uint32_t TEXTURE_SLOT_COUNT = 4u; // 0=albedo, 1=normal, 2=orm, 3=emission

	using AtlasRectLookupFn = Rect2 (*)(void *p_ctx, RID p_texture);

	struct ExternalDecalSlotRegistration {
		using SlotLiveCheckFn = bool (*)(const ExternalOutputOwner *p_owner, uint64_t p_generation, uint32_t p_slot_index, uint64_t p_lease_serial);
		uint64_t owner_key = 0;
		const ExternalOutputOwner *owner_ptr = nullptr;
		SlotLiveCheckFn is_slot_live_fn = nullptr;
		SlotLiveCheckFn is_slot_visible_fn = nullptr;
		uint64_t generation = 0;
		uint32_t slot_index = UINT32_MAX;
		uint64_t lease_serial = 0;
		RID scenario_rid;
		RID viewport_rid;
		AABB world_bounds;
		RID records_rid;
		RID meta_rid;
		RID harvest_uniform_set;
		RID textures[TEXTURE_SLOT_COUNT];
		uint32_t capacity = 0;
		uint64_t present_seq = 1u;
	};

	struct ExternalDecalSlotPresentSeqUpdate {
		uint64_t owner_key = 0;
		uint64_t generation = 0;
		uint32_t slot_index = UINT32_MAX;
		uint64_t lease_serial = 0;
		uint64_t next_present_seq = 0;
		AABB world_bounds;
		bool has_world_bounds = false;
	};

	struct ReservedGpuDecalBounds {
		AABB world_bounds;
		uint32_t first_buffer_index = 0;
		uint32_t count = 0;
	};

private:
	static constexpr uint32_t DECAL_DATA_WORD_STRIDE = 60u; // sizeof(TextureStorage::DecalData) == 240B
	static constexpr uint32_t CLUSTER_ELEMENT_WORD_STRIDE = 20u; // sizeof(ClusterBuilderRD::RenderElementData) == 80B
	static constexpr uint32_t UNIFORM_SET_CACHE_CAPACITY = 64u;

	struct ExternalOwnerDecalState {
		ExternalDecalSlotRegistration pending_slot;
		bool has_pending_rd_slot = false;
	};

	struct CachedDecalUniformSet {
		RID shader;
		RID rids[5];
		RID uniform_set;
		uint64_t last_used_tick = 0;
	};

	struct ClusterFrameContext {
		const ClusterBuilderRD *builder = nullptr;
		Transform3D view_xform;
		float z_near = 0.05f;
		float z_far = 4000.0f;
		uint32_t max_elements_by_type = 0;
		uint32_t pending_gpu_decal_count = 0;
		bool camera_orthogonal = false;
		bool flip_y = true;
		bool active = false;
	};

	mutable Mutex active_decals_mutex;
	HashMap<uint64_t, ExternalOwnerDecalState> external_owner_decals;
	RenderingDevice *decal_gpu_bridge_rd = nullptr;
	RID decal_harvest_shader;
	RID decal_harvest_pipeline;
	RID decal_topk_emit_shader;
	RID decal_topk_emit_pipeline;
	RID decal_cluster_blit_shader;
	RID decal_cluster_blit_pipeline;
	RID decal_candidate_staging_buffer;
	RID decal_cluster_elements_buffer;
	RID decal_emit_params_ubo;
	RID decal_emitter_atlas_rects_buffer;
	CachedDecalUniformSet decal_uniform_set_cache[UNIFORM_SET_CACHE_CAPACITY];
	uint64_t decal_uniform_set_tick = 0;
	uint32_t last_dispatch_barrier_count = 0;
	ClusterFrameContext active_cluster_context;

	Error _ensure_decal_gpu_bridge(RenderingDevice *p_rd);
	void _release_decal_gpu_bridge(RenderingDevice *p_rd);
	RID _acquire_cached_uniform_set(RenderingDevice *p_rd, RID p_shader, const RD::Uniform *p_uniforms, uint32_t p_count);

public:
	ParticlesDecalBridgeRD() = default;
	~ParticlesDecalBridgeRD();

	static Error compact_particle_decals_deterministic(
			const LocalVector<ParticleDecalRecord128> &p_candidates,
			uint32_t p_max_decals,
			LocalVector<ParticleDecalRecord128> &r_selected);

	void register_sealed_external_decal_slot(const ExternalDecalSlotRegistration &p_slot);
	bool update_external_decal_slot_present_seq(
			uint64_t p_owner_key,
			uint64_t p_generation,
			uint32_t p_slot_index,
			uint64_t p_lease_serial,
			uint64_t p_next_present_seq,
			const AABB *p_world_bounds = nullptr);
	uint32_t update_external_decal_slots_present_seq_batch(const ExternalDecalSlotPresentSeqUpdate *p_updates, uint32_t p_count);
	void unregister_external_decal_slot(uint64_t p_owner_key, uint64_t p_generation, uint32_t p_slot_index);
	void unregister_external_decal_owner(uint64_t p_owner_key);
	void clear_registered_external_decals();

	uint32_t dispatch_registered_external_decals_gpu(
			RenderingDevice *p_rd,
			RID p_source_viewport,
			RID p_source_scenario,
			bool p_is_reflection_probe,
			const Transform3D &p_camera_xform,
			RID p_decal_buffer,
			uint32_t p_base_decal_index,
			uint32_t p_max_decals,
			AtlasRectLookupFn p_atlas_rect_lookup,
			void *p_atlas_rect_ctx,
			LocalVector<ReservedGpuDecalBounds> &r_mobile_bounds_out);

	void begin_cluster_builder_frame(
			const ClusterBuilderRD *p_builder,
			const Transform3D &p_view_xform,
			float p_z_near,
			float p_z_far,
			bool p_camera_orthogonal,
			bool p_flip_y,
			uint32_t p_max_elements_by_type);
	uint32_t get_pending_cluster_gpu_decal_count(const ClusterBuilderRD *p_builder) const;
	void blit_cluster_gpu_decal_elements(
			RenderingDevice *p_rd,
			const ClusterBuilderRD *p_builder,
			RID p_dst_element_buffer,
			uint32_t p_dst_element_offset,
			uint32_t p_count);
	void clear_cluster_builder_frame(const ClusterBuilderRD *p_builder);

	Error ensure_decal_gpu_bridge(RenderingDevice *p_rd) { return _ensure_decal_gpu_bridge(p_rd); }
	void release_decal_gpu_bridge(RenderingDevice *p_rd) { _release_decal_gpu_bridge(p_rd); }
	RID get_decal_harvest_shader() const { return decal_harvest_shader; }
	RID get_decal_candidate_staging_buffer() const { return decal_candidate_staging_buffer; }
	RID get_decal_cluster_elements_buffer() const { return decal_cluster_elements_buffer; }
	uint32_t get_last_dispatch_barrier_count() const { return last_dispatch_barrier_count; }
};

} // namespace RendererRD
