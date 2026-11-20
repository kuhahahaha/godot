/**************************************************************************/
/*  particles_mesh_light_sort_rd.h                                        */
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
#include "core/math/transform_3d.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "core/templates/rid.h"
#include "servers/rendering/renderer_rd/shaders/particles_translucent_seed_unlit.glsl.gen.h"

#include <cstdint>
#include <type_traits>

class ClusterBuilderRD;
class RenderingDevice;

namespace RendererRD {

class ExternalOutputOwner;

struct Instance192Record {
	static constexpr uint32_t FLAG_VALID_RECORD = 1u;
	static constexpr uint32_t FLAG_HAS_PREVIOUS_RECORD = 2u;
	static constexpr uint32_t FLAG_NEGATIVE_DETERMINANT = 4u;
	static constexpr uint32_t FLAG_NON_UNIFORM_SCALE = 8u;
	static constexpr uint32_t FLAG_INVERSE_AVAILABLE = 16u;

	float transform_rows[3][4] = {}; // words 0..11 (model)
	float normal_row0[3] = { 1.0f, 0.0f, 0.0f }; // words 12..14
	uint32_t pad0 = 0; // word 15 (must be 0)
	float normal_row1[3] = { 0.0f, 1.0f, 0.0f }; // words 16..18
	uint32_t pad1 = 0; // word 19 (must be 0)
	float normal_row2[3] = { 0.0f, 0.0f, 1.0f }; // words 20..22
	uint32_t pad2 = 0; // word 23 (must be 0)
	float inverse_transform_rows[3][4] = {}; // words 24..35 (inverse_model)
	float color[4] = {}; // words 36..39
	float custom0[4] = {}; // words 40..43 (INSTANCE_CUSTOM; custom0[1] = subuv frame_coord)
	uint32_t particle_id_lo = 0; // word 44
	uint32_t particle_id_hi = 0; // word 45
	uint32_t flags = FLAG_VALID_RECORD; // word 46
	uint32_t reserved_zero = 0; // word 47 (must be 0)

	void set_particle_id(uint64_t p_id) {
		particle_id_lo = uint32_t(p_id & 0xffffffffu);
		particle_id_hi = uint32_t(p_id >> 32u);
	}
	uint64_t get_particle_id() const {
		return (uint64_t(particle_id_hi) << 32u) | uint64_t(particle_id_lo);
	}
	Vector3 get_translation() const {
		return Vector3(transform_rows[0][3], transform_rows[1][3], transform_rows[2][3]);
	}
};

struct ParticleLightRecord64 {
	static constexpr uint32_t FLAG_ACTIVE = 1u;
	static constexpr uint32_t FLAG_AFFECTS_TRANSLUCENCY = 2u;
	static constexpr uint32_t FLAG_KNOWN_MASK = 3u;

