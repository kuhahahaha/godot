/**************************************************************************/
/*  particles_external_guard_indexed144.h                                 */
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

#include "servers/rendering/renderer_rd/shaders/particles_external_guard_indexed144.glsl.gen.h"

#include <cstddef>
#include <cstdint>

namespace RendererRD {

// K05 INDEXED_144 ribbon/trail guard preparation and layout validator.
class ParticlesExternalGuardIndexed144 {
public:
	static constexpr uint32_t MAX_SEGMENTS = (1u << 18u); // 262,144 segments (1,048,576 vertices)
	static constexpr uint32_t VERTEX_STRIDE_BYTES = 144u;
	static constexpr uint32_t VERTICES_PER_SEGMENT = 4u;
	static constexpr uint32_t INDICES_PER_SEGMENT = 6u;

	enum Mode {
		INIT,
		VALIDATE,
		FINALIZE,
		INIT_EMPTY,
		FINALIZE_EMPTY,
		INIT_NO_MOTION,
		VALIDATE_NO_MOTION,
		FINALIZE_NO_MOTION,
		MODE_COUNT,
	};

	enum Binding {
		CURRENT_VERTICES = 0,
		ALIGNED_PREVIOUS_VERTICES = 1,
		VERTEX_HISTORY_VALIDITY = 2,
		INDEX_STAGING = 3,
		OUTPUT_META = 4,
		SOURCE_COUNT = 5,
		GUARD_SCRATCH = 6,
		GUARD_STATUS = 7,
		PROTECTED_COMMANDS = 8,
		GEOMETRY_PROOF = 9,
		SOURCE_COUNT_VALIDITY = 10,
		SOURCE_ATTRIBUTES_VALIDITY = 11,
		SOURCE_DENSE_IDS_VALIDITY = 12,
		SOURCE_INSTANCE_GATE = 13,
		BINDING_COUNT = 14,
	};

	struct PushConstant {
		uint32_t capacity = 0; // max_segments
		uint32_t sequence_low = 0;
		uint32_t sequence_high = 0;
		uint32_t surface_count = 0;
		uint32_t segment_base = 0;
		uint32_t segment_count = 0;
		uint32_t reserved0 = 0;
		uint32_t reserved1 = 0;
	};

	struct GeometryProofHeader {
		float current_decl_min[4] = {};
		float current_decl_max[4] = {};
		float previous_decl_min[4] = {};
		float previous_decl_max[4] = {};
		float cull_min[4] = {};
		float cull_max[4] = {};
		float current_host_inverse_rows[3][4] = {};
		float previous_host_inverse_rows[3][4] = {};
		uint32_t source_accessor[4] = {}; // count word, 0, 0, space (1=WORLD, 2=LOCAL)
		uint32_t source_gate_words[4] = {};
		uint32_t options[4] = {}; // motion_required (0/1), 0, 0, 0
	};

	struct RibbonSurfaceProof {
		float current_displacement_min[4] = {};
		float current_displacement_max[4] = {};
		float previous_displacement_min[4] = {};
		float previous_displacement_max[4] = {};
		uint32_t command[4] = {}; // {1, 6, 0, 0}
	};

	struct BufferBinding {
		RID rid;
		uint64_t byte_length = 0;
	};

	struct Bindings {
		BufferBinding buffers[BINDING_COUNT];
	};

	struct LayoutPlan {
		Mode initialize_mode = INIT;
		Mode validate_mode = VALIDATE;
		Mode finalize_mode = FINALIZE;
		PushConstant initialize;
		Vector<PushConstant> validation_chunks;
		Vector<uint32_t> validation_groups_x;
		PushConstant finalize;
		Vector<RD::Uniform> uniforms;
	};

private:
	ParticlesExternalGuardIndexed144ShaderRD *shader = nullptr;
	RID shader_version;
	RID pipelines[MODE_COUNT];
	bool initialized = false;
	bool ready = false;
#ifdef TESTS_ENABLED
	bool standalone_rd_test_owner = false;
#endif

	Error _require_main_render_thread() const;
	Error _clear_pipeline_prefix();
	Error _prepare_pipelines_impl();
	Error _release_pipelines_impl();

public:
	ParticlesExternalGuardIndexed144() = default;
	ParticlesExternalGuardIndexed144(const ParticlesExternalGuardIndexed144 &) = delete;
	ParticlesExternalGuardIndexed144 &operator=(const ParticlesExternalGuardIndexed144 &) = delete;
	~ParticlesExternalGuardIndexed144();

	Error prepare_pipelines();
#ifdef TESTS_ENABLED
	Error prepare_pipelines_standalone_rd();
	Error release_pipelines_standalone_rd();
#endif
	bool is_prepared() const { return ready; }
	RID get_pipeline(Mode p_mode) const;
	RID get_shader(Mode p_mode);
	Error release_pipelines();

	static Error plan_layout(
			const PushConstant &p_request,
			const Bindings &p_bindings,
			const GeometryProofHeader &p_proof,
			uint32_t p_max_groups_x,
			LayoutPlan &r_plan);

	Error record(
			RD::ComputeListID p_list,
			const LayoutPlan &p_plan,
			RID p_init_uniform_set,
			RID p_validate_uniform_set,
			RID p_finalize_uniform_set) const;
};

static_assert(ParticlesExternalGuardIndexed144::BINDING_COUNT == 14);
static_assert(sizeof(ParticlesExternalGuardIndexed144::PushConstant) == 32);
static_assert(sizeof(ParticlesExternalGuardIndexed144::GeometryProofHeader) == 240);
static_assert(offsetof(ParticlesExternalGuardIndexed144::GeometryProofHeader, source_accessor) == 192);
static_assert(offsetof(ParticlesExternalGuardIndexed144::GeometryProofHeader, source_gate_words) == 208);
static_assert(offsetof(ParticlesExternalGuardIndexed144::GeometryProofHeader, options) == 224);
static_assert(sizeof(ParticlesExternalGuardIndexed144::RibbonSurfaceProof) == 80);
static_assert(offsetof(ParticlesExternalGuardIndexed144::RibbonSurfaceProof, command) == 64);

} // namespace RendererRD
