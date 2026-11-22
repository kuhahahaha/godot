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
	if (light_gpu_bridge_rd != nullptr && light_gpu_bridge_rd == RD::get_singleton()) {
		_release_light_gpu_bridge(light_gpu_bridge_rd);
	}
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
	if (light_gpu_bridge_rd != nullptr && light_gpu_bridge_rd == RD::get_singleton()) {
		_release_light_gpu_bridge(light_gpu_bridge_rd);
	}
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
		uint32_t zero_bits = 0u;
		float zero_f = 0.0f;
		memcpy(&zero_f, &zero_bits, sizeof(float));
		return zero_f;
	}
	return p_value;
}

bool ParticlesMeshLightSortRD::flush_subnormal_and_neg_zero(float p_value, float &r_out) {
	uint32_t bits = 0;
	memcpy(&bits, &p_value, sizeof(uint32_t));
	const uint32_t exp_bits = bits & 0x7f800000u;
	if (exp_bits == 0x7f800000u) {
		uint32_t zero_bits = 0u;
		memcpy(&r_out, &zero_bits, sizeof(float));
		return false;
	}
	if (exp_bits == 0u) {
		uint32_t zero_bits = 0u;
		memcpy(&r_out, &zero_bits, sizeof(float));
		return true;
	}
	r_out = p_value;
	return true;
}

Instance192Record ParticlesMeshLightSortRD::make_canonical_degenerate_instance192(
		const Vector3 &p_translation_world,
		const Color &p_uv_rect,
		uint64_t p_particle_id,
		bool p_has_previous) {
	Instance192Record rec;
	memset(&rec, 0, sizeof(Instance192Record));

	float tx = 0.0f;
	float ty = 0.0f;
	float tz = 0.0f;
	float neg_tx = 0.0f;
	float neg_ty = 0.0f;
	float neg_tz = 0.0f;
	float uv0 = 0.0f;
	float uv1 = 0.0f;
	float uv2 = 0.0f;
	float uv3 = 0.0f;
	if (!flush_subnormal_and_neg_zero(p_translation_world.x, tx) ||
			!flush_subnormal_and_neg_zero(p_translation_world.y, ty) ||
			!flush_subnormal_and_neg_zero(p_translation_world.z, tz) ||
			!flush_subnormal_and_neg_zero(-tx, neg_tx) ||
			!flush_subnormal_and_neg_zero(-ty, neg_ty) ||
			!flush_subnormal_and_neg_zero(-tz, neg_tz) ||
			!flush_subnormal_and_neg_zero(p_uv_rect.r, uv0) ||
			!flush_subnormal_and_neg_zero(p_uv_rect.g, uv1) ||
			!flush_subnormal_and_neg_zero(p_uv_rect.b, uv2) ||
			!flush_subnormal_and_neg_zero(p_uv_rect.a, uv3)) {
		memset(&rec, 0, sizeof(Instance192Record));
		return rec;
	}

	rec.transform_rows[0][3] = tx;
	rec.transform_rows[1][3] = ty;
	rec.transform_rows[2][3] = tz;

	rec.normal_row0[0] = 1.0f;
	rec.normal_row1[1] = 1.0f;
	rec.normal_row2[2] = 1.0f;

	rec.inverse_transform_rows[0][0] = 1.0f;
	rec.inverse_transform_rows[0][3] = neg_tx;
	rec.inverse_transform_rows[1][1] = 1.0f;
	rec.inverse_transform_rows[1][3] = neg_ty;
	rec.inverse_transform_rows[2][2] = 1.0f;
	rec.inverse_transform_rows[2][3] = neg_tz;

	rec.custom0[0] = uv0;
	rec.custom0[1] = uv1;
	rec.custom0[2] = uv2;
	rec.custom0[3] = uv3;
	rec.set_particle_id(p_particle_id);
	rec.flags = Instance192Record::FLAG_VALID_RECORD |
			Instance192Record::FLAG_INVERSE_AVAILABLE |
			(p_has_previous ? Instance192Record::FLAG_HAS_PREVIOUS_RECORD : 0u);
	rec.reserved_zero = 0u;
	return rec;
}

