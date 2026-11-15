/**************************************************************************/
/*  particles_mesh_light_sort_rd.cpp                                      */
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

#include "particles_mesh_light_sort_rd.h"

#include "core/templates/hash_map.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_config.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace RendererRD {

namespace {

uint32_t wrap_subuv_frame_double(double p_floored_frame, uint32_t p_total_frames, SubUVWrapMode p_wrap) {
	if (p_total_frames <= 1u) {
		return 0u;
	}
	if (p_wrap == SubUVWrapMode::REPEAT) {
		double m = std::fmod(p_floored_frame, double(p_total_frames));
		if (m < 0.0) {
			m += double(p_total_frames);
		}
		if (!(m >= 0.0) || m >= double(p_total_frames)) {
			m = 0.0;
		}
		return uint32_t(m);
	}
	if (p_floored_frame <= 0.0) {
		return 0u;
	}
	if (p_floored_frame >= double(p_total_frames - 1u)) {
		return p_total_frames - 1u;
	}
	return uint32_t(p_floored_frame);
}

Vector2 subuv_cell_uv(const Vector2 &p_base_uv, uint32_t p_frame, uint32_t p_cols, uint32_t p_rows) {
	const uint32_t col = p_frame % p_cols;
	const uint32_t row = p_frame / p_cols;
	const float u = (float(col) + CLAMP(p_base_uv.x, 0.0f, 1.0f)) / float(p_cols);
	const float v = (float(row) + CLAMP(p_base_uv.y, 0.0f, 1.0f)) / float(p_rows);
	return Vector2(u, v);
}

} // namespace

ParticlesMeshLightSortRD::~ParticlesMeshLightSortRD() {
	release_translucent_seed_shader();
}

Error ParticlesMeshLightSortRD::_prepare_translucent_seed_shader_impl() {
	if (!RD::get_singleton()) {
		return ERR_UNAVAILABLE;
	}
	if (!translucent_shader_initialized) {
		Vector<String> modes;
		modes.push_back("\n#define MODE_SUBUV_NONE\n");
		modes.push_back("\n#define MODE_SUBUV_STEPPED\n");
		modes.push_back("\n#define MODE_SUBUV_LINEAR_BLEND\n");
		translucent_shader = memnew(ParticlesTranslucentSeedUnlitShaderRD);
		translucent_shader->initialize(modes);
		translucent_shader_initialized = true;
	}
	if (translucent_shader_version.is_null()) {
		translucent_shader_version = translucent_shader->version_create();
		if (translucent_shader_version.is_null()) {
			return ERR_CANT_CREATE;
		}
	}
	return OK;
}

Error ParticlesMeshLightSortRD::prepare_translucent_seed_shader() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs || !rs->is_on_render_thread() || !RD::get_singleton() || rs->get_rendering_device() != RD::get_singleton()) {
		return ERR_UNAVAILABLE;
	}
	return _prepare_translucent_seed_shader_impl();
}

#ifdef TESTS_ENABLED
Error ParticlesMeshLightSortRD::prepare_translucent_seed_shader_standalone_rd() {
	if (RD::get_singleton() == nullptr) {
		return ERR_UNAVAILABLE;
	}
	standalone_rd_test_owner = true;
	const Error err = _prepare_translucent_seed_shader_impl();
	if (err != OK) {
		standalone_rd_test_owner = false;
	}
	return err;
}
#endif

RID ParticlesMeshLightSortRD::get_translucent_seed_shader_variant(SubUVBlendMode p_mode) {
	if (!translucent_shader || translucent_shader_version.is_null()) {
		return RID();
	}
	const int idx = int(p_mode);
	if (idx < 0 || idx > 2) {
		return RID();
	}
	return translucent_shader->version_get_shader(translucent_shader_version, idx);
}

void ParticlesMeshLightSortRD::release_translucent_seed_shader() {
	if (translucent_shader) {
		RenderingServer *rs = RenderingServer::get_singleton();
		bool allowed = (rs && rs->is_on_render_thread() && RD::get_singleton() && rs->get_rendering_device() == RD::get_singleton());
#ifdef TESTS_ENABLED
		if (!allowed && standalone_rd_test_owner && RD::get_singleton() != nullptr) {
			allowed = true;
		}
#endif
		ERR_FAIL_COND_MSG(!allowed, "ParticlesMeshLightSortRD::release_translucent_seed_shader must be called on the active RenderingDevice render thread.");
		if (translucent_shader_version.is_valid()) {
			translucent_shader->version_free(translucent_shader_version);
			translucent_shader_version = RID();
		}
		memdelete(translucent_shader);
		translucent_shader = nullptr;
		translucent_shader_initialized = false;
#ifdef TESTS_ENABLED
		standalone_rd_test_owner = false;
#endif
	}
}