	float position_world[3] = {}; // words 0..2  (0..11B, world-space)
	float radius = 0.0f; // word  3     (12..15B, world-space > 0)
	float color_hdr[3] = {}; // words 4..6  (16..27B, linear RGB HDR energy)
	float attenuation_exp = 1.0f; // word  7     (28..31B, +2.0f when inverse_squared else >= 0)
	uint32_t id_index = 0; // word  8     (32..35B)
	uint32_t id_acquire_tag = 0; // word  9     (36..39B)
	uint32_t flags = FLAG_ACTIVE; // word  10    (40..43B, bit0=active, bit1=affects_translucency)
	uint32_t screen_importance_q16 = 0; // word  11    (44..47B, [1..65535] for active lights)
	float specular_scale = 1.0f; // word  12    (48..51B)
	float diffuse_scale = 1.0f; // word  13    (52..55B)
	float volumetric_scattering = 1.0f; // word  14    (56..59B)
	uint32_t reserved0 = 0; // word  15    (60..63B, must be 0u)
};

struct MeshSurfaceIndirectCommand20 {
	uint32_t index_count = 0;
	uint32_t instance_count = 0;
	uint32_t first_index = 0;
	int32_t vertex_offset = 0;
	uint32_t first_instance = 0;
};

static_assert(std::is_standard_layout_v<Instance192Record> && sizeof(Instance192Record) == 192);
static_assert(std::is_standard_layout_v<ParticleLightRecord64> && sizeof(ParticleLightRecord64) == 64);
static_assert(offsetof(ParticleLightRecord64, id_index) == 32);
static_assert(offsetof(ParticleLightRecord64, id_acquire_tag) == 36);
static_assert(offsetof(ParticleLightRecord64, flags) == 40);
static_assert(offsetof(ParticleLightRecord64, screen_importance_q16) == 44);
static_assert(offsetof(ParticleLightRecord64, specular_scale) == 48);
static_assert(offsetof(ParticleLightRecord64, diffuse_scale) == 52);
static_assert(offsetof(ParticleLightRecord64, volumetric_scattering) == 56);
static_assert(offsetof(ParticleLightRecord64, reserved0) == 60);
static_assert(std::is_standard_layout_v<MeshSurfaceIndirectCommand20> && sizeof(MeshSurfaceIndirectCommand20) == 20);

enum class SubUVBlendMode : uint32_t {
	NONE = 0,
	STEPPED = 1,
	LINEAR_BLEND = 2,
};

enum class SubUVWrapMode : uint32_t {
	CLAMP = 0,
	REPEAT = 1,
};

struct SubUVFlipbookSpec {
	uint32_t cols = 1;
	uint32_t rows = 1;
	SubUVBlendMode blend_mode = SubUVBlendMode::NONE;
	SubUVWrapMode wrap_mode = SubUVWrapMode::CLAMP;
};

struct SubUVSamplePair {
	uint32_t frame0 = 0;
	uint32_t frame1 = 0;
	Vector2 uv0;
	Vector2 uv1;
	float blend_alpha = 0.0f;
};

class ParticlesMeshLightSortRD {
public:
	static constexpr uint32_t MAX_VIEWS = 8u;
	static constexpr uint32_t MAX_PARTICLE_LIGHTS = 256u;
	static constexpr uint32_t MAX_STAGED_LIGHT_CANDIDATES = 512u;
	static constexpr uint32_t MAX_SUBUV_AXIS = 64u;
	static constexpr uint32_t MAX_SUBUV_FRAMES = 4096u;

	struct ViewConsumedTuple {
		uint64_t view_id = 0;
		uint64_t view_epoch = 0;
		uint64_t present_seq = 0;
		uint64_t camera_capture_ordinal = 0;
		Vector3 camera_position;
		Vector3 camera_forward;
		bool occupied = false;
		bool paused = false;
		LocalVector<Instance192Record> retained_records;
		LocalVector<uint32_t> sorted_indices;
	};

	struct LightClusterBufferRD {
		RID header_buffer;
		RID records_buffer;
		uint32_t capacity = 0;
		uint32_t active_count = 0;
	};

	struct ExternalLightSlotRegistration {
		using SlotLiveCheckFn = bool (*)(const ExternalOutputOwner *p_owner, uint64_t p_generation, uint32_t p_slot_index, uint64_t p_lease_serial);
		uint64_t owner_key = 0;
		const ExternalOutputOwner *owner_ptr = nullptr;
		SlotLiveCheckFn is_slot_live_fn = nullptr;
		uint64_t generation = 0;
		uint32_t slot_index = UINT32_MAX;
		uint64_t lease_serial = 0;
		RID scenario_rid;
		RID viewport_rid;
		AABB world_bounds;
		RID records_rid;
		RID meta_rid;
		RID harvest_uniform_set;
		uint32_t capacity = 0;
		uint64_t present_seq = 1u;
	};

	struct ReservedGpuLightBounds {
		AABB world_bounds;
		uint32_t first_buffer_index = 0;
		uint32_t count = 0;
	};

private:
	static constexpr uint32_t LIGHT_DATA_WORD_STRIDE = 56u; // sizeof(LightStorage::LightData) == 224B
	static constexpr uint32_t CLUSTER_ELEMENT_WORD_STRIDE = 20u; // sizeof(ClusterBuilderRD::RenderElementData) == 80B
	static constexpr uint32_t UNIFORM_SET_CACHE_CAPACITY = 32u;

	struct SortKey {
		float view_depth = 0.0f;
		uint64_t particle_id = 0;
		uint32_t index = 0;
	};

	struct ExternalOwnerLightState {
		ExternalLightSlotRegistration pending_slot;
		bool has_pending_rd_slot = false;
		LocalVector<ParticleLightRecord64> compacted_lights;
	};

	struct CachedLightUniformSet {
		RID shader;
		RID rids[4];
		RID uniform_set;
		uint64_t last_used_tick = 0;
	};