Error ParticlesMeshLightSortRD::build_instance192_record_checked(
		const Transform3D &p_transform,
		const Color &p_color,
		const Color &p_custom_uv_rect,
		uint64_t p_particle_id,
		bool p_has_previous,
		bool p_allow_zero_scale_degenerate,
		Instance192Record &r_record) {
	memset(&r_record, 0, sizeof(Instance192Record));

	float flushed_basis[3][3] = {};
	float flushed_origin[3] = {};
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			if (!flush_subnormal_and_neg_zero(p_transform.basis.rows[r][c], flushed_basis[r][c])) {
				return ERR_INVALID_PARAMETER;
			}
		}
		if (!flush_subnormal_and_neg_zero(p_transform.origin[r], flushed_origin[r])) {
			return ERR_INVALID_PARAMETER;
		}
	}

	float flushed_color[4] = {};
	float flushed_custom[4] = {};
	if (!flush_subnormal_and_neg_zero(p_color.r, flushed_color[0]) ||
			!flush_subnormal_and_neg_zero(p_color.g, flushed_color[1]) ||
			!flush_subnormal_and_neg_zero(p_color.b, flushed_color[2]) ||
			!flush_subnormal_and_neg_zero(p_color.a, flushed_color[3]) ||
			!flush_subnormal_and_neg_zero(p_custom_uv_rect.r, flushed_custom[0]) ||
			!flush_subnormal_and_neg_zero(p_custom_uv_rect.g, flushed_custom[1]) ||
			!flush_subnormal_and_neg_zero(p_custom_uv_rect.b, flushed_custom[2]) ||
			!flush_subnormal_and_neg_zero(p_custom_uv_rect.a, flushed_custom[3])) {
		return ERR_INVALID_PARAMETER;
	}

	double col_len_sq[3] = {};
	double col_len[3] = {};
	for (int c = 0; c < 3; ++c) {
		const double x = double(flushed_basis[0][c]);
		const double y = double(flushed_basis[1][c]);
		const double z = double(flushed_basis[2][c]);
		col_len_sq[c] = x * x + y * y + z * z;
		if (!std::isfinite(col_len_sq[c])) {
			return ERR_INVALID_PARAMETER;
		}
		col_len[c] = std::sqrt(col_len_sq[c]);
	}

	const double min_col_len = MIN(col_len[0], MIN(col_len[1], col_len[2]));
	if (min_col_len <= 1.0e-6) {
		if (!p_allow_zero_scale_degenerate) {
			return ERR_INVALID_PARAMETER;
		}
		r_record = make_canonical_degenerate_instance192(
				Vector3(flushed_origin[0], flushed_origin[1], flushed_origin[2]),
				p_custom_uv_rect,
				p_particle_id,
				p_has_previous);
		return r_record.flags != 0u ? OK : ERR_INVALID_PARAMETER;
	}

	const double m00 = double(flushed_basis[0][0]);
	const double m01 = double(flushed_basis[0][1]);
	const double m02 = double(flushed_basis[0][2]);
	const double m10 = double(flushed_basis[1][0]);
	const double m11 = double(flushed_basis[1][1]);
	const double m12 = double(flushed_basis[1][2]);
	const double m20 = double(flushed_basis[2][0]);
	const double m21 = double(flushed_basis[2][1]);
	const double m22 = double(flushed_basis[2][2]);

	const double co00 = m11 * m22 - m12 * m21;
	const double co01 = m12 * m20 - m10 * m22;
	const double co02 = m10 * m21 - m11 * m20;
	const double det = m00 * co00 + m01 * co01 + m02 * co02;
	const double vol_scale = col_len[0] * col_len[1] * col_len[2];
	if (!std::isfinite(det) || !std::isfinite(vol_scale) || std::abs(det) <= 1.0e-12 * vol_scale) {
		return ERR_INVALID_PARAMETER;
	}

	const double inv_det = 1.0 / det;
	if (!std::isfinite(inv_det)) {
		return ERR_INVALID_PARAMETER;
	}

	const double inv_b[3][3] = {
		{ co00 * inv_det, (m02 * m21 - m01 * m22) * inv_det, (m01 * m12 - m02 * m11) * inv_det },
		{ co01 * inv_det, (m00 * m22 - m02 * m20) * inv_det, (m02 * m10 - m00 * m12) * inv_det },
		{ co02 * inv_det, (m01 * m20 - m00 * m21) * inv_det, (m00 * m11 - m01 * m10) * inv_det },
	};
	const double tx = double(flushed_origin[0]);
	const double ty = double(flushed_origin[1]);
	const double tz = double(flushed_origin[2]);
	const double inv_t[3] = {
		-(inv_b[0][0] * tx + inv_b[0][1] * ty + inv_b[0][2] * tz),
		-(inv_b[1][0] * tx + inv_b[1][1] * ty + inv_b[1][2] * tz),
		-(inv_b[2][0] * tx + inv_b[2][1] * ty + inv_b[2][2] * tz),
	};

	Instance192Record rec;
	memset(&rec, 0, sizeof(Instance192Record));
	for (int r = 0; r < 3; ++r) {
		rec.transform_rows[r][0] = flushed_basis[r][0];
		rec.transform_rows[r][1] = flushed_basis[r][1];
		rec.transform_rows[r][2] = flushed_basis[r][2];
		rec.transform_rows[r][3] = flushed_origin[r];
		for (int c = 0; c < 3; ++c) {
			if (!flush_subnormal_and_neg_zero(float(inv_b[r][c]), rec.inverse_transform_rows[r][c])) {
				return ERR_INVALID_PARAMETER;
			}
		}
		if (!flush_subnormal_and_neg_zero(float(inv_t[r]), rec.inverse_transform_rows[r][3])) {
			return ERR_INVALID_PARAMETER;
		}
	}
	for (int c = 0; c < 3; ++c) {
		if (!flush_subnormal_and_neg_zero(float(inv_b[c][0]), rec.normal_row0[c]) ||
				!flush_subnormal_and_neg_zero(float(inv_b[c][1]), rec.normal_row1[c]) ||
				!flush_subnormal_and_neg_zero(float(inv_b[c][2]), rec.normal_row2[c])) {
			return ERR_INVALID_PARAMETER;
		}
	}

	const double origin_norm = std::sqrt(tx * tx + ty * ty + tz * tz);
	const double trans_tol = 1.0e-3 * MAX(1.0, origin_norm);
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			const double prod = double(rec.transform_rows[r][0]) * double(rec.inverse_transform_rows[0][c]) +
					double(rec.transform_rows[r][1]) * double(rec.inverse_transform_rows[1][c]) +
					double(rec.transform_rows[r][2]) * double(rec.inverse_transform_rows[2][c]);
			const double expected = (r == c) ? 1.0 : 0.0;
			if (!std::isfinite(prod) || std::abs(prod - expected) > 1.0e-3) {
				return ERR_INVALID_PARAMETER;
			}
		}
		const double t_res = double(rec.transform_rows[r][0]) * double(rec.inverse_transform_rows[0][3]) +
				double(rec.transform_rows[r][1]) * double(rec.inverse_transform_rows[1][3]) +
				double(rec.transform_rows[r][2]) * double(rec.inverse_transform_rows[2][3]) +
				double(rec.transform_rows[r][3]);
		if (!std::isfinite(t_res) || std::abs(t_res) > trans_tol) {
			return ERR_INVALID_PARAMETER;
		}
	}

	uint32_t extra_flags = Instance192Record::FLAG_INVERSE_AVAILABLE;
	if (det < 0.0) {
		extra_flags |= Instance192Record::FLAG_NEGATIVE_DETERMINANT;
	}
	const double scale_ref = MAX(MAX(col_len_sq[0], col_len_sq[1]), MAX(col_len_sq[2], 1.0e-12));
	const double eps = 1.0e-5 * scale_ref;
	const double dot01 = m00 * m01 + m10 * m11 + m20 * m21;
	const double dot02 = m00 * m02 + m10 * m12 + m20 * m22;
	const double dot12 = m01 * m02 + m11 * m12 + m21 * m22;
	if (std::abs(col_len_sq[0] - col_len_sq[1]) > eps ||
			std::abs(col_len_sq[0] - col_len_sq[2]) > eps ||
			std::abs(dot01) > eps ||
			std::abs(dot02) > eps ||
			std::abs(dot12) > eps) {
		extra_flags |= Instance192Record::FLAG_NON_UNIFORM_SCALE;
	}

	for (int k = 0; k < 4; ++k) {
		rec.color[k] = flushed_color[k];
		rec.custom0[k] = flushed_custom[k];
	}
	rec.set_particle_id(p_particle_id);
	rec.flags = Instance192Record::FLAG_VALID_RECORD |
			(p_has_previous ? Instance192Record::FLAG_HAS_PREVIOUS_RECORD : 0u) |
			extra_flags;
	rec.reserved_zero = 0u;
	r_record = rec;
	return OK;
}

