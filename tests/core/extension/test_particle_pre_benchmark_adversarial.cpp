/**************************************************************************/
/*  test_particle_pre_benchmark_adversarial.cpp                           */
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

#include "core/extension/gdextension.h"
#include "core/extension/gdextension_core_evidence_capsule.h"
#include "core/extension/gdextension_production_output_ribbon.h"
#include "core/extension/gdextension_production_request.h"
#include "core/extension/gdextension_scene_late_depth.h"
#include "core/io/file_access.h"
#include "core/io/marshalls.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "servers/rendering/renderer_rd/effects/particles_external_guard.h"
#include "servers/rendering/renderer_rd/effects/particles_external_guard_indexed144.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_config.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_collision_scene_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_mesh_light_sort_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_ribbon_rd.h"
#include "servers/rendering/rendering_device.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

#if defined(UNIX_ENABLED) || defined(MACOS_ENABLED)
#include <unistd.h>
#endif

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
#include "drivers/metal/rendering_context_driver_metal.h"
#endif

TEST_FORCE_LINK(test_particle_pre_benchmark_adversarial)

namespace TestParticlePreBenchmarkAdversarial {

static bool float_word_is_guard_clean(uint32_t p_bits) {
	const uint32_t mag = p_bits & 0x7fffffffu;
	return mag == 0u || (mag >= 0x00800000u && mag < 0x7f800000u);
}

static GDExtensionProductionOutputPrepareV3 make_baseline_prepare_v3_indexed144(bool p_motion_required, uint32_t p_segments = 4u) {
	using namespace GDExtensionDeviceExit;
	GDExtensionProductionOutputPrepareV3 prep = {};
	prep.abi = { 3u, uint32_t(sizeof(GDExtensionProductionOutputPrepareV3)), 0u };
	prep.action_ordinal = 1u;
	prep.canonical_suboperation = 1u;
	prep.source_recipe_substep = 0u;
	prep.source_resource_slot = 0u;
	prep.source_use_index = 0u;
	prep.reserved = 0u;
	prep.source_generation = 1u;
	prep.first_source_use = 0u;
	prep.source_use_count = 1u;
	prep.read_capacity = 2u;
	prep.read_graph_capacity = 4u;

	for (uint32_t i = 0; i < 8u; ++i) {
		prep.roles[i] = { 0u, UINT32_MAX, UINT32_MAX, 0u, 0u, 0u };
	}

	const uint64_t curr_bytes = uint64_t(p_segments) * 576ull;
	const uint64_t idx_bytes = uint64_t(p_segments) * 24ull;
	if (p_motion_required) {
		prep.roles[0] = { GDEXTENSION_OUTPUT_ROLE_CURRENT_VERTICES_V1, 1u, 1u, 0u, 10u, curr_bytes };
		prep.roles[1] = { GDEXTENSION_OUTPUT_ROLE_ALIGNED_PREVIOUS_VERTICES_V1, 2u, 2u, 0u, 11u, curr_bytes };
		prep.roles[2] = { GDEXTENSION_OUTPUT_ROLE_VERTEX_HISTORY_VALIDITY_V1, 3u, 3u, 0u, 12u, uint64_t(p_segments) * 16ull };
		prep.roles[3] = { GDEXTENSION_OUTPUT_ROLE_INDEX_STAGING_V1, 4u, 4u, 0u, 13u, idx_bytes };
		prep.roles[4] = { GDEXTENSION_OUTPUT_ROLE_OUTPUT_META_V1, 5u, 5u, 0u, 14u, 32ull };
	} else {
		prep.roles[0] = { GDEXTENSION_OUTPUT_ROLE_CURRENT_VERTICES_V1, 1u, 1u, 0u, 10u, curr_bytes };
		prep.roles[1] = { GDEXTENSION_OUTPUT_ROLE_INDEX_STAGING_V1, 4u, 4u, 0u, 13u, idx_bytes };
		prep.roles[2] = { GDEXTENSION_OUTPUT_ROLE_OUTPUT_META_V1, 5u, 5u, 0u, 14u, 32ull };
	}

	prep.input.abi = production_dto_abi<GDExtensionOutputAcquireInputV1>();
	prep.input.particles = 101ull;
	prep.input.common.generation = 1ull;
	prep.input.common.present_seq = 1ull;
	prep.input.common.bounds.position[0] = -4.0;
	prep.input.common.bounds.position[1] = -4.0;
	prep.input.common.bounds.position[2] = -4.0;
	prep.input.common.bounds.size[0] = 8.0;
	prep.input.common.bounds.size[1] = 8.0;
	prep.input.common.bounds.size[2] = 8.0;
	prep.input.common.history_reset = 0u;
	prep.input.common.has_actual_previous_camera = 0u;
	prep.input.common.has_previous_consumed_tuple = 0u;
	prep.input.common.current_camera = { 1ull, 1ull, 1ull };
	prep.input.content_version = 1ull;
	prep.input.view_epoch = 1ull;
	prep.input.pass_mask = RendererRD::ExternalOutputConfig::PASS_OPAQUE;
	return prep;
}

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
static RID compile_compute_shader_glsl(RenderingDevice &p_rd, const String &p_glsl) {
	String err;
	RenderingDevice::ShaderStageSPIRVData stage;
	stage.shader_stage = RenderingDevice::SHADER_STAGE_COMPUTE;
	stage.spirv = p_rd.shader_compile_spirv_from_source(RenderingDevice::SHADER_STAGE_COMPUTE, p_glsl, RenderingDevice::SHADER_LANGUAGE_GLSL, &err, false);
	REQUIRE_MESSAGE(!stage.spirv.is_empty(), err.utf8().get_data());
	Vector<RenderingDevice::ShaderStageSPIRVData> stages;
	stages.push_back(stage);
	RID shader = p_rd.shader_create_from_spirv(stages);
	REQUIRE(shader.is_valid());
	return shader;
}

static RID compile_raster_shader_glsl(RenderingDevice &p_rd, const String &p_vert_glsl, const String &p_frag_glsl) {
	String vert_err;
	RenderingDevice::ShaderStageSPIRVData vert_stage;
	vert_stage.shader_stage = RenderingDevice::SHADER_STAGE_VERTEX;
	vert_stage.spirv = p_rd.shader_compile_spirv_from_source(RenderingDevice::SHADER_STAGE_VERTEX, p_vert_glsl, RenderingDevice::SHADER_LANGUAGE_GLSL, &vert_err, false);
	REQUIRE_MESSAGE(!vert_stage.spirv.is_empty(), vert_err.utf8().get_data());

	String frag_err;
	RenderingDevice::ShaderStageSPIRVData frag_stage;
	frag_stage.shader_stage = RenderingDevice::SHADER_STAGE_FRAGMENT;
	frag_stage.spirv = p_rd.shader_compile_spirv_from_source(RenderingDevice::SHADER_STAGE_FRAGMENT, p_frag_glsl, RenderingDevice::SHADER_LANGUAGE_GLSL, &frag_err, false);
	REQUIRE_MESSAGE(!frag_stage.spirv.is_empty(), frag_err.utf8().get_data());

	Vector<RenderingDevice::ShaderStageSPIRVData> stages;
	stages.push_back(vert_stage);
	stages.push_back(frag_stage);
	RID shader = p_rd.shader_create_from_spirv(stages);
	REQUIRE(shader.is_valid());
	return shader;
}
#endif

// ============================================================================
// CATEGORY 1: Compute -> GuardIndexed144 -> BufferCopy -> Indirect Draw /
//             Vertex / Index / Motion MRT Same-Frame Barrier & Multi-Flight Sync
// ============================================================================
TEST_CASE("[particle_pre_benchmark_adversarial] Cat-1 Live Metal compute->GuardIndexed144->buffer_copy->indirect MRT draw & multi-flight sync") {
#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
	// Live Metal GPU: External Compute Writer -> ParticlesExternalGuardIndexed144 ->
	// Same-Frame Index Buffer Copy -> Indirect Indexed MRT Draw (Color + Motion Velocity) across 6 frames!
	REQUIRE(RenderingDevice::get_singleton() == nullptr);
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	using Guard = RendererRD::ParticlesExternalGuardIndexed144;
	using RibbonRD = RendererRD::ParticlesRibbonRD;

	Guard guard;
	REQUIRE(guard.prepare_pipelines_standalone_rd() == OK);

	RibbonRD::Indexed144Layout layout;
	REQUIRE(RibbonRD::plan_indexed144(1, true, 1, layout) == OK);
	RibbonRD::AllocatedBuffers bufs;
	REQUIRE(RibbonRD::allocate_buffers_rd(rd, layout, bufs) == OK);

	Vector<uint8_t> source_init;
	source_init.resize(32);
	std::memset(source_init.ptrw(), 0, 32);
	encode_uint32(1u, source_init.ptrw() + 0);
	const RID source_rid = rd.storage_buffer_create(32, source_init);
	REQUIRE(source_rid.is_valid());

	Guard::GeometryProofHeader proof = {};
	for (int k = 0; k < 3; ++k) {
		proof.current_decl_min[k] = -4.0f;
		proof.current_decl_max[k] = 4.0f;
		proof.previous_decl_min[k] = -4.0f;
		proof.previous_decl_max[k] = 4.0f;
		proof.cull_min[k] = -8.0f;
		proof.cull_max[k] = 8.0f;
	}
	proof.source_accessor[0] = 0;
	proof.source_accessor[3] = 1;
	proof.source_gate_words[0] = 1;
	proof.source_gate_words[1] = 2;
	proof.source_gate_words[2] = 3;
	proof.source_gate_words[3] = 4;
	proof.options[0] = 1; // motion_required = true

	Guard::RibbonSurfaceProof surf = {};
	surf.command[0] = 1;
	surf.command[1] = 6;

	Vector<uint8_t> proof_bytes;
	proof_bytes.resize(sizeof(proof) + sizeof(surf));
	std::memcpy(proof_bytes.ptrw(), &proof, sizeof(proof));
	std::memcpy(proof_bytes.ptrw() + sizeof(proof), &surf, sizeof(surf));
	REQUIRE(rd.buffer_update(bufs.rids[RibbonRD::GEOMETRY_PROOF], 0, proof_bytes.size(), proof_bytes.ptr()) == OK);

	// Compute writer shader: writes 4 Float144Vertex (36 uint words each = 144B), previous vertices,
	// history validity, 6 triangle indices, and OUTPUT_META directly on the GPU!
	const String writer_glsl = R"GLSL(
#version 450
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0, std430) restrict writeonly buffer CurrVerts { uint curr_words[]; };
layout(set = 0, binding = 1, std430) restrict writeonly buffer PrevVerts { uint prev_words[]; };
layout(set = 0, binding = 2, std430) restrict writeonly buffer HistValid { uint valid_flags[]; };
layout(set = 0, binding = 3, std430) restrict writeonly buffer IndexStaging { uint indices[]; };
layout(set = 0, binding = 4, std430) restrict writeonly buffer OutputMeta { uint meta_words[]; };

layout(push_constant, std430) uniform Params {
	float red_chan;
	float green_chan;
	float motion_dx;
	float motion_dy;
	uint seq_lo;
	uint seq_hi;
	uint pad0;
	uint pad1;
} params;

void write_vertex(uint v, vec2 pos_xy, vec2 prev_xy, vec4 col) {
	uint base = v * 36u;
	for (uint i = 0u; i < 36u; ++i) {
		curr_words[base + i] = 0u;
		prev_words[base + i] = 0u;
	}
	// position (words 0..2), normal = (0,0,1) (words 4..6), tangent = (1,0,0,1) (words 8..11)
	curr_words[base + 0u] = floatBitsToUint(pos_xy.x);
	curr_words[base + 1u] = floatBitsToUint(pos_xy.y);
	curr_words[base + 2u] = floatBitsToUint(0.0);
	curr_words[base + 6u] = floatBitsToUint(1.0);
	curr_words[base + 8u] = floatBitsToUint(1.0);
	curr_words[base + 11u] = floatBitsToUint(1.0);
	// color (words 12..15)
	curr_words[base + 12u] = floatBitsToUint(col.r);
	curr_words[base + 13u] = floatBitsToUint(col.g);
	curr_words[base + 14u] = floatBitsToUint(col.b);
	curr_words[base + 15u] = floatBitsToUint(col.a);
	// endpoint_key_words (words 28..35) - identical between curr and prev!
	curr_words[base + 28u] = 7u;
	curr_words[base + 30u] = 3u;
	curr_words[base + 32u] = 101u;
	curr_words[base + 34u] = 100u;

	prev_words[base + 0u] = floatBitsToUint(prev_xy.x);
	prev_words[base + 1u] = floatBitsToUint(prev_xy.y);
	prev_words[base + 2u] = floatBitsToUint(0.0);
	prev_words[base + 6u] = floatBitsToUint(1.0);
	prev_words[base + 8u] = floatBitsToUint(1.0);
	prev_words[base + 11u] = floatBitsToUint(1.0);
	prev_words[base + 12u] = floatBitsToUint(col.r);
	prev_words[base + 13u] = floatBitsToUint(col.g);
	prev_words[base + 14u] = floatBitsToUint(col.b);
	prev_words[base + 15u] = floatBitsToUint(col.a);
	prev_words[base + 28u] = 7u;
	prev_words[base + 30u] = 3u;
	prev_words[base + 32u] = 101u;
	prev_words[base + 34u] = 100u;

	valid_flags[v] = 1u;
}

void main() {
	vec2 corners[4] = vec2[4](
		vec2(-1.0, -1.0),
		vec2( 1.0, -1.0),
		vec2(-1.0,  1.0),
		vec2( 1.0,  1.0)
	);
	vec4 col = vec4(params.red_chan, params.green_chan, 0.25, 1.0);
	for (uint v = 0u; v < 4u; ++v) {
		write_vertex(v, corners[v], corners[v] - vec2(params.motion_dx, params.motion_dy), col);
	}
	indices[0] = 0u;
	indices[1] = 1u;
	indices[2] = 2u;
	indices[3] = 2u;
	indices[4] = 1u;
	indices[5] = 3u;

	for (uint m = 0u; m < 8u; ++m) {
		meta_words[m] = 0u;
	}
	meta_words[0] = 1u;
	meta_words[4] = 1u;
	meta_words[5] = 2u; // Profile 2 = INDEXED_144
	meta_words[6] = params.seq_lo;
	meta_words[7] = params.seq_hi;
}
)GLSL";

	const String raster_vert_glsl = R"GLSL(
#version 450
layout(set = 0, binding = 0, std430) restrict readonly buffer CurrVerts { uint curr_words[]; };
layout(set = 0, binding = 1, std430) restrict readonly buffer PrevVerts { uint prev_words[]; };
layout(set = 0, binding = 2, std430) restrict readonly buffer HistValid { uint valid_flags[]; };

layout(location = 0) out vec4 v_color;
layout(location = 1) out vec4 v_velocity;

void main() {
	uint v = uint(gl_VertexIndex);
	uint base = v * 36u;
	vec2 curr_xy = vec2(uintBitsToFloat(curr_words[base + 0u]), uintBitsToFloat(curr_words[base + 1u]));
	vec2 prev_xy = vec2(uintBitsToFloat(prev_words[base + 0u]), uintBitsToFloat(prev_words[base + 1u]));
	vec2 vel = (valid_flags[v] != 0u) ? (curr_xy - prev_xy) : vec2(0.0);
	v_color = vec4(
		uintBitsToFloat(curr_words[base + 12u]),
		uintBitsToFloat(curr_words[base + 13u]),
		uintBitsToFloat(curr_words[base + 14u]),
		uintBitsToFloat(curr_words[base + 15u])
	);
	v_velocity = vec4(clamp(vel * 0.5 + vec2(0.5), vec2(0.0), vec2(1.0)), 1.0, 1.0);
	gl_Position = vec4(curr_xy, 0.5, 1.0);
}
)GLSL";

	const String raster_frag_glsl = R"GLSL(
#version 450
layout(location = 0) in vec4 v_color;
layout(location = 1) in vec4 v_velocity;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_velocity;

