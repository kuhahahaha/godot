/**************************************************************************/
/*  particles_external_guard_indexed144.cpp                               */
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

#include "particles_external_guard_indexed144.h"

#include "servers/rendering/renderer_rd/storage_rd/external_output_config.h"
#include "servers/rendering/rendering_server.h"

namespace RendererRD {

Error ParticlesExternalGuardIndexed144::_require_main_render_thread() const {
#ifdef TESTS_ENABLED
	if (standalone_rd_test_owner) {
		return RD::get_singleton() != nullptr ? OK : ERR_UNAVAILABLE;
	}
#endif
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs || !rs->is_on_render_thread() || !RD::get_singleton() || rs->get_rendering_device() != RD::get_singleton()) {
		return ERR_UNAVAILABLE;
	}
	return OK;
}

Error ParticlesExternalGuardIndexed144::_clear_pipeline_prefix() {
	ready = false;
	RD *rd = RD::get_singleton();
	for (int i = MODE_COUNT - 1; i >= 0; i--) {
		if (pipelines[i].is_valid()) {
			if (rd->compute_pipeline_is_valid(pipelines[i])) {
				rd->free_rid(pipelines[i]);
			}
			pipelines[i] = RID();
		}
	}
	if (shader_version.is_valid()) {
		if (!shader->version_free(shader_version)) {
			return ERR_BUG;
		}
		shader_version = RID();
	}
	return OK;
}

ParticlesExternalGuardIndexed144::~ParticlesExternalGuardIndexed144() {
	if (shader) {
		ERR_FAIL_COND_MSG(_require_main_render_thread() != OK, "Indexed144 external guard pipelines must be retired and destroyed on the render thread.");
		ERR_FAIL_COND_MSG(_clear_pipeline_prefix() != OK, "Indexed144 external guard shader version could not be released.");
		memdelete(shader);
	}
}

Error ParticlesExternalGuardIndexed144::_prepare_pipelines_impl() {
	if (ready) {
		return OK;
	}
	if (shader_version.is_valid() && _clear_pipeline_prefix() != OK) {
		return ERR_BUG;
	}
	RD *rd = RD::get_singleton();
	if (!rd ||
			rd->limit_get(RD::LIMIT_MAX_PUSH_CONSTANT_SIZE) < sizeof(PushConstant) ||
			rd->limit_get(RD::LIMIT_MAX_STORAGE_BUFFERS_PER_UNIFORM_SET) < BINDING_COUNT ||
			rd->limit_get(RD::LIMIT_MAX_STORAGE_BUFFERS_PER_SHADER_STAGE) < BINDING_COUNT ||
			rd->limit_get(RD::LIMIT_MAX_COMPUTE_WORKGROUP_SIZE_X) < 64 ||
			rd->limit_get(RD::LIMIT_MAX_COMPUTE_WORKGROUP_INVOCATIONS) < 64 ||
			rd->limit_get(RD::LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X) == 0) {
		return ERR_UNAVAILABLE;
	}
	if (!initialized) {
		Vector<String> modes;
		modes.push_back("\n#define MODE_INIT\n");
		modes.push_back("\n#define MODE_VALIDATE\n");
		modes.push_back("\n#define MODE_FINALIZE\n");
		modes.push_back("\n#define MODE_INIT\n#define MODE_EMPTY\n");
		modes.push_back("\n#define MODE_FINALIZE\n#define MODE_EMPTY\n");
		modes.push_back("\n#define MODE_INIT\n#define MOTION_DISABLED\n");
		modes.push_back("\n#define MODE_VALIDATE\n#define MOTION_DISABLED\n");
		modes.push_back("\n#define MODE_FINALIZE\n#define MOTION_DISABLED\n");
		shader = memnew(ParticlesExternalGuardIndexed144ShaderRD);
		shader->initialize(modes, "\n#define EXTERNAL_MAX_SURFACES " + uitos(ExternalOutputConfig::MAX_FLATTENED_SURFACES) + "u\n");
		initialized = true;
	}
	shader_version = shader->version_create();
	if (shader_version.is_null()) {
		return ERR_CANT_CREATE;
	}
	for (int i = 0; i < MODE_COUNT; i++) {
		const RID variant = shader->version_get_shader(shader_version, i);
		if (variant.is_null() || !shader->version_is_valid(shader_version)) {
			const Error cleanup = _clear_pipeline_prefix();
			return cleanup == OK ? ERR_CANT_CREATE : cleanup;
		}
		pipelines[i] = rd->compute_pipeline_create(variant);
		if (pipelines[i].is_null() || !rd->compute_pipeline_is_valid(pipelines[i])) {
			const Error cleanup = _clear_pipeline_prefix();
			return cleanup == OK ? ERR_CANT_CREATE : cleanup;
		}
	}
	ready = true;
	return OK;
}