Instance192Record ParticlesMeshLightSortRD::make_instance192_record(
		const Transform3D &p_transform,
		const Vector3 &p_velocity,
		const Color &p_color,
		float p_subuv_frame,
		uint64_t p_particle_id,
		bool p_has_previous) {
	(void)p_velocity;
	Instance192Record rec;
	memset(&rec, 0, sizeof(Instance192Record));
	const Error err = build_instance192_record_checked(
			p_transform,
			p_color,
			Color(0.0f, p_subuv_frame, 0.0f, 0.0f),
			p_particle_id,
			p_has_previous,
			true,
			rec);
	if (err != OK) {
		memset(&rec, 0, sizeof(Instance192Record));
	}
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
				!Math::is_finite(c.specular_scale) || c.specular_scale < 0.0f ||
				!Math::is_finite(c.diffuse_scale) || c.diffuse_scale < 0.0f ||
				!Math::is_finite(c.volumetric_scattering) || c.volumetric_scattering < 0.0f ||
				c.reserved0 != 0u || (c.flags & ~ParticleLightRecord64::FLAG_KNOWN_MASK) != 0u ||
				(c.flags & ParticleLightRecord64::FLAG_ACTIVE) == 0u ||
				c.id_acquire_tag == 0u || c.screen_importance_q16 == 0u || c.screen_importance_q16 > 65535u) {
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

void ParticlesMeshLightSortRD::register_sealed_external_light_slot(const ExternalLightSlotRegistration &p_slot) {
	if (p_slot.owner_key == 0 || p_slot.capacity > MAX_PARTICLE_LIGHTS) {
		return;
	}
	MutexLock lock(active_lights_mutex);
	ExternalOwnerLightState &st = external_owner_lights[p_slot.owner_key];
	if (!st.has_pending_rd_slot || p_slot.generation != st.pending_slot.generation || p_slot.present_seq >= st.pending_slot.present_seq) {
		st.pending_slot = p_slot;
		st.has_pending_rd_slot = true;
	}
}

bool ParticlesMeshLightSortRD::update_external_light_slot_present_seq(
		uint64_t p_owner_key,
		uint64_t p_generation,
		uint32_t p_slot_index,
		uint64_t p_lease_serial,
		uint64_t p_next_present_seq) {
	if (p_owner_key == 0 || p_next_present_seq == 0) {
		return false;
	}
	MutexLock lock(active_lights_mutex);
	HashMap<uint64_t, ExternalOwnerLightState>::Iterator it = external_owner_lights.find(p_owner_key);
	if (!it || !it->value.has_pending_rd_slot) {
		return false;
	}
	ExternalLightSlotRegistration &reg = it->value.pending_slot;
	if (reg.generation != p_generation || reg.slot_index != p_slot_index || reg.lease_serial != p_lease_serial) {
		return false;
	}
	reg.present_seq = p_next_present_seq;
	return true;
}

uint32_t ParticlesMeshLightSortRD::update_external_light_slots_present_seq_batch(
		const ExternalLightSlotPresentSeqUpdate *p_updates,
		uint32_t p_count) {
	if (!p_updates || p_count == 0) {
		return 0;
	}
	uint32_t updated = 0;
	MutexLock lock(active_lights_mutex);
	for (uint32_t i = 0; i < p_count; ++i) {
		const ExternalLightSlotPresentSeqUpdate &u = p_updates[i];
		if (u.owner_key == 0 || u.next_present_seq == 0) {
			continue;
		}
		HashMap<uint64_t, ExternalOwnerLightState>::Iterator it = external_owner_lights.find(u.owner_key);
		if (!it || !it->value.has_pending_rd_slot) {
			continue;
		}
		ExternalLightSlotRegistration &reg = it->value.pending_slot;
		if (reg.generation != u.generation || reg.slot_index != u.slot_index || reg.lease_serial != u.lease_serial) {
			continue;
		}
		reg.present_seq = u.next_present_seq;
		++updated;
	}
	return updated;
}

void ParticlesMeshLightSortRD::unregister_external_light_slot(uint64_t p_owner_key, uint64_t p_generation, uint32_t p_slot_index) {
	if (p_owner_key == 0) {
		return;
	}
	MutexLock lock(active_lights_mutex);
	HashMap<uint64_t, ExternalOwnerLightState>::Iterator it = external_owner_lights.find(p_owner_key);
	if (!it) {
		return;
	}
	if (it->value.has_pending_rd_slot &&
			(p_generation == 0 || it->value.pending_slot.generation == p_generation) &&
			(p_slot_index == UINT32_MAX || it->value.pending_slot.slot_index == p_slot_index)) {
		it->value.pending_slot = ExternalLightSlotRegistration{};
		it->value.has_pending_rd_slot = false;
		if (it->value.compacted_lights.is_empty()) {
			external_owner_lights.remove(it);
		}
	}
}

void ParticlesMeshLightSortRD::unregister_external_light_owner(uint64_t p_owner_key) {
	if (p_owner_key == 0) {
		return;
	}
	MutexLock lock(active_lights_mutex);
	external_owner_lights.erase(p_owner_key);
}

Error ParticlesMeshLightSortRD::upsert_external_role8_packet(
		uint64_t p_owner_key,
		const uint8_t *p_meta32,
		size_t p_meta_bytes,
		const uint8_t *p_role8,
		size_t p_role8_bytes,
		uint32_t p_capacity,
		uint64_t p_present_seq) {
	if (p_owner_key == 0 || p_capacity > MAX_PARTICLE_LIGHTS || p_meta32 == nullptr || p_meta_bytes != 32u) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_capacity == 0) {
		MutexLock lock(active_lights_mutex);
		ExternalOwnerLightState &st = external_owner_lights[p_owner_key];
		st.has_pending_rd_slot = false;
		st.compacted_lights.clear();
		return OK;
	}
	uint32_t meta_words[8] = {};
	memcpy(meta_words, p_meta32, 32u);
	const uint32_t active_count = meta_words[0];
	const uint32_t fault_bits = meta_words[3];
	const uint32_t meta_cap = meta_words[4];
	const uint32_t schema_id = meta_words[5];
	const uint32_t expected_seq_lo = uint32_t(p_present_seq & 0xFFFFFFFFull);
	const uint32_t expected_seq_hi = uint32_t(p_present_seq >> 32u);
	if (meta_words[1] != 0u || meta_words[2] != 0u ||
			meta_words[6] != expected_seq_lo || meta_words[7] != expected_seq_hi ||
			schema_id != uint32_t(ExternalOutputConfig::PARTICLE_LIGHT_64) || meta_cap != p_capacity || active_count > p_capacity) {
		return ERR_INVALID_DATA;
	}
	LocalVector<ParticleLightRecord64> compacted;
	if (fault_bits == 0u && active_count > 0u) {
		if (p_role8 == nullptr || p_role8_bytes < size_t(active_count) * sizeof(ParticleLightRecord64)) {
			return ERR_INVALID_DATA;
		}
		LocalVector<ParticleLightRecord64> raw_candidates;
		raw_candidates.resize(active_count);
		memcpy(raw_candidates.ptr(), p_role8, size_t(active_count) * sizeof(ParticleLightRecord64));
		const Error err = compact_particle_lights_deterministic(raw_candidates, p_capacity, compacted);
		if (err != OK) {
			return err;
		}
	}
	MutexLock lock(active_lights_mutex);
	ExternalOwnerLightState &st = external_owner_lights[p_owner_key];
	st.has_pending_rd_slot = false;
	st.compacted_lights = compacted;
	return OK;
}

namespace {

static const char *kLightHarvestComputeGLSL = R"GLSL(#version 450
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0, std430) readonly buffer SlotMeta {
	uint words[];
} slot_meta;

layout(set = 0, binding = 1, std430) readonly buffer SlotRecords {
	uint words[];
} slot_records;

layout(set = 0, binding = 2, std430) buffer CandidateStaging {
	uint words[];
} staging;

layout(push_constant, std430) uniform PushParams {
	uint slot_capacity;
	uint max_staged;
	uint seq_lo;
	uint seq_hi;
} push_params;

shared uint s_active_count;
shared uint s_prev_count;
shared uint s_slot_valid;
shared uint s_prefix[256];
shared uint s_winner_src[256];
shared uvec4 s_keys[512];

bool is_finite_u32(uint u) {
	return (u & 0x7F800000u) != 0x7F800000u;
}

uint flush_subnormal_u32(uint u) {
	uint mag = u & 0x7FFFFFFFu;
	return (mag < 0x00800000u) ? 0u : u;
}

float clean_f32(uint u) {
	return uintBitsToFloat(flush_subnormal_u32(u));
}

bool beats(uvec4 b, uvec4 a) {
	if (b.x != a.x) {
		return b.x > a.x;
	}
	if (b.y != a.y) {
		return b.y < a.y;
	}
	if (b.z != a.z) {
		return b.z < a.z;
	}
	return b.w < a.w;
}