void main() {
	out_color = v_color;
	out_velocity = v_velocity;
}
)GLSL";

	const RID writer_shader = compile_compute_shader_glsl(rd, writer_glsl);
	const RID writer_pipeline = rd.compute_pipeline_create(writer_shader);
	REQUIRE(writer_pipeline.is_valid());

	const RID raster_shader = compile_raster_shader_glsl(rd, raster_vert_glsl, raster_frag_glsl);

	// Create 8x8 MRT offscreen attachments (Attachment 0: color, Attachment 1: motion velocity):
	RD::TextureFormat tf;
	tf.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
	tf.width = 8;
	tf.height = 8;
	tf.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
	const RID color_tex = rd.texture_create(tf, RD::TextureView());
	const RID velocity_tex = rd.texture_create(tf, RD::TextureView());
	REQUIRE(color_tex.is_valid());
	REQUIRE(velocity_tex.is_valid());

	Vector<RID> attachments;
	attachments.push_back(color_tex);
	attachments.push_back(velocity_tex);
	const RID framebuffer = rd.framebuffer_create(attachments);
	REQUIRE(framebuffer.is_valid());

	RD::PipelineRasterizationState raster_state;
	raster_state.cull_mode = RD::POLYGON_CULL_DISABLED;
	RD::PipelineColorBlendState blend_state;
	blend_state.attachments.push_back(RD::PipelineColorBlendState::Attachment());
	blend_state.attachments.push_back(RD::PipelineColorBlendState::Attachment());
	const RID raster_pipeline = rd.render_pipeline_create(
			raster_shader,
			rd.framebuffer_get_format(framebuffer),
			RD::INVALID_ID,
			RD::RENDER_PRIMITIVE_TRIANGLES,
			raster_state,
			RD::PipelineMultisampleState(),
			RD::PipelineDepthStencilState(),
			blend_state);
	REQUIRE(raster_pipeline.is_valid());

	// Dynamic empty-initialized index buffer (enables RDG resource_tracker barrier between buffer_copy & draw_list!):
	const RID index_buffer = rd.index_buffer_create(6, RD::INDEX_BUFFER_FORMAT_UINT32);
	REQUIRE(index_buffer.is_valid());
	const RID index_array = rd.index_array_create(index_buffer, 0, 6);
	REQUIRE(index_array.is_valid());

	Vector<RD::Uniform> writer_uniforms;
	for (uint32_t b = 0; b < 5u; ++b) {
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
		u.binding = b;
		u.append_id(bufs.rids[b]);
		writer_uniforms.push_back(u);
	}
	const RID writer_uset = rd.uniform_set_create(writer_uniforms, writer_shader, 0);
	REQUIRE(writer_uset.is_valid());

	Vector<RD::Uniform> raster_uniforms;
	for (uint32_t b = 0; b < 3u; ++b) {
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
		u.binding = b;
		u.append_id(bufs.rids[b]);
		raster_uniforms.push_back(u);
	}
	const RID raster_uset = rd.uniform_set_create(raster_uniforms, raster_shader, 0);
	REQUIRE(raster_uset.is_valid());

	Guard::Bindings guard_bindings;
	guard_bindings.buffers[Guard::CURRENT_VERTICES] = { bufs.rids[RibbonRD::CURRENT_VERTICES], bufs.bytes[RibbonRD::CURRENT_VERTICES] };
	guard_bindings.buffers[Guard::ALIGNED_PREVIOUS_VERTICES] = { bufs.rids[RibbonRD::ALIGNED_PREVIOUS_VERTICES], bufs.bytes[RibbonRD::ALIGNED_PREVIOUS_VERTICES] };
	guard_bindings.buffers[Guard::VERTEX_HISTORY_VALIDITY] = { bufs.rids[RibbonRD::VERTEX_HISTORY_VALIDITY], bufs.bytes[RibbonRD::VERTEX_HISTORY_VALIDITY] };
	guard_bindings.buffers[Guard::INDEX_STAGING] = { bufs.rids[RibbonRD::INDEX_STAGING], bufs.bytes[RibbonRD::INDEX_STAGING] };
	guard_bindings.buffers[Guard::OUTPUT_META] = { bufs.rids[RibbonRD::OUTPUT_META], bufs.bytes[RibbonRD::OUTPUT_META] };
	guard_bindings.buffers[Guard::SOURCE_COUNT] = { source_rid, 32 };
	guard_bindings.buffers[Guard::GUARD_SCRATCH] = { bufs.rids[RibbonRD::GUARD_SCRATCH], bufs.bytes[RibbonRD::GUARD_SCRATCH] };
	guard_bindings.buffers[Guard::GUARD_STATUS] = { bufs.rids[RibbonRD::GUARD_STATUS], bufs.bytes[RibbonRD::GUARD_STATUS] };
	guard_bindings.buffers[Guard::PROTECTED_COMMANDS] = { bufs.rids[RibbonRD::PROTECTED_COMMANDS], bufs.bytes[RibbonRD::PROTECTED_COMMANDS] };
	guard_bindings.buffers[Guard::GEOMETRY_PROOF] = { bufs.rids[RibbonRD::GEOMETRY_PROOF], bufs.bytes[RibbonRD::GEOMETRY_PROOF] };
	guard_bindings.buffers[Guard::SOURCE_COUNT_VALIDITY] = { source_rid, 32 };
	guard_bindings.buffers[Guard::SOURCE_ATTRIBUTES_VALIDITY] = { source_rid, 32 };
	guard_bindings.buffers[Guard::SOURCE_DENSE_IDS_VALIDITY] = { source_rid, 32 };
	guard_bindings.buffers[Guard::SOURCE_INSTANCE_GATE] = { source_rid, 32 };

	Guard::PushConstant dummy_req;
	dummy_req.capacity = 1;
	dummy_req.sequence_low = 1u;
	dummy_req.sequence_high = 1u;
	dummy_req.surface_count = 1;
	dummy_req.segment_base = 0;
	dummy_req.segment_count = 1;

	Guard::LayoutPlan guard_plan;
	REQUIRE(Guard::plan_layout(dummy_req, guard_bindings, proof, 4, guard_plan) == OK);
	const RID guard_uset = rd.uniform_set_create(guard_plan.uniforms, guard.get_shader(guard_plan.initialize_mode), 0);
	REQUIRE(guard_uset.is_valid());

	struct WriterPush {
		float red_chan;
		float green_chan;
		float motion_dx;
		float motion_dy;
		uint32_t seq_lo;
		uint32_t seq_hi;
		uint32_t pad0;
		uint32_t pad1;
	};
	static_assert(sizeof(WriterPush) == 32);

	Vector<Color> clear_colors;
	clear_colors.push_back(Color(0.0f, 0.0f, 0.0f, 0.0f));
	clear_colors.push_back(Color(0.0f, 0.0f, 0.0f, 0.0f));

	// Execute 6 back-to-back frames verifying zero stale-buffer barrier hazards:
	for (uint32_t frame = 0; frame < 6u; ++frame) {
		const float r_val = float(frame + 1u) / 8.0f;
		const float g_val = float(7u - frame) / 8.0f;
		const float dx = (float(frame) - 2.5f) * 0.2f; // in [-0.5, +0.5]
		const float dy = (2.5f - float(frame)) * 0.2f;
		const uint32_t seq_lo = 0x1000u + frame;
		const uint32_t seq_hi = 0x2000u + frame;

		WriterPush wp = { r_val, g_val, dx, dy, seq_lo, seq_hi, 0u, 0u };
		dummy_req.sequence_low = seq_lo;
		dummy_req.sequence_high = seq_hi;
		REQUIRE(Guard::plan_layout(dummy_req, guard_bindings, proof, 4, guard_plan) == OK);

		// Pass 1: External compute writes vertices, motion history, indices, and metadata:
		RD::ComputeListID cl_write = rd.compute_list_begin();
		rd.compute_list_bind_compute_pipeline(cl_write, writer_pipeline);
		rd.compute_list_bind_uniform_set(cl_write, writer_uset, 0);
		rd.compute_list_set_push_constant(cl_write, &wp, sizeof(wp));
		rd.compute_list_dispatch(cl_write, 1, 1, 1);
		rd.compute_list_end();

		// Pass 2: Guard compute validates vertices/indices/keys and emits PROTECTED_COMMANDS:
		RD::ComputeListID cl_guard = rd.compute_list_begin();
		REQUIRE(guard.record(cl_guard, guard_plan, guard_uset, guard_uset, guard_uset) == OK);
		rd.compute_list_end();

		// Pass 3: Same-frame GPU buffer_copy from INDEX_STAGING -> index_buffer:
		REQUIRE(rd.buffer_copy(bufs.rids[RibbonRD::INDEX_STAGING], index_buffer, 0, 0, 6u * sizeof(uint32_t)) == OK);

		// Pass 4: Same-frame indirect indexed MRT draw reading compute-written buffers:
		RD::DrawListID dl = rd.draw_list_begin(framebuffer, RD::DRAW_CLEAR_COLOR_ALL, clear_colors);
		rd.draw_list_bind_render_pipeline(dl, raster_pipeline);
		rd.draw_list_bind_uniform_set(dl, raster_uset, 0);
		rd.draw_list_bind_index_array(dl, index_array);
		rd.draw_list_draw_indirect(dl, true, bufs.rids[RibbonRD::PROTECTED_COMMANDS], 0, 1, 0);
		rd.draw_list_end();

		rd.submit();
		rd.sync();

		const Vector<uint8_t> status_rb = rd.buffer_get_data(bufs.rids[RibbonRD::GUARD_STATUS]);
		REQUIRE(status_rb.size() >= 4);
		CHECK(decode_uint32(status_rb.ptr()) == 0u);

		const Vector<uint8_t> color_rb = rd.texture_get_data(color_tex, 0);
		const Vector<uint8_t> vel_rb = rd.texture_get_data(velocity_tex, 0);
		REQUIRE(color_rb.size() == 8 * 8 * 4);
		REQUIRE(vel_rb.size() == 8 * 8 * 4);

		// Sample center texel (4, 4):
		const int px = (4 * 8 + 4) * 4;
		const int exp_r = int(std::round(r_val * 255.0f));
		const int exp_g = int(std::round(g_val * 255.0f));
		const int exp_vx = int(std::round((dx * 0.5f + 0.5f) * 255.0f));
		const int exp_vy = int(std::round((dy * 0.5f + 0.5f) * 255.0f));
		CHECK(std::abs(int(color_rb[px + 0]) - exp_r) <= 2);
		CHECK(std::abs(int(color_rb[px + 1]) - exp_g) <= 2);
		CHECK(color_rb[px + 3] == 255);
		CHECK(std::abs(int(vel_rb[px + 0]) - exp_vx) <= 2);
		CHECK(std::abs(int(vel_rb[px + 1]) - exp_vy) <= 2);
		CHECK(vel_rb[px + 2] == 255);
	}

	// Clean child-before-parent disposal:
	rd.free_rid(guard_uset);
	rd.free_rid(raster_uset);
	rd.free_rid(writer_uset);
	rd.free_rid(index_array);
	rd.free_rid(index_buffer);
	rd.free_rid(raster_pipeline);
	rd.free_rid(framebuffer);
	rd.free_rid(velocity_tex);
	rd.free_rid(color_tex);
	rd.free_rid(raster_shader);
	rd.free_rid(writer_pipeline);
	rd.free_rid(writer_shader);
	rd.free_rid(source_rid);
	RibbonRD::free_buffers_rd(rd, bufs);
	CHECK(bufs.live_count == 0);
	REQUIRE(guard.release_pipelines_standalone_rd() == OK);
#endif
}