float ParticlesMeshLightSortRD::canonicalize_finite_float(float p_value) {
	uint32_t bits = 0;
	memcpy(&bits, &p_value, sizeof(uint32_t));
	const uint32_t mag = bits & 0x7fffffffu;
	if (mag >= 0x7f800000u || mag < 0x00800000u) {
		return 0.0f;
	}
	return p_value;
}

Instance192Record ParticlesMeshLightSortRD::make_instance192_record(
		const Transform3D &p_transform,
		const Vector3 &p_velocity,
		const Color &p_color,
		float p_subuv_frame,
		uint64_t p_particle_id,
		bool p_has_previous) {
	(void)p_velocity;
	Instance192Record rec{};
	for (int r = 0; r < 3; ++r) {
		rec.transform_rows[r][0] = canonicalize_finite_float(p_transform.basis.rows[r][0]);
		rec.transform_rows[r][1] = canonicalize_finite_float(p_transform.basis.rows[r][1]);
		rec.transform_rows[r][2] = canonicalize_finite_float(p_transform.basis.rows[r][2]);
		rec.transform_rows[r][3] = canonicalize_finite_float(p_transform.origin[r]);
	}
	const float det = p_transform.basis.determinant();
	uint32_t extra_flags = 0u;
	if (Math::is_finite(det) && std::abs(det) > 1e-12f) {
		const Transform3D inv_xf = p_transform.affine_inverse();
		const Basis nrm_basis = inv_xf.basis.transposed();
		for (int c = 0; c < 3; ++c) {
			rec.normal_row0[c] = canonicalize_finite_float(nrm_basis.rows[0][c]);
			rec.normal_row1[c] = canonicalize_finite_float(nrm_basis.rows[1][c]);
			rec.normal_row2[c] = canonicalize_finite_float(nrm_basis.rows[2][c]);
		}
		for (int r = 0; r < 3; ++r) {
			rec.inverse_transform_rows[r][0] = canonicalize_finite_float(inv_xf.basis.rows[r][0]);
			rec.inverse_transform_rows[r][1] = canonicalize_finite_float(inv_xf.basis.rows[r][1]);
			rec.inverse_transform_rows[r][2] = canonicalize_finite_float(inv_xf.basis.rows[r][2]);
			rec.inverse_transform_rows[r][3] = canonicalize_finite_float(inv_xf.origin[r]);
		}
		extra_flags |= Instance192Record::FLAG_INVERSE_AVAILABLE;
		if (det < 0.0f) {
			extra_flags |= Instance192Record::FLAG_NEGATIVE_DETERMINANT;
		}
		const Vector3 col0 = p_transform.basis.get_column(0);
		const Vector3 col1 = p_transform.basis.get_column(1);
		const Vector3 col2 = p_transform.basis.get_column(2);
		const float l0 = col0.length_squared();
		const float l1 = col1.length_squared();
		const float l2 = col2.length_squared();
		const float scale_ref = MAX(MAX(l0, l1), MAX(l2, 1e-12f));
		const float eps = 1e-5f * scale_ref;
		if (std::abs(l0 - l1) > eps || std::abs(l0 - l2) > eps ||
				std::abs(col0.dot(col1)) > eps || std::abs(col0.dot(col2)) > eps || std::abs(col1.dot(col2)) > eps) {
			extra_flags |= Instance192Record::FLAG_NON_UNIFORM_SCALE;
		}
	}
	rec.color[0] = canonicalize_finite_float(p_color.r);
	rec.color[1] = canonicalize_finite_float(p_color.g);
	rec.color[2] = canonicalize_finite_float(p_color.b);
	rec.color[3] = canonicalize_finite_float(p_color.a);
	rec.custom0[1] = canonicalize_finite_float(p_subuv_frame);
	rec.set_particle_id(p_particle_id);
	rec.flags = Instance192Record::FLAG_VALID_RECORD | (p_has_previous ? Instance192Record::FLAG_HAS_PREVIOUS_RECORD : 0u) | extra_flags;
	return rec;
}