void main() {
	uint tid = gl_LocalInvocationID.x;
	if (tid == 0u) {
		uint active_count = slot_meta.words[0];
		uint res1 = slot_meta.words[1];
		uint res2 = slot_meta.words[2];
		uint fault_bits = slot_meta.words[3];
		uint meta_cap = slot_meta.words[4];
		uint schema_id = slot_meta.words[5];
		uint meta_seq_lo = slot_meta.words[6];
		uint meta_seq_hi = slot_meta.words[7];
		if (res1 == 0u && res2 == 0u && fault_bits == 0u && schema_id == 4u &&
				meta_cap == push_params.slot_capacity && active_count <= push_params.slot_capacity &&
				meta_seq_lo == push_params.seq_lo && meta_seq_hi == push_params.seq_hi) {
			s_active_count = active_count;
		} else {
			s_active_count = 0u;
		}
		s_prev_count = min(staging.words[0], 256u);
	}
	barrier();

	uint w[16];
	bool valid = false;
	if (tid < s_active_count) {
		uint src_base = tid * 16u;
		for (uint i = 0u; i < 16u; ++i) {
			w[i] = slot_records.words[src_base + i];
		}
		bool finite_pos = is_finite_u32(w[0]) && is_finite_u32(w[1]) && is_finite_u32(w[2]);
		float radius = clean_f32(w[3]);
		float cr = clean_f32(w[4]);
		float cg = clean_f32(w[5]);
		float cb = clean_f32(w[6]);
		float atten = clean_f32(w[7]);
		uint acquire_tag = w[9];
		uint flags = w[10];
		uint importance = w[11];
		float spec_s = clean_f32(w[12]);
		float diff_s = clean_f32(w[13]);
		float fog_s = clean_f32(w[14]);
		uint res0 = w[15];

		if (finite_pos &&
				is_finite_u32(w[3]) && radius > 0.0 &&
				is_finite_u32(w[4]) && cr >= 0.0 &&
				is_finite_u32(w[5]) && cg >= 0.0 &&
				is_finite_u32(w[6]) && cb >= 0.0 &&
				(cr > 0.0 || cg > 0.0 || cb > 0.0) &&
				is_finite_u32(w[7]) && atten >= 0.0 &&
				is_finite_u32(w[12]) && spec_s >= 0.0 &&
				is_finite_u32(w[13]) && diff_s >= 0.0 &&
				is_finite_u32(w[14]) && fog_s >= 0.0 &&
				res0 == 0u && (flags & ~3u) == 0u && (flags & 1u) != 0u &&
				acquire_tag != 0u && importance > 0u && importance <= 65535u) {
			w[0] = flush_subnormal_u32(w[0]);
			w[1] = flush_subnormal_u32(w[1]);
			w[2] = flush_subnormal_u32(w[2]);
			w[3] = flush_subnormal_u32(w[3]);
			w[4] = flush_subnormal_u32(w[4]);
			w[5] = flush_subnormal_u32(w[5]);
			w[6] = flush_subnormal_u32(w[6]);
			w[7] = flush_subnormal_u32(w[7]);
			w[12] = flush_subnormal_u32(w[12]);
			w[13] = flush_subnormal_u32(w[13]);
			w[14] = flush_subnormal_u32(w[14]);
			valid = true;
		}
	}

	s_prefix[tid] = valid ? 1u : 0u;
	barrier();

	if (tid == 0u) {
		uint sum = 0u;
		for (uint i = 0u; i < 256u; ++i) {
			uint v = (i < s_active_count) ? s_prefix[i] : 0u;
			s_prefix[i] = sum;
			sum += v;
		}
		s_slot_valid = sum;
	}
	barrier();

	if (s_slot_valid == 0u) {
		return;
	}

	if (valid) {
		uint dst = s_prev_count + s_prefix[tid];
		if (dst < push_params.max_staged) {
			uint dst_base = 4u + dst * 16u;
			for (uint i = 0u; i < 16u; ++i) {
				staging.words[dst_base + i] = w[i];
			}
		}
	}
	memoryBarrierBuffer();
	barrier();

	uint total = min(s_prev_count + s_slot_valid, push_params.max_staged);
	uint new_count = min(total, 256u);

	if (tid < total) {
		uint b0 = 4u + tid * 16u;
		s_keys[tid] = uvec4(staging.words[b0 + 11u], staging.words[b0 + 9u], staging.words[b0 + 8u], tid);
	} else {
		s_keys[tid] = uvec4(0u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu);
	}
	uint idx1 = 256u + tid;
	if (idx1 < total) {
		uint b1 = 4u + idx1 * 16u;
		s_keys[idx1] = uvec4(staging.words[b1 + 11u], staging.words[b1 + 9u], staging.words[b1 + 8u], idx1);
	} else {
		s_keys[idx1] = uvec4(0u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu);
	}
	barrier();

	if (tid < total) {
		uvec4 k0 = s_keys[tid];
		uint r0 = 0u;
		for (uint j = 0u; j < total; ++j) {
			if (beats(s_keys[j], k0)) {
				r0++;
			}
		}
		if (r0 < new_count) {
			s_winner_src[r0] = tid;
		}
	}
	if (idx1 < total) {
		uvec4 k1 = s_keys[idx1];
		uint r1 = 0u;
		for (uint j = 0u; j < total; ++j) {
			if (beats(s_keys[j], k1)) {
				r1++;
			}
		}
		if (r1 < new_count) {
			s_winner_src[r1] = idx1;
		}
	}
	barrier();

	uint win_w[16];
	if (tid < new_count) {
		uint win_base = 4u + s_winner_src[tid] * 16u;
		for (uint i = 0u; i < 16u; ++i) {
			win_w[i] = staging.words[win_base + i];
		}
	}
	barrier();

	if (tid < new_count) {
		uint out_base = 4u + tid * 16u;
		for (uint i = 0u; i < 16u; ++i) {
			staging.words[out_base + i] = win_w[i];
		}
	}
	if (tid == 0u) {
		staging.words[0] = new_count;
	}
}
)GLSL";

static const char *kLightTopKEmitComputeGLSL = R"GLSL(#version 450
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0, std430) readonly buffer CandidateStaging {
	uint words[];
} staging;

layout(set = 0, binding = 1, std430) buffer OmniLights {
	uint words[];
} omni_lights;

layout(set = 0, binding = 2, std430) buffer ClusterElements {
	uint words[];
} cluster_elements;

layout(set = 0, binding = 3, std140) uniform EmitParams {
	vec4 light_view_row0;
	vec4 light_view_row1;
	vec4 light_view_row2;
	vec4 cluster_view_row0;
	vec4 cluster_view_row1;
	vec4 cluster_view_row2;
	vec4 misc_floats; // x=exposure_norm, y=z_near, z=z_far, w=sphere_overfit
	uvec4 counts;     // x=base_omni_index, y=reserved_count, z=max_staged, w=camera_orthogonal
} params;

shared uint s_total_candidates;
shared uvec4 s_keys[256];

bool beats(uvec4 b, uvec4 a) {
	if (b.x != a.x) {
		return b.x > a.x;
	}
	if (b.y != a.y) {
		return b.y < a.y;
	}
	if (b.z != a.z) {
		return b.z < a.z;
	}
	return b.w < a.w;
}