// ============================================================================
// CATEGORY 2: Degenerate 0-Alive / 1-Particle Ribbon / Subnormal & NaN Guard /
//             0-Light & Live Metal 0-Instance Indirect Draw Safety
// ============================================================================
TEST_CASE("[particle_pre_benchmark_adversarial] Cat-2 Degenerate 0/1-particle ribbon, subnormal canonicalization, 0-light & 0-instance indirect draw") {
	using RibbonRD = RendererRD::ParticlesRibbonRD;
	using MeshLightRD = RendererRD::ParticlesMeshLightSortRD;

	// 2A. 0-particle and 1-particle chain safety (zero uint32 count - 1 underflow!):
	CHECK(RibbonRD::segment_count_for_chain_particles(0u) == 0u);
	CHECK(RibbonRD::segment_count_for_chain_particles(1u) == 0u);
	CHECK(RibbonRD::segment_count_for_chain_particles(2u) == 1u);
	CHECK(RibbonRD::segment_count_for_chain_particles(5u) == 4u);

	LocalVector<Vector3> chain_pos;
	LocalVector<uint64_t> chain_ids;
	LocalVector<RendererRD::Float144Vertex> chain_verts;
	LocalVector<uint32_t> chain_indices;

	// 0 particles:
	REQUIRE(RibbonRD::build_ribbon_chain_segments(1, 1, chain_pos, chain_ids, Vector3(0, 0, 5), 0.25f, Color(1, 1, 1, 1), Basis(), chain_verts, chain_indices) == OK);
	CHECK(chain_verts.is_empty());
	CHECK(chain_indices.is_empty());

	// 1 particle (single-point trail must emit 0 segments, NOT underflow to UINT32_MAX!):
	chain_pos.push_back(Vector3(1.0f, 2.0f, 3.0f));
	chain_ids.push_back(42ull);
	REQUIRE(RibbonRD::build_ribbon_chain_segments(1, 1, chain_pos, chain_ids, Vector3(0, 0, 5), 0.25f, Color(1, 1, 1, 1), Basis(), chain_verts, chain_indices) == OK);
	CHECK(chain_verts.is_empty());
	CHECK(chain_indices.is_empty());

	// Mismatched positions vs IDs fails closed:
	chain_ids.push_back(43ull);
	CHECK(RibbonRD::build_ribbon_chain_segments(1, 1, chain_pos, chain_ids, Vector3(0, 0, 5), 0.25f, Color(1, 1, 1, 1), Basis(), chain_verts, chain_indices) == ERR_INVALID_PARAMETER);

	// 4 particles -> 3 segments (12 vertices, 18 indices):
	chain_pos.push_back(Vector3(1.0f, 3.0f, 3.0f));
	chain_pos.push_back(Vector3(1.0f, 4.0f, 3.0f));
	chain_pos.push_back(Vector3(1.0f, 5.0f, 3.0f));
	chain_ids.push_back(44ull);
	chain_ids.push_back(45ull);
	REQUIRE(RibbonRD::build_ribbon_chain_segments(1, 1, chain_pos, chain_ids, Vector3(0, 0, 5), 0.25f, Color(1, 1, 1, 1), Basis(), chain_verts, chain_indices) == OK);
	CHECK(chain_verts.size() == 12u);
	CHECK(chain_indices.size() == 18u);

	// 2B. Subnormal / -0.0f / NaN / Inf canonicalization in build_ribbon_segment_quad:
	const float denorm = std::numeric_limits<float>::denorm_min();
	const float qnan = std::numeric_limits<float>::quiet_NaN();
	const float pinf = std::numeric_limits<float>::infinity();
	Basis poisoned_basis;
	poisoned_basis.rows[0][0] = denorm;
	poisoned_basis.rows[1][1] = qnan;
	poisoned_basis.rows[2][2] = -pinf;

	RendererRD::Float144Vertex adv_verts[4] = {};
	uint32_t adv_indices[6] = {};
	RibbonRD::build_ribbon_segment_quad(
			0u,
			Vector3(denorm, -denorm, -0.0f),
			Vector3(qnan, pinf, -pinf),
			Vector3(denorm, qnan, 5.0f),
			denorm,
			denorm,
			qnan,
			Color(denorm, -denorm, qnan, pinf),
			poisoned_basis,
			{ 9ull, 2ull, 55ull, 54ull },
			adv_verts,
			adv_indices);

	for (int v = 0; v < 4; ++v) {
		uint32_t raw_words[36] = {};
		std::memcpy(raw_words, &adv_verts[v], sizeof(raw_words));
		// Float payload words are 0..2, 4..6, 8..27:
		for (int w = 0; w < 28; ++w) {
			if (w == 3 || w == 7) {
				CHECK(raw_words[w] == 0u);
			} else {
				CHECK(float_word_is_guard_clean(raw_words[w]));
			}
		}
	}

	// 2C. 0-light & poisoned NaN/Inf/negative color_hdr & attenuation_exp rejection:
	LocalVector<RendererRD::ParticleLightRecord64> light_candidates;
	RendererRD::ParticleLightRecord64 bad_neg_hdr;
	bad_neg_hdr.radius = 4.0f;
	bad_neg_hdr.color_hdr[0] = -1.0f;
	bad_neg_hdr.color_hdr[1] = 2.0f;
	bad_neg_hdr.color_hdr[2] = 2.0f;
	bad_neg_hdr.attenuation_exp = 1.0f;
	bad_neg_hdr.screen_importance_q16 = 500u;
	light_candidates.push_back(bad_neg_hdr);

	RendererRD::ParticleLightRecord64 bad_nan_atten = bad_neg_hdr;
	bad_nan_atten.color_hdr[0] = 2.0f;
	bad_nan_atten.attenuation_exp = qnan;
	light_candidates.push_back(bad_nan_atten);

	RendererRD::ParticleLightRecord64 bad_neg_atten = bad_neg_hdr;
	bad_neg_atten.color_hdr[0] = 2.0f;
	bad_neg_atten.attenuation_exp = -0.5f;
	light_candidates.push_back(bad_neg_atten);

	RendererRD::ParticleLightRecord64 bad_inf_pos = bad_neg_hdr;
	bad_inf_pos.color_hdr[0] = 2.0f;
	bad_inf_pos.position_world[1] = pinf;
	light_candidates.push_back(bad_inf_pos);

	LocalVector<RendererRD::ParticleLightRecord64> compacted_lights;
	REQUIRE(MeshLightRD::compact_particle_lights_deterministic(light_candidates, 16u, compacted_lights) == OK);
	CHECK(compacted_lights.is_empty());

	MeshLightRD light_lane;
	REQUIRE(light_lane.publish_active_particle_lights(light_candidates, 16u) == OK);
	CHECK(light_lane.get_active_particle_light_count() == 0u);

	// Constant attenuation (attenuation_exp == 0.0f) with valid positive HDR is accepted:
	{
		LocalVector<RendererRD::ParticleLightRecord64> const_atten_in;
		RendererRD::ParticleLightRecord64 const_atten = bad_neg_hdr;
		const_atten.color_hdr[0] = 2.0f;
		const_atten.attenuation_exp = 0.0f;
		const_atten_in.push_back(const_atten);
		LocalVector<RendererRD::ParticleLightRecord64> const_atten_out;
		REQUIRE(MeshLightRD::compact_particle_lights_deterministic(const_atten_in, 16u, const_atten_out) == OK);
		CHECK(const_atten_out.size() == 1u);
	}

	// 2D. 0-active-instance and >0-active-instance multi-surface indirect command planning:
	LocalVector<uint32_t> surf_idx_counts;
	surf_idx_counts.push_back(36u);
	surf_idx_counts.push_back(72u);
	LocalVector<RendererRD::MeshSurfaceIndirectCommand20> surf_cmds;
	REQUIRE(MeshLightRD::plan_multisurface_mesh_commands(surf_idx_counts, 0u, false, surf_cmds) == OK);
	REQUIRE(surf_cmds.size() == 2u);
	CHECK(surf_cmds[0].index_count == 0u);
	CHECK(surf_cmds[0].instance_count == 0u);
	CHECK(surf_cmds[1].index_count == 0u);
	CHECK(surf_cmds[1].instance_count == 0u);

	REQUIRE(MeshLightRD::plan_multisurface_mesh_commands(surf_idx_counts, 3u, false, surf_cmds) == OK);
	REQUIRE(surf_cmds.size() == 2u);
	CHECK(surf_cmds[0].index_count == 36u);
	CHECK(surf_cmds[0].instance_count == 3u);
	CHECK(surf_cmds[1].index_count == 72u);
	CHECK(surf_cmds[1].instance_count == 3u);

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
	// 2E. Live Metal GPU:
	//     (i) Subnormal/NaN-canonicalized vertices pass ParticlesExternalGuardIndexed144 with 0 NUMERIC_FAULT;
	//     (ii) 0-active-segment guard emits (index_count=0, instance_count=0) AND live Metal draw_list_draw_indirect
	//          executes cleanly with 0 touched pixels;
	//     (iii) upload_particle_lights_rd with 0 selected lights writes active_count == 0 cleanly.
	REQUIRE(RenderingDevice::get_singleton() == nullptr);
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	using Guard = RendererRD::ParticlesExternalGuardIndexed144;
	Guard guard;
	REQUIRE(guard.prepare_pipelines_standalone_rd() == OK);

	RibbonRD::Indexed144Layout layout;
	REQUIRE(RibbonRD::plan_indexed144(1, false, 1, layout) == OK);
	RibbonRD::AllocatedBuffers bufs;
	REQUIRE(RibbonRD::allocate_buffers_rd(rd, layout, bufs) == OK);

	Vector<uint8_t> source_bytes;
	source_bytes.resize(32);
	std::memset(source_bytes.ptrw(), 0, 32);
	encode_uint32(1u, source_bytes.ptrw() + 0);
	const RID source_rid = rd.storage_buffer_create(32, source_bytes);
	REQUIRE(source_rid.is_valid());

	Guard::GeometryProofHeader proof = {};
	for (int k = 0; k < 3; ++k) {
		proof.current_decl_min[k] = -10.0f;
		proof.current_decl_max[k] = 10.0f;
		proof.cull_min[k] = -20.0f;
		proof.cull_max[k] = 20.0f;
	}
	proof.source_accessor[0] = 0;
	proof.source_accessor[3] = 1;
	proof.source_gate_words[0] = 1;
	proof.source_gate_words[1] = 2;
	proof.source_gate_words[2] = 3;
	proof.source_gate_words[3] = 4;
	proof.options[0] = 0;

	Guard::RibbonSurfaceProof surf = {};
	surf.command[0] = 1;
	surf.command[1] = 6;
	Vector<uint8_t> proof_bytes;
	proof_bytes.resize(sizeof(proof) + sizeof(surf));
	std::memcpy(proof_bytes.ptrw(), &proof, sizeof(proof));
	std::memcpy(proof_bytes.ptrw() + sizeof(proof), &surf, sizeof(surf));
	REQUIRE(rd.buffer_update(bufs.rids[RibbonRD::GEOMETRY_PROOF], 0, proof_bytes.size(), proof_bytes.ptr()) == OK);

	Vector<uint8_t> idx_bytes;
	idx_bytes.resize(24);
	for (int j = 0; j < 6; ++j) {
		encode_uint32(adv_indices[j], idx_bytes.ptrw() + j * 4);
	}
	Vector<uint8_t> meta_bytes;
	meta_bytes.resize(32);
	std::memset(meta_bytes.ptrw(), 0, 32);
	encode_uint32(1u, meta_bytes.ptrw() + 0);
	encode_uint32(1u, meta_bytes.ptrw() + 16);
	encode_uint32(2u, meta_bytes.ptrw() + 20); // Profile 2 = INDEXED_144
	encode_uint32(0x1111u, meta_bytes.ptrw() + 24);
	encode_uint32(0x2222u, meta_bytes.ptrw() + 28);

	REQUIRE(rd.buffer_update(bufs.rids[RibbonRD::CURRENT_VERTICES], 0, sizeof(adv_verts), adv_verts) == OK);
	REQUIRE(rd.buffer_update(bufs.rids[RibbonRD::INDEX_STAGING], 0, idx_bytes.size(), idx_bytes.ptr()) == OK);
	REQUIRE(rd.buffer_update(bufs.rids[RibbonRD::OUTPUT_META], 0, meta_bytes.size(), meta_bytes.ptr()) == OK);

	Guard::Bindings bindings;
	bindings.buffers[Guard::CURRENT_VERTICES] = { bufs.rids[RibbonRD::CURRENT_VERTICES], bufs.bytes[RibbonRD::CURRENT_VERTICES] };
	bindings.buffers[Guard::ALIGNED_PREVIOUS_VERTICES] = { RID(), 0 };
	bindings.buffers[Guard::VERTEX_HISTORY_VALIDITY] = { RID(), 0 };
	bindings.buffers[Guard::INDEX_STAGING] = { bufs.rids[RibbonRD::INDEX_STAGING], bufs.bytes[RibbonRD::INDEX_STAGING] };
	bindings.buffers[Guard::OUTPUT_META] = { bufs.rids[RibbonRD::OUTPUT_META], bufs.bytes[RibbonRD::OUTPUT_META] };
	bindings.buffers[Guard::SOURCE_COUNT] = { source_rid, 32 };
	bindings.buffers[Guard::GUARD_SCRATCH] = { bufs.rids[RibbonRD::GUARD_SCRATCH], bufs.bytes[RibbonRD::GUARD_SCRATCH] };
	bindings.buffers[Guard::GUARD_STATUS] = { bufs.rids[RibbonRD::GUARD_STATUS], bufs.bytes[RibbonRD::GUARD_STATUS] };
	bindings.buffers[Guard::PROTECTED_COMMANDS] = { bufs.rids[RibbonRD::PROTECTED_COMMANDS], bufs.bytes[RibbonRD::PROTECTED_COMMANDS] };
	bindings.buffers[Guard::GEOMETRY_PROOF] = { bufs.rids[RibbonRD::GEOMETRY_PROOF], bufs.bytes[RibbonRD::GEOMETRY_PROOF] };
	bindings.buffers[Guard::SOURCE_COUNT_VALIDITY] = { source_rid, 32 };
	bindings.buffers[Guard::SOURCE_ATTRIBUTES_VALIDITY] = { source_rid, 32 };
	bindings.buffers[Guard::SOURCE_DENSE_IDS_VALIDITY] = { source_rid, 32 };
	bindings.buffers[Guard::SOURCE_INSTANCE_GATE] = { source_rid, 32 };

	Guard::PushConstant req;
	req.capacity = 1;
	req.sequence_low = 0x1111u;
	req.sequence_high = 0x2222u;
	req.surface_count = 1;
	req.segment_base = 0;
	req.segment_count = 1;

	Guard::LayoutPlan plan;
	REQUIRE(Guard::plan_layout(req, bindings, proof, 4, plan) == OK);
	const RID guard_uset = rd.uniform_set_create(plan.uniforms, guard.get_shader(plan.initialize_mode), 0);
	REQUIRE(guard_uset.is_valid());

	// (i) Canonicalized subnormal/NaN ribbon quad passes GPU guard with 0 fault bits:
	{
		RD::ComputeListID cl = rd.compute_list_begin();
		REQUIRE(guard.record(cl, plan, guard_uset, guard_uset, guard_uset) == OK);
		rd.compute_list_end();
		rd.submit();
		rd.sync();

		const Vector<uint8_t> status_rb = rd.buffer_get_data(bufs.rids[RibbonRD::GUARD_STATUS]);
		const Vector<uint8_t> cmd_rb = rd.buffer_get_data(bufs.rids[RibbonRD::PROTECTED_COMMANDS]);
		CHECK(decode_uint32(status_rb.ptr() + 0) == 0u);
		CHECK(decode_uint32(cmd_rb.ptr() + 0) == 6u);
		CHECK(decode_uint32(cmd_rb.ptr() + 4) == 1u);
	}

	// (ii) Now set 0 alive segments (source_count = 0, meta active = 0 while capacity=1 & profile=2) and execute live Metal indirect draw!
	encode_uint32(0u, source_bytes.ptrw() + 0);
	encode_uint32(0u, meta_bytes.ptrw() + 0);
	encode_uint32(1u, meta_bytes.ptrw() + 16);
	encode_uint32(2u, meta_bytes.ptrw() + 20);
	REQUIRE(rd.buffer_update(source_rid, 0, 32, source_bytes.ptr()) == OK);
	REQUIRE(rd.buffer_update(bufs.rids[RibbonRD::OUTPUT_META], 0, 32, meta_bytes.ptr()) == OK);

	const RID empty_draw_shader = compile_raster_shader_glsl(
			rd,
			"#version 450\nvoid main() { gl_Position = vec4(0.0, 0.0, 0.5, 1.0); }\n",
			"#version 450\nlayout(location=0) out vec4 out_col; void main() { out_col = vec4(1.0, 0.0, 0.0, 1.0); }\n");

	RD::TextureFormat tf;
	tf.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
	tf.width = 4;
	tf.height = 4;
	tf.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
	const RID target_tex = rd.texture_create(tf, RD::TextureView());
	Vector<RID> fb_tex;
	fb_tex.push_back(target_tex);
	const RID fb = rd.framebuffer_create(fb_tex);
	RD::PipelineColorBlendState bs;
	bs.attachments.push_back(RD::PipelineColorBlendState::Attachment());
	const RID pipe = rd.render_pipeline_create(
			empty_draw_shader,
			rd.framebuffer_get_format(fb),
			RD::INVALID_ID,
			RD::RENDER_PRIMITIVE_TRIANGLES,
			RD::PipelineRasterizationState(),
			RD::PipelineMultisampleState(),
			RD::PipelineDepthStencilState(),
			bs);
	const RID idx_buf = rd.index_buffer_create(6, RD::INDEX_BUFFER_FORMAT_UINT32, idx_bytes);
	const RID idx_arr = rd.index_array_create(idx_buf, 0, 6);

	REQUIRE(Guard::plan_layout(req, bindings, proof, 4, plan) == OK);
	RD::ComputeListID cl_zero = rd.compute_list_begin();
	REQUIRE(guard.record(cl_zero, plan, guard_uset, guard_uset, guard_uset) == OK);
	rd.compute_list_end();

	Vector<Color> sentinel_clear;
	sentinel_clear.push_back(Color(17.0f / 255.0f, 89.0f / 255.0f, 203.0f / 255.0f, 1.0f));
	RD::DrawListID dl_zero = rd.draw_list_begin(fb, RD::DRAW_CLEAR_COLOR_ALL, sentinel_clear);
	rd.draw_list_bind_render_pipeline(dl_zero, pipe);
	rd.draw_list_bind_index_array(dl_zero, idx_arr);
	rd.draw_list_draw_indirect(dl_zero, true, bufs.rids[RibbonRD::PROTECTED_COMMANDS], 0, 1, 0);
	rd.draw_list_end();

	rd.submit();
	rd.sync();

	const Vector<uint8_t> zero_status_rb = rd.buffer_get_data(bufs.rids[RibbonRD::GUARD_STATUS]);
	const Vector<uint8_t> zero_cmd_rb = rd.buffer_get_data(bufs.rids[RibbonRD::PROTECTED_COMMANDS]);
	CHECK(decode_uint32(zero_status_rb.ptr() + 0) == 0u);
	CHECK(decode_uint32(zero_cmd_rb.ptr() + 0) == 0u);
	CHECK(decode_uint32(zero_cmd_rb.ptr() + 4) == 0u);

	const Vector<uint8_t> target_rb = rd.texture_get_data(target_tex, 0);
	REQUIRE(target_rb.size() == 4 * 4 * 4);
	for (int p = 0; p < 16; ++p) {
		CHECK(target_rb[p * 4 + 0] == 17);
		CHECK(target_rb[p * 4 + 1] == 89);
		CHECK(target_rb[p * 4 + 2] == 203);
		CHECK(target_rb[p * 4 + 3] == 255);
	}

	// (iii) 0-light GPU upload:
	MeshLightRD::LightClusterBufferRD gpu_lights;
	REQUIRE(MeshLightRD::upload_particle_lights_rd(rd, 16u, compacted_lights, gpu_lights) == OK);
	CHECK(gpu_lights.active_count == 0u);
	const Vector<uint8_t> light_hdr = rd.buffer_get_data(gpu_lights.header_buffer);
	REQUIRE(light_hdr.size() >= 16);
	CHECK(decode_uint32(light_hdr.ptr() + 0) == 0u);
	MeshLightRD::free_particle_lights_rd(rd, gpu_lights);

	rd.free_rid(idx_arr);
	rd.free_rid(idx_buf);
	rd.free_rid(pipe);
	rd.free_rid(fb);
	rd.free_rid(target_tex);
	rd.free_rid(empty_draw_shader);
	rd.free_rid(guard_uset);
	rd.free_rid(source_rid);
	RibbonRD::free_buffers_rd(rd, bufs);
	REQUIRE(guard.release_pipelines_standalone_rd() == OK);
#endif
}