Error ParticlesMeshLightSortRD::plan_multisurface_mesh_commands(
		const LocalVector<uint32_t> &p_surface_index_counts,
		uint32_t p_active_instances,
		bool p_guard_faulted,
		LocalVector<MeshSurfaceIndirectCommand20> &r_commands) {
	if (p_surface_index_counts.is_empty() || p_surface_index_counts.size() > ExternalOutputConfig::MAX_FLATTENED_SURFACES) {
		return ERR_INVALID_PARAMETER;
	}
	for (uint32_t i = 0; i < p_surface_index_counts.size(); ++i) {
		if (p_surface_index_counts[i] == 0) {
			return ERR_INVALID_PARAMETER;
		}
	}
	if (p_active_instances > (1u << 20u)) {
		return ERR_PARAMETER_RANGE_ERROR;
	}

	r_commands.resize(p_surface_index_counts.size());
	for (uint32_t i = 0; i < p_surface_index_counts.size(); ++i) {
		MeshSurfaceIndirectCommand20 cmd{};
		if (!p_guard_faulted && p_active_instances > 0) {
			cmd.index_count = p_surface_index_counts[i];
			cmd.instance_count = p_active_instances;
		}
		r_commands[i] = cmd;
	}
	return OK;
}

Error ParticlesMeshLightSortRD::validate_subuv_domain(const SubUVFlipbookSpec &p_spec) {
	if (p_spec.cols == 0 || p_spec.cols > MAX_SUBUV_AXIS ||
			p_spec.rows == 0 || p_spec.rows > MAX_SUBUV_AXIS ||
			uint64_t(p_spec.cols) * uint64_t(p_spec.rows) > MAX_SUBUV_FRAMES) {
		return ERR_INVALID_PARAMETER;
	}
	if (uint32_t(p_spec.blend_mode) > uint32_t(SubUVBlendMode::LINEAR_BLEND) ||
			uint32_t(p_spec.wrap_mode) > uint32_t(SubUVWrapMode::REPEAT)) {
		return ERR_INVALID_PARAMETER;
	}
	return OK;
}

Error ParticlesMeshLightSortRD::evaluate_subuv_sample(
		const SubUVFlipbookSpec &p_spec,
		const Vector2 &p_base_uv,
		float p_frame_coord,
		SubUVSamplePair &r_out) {
	const Error valid = validate_subuv_domain(p_spec);
	if (valid != OK) {
		return valid;
	}
	if (!Math::is_finite(p_frame_coord) || !Math::is_finite(p_base_uv.x) || !Math::is_finite(p_base_uv.y)) {
		return ERR_INVALID_PARAMETER;
	}

	r_out = SubUVSamplePair{};
	if (p_spec.blend_mode == SubUVBlendMode::NONE) {
		r_out.uv0 = Vector2(CLAMP(p_base_uv.x, 0.0f, 1.0f), CLAMP(p_base_uv.y, 0.0f, 1.0f));
		r_out.uv1 = r_out.uv0;
		r_out.blend_alpha = 0.0f;
		return OK;
	}

	const uint32_t total = p_spec.cols * p_spec.rows;
	const double fl = std::floor(double(p_frame_coord));
	r_out.frame0 = wrap_subuv_frame_double(fl, total, p_spec.wrap_mode);
	r_out.uv0 = subuv_cell_uv(p_base_uv, r_out.frame0, p_spec.cols, p_spec.rows);

	if (p_spec.blend_mode == SubUVBlendMode::STEPPED) {
		r_out.frame1 = r_out.frame0;
		r_out.uv1 = r_out.uv0;
		r_out.blend_alpha = 0.0f;
	} else {
		r_out.frame1 = wrap_subuv_frame_double(fl + 1.0, total, p_spec.wrap_mode);
		r_out.uv1 = subuv_cell_uv(p_base_uv, r_out.frame1, p_spec.cols, p_spec.rows);
		const double frac = double(p_frame_coord) - fl;
		r_out.blend_alpha = float(CLAMP(frac, 0.0, 1.0));
	}
	return OK;
}