Error ParticlesExternalGuardIndexed144::_release_pipelines_impl() {
	const Error cleanup = _clear_pipeline_prefix();
	if (cleanup == OK && shader) {
		memdelete(shader);
		shader = nullptr;
		initialized = false;
	}
	return cleanup;
}

Error ParticlesExternalGuardIndexed144::prepare_pipelines() {
	if (_require_main_render_thread() != OK) {
		return ERR_UNAVAILABLE;
	}
	return _prepare_pipelines_impl();
}

#ifdef TESTS_ENABLED
Error ParticlesExternalGuardIndexed144::prepare_pipelines_standalone_rd() {
	if (RD::get_singleton() == nullptr) {
		return ERR_UNAVAILABLE;
	}
	standalone_rd_test_owner = true;
	const Error err = _prepare_pipelines_impl();
	if (err != OK) {
		standalone_rd_test_owner = false;
	}
	return err;
}

Error ParticlesExternalGuardIndexed144::release_pipelines_standalone_rd() {
	if (!standalone_rd_test_owner || RD::get_singleton() == nullptr) {
		return ERR_UNAVAILABLE;
	}
	const Error cleanup = _release_pipelines_impl();
	standalone_rd_test_owner = false;
	return cleanup;
}
#endif

RID ParticlesExternalGuardIndexed144::get_pipeline(Mode p_mode) const {
	if (!ready || p_mode < INIT || p_mode >= MODE_COUNT || _require_main_render_thread() != OK) {
		return RID();
	}
	return pipelines[p_mode];
}

RID ParticlesExternalGuardIndexed144::get_shader(Mode p_mode) {
	if (!ready || p_mode < INIT || p_mode >= MODE_COUNT || _require_main_render_thread() != OK) {
		return RID();
	}
	return shader->version_get_shader(shader_version, p_mode);
}

Error ParticlesExternalGuardIndexed144::release_pipelines() {
	if (_require_main_render_thread() != OK) {
		return ERR_UNAVAILABLE;
	}
	return _release_pipelines_impl();
}