	struct ClusterFrameContext {
		const ClusterBuilderRD *builder = nullptr;
		Transform3D view_xform;
		float z_near = 0.05f;
		float z_far = 4000.0f;
		float sphere_overfit = 1.05f;
		uint32_t max_elements_by_type = 0;
		uint32_t pending_gpu_omni_count = 0;
		bool camera_orthogonal = false;
		bool active = false;
	};

	mutable Mutex view_slots_mutex;
	ViewConsumedTuple view_slots[MAX_VIEWS];
	LocalVector<Instance192Record> scratch_next_retained;
	HashMap<uint64_t, uint32_t> scratch_prev_by_pid;
	LocalVector<SortKey> scratch_sort_keys;
	mutable Mutex active_lights_mutex;
	LocalVector<ParticleLightRecord64> active_particle_lights;
	HashMap<uint64_t, ExternalOwnerLightState> external_owner_lights;
	RenderingDevice *light_gpu_bridge_rd = nullptr;
	RID light_harvest_shader;
	RID light_harvest_pipeline;
	RID light_topk_emit_shader;
	RID light_topk_emit_pipeline;
	RID light_cluster_blit_shader;
	RID light_cluster_blit_pipeline;
	RID light_candidate_staging_buffer;
	RID light_cluster_elements_buffer;
	RID light_emit_params_ubo;
	CachedLightUniformSet light_uniform_set_cache[UNIFORM_SET_CACHE_CAPACITY];
	uint64_t light_uniform_set_tick = 0;
	ClusterFrameContext active_cluster_context;
	ParticlesTranslucentSeedUnlitShaderRD *translucent_shader = nullptr;
	RID translucent_shader_version;
	bool translucent_shader_initialized = false;
#ifdef TESTS_ENABLED
	bool standalone_rd_test_owner = false;
#endif
	Error _prepare_translucent_seed_shader_impl();
	Error _ensure_light_gpu_bridge(RenderingDevice *p_rd);
	void _release_light_gpu_bridge(RenderingDevice *p_rd);
	RID _acquire_cached_uniform_set(RenderingDevice *p_rd, RID p_shader, const RD::Uniform *p_uniforms, uint32_t p_count);

public:
	ParticlesMeshLightSortRD() = default;
	~ParticlesMeshLightSortRD();

	// Translucent unlit seed shader lifecycle (main RD render thread):
	Error prepare_translucent_seed_shader();
#ifdef TESTS_ENABLED
	Error prepare_translucent_seed_shader_standalone_rd();
#endif
	RID get_translucent_seed_shader_variant(SubUVBlendMode p_mode);
	void release_translucent_seed_shader();

	// Subnormal-free float canonicalization & Instance192 builder:
	static float canonicalize_finite_float(float p_value);
	static bool flush_subnormal_and_neg_zero(float p_value, float &r_out);
	static Instance192Record make_canonical_degenerate_instance192(
			const Vector3 &p_translation_world,
			const Color &p_uv_rect,
			uint64_t p_particle_id,
			bool p_has_previous);
	static Error build_instance192_record_checked(
			const Transform3D &p_transform,
			const Color &p_color,
			const Color &p_custom_uv_rect,
			uint64_t p_particle_id,
			bool p_has_previous,
			bool p_allow_zero_scale_degenerate,
			Instance192Record &r_record);
	static Instance192Record make_instance192_record(
			const Transform3D &p_transform,
			const Vector3 &p_velocity,
			const Color &p_color,
			float p_subuv_frame,
			uint64_t p_particle_id,
			bool p_has_previous);

	// Multi-surface mesh indirect command planner (1..MAX_FLATTENED_SURFACES):
	static Error plan_multisurface_mesh_commands(
			const LocalVector<uint32_t> &p_surface_index_counts,
			uint32_t p_active_instances,
			bool p_guard_faulted,
			LocalVector<MeshSurfaceIndirectCommand20> &r_commands);

	// Sub-UV domain validation & dual-frame evaluator:
	static Error validate_subuv_domain(const SubUVFlipbookSpec &p_spec);
	static Error evaluate_subuv_sample(
			const SubUVFlipbookSpec &p_spec,
			const Vector2 &p_base_uv,
			float p_frame_coord,
			SubUVSamplePair &r_out);