Error ParticlesMeshLightSortRD::submit_view_frame(
		uint64_t p_view_id,
		uint64_t p_view_epoch,
		uint64_t p_present_seq,
		uint64_t p_camera_capture_ordinal,
		const Vector3 &p_camera_pos,
		const Vector3 &p_camera_forward,
		const LocalVector<Instance192Record> &p_current_records,
		LocalVector<Instance192Record> &r_aligned_previous_out,
		LocalVector<uint32_t> &r_sorted_indices_out) {
	if (p_view_id == 0 || p_view_epoch == 0 || p_present_seq == 0) {
		return ERR_INVALID_PARAMETER;
	}

	MutexLock lock(view_slots_mutex);
	uint32_t slot_idx = UINT32_MAX;
	uint32_t free_idx = UINT32_MAX;
	for (uint32_t i = 0; i < MAX_VIEWS; ++i) {
		if (view_slots[i].occupied && view_slots[i].view_id == p_view_id) {
			slot_idx = i;
			break;
		}
		if (!view_slots[i].occupied && free_idx == UINT32_MAX) {
			free_idx = i;
		}
	}
	if (slot_idx == UINT32_MAX) {
		if (free_idx == UINT32_MAX) {
			return ERR_OUT_OF_MEMORY;
		}
		slot_idx = free_idx;
	}

	ViewConsumedTuple &slot = view_slots[slot_idx];
	if (slot.occupied && slot.paused) {
		return ERR_BUSY;
	}
	if (slot.occupied && p_view_epoch < slot.view_epoch) {
		return ERR_INVALID_DATA;
	}
	if (slot.occupied && p_view_epoch == slot.view_epoch && p_present_seq <= slot.present_seq) {
		return ERR_INVALID_DATA;
	}

	const bool has_valid_epoch_history = slot.occupied && (slot.view_epoch == p_view_epoch);
	const uint32_t count = p_current_records.size();
	r_aligned_previous_out.resize(count);
	r_sorted_indices_out.resize(count);

	scratch_next_retained.resize(count);
	scratch_prev_by_pid.clear();
	if (has_valid_epoch_history && !slot.retained_records.is_empty()) {
		scratch_prev_by_pid.reserve(slot.retained_records.size());
		for (uint32_t j = 0; j < slot.retained_records.size(); ++j) {
			const uint64_t prev_pid = slot.retained_records[j].get_particle_id();
			if (prev_pid != 0 && !scratch_prev_by_pid.has(prev_pid)) {
				scratch_prev_by_pid.insert(prev_pid, j);
			}
		}
	}

	for (uint32_t i = 0; i < count; ++i) {
		Instance192Record cur = p_current_records[i];
		cur.flags = (cur.flags & 31u & ~Instance192Record::FLAG_HAS_PREVIOUS_RECORD) | Instance192Record::FLAG_VALID_RECORD;
		r_aligned_previous_out[i] = cur;

		const uint64_t cur_pid = cur.get_particle_id();
		if (has_valid_epoch_history && cur_pid != 0) {
			const uint32_t *prev_idx = scratch_prev_by_pid.getptr(cur_pid);
			if (prev_idx != nullptr) {
				Instance192Record prev = slot.retained_records[*prev_idx];
				prev.flags = (prev.flags & 31u) | Instance192Record::FLAG_VALID_RECORD | Instance192Record::FLAG_HAS_PREVIOUS_RECORD;
				r_aligned_previous_out[i] = prev;
				cur.flags |= Instance192Record::FLAG_HAS_PREVIOUS_RECORD;
			}
		}
		scratch_next_retained[i] = cur;
		r_sorted_indices_out[i] = i;
	}

	// Deterministic per-view back-to-front sort along camera_forward:
	// Higher distance along view ray comes first (back-to-front alpha compositing);
	// non-finite depths sort to tail (-INFINITY); ties break by persistent 64-bit particle_id, then index.
	Vector3 fwd = p_camera_forward;
	if (Math::is_finite(fwd.x) && Math::is_finite(fwd.y) && Math::is_finite(fwd.z) && fwd.length_squared() > 1e-12f) {
		fwd.normalize();
	} else {
		fwd = Vector3(0.0f, 0.0f, -1.0f);
	}
	const Vector3 cam_pos = (Math::is_finite(p_camera_pos.x) && Math::is_finite(p_camera_pos.y) && Math::is_finite(p_camera_pos.z))
			? p_camera_pos
			: Vector3();

	scratch_sort_keys.resize(count);
	for (uint32_t i = 0; i < count; ++i) {
		const Vector3 tr = scratch_next_retained[i].get_translation();
		const bool tr_finite = Math::is_finite(tr.x) && Math::is_finite(tr.y) && Math::is_finite(tr.z);
		const float raw_d = tr_finite ? (tr - cam_pos).dot(fwd) : NAN;
		scratch_sort_keys[i].view_depth = Math::is_finite(raw_d) ? raw_d : -INFINITY;
		scratch_sort_keys[i].particle_id = scratch_next_retained[i].get_particle_id();
		scratch_sort_keys[i].index = i;
	}
	std::sort(scratch_sort_keys.ptr(), scratch_sort_keys.ptr() + count, [](const SortKey &a, const SortKey &b) {
		if (a.view_depth != b.view_depth) {
			return a.view_depth > b.view_depth; // Back-to-front
		}
		if (a.particle_id != b.particle_id) {
			return a.particle_id < b.particle_id;
		}
		return a.index < b.index;
	});
	for (uint32_t i = 0; i < count; ++i) {
		r_sorted_indices_out[i] = scratch_sort_keys[i].index;
	}

	slot.occupied = true;
	slot.view_id = p_view_id;
	slot.view_epoch = p_view_epoch;
	slot.present_seq = p_present_seq;
	slot.camera_capture_ordinal = p_camera_capture_ordinal;
	slot.camera_position = cam_pos;
	slot.camera_forward = fwd;
	SWAP(slot.retained_records, scratch_next_retained);
	slot.sorted_indices = r_sorted_indices_out;
	return OK;
}