Error ParticlesExternalGuardIndexed144::plan_layout(
		const PushConstant &p_request,
		const Bindings &p_bindings,
		const GeometryProofHeader &p_proof,
		uint32_t p_max_groups_x,
		LayoutPlan &r_plan) {
	if (p_request.capacity > MAX_SEGMENTS ||
			p_request.surface_count == 0 ||
			p_request.surface_count > ExternalOutputConfig::MAX_FLATTENED_SURFACES ||
			p_request.segment_base != 0 ||
			p_request.segment_count != p_request.capacity ||
			p_request.reserved0 != 0 ||
			p_request.reserved1 != 0 ||
			p_max_groups_x == 0) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_proof.options[0] > 1 || p_proof.options[1] != 0 || p_proof.options[2] != 0 || p_proof.options[3] != 0) {
		return ERR_INVALID_PARAMETER;
	}

	const bool motion_required = (p_proof.options[0] == 1u);
	const uint64_t vertex_count = uint64_t(p_request.capacity) * VERTICES_PER_SEGMENT;
	const uint64_t current_vertices_bytes = vertex_count * VERTEX_STRIDE_BYTES;
	// In non-empty validate dispatches, bindings 1 and 2 must remain valid storage
	// descriptors in SPIR-V; when motion is disabled, allow either exact full-size
	// or a minimal 4-byte sentinel buffer, while motion_required mandates full size.
	const uint64_t prev_vertices_bytes = motion_required ? current_vertices_bytes : 0;
	const uint64_t history_bytes = motion_required ? (vertex_count * 4u) : 0;
	const uint64_t index_bytes = uint64_t(p_request.capacity) * INDICES_PER_SEGMENT * 4u;
	const uint64_t commands_bytes = uint64_t(p_request.surface_count) * 20u;
	const uint64_t proof_bytes = sizeof(GeometryProofHeader) + uint64_t(p_request.surface_count) * sizeof(RibbonSurfaceProof);

	const uint64_t expected[BINDING_COUNT] = {
		current_vertices_bytes, // 0 CURRENT_VERTICES
		prev_vertices_bytes, // 1 ALIGNED_PREVIOUS_VERTICES
		history_bytes, // 2 VERTEX_HISTORY_VALIDITY
		index_bytes, // 3 INDEX_STAGING
		32, // 4 OUTPUT_META
		0, // 5 SOURCE_COUNT
		64, // 6 GUARD_SCRATCH
		32, // 7 GUARD_STATUS
		commands_bytes, // 8 PROTECTED_COMMANDS
		proof_bytes, // 9 GEOMETRY_PROOF
		0, // 10 SOURCE_COUNT_VALIDITY
		0, // 11 SOURCE_ATTRIBUTES_VALIDITY
		0, // 12 SOURCE_DENSE_IDS_VALIDITY
		0, // 13 SOURCE_INSTANCE_GATE
	};

	auto is_source = [](uint32_t p_binding) {
		return p_binding == SOURCE_COUNT || (p_binding >= SOURCE_COUNT_VALIDITY && p_binding <= SOURCE_INSTANCE_GATE);
	};

	bool motion_buffers_omitted = false;
	for (uint32_t i = 0; i < BINDING_COUNT; i++) {
		const BufferBinding &buffer = p_bindings.buffers[i];
		if (p_request.capacity == 0 && (i >= CURRENT_VERTICES && i <= INDEX_STAGING)) {
			if (buffer.rid.is_valid() || buffer.byte_length != 0) {
				return ERR_INVALID_PARAMETER;
			}
			continue;
		}
		if (!motion_required && p_request.capacity > 0 && (i == ALIGNED_PREVIOUS_VERTICES || i == VERTEX_HISTORY_VALIDITY)) {
			const BufferBinding &prev_buf = p_bindings.buffers[ALIGNED_PREVIOUS_VERTICES];
			const BufferBinding &hist_buf = p_bindings.buffers[VERTEX_HISTORY_VALIDITY];
			if (prev_buf.rid.is_null() && prev_buf.byte_length == 0 && hist_buf.rid.is_null() && hist_buf.byte_length == 0) {
				motion_buffers_omitted = true;
				continue;
			}
			if (buffer.rid.is_null() || buffer.byte_length < 4 || (buffer.byte_length & 3) != 0 || buffer.byte_length > UINT32_MAX) {
				return ERR_INVALID_PARAMETER;
			}
			for (uint32_t j = 0; j < i; j++) {
				if (buffer.rid == p_bindings.buffers[j].rid) {
					return ERR_INVALID_PARAMETER;
				}
			}
			continue;
		}
		if (buffer.byte_length > UINT32_MAX || (buffer.byte_length & 3) != 0 ||
				(is_source(i) ? buffer.byte_length < 4 : buffer.byte_length != expected[i])) {
			return ERR_INVALID_PARAMETER;
		}
		if (buffer.rid.is_null()) {
			return ERR_INVALID_PARAMETER;
		}
		for (uint32_t j = 0; j < i; j++) {
			if (buffer.rid == p_bindings.buffers[j].rid &&
					(!is_source(i) || !is_source(j) || buffer.byte_length != p_bindings.buffers[j].byte_length)) {
				return ERR_INVALID_PARAMETER;
			}
		}
	}

	if (p_proof.source_accessor[0] >= p_bindings.buffers[SOURCE_COUNT].byte_length / 4 ||
			p_proof.source_accessor[1] != 0 ||
			p_proof.source_accessor[2] != 0 ||
			(p_proof.source_accessor[3] != 1 && p_proof.source_accessor[3] != 2)) {
		return ERR_INVALID_PARAMETER;
	}
	for (uint32_t gate = 0; gate < 4; gate++) {
		if (p_proof.source_gate_words[gate] >= p_bindings.buffers[SOURCE_COUNT_VALIDITY + gate].byte_length / 4) {
			return ERR_INVALID_PARAMETER;
		}
	}

	LayoutPlan plan;
	if (p_request.capacity == 0) {
		plan.initialize_mode = INIT_EMPTY;
		plan.validate_mode = VALIDATE;
		plan.finalize_mode = FINALIZE_EMPTY;
	} else if (motion_buffers_omitted) {
		plan.initialize_mode = INIT_NO_MOTION;
		plan.validate_mode = VALIDATE_NO_MOTION;
		plan.finalize_mode = FINALIZE_NO_MOTION;
	}
	plan.initialize = p_request;
	plan.initialize.segment_count = 0;
	plan.finalize = plan.initialize;

	const uint64_t chunk_capacity = MIN(uint64_t(p_max_groups_x) * 64u, uint64_t(p_request.capacity));
	for (uint32_t base = 0; base < p_request.capacity;) {
		PushConstant chunk = p_request;
		chunk.segment_base = base;
		chunk.segment_count = uint32_t(MIN(chunk_capacity, uint64_t(p_request.capacity - base)));
		plan.validation_chunks.push_back(chunk);
		plan.validation_groups_x.push_back((chunk.segment_count + 63u) / 64u);
		base += chunk.segment_count;
	}
	for (uint32_t i = 0; i < BINDING_COUNT; i++) {
		if (p_bindings.buffers[i].rid.is_valid()) {
			plan.uniforms.push_back(RD::Uniform(RD::UNIFORM_TYPE_STORAGE_BUFFER, i, p_bindings.buffers[i].rid));
		}
	}
	r_plan = plan;
	return OK;
}