	// Per-view E14-E16 history & back-to-front sort isolation (A06):
	Error submit_view_frame(
			uint64_t p_view_id,
			uint64_t p_view_epoch,
			uint64_t p_present_seq,
			uint64_t p_camera_capture_ordinal,
			const Vector3 &p_camera_pos,
			const Vector3 &p_camera_forward,
			const LocalVector<Instance192Record> &p_current_records,
			LocalVector<Instance192Record> &r_aligned_previous_out,
			LocalVector<uint32_t> &r_sorted_indices_out);
	Error set_view_paused(uint64_t p_view_id, bool p_paused);
	Error resize_or_epoch_bump_view(uint64_t p_view_id, uint64_t p_new_epoch);
	Error destroy_view(uint64_t p_view_id);
	const ViewConsumedTuple *get_view_state(uint64_t p_view_id) const;

	// Deterministic top-K GPU particle light compaction (0 flicker) + RD buffer seam:
	static Error compact_particle_lights_deterministic(
			const LocalVector<ParticleLightRecord64> &p_candidates,
			uint32_t p_max_lights,
			LocalVector<ParticleLightRecord64> &r_selected);

	static Error upload_particle_lights_rd(
			RenderingDevice &p_rd,
			uint32_t p_max_lights,
			const LocalVector<ParticleLightRecord64> &p_selected,
			LightClusterBufferRD &r_gpu_lights);

	static void free_particle_lights_rd(
			RenderingDevice &p_rd,
			LightClusterBufferRD &r_gpu_lights);

	// Active compacted particle omni-light publication consumed by LightStorage /
	// Forward+ ClusterBuilderRD & Mobile omni-light pairing:
	Error publish_active_particle_lights(
			const LocalVector<ParticleLightRecord64> &p_candidates,
			uint32_t p_max_lights = MAX_PARTICLE_LIGHTS);
	void register_sealed_external_light_slot(const ExternalLightSlotRegistration &p_slot);
	bool update_external_light_slot_present_seq(uint64_t p_owner_key, uint64_t p_generation, uint32_t p_slot_index, uint64_t p_lease_serial, uint64_t p_next_present_seq);
	void unregister_external_light_slot(uint64_t p_owner_key, uint64_t p_generation, uint32_t p_slot_index);
	void unregister_external_light_owner(uint64_t p_owner_key);
	Error upsert_external_role8_packet(
			uint64_t p_owner_key,
			const uint8_t *p_meta32,
			size_t p_meta_bytes,
			const uint8_t *p_role8,
			size_t p_role8_bytes,
			uint32_t p_capacity,
			uint64_t p_present_seq = 1u);
	uint32_t dispatch_registered_external_lights_gpu(
			RenderingDevice *p_rd,
			RID p_source_viewport,
			RID p_source_scenario,
			bool p_is_reflection_probe,
			const Transform3D &p_view_inv_transform,
			float p_exposure_norm,
			RID p_omni_light_buffer,
			uint32_t p_base_omni_index,
			uint32_t p_max_omni_lights,
			LocalVector<ReservedGpuLightBounds> &r_mobile_bounds_out);
	void begin_cluster_builder_frame(
			const ClusterBuilderRD *p_builder,
			const Transform3D &p_view_xform,
			float p_z_near,
			float p_z_far,
			bool p_camera_orthogonal,
			float p_sphere_overfit,
			uint32_t p_max_elements_by_type);
	uint32_t get_pending_cluster_gpu_omni_count(const ClusterBuilderRD *p_builder) const;
	void blit_cluster_gpu_omni_elements(
			RenderingDevice *p_rd,
			const ClusterBuilderRD *p_builder,
			RID p_dst_element_buffer,
			uint32_t p_dst_element_offset,
			uint32_t p_count);
	void clear_cluster_builder_frame(const ClusterBuilderRD *p_builder);
	Error ensure_light_gpu_bridge(RenderingDevice *p_rd) { return _ensure_light_gpu_bridge(p_rd); }
	RID get_light_harvest_shader() const { return light_harvest_shader; }
	RID get_light_candidate_staging_buffer() const { return light_candidate_staging_buffer; }
	void release_light_gpu_bridge(RenderingDevice *p_rd) { _release_light_gpu_bridge(p_rd); }
	void clear_active_particle_lights();
	void get_active_particle_lights(LocalVector<ParticleLightRecord64> &r_out) const;
	uint32_t get_active_particle_light_count() const;
};

} // namespace RendererRD