Error ParticlesMeshLightSortRD::set_view_paused(uint64_t p_view_id, bool p_paused) {
	MutexLock lock(view_slots_mutex);
	for (uint32_t i = 0; i < MAX_VIEWS; ++i) {
		if (view_slots[i].occupied && view_slots[i].view_id == p_view_id) {
			view_slots[i].paused = p_paused;
			return OK;
		}
	}
	return ERR_DOES_NOT_EXIST;
}

Error ParticlesMeshLightSortRD::resize_or_epoch_bump_view(uint64_t p_view_id, uint64_t p_new_epoch) {
	MutexLock lock(view_slots_mutex);
	for (uint32_t i = 0; i < MAX_VIEWS; ++i) {
		if (view_slots[i].occupied && view_slots[i].view_id == p_view_id) {
			if (p_new_epoch <= view_slots[i].view_epoch) {
				return ERR_INVALID_PARAMETER;
			}
			view_slots[i].view_epoch = p_new_epoch;
			view_slots[i].retained_records.clear();
			view_slots[i].sorted_indices.clear();
			return OK;
		}
	}
	return ERR_DOES_NOT_EXIST;
}

Error ParticlesMeshLightSortRD::destroy_view(uint64_t p_view_id) {
	MutexLock lock(view_slots_mutex);
	for (uint32_t i = 0; i < MAX_VIEWS; ++i) {
		if (view_slots[i].occupied && view_slots[i].view_id == p_view_id) {
			view_slots[i] = ViewConsumedTuple{};
			return OK;
		}
	}
	return ERR_DOES_NOT_EXIST;
}

const ParticlesMeshLightSortRD::ViewConsumedTuple *ParticlesMeshLightSortRD::get_view_state(uint64_t p_view_id) const {
	MutexLock lock(view_slots_mutex);
	for (uint32_t i = 0; i < MAX_VIEWS; ++i) {
		if (view_slots[i].occupied && view_slots[i].view_id == p_view_id) {
			return &view_slots[i];
		}
	}
	return nullptr;
}

Error ParticlesMeshLightSortRD::compact_particle_lights_deterministic(
		const LocalVector<ParticleLightRecord64> &p_candidates,
		uint32_t p_max_lights,
		LocalVector<ParticleLightRecord64> &r_selected) {
	if (p_max_lights == 0 || p_max_lights > MAX_PARTICLE_LIGHTS) {
		return ERR_INVALID_PARAMETER;
	}

	LocalVector<ParticleLightRecord64> valid;
	valid.reserve(p_candidates.size());
	for (uint32_t i = 0; i < p_candidates.size(); ++i) {
		const ParticleLightRecord64 &c = p_candidates[i];
		if (!Math::is_finite(c.position_world[0]) || !Math::is_finite(c.position_world[1]) || !Math::is_finite(c.position_world[2]) ||
				!Math::is_finite(c.radius) || c.radius <= 0.0f ||
				!Math::is_finite(c.color_hdr[0]) || c.color_hdr[0] < 0.0f ||
				!Math::is_finite(c.color_hdr[1]) || c.color_hdr[1] < 0.0f ||
				!Math::is_finite(c.color_hdr[2]) || c.color_hdr[2] < 0.0f ||
				(c.color_hdr[0] <= 0.0f && c.color_hdr[1] <= 0.0f && c.color_hdr[2] <= 0.0f) ||
				!Math::is_finite(c.attenuation_exp) || c.attenuation_exp < 0.0f ||
				c.screen_importance_q16 == 0) {
			continue;
		}
		valid.push_back(c);
	}

	// Deterministic top-K total order:
	// 1. screen_importance_q16 DESC
	// 2. id_acquire_tag ASC (older persistent particles retain light budget without flicker)
	// 3. id_index ASC
	std::sort(valid.ptr(), valid.ptr() + valid.size(), [](const ParticleLightRecord64 &a, const ParticleLightRecord64 &b) {
		if (a.screen_importance_q16 != b.screen_importance_q16) {
			return a.screen_importance_q16 > b.screen_importance_q16;
		}
		if (a.id_acquire_tag != b.id_acquire_tag) {
			return a.id_acquire_tag < b.id_acquire_tag;
		}
		return a.id_index < b.id_index;
	});

	const uint32_t take = MIN(uint32_t(valid.size()), p_max_lights);
	r_selected.resize(take);
	for (uint32_t i = 0; i < take; ++i) {
		r_selected[i] = valid[i];
	}
	return OK;
}