void main() {
	uint tid = gl_LocalInvocationID.x;
	uint base_omni = params.counts.x;
	uint reserved = params.counts.y;

	if (tid == 0u) {
		s_total_candidates = min(staging.words[0], params.counts.z);
	}

	if (tid < reserved) {
		uint omni_base = (base_omni + tid) * 56u;
		for (uint i = 0u; i < 56u; ++i) {
			omni_lights.words[omni_base + i] = 0u;
		}
		uint elem_base = tid * 20u;
		for (uint i = 0u; i < 20u; ++i) {
			cluster_elements.words[elem_base + i] = 0u;
		}
		cluster_elements.words[elem_base + 3u] = base_omni + tid;
		cluster_elements.words[elem_base + 4u] = floatBitsToUint(1.0);
		cluster_elements.words[elem_base + 9u] = floatBitsToUint(1.0);
		cluster_elements.words[elem_base + 14u] = floatBitsToUint(1.0);
		cluster_elements.words[elem_base + 15u] = floatBitsToUint(params.misc_floats.y + 16.0);
	}
	memoryBarrierBuffer();
	barrier();

	uint N = s_total_candidates;
	if (N == 0u || reserved == 0u) {
		return;
	}

	for (uint outer_base = 0u; outer_base < N; outer_base += 256u) {
		uint my_idx = outer_base + tid;
		uvec4 my_key = uvec4(0u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu);
		if (my_idx < N) {
			uint cbase = 4u + my_idx * 16u;
			my_key = uvec4(staging.words[cbase + 11u], staging.words[cbase + 9u], staging.words[cbase + 8u], my_idx);
		}
		uint rank = 0u;
		for (uint tile_base = 0u; tile_base < N; tile_base += 256u) {
			uint load_idx = tile_base + tid;
			if (load_idx < N) {
				uint lbase = 4u + load_idx * 16u;
				s_keys[tid] = uvec4(staging.words[lbase + 11u], staging.words[lbase + 9u], staging.words[lbase + 8u], load_idx);
			} else {
				s_keys[tid] = uvec4(0u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu);
			}
			barrier();

			if (my_idx < N) {
				uint tile_cnt = min(256u, N - tile_base);
				for (uint j = 0u; j < tile_cnt; ++j) {
					if (beats(s_keys[j], my_key)) {
						rank++;
					}
				}
			}
			barrier();
		}

		if (my_idx < N && rank < reserved) {
			uint cbase = 4u + my_idx * 16u;
			vec3 world_pos = vec3(
					uintBitsToFloat(staging.words[cbase + 0u]),
					uintBitsToFloat(staging.words[cbase + 1u]),
					uintBitsToFloat(staging.words[cbase + 2u]));
			float radius = max(0.001, uintBitsToFloat(staging.words[cbase + 3u]));
			vec3 color_hdr = vec3(
					uintBitsToFloat(staging.words[cbase + 4u]),
					uintBitsToFloat(staging.words[cbase + 5u]),
					uintBitsToFloat(staging.words[cbase + 6u]));
			float raw_atten = uintBitsToFloat(staging.words[cbase + 7u]);
			float atten = (raw_atten > 0.0) ? raw_atten : 1.0;
			float spec_scale = uintBitsToFloat(staging.words[cbase + 12u]);
			float diff_scale = uintBitsToFloat(staging.words[cbase + 13u]);
			float vol_fog = uintBitsToFloat(staging.words[cbase + 14u]);
			float exposure = params.misc_floats.x;

			vec3 light_view_pos = vec3(
					dot(params.light_view_row0.xyz, world_pos) + params.light_view_row0.w,
					dot(params.light_view_row1.xyz, world_pos) + params.light_view_row1.w,
					dot(params.light_view_row2.xyz, world_pos) + params.light_view_row2.w);
			vec3 final_col = color_hdr * (diff_scale * exposure);

			uint omni_idx = base_omni + rank;
			uint obase = omni_idx * 56u;
			omni_lights.words[obase + 0u] = floatBitsToUint(light_view_pos.x);
			omni_lights.words[obase + 1u] = floatBitsToUint(light_view_pos.y);
			omni_lights.words[obase + 2u] = floatBitsToUint(light_view_pos.z);
			omni_lights.words[obase + 3u] = floatBitsToUint(1.0 / radius);
			omni_lights.words[obase + 8u] = floatBitsToUint(final_col.r);
			omni_lights.words[obase + 9u] = floatBitsToUint(final_col.g);
			omni_lights.words[obase + 10u] = floatBitsToUint(final_col.b);
			omni_lights.words[obase + 11u] = floatBitsToUint(atten);
			omni_lights.words[obase + 20u] = floatBitsToUint(spec_scale);
			omni_lights.words[obase + 49u] = 0xFFFFFFFFu;
			omni_lights.words[obase + 50u] = floatBitsToUint(vol_fog);

			vec3 cluster_pos = vec3(
					dot(params.cluster_view_row0.xyz, world_pos) + params.cluster_view_row0.w,
					dot(params.cluster_view_row1.xyz, world_pos) + params.cluster_view_row1.w,
					dot(params.cluster_view_row2.xyz, world_pos) + params.cluster_view_row2.w);
			float overfit = params.misc_floats.w;
			float r_elem = radius * overfit;
			float depth = -cluster_pos.z;
			bool touches_near = false;
			if (params.counts.w != 0u) {
				touches_near = (depth - r_elem) < params.misc_floats.y;
			} else {
				float r2 = r_elem * overfit;
				touches_near = dot(cluster_pos, cluster_pos) < (r2 * r2);
			}
			bool touches_far = (depth + r_elem) > params.misc_floats.z;

			uint ebase = rank * 20u;
			cluster_elements.words[ebase + 0u] = 0u;
			cluster_elements.words[ebase + 1u] = touches_near ? 1u : 0u;
			cluster_elements.words[ebase + 2u] = touches_far ? 1u : 0u;
			cluster_elements.words[ebase + 3u] = omni_idx;
			cluster_elements.words[ebase + 4u] = floatBitsToUint(params.cluster_view_row0.x);
			cluster_elements.words[ebase + 5u] = floatBitsToUint(params.cluster_view_row0.y);
			cluster_elements.words[ebase + 6u] = floatBitsToUint(params.cluster_view_row0.z);
			cluster_elements.words[ebase + 7u] = floatBitsToUint(cluster_pos.x);
			cluster_elements.words[ebase + 8u] = floatBitsToUint(params.cluster_view_row1.x);
			cluster_elements.words[ebase + 9u] = floatBitsToUint(params.cluster_view_row1.y);
			cluster_elements.words[ebase + 10u] = floatBitsToUint(params.cluster_view_row1.z);
			cluster_elements.words[ebase + 11u] = floatBitsToUint(cluster_pos.y);
			cluster_elements.words[ebase + 12u] = floatBitsToUint(params.cluster_view_row2.x);
			cluster_elements.words[ebase + 13u] = floatBitsToUint(params.cluster_view_row2.y);
			cluster_elements.words[ebase + 14u] = floatBitsToUint(params.cluster_view_row2.z);
			cluster_elements.words[ebase + 15u] = floatBitsToUint(cluster_pos.z);
			cluster_elements.words[ebase + 16u] = floatBitsToUint(r_elem);
			cluster_elements.words[ebase + 17u] = floatBitsToUint(r_elem);
			cluster_elements.words[ebase + 18u] = floatBitsToUint(r_elem);
			cluster_elements.words[ebase + 19u] = 0u;
		}
	}
}
)GLSL";

static const char *kLightClusterBlitComputeGLSL = R"GLSL(#version 450
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0, std430) readonly buffer SrcElements {
	uint words[];
} src_elements;

layout(set = 0, binding = 1, std430) buffer DstElements {
	uint words[];
} dst_elements;

layout(push_constant, std430) uniform BlitPush {
	uint dst_element_offset;
	uint element_count;
	uint pad0;
	uint pad1;
} push_params;

void main() {
	uint tid = gl_LocalInvocationID.x;
	if (tid >= push_params.element_count) {
		return;
	}
	uint src_base = tid * 20u;
	uint dst_base = (push_params.dst_element_offset + tid) * 20u;
	for (uint i = 0u; i < 20u; ++i) {
		dst_elements.words[dst_base + i] = src_elements.words[src_base + i];
	}
}
)GLSL";

static Error compile_light_bridge_compute_pipeline(RenderingDevice &p_rd, const char *p_glsl, const char *p_name, RID &r_shader, RID &r_pipeline) {
	String err;
	RD::ShaderStageSPIRVData stage;
	stage.shader_stage = RD::SHADER_STAGE_COMPUTE;
	stage.spirv = p_rd.shader_compile_spirv_from_source(RD::SHADER_STAGE_COMPUTE, p_glsl, RD::SHADER_LANGUAGE_GLSL, &err);
	ERR_FAIL_COND_V_MSG(stage.spirv.is_empty(), ERR_COMPILATION_FAILED, String("ParticleLight64 GPU bridge shader compile failed: ") + err);
	Vector<RD::ShaderStageSPIRVData> stages;
	stages.push_back(stage);
	r_shader = p_rd.shader_create_from_spirv(stages, p_name);
	ERR_FAIL_COND_V(r_shader.is_null(), ERR_CANT_CREATE);
	r_pipeline = p_rd.compute_pipeline_create(r_shader);
	if (r_pipeline.is_null()) {
		p_rd.free_rid(r_shader);
		r_shader = RID();
		return ERR_CANT_CREATE;
	}
	return OK;
}