// ============================================================================
// CATEGORY 3: Multi-Viewport & Concurrent Split-Screen Isolation, SubUV Extreme
//             Float Safety, LateDepth Bounded Cache & NaN Collision Fuzzing
// ============================================================================
namespace {
struct MultiViewportWorkerCtx {
	RendererRD::ParticlesMeshLightSortRD *lane = nullptr;
	uint32_t worker_id = 0;
	std::atomic<uint32_t> *failures = nullptr;
};

static void _multi_viewport_worker_fn(void *p_userdata) {
	MultiViewportWorkerCtx *ctx = static_cast<MultiViewportWorkerCtx *>(p_userdata);
	using MeshLightRD = RendererRD::ParticlesMeshLightSortRD;
	// 4 workers * 2 views each = all 8 MAX_VIEWS slots hammered concurrently!
	const uint64_t own_view_a = 1000ull + uint64_t(ctx->worker_id) * 2ull;
	const uint64_t own_view_b = own_view_a + 1ull;

	LocalVector<RendererRD::Instance192Record> recs;
	for (uint32_t k = 0; k < 4u; ++k) {
		Transform3D xf(Basis(), Vector3(0.0f, 0.0f, float(k) * 2.0f));
		recs.push_back(MeshLightRD::make_instance192_record(xf, Vector3(0, 1, 0), Color(1, 1, 1, 1), float(k), 500ull + k, true));
	}

	uint64_t epoch_a = 1ull;
	for (uint32_t iter = 0; iter < 128u; ++iter) {
		LocalVector<RendererRD::Instance192Record> prev_out;
		LocalVector<uint32_t> sorted_out;
		const Vector3 cam_pos_a = (iter % 7u == 0u)
				? Vector3(std::numeric_limits<float>::quiet_NaN(), 0.0f, 10.0f)
				: Vector3(0.0f, 0.0f, 10.0f);
		const Vector3 cam_pos_b = Vector3(0.0f, 0.0f, -10.0f);

		if (ctx->lane->submit_view_frame(own_view_a, epoch_a, iter + 1u, iter + 1u, cam_pos_a, Vector3(0.0f, 0.0f, -1.0f), recs, prev_out, sorted_out) != OK || sorted_out.size() != 4u) {
			ctx->failures->fetch_add(1u, std::memory_order_relaxed);
		}
		if (ctx->lane->submit_view_frame(own_view_b, 1ull, iter + 1u, iter + 1u, cam_pos_b, Vector3(0.0f, 0.0f, 1.0f), recs, prev_out, sorted_out) != OK || sorted_out.size() != 4u) {
			ctx->failures->fetch_add(1u, std::memory_order_relaxed);
		}
		if (iter % 19u == 0u) {
			if (ctx->lane->set_view_paused(own_view_a, true) != OK || ctx->lane->set_view_paused(own_view_a, false) != OK) {
				ctx->failures->fetch_add(1u, std::memory_order_relaxed);
			}
		}
		if (iter % 29u == 0u) {
			++epoch_a;
			if (ctx->lane->resize_or_epoch_bump_view(own_view_a, epoch_a) != OK) {
				ctx->failures->fetch_add(1u, std::memory_order_relaxed);
			}
		}
	}
}
} // namespace

TEST_CASE("[particle_pre_benchmark_adversarial] Cat-3 Multi-viewport split-screen isolation, SubUV FLT_MAX safety, LateDepth cap & NaN collision fuzzing") {
	using MeshLightRD = RendererRD::ParticlesMeshLightSortRD;
	using CollisionRD = RendererRD::ParticlesCollisionSceneRD;
	const float qnan = std::numeric_limits<float>::quiet_NaN();
	const float pinf = std::numeric_limits<float>::infinity();

	// 3A. Editor Main Viewport (looking -Z from +10) vs Preview SubViewport (looking +Z from -10) in SAME frame:
	{
		MeshLightRD lane;
		LocalVector<RendererRD::Instance192Record> frame1_recs;
		// z = -3 (idx 0), z = 0 (idx 1), z = +3 (idx 2):
		frame1_recs.push_back(MeshLightRD::make_instance192_record(Transform3D(Basis(), Vector3(0, 0, -3.0f)), Vector3(), Color(1, 1, 1, 1), 0.0f, 10ull, false));
		frame1_recs.push_back(MeshLightRD::make_instance192_record(Transform3D(Basis(), Vector3(0, 0, 0.0f)), Vector3(), Color(1, 1, 1, 1), 0.0f, 20ull, false));
		frame1_recs.push_back(MeshLightRD::make_instance192_record(Transform3D(Basis(), Vector3(0, 0, 3.0f)), Vector3(), Color(1, 1, 1, 1), 0.0f, 30ull, false));

		LocalVector<RendererRD::Instance192Record> prev_main, prev_sub;
		LocalVector<uint32_t> sorted_main, sorted_sub;

		// Frame 1: both viewports render the same particle set from opposite cameras:
		REQUIRE(lane.submit_view_frame(101ull, 1ull, 1ull, 1ull, Vector3(0, 0, 10.0f), Vector3(0, 0, -1.0f), frame1_recs, prev_main, sorted_main) == OK);
		REQUIRE(lane.submit_view_frame(202ull, 1ull, 1ull, 1ull, Vector3(0, 0, -10.0f), Vector3(0, 0, 1.0f), frame1_recs, prev_sub, sorted_sub) == OK);

		// Main camera at +10 looking -Z: farthest is z=-3 (idx 0, depth 13), then z=0 (idx 1, depth 10), nearest z=+3 (idx 2, depth 7):
		REQUIRE(sorted_main.size() == 3u);
		CHECK(sorted_main[0] == 0u);
		CHECK(sorted_main[1] == 1u);
		CHECK(sorted_main[2] == 2u);

		// Preview subviewport at -10 looking +Z: farthest is z=+3 (idx 2, depth 13), then z=0 (idx 1, depth 10), nearest z=-3 (idx 0, depth 7):
		REQUIRE(sorted_sub.size() == 3u);
		CHECK(sorted_sub[0] == 2u);
		CHECK(sorted_sub[1] == 1u);
		CHECK(sorted_sub[2] == 0u);

		// Pause Preview SubViewport while Main Viewport advances to Frame 2; then epoch-bump Preview SubViewport:
		REQUIRE(lane.set_view_paused(202ull, true) == OK);
		LocalVector<RendererRD::Instance192Record> frame2_recs(frame1_recs);
		frame2_recs[0].transform_rows[2][3] = -4.0f;
		REQUIRE(lane.submit_view_frame(101ull, 1ull, 2ull, 2ull, Vector3(0, 0, 10.0f), Vector3(0, 0, -1.0f), frame2_recs, prev_main, sorted_main) == OK);
		// Main viewport retained previous z=-3.0f from Frame 1!
		CHECK((prev_main[0].flags & RendererRD::Instance192Record::FLAG_HAS_PREVIOUS_RECORD) != 0u);
		CHECK(Math::is_equal_approx(prev_main[0].transform_rows[2][3], -3.0f));

		// Preview subviewport epoch bump clears ONLY view 202's history without disturbing view 101:
		REQUIRE(lane.set_view_paused(202ull, false) == OK);
		REQUIRE(lane.resize_or_epoch_bump_view(202ull, 2ull) == OK);
		REQUIRE(lane.submit_view_frame(202ull, 2ull, 2ull, 2ull, Vector3(0, 0, -10.0f), Vector3(0, 0, 1.0f), frame2_recs, prev_sub, sorted_sub) == OK);
		CHECK((prev_sub[0].flags & RendererRD::Instance192Record::FLAG_HAS_PREVIOUS_RECORD) == 0u);

		// Fill remaining 6 view slots up to MAX_VIEWS = 8, verify 9th fails with ERR_OUT_OF_MEMORY, then destroy & reuse:
		for (uint64_t extra = 301ull; extra <= 306ull; ++extra) {
			REQUIRE(lane.submit_view_frame(extra, 1ull, 1ull, 1ull, Vector3(), Vector3(0, 0, -1), frame2_recs, prev_sub, sorted_sub) == OK);
		}
		CHECK(lane.submit_view_frame(999ull, 1ull, 1ull, 1ull, Vector3(), Vector3(0, 0, -1), frame2_recs, prev_sub, sorted_sub) == ERR_OUT_OF_MEMORY);
		REQUIRE(lane.destroy_view(303ull) == OK);
		REQUIRE(lane.submit_view_frame(999ull, 1ull, 1ull, 1ull, Vector3(), Vector3(0, 0, -1), frame2_recs, prev_sub, sorted_sub) == OK);
	}

	// 3B. Multi-threaded 4-worker split-screen contention test on ParticlesMeshLightSortRD:
	{
		MeshLightRD shared_lane;
		std::atomic<uint32_t> failures{ 0u };
		Thread workers[4];
		MultiViewportWorkerCtx ctxs[4];
		for (uint32_t w = 0; w < 4u; ++w) {
			ctxs[w] = { &shared_lane, w, &failures };
			workers[w].start(_multi_viewport_worker_fn, &ctxs[w]);
		}
		for (uint32_t w = 0; w < 4u; ++w) {
			workers[w].wait_to_finish();
		}
		CHECK(failures.load() == 0u);
	}

	// 3C. Extreme float Sub-UV evaluation (FLT_MAX, -FLT_MAX, 1e20f, -1e20f, subnormals, NaN, Inf):
	{
		RendererRD::SubUVFlipbookSpec clamp_spec = { 8u, 8u, RendererRD::SubUVBlendMode::LINEAR_BLEND, RendererRD::SubUVWrapMode::CLAMP };
		RendererRD::SubUVFlipbookSpec repeat_spec = { 8u, 8u, RendererRD::SubUVBlendMode::LINEAR_BLEND, RendererRD::SubUVWrapMode::REPEAT };
		RendererRD::SubUVSamplePair sample;

		const float extreme_finites[] = {
			std::numeric_limits<float>::max(),
			-std::numeric_limits<float>::max(),
			1e20f,
			-1e20f,
			std::numeric_limits<float>::denorm_min(),
			-0.0f,
			63.75f,
		};
		for (float fc : extreme_finites) {
			REQUIRE(MeshLightRD::evaluate_subuv_sample(clamp_spec, Vector2(0.5f, 0.5f), fc, sample) == OK);
			CHECK(sample.frame0 < 64u);
			CHECK(sample.frame1 < 64u);
			CHECK(sample.blend_alpha >= 0.0f);
			CHECK(sample.blend_alpha <= 1.0f);

			REQUIRE(MeshLightRD::evaluate_subuv_sample(repeat_spec, Vector2(0.5f, 0.5f), fc, sample) == OK);
			CHECK(sample.frame0 < 64u);
			CHECK(sample.frame1 < 64u);
			CHECK(sample.blend_alpha >= 0.0f);
			CHECK(sample.blend_alpha <= 1.0f);
		}
		CHECK(MeshLightRD::evaluate_subuv_sample(repeat_spec, Vector2(0.5f, 0.5f), qnan, sample) == ERR_INVALID_PARAMETER);
		CHECK(MeshLightRD::evaluate_subuv_sample(repeat_spec, Vector2(0.5f, 0.5f), pinf, sample) == ERR_INVALID_PARAMETER);
	}

	// 3D. LateTransparentDepthContent NaN z_near/z_far rejection & 64-viewport bounded cache eviction:
	{
		using namespace GDExtensionDeviceExit;
		GDExtensionSceneOpaque32V1 dev_tok = {};
		dev_tok.bytes[0] = 0x34u;
		GDExtensionSceneOpaque32V1 samp_tok = make_late_depth_sample_token(77ull, 1ull, 64u, 64u);

		LateTransparentDepthDescriptor desc;
		desc.viewport = RID::from_uint64(77ull);
		desc.depth_texture = RID::from_uint64(88ull);
		desc.width = 64u;
		desc.height = 64u;
		desc.z_near = qnan;
		desc.z_far = 100.0f;
		{
			LateTransparentDepthContent bad_near(dev_tok, samp_tok, desc);
			CHECK(bad_near.validate(dev_tok, samp_tok) == ERR_INVALID_DATA);
		}
		desc.z_near = 0.05f;
		desc.z_far = pinf;
		{
			LateTransparentDepthContent bad_far(dev_tok, samp_tok, desc);
			CHECK(bad_far.validate(dev_tok, samp_tok) == ERR_INVALID_DATA);
		}

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
		REQUIRE(RenderingDevice::get_singleton() == nullptr);
		RenderingContextDriverMetal context;
		REQUIRE(context.initialize() == OK);
		RenderingDevice rd;
		REQUIRE(rd.initialize(&context) == OK);

		RD::TextureFormat depth_tf;
		depth_tf.format = RD::DATA_FORMAT_D32_SFLOAT;
		depth_tf.width = 4;
		depth_tf.height = 4;
		depth_tf.usage_bits = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
		const RID live_depth_tex = rd.texture_create(depth_tf, RD::TextureView());
		REQUIRE(live_depth_tex.is_valid());

		for (uint64_t vp_id = 5000ull; vp_id < 5070ull; ++vp_id) {
			LateTransparentDepthDescriptor vp_desc;
			vp_desc.viewport = RID::from_uint64(vp_id);
			vp_desc.depth_texture = live_depth_tex;
			vp_desc.width = 4u;
			vp_desc.height = 4u;
			vp_desc.frame_number = vp_id;
			dispatch_late_transparent_depth_phase(rd, vp_desc);
		}
		LateTransparentDepthDescriptor fetched;
		// Oldest 6 viewports (5000..5005) were evicted by the 64-entry LRU cap; newest 64 (5006..5069) remain live:
		CHECK_FALSE(get_last_published_late_transparent_depth_descriptor(RID::from_uint64(5000ull), fetched));
		CHECK_FALSE(get_last_published_late_transparent_depth_descriptor(RID::from_uint64(5005ull), fetched));
		CHECK(get_last_published_late_transparent_depth_descriptor(RID::from_uint64(5006ull), fetched));
		CHECK(get_last_published_late_transparent_depth_descriptor(RID::from_uint64(5069ull), fetched));
		CHECK(fetched.frame_number == 5069ull);

		// Tearing down the backing depth texture immediately invalidates cached late-depth lookup:
		rd.free_rid(live_depth_tex);
		CHECK_FALSE(get_last_published_late_transparent_depth_descriptor(RID::from_uint64(5069ull), fetched));
#endif
	}

	// 3E. NaN / Inf / inverted-bounds fuzzing across all 4 K06 collision evaluators:
	{
		CollisionRD col_mgr;
		GDExtensionSceneOpaque32V1 cap = {};
		cap.bytes[0] = 0x99u;
		col_mgr.initialize(nullptr, cap);
		LocalVector<RendererRD::SceneSphereRecord32> spheres;
		RendererRD::SceneSphereRecord32 s;
		s.radius = 1.0f;
		s.restitution = 0.5f;
		s.friction = 0.2f;
		spheres.push_back(s);

		// Inverted bounds & NaN bounds rejected:
		CHECK(col_mgr.update_sphere_colliders(spheres, Vector3(5, 0, 0), Vector3(-5, 1, 1)) == ERR_INVALID_PARAMETER);
		CHECK(col_mgr.update_sphere_colliders(spheres, Vector3(qnan, 0, 0), Vector3(5, 1, 1)) == ERR_INVALID_PARAMETER);

		// Sphere evaluator with NaN particle position/velocity/dt/sphere fields:
		CHECK_FALSE(CollisionRD::evaluate_sphere_collision(spheres, Vector3(qnan, 0, 0), Vector3(0, -1, 0), 0.1f, 0.016f).collided);
		CHECK_FALSE(CollisionRD::evaluate_sphere_collision(spheres, Vector3(0, 0.5f, 0), Vector3(0, qnan, 0), 0.1f, 0.016f).collided);
		CHECK_FALSE(CollisionRD::evaluate_sphere_collision(spheres, Vector3(0, 0.5f, 0), Vector3(0, -1, 0), 0.1f, qnan).collided);

		// Heightfield D32 evaluator with NaN position / NaN texel / NaN z_near:
		RendererRD::SceneHeightMeta352 hmeta;
		hmeta.width = 4u;
		hmeta.height = 4u;
		hmeta.bounds_min[0] = -4.0f;
		hmeta.bounds_min[1] = -4.0f;
		hmeta.bounds_min[2] = -4.0f;
		hmeta.bounds_max[0] = 4.0f;
		hmeta.bounds_max[1] = 4.0f;
		hmeta.bounds_max[2] = 4.0f;
		LocalVector<float> htex;
		htex.resize(16u);
		for (uint32_t i = 0; i < 16u; ++i) {
			htex[i] = qnan;
		}
		CHECK_FALSE(CollisionRD::evaluate_heightfield_d32_collision(hmeta, htex, Transform3D(), Vector3(0, 0, 0), Vector3(0, -1, 0), 0.2f, 0.016f).collided);
		CHECK_FALSE(CollisionRD::evaluate_heightfield_d32_collision(hmeta, htex, Transform3D(), Vector3(qnan, 0, 0), Vector3(0, -1, 0), 0.2f, 0.016f).collided);

		// Current-depth evaluator with NaN position / NaN texel:
		RendererRD::SceneCurrentDepthMeta256 dmeta;
		dmeta.width = 4u;
		dmeta.height = 4u;
		CHECK_FALSE(CollisionRD::evaluate_current_depth_collision(dmeta, htex, Transform3D(), Vector3(0, 0, -2.0f), Vector3(0, 0, -1.0f), 0.2f, 0.016f).collided);
		CHECK_FALSE(CollisionRD::evaluate_current_depth_collision(dmeta, htex, Transform3D(), Vector3(qnan, 0, -2.0f), Vector3(0, 0, -1.0f), 0.2f, 0.016f).collided);

		// 3D SDF evaluator with NaN position / NaN voxels:
		RendererRD::SceneSdfMeta256 smeta;
		smeta.dim_x = 2u;
		smeta.dim_y = 2u;
		smeta.dim_z = 2u;
		smeta.bounds_min[0] = -2.0f;
		smeta.bounds_min[1] = -2.0f;
		smeta.bounds_min[2] = -2.0f;
		smeta.bounds_max[0] = 2.0f;
		smeta.bounds_max[1] = 2.0f;
		smeta.bounds_max[2] = 2.0f;
		LocalVector<float> svox;
		svox.resize(8u);
		for (uint32_t i = 0; i < 8u; ++i) {
			svox[i] = qnan;
		}
		CHECK_FALSE(CollisionRD::evaluate_sdf_3d_collision(smeta, svox, Transform3D(), Vector3(0, 0, 0), Vector3(0, -1, 0), 0.2f, 0.016f).collided);
		CHECK_FALSE(CollisionRD::evaluate_sdf_3d_collision(smeta, svox, Transform3D(), Vector3(qnan, 0, 0), Vector3(0, -1, 0), 0.2f, 0.016f).collided);
		col_mgr.shutdown();
	}
}