Error ParticlesExternalGuardIndexed144::record(
		RD::ComputeListID p_list,
		const LayoutPlan &p_plan,
		RID p_init_uniform_set,
		RID p_validate_uniform_set,
		RID p_finalize_uniform_set) const {
	if (!ready || _require_main_render_thread() != OK) {
		return ERR_UNAVAILABLE;
	}
	RD *rd = RD::get_singleton();
	if (!rd || p_init_uniform_set.is_null() || p_finalize_uniform_set.is_null() ||
			(!p_plan.validation_chunks.is_empty() && p_validate_uniform_set.is_null())) {
		return ERR_INVALID_PARAMETER;
	}
	RID init_pipe = get_pipeline(p_plan.initialize_mode);
	RID finalize_pipe = get_pipeline(p_plan.finalize_mode);
	RID validate_pipe = p_plan.validation_chunks.is_empty() ? RID() : get_pipeline(p_plan.validate_mode);
	if (init_pipe.is_null() || finalize_pipe.is_null() || (!p_plan.validation_chunks.is_empty() && validate_pipe.is_null())) {
		return ERR_UNAVAILABLE;
	}

	rd->compute_list_bind_compute_pipeline(p_list, init_pipe);
	rd->compute_list_bind_uniform_set(p_list, p_init_uniform_set, 0);
	rd->compute_list_set_push_constant(p_list, &p_plan.initialize, sizeof(PushConstant));
	rd->compute_list_dispatch(p_list, 1, 1, 1);
	rd->compute_list_add_barrier(p_list);

	for (int i = 0; i < p_plan.validation_chunks.size(); i++) {
		rd->compute_list_bind_compute_pipeline(p_list, validate_pipe);
		rd->compute_list_bind_uniform_set(p_list, p_validate_uniform_set, 0);
		rd->compute_list_set_push_constant(p_list, &p_plan.validation_chunks[i], sizeof(PushConstant));
		rd->compute_list_dispatch(p_list, p_plan.validation_groups_x[i], 1, 1);
	}
	if (!p_plan.validation_chunks.is_empty()) {
		rd->compute_list_add_barrier(p_list);
	}

	rd->compute_list_bind_compute_pipeline(p_list, finalize_pipe);
	rd->compute_list_bind_uniform_set(p_list, p_finalize_uniform_set, 0);
	rd->compute_list_set_push_constant(p_list, &p_plan.finalize, sizeof(PushConstant));
	rd->compute_list_dispatch(p_list, 1, 1, 1);
	return OK;
}

} // namespace RendererRD