Error ParticlesMeshLightSortRD::upload_particle_lights_rd(
		RenderingDevice &p_rd,
		uint32_t p_max_lights,
		const LocalVector<ParticleLightRecord64> &p_selected,
		LightClusterBufferRD &r_gpu_lights) {
	if (p_max_lights == 0 || p_max_lights > MAX_PARTICLE_LIGHTS || p_selected.size() > p_max_lights) {
		return ERR_INVALID_PARAMETER;
	}

	if (r_gpu_lights.capacity != p_max_lights || r_gpu_lights.header_buffer.is_null() || r_gpu_lights.records_buffer.is_null()) {
		free_particle_lights_rd(p_rd, r_gpu_lights);
		r_gpu_lights.header_buffer = p_rd.storage_buffer_create(16u);
		if (r_gpu_lights.header_buffer.is_null()) {
			return ERR_CANT_CREATE;
		}
		r_gpu_lights.records_buffer = p_rd.storage_buffer_create(p_max_lights * uint32_t(sizeof(ParticleLightRecord64)));
		if (r_gpu_lights.records_buffer.is_null()) {
			p_rd.free_rid(r_gpu_lights.header_buffer);
			r_gpu_lights.header_buffer = RID();
			return ERR_CANT_CREATE;
		}
		r_gpu_lights.capacity = p_max_lights;
	}

	const uint32_t header_words[4] = { p_selected.size(), p_max_lights, 0u, 0u };
	Error err = p_rd.buffer_update(r_gpu_lights.header_buffer, 0, sizeof(header_words), header_words);
	if (err != OK) {
		return err;
	}
	if (!p_selected.is_empty()) {
		err = p_rd.buffer_update(r_gpu_lights.records_buffer, 0, p_selected.size() * sizeof(ParticleLightRecord64), p_selected.ptr());
		if (err != OK) {
			return err;
		}
	}
	r_gpu_lights.active_count = p_selected.size();
	return OK;
}

void ParticlesMeshLightSortRD::free_particle_lights_rd(
		RenderingDevice &p_rd,
		LightClusterBufferRD &r_gpu_lights) {
	if (r_gpu_lights.records_buffer.is_valid()) {
		p_rd.free_rid(r_gpu_lights.records_buffer);
		r_gpu_lights.records_buffer = RID();
	}
	if (r_gpu_lights.header_buffer.is_valid()) {
		p_rd.free_rid(r_gpu_lights.header_buffer);
		r_gpu_lights.header_buffer = RID();
	}
	r_gpu_lights.capacity = 0;
	r_gpu_lights.active_count = 0;
}

Error ParticlesMeshLightSortRD::publish_active_particle_lights(
		const LocalVector<ParticleLightRecord64> &p_candidates,
		uint32_t p_max_lights) {
	LocalVector<ParticleLightRecord64> compacted;
	const Error err = compact_particle_lights_deterministic(p_candidates, p_max_lights, compacted);
	if (err != OK) {
		return err;
	}
	MutexLock lock(active_lights_mutex);
	active_particle_lights = compacted;
	return OK;
}

void ParticlesMeshLightSortRD::clear_active_particle_lights() {
	MutexLock lock(active_lights_mutex);
	active_particle_lights.clear();
}

void ParticlesMeshLightSortRD::get_active_particle_lights(LocalVector<ParticleLightRecord64> &r_out) const {
	MutexLock lock(active_lights_mutex);
	r_out = active_particle_lights;
}

uint32_t ParticlesMeshLightSortRD::get_active_particle_light_count() const {
	MutexLock lock(active_lights_mutex);
	return active_particle_lights.size();
}

} // namespace RendererRD