// ============================================================================
// CATEGORY 4: 128 Rapid Mid-Flight Teardown & Aborted/Empty Draw List Cycles
// ============================================================================
#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
TEST_CASE("[particle_pre_benchmark_adversarial] Cat-4 128 rapid mid-flight teardown & aborted draw_list cycles with 0 leaked RIDs") {
	REQUIRE(RenderingDevice::get_singleton() == nullptr);
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	using RibbonRD = RendererRD::ParticlesRibbonRD;
	using MeshLightRD = RendererRD::ParticlesMeshLightSortRD;
	using CollisionRD = RendererRD::ParticlesCollisionSceneRD;

	const RID comp_shader = compile_compute_shader_glsl(
			rd,
			"#version 450\nlayout(local_size_x=1) in;\n"
			"layout(set=0,binding=0,std430) buffer Cmd { uint words[]; };\n"
			"void main() { words[0]=3u; words[1]=1u; words[2]=0u; words[3]=0u; words[4]=0u; }\n");
	const RID comp_pipe = rd.compute_pipeline_create(comp_shader);
	REQUIRE(comp_pipe.is_valid());

	const RID rast_shader = compile_raster_shader_glsl(
			rd,
			"#version 450\nlayout(set=0,binding=0,std430) readonly buffer Dat { vec4 tint; };\n"
			"layout(location=0) out vec4 v_c;\n"
			"void main() {\n"
			"  vec2 p = (gl_VertexIndex == 0) ? vec2(-1.0,-1.0) : ((gl_VertexIndex == 1) ? vec2(3.0,-1.0) : vec2(-1.0,3.0));\n"
			"  v_c = tint; gl_Position = vec4(p, 0.5, 1.0);\n"
			"}\n",
			"#version 450\nlayout(location=0) in vec4 v_c; layout(location=0) out vec4 o_c; void main() { o_c = v_c; }\n");

	RD::TextureFormat tf;
	tf.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
	tf.width = 8;
	tf.height = 8;
	tf.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
	const RID proto_tex = rd.texture_create(tf, RD::TextureView());
	Vector<RID> proto_fb_tex;
	proto_fb_tex.push_back(proto_tex);
	const RID proto_fb = rd.framebuffer_create(proto_fb_tex);
	const RD::FramebufferFormatID fb_fmt = rd.framebuffer_get_format(proto_fb);
	RD::PipelineColorBlendState bs;
	bs.attachments.push_back(RD::PipelineColorBlendState::Attachment());
	const RID rast_pipe = rd.render_pipeline_create(
			rast_shader,
			fb_fmt,
			RD::INVALID_ID,
			RD::RENDER_PRIMITIVE_TRIANGLES,
			RD::PipelineRasterizationState(),
			RD::PipelineMultisampleState(),
			RD::PipelineDepthStencilState(),
			bs);
	REQUIRE(rast_pipe.is_valid());
	rd.free_rid(proto_fb);
	rd.free_rid(proto_tex);
	rd.submit();
	rd.sync();

	CollisionRD collision;
	GDExtensionSceneOpaque32V1 dev_cap = {};
	dev_cap.bytes[0] = 0xfeu;
	collision.initialize(&rd, dev_cap);

	SafeNumeric<uint32_t> active_late_depth_leases{ 0u };
	const uint64_t baseline_mem = rd.get_memory_usage(RD::MEMORY_TOTAL);

	RibbonRD::Indexed144Layout ribbon_layout;
	REQUIRE(RibbonRD::plan_indexed144(2, true, 1, ribbon_layout) == OK);

	Vector<uint8_t> tri_indices;
	tri_indices.resize(12);
	encode_uint32(0u, tri_indices.ptrw() + 0);
	encode_uint32(1u, tri_indices.ptrw() + 4);
	encode_uint32(2u, tri_indices.ptrw() + 8);

	Vector<uint8_t> tint_bytes;
	tint_bytes.resize(16);
	encode_float(0.5f, tint_bytes.ptrw() + 0);
	encode_float(0.75f, tint_bytes.ptrw() + 4);
	encode_float(0.25f, tint_bytes.ptrw() + 8);
	encode_float(1.0f, tint_bytes.ptrw() + 12);

	Vector<Color> clear_cols;
	clear_cols.push_back(Color(0, 0, 0, 1));

	for (uint32_t cycle = 0; cycle < 128u; ++cycle) {
		// Update sphere collider snapshot & acquire lease in every cycle:
		LocalVector<RendererRD::SceneSphereRecord32> spheres;
		RendererRD::SceneSphereRecord32 sph;
		sph.center_y = float(cycle) * 0.05f;
		sph.radius = 1.5f;
		sph.set_collider_key(1000ull + cycle);
		spheres.push_back(sph);
		REQUIRE(collision.update_sphere_colliders(spheres, Vector3(-10, -10, -10), Vector3(10, 20, 10)) == OK);

		Ref<CollisionRD::SphereReadLease> sphere_lease;
		GDExtensionSceneOpaque32V1 sphere_tok = {};
		REQUIRE(collision.acquire_sphere_read_lease(sphere_lease, sphere_tok) == OK);
		REQUIRE(sphere_lease.is_valid());
		CHECK(sphere_lease->validate(dev_cap, sphere_tok) == OK);

		GDExtensionDeviceExit::LateTransparentDepthDescriptor ld_desc;
		ld_desc.viewport = RID::from_uint64(700ull + (cycle & 3u));
		ld_desc.depth_texture = RID::from_uint64(800ull + cycle);
		ld_desc.width = 8u;
		ld_desc.height = 8u;
		ld_desc.frame_number = cycle + 1u;
		const GDExtensionSceneOpaque32V1 ld_tok = GDExtensionDeviceExit::make_late_depth_sample_token(700ull + (cycle & 3u), cycle + 1u, 8u, 8u);
		Ref<GDExtensionDeviceExit::LateTransparentDepthContent> ld_lease = memnew(
				GDExtensionDeviceExit::LateTransparentDepthContent(dev_cap, ld_tok, ld_desc, &active_late_depth_leases));
		CHECK(ld_lease->validate(dev_cap, ld_tok) == OK);

		if (cycle % 3u == 0u) {
			// Branch A: Submit live GPU compute -> indirect indexed draw, then IMMEDIATELY free all resources while GPU is in flight!
			RibbonRD::AllocatedBuffers bufs;
			REQUIRE(RibbonRD::allocate_buffers_rd(rd, ribbon_layout, bufs) == OK);
			const RID tint_ssbo = rd.storage_buffer_create(16, tint_bytes);
			const RID idx_buf = rd.index_buffer_create(3, RD::INDEX_BUFFER_FORMAT_UINT32, tri_indices);
			const RID idx_arr = rd.index_array_create(idx_buf, 0, 3);
			const RID tex = rd.texture_create(tf, RD::TextureView());
			Vector<RID> fb_att;
			fb_att.push_back(tex);
			const RID fb = rd.framebuffer_create(fb_att);

			RD::Uniform cu;
			cu.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			cu.binding = 0;
			cu.append_id(bufs.rids[RibbonRD::PROTECTED_COMMANDS]);
			Vector<RD::Uniform> cus;
			cus.push_back(cu);
			const RID comp_uset = rd.uniform_set_create(cus, comp_shader, 0);

			RD::Uniform ru;
			ru.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			ru.binding = 0;
			ru.append_id(tint_ssbo);
			Vector<RD::Uniform> rus;
			rus.push_back(ru);
			const RID rast_uset = rd.uniform_set_create(rus, rast_shader, 0);

			RD::ComputeListID cl = rd.compute_list_begin();
			rd.compute_list_bind_compute_pipeline(cl, comp_pipe);
			rd.compute_list_bind_uniform_set(cl, comp_uset, 0);
			rd.compute_list_dispatch(cl, 1, 1, 1);
			rd.compute_list_end();

			RD::DrawListID dl = rd.draw_list_begin(fb, RD::DRAW_CLEAR_COLOR_ALL, clear_cols);
			rd.draw_list_bind_render_pipeline(dl, rast_pipe);
			rd.draw_list_bind_uniform_set(dl, rast_uset, 0);
			rd.draw_list_bind_index_array(dl, idx_arr);
			rd.draw_list_draw_indirect(dl, true, bufs.rids[RibbonRD::PROTECTED_COMMANDS], 0, 1, 0);
			rd.draw_list_end();

			rd.submit();

			// Immediate mid-flight child-before-parent teardown while Metal command buffer is actively in flight:
			CHECK(sphere_lease->return_after_reads() == OK);
			CHECK(ld_lease->return_after_reads() == OK);
			rd.free_rid(rast_uset);
			rd.free_rid(comp_uset);
			rd.free_rid(idx_arr);
			rd.free_rid(fb);
			rd.free_rid(tex);
			rd.free_rid(idx_buf);
			rd.free_rid(tint_ssbo);
			RibbonRD::free_buffers_rd(rd, bufs);
			CHECK(bufs.live_count == 0u);

			// Re-open next local RD recording frame after mid-flight disposal requests are queued:
			rd.sync();
		} else if (cycle % 3u == 1u) {
			// Branch B: Open draw_list_begin, bind pipeline + uniform set, abort/close with 0 draws, immediate teardown!
			const RID tint_ssbo = rd.storage_buffer_create(16, tint_bytes);
			const RID tex = rd.texture_create(tf, RD::TextureView());
			Vector<RID> fb_att;
			fb_att.push_back(tex);
			const RID fb = rd.framebuffer_create(fb_att);

			RD::Uniform ru;
			ru.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			ru.binding = 0;
			ru.append_id(tint_ssbo);
			Vector<RD::Uniform> rus;
			rus.push_back(ru);
			const RID rast_uset = rd.uniform_set_create(rus, rast_shader, 0);

			RD::DrawListID dl = rd.draw_list_begin(fb, RD::DRAW_CLEAR_COLOR_ALL, clear_cols);
			rd.draw_list_bind_render_pipeline(dl, rast_pipe);
			rd.draw_list_bind_uniform_set(dl, rast_uset, 0);
			rd.draw_list_end();

			CHECK(sphere_lease->return_after_reads() == OK);
			CHECK(ld_lease->return_after_reads() == OK);
			rd.free_rid(rast_uset);
			rd.free_rid(fb);
			rd.free_rid(tex);
			rd.free_rid(tint_ssbo);
		} else {
			// Branch C: Allocate Ribbon + ParticleLight RD buffers and tear down immediately before draw; periodic sync drain!
			RibbonRD::AllocatedBuffers bufs;
			REQUIRE(RibbonRD::allocate_buffers_rd(rd, ribbon_layout, bufs) == OK);

			LocalVector<RendererRD::ParticleLightRecord64> lights;
			RendererRD::ParticleLightRecord64 l0;
			l0.radius = 3.0f;
			l0.color_hdr[0] = 2.0f;
			l0.color_hdr[1] = 1.0f;
			l0.color_hdr[2] = 0.5f;
			l0.attenuation_exp = 1.0f;
			lights.push_back(l0);
			MeshLightRD::LightClusterBufferRD gpu_lights;
			REQUIRE(MeshLightRD::upload_particle_lights_rd(rd, 8u, lights, gpu_lights) == OK);

			CHECK(sphere_lease->return_after_reads() == OK);
			CHECK(ld_lease->return_after_reads() == OK);
			MeshLightRD::free_particle_lights_rd(rd, gpu_lights);
			RibbonRD::free_buffers_rd(rd, bufs);

			if ((cycle + 1u) % 16u == 0u) {
				rd.submit();
				rd.sync();
			}
		}
	}

	collision.shutdown();
	rd.submit();
	rd.sync();

	CHECK(collision.get_active_lease_count() == 0u);
	CHECK(collision.get_live_rid_count() == 0u);
	CHECK(active_late_depth_leases.get() == 0u);
	CHECK(rd.get_memory_usage(RD::MEMORY_TOTAL) == baseline_mem);

	rd.free_rid(rast_pipe);
	rd.free_rid(rast_shader);
	rd.free_rid(comp_pipe);
	rd.free_rid(comp_shader);
}
#endif