struct LightEmitParamsStd140 {
	float light_view_row0[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
	float light_view_row1[4] = { 0.0f, 1.0f, 0.0f, 0.0f };
	float light_view_row2[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
	float cluster_view_row0[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
	float cluster_view_row1[4] = { 0.0f, 1.0f, 0.0f, 0.0f };
	float cluster_view_row2[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
	float misc_floats[4] = { 1.0f, 0.05f, 4000.0f, 1.05f };
	uint32_t counts[4] = { 0u, 0u, 0u, 0u };
};
static_assert(sizeof(LightEmitParamsStd140) == 128, "LightEmitParamsStd140 ABI");

} // namespace

Error ParticlesMeshLightSortRD::_ensure_light_gpu_bridge(RenderingDevice *p_rd) {
	if (!p_rd) {
		return ERR_UNAVAILABLE;
	}
	if (light_gpu_bridge_rd == p_rd && light_harvest_pipeline.is_valid() && light_topk_emit_pipeline.is_valid() && light_cluster_blit_pipeline.is_valid()) {
		return OK;
	}
	if (light_gpu_bridge_rd != nullptr && light_gpu_bridge_rd != p_rd) {
		for (uint32_t i = 0; i < UNIFORM_SET_CACHE_CAPACITY; ++i) {
			light_uniform_set_cache[i] = CachedLightUniformSet{};
		}
		light_harvest_shader = light_harvest_pipeline = RID();
		light_topk_emit_shader = light_topk_emit_pipeline = RID();
		light_cluster_blit_shader = light_cluster_blit_pipeline = RID();
		light_candidate_staging_buffer = light_cluster_elements_buffer = light_emit_params_ubo = RID();
		light_gpu_bridge_rd = nullptr;
	}

	Error err = compile_light_bridge_compute_pipeline(*p_rd, kLightHarvestComputeGLSL, "particle_light64_harvest", light_harvest_shader, light_harvest_pipeline);
	if (err == OK) {
		err = compile_light_bridge_compute_pipeline(*p_rd, kLightTopKEmitComputeGLSL, "particle_light64_topk_emit", light_topk_emit_shader, light_topk_emit_pipeline);
	}
	if (err == OK) {
		err = compile_light_bridge_compute_pipeline(*p_rd, kLightClusterBlitComputeGLSL, "particle_light64_cluster_blit", light_cluster_blit_shader, light_cluster_blit_pipeline);
	}
	if (err == OK) {
		const uint32_t staging_bytes = 16u + MAX_STAGED_LIGHT_CANDIDATES * uint32_t(sizeof(ParticleLightRecord64));
		light_candidate_staging_buffer = p_rd->storage_buffer_create(staging_bytes);
		light_cluster_elements_buffer = p_rd->storage_buffer_create(MAX_PARTICLE_LIGHTS * CLUSTER_ELEMENT_WORD_STRIDE * uint32_t(sizeof(uint32_t)));
		light_emit_params_ubo = p_rd->uniform_buffer_create(sizeof(LightEmitParamsStd140));
		if (light_candidate_staging_buffer.is_null() || light_cluster_elements_buffer.is_null() || light_emit_params_ubo.is_null()) {
			err = ERR_CANT_CREATE;
		}
	}
	light_gpu_bridge_rd = p_rd;
	if (err != OK) {
		_release_light_gpu_bridge(p_rd);
		return err;
	}
	return OK;
}

void ParticlesMeshLightSortRD::_release_light_gpu_bridge(RenderingDevice *p_rd) {
	if (!p_rd || light_gpu_bridge_rd != p_rd) {
		return;
	}
	for (uint32_t i = 0; i < UNIFORM_SET_CACHE_CAPACITY; ++i) {
		if (light_uniform_set_cache[i].uniform_set.is_valid() && p_rd->uniform_set_is_valid(light_uniform_set_cache[i].uniform_set)) {
			p_rd->free_rid(light_uniform_set_cache[i].uniform_set);
		}
		light_uniform_set_cache[i] = CachedLightUniformSet{};
	}
	const RID to_free[] = {
		light_harvest_pipeline,
		light_harvest_shader,
		light_topk_emit_pipeline,
		light_topk_emit_shader,
		light_cluster_blit_pipeline,
		light_cluster_blit_shader,
		light_candidate_staging_buffer,
		light_cluster_elements_buffer,
		light_emit_params_ubo,
	};
	for (uint32_t i = 0; i < sizeof(to_free) / sizeof(to_free[0]); ++i) {
		if (to_free[i].is_valid()) {
			p_rd->free_rid(to_free[i]);
		}
	}
	light_harvest_pipeline = light_harvest_shader = RID();
	light_topk_emit_pipeline = light_topk_emit_shader = RID();
	light_cluster_blit_pipeline = light_cluster_blit_shader = RID();
	light_candidate_staging_buffer = light_cluster_elements_buffer = light_emit_params_ubo = RID();
	light_gpu_bridge_rd = nullptr;
}

RID ParticlesMeshLightSortRD::_acquire_cached_uniform_set(RenderingDevice *p_rd, RID p_shader, const RD::Uniform *p_uniforms, uint32_t p_count) {
	if (!p_rd || p_shader.is_null() || !p_uniforms || p_count == 0 || p_count > 4u) {
		return RID();
	}
	RID key_rids[4] = {};
	for (uint32_t i = 0; i < p_count; ++i) {
		key_rids[i] = p_uniforms[i].get_id_count() > 0 ? p_uniforms[i].get_id(0) : RID();
	}
	++light_uniform_set_tick;
	uint32_t free_idx = UINT32_MAX;
	uint32_t oldest_idx = 0;
	uint64_t oldest_tick = UINT64_MAX;
	for (uint32_t i = 0; i < UNIFORM_SET_CACHE_CAPACITY; ++i) {
		CachedLightUniformSet &entry = light_uniform_set_cache[i];
		if (entry.uniform_set.is_valid() && !p_rd->uniform_set_is_valid(entry.uniform_set)) {
			entry = CachedLightUniformSet{};
		}
		if (entry.uniform_set.is_null()) {
			if (free_idx == UINT32_MAX) {
				free_idx = i;
			}
			continue;
		}
		if (entry.shader == p_shader && entry.rids[0] == key_rids[0] && entry.rids[1] == key_rids[1] &&
				entry.rids[2] == key_rids[2] && entry.rids[3] == key_rids[3]) {
			entry.last_used_tick = light_uniform_set_tick;
			return entry.uniform_set;
		}
		if (entry.last_used_tick < oldest_tick) {
			oldest_tick = entry.last_used_tick;
			oldest_idx = i;
		}
	}

	Vector<RD::Uniform> uvec;
	uvec.resize(p_count);
	for (uint32_t i = 0; i < p_count; ++i) {
		uvec.write[i] = p_uniforms[i];
	}
	const RID created = p_rd->uniform_set_create(uvec, p_shader, 0);
	if (created.is_null()) {
		return RID();
	}
	const uint32_t slot = (free_idx != UINT32_MAX) ? free_idx : oldest_idx;
	if (light_uniform_set_cache[slot].uniform_set.is_valid() && p_rd->uniform_set_is_valid(light_uniform_set_cache[slot].uniform_set)) {
		p_rd->free_rid(light_uniform_set_cache[slot].uniform_set);
	}
	light_uniform_set_cache[slot].shader = p_shader;
	for (uint32_t i = 0; i < 4u; ++i) {
		light_uniform_set_cache[slot].rids[i] = key_rids[i];
	}
	light_uniform_set_cache[slot].uniform_set = created;
	light_uniform_set_cache[slot].last_used_tick = light_uniform_set_tick;
	return created;
}

void ParticlesMeshLightSortRD::begin_cluster_builder_frame(
		const ClusterBuilderRD *p_builder,
		const Transform3D &p_view_xform,
		float p_z_near,
		float p_z_far,
		bool p_camera_orthogonal,
		float p_sphere_overfit,
		uint32_t p_max_elements_by_type) {
	active_cluster_context.builder = p_builder;
	active_cluster_context.view_xform = p_view_xform;
	active_cluster_context.z_near = p_z_near;
	active_cluster_context.z_far = p_z_far;
	active_cluster_context.camera_orthogonal = p_camera_orthogonal;
	active_cluster_context.sphere_overfit = p_sphere_overfit;
	active_cluster_context.max_elements_by_type = p_max_elements_by_type;
	active_cluster_context.pending_gpu_omni_count = 0;
	active_cluster_context.active = (p_builder != nullptr);
}

uint32_t ParticlesMeshLightSortRD::get_pending_cluster_gpu_omni_count(const ClusterBuilderRD *p_builder) const {
	if (!active_cluster_context.active || active_cluster_context.builder != p_builder) {
		return 0u;
	}
	return active_cluster_context.pending_gpu_omni_count;
}

void ParticlesMeshLightSortRD::clear_cluster_builder_frame(const ClusterBuilderRD *p_builder) {
	if (p_builder == nullptr || active_cluster_context.builder == p_builder) {
		active_cluster_context = ClusterFrameContext{};
	}
}

void ParticlesMeshLightSortRD::blit_cluster_gpu_omni_elements(
		RenderingDevice *p_rd,
		const ClusterBuilderRD *p_builder,
		RID p_dst_element_buffer,
		uint32_t p_dst_element_offset,
		uint32_t p_count) {
	if (!p_rd || !active_cluster_context.active || active_cluster_context.builder != p_builder || p_count == 0u || p_dst_element_buffer.is_null() || light_cluster_elements_buffer.is_null() || light_cluster_blit_pipeline.is_null()) {
		active_cluster_context.pending_gpu_omni_count = 0u;
		return;
	}
	RD::Uniform uniforms[2];
	uniforms[0].uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
	uniforms[0].binding = 0;
	uniforms[0].append_id(light_cluster_elements_buffer);
	uniforms[1].uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
	uniforms[1].binding = 1;
	uniforms[1].append_id(p_dst_element_buffer);
	const RID uset = _acquire_cached_uniform_set(p_rd, light_cluster_blit_shader, uniforms, 2u);
	if (uset.is_valid()) {
		const uint32_t push[4] = { p_dst_element_offset, MIN(p_count, MAX_PARTICLE_LIGHTS), 0u, 0u };
		RD::ComputeListID cl = p_rd->compute_list_begin();
		p_rd->compute_list_bind_compute_pipeline(cl, light_cluster_blit_pipeline);
		p_rd->compute_list_bind_uniform_set(cl, uset, 0);
		p_rd->compute_list_set_push_constant(cl, push, sizeof(push));
		p_rd->compute_list_dispatch(cl, 1u, 1u, 1u);
		p_rd->compute_list_end();
	}
	active_cluster_context.pending_gpu_omni_count = 0u;
}

uint32_t ParticlesMeshLightSortRD::dispatch_registered_external_lights_gpu(
		RenderingDevice *p_rd,
		RID p_source_viewport,
		RID p_source_scenario,
		bool p_is_reflection_probe,
		const Transform3D &p_view_inv_transform,
		float p_exposure_norm,
		RID p_omni_light_buffer,
		uint32_t p_base_omni_index,
		uint32_t p_max_omni_lights,
		LocalVector<ReservedGpuLightBounds> &r_mobile_bounds_out) {
	r_mobile_bounds_out.clear();
	if (active_cluster_context.active) {
		active_cluster_context.pending_gpu_omni_count = 0u;
	}
	if (!p_rd || p_omni_light_buffer.is_null() || p_base_omni_index >= p_max_omni_lights) {
		return 0u;
	}

	LocalVector<ExternalLightSlotRegistration> matched_slots;
	LocalVector<uint64_t> stale_owners;
	{
		MutexLock lock(active_lights_mutex);
		for (KeyValue<uint64_t, ExternalOwnerLightState> &E : external_owner_lights) {
			const ExternalOwnerLightState &st = E.value;
			if (!st.has_pending_rd_slot) {
				continue;
			}
			const ExternalLightSlotRegistration &reg = st.pending_slot;
			if (reg.capacity == 0u) {
				continue;
			}
			if (reg.meta_rid.is_null() || reg.records_rid.is_null() || reg.capacity > MAX_PARTICLE_LIGHTS) {
				stale_owners.push_back(E.key);
				continue;
			}
			if (reg.owner_ptr != nullptr && reg.is_slot_live_fn != nullptr) {
				if (!reg.is_slot_live_fn(reg.owner_ptr, reg.generation, reg.slot_index, reg.lease_serial)) {
					stale_owners.push_back(E.key);
					continue;
				}
			}
			if (reg.owner_ptr != nullptr && reg.is_slot_visible_fn != nullptr) {
				if (!reg.is_slot_visible_fn(reg.owner_ptr, reg.generation, reg.slot_index, reg.lease_serial)) {
					continue;
				}
			}
			// Strict Cross-World & Cross-Viewport isolation:
			if (reg.viewport_rid.is_valid()) {
				if (p_is_reflection_probe || p_source_viewport != reg.viewport_rid) {
					continue;
				}
			}
			if (reg.scenario_rid.is_valid() && p_source_scenario.is_valid() && p_source_scenario != reg.scenario_rid) {
				continue;
			}
			matched_slots.push_back(reg);
		}
		for (uint32_t i = 0; i < stale_owners.size(); ++i) {
			external_owner_lights.erase(stale_owners[i]);
		}
	}

	if (matched_slots.is_empty()) {
		return 0u;
	}

	std::sort(matched_slots.ptr(), matched_slots.ptr() + matched_slots.size(),
			[](const ExternalLightSlotRegistration &a, const ExternalLightSlotRegistration &b) {
				return a.owner_key < b.owner_key;
			});

	uint32_t total_requested_cap = 0u;
	for (uint32_t i = 0; i < matched_slots.size(); ++i) {
		total_requested_cap = MIN(MAX_PARTICLE_LIGHTS, total_requested_cap + matched_slots[i].capacity);
	}
	uint32_t avail_omni = p_max_omni_lights - p_base_omni_index;
	if (active_cluster_context.active && active_cluster_context.max_elements_by_type > p_base_omni_index) {
		avail_omni = MIN(avail_omni, active_cluster_context.max_elements_by_type - p_base_omni_index);
	} else if (active_cluster_context.active) {
		return 0u;
	}
	const uint32_t reserved_lights = MIN(total_requested_cap, MIN(MAX_PARTICLE_LIGHTS, avail_omni));
	if (reserved_lights == 0u) {
		return 0u;
	}

	if (_ensure_light_gpu_bridge(p_rd) != OK) {
		return 0u;
	}

	p_rd->buffer_clear(light_candidate_staging_buffer, 0, 16u);

	uint32_t harvested_slots = 0u;
	AABB union_world_bounds;
	bool has_bounds = false;
	{
		RD::ComputeListID cl = p_rd->compute_list_begin();
		p_rd->compute_list_bind_compute_pipeline(cl, light_harvest_pipeline);
		for (uint32_t i = 0; i < matched_slots.size(); ++i) {
			const ExternalLightSlotRegistration &reg = matched_slots[i];
			RID uset;
			if (reg.harvest_uniform_set.is_valid()) {
				if (p_rd->uniform_set_is_valid(reg.harvest_uniform_set)) {
					uset = reg.harvest_uniform_set;
				}
			} else {
				RD::Uniform u[3];
				u[0].uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u[0].binding = 0;
				u[0].append_id(reg.meta_rid);
				u[1].uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u[1].binding = 1;
				u[1].append_id(reg.records_rid);
				u[2].uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u[2].binding = 2;
				u[2].append_id(light_candidate_staging_buffer);
				uset = _acquire_cached_uniform_set(p_rd, light_harvest_shader, u, 3u);
			}
			if (uset.is_null()) {
				continue;
			}
			if (harvested_slots > 0u) {
				p_rd->compute_list_add_barrier(cl);
			}
			const uint32_t push[4] = {
				reg.capacity,
				MAX_STAGED_LIGHT_CANDIDATES,
				uint32_t(reg.present_seq & 0xFFFFFFFFull),
				uint32_t(reg.present_seq >> 32u),
			};
			p_rd->compute_list_bind_uniform_set(cl, uset, 0);
			p_rd->compute_list_set_push_constant(cl, push, sizeof(push));
			p_rd->compute_list_dispatch(cl, 1u, 1u, 1u);
			if (reg.world_bounds.size != Vector3()) {
				union_world_bounds = has_bounds ? union_world_bounds.merge(reg.world_bounds) : reg.world_bounds;
				has_bounds = true;
			}
			++harvested_slots;
		}
		p_rd->compute_list_end();
	}

	if (harvested_slots == 0u) {
		return 0u;
	}

	LightEmitParamsStd140 emit_params;
	for (int r = 0; r < 3; ++r) {
		emit_params.light_view_row0[r] = (r == 0) ? p_view_inv_transform.basis.rows[0][0] : emit_params.light_view_row0[r];
	}
	emit_params.light_view_row0[0] = p_view_inv_transform.basis.rows[0][0];
	emit_params.light_view_row0[1] = p_view_inv_transform.basis.rows[0][1];
	emit_params.light_view_row0[2] = p_view_inv_transform.basis.rows[0][2];
	emit_params.light_view_row0[3] = p_view_inv_transform.origin.x;
	emit_params.light_view_row1[0] = p_view_inv_transform.basis.rows[1][0];
	emit_params.light_view_row1[1] = p_view_inv_transform.basis.rows[1][1];
	emit_params.light_view_row1[2] = p_view_inv_transform.basis.rows[1][2];
	emit_params.light_view_row1[3] = p_view_inv_transform.origin.y;
	emit_params.light_view_row2[0] = p_view_inv_transform.basis.rows[2][0];
	emit_params.light_view_row2[1] = p_view_inv_transform.basis.rows[2][1];
	emit_params.light_view_row2[2] = p_view_inv_transform.basis.rows[2][2];
	emit_params.light_view_row2[3] = p_view_inv_transform.origin.z;

	Transform3D cluster_view = active_cluster_context.active ? active_cluster_context.view_xform : p_view_inv_transform;
	float cluster_scale = cluster_view.basis.get_uniform_scale();
	Transform3D cluster_ortho = cluster_view;
	if (cluster_scale < 0.98f || cluster_scale > 1.02f) {
		cluster_ortho.basis.orthonormalize();
	}
	emit_params.cluster_view_row0[0] = cluster_ortho.basis.rows[0][0];
	emit_params.cluster_view_row0[1] = cluster_ortho.basis.rows[0][1];
	emit_params.cluster_view_row0[2] = cluster_ortho.basis.rows[0][2];
	emit_params.cluster_view_row0[3] = cluster_view.origin.x;
	emit_params.cluster_view_row1[0] = cluster_ortho.basis.rows[1][0];
	emit_params.cluster_view_row1[1] = cluster_ortho.basis.rows[1][1];
	emit_params.cluster_view_row1[2] = cluster_ortho.basis.rows[1][2];
	emit_params.cluster_view_row1[3] = cluster_view.origin.y;
	emit_params.cluster_view_row2[0] = cluster_ortho.basis.rows[2][0];
	emit_params.cluster_view_row2[1] = cluster_ortho.basis.rows[2][1];
	emit_params.cluster_view_row2[2] = cluster_ortho.basis.rows[2][2];
	emit_params.cluster_view_row2[3] = cluster_view.origin.z;

	const float overfit = (active_cluster_context.active ? active_cluster_context.sphere_overfit : 1.05f) * cluster_scale;
	emit_params.misc_floats[0] = p_exposure_norm;
	emit_params.misc_floats[1] = active_cluster_context.active ? active_cluster_context.z_near : 0.05f;
	emit_params.misc_floats[2] = active_cluster_context.active ? active_cluster_context.z_far : 4000.0f;
	emit_params.misc_floats[3] = overfit;
	emit_params.counts[0] = p_base_omni_index;
	emit_params.counts[1] = reserved_lights;
	emit_params.counts[2] = MAX_PARTICLE_LIGHTS;
	emit_params.counts[3] = (active_cluster_context.active && active_cluster_context.camera_orthogonal) ? 1u : 0u;

	p_rd->buffer_update(light_emit_params_ubo, 0, sizeof(emit_params), &emit_params);

	RD::Uniform emit_u[4];
	emit_u[0].uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
	emit_u[0].binding = 0;
	emit_u[0].append_id(light_candidate_staging_buffer);
	emit_u[1].uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
	emit_u[1].binding = 1;
	emit_u[1].append_id(p_omni_light_buffer);
	emit_u[2].uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
	emit_u[2].binding = 2;
	emit_u[2].append_id(light_cluster_elements_buffer);
	emit_u[3].uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
	emit_u[3].binding = 3;
	emit_u[3].append_id(light_emit_params_ubo);
	const RID emit_uset = _acquire_cached_uniform_set(p_rd, light_topk_emit_shader, emit_u, 4u);
	if (emit_uset.is_null()) {
		return 0u;
	}

	{
		RD::ComputeListID cl = p_rd->compute_list_begin();
		p_rd->compute_list_bind_compute_pipeline(cl, light_topk_emit_pipeline);
		p_rd->compute_list_bind_uniform_set(cl, emit_uset, 0);
		p_rd->compute_list_dispatch(cl, 1u, 1u, 1u);
		p_rd->compute_list_end();
	}

	if (active_cluster_context.active) {
		active_cluster_context.pending_gpu_omni_count = reserved_lights;
	}
	if (has_bounds) {
		r_mobile_bounds_out.push_back({ union_world_bounds, p_base_omni_index, reserved_lights });
	}
	return reserved_lights;
}

void ParticlesMeshLightSortRD::clear_active_particle_lights() {
	MutexLock lock(active_lights_mutex);
	active_particle_lights.clear();
	external_owner_lights.clear();
}

void ParticlesMeshLightSortRD::get_active_particle_lights(LocalVector<ParticleLightRecord64> &r_out) const {
	MutexLock lock(active_lights_mutex);
	if (external_owner_lights.is_empty()) {
		r_out = active_particle_lights;
		return;
	}
	LocalVector<ParticleLightRecord64> merged(active_particle_lights);
	for (const KeyValue<uint64_t, ExternalOwnerLightState> &E : external_owner_lights) {
		const LocalVector<ParticleLightRecord64> &sub = E.value.compacted_lights;
		for (uint32_t i = 0; i < sub.size(); ++i) {
			merged.push_back(sub[i]);
		}
	}
	if (merged.is_empty()) {
		r_out.clear();
		return;
	}
	(void)compact_particle_lights_deterministic(merged, MAX_PARTICLE_LIGHTS, r_out);
}

uint32_t ParticlesMeshLightSortRD::get_active_particle_light_count() const {
	LocalVector<ParticleLightRecord64> out;
	get_active_particle_lights(out);
	return out.size();
}

} // namespace RendererRD
