/**************************************************************************/
/*  particles_external_guard.cpp                                          */
/**************************************************************************/

#include "particles_external_guard.h"

#include "servers/rendering/rendering_server.h"

namespace RendererRD {

Error ParticlesExternalGuard::_require_main_render_thread() const {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs || !rs->is_on_render_thread() || !RD::get_singleton() || rs->get_rendering_device() != RD::get_singleton()) {
		return ERR_UNAVAILABLE;
	}
	return OK;
}

Error ParticlesExternalGuard::_clear_pipeline_prefix() {
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

ParticlesExternalGuard::~ParticlesExternalGuard() {
	if (shader) {
		// No scheduler or completion guess is introduced here. A wrong-thread
		// destruction is an owner bug; retain native objects rather than free them
		// off-thread via ShaderRD's destructor. The owner must release on main RT.
		ERR_FAIL_COND_MSG(_require_main_render_thread() != OK, "External guard pipelines must be retired and destroyed by their main-RD owner on the render thread.");
		ERR_FAIL_COND_MSG(_clear_pipeline_prefix() != OK, "External guard shader version could not be released.");
		memdelete(shader);
	}
}

Error ParticlesExternalGuard::prepare_pipelines() {
	if (_require_main_render_thread() != OK) {
		return ERR_UNAVAILABLE;
	}
	if (ready) {
		return OK;
	}
	if (shader_version.is_valid() && _clear_pipeline_prefix() != OK) {
		return ERR_BUG;
	}
	RD *rd = RD::get_singleton();
	if (rd->limit_get(RD::LIMIT_MAX_PUSH_CONSTANT_SIZE) < sizeof(PushConstant) ||
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
		shader = memnew(ParticlesExternalGuardShaderRD);
		shader->initialize(modes);
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

RID ParticlesExternalGuard::get_pipeline(Mode p_mode) const {
	if (!ready || p_mode < INIT || p_mode >= MODE_COUNT || _require_main_render_thread() != OK) {
		return RID();
	}
	return pipelines[p_mode];
}

RID ParticlesExternalGuard::get_shader(Mode p_mode) {
	if (!ready || p_mode < INIT || p_mode >= MODE_COUNT || _require_main_render_thread() != OK) {
		return RID();
	}
	return shader->version_get_shader(shader_version, p_mode);
}

Error ParticlesExternalGuard::release_pipelines() {
	if (_require_main_render_thread() != OK) {
		return ERR_UNAVAILABLE;
	}
	const Error cleanup = _clear_pipeline_prefix();
	if (cleanup == OK && shader) {
		memdelete(shader);
		shader = nullptr;
		initialized = false;
	}
	return cleanup;
}

Error ParticlesExternalGuard::plan_layout(const PushConstant &p_request, const Bindings &p_bindings, const GeometryProofHeader &p_proof, uint32_t p_max_groups_x, LayoutPlan &r_plan) {
	if (p_request.capacity > (1u << 20) || p_request.surface_count == 0 || p_request.surface_count > 8 ||
			p_request.record_base != 0 || p_request.record_count != p_request.capacity || p_request.reserved0 != 0 || p_request.reserved1 != 0 || p_max_groups_x == 0) {
		return ERR_INVALID_PARAMETER;
	}
	const uint64_t records_bytes = uint64_t(p_request.capacity) * 192;
	const uint64_t expected[BINDING_COUNT] = { records_bytes, records_bytes, 32, 0, 64, 32, uint64_t(p_request.surface_count) * 20, 224 + uint64_t(p_request.surface_count) * 144 };
	for (uint32_t i = 0; i < BINDING_COUNT; i++) {
		const BufferBinding &buffer = p_bindings.buffers[i];
		if (buffer.byte_length > UINT32_MAX || (buffer.byte_length & 3) != 0 ||
				(i == SOURCE_BACKING ? buffer.byte_length < 4 : buffer.byte_length != expected[i])) {
			return ERR_INVALID_PARAMETER;
		}
		const bool absent_records = p_request.capacity == 0 && (i == CURRENT_RECORDS || i == PREVIOUS_RECORDS);
		if (absent_records ? buffer.rid.is_valid() : buffer.rid.is_null()) {
			return ERR_INVALID_PARAMETER;
		}
		// All blocks use restrict. Writable status/scratch/commands must never
		// alias source, metadata, records or proof. Backing aliases across distinct
		// RIDs still require the original owner's exact allocation proof.
		for (uint32_t j = 0; j < i; j++) {
			if (buffer.rid.is_valid() && buffer.rid == p_bindings.buffers[j].rid) {
				return ERR_INVALID_PARAMETER;
			}
		}
	}
	const uint64_t source_words = p_bindings.buffers[SOURCE_BACKING].byte_length / 4;
	if (p_proof.source_accessor[0] >= source_words || p_proof.source_accessor[1] >= source_words ||
			(p_proof.source_accessor[2] != 1 && p_proof.source_accessor[2] != 2) ||
			(p_proof.source_accessor[3] != 1 && p_proof.source_accessor[3] != 2) ||
			p_proof.options[0] > 1 || p_proof.options[1] != 0 || p_proof.options[2] != 0 || p_proof.options[3] != 0) {
		return ERR_INVALID_PARAMETER;
	}
	LayoutPlan plan;
	if (p_request.capacity == 0) {
		// These variants omit record descriptors entirely. No zero-sized or
		// dummy particle buffer is required by their reflected layout.
		plan.initialize_mode = INIT_EMPTY;
		plan.finalize_mode = FINALIZE_EMPTY;
	}
	plan.initialize = p_request;
	plan.initialize.record_count = 0;
	plan.finalize = plan.initialize;
	// Each mode needs its own closed dispatch with an intervening compute
	// barrier. No aggregate status may be finalized inside validation dispatch.
	const uint64_t chunk_capacity = MIN(uint64_t(p_max_groups_x) * 64, uint64_t(p_request.capacity));
	for (uint32_t base = 0; base < p_request.capacity;) {
		PushConstant chunk = p_request;
		chunk.record_base = base;
		chunk.record_count = uint32_t(MIN(chunk_capacity, uint64_t(p_request.capacity - base)));
		plan.validation_chunks.push_back(chunk);
		plan.validation_groups_x.push_back((chunk.record_count + 63) / 64);
		base += chunk.record_count;
	}
	for (uint32_t i = 0; i < BINDING_COUNT; i++) {
		if (p_bindings.buffers[i].rid.is_valid()) {
			plan.uniforms.push_back(RD::Uniform(RD::UNIFORM_TYPE_STORAGE_BUFFER, i, p_bindings.buffers[i].rid));
		}
	}
	r_plan = plan;
	return OK;
}

} // namespace RendererRD