// ============================================================================
// CATEGORY 5: PrepareV3 ABI / Role Grammar Fuzzing & ANGECAP1 Symlink-Safe K03
// ============================================================================
TEST_CASE("[particle_pre_benchmark_adversarial] Cat-5 PrepareV3 ABI/role grammar fuzzing & ANGECAP1 symlink-safe atomic K03 capsule") {
	using namespace GDExtensionDeviceExit;

	// 5A. Resolve registered C interface entry point device_session_production_prepare_output_v3:
	GDExtensionInterfaceFunctionPtr raw_fn = GDExtension::get_interface_function("device_session_production_prepare_output_v3");
	REQUIRE(raw_fn != nullptr);
	const auto prepare_v3_fn = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionPrepareOutputV3>(raw_fn);

	// Discover active Registry::instance so valid pre-lock inputs reach production_request_locked -> ERR_DOES_NOT_EXIST:
	const GDExtensionProductionOutputPrepareV3 valid_idx_mv = make_baseline_prepare_v3_indexed144(true, 4u);
	GDExtensionProductionRequestRefV1 unknown_req = { 0ull, 0x7fffffff00000001ull };
	if (prepare_v3_fn(unknown_req, &valid_idx_mv).native_error != ERR_DOES_NOT_EXIST) {
		unknown_req.registry_instance = 1ull;
	}
	REQUIRE(prepare_v3_fn(unknown_req, &valid_idx_mv).native_error == ERR_DOES_NOT_EXIST);

	// Verify all 5 legal PrepareV3 role grammars pass pre-lock validation and reach ERR_DOES_NOT_EXIST:
	{
		// Grammar 2: INDEXED_144 without motion {3, 6, 7}:
		const GDExtensionProductionOutputPrepareV3 valid_idx_nomv = make_baseline_prepare_v3_indexed144(false, 4u);
		CHECK(prepare_v3_fn(unknown_req, &valid_idx_nomv).native_error == ERR_DOES_NOT_EXIST);

		// Grammar 3: PARTICLE_LIGHTS_V2 {8, 7} with read_capacity == 0 && read_graph_capacity == 0:
		GDExtensionProductionOutputPrepareV3 valid_lights = valid_idx_nomv;
		valid_lights.read_capacity = 0u;
		valid_lights.read_graph_capacity = 0u;
		for (uint32_t i = 0; i < 8u; ++i) {
			valid_lights.roles[i] = { 0u, UINT32_MAX, UINT32_MAX, 0u, 0u, 0u };
		}
		valid_lights.roles[0] = { GDEXTENSION_OUTPUT_ROLE_PARTICLE_LIGHTS_V2, 1u, 1u, 0u, 10u, 16ull * 64ull };
		valid_lights.roles[1] = { GDEXTENSION_OUTPUT_ROLE_OUTPUT_META_V1, 2u, 2u, 0u, 11u, 32ull };
		CHECK(prepare_v3_fn(unknown_req, &valid_lights).native_error == ERR_DOES_NOT_EXIST);

		// Grammar 4: INSTANCE_192 with motion {1, 2, 7}:
		GDExtensionProductionOutputPrepareV3 valid_inst_mv = valid_idx_nomv;
		for (uint32_t i = 0; i < 8u; ++i) {
			valid_inst_mv.roles[i] = { 0u, 0u, 0u, 0u, 0u, 0u }; // symmetric (0,0) trailing sentinels!
		}
		valid_inst_mv.roles[0] = { GDEXTENSION_OUTPUT_ROLE_CURRENT_RECORDS_V1, 1u, 1u, 0u, 10u, 8ull * 192ull };
		valid_inst_mv.roles[1] = { GDEXTENSION_OUTPUT_ROLE_ALIGNED_PREVIOUS_RECORDS_V1, 2u, 2u, 0u, 11u, 8ull * 192ull };
		valid_inst_mv.roles[2] = { GDEXTENSION_OUTPUT_ROLE_OUTPUT_META_V1, 3u, 3u, 0u, 12u, 32ull };
		CHECK(prepare_v3_fn(unknown_req, &valid_inst_mv).native_error == ERR_DOES_NOT_EXIST);

		// Grammar 5: INSTANCE_192 without motion {1, 7}:
		GDExtensionProductionOutputPrepareV3 valid_inst_nomv = valid_inst_mv;
		valid_inst_nomv.roles[1] = { GDEXTENSION_OUTPUT_ROLE_OUTPUT_META_V1, 3u, 3u, 0u, 12u, 32ull };
		valid_inst_nomv.roles[2] = { 0u, UINT32_MAX, UINT32_MAX, 0u, 0u, 0u };
		CHECK(prepare_v3_fn(unknown_req, &valid_inst_nomv).native_error == ERR_DOES_NOT_EXIST);
	}

	// 5B. Fuzz ABI, alignment, and outer DTO invariants -> ERR_INVALID_PARAMETER:
	{
		CHECK(prepare_v3_fn(unknown_req, nullptr).native_error == ERR_INVALID_PARAMETER);

		alignas(64) uint8_t raw_buf[sizeof(GDExtensionProductionOutputPrepareV3) + 16] = {};
		std::memcpy(raw_buf + 1, &valid_idx_mv, sizeof(valid_idx_mv));
		const auto *misaligned = reinterpret_cast<const GDExtensionProductionOutputPrepareV3 *>(raw_buf + 1);
		CHECK(prepare_v3_fn(unknown_req, misaligned).native_error == ERR_INVALID_PARAMETER);

		for (uint32_t bad_ver : { 0u, 1u, 4u, 99u }) {
			GDExtensionProductionOutputPrepareV3 m = valid_idx_mv;
			m.abi.abi_version = bad_ver;
			CHECK(prepare_v3_fn(unknown_req, &m).native_error == ERR_INVALID_PARAMETER);
		}
		{
			GDExtensionProductionOutputPrepareV3 m = valid_idx_mv;
			m.abi.struct_size -= 8u;
			CHECK(prepare_v3_fn(unknown_req, &m).native_error == ERR_INVALID_PARAMETER);
		}
		{
			GDExtensionProductionOutputPrepareV3 m = valid_idx_mv;
			m.abi.reserved = 1u;
			CHECK(prepare_v3_fn(unknown_req, &m).native_error == ERR_INVALID_PARAMETER);
		}
		{
			GDExtensionProductionOutputPrepareV3 m = valid_idx_mv;
			m.read_capacity = 4u;
			m.read_graph_capacity = 0u;
			CHECK(prepare_v3_fn(unknown_req, &m).native_error == ERR_INVALID_PARAMETER);
		}
	}

	// 5C. Fuzz role grammars, duplicate slots/uses, motion symmetry, strides & ParticleLight64 bounds -> ERR_INVALID_DATA:
	{
		// Invalid roles[0].role (0, 2, 4, 6, 7, 9, UINT32_MAX):
		for (uint32_t bad_r0 : { 0u, 2u, 4u, 6u, 7u, 9u, UINT32_MAX }) {
			GDExtensionProductionOutputPrepareV3 m = valid_idx_mv;
			m.roles[0].role = bad_r0;
			CHECK(prepare_v3_fn(unknown_req, &m).native_error == ERR_INVALID_DATA);
		}

		// Duplicate active roles:
		{
			GDExtensionProductionOutputPrepareV3 m = valid_idx_mv;
			m.roles[1].role = GDEXTENSION_OUTPUT_ROLE_CURRENT_VERTICES_V1;
			CHECK(prepare_v3_fn(unknown_req, &m).native_error == ERR_INVALID_DATA);
		}

		// Duplicate resource_slot across distinct active roles:
		{
			GDExtensionProductionOutputPrepareV3 m = valid_idx_mv;
			m.roles[3].resource_slot = m.roles[0].resource_slot;
			CHECK(prepare_v3_fn(unknown_req, &m).native_error == ERR_INVALID_DATA);
		}

		// Duplicate use_index across distinct active roles:
		{
			GDExtensionProductionOutputPrepareV3 m = valid_idx_mv;
			m.roles[3].use_index = m.roles[0].use_index;
			CHECK(prepare_v3_fn(unknown_req, &m).native_error == ERR_INVALID_DATA);
		}

		// Asymmetric trailing sentinel (resource_slot = 0, use_index = UINT32_MAX) and vice versa:
		{
			GDExtensionProductionOutputPrepareV3 m1 = valid_idx_mv;
			m1.roles[5].resource_slot = 0u;
			m1.roles[5].use_index = UINT32_MAX;
			CHECK(prepare_v3_fn(unknown_req, &m1).native_error == ERR_INVALID_DATA);

			GDExtensionProductionOutputPrepareV3 m2 = valid_idx_mv;
			m2.roles[5].resource_slot = UINT32_MAX;
			m2.roles[5].use_index = 0u;
			CHECK(prepare_v3_fn(unknown_req, &m2).native_error == ERR_INVALID_DATA);
		}

		// Asymmetric motion pair in INDEXED_144 (ALIGNED_PREVIOUS_VERTICES active while VERTEX_HISTORY_VALIDITY == 0):
		{
			GDExtensionProductionOutputPrepareV3 m = valid_idx_mv;
			m.roles[2] = { GDEXTENSION_OUTPUT_ROLE_VERTEX_HISTORY_VALIDITY_V1, UINT32_MAX, UINT32_MAX, 0u, 0u, 0ull };
			CHECK(prepare_v3_fn(unknown_req, &m).native_error == ERR_INVALID_DATA);
		}

		// Non-576B segment vertex size & mismatched index staging bytes in INDEXED_144:
		{
			GDExtensionProductionOutputPrepareV3 m1 = valid_idx_mv;
			m1.roles[0].bytes = 576ull * 4ull + 144ull;
			CHECK(prepare_v3_fn(unknown_req, &m1).native_error == ERR_INVALID_DATA);

			GDExtensionProductionOutputPrepareV3 m2 = valid_idx_mv;
			m2.roles[3].bytes = 24ull * 3ull; // 3 segs instead of 4
			CHECK(prepare_v3_fn(unknown_req, &m2).native_error == ERR_INVALID_DATA);
		}

		// ParticleLight64 role 8 with non-64B stride, >256 lights, or non-zero read_capacity:
		for (uint64_t bad_light_bytes : { 48ull, 65ull, 100ull, 257ull * 64ull }) {
			GDExtensionProductionOutputPrepareV3 m = valid_idx_mv;
			m.read_capacity = 0u;
			m.read_graph_capacity = 0u;
			for (uint32_t i = 0; i < 8u; ++i) {
				m.roles[i] = { 0u, UINT32_MAX, UINT32_MAX, 0u, 0u, 0u };
			}
			m.roles[0] = { GDEXTENSION_OUTPUT_ROLE_PARTICLE_LIGHTS_V2, 1u, 1u, 0u, 10u, bad_light_bytes };
			m.roles[1] = { GDEXTENSION_OUTPUT_ROLE_OUTPUT_META_V1, 2u, 2u, 0u, 11u, 32ull };
			CHECK(prepare_v3_fn(unknown_req, &m).native_error == ERR_INVALID_DATA);
		}
		{
			GDExtensionProductionOutputPrepareV3 m = valid_idx_mv;
			m.read_capacity = 1u;
			m.read_graph_capacity = 1u;
			for (uint32_t i = 0; i < 8u; ++i) {
				m.roles[i] = { 0u, UINT32_MAX, UINT32_MAX, 0u, 0u, 0u };
			}
			m.roles[0] = { GDEXTENSION_OUTPUT_ROLE_PARTICLE_LIGHTS_V2, 1u, 1u, 0u, 10u, 4ull * 64ull };
			m.roles[1] = { GDEXTENSION_OUTPUT_ROLE_OUTPUT_META_V1, 2u, 2u, 0u, 11u, 32ull };
			CHECK(prepare_v3_fn(unknown_req, &m).native_error == ERR_INVALID_DATA);
		}
	}

	// 5D. Fuzz validate_indexed144_prepare_v2_roles & project_indexed144_acquire_v2_writables:
	{
		GDExtensionProductionOutputRoleV1 v2_roles[8] = {};
		v2_roles[0] = { GDEXTENSION_OUTPUT_ROLE_CURRENT_VERTICES_V1, 1u, 1u, 0u, 10u, 2ull * 576ull };
		v2_roles[1] = { GDEXTENSION_OUTPUT_ROLE_ALIGNED_PREVIOUS_VERTICES_V1, 2u, 2u, 0u, 11u, 2ull * 576ull };
		v2_roles[2] = { GDEXTENSION_OUTPUT_ROLE_VERTEX_HISTORY_VALIDITY_V1, 3u, 3u, 0u, 12u, 2ull * 16ull };
		v2_roles[3] = { GDEXTENSION_OUTPUT_ROLE_INDEX_STAGING_V1, 4u, 4u, 0u, 13u, 2ull * 24ull };
		v2_roles[4] = { GDEXTENSION_OUTPUT_ROLE_OUTPUT_META_V1, 5u, 5u, 0u, 14u, 32ull };
		for (int i = 5; i < 8; ++i) {
			v2_roles[i] = { 0u, UINT32_MAX, UINT32_MAX, 0u, 0u, 0u };
		}
		CHECK(validate_indexed144_prepare_v2_roles(v2_roles, 2u, true) == OK);

		// Duplicate use_index rejected:
		GDExtensionProductionOutputRoleV1 dup_use[8];
		std::memcpy(dup_use, v2_roles, sizeof(v2_roles));
		dup_use[4].use_index = dup_use[0].use_index;
		CHECK(validate_indexed144_prepare_v2_roles(dup_use, 2u, true) == ERR_INVALID_DATA);

		// Duplicate primary_rid rejected in project_indexed144_acquire_v2_writables WITHOUT mutating r_out:
		Indexed144RoleBindingInput bind_in[5] = {
			{ GDEXTENSION_OUTPUT_ROLE_CURRENT_VERTICES_V1, 1u, 1u, 9001ull, 10u, 2ull * 576ull },
			{ GDEXTENSION_OUTPUT_ROLE_ALIGNED_PREVIOUS_VERTICES_V1, 2u, 2u, 9001ull, 11u, 2ull * 576ull }, // duplicate RID!
			{ GDEXTENSION_OUTPUT_ROLE_VERTEX_HISTORY_VALIDITY_V1, 3u, 3u, 9003ull, 12u, 2ull * 16ull },
			{ GDEXTENSION_OUTPUT_ROLE_INDEX_STAGING_V1, 4u, 4u, 9004ull, 13u, 2ull * 24ull },
			{ GDEXTENSION_OUTPUT_ROLE_OUTPUT_META_V1, 5u, 5u, 9005ull, 14u, 32ull },
		};
		GDExtensionOutputAcquireResultV2 acq_out;
		std::memset(&acq_out, 0x5a, sizeof(acq_out));
		const GDExtensionOutputAcquireResultV2 sentinel_out = acq_out;
		CHECK(project_indexed144_acquire_v2_writables(bind_in, 2u, true, acq_out) == ERR_INVALID_DATA);
		CHECK(std::memcmp(&acq_out, &sentinel_out, sizeof(acq_out)) == 0);
	}

	// 5E. ANGECAP1 atomic K03 capsule symlink rejection (final path & .tmp path) + JSON special-char round-trip:
	{
		CoreFailureWitnessV2 failure;
		failure.child_serial = 77ull;
		failure.registration_serial = 88ull;
		failure.failure_error = ERR_UNAUTHORIZED;
		failure.failure_origin = String::utf8("quote\"slash\\newline\ntab\tctrl\x01utf8_粒子");
		failure.source_stream_ref = String::utf8("ange://stream/\"adversarial\"\\path\n01");
		failure.close_disposition = "UnsafePreOSFailure";

		Vector<uint8_t> capsule_bytes;
		REQUIRE(emit_core_failure_capsule_v2(failure, capsule_bytes) == OK);

#if defined(UNIX_ENABLED) || defined(MACOS_ENABLED)
		const String victim_path = TestUtils::get_temp_path("f5_k03_symlink_victim.txt");
		const String symlink_out = TestUtils::get_temp_path("f5_k03_capsule_symlink.angecap1");
		const String regular_out = TestUtils::get_temp_path("f5_k03_capsule_regular.angecap1");
		const String regular_tmp = regular_out + ".tmp";

		::unlink(symlink_out.utf8().get_data());
		::unlink((symlink_out + ".tmp").utf8().get_data());
		::unlink(regular_out.utf8().get_data());
		::unlink(regular_tmp.utf8().get_data());

		{
			Ref<FileAccess> vf = FileAccess::open(victim_path, FileAccess::WRITE);
			REQUIRE(vf.is_valid());
			vf->store_string("DO_NOT_OVERWRITE_VICTIM");
		}

		// (i) Symlink at out_path -> ERR_UNAUTHORIZED, victim untouched:
		REQUIRE(::symlink(victim_path.utf8().get_data(), symlink_out.utf8().get_data()) == 0);
		CHECK(write_unload_capsule_stream_atomic(symlink_out, capsule_bytes) == ERR_UNAUTHORIZED);
		CHECK(FileAccess::get_file_as_string(victim_path) == "DO_NOT_OVERWRITE_VICTIM");
		::unlink(symlink_out.utf8().get_data());

		// (ii) Symlink at out_path + ".tmp" -> ERR_UNAUTHORIZED, victim untouched:
		REQUIRE(::symlink(victim_path.utf8().get_data(), regular_tmp.utf8().get_data()) == 0);
		CHECK(write_unload_capsule_stream_atomic(regular_out, capsule_bytes) == ERR_UNAUTHORIZED);
		CHECK(FileAccess::get_file_as_string(victim_path) == "DO_NOT_OVERWRITE_VICTIM");
		::unlink(regular_tmp.utf8().get_data());

		// (iii) Regular path -> atomic write succeeds, .tmp cleaned up, SHA-256 + JSON special-char round-trip exact:
		REQUIRE(write_unload_capsule_stream_atomic(regular_out, capsule_bytes) == OK);
		CHECK_FALSE(FileAccess::exists(regular_tmp));
		const Vector<uint8_t> disk_bytes = FileAccess::get_file_as_bytes(regular_out);
		VerifiedCapsuleSummary summary;
		REQUIRE(verify_unload_capsule_stream(disk_bytes, summary) == OK);
		CHECK(summary.is_core_failure);
		CHECK(summary.wire_version == ANGECAP1_WIRE_VERSION_FAILURE_V2);

		Vector<DecodedAngeCapFrame> frames;
		REQUIRE(decode_angecap1_stream(disk_bytes, frames) == OK);
		REQUIRE(frames.size() == 1);
		CHECK(String(frames[0].payload["CoreFailureOrigin"]) == failure.failure_origin);
		CHECK(String(frames[0].payload["CoreClose_source_stream_ref"]) == failure.source_stream_ref);

		::unlink(regular_out.utf8().get_data());
		::unlink(victim_path.utf8().get_data());
#endif
	}
}

// ============================================================================
// CATEGORY 6: Wave G-Godot Cross-ABI, K03 Stale-Capsule Unlink & Live Metal
//             Instance192Record Guard (Motion + No-Motion) Verification
// ============================================================================
TEST_CASE("[particle_pre_benchmark_adversarial][WaveGGodot] Cat-6 NATIVE-S02 K03 stale-capsule unlink & GODOT-01..06 cross-ABI + live Metal Instance192 guard") {
	using namespace GDExtensionDeviceExit;
	using MeshLightRD = RendererRD::ParticlesMeshLightSortRD;
	using CollisionRD = RendererRD::ParticlesCollisionSceneRD;

	// 6A. NATIVE-S02: Pre-existing regular file at ANGE_K03_CAPSULE_OUT is unlinked before write so
	//     a failed emission NEVER leaves a stale capsule on disk.
#if defined(UNIX_ENABLED) || defined(MACOS_ENABLED)
	{
		const String prev_env = OS::get_singleton()->get_environment("ANGE_K03_CAPSULE_OUT");
		const String prev_match = OS::get_singleton()->get_environment("GODOT_EXTENSION_UNLOAD_AUDIT_MATCH");
		const String out_path = TestUtils::get_temp_path("wave_g_k03_stale_capsule.angecap1");
		const String tmp_path = out_path + ".tmp";
		const String dummy_target = TestUtils::get_temp_path("wave_g_k03_dummy_target.txt");
		::unlink(out_path.utf8().get_data());
		::unlink(tmp_path.utf8().get_data());
		::unlink(dummy_target.utf8().get_data());

		{
			Ref<FileAccess> sf = FileAccess::open(out_path, FileAccess::WRITE);
			REQUIRE(sf.is_valid());
			sf->store_string("STALE_CAPSULE_BYTES_FROM_PRIOR_RUN");
		}
		{
			Ref<FileAccess> df = FileAccess::open(dummy_target, FileAccess::WRITE);
			REQUIRE(df.is_valid());
			df->store_string("TARGET");
		}
		REQUIRE(FileAccess::exists(out_path));
		// Plant symlink at out_path + ".tmp" so write_unload_capsule_stream_atomic fails closed:
		REQUIRE(::symlink(dummy_target.utf8().get_data(), tmp_path.utf8().get_data()) == 0);

		auto &local_reg = GDExtensionDeviceExit::Registry::get_singleton();
		GDExtensionDeviceExit::RecordRef wrapper_ref, scope_ref;
		REQUIRE(local_reg.reserve_open(uint32_t(GDExtensionLoader::LoaderKind::DYNAMIC), wrapper_ref, scope_ref) == OK);
		local_reg.attach(wrapper_ref);

		OS::get_singleton()->set_environment("ANGE_K03_CAPSULE_OUT", out_path);
		OS::get_singleton()->set_environment("GODOT_EXTENSION_UNLOAD_AUDIT_MATCH", "libtest_dummy.dylib");
		ERR_PRINT_OFF;
		const Error emit_err = local_reg.maybe_emit_post_close_capsule(wrapper_ref, "/tmp/libtest_dummy.dylib");
		ERR_PRINT_ON;
		CHECK(emit_err == ERR_UNAUTHORIZED);
		// Stale capsule at out_path MUST have been unlinked before the failed atomic write!
		CHECK_FALSE(FileAccess::exists(out_path));

		// Also verify symlink at out_path itself is rejected BEFORE unlink (symlink not followed, target intact):
		::unlink(tmp_path.utf8().get_data());
		REQUIRE(::symlink(dummy_target.utf8().get_data(), out_path.utf8().get_data()) == 0);
		ERR_PRINT_OFF;
		CHECK(local_reg.maybe_emit_post_close_capsule(wrapper_ref, "/tmp/libtest_dummy.dylib") == ERR_UNAUTHORIZED);
		ERR_PRINT_ON;
		CHECK(FileAccess::get_file_as_string(dummy_target) == "TARGET");

		local_reg.detach(wrapper_ref);
		local_reg.detach(scope_ref);

		::unlink(out_path.utf8().get_data());
		::unlink(tmp_path.utf8().get_data());
		::unlink(dummy_target.utf8().get_data());
		OS::get_singleton()->set_environment("ANGE_K03_CAPSULE_OUT", prev_env);
		OS::get_singleton()->set_environment("GODOT_EXTENSION_UNLOAD_AUDIT_MATCH", prev_match);
	}
#endif

	// 6B. GODOT-03a/b: Exact 48-word Instance192Record ABI layout matching instance192_inc.glsl:
	{
		static_assert(sizeof(RendererRD::Instance192Record) == 192);
		static_assert(offsetof(RendererRD::Instance192Record, transform_rows) == 0);
		static_assert(offsetof(RendererRD::Instance192Record, normal_row0) == 48);
		static_assert(offsetof(RendererRD::Instance192Record, pad0) == 60);
		static_assert(offsetof(RendererRD::Instance192Record, normal_row1) == 64);
		static_assert(offsetof(RendererRD::Instance192Record, pad1) == 76);
		static_assert(offsetof(RendererRD::Instance192Record, normal_row2) == 80);
		static_assert(offsetof(RendererRD::Instance192Record, pad2) == 92);
		static_assert(offsetof(RendererRD::Instance192Record, inverse_transform_rows) == 96);
		static_assert(offsetof(RendererRD::Instance192Record, color) == 144);
		static_assert(offsetof(RendererRD::Instance192Record, custom0) == 160);
		static_assert(offsetof(RendererRD::Instance192Record, particle_id_lo) == 176);
		static_assert(offsetof(RendererRD::Instance192Record, particle_id_hi) == 180);
		static_assert(offsetof(RendererRD::Instance192Record, flags) == 184);
		static_assert(offsetof(RendererRD::Instance192Record, reserved_zero) == 188);

		const Transform3D xf(Basis::from_scale(Vector3(2.0f, 3.0f, 4.0f)), Vector3(1.5f, -2.0f, 3.5f));
		const RendererRD::Instance192Record rec = MeshLightRD::make_instance192_record(
				xf,
				Vector3(99.0f, -88.0f, 77.0f),
				Color(0.25f, 0.5f, 0.75f, 1.0f),
				0.625f,
				0x0000000200000001ull,
				true);

		CHECK(rec.pad0 == 0u);
		CHECK(rec.pad1 == 0u);
		CHECK(rec.pad2 == 0u);
		CHECK(rec.reserved_zero == 0u);
		CHECK(Math::is_equal_approx(rec.normal_row0[0], 0.5f));
		CHECK(Math::is_equal_approx(rec.normal_row1[1], 1.0f / 3.0f));
		CHECK(Math::is_equal_approx(rec.normal_row2[2], 0.25f));
		CHECK(Math::is_equal_approx(rec.inverse_transform_rows[0][0], 0.5f));
		CHECK(Math::is_equal_approx(rec.inverse_transform_rows[1][1], 1.0f / 3.0f));
		CHECK(Math::is_equal_approx(rec.inverse_transform_rows[2][2], 0.25f));
		CHECK(Math::is_equal_approx(rec.inverse_transform_rows[0][3], -0.75f));
		CHECK(Math::is_equal_approx(rec.inverse_transform_rows[1][3], 2.0f / 3.0f));
		CHECK(Math::is_equal_approx(rec.inverse_transform_rows[2][3], -0.875f));
		CHECK(Math::is_equal_approx(rec.color[0], 0.25f));
		CHECK(Math::is_equal_approx(rec.color[1], 0.5f));
		CHECK(Math::is_equal_approx(rec.color[2], 0.75f));
		CHECK(Math::is_equal_approx(rec.color[3], 1.0f));
		CHECK(Math::is_equal_approx(rec.custom0[0], 0.0f));
		CHECK(Math::is_equal_approx(rec.custom0[1], 0.625f));
		CHECK(Math::is_equal_approx(rec.custom0[2], 0.0f));
		CHECK(Math::is_equal_approx(rec.custom0[3], 0.0f));
		CHECK(rec.get_particle_id() == 0x0000000200000001ull);
		CHECK(rec.flags == (RendererRD::Instance192Record::FLAG_VALID_RECORD | RendererRD::Instance192Record::FLAG_HAS_PREVIOUS_RECORD | RendererRD::Instance192Record::FLAG_NON_UNIFORM_SCALE | RendererRD::Instance192Record::FLAG_INVERSE_AVAILABLE));

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
		// Live Metal GPU execution of ParticlesExternalGuard on 2 Instance192Records in BOTH
		// MOTION_REQUIRED (modes INIT / VALIDATE / FINALIZE) and MOTION_DISABLED (modes INIT_NO_MOTION /
		// VALIDATE_NO_MOTION / FINALIZE_NO_MOTION with PREVIOUS_RECORDS = {RID(), 0})!
		REQUIRE(RenderingDevice::get_singleton() == nullptr);
		RenderingContextDriverMetal context;
		REQUIRE(context.initialize() == OK);
		RenderingDevice rd;
		REQUIRE(rd.initialize(&context) == OK);

		using Guard192 = RendererRD::ParticlesExternalGuard;
		Guard192 guard192;
		REQUIRE(guard192.prepare_pipelines_standalone_rd() == OK);

		RendererRD::Instance192Record records[2];
		records[0] = rec;
		records[1] = MeshLightRD::make_instance192_record(
				Transform3D(Basis(), Vector3(-1.0f, 0.5f, -2.0f)),
				Vector3(),
				Color(1.0f, 0.5f, 0.25f, 1.0f),
				0.25f,
				42ull,
				true);
		RendererRD::Instance192Record prev_records[2];
		prev_records[0] = records[0];
		prev_records[1] = records[1];

		Vector<uint8_t> cur_bytes;
		cur_bytes.resize(sizeof(records));
		std::memcpy(cur_bytes.ptrw(), records, sizeof(records));
		Vector<uint8_t> prev_bytes;
		prev_bytes.resize(sizeof(prev_records));
		std::memcpy(prev_bytes.ptrw(), prev_records, sizeof(prev_records));

		Vector<uint8_t> meta_bytes;
		meta_bytes.resize(32);
		std::memset(meta_bytes.ptrw(), 0, 32);
		encode_uint32(2u, meta_bytes.ptrw() + 0); // active_count = 2
		encode_uint32(2u, meta_bytes.ptrw() + 16); // capacity = 2
		encode_uint32(1u, meta_bytes.ptrw() + 20); // profile = 1 (INSTANCE_192)
		encode_uint32(7u, meta_bytes.ptrw() + 24); // sequence_low = 7
		encode_uint32(0u, meta_bytes.ptrw() + 28); // sequence_high = 0

		Vector<uint8_t> src_bytes;
		src_bytes.resize(32);
		std::memset(src_bytes.ptrw(), 0, 32);
		encode_uint32(2u, src_bytes.ptrw() + 0); // source count = 2 (gates at words 1..4 == 0)

		Vector<uint8_t> zero64;
		zero64.resize(64);
		zero64.fill(0);
		Vector<uint8_t> zero32;
		zero32.resize(32);
		zero32.fill(0);
		Vector<uint8_t> zero20;
		zero20.resize(20);
		zero20.fill(0);

		const RID cur_rid = rd.storage_buffer_create(cur_bytes.size(), cur_bytes);
		const RID prev_rid = rd.storage_buffer_create(prev_bytes.size(), prev_bytes);
		const RID meta_rid = rd.storage_buffer_create(32, meta_bytes);
		const RID src_rid = rd.storage_buffer_create(32, src_bytes);
		const RID scratch_rid = rd.storage_buffer_create(64, zero64);
		const RID status_rid = rd.storage_buffer_create(32, zero32);
		const RID cmd_rid = rd.storage_buffer_create(20, zero20, RD::STORAGE_BUFFER_USAGE_DISPATCH_INDIRECT);

		Guard192::SurfaceProof surf = {};
		for (int a = 0; a < 3; ++a) {
			surf.current_mesh_min[a] = -0.5f;
			surf.current_mesh_max[a] = 0.5f;
			surf.previous_mesh_min[a] = -0.5f;
			surf.previous_mesh_max[a] = 0.5f;
		}
		surf.command[0] = 1u;
		surf.command[1] = 36u;

		for (int motion_case = 0; motion_case < 2; ++motion_case) {
			const bool motion_enabled = (motion_case == 0);
			RendererRD::Instance192Record case_records[2] = { records[0], records[1] };
			if (!motion_enabled) {
				case_records[0].flags &= ~RendererRD::Instance192Record::FLAG_HAS_PREVIOUS_RECORD;
				case_records[1].flags &= ~RendererRD::Instance192Record::FLAG_HAS_PREVIOUS_RECORD;
			}
			REQUIRE(rd.buffer_update(cur_rid, 0, sizeof(case_records), case_records) == OK);

			Guard192::GeometryProofHeader proof = {};
			for (int a = 0; a < 3; ++a) {
				proof.current_decl_min[a] = -50.0f;
				proof.current_decl_max[a] = 50.0f;
				proof.previous_decl_min[a] = -50.0f;
				proof.previous_decl_max[a] = 50.0f;
				proof.cull_min[a] = -100.0f;
				proof.cull_max[a] = 100.0f;
				proof.current_host_inverse_rows[a][a] = 1.0f;
				proof.previous_host_inverse_rows[a][a] = 1.0f;
			}
			proof.source_accessor[0] = 0u;
			proof.source_accessor[3] = 1u; // WORLD
			proof.source_gate_words[0] = 1u;
			proof.source_gate_words[1] = 2u;
			proof.source_gate_words[2] = 3u;
			proof.source_gate_words[3] = 4u;
			proof.options[0] = motion_enabled ? 1u : 0u;

			Vector<uint8_t> proof_bytes;
			proof_bytes.resize(sizeof(proof) + sizeof(surf));
			std::memcpy(proof_bytes.ptrw(), &proof, sizeof(proof));
			std::memcpy(proof_bytes.ptrw() + sizeof(proof), &surf, sizeof(surf));
			const RID proof_rid = rd.storage_buffer_create(proof_bytes.size(), proof_bytes);

			Guard192::Bindings bindings = {};
			bindings.buffers[Guard192::CURRENT_RECORDS] = { cur_rid, sizeof(case_records) };
			bindings.buffers[Guard192::PREVIOUS_RECORDS] = motion_enabled
					? Guard192::BufferBinding{ prev_rid, sizeof(prev_records) }
					: Guard192::BufferBinding{ RID(), 0u };
			bindings.buffers[Guard192::OUTPUT_META] = { meta_rid, 32u };
			bindings.buffers[Guard192::SOURCE_COUNT] = { src_rid, 32u };
			bindings.buffers[Guard192::GUARD_SCRATCH] = { scratch_rid, 64u };
			bindings.buffers[Guard192::GUARD_STATUS] = { status_rid, 32u };
			bindings.buffers[Guard192::PROTECTED_COMMANDS] = { cmd_rid, 20u };
			bindings.buffers[Guard192::GEOMETRY_PROOF] = { proof_rid, uint64_t(proof_bytes.size()) };
			for (uint32_t g = 0; g < 4u; ++g) {
				bindings.buffers[Guard192::SOURCE_COUNT_VALIDITY + g] = { src_rid, 32u };
			}

			Guard192::PushConstant req = { 2u, 7u, 0u, 1u, 0u, 2u, 0u, 0u };
			Guard192::LayoutPlan plan;
			REQUIRE(Guard192::plan_layout(req, bindings, proof, 4u, plan) == OK);
			if (motion_enabled) {
				CHECK(plan.initialize_mode == Guard192::INIT);
				CHECK(plan.validate_mode == Guard192::VALIDATE);
				CHECK(plan.finalize_mode == Guard192::FINALIZE);
			} else {
				CHECK(plan.initialize_mode == Guard192::INIT_NO_MOTION);
				CHECK(plan.validate_mode == Guard192::VALIDATE_NO_MOTION);
				CHECK(plan.finalize_mode == Guard192::FINALIZE_NO_MOTION);
			}

			const RID init_uset = rd.uniform_set_create(plan.uniforms, guard192.get_shader(plan.initialize_mode), 0);
			const RID val_uset = rd.uniform_set_create(plan.uniforms, guard192.get_shader(plan.validate_mode), 0);
			const RID fin_uset = rd.uniform_set_create(plan.uniforms, guard192.get_shader(plan.finalize_mode), 0);
			REQUIRE(init_uset.is_valid());
			REQUIRE(val_uset.is_valid());
			REQUIRE(fin_uset.is_valid());

			RD::ComputeListID cl = rd.compute_list_begin();
			rd.compute_list_bind_compute_pipeline(cl, guard192.get_pipeline(plan.initialize_mode));
			rd.compute_list_bind_uniform_set(cl, init_uset, 0);
			rd.compute_list_set_push_constant(cl, &plan.initialize, sizeof(Guard192::PushConstant));
			rd.compute_list_dispatch(cl, 1, 1, 1);
			rd.compute_list_add_barrier(cl);
			for (int ci = 0; ci < plan.validation_chunks.size(); ++ci) {
				rd.compute_list_bind_compute_pipeline(cl, guard192.get_pipeline(plan.validate_mode));
				rd.compute_list_bind_uniform_set(cl, val_uset, 0);
				rd.compute_list_set_push_constant(cl, &plan.validation_chunks[ci], sizeof(Guard192::PushConstant));
				rd.compute_list_dispatch(cl, plan.validation_groups_x[ci], 1, 1);
			}
			rd.compute_list_add_barrier(cl);
			rd.compute_list_bind_compute_pipeline(cl, guard192.get_pipeline(plan.finalize_mode));
			rd.compute_list_bind_uniform_set(cl, fin_uset, 0);
			rd.compute_list_set_push_constant(cl, &plan.finalize, sizeof(Guard192::PushConstant));
			rd.compute_list_dispatch(cl, 1, 1, 1);
			rd.compute_list_end();
			rd.submit();
			rd.sync();

			const Vector<uint8_t> status_rb = rd.buffer_get_data(status_rid);
			const Vector<uint8_t> cmd_rb = rd.buffer_get_data(cmd_rid);
			CHECK(decode_uint32(status_rb.ptr() + 0) == 0u); // fault_bits == 0!
			CHECK(decode_uint32(status_rb.ptr() + 4) == 2u); // valid_records == 2!
			CHECK(decode_uint32(cmd_rb.ptr() + 0) == 36u); // index_count == 36
			CHECK(decode_uint32(cmd_rb.ptr() + 4) == 2u); // instance_count == 2

			rd.free_rid(fin_uset);
			rd.free_rid(val_uset);
			rd.free_rid(init_uset);
			rd.free_rid(proof_rid);
		}

		rd.free_rid(cmd_rid);
		rd.free_rid(status_rid);
		rd.free_rid(scratch_rid);
		rd.free_rid(src_rid);
		rd.free_rid(meta_rid);
		rd.free_rid(prev_rid);
		rd.free_rid(cur_rid);
		REQUIRE(guard192.release_pipelines_standalone_rd() == OK);
#endif
	}

	// 6C. GODOT-04 + GODOT-05 + GODOT-06: particle_id == 0 history rejection, custom flag preservation,
	//     NaN depth deterministic tail sort, and {0,0,0} black-light rejection:
	{
		MeshLightRD lane;
		LocalVector<RendererRD::Instance192Record> f1;
		RendererRD::Instance192Record r_zero = MeshLightRD::make_instance192_record(
				Transform3D(Basis(), Vector3(0.0f, 0.0f, -2.0f)), Vector3(), Color(1, 1, 1, 1), 0.0f, 0ull, false);
		r_zero.flags |= 28u; // custom upper bits (4 | 8 | 16)
		RendererRD::Instance192Record r_valid = MeshLightRD::make_instance192_record(
				Transform3D(Basis(), Vector3(0.0f, 0.0f, -5.0f)), Vector3(), Color(1, 1, 1, 1), 0.0f, 42ull, false);
		r_valid.flags |= 28u;
		f1.push_back(r_zero);
		f1.push_back(r_valid);

		LocalVector<RendererRD::Instance192Record> prev_out;
		LocalVector<uint32_t> sorted_out;
		REQUIRE(lane.submit_view_frame(501ull, 1ull, 1ull, 1ull, Vector3(0, 0, 10.0f), Vector3(0, 0, -1.0f), f1, prev_out, sorted_out) == OK);

		// Frame 2 with identical particle_id = 0 and particle_id = 42:
		LocalVector<RendererRD::Instance192Record> f2(f1);
		f2[0].transform_rows[2][3] = -3.0f;
		f2[1].transform_rows[2][3] = -6.0f;
		REQUIRE(lane.submit_view_frame(501ull, 1ull, 2ull, 2ull, Vector3(0, 0, 10.0f), Vector3(0, 0, -1.0f), f2, prev_out, sorted_out) == OK);
		REQUIRE(prev_out.size() == 2u);
		// particle_id == 0 MUST NOT false-match Frame 1's particle_id == 0!
		CHECK((prev_out[0].flags & RendererRD::Instance192Record::FLAG_HAS_PREVIOUS_RECORD) == 0u);
		CHECK((prev_out[0].flags & 28u) == 28u);
		// particle_id == 42 MUST match Frame 1's particle_id == 42 and preserve upper bits!
		CHECK((prev_out[1].flags & RendererRD::Instance192Record::FLAG_HAS_PREVIOUS_RECORD) != 0u);
		CHECK((prev_out[1].flags & 28u) == 28u);
		CHECK(Math::is_equal_approx(prev_out[1].transform_rows[2][3], -5.0f));

		// NaN depth particle sorts deterministically to the tail behind all finite depths:
		const float qnan = std::numeric_limits<float>::quiet_NaN();
		LocalVector<RendererRD::Instance192Record> nan_recs;
		nan_recs.push_back(MeshLightRD::make_instance192_record(Transform3D(Basis(), Vector3(0.0f, 0.0f, 5.0f)), Vector3(), Color(1, 1, 1, 1), 0.0f, 1ull, false)); // depth 5
		nan_recs.push_back(MeshLightRD::make_instance192_record(Transform3D(Basis(), Vector3(0.0f, 0.0f, 0.0f)), Vector3(), Color(1, 1, 1, 1), 0.0f, 2ull, false));
		nan_recs[1].transform_rows[0][3] = qnan; // raw non-finite translation -> -INF tail!
		nan_recs.push_back(MeshLightRD::make_instance192_record(Transform3D(Basis(), Vector3(0.0f, 0.0f, -5.0f)), Vector3(), Color(1, 1, 1, 1), 0.0f, 3ull, false)); // depth 15
		nan_recs.push_back(MeshLightRD::make_instance192_record(Transform3D(Basis(), Vector3(0.0f, 0.0f, 0.0f)), Vector3(), Color(1, 1, 1, 1), 0.0f, 4ull, false)); // depth 10
		REQUIRE(lane.submit_view_frame(502ull, 1ull, 1ull, 1ull, Vector3(0, 0, 10.0f), Vector3(0, 0, -1.0f), nan_recs, prev_out, sorted_out) == OK);
		REQUIRE(sorted_out.size() == 4u);
		CHECK(sorted_out[0] == 2u); // depth 15
		CHECK(sorted_out[1] == 3u); // depth 10
		CHECK(sorted_out[2] == 0u); // depth 5
		CHECK(sorted_out[3] == 1u); // NaN depth deterministically at tail!

		// Black light {0, 0, 0} rejected, positive radiance {0, 0.01, 0} accepted:
		LocalVector<RendererRD::ParticleLightRecord64> lights_in;
		RendererRD::ParticleLightRecord64 black_light = {};
		black_light.radius = 5.0f;
		black_light.attenuation_exp = 1.0f;
		black_light.screen_importance_q16 = 1000u;
		RendererRD::ParticleLightRecord64 dim_green = black_light;
		dim_green.color_hdr[1] = 0.01f;
		lights_in.push_back(black_light);
		lights_in.push_back(dim_green);
		LocalVector<RendererRD::ParticleLightRecord64> lights_out;
		REQUIRE(MeshLightRD::compact_particle_lights_deterministic(lights_in, 8u, lights_out) == OK);
		REQUIRE(lights_out.size() == 1u);
		CHECK(Math::is_equal_approx(lights_out[0].color_hdr[1], 0.01f));
	}

	// 6D. GODOT-01 + GODOT-02: Forward-Z sky sentinel (raw >= 1.0), hyperbolic perspective forward-Z
	//     unprojection, and tilted-surface push-out along world_normal:
	{
		// (i) Forward-Z (reverse_z = 0) sky sentinel raw == 1.0f -> empty_depth = true, collided = false:
		RendererRD::SceneHeightMeta352 hf = {};
		hf.width = 2;
		hf.height = 2;
		hf.bounds_min[0] = -10.0f;
		hf.bounds_min[1] = 0.0f;
		hf.bounds_min[2] = -10.0f;
		hf.bounds_max[0] = 10.0f;
		hf.bounds_max[1] = 100.0f;
		hf.bounds_max[2] = 10.0f;
		hf.z_near = 0.1f;
		hf.z_far = 100.1f;
		hf.reverse_z = 0;
		LocalVector<float> sky_texels;
		for (int i = 0; i < 4; ++i) {
			sky_texels.push_back(1.0f);
		}
		const RendererRD::CollisionContactResult hf_sky = CollisionRD::evaluate_heightfield_d32_collision(
				hf, sky_texels, Transform3D(), Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f), 0.5f, 1.0f / 60.0f);
		CHECK(hf_sky.empty_depth);
		CHECK_FALSE(hf_sky.collided);

		RendererRD::SceneCurrentDepthMeta256 dd_fwd = {};
		dd_fwd.width = 4;
		dd_fwd.height = 4;
		dd_fwd.z_near = 0.1f;
		dd_fwd.z_far = 100.0f;
		dd_fwd.thickness = 0.5f;
		dd_fwd.reverse_z = 0;
		dd_fwd.projection_rows[0][0] = 1.0f;
		dd_fwd.projection_rows[1][1] = 1.0f;
		dd_fwd.projection_rows[3][2] = -1.0f;
		dd_fwd.projection_rows[3][3] = 0.0f; // perspective!
		LocalVector<float> depth_sky_texels;
		for (int i = 0; i < 16; ++i) {
			depth_sky_texels.push_back(1.0f);
		}
		const RendererRD::CollisionContactResult depth_sky = CollisionRD::evaluate_current_depth_collision(
				dd_fwd, depth_sky_texels, Transform3D(), Vector3(0.0f, 0.0f, -0.21f), Vector3(0.0f, 0.0f, -0.05f), 0.05f, 0.0f);
		CHECK(depth_sky.empty_depth);
		CHECK_FALSE(depth_sky.collided);

		// (ii) Perspective forward-Z hyperbolic unprojection:
		// For n = 0.1, f = 100.0, raw = 0.5, reverse_z = 0:
		// True hyperbolic depth = (0.1 * 100.0) / (100.0 - 0.5 * 99.9) = 10.0 / 50.05 ≈ 0.1998002m!
		// (Broken linear lerp would have been 50.05m and missed a particle at view_pos.z = -0.21m -> linear_z = 0.21m with radius = 0.05m!)
		LocalVector<float> depth_half_texels;
		for (int i = 0; i < 16; ++i) {
			depth_half_texels.push_back(0.5f);
		}
		const RendererRD::CollisionContactResult depth_hyperbolic = CollisionRD::evaluate_current_depth_collision(
				dd_fwd, depth_half_texels, Transform3D(), Vector3(0.0f, 0.0f, -0.21f), Vector3(0.0f, 0.0f, -1.0f), 0.05f, 0.0f);
		CHECK_FALSE(depth_hyperbolic.empty_depth);
		CHECK(depth_hyperbolic.collided);

		// (iii) Tilted 45-degree heightfield surface:
		// 2x2 heightfield on x in [-1, 1], z in [-1, 1], bmax.y = 10, z_near = 0, z_far = 10, reverse_z = 1:
		// Left column (x=-1): raw = 0.6 -> linear_d = 4.0 -> h = 6.0; Right column (x=+1): raw = 0.4 -> linear_d = 6.0 -> h = 4.0!
		// Slope dh/dx = (4.0 - 6.0) / 2.0 = -1.0 -> world_normal = normalize(+1, +1, 0)!
		// At x = 0, surface_y = 5.0; particle at (0, 5.1, 0) with radius 0.5 has signed_normal_dist = 0.1 * 0.7071 < 0.5 -> penetration > 0,
		// and post_position MUST push out along +X as well as +Y!
		RendererRD::SceneHeightMeta352 hf_slope = {};
		hf_slope.width = 2;
		hf_slope.height = 2;
		hf_slope.bounds_min[0] = -1.0f;
		hf_slope.bounds_min[1] = 0.0f;
		hf_slope.bounds_min[2] = -1.0f;
		hf_slope.bounds_max[0] = 1.0f;
		hf_slope.bounds_max[1] = 10.0f;
		hf_slope.bounds_max[2] = 1.0f;
		hf_slope.z_near = 0.0f;
		hf_slope.z_far = 10.0f;
		hf_slope.reverse_z = 1;
		LocalVector<float> slope_texels;
		slope_texels.push_back(0.6f); // (x=0, y=0) -> h=6
		slope_texels.push_back(0.4f); // (x=1, y=0) -> h=4
		slope_texels.push_back(0.6f); // (x=0, y=1) -> h=6
		slope_texels.push_back(0.4f); // (x=1, y=1) -> h=4
		const RendererRD::CollisionContactResult slope_hit = CollisionRD::evaluate_heightfield_d32_collision(
				hf_slope, slope_texels, Transform3D(), Vector3(0.0f, 5.1f, 0.0f), Vector3(-1.0f, -1.0f, 0.0f), 0.5f, 0.0f);
		CHECK(slope_hit.collided);
		CHECK(slope_hit.post_position.x > 0.25f);
		CHECK(Math::is_equal_approx(slope_hit.post_position.x, slope_hit.post_position.y - 5.1f, 1e-4f));
	}
}

} // namespace TestParticlePreBenchmarkAdversarial
