/**************************************************************************/
/*  test_external_output_block23.cpp                                      */
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

#include "core/extension/gdextension_library_exit_guard.h"
#include "core/extension/gdextension_production_request.h"
#include "servers/rendering/renderer_rd/forward_clustered/scene_shader_forward_clustered.h"
#include "servers/rendering/renderer_rd/forward_mobile/scene_shader_forward_mobile.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_config.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_mesh_light_sort_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/renderer_scene_cull.h"
#include "servers/rendering/rendering_server.h"
#include "servers/rendering/rendering_server_globals.h"
#include "tests/test_macros.h"

#include <limits>

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
#include "drivers/metal/metal_allocator.h"
#include "drivers/metal/rendering_context_driver_metal.h"
#include "drivers/metal/rendering_device_driver_metal.h"
#endif

TEST_FORCE_LINK(test_external_output_block23)

namespace TestExternalOutputBlock23 {

struct BaseOnlyShaderData : public RendererRD::MaterialStorage::ShaderData {
	void set_code(const String &) override {}
	bool is_animated() const override { return false; }
	bool casts_shadows() const override { return false; }
	RenderingServerTypes::ShaderNativeSourceCode get_native_source_code() const override {
		return RenderingServerTypes::ShaderNativeSourceCode();
	}
	Pair<ShaderRD *, RID> get_native_shader_and_version() const override {
		return Pair<ShaderRD *, RID>(nullptr, RID());
	}
};

static bool g_mock_slot_visible = true;

static bool mock_is_slot_live(const RendererRD::ExternalOutputOwner *, uint64_t p_gen, uint32_t p_slot, uint64_t p_serial) {
	return p_gen == 11u && p_slot == 0u && p_serial == 3u;
}

static bool mock_is_slot_visible(const RendererRD::ExternalOutputOwner *, uint64_t p_gen, uint32_t p_slot, uint64_t p_serial) {
	return g_mock_slot_visible && p_gen == 11u && p_slot == 0u && p_serial == 3u;
}

TEST_CASE("[ExternalOutput][Block23] ShaderData virtual backbuffer texture queries") {
	BaseOnlyShaderData base_sd;
	const RendererRD::MaterialStorage::ShaderData &base_ref = base_sd;
	CHECK_FALSE(base_ref.uses_screen_texture_backbuffer());
	CHECK_FALSE(base_ref.uses_depth_texture_backbuffer());

	RendererSceneRenderImplementation::SceneShaderForwardMobile::ShaderData mobile_sd;
	const RendererRD::MaterialStorage::ShaderData &mobile_ref = mobile_sd;
	CHECK_FALSE(mobile_ref.uses_screen_texture_backbuffer());
	CHECK_FALSE(mobile_ref.uses_depth_texture_backbuffer());
	mobile_sd.uses_screen_texture = true;
	CHECK(mobile_ref.uses_screen_texture_backbuffer());
	CHECK_FALSE(mobile_ref.uses_depth_texture_backbuffer());
	mobile_sd.uses_depth_texture = true;
	CHECK(mobile_ref.uses_screen_texture_backbuffer());
	CHECK(mobile_ref.uses_depth_texture_backbuffer());

	RendererSceneRenderImplementation::SceneShaderForwardClustered::ShaderData clustered_sd;
	const RendererRD::MaterialStorage::ShaderData &clustered_ref = clustered_sd;
	CHECK_FALSE(clustered_ref.uses_screen_texture_backbuffer());
	CHECK_FALSE(clustered_ref.uses_depth_texture_backbuffer());
	clustered_sd.uses_depth_texture = true;
	CHECK_FALSE(clustered_ref.uses_screen_texture_backbuffer());
	CHECK(clustered_ref.uses_depth_texture_backbuffer());
	clustered_sd.uses_screen_texture = true;
	CHECK(clustered_ref.uses_screen_texture_backbuffer());
	CHECK(clustered_ref.uses_depth_texture_backbuffer());

	CHECK_FALSE(GDExtensionDeviceExit::Registry::get_singleton().has_natural_close_pending_work(nullptr));
	CHECK(GDExtensionDeviceExit::ProductionGraphCell::MAX_OUTPUT_READ_TRANSFERS == 32u);
}

TEST_CASE("[ExternalOutput][Block23] fast_refresh_invert_affine_f32 flushes subnormals without discarding valid rotation") {
	// 90-degree Z rotation with a subnormal (0,0) component (det == 1.0):
	// inverse has subnormal (1,1) entry which must flush to 0.0f while preserving
	// the valid rotation and rotated translation.
	const float subnormal = std::numeric_limits<float>::min() * 0.25f;
	REQUIRE(subnormal != 0.0f);
	REQUIRE(Math::abs(subnormal) < std::numeric_limits<float>::min());

	const float rotated[12] = {
		subnormal, -1.0f, 0.0f, 3.0f,
		1.0f, 0.0f, 0.0f, -4.0f,
		0.0f, 0.0f, 1.0f, 5.0f,
	};
	float inv[12] = {};
	REQUIRE(RendererRD::ParticlesStorage::fast_refresh_invert_affine_f32(rotated, inv));
	CHECK(inv[0] == 0.0f);
	CHECK(inv[1] == doctest::Approx(1.0f));
	CHECK(inv[2] == 0.0f);
	CHECK(inv[3] == doctest::Approx(4.0f));
	CHECK(inv[4] == doctest::Approx(-1.0f));
	CHECK(inv[5] == 0.0f);
	CHECK(inv[6] == 0.0f);
	CHECK(inv[7] == doctest::Approx(3.0f));
	CHECK(inv[8] == 0.0f);
	CHECK(inv[9] == 0.0f);
	CHECK(inv[10] == doctest::Approx(1.0f));
	CHECK(inv[11] == doctest::Approx(-5.0f));

	// True singular matrix (zero scale on Y axis) falls back cleanly to identity basis
	// and negated translation so Tier-2 fast-refresh stays resident without shader faults.
	const float singular[12] = {
		1.0f, 0.0f, 0.0f, 2.0f,
		0.0f, 0.0f, 0.0f, -3.0f,
		0.0f, 0.0f, 1.0f, 4.0f,
	};
	float singular_inv[12] = {};
	REQUIRE(RendererRD::ParticlesStorage::fast_refresh_invert_affine_f32(singular, singular_inv));
	CHECK(singular_inv[0] == 1.0f);
	CHECK(singular_inv[1] == 0.0f);
	CHECK(singular_inv[2] == 0.0f);
	CHECK(singular_inv[3] == -2.0f);
	CHECK(singular_inv[4] == 0.0f);
	CHECK(singular_inv[5] == 1.0f);
	CHECK(singular_inv[6] == 0.0f);
	CHECK(singular_inv[7] == 3.0f);
	CHECK(singular_inv[8] == 0.0f);
	CHECK(singular_inv[9] == 0.0f);
	CHECK(singular_inv[10] == 1.0f);
	CHECK(singular_inv[11] == -4.0f);

	// Non-finite input still fails validation.
	float nan_affine[12] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, std::numeric_limits<float>::quiet_NaN(),
	};
	CHECK_FALSE(RendererRD::ParticlesStorage::fast_refresh_invert_affine_f32(nan_affine, singular_inv));
}

TEST_CASE("[ExternalOutput][BUG-99] PARTICLE_LIGHT_64 fast-refresh GUARD_STATUS uses canonical 32-byte ABI layout") {
	// Verify canonical GUARD_STATUS word layout matching gdextension_production_output.cpp
	// and ANGE validate_output_guard_status32:
	//   { 0, 0, 0, 0, seq_lo, seq_hi, 1, 0 }
	// (Regression guard against M-06 where fast-refresh wrote OUTPUT_META layout
	//   { 0, 0, 0, 1, cap, 4, seq_lo, seq_hi } into GUARD_STATUS.)
	const uint64_t test_seqs[3] = {
		1ull,
		0x00000000FFFFFFFFull,
		0x123456789ABCDEF0ull,
	};
	for (uint64_t seq : test_seqs) {
		uint32_t words[8] = { 0xdeadbeefu, 0xdeadbeefu, 0xdeadbeefu, 0xdeadbeefu, 0xdeadbeefu, 0xdeadbeefu, 0xdeadbeefu, 0xdeadbeefu };
		RendererRD::ParticlesStorage::fast_refresh_encode_light64_guard_status(seq, words);
		CHECK(words[0] == 0u);
		CHECK(words[1] == 0u);
		CHECK(words[2] == 0u);
		CHECK(words[3] == 0u);
		CHECK(words[4] == uint32_t(seq));
		CHECK(words[5] == uint32_t(seq >> 32));
		CHECK(words[6] == 1u);
		CHECK(words[7] == 0u);
	}
}

TEST_CASE("[SceneTree][ExternalOutput][Block23] RendererSceneCull instance_is_visible tracks visibility state") {
	RenderingServer *rs = RenderingServer::get_singleton();
	REQUIRE(rs != nullptr);
	REQUIRE(RSG::scene != nullptr);
	RendererSceneCull *scene_cull = static_cast<RendererSceneCull *>(RSG::scene);

	CHECK_FALSE(scene_cull->instance_is_visible(RID()));

	const RID scenario = rs->scenario_create();
	const RID instance = rs->instance_create();
	rs->instance_set_scenario(instance, scenario);
	rs->sync();

	CHECK(scene_cull->instance_is_visible(instance));

	rs->instance_set_visible(instance, false);
	rs->sync();
	CHECK_FALSE(scene_cull->instance_is_visible(instance));

	rs->instance_set_visible(instance, true);
	rs->sync();
	CHECK(scene_cull->instance_is_visible(instance));

	rs->free_rid(instance);
	rs->free_rid(scenario);
	rs->sync();
	CHECK_FALSE(scene_cull->instance_is_visible(instance));
}

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
TEST_CASE("[ExternalOutput][Block23] ParticleLight64 instant hide via is_slot_visible_fn and withdraw unregister") {
	REQUIRE(RenderingDevice::get_singleton() == nullptr);
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	using Lane = RendererRD::ParticlesMeshLightSortRD;
	Lane lane;

	RendererRD::ParticleLightRecord64 gpu_recs[2] = {};
	gpu_recs[0].position_world[0] = 1.0f;
	gpu_recs[0].position_world[1] = 2.0f;
	gpu_recs[0].position_world[2] = -3.0f;
	gpu_recs[0].radius = 6.0f;
	gpu_recs[0].color_hdr[0] = 3.0f;
	gpu_recs[0].color_hdr[1] = 1.0f;
	gpu_recs[0].color_hdr[2] = 0.5f;
	gpu_recs[0].attenuation_exp = 1.0f;
	gpu_recs[0].flags = RendererRD::ParticleLightRecord64::FLAG_ACTIVE;
	gpu_recs[0].id_index = 1u;
	gpu_recs[0].id_acquire_tag = 10u;
	gpu_recs[0].screen_importance_q16 = 4096u;
	gpu_recs[0].specular_scale = 1.0f;
	gpu_recs[0].diffuse_scale = 1.0f;

	const uint32_t meta_words[8] = {
		1u, 0u, 0u, 0u, 2u, uint32_t(RendererRD::ExternalOutputConfig::PARTICLE_LIGHT_64), 5u, 0u
	};
	Vector<uint8_t> meta_init;
	meta_init.resize(sizeof(meta_words));
	memcpy(meta_init.ptrw(), meta_words, sizeof(meta_words));

	Vector<uint8_t> recs_init;
	recs_init.resize(sizeof(gpu_recs));
	memcpy(recs_init.ptrw(), gpu_recs, sizeof(gpu_recs));

	const RID meta_rid = rd.storage_buffer_create(sizeof(meta_words), meta_init);
	const RID recs_rid = rd.storage_buffer_create(sizeof(gpu_recs), recs_init);
	const RID omni_buf = rd.storage_buffer_create(8u * 224u);
	rd.buffer_clear(omni_buf, 0, 8u * 224u);

	const RID vp_rid = RID::from_uint64(0x3333u);
	const RID scen_rid = RID::from_uint64(0xCCCCu);
	const RendererRD::ExternalOutputOwner *dummy_owner = reinterpret_cast<const RendererRD::ExternalOutputOwner *>(uintptr_t(0x4000));

	Lane::ExternalLightSlotRegistration reg;
	reg.owner_key = 0x9901u;
	reg.owner_ptr = dummy_owner;
	reg.is_slot_live_fn = &mock_is_slot_live;
	reg.is_slot_visible_fn = &mock_is_slot_visible;
	reg.generation = 11u;
	reg.slot_index = 0u;
	reg.lease_serial = 3u;
	reg.viewport_rid = vp_rid;
	reg.scenario_rid = scen_rid;
	reg.world_bounds = AABB(Vector3(-10, -10, -10), Vector3(20, 20, 20));
	reg.meta_rid = meta_rid;
	reg.records_rid = recs_rid;
	reg.capacity = 2u;
	reg.present_seq = 5u;

	g_mock_slot_visible = true;
	lane.register_sealed_external_light_slot(reg);

	LocalVector<Lane::ReservedGpuLightBounds> bounds;
	CHECK(lane.dispatch_registered_external_lights_gpu(&rd, vp_rid, scen_rid, false, Transform3D(), 1.0f, omni_buf, 0u, 8u, bounds) == 2u);
	CHECK(bounds.size() == 1u);

	// Instant hide (visible = false) skips dispatching lights without erasing the registered owner:
	g_mock_slot_visible = false;
	CHECK(lane.dispatch_registered_external_lights_gpu(&rd, vp_rid, scen_rid, false, Transform3D(), 1.0f, omni_buf, 0u, 8u, bounds) == 0u);
	CHECK(bounds.is_empty());

	// Restoring visibility resumes light dispatch immediately:
	g_mock_slot_visible = true;
	CHECK(lane.dispatch_registered_external_lights_gpu(&rd, vp_rid, scen_rid, false, Transform3D(), 1.0f, omni_buf, 0u, 8u, bounds) == 2u);
	CHECK(bounds.size() == 1u);

	// Withdraw permanently unregisters the owner:
	lane.unregister_external_light_owner(0x9901u);
	CHECK(lane.dispatch_registered_external_lights_gpu(&rd, vp_rid, scen_rid, false, Transform3D(), 1.0f, omni_buf, 0u, 8u, bounds) == 0u);

	lane.release_light_gpu_bridge(&rd);
	rd.free_rid(omni_buf);
	rd.free_rid(recs_rid);
	rd.free_rid(meta_rid);
}

TEST_CASE("[ExternalOutput][Block23] compute_list_dispatch guards missing or freed uniform sets without crashing") {
	REQUIRE(RenderingDevice::get_singleton() == nullptr);
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	static constexpr const char *kWriteSsboGlsl =
			"#version 450\n"
			"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
			"layout(set = 0, binding = 0, std430) buffer OutBuf { uint v; } u_out;\n"
			"void main() { u_out.v = 1u; }\n";
	String shd_err;
	RD::ShaderStageSPIRVData cs_stage;
	cs_stage.shader_stage = RD::SHADER_STAGE_COMPUTE;
	cs_stage.spirv = rd.shader_compile_spirv_from_source(RD::SHADER_STAGE_COMPUTE, String::utf8(kWriteSsboGlsl), RD::SHADER_LANGUAGE_GLSL, &shd_err, false);
	REQUIRE(!cs_stage.spirv.is_empty());

	Vector<RD::ShaderStageSPIRVData> stages;
	stages.push_back(cs_stage);
	const RID shader = rd.shader_create_from_spirv(stages);
	REQUIRE(shader.is_valid());
	const RID pipeline = rd.compute_pipeline_create(shader);
	REQUIRE(pipeline.is_valid());

	const RID ssbo = rd.storage_buffer_create(16u);
	REQUIRE(ssbo.is_valid());
	REQUIRE(rd.buffer_clear(ssbo, 0u, 16u) == OK);
	Vector<uint8_t> indirect_init;
	indirect_init.resize(16);
	memset(indirect_init.ptrw(), 0, 16);
	uint32_t *ind = reinterpret_cast<uint32_t *>(indirect_init.ptrw());
	ind[0] = 1u;
	ind[1] = 1u;
	ind[2] = 1u;
	const RID indirect_buf = rd.storage_buffer_create(16u, indirect_init, RD::STORAGE_BUFFER_USAGE_DISPATCH_INDIRECT);
	REQUIRE(indirect_buf.is_valid());

	// Flush initial buffer uploads before acquiring an immutable hold.
	rd.submit();
	rd.sync();

	// Case 1: writable SSBO held immutable causes compute_list_bind_uniform_set to fail;
	// subsequent dispatch / dispatch_indirect must return cleanly without dereferencing nullptr.
	// Acquire the hold before creating the writable uniform set so native_immutable_hold_acquire
	// does not reject the pre-existing writable dependent uniform set.
	Vector<RID> roots;
	roots.push_back(ssbo);
	RD::NativeImmutableHold hold;
	REQUIRE(rd.native_immutable_hold_acquire(roots, hold, 8u) == OK);

	Vector<RD::Uniform> uniforms;
	RD::Uniform u;
	u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
	u.binding = 0;
	u.append_id(ssbo);
	uniforms.push_back(u);
	const RID uniform_set = rd.uniform_set_create(uniforms, shader, 0);
	REQUIRE(uniform_set.is_valid());
	ERR_PRINT_OFF;
	{
		RD::ComputeListID cl = rd.compute_list_begin();
		rd.compute_list_bind_compute_pipeline(cl, pipeline);
		rd.compute_list_bind_uniform_set(cl, uniform_set, 0);
		rd.compute_list_dispatch(cl, 1, 1, 1);
		rd.compute_list_dispatch_indirect(cl, indirect_buf, 0);
		rd.compute_list_end();
	}
	ERR_PRINT_ON;
	REQUIRE(rd.native_immutable_hold_release(hold) == OK);
	rd.submit();
	rd.sync();
	{
		const Vector<uint8_t> out = rd.buffer_get_data(ssbo);
		REQUIRE(out.size() == 16);
		uint32_t v = 0xdeadbeefu;
		memcpy(&v, out.ptr(), sizeof(v));
		CHECK(v == 0u);
	}

	// Case 2: uniform set freed after binding (format still matches pipeline);
	// dispatch / dispatch_indirect must still guard null uniform_set cleanly.
	ERR_PRINT_OFF;
	{
		RD::ComputeListID cl = rd.compute_list_begin();
		rd.compute_list_bind_compute_pipeline(cl, pipeline);
		rd.compute_list_bind_uniform_set(cl, uniform_set, 0);
		rd.free_rid(uniform_set);
		rd.compute_list_dispatch(cl, 1, 1, 1);
		rd.compute_list_dispatch_indirect(cl, indirect_buf, 0);
		rd.compute_list_end();
	}
	ERR_PRINT_ON;
	rd.submit();
	rd.sync();
	{
		const Vector<uint8_t> out = rd.buffer_get_data(ssbo);
		REQUIRE(out.size() == 16);
		uint32_t v = 0xdeadbeefu;
		memcpy(&v, out.ptr(), sizeof(v));
		CHECK(v == 0u);
	}

	rd.free_rid(indirect_buf);
	rd.free_rid(ssbo);
	rd.free_rid(pipeline);
	rd.free_rid(shader);
}

TEST_CASE("[ExternalOutput][Block23] Metal driver render PSO cache reuses pipeline across consecutive lease frames and invalidates on shader recreation") {
	REQUIRE(RenderingDevice::get_singleton() == nullptr);
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	static constexpr const char *kVertGlsl =
			"#version 450\n"
			"void main() {\n"
			"    vec2 pos[3] = vec2[](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));\n"
			"    gl_Position = vec4(pos[gl_VertexIndex % 3], 0.0, 1.0);\n"
			"}\n";
	static constexpr const char *kFragGlsl =
			"#version 450\n"
			"layout(location = 0) out vec4 out_color;\n"
			"void main() {\n"
			"    out_color = vec4(1.0, 0.5, 0.25, 1.0);\n"
			"}\n";

	auto compile_render_shader = [&]() -> RID {
		String err;
		RD::ShaderStageSPIRVData vs;
		vs.shader_stage = RD::SHADER_STAGE_VERTEX;
		vs.spirv = rd.shader_compile_spirv_from_source(RD::SHADER_STAGE_VERTEX, String::utf8(kVertGlsl), RD::SHADER_LANGUAGE_GLSL, &err, false);
		REQUIRE(!vs.spirv.is_empty());
		RD::ShaderStageSPIRVData fs;
		fs.shader_stage = RD::SHADER_STAGE_FRAGMENT;
		fs.spirv = rd.shader_compile_spirv_from_source(RD::SHADER_STAGE_FRAGMENT, String::utf8(kFragGlsl), RD::SHADER_LANGUAGE_GLSL, &err, false);
		REQUIRE(!fs.spirv.is_empty());
		Vector<RD::ShaderStageSPIRVData> stages;
		stages.push_back(vs);
		stages.push_back(fs);
		const RID s = rd.shader_create_from_spirv(stages);
		REQUIRE(s.is_valid());
		return s;
	};

	Vector<RD::AttachmentFormat> attachments;
	RD::AttachmentFormat color_att;
	color_att.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
	color_att.samples = RD::TEXTURE_SAMPLES_1;
	color_att.usage_flags = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
	attachments.push_back(color_att);
	const RD::FramebufferFormatID fb_fmt = rd.framebuffer_format_create(attachments);
	REQUIRE(fb_fmt != RD::INVALID_FORMAT_ID);

	const RID shader_gen1 = compile_render_shader();
	const RD::PipelineRasterizationState raster;
	const RD::PipelineMultisampleState msaa;
	const RD::PipelineDepthStencilState ds;
	const RD::PipelineColorBlendState blend = RD::PipelineColorBlendState::create_disabled(1);

	const uint64_t hits_before = RenderingDeviceDriverMetal::get_total_render_pso_cache_hits();
	const uint64_t misses_before = RenderingDeviceDriverMetal::get_total_render_pso_cache_misses();

	// Frame 1 (lease_serial = 1): cold miss compiles MTL::RenderPipelineState once.
	const RID pso_frame1 = rd.render_pipeline_create(shader_gen1, fb_fmt, RD::INVALID_FORMAT_ID, RD::RENDER_PRIMITIVE_TRIANGLES, raster, msaa, ds, blend);
	REQUIRE(pso_frame1.is_valid());
	CHECK(RenderingDeviceDriverMetal::get_total_render_pso_cache_misses() == misses_before + 1u);
	CHECK(RenderingDeviceDriverMetal::get_total_render_pso_cache_hits() == hits_before);
	rd.free_rid(pso_frame1);

	// Frames 2..4 (monotonically increasing lease_serials): every new per-slot pipeline
	// hits the Metal PSO cache without invoking newRenderPipelineState again.
	for (uint32_t lease_serial = 2; lease_serial <= 4; ++lease_serial) {
		const RID pso_next = rd.render_pipeline_create(shader_gen1, fb_fmt, RD::INVALID_FORMAT_ID, RD::RENDER_PRIMITIVE_TRIANGLES, raster, msaa, ds, blend);
		REQUIRE(pso_next.is_valid());
		rd.free_rid(pso_next);
	}
	CHECK(RenderingDeviceDriverMetal::get_total_render_pso_cache_misses() == misses_before + 1u);
	CHECK(RenderingDeviceDriverMetal::get_total_render_pso_cache_hits() == hits_before + 3u);

	// Material generation change (free old shader, create new shader): must invalidate
	// the prior shader's cache entries and miss once before hitting again.
	rd.free_rid(shader_gen1);
	rd.submit();
	rd.sync();

	const RID shader_gen2 = compile_render_shader();
	const RID pso_gen2_a = rd.render_pipeline_create(shader_gen2, fb_fmt, RD::INVALID_FORMAT_ID, RD::RENDER_PRIMITIVE_TRIANGLES, raster, msaa, ds, blend);
	REQUIRE(pso_gen2_a.is_valid());
	CHECK(RenderingDeviceDriverMetal::get_total_render_pso_cache_misses() == misses_before + 2u);
	CHECK(RenderingDeviceDriverMetal::get_total_render_pso_cache_hits() == hits_before + 3u);
	rd.free_rid(pso_gen2_a);

	const RID pso_gen2_b = rd.render_pipeline_create(shader_gen2, fb_fmt, RD::INVALID_FORMAT_ID, RD::RENDER_PRIMITIVE_TRIANGLES, raster, msaa, ds, blend);
	REQUIRE(pso_gen2_b.is_valid());
	CHECK(RenderingDeviceDriverMetal::get_total_render_pso_cache_misses() == misses_before + 2u);
	CHECK(RenderingDeviceDriverMetal::get_total_render_pso_cache_hits() == hits_before + 4u);
	rd.free_rid(pso_gen2_b);
	rd.free_rid(shader_gen2);
	rd.submit();
	rd.sync();
}

TEST_CASE("[ExternalOutput][Block23] Metal heap allocator recycles freed storage buffers across consecutive lease frames") {
	REQUIRE(RenderingDevice::get_singleton() == nullptr);
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	const uint64_t recycle_before = MetalHeapAllocator::get_total_buffer_recycle_hits();
	constexpr uint32_t kSlotBytes = 192u * 512u;

	const RID buf1 = rd.storage_buffer_create(kSlotBytes);
	REQUIRE(buf1.is_valid());
	rd.free_rid(buf1);
	rd.submit();
	rd.sync();

	const RID buf2 = rd.storage_buffer_create(kSlotBytes);
	REQUIRE(buf2.is_valid());
	CHECK(MetalHeapAllocator::get_total_buffer_recycle_hits() > recycle_before);
	rd.free_rid(buf2);
	rd.submit();
	rd.sync();
}

TEST_CASE("[ExternalOutput][Block23] BUG-50 PARTICLE_LIGHT_64 fast-refresh updates world_bounds and scaled camera cluster emit matches ClusterBuilderRD") {
	REQUIRE(RenderingDevice::get_singleton() == nullptr);
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	using Lane = RendererRD::ParticlesMeshLightSortRD;
	Lane lane;

	RendererRD::ParticleLightRecord64 gpu_recs[2] = {};
	gpu_recs[0].position_world[0] = 3.0f;
	gpu_recs[0].position_world[1] = -2.0f;
	gpu_recs[0].position_world[2] = -5.0f;
	gpu_recs[0].radius = 4.0f;
	gpu_recs[0].color_hdr[0] = 2.0f;
	gpu_recs[0].color_hdr[1] = 1.0f;
	gpu_recs[0].color_hdr[2] = 0.5f;
	gpu_recs[0].attenuation_exp = 1.0f;
	gpu_recs[0].flags = RendererRD::ParticleLightRecord64::FLAG_ACTIVE;
	gpu_recs[0].id_index = 1u;
	gpu_recs[0].id_acquire_tag = 10u;
	gpu_recs[0].screen_importance_q16 = 4096u;
	gpu_recs[0].specular_scale = 1.0f;
	gpu_recs[0].diffuse_scale = 1.0f;

	const uint32_t meta_words[8] = {
		1u, 0u, 0u, 0u, 2u, uint32_t(RendererRD::ExternalOutputConfig::PARTICLE_LIGHT_64), 5u, 0u
	};
	Vector<uint8_t> meta_init;
	meta_init.resize(sizeof(meta_words));
	memcpy(meta_init.ptrw(), meta_words, sizeof(meta_words));

	Vector<uint8_t> recs_init;
	recs_init.resize(sizeof(gpu_recs));
	memcpy(recs_init.ptrw(), gpu_recs, sizeof(gpu_recs));

	const RID meta_rid = rd.storage_buffer_create(sizeof(meta_words), meta_init);
	const RID recs_rid = rd.storage_buffer_create(sizeof(gpu_recs), recs_init);
	const RID omni_buf = rd.storage_buffer_create(8u * 224u);
	rd.buffer_clear(omni_buf, 0, 8u * 224u);

	const RID vp_rid = RID::from_uint64(0x4444u);
	const RID scen_rid = RID::from_uint64(0xDDDDu);
	const RendererRD::ExternalOutputOwner *dummy_owner = reinterpret_cast<const RendererRD::ExternalOutputOwner *>(uintptr_t(0x5000));

	const AABB initial_bounds(Vector3(-5.0f, -5.0f, -5.0f), Vector3(10.0f, 10.0f, 10.0f));
	const AABB moved_bounds_single(Vector3(100.0f, 20.0f, -40.0f), Vector3(12.0f, 12.0f, 12.0f));
	const AABB moved_bounds_batch(Vector3(-250.0f, 5.0f, 80.0f), Vector3(16.0f, 16.0f, 16.0f));

	Lane::ExternalLightSlotRegistration reg;
	reg.owner_key = 0xAA01u;
	reg.owner_ptr = dummy_owner;
	reg.is_slot_live_fn = &mock_is_slot_live;
	reg.is_slot_visible_fn = &mock_is_slot_visible;
	reg.generation = 11u;
	reg.slot_index = 0u;
	reg.lease_serial = 3u;
	reg.viewport_rid = vp_rid;
	reg.scenario_rid = scen_rid;
	reg.world_bounds = initial_bounds;
	reg.meta_rid = meta_rid;
	reg.records_rid = recs_rid;
	reg.capacity = 2u;
	reg.present_seq = 5u;

	g_mock_slot_visible = true;
	lane.register_sealed_external_light_slot(reg);

	// 1. Single fast-refresh present_seq + world_bounds propagation:
	CHECK(lane.update_external_light_slot_present_seq(0xAA01u, 11u, 0u, 3u, 6u, &moved_bounds_single));
	LocalVector<Lane::ReservedGpuLightBounds> bounds;
	CHECK(lane.dispatch_registered_external_lights_gpu(&rd, vp_rid, scen_rid, false, Transform3D(), 1.0f, omni_buf, 0u, 8u, bounds) == 2u);
	REQUIRE(bounds.size() == 1u);
	CHECK(bounds[0].world_bounds.position.is_equal_approx(moved_bounds_single.position));
	CHECK(bounds[0].world_bounds.size.is_equal_approx(moved_bounds_single.size));

	// 2. Batch fast-refresh present_seq + world_bounds propagation:
	Lane::ExternalLightSlotPresentSeqUpdate batch_upd;
	batch_upd.owner_key = 0xAA01u;
	batch_upd.generation = 11u;
	batch_upd.slot_index = 0u;
	batch_upd.lease_serial = 3u;
	batch_upd.next_present_seq = 7u;
	batch_upd.world_bounds = moved_bounds_batch;
	batch_upd.has_world_bounds = true;
	CHECK(lane.update_external_light_slots_present_seq_batch(&batch_upd, 1u) == 1u);

	// Advance the GPU slot meta's present_seq to match fast-refresh seq 7 so the
	// harvest compute shader accepts the record and emits the scaled cluster element.
	const uint32_t meta_words_seq7[8] = {
		1u, 0u, 0u, 0u, 2u, uint32_t(RendererRD::ExternalOutputConfig::PARTICLE_LIGHT_64), 7u, 0u
	};
	REQUIRE(rd.buffer_update(meta_rid, 0u, sizeof(meta_words_seq7), meta_words_seq7) == OK);

	// 3. Non-unit camera scale (cluster_scale = 2.0): verify GPU cluster element
	// matches ClusterBuilderRD::add_light (un-orthonormalized cluster_pos, orthonormal
	// basis rows, r_elem = radius * cluster_scale * sphere_overfit, and non-squared r2).
	// world_pos = (3, -2, -5), view = scale(2) + origin(0, 0, -2) => cluster_pos = (6, -4, -12),
	// length_sq = 36 + 16 + 144 = 196.
	// r_elem = 4 * 2 * 1.05 = 8.4; correct r2 = 8.4 * 1.05 = 8.82 => r2^2 = 77.79 (< 196 => touches_near = 0).
	// Buggy squared r2 would be 8.4 * 2.1 = 17.64 => r2^2 = 311.17 (> 196 => falsely touches_near = 1!).
	const Transform3D scaled_view(Basis::from_scale(Vector3(2.0f, 2.0f, 2.0f)), Vector3(0.0f, 0.0f, -2.0f));
	CHECK(lane.dispatch_registered_external_lights_gpu(&rd, vp_rid, scen_rid, false, scaled_view, 1.0f, omni_buf, 0u, 8u, bounds) == 2u);
	REQUIRE(bounds.size() == 1u);
	CHECK(bounds[0].world_bounds.position.is_equal_approx(moved_bounds_batch.position));
	CHECK(bounds[0].world_bounds.size.is_equal_approx(moved_bounds_batch.size));

	rd.submit();
	rd.sync();

	const RID elem_buf = lane.get_light_cluster_elements_buffer();
	REQUIRE(elem_buf.is_valid());
	const Vector<uint8_t> elem_bytes = rd.buffer_get_data(elem_buf, 0u, 20u * sizeof(uint32_t));
	REQUIRE(elem_bytes.size() == int(20u * sizeof(uint32_t)));
	uint32_t words[20] = {};
	memcpy(words, elem_bytes.ptr(), sizeof(words));
	auto as_f32 = [](uint32_t w) -> float {
		float f = 0.0f;
		memcpy(&f, &w, sizeof(f));
		return f;
	};
	CHECK(words[1] == 0u); // touches_near must be 0 (proves cluster_scale is NOT squared in r2).
	CHECK(Math::is_equal_approx(as_f32(words[4]), 1.0f)); // Orthonormalized basis row0.x.
	CHECK(Math::is_equal_approx(as_f32(words[9]), 1.0f)); // Orthonormalized basis row1.y.
	CHECK(Math::is_equal_approx(as_f32(words[14]), 1.0f)); // Orthonormalized basis row2.z.
	CHECK(Math::is_equal_approx(as_f32(words[7]), 6.0f)); // cluster_pos.x = 2*3 + 0.
	CHECK(Math::is_equal_approx(as_f32(words[11]), -4.0f)); // cluster_pos.y = 2*(-2) + 0.
	CHECK(Math::is_equal_approx(as_f32(words[15]), -12.0f)); // cluster_pos.z = 2*(-5) - 2.
	CHECK(Math::is_equal_approx(as_f32(words[16]), 8.4f)); // r_elem = 4 * 2 * 1.05.

	lane.unregister_external_light_owner(0xAA01u);
	lane.release_light_gpu_bridge(&rd);
	rd.free_rid(omni_buf);
	rd.free_rid(recs_rid);
	rd.free_rid(meta_rid);
}

TEST_CASE("[ExternalOutput][Block23] BUG-71 Metal compute PSO cache deduplicates identical compute shaders across distinct units") {
	REQUIRE(RenderingDevice::get_singleton() == nullptr);
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	static constexpr const char *kComputeGlslA =
			"#version 450\n"
			"layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;\n"
			"layout(set = 0, binding = 0, std430) buffer OutBuf { uint data[]; };\n"
			"void main() {\n"
			"    uint idx = gl_GlobalInvocationID.x;\n"
			"    data[idx] = idx * 3u + 7u;\n"
			"}\n";
	static constexpr const char *kComputeGlslDiff =
			"#version 450\n"
			"layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;\n"
			"layout(set = 0, binding = 0, std430) buffer OutBuf { uint data[]; };\n"
			"void main() {\n"
			"    uint idx = gl_GlobalInvocationID.x;\n"
			"    data[idx] = idx * 5u + 11u;\n"
			"}\n";

	auto compile_compute_bytecode = [&](const char *p_glsl) -> Vector<uint8_t> {
		String err;
		RD::ShaderStageSPIRVData cs;
		cs.shader_stage = RD::SHADER_STAGE_COMPUTE;
		cs.spirv = rd.shader_compile_spirv_from_source(RD::SHADER_STAGE_COMPUTE, String::utf8(p_glsl), RD::SHADER_LANGUAGE_GLSL, &err, false);
		REQUIRE(!cs.spirv.is_empty());
		Vector<RD::ShaderStageSPIRVData> stages;
		stages.push_back(cs);
		const Vector<uint8_t> bytecode = rd.shader_compile_binary_from_spirv(stages, String("bug71_compute"));
		REQUIRE(!bytecode.is_empty());
		return bytecode;
	};

	const Vector<uint8_t> bytecode_a = compile_compute_bytecode(kComputeGlslA);
	const Vector<uint8_t> bytecode_diff = compile_compute_bytecode(kComputeGlslDiff);

	const RID shader_a = rd.shader_create_from_bytecode(bytecode_a);
	const RID shader_b = rd.shader_create_from_bytecode(bytecode_a);
	REQUIRE(shader_a.is_valid());
	REQUIRE(shader_b.is_valid());
	CHECK(shader_a != shader_b);

	const uint64_t misses_before = RenderingDeviceDriverMetal::get_total_compute_pso_cache_misses();
	const uint64_t hits_before = RenderingDeviceDriverMetal::get_total_compute_pso_cache_hits();

	const RID pipe_a = rd.compute_pipeline_create(shader_a);
	REQUIRE(pipe_a.is_valid());
	CHECK(RenderingDeviceDriverMetal::get_total_compute_pso_cache_misses() == misses_before + 1u);
	CHECK(RenderingDeviceDriverMetal::get_total_compute_pso_cache_hits() == hits_before);

	// Distinct MDComputeShader unit with identical SPIR-V bytecode MUST hit compute_pso_cache
	// and reuse the exact same MTL::ComputePipelineState* pointer!
	const RID pipe_b = rd.compute_pipeline_create(shader_b);
	REQUIRE(pipe_b.is_valid());
	CHECK(RenderingDeviceDriverMetal::get_total_compute_pso_cache_misses() == misses_before + 1u);
	CHECK(RenderingDeviceDriverMetal::get_total_compute_pso_cache_hits() == hits_before + 1u);

	const uint64_t mtl_pso_a = rd.get_driver_resource(RD::DRIVER_RESOURCE_COMPUTE_PIPELINE, pipe_a, 0);
	const uint64_t mtl_pso_b = rd.get_driver_resource(RD::DRIVER_RESOURCE_COMPUTE_PIPELINE, pipe_b, 0);
	CHECK(mtl_pso_a != 0u);
	CHECK(mtl_pso_b == mtl_pso_a);

	// Freeing shader_a + pipe_a MUST NOT evict the content-addressed PSO entry or break pipe_b,
	// and recreating a third identical shader unit MUST still hit compute_pso_cache.
	rd.free_rid(pipe_a);
	rd.free_rid(shader_a);
	rd.submit();
	rd.sync();

	const RID shader_c = rd.shader_create_from_bytecode(bytecode_a);
	REQUIRE(shader_c.is_valid());
	const RID pipe_c = rd.compute_pipeline_create(shader_c);
	REQUIRE(pipe_c.is_valid());
	CHECK(RenderingDeviceDriverMetal::get_total_compute_pso_cache_misses() == misses_before + 1u);
	CHECK(RenderingDeviceDriverMetal::get_total_compute_pso_cache_hits() == hits_before + 2u);
	CHECK(rd.get_driver_resource(RD::DRIVER_RESOURCE_COMPUTE_PIPELINE, pipe_c, 0) == mtl_pso_a);

	// A different compute shader MUST miss and produce a distinct MTL::ComputePipelineState*.
	const RID shader_diff = rd.shader_create_from_bytecode(bytecode_diff);
	REQUIRE(shader_diff.is_valid());
	const RID pipe_diff = rd.compute_pipeline_create(shader_diff);
	REQUIRE(pipe_diff.is_valid());
	CHECK(RenderingDeviceDriverMetal::get_total_compute_pso_cache_misses() == misses_before + 2u);
	CHECK(RenderingDeviceDriverMetal::get_total_compute_pso_cache_hits() == hits_before + 2u);
	const uint64_t mtl_pso_diff = rd.get_driver_resource(RD::DRIVER_RESOURCE_COMPUTE_PIPELINE, pipe_diff, 0);
	CHECK(mtl_pso_diff != 0u);
	CHECK(mtl_pso_diff != mtl_pso_a);

	rd.free_rid(pipe_diff);
	rd.free_rid(shader_diff);
	rd.free_rid(pipe_c);
	rd.free_rid(shader_c);
	rd.free_rid(pipe_b);
	rd.free_rid(shader_b);
	rd.submit();
	rd.sync();
}

TEST_CASE("[ExternalOutput][Metal][BUG-82][BUG-83][BUG-86] binary archive bounds warmup memory/churn and heap allocator never pins dedicated or spill blocks") {
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);

	RenderingDeviceDriver *driver_base = context.driver_create();
	REQUIRE(driver_base != nullptr);
	auto *metal_driver = static_cast<RenderingDeviceDriverMetal *>(driver_base);
	REQUIRE(metal_driver->initialize(0, 2) == OK);

	// BUG-82 + BUG-86: Binary archive MUST release under continuous per-frame PSO churn
	// at ARCHIVE_WARMUP_MAX_FRAMES (90), release after ARCHIVE_WARMUP_IDLE_FRAMES (30)
	// when idle, and cap additions at ARCHIVE_WARMUP_MAX_PIPELINES (64) / compute (16).
	REQUIRE(metal_driver->pipeline_cache_create(Vector<uint8_t>()));
	CHECK(metal_driver->is_pipeline_archive_active());
	for (uint32_t f = 1; f < 90; ++f) {
		metal_driver->test_step_pipeline_archive_frame(f, true);
		CHECK(metal_driver->is_pipeline_archive_active());
	}
	metal_driver->test_step_pipeline_archive_frame(90, true);
	CHECK_FALSE(metal_driver->is_pipeline_archive_active());

	// Reopen and verify idle release after 30 idle frames (with p_frames_drawn >= 30).
	REQUIRE(metal_driver->pipeline_cache_create(Vector<uint8_t>()));
	CHECK(metal_driver->is_pipeline_archive_active());
	for (uint32_t f = 1; f < 30; ++f) {
		metal_driver->test_step_pipeline_archive_frame(f, false);
		CHECK(metal_driver->is_pipeline_archive_active());
	}
	metal_driver->test_step_pipeline_archive_frame(30, false);
	CHECK_FALSE(metal_driver->is_pipeline_archive_active());

	// Reopen and verify compute additions cap at 16 and total additions cap at 64,
	// triggering immediate archive release on the very next frame!
	REQUIRE(metal_driver->pipeline_cache_create(Vector<uint8_t>()));
	CHECK(metal_driver->is_pipeline_archive_active());
	for (uint32_t i = 0; i < 16; ++i) {
		CHECK(metal_driver->test_record_pipeline_archive_addition(true));
	}
	CHECK_FALSE(metal_driver->test_record_pipeline_archive_addition(true)); // Compute cap reached!
	for (uint32_t i = 16; i < 64; ++i) {
		CHECK(metal_driver->test_record_pipeline_archive_addition(false));
	}
	CHECK_FALSE(metal_driver->test_record_pipeline_archive_addition(false)); // Total cap reached!
	metal_driver->test_step_pipeline_archive_frame(1, true);
	CHECK_FALSE(metal_driver->is_pipeline_archive_active());

	// BUG-155B: Under default RELEASE_AFTER_WARMUP mode, calling pipeline_cache_serialize()
	// (invoked synchronously by RenderingDevice::finalize() -> update_pipeline_cache(true))
	// MUST skip MTLBinaryArchive::serializeToURL() so shutdown never triggers a +1.2 GB RSS spike.
	REQUIRE(metal_driver->pipeline_cache_create(Vector<uint8_t>()));
	CHECK(metal_driver->is_pipeline_archive_active());
	CHECK(metal_driver->test_record_pipeline_archive_addition(true));
	CHECK(metal_driver->test_record_pipeline_archive_addition(false));
	CHECK(metal_driver->pipeline_cache_query_size() == 3u * 1024u);
	const uint64_t serialize_calls_before = metal_driver->test_get_pipeline_archive_serialize_calls();
	const Vector<uint8_t> serialized_blob = metal_driver->pipeline_cache_serialize();
	CHECK(serialized_blob.is_empty());
	CHECK(metal_driver->test_get_pipeline_archive_serialize_calls() == serialize_calls_before);
	metal_driver->pipeline_cache_free();
	CHECK_FALSE(metal_driver->is_pipeline_archive_active());
	CHECK(metal_driver->pipeline_cache_query_size() == 1024u);

	// BUG-83.2: MetalHeapAllocator must NEVER recycle Kind::DEDICATED buffers,
	// and must evict recycled buffers from spill POOL blocks so extra placement heaps
	// are reclaimed rather than pinned.
	{
		MetalHeapAllocator heap_alloc(metal_driver->get_device());
		const MTL::ResourceOptions shared_opts = MTL::ResourceStorageModeShared;

		// 10 MiB > POOL_SHARED preferred block/2 (8 MiB), <= max_recycled_bytes (16 MiB):
		// lands in DEDICATED and MUST be freed immediately (dedicated_count -> 0).
		MetalBuffer dedicated = heap_alloc.new_buffer(10u * 1024u * 1024u, shared_opts);
		REQUIRE(dedicated.buffer.get() != nullptr);
		CHECK(dedicated.allocation.kind == MetalAllocation::Kind::DEDICATED);
		MetalAllocatorStats stats = {};
		heap_alloc.get_stats(stats);
		CHECK(stats.pools[MetalHeapAllocator::POOL_SHARED].dedicated_count == 1u);
		heap_alloc.free_buffer(dedicated);
		heap_alloc.get_stats(stats);
		CHECK(stats.pools[MetalHeapAllocator::POOL_SHARED].dedicated_count == 0u);
		CHECK(stats.pools[MetalHeapAllocator::POOL_SHARED].dedicated_bytes == 0u);

		// Spill POOL_SHARED (16 MiB blocks) across 3 blocks using 3x 7 MiB POOL buffers
		// (each <= 8 MiB -> Kind::POOL, two fit per block -> 2 blocks; add 3rd -> 2 blocks,
		// 5x 7 MiB -> 3 blocks). Freeing the 3rd block's sole buffer MUST NOT leave it
		// pinned in recycled_buffers once there are > 1 blocks + empty block!
		MetalBuffer b0 = heap_alloc.new_buffer(7u * 1024u * 1024u, shared_opts);
		MetalBuffer b1 = heap_alloc.new_buffer(7u * 1024u * 1024u, shared_opts);
		MetalBuffer b2 = heap_alloc.new_buffer(7u * 1024u * 1024u, shared_opts);
		MetalBuffer b3 = heap_alloc.new_buffer(7u * 1024u * 1024u, shared_opts);
		MetalBuffer b4 = heap_alloc.new_buffer(7u * 1024u * 1024u, shared_opts);
		REQUIRE(b4.buffer.get() != nullptr);
		CHECK(b4.allocation.kind == MetalAllocation::Kind::POOL);
		heap_alloc.get_stats(stats);
		CHECK(stats.pools[MetalHeapAllocator::POOL_SHARED].block_count == 3u);

		// Free b2, b3, b4 (emptying blocks 1 and 2) -> spill block 2 MUST be reclaimed
		// (block_count drops from 3 to 2, keeping at most 1 empty cached block!).
		heap_alloc.free_buffer(b2);
		heap_alloc.free_buffer(b3);
		heap_alloc.free_buffer(b4);
		heap_alloc.get_stats(stats);
		CHECK(stats.pools[MetalHeapAllocator::POOL_SHARED].block_count == 2u);

		// Small POOL buffer in block 0 still recycles cleanly.
		const uint64_t hits_before = heap_alloc.get_buffer_recycle_hits();
		MetalBuffer small_a = heap_alloc.new_buffer(4096u, shared_opts);
		REQUIRE(small_a.buffer.get() != nullptr);
		heap_alloc.free_buffer(small_a);
		MetalBuffer small_b = heap_alloc.new_buffer(4096u, shared_opts);
		REQUIRE(small_b.buffer.get() != nullptr);
		CHECK(heap_alloc.get_buffer_recycle_hits() == hits_before + 1u);

		heap_alloc.free_buffer(small_b);
		heap_alloc.free_buffer(b1);
		heap_alloc.free_buffer(b0);
	}

	// BUG-83.3: fence_free cleanly drains unrecycled fences with zero crash.
	const RDD::FenceID fence = metal_driver->fence_create();
	REQUIRE(fence.id != 0);
	metal_driver->fence_free(fence);

	metal_driver->pipeline_cache_free();
	context.driver_free(driver_base);
}

TEST_CASE("[ExternalOutput][Metal][BUG-175B] compute_pso_cache evicts entries in O(k) when the last shader owning an MDLibrary is freed") {
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);
	struct FinalizeGuard {
		RenderingDevice &rd;
		~FinalizeGuard() { rd.finalize(); }
	} guard{ rd };

	const char *kComputeGlslA = R"(#version 450
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(set = 0, binding = 0, std430) buffer Buf { uint v[]; };
void main() { v[0] += 11u; }
)";
	const char *kComputeGlslB = R"(#version 450
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(set = 0, binding = 0, std430) buffer Buf { uint v[]; };
void main() { v[0] += 23u; }
)";

	auto compile_compute_bytecode = [&](const char *p_src, const char *p_name) -> Vector<uint8_t> {
		RD::ShaderStageSPIRVData cs;
		cs.shader_stage = RD::SHADER_STAGE_COMPUTE;
		String err;
		cs.spirv = rd.shader_compile_spirv_from_source(RD::SHADER_STAGE_COMPUTE, String(p_src), RD::SHADER_LANGUAGE_GLSL, &err);
		REQUIRE(err.is_empty());
		REQUIRE(!cs.spirv.is_empty());
		Vector<RD::ShaderStageSPIRVData> stages;
		stages.push_back(cs);
		const Vector<uint8_t> bytecode = rd.shader_compile_binary_from_spirv(stages, String(p_name));
		REQUIRE(!bytecode.is_empty());
		return bytecode;
	};

	const Vector<uint8_t> bytecode_a = compile_compute_bytecode(kComputeGlslA, "bug175b_a");
	const Vector<uint8_t> bytecode_b = compile_compute_bytecode(kComputeGlslB, "bug175b_b");

	const uint64_t misses_before = RenderingDeviceDriverMetal::get_total_compute_pso_cache_misses();
	const uint64_t evictions_before = RenderingDeviceDriverMetal::get_total_compute_pso_cache_evictions();

	const RID shader_a1 = rd.shader_create_from_bytecode(bytecode_a);
	const RID shader_a2 = rd.shader_create_from_bytecode(bytecode_a);
	const RID shader_b = rd.shader_create_from_bytecode(bytecode_b);
	REQUIRE(shader_a1.is_valid());
	REQUIRE(shader_a2.is_valid());
	REQUIRE(shader_b.is_valid());

	const RID pipe_a1 = rd.compute_pipeline_create(shader_a1);
	const RID pipe_a2 = rd.compute_pipeline_create(shader_a2);
	const RID pipe_b = rd.compute_pipeline_create(shader_b);
	REQUIRE(pipe_a1.is_valid());
	REQUIRE(pipe_a2.is_valid());
	REQUIRE(pipe_b.is_valid());
	CHECK(RenderingDeviceDriverMetal::get_total_compute_pso_cache_misses() == misses_before + 2u);
	CHECK(RenderingDeviceDriverMetal::get_total_compute_pso_cache_evictions() == evictions_before);

	// Freeing shader_a1 while shader_a2 still owns the shared MDLibrary keeps its cached PSO alive.
	rd.free_rid(pipe_a1);
	rd.free_rid(shader_a1);
	rd.submit();
	rd.sync();
	CHECK(RenderingDeviceDriverMetal::get_total_compute_pso_cache_evictions() == evictions_before);

	// Freeing shader_b (sole owner of bytecode_b's MDLibrary) immediately evicts its compute PSO entry in O(k).
	rd.free_rid(pipe_b);
	rd.free_rid(shader_b);
	rd.submit();
	rd.sync();
	CHECK(RenderingDeviceDriverMetal::get_total_compute_pso_cache_evictions() == evictions_before + 1u);

	// Recreating shader_b2 + pipe_b2 MUST miss compute_pso_cache, proving genuine eviction.
	const RID shader_b2 = rd.shader_create_from_bytecode(bytecode_b);
	REQUIRE(shader_b2.is_valid());
	const RID pipe_b2 = rd.compute_pipeline_create(shader_b2);
	REQUIRE(pipe_b2.is_valid());
	CHECK(RenderingDeviceDriverMetal::get_total_compute_pso_cache_misses() == misses_before + 3u);

	// Freeing all remaining shaders immediately evicts both remaining entries.
	rd.free_rid(pipe_b2);
	rd.free_rid(shader_b2);
	rd.free_rid(pipe_a2);
	rd.free_rid(shader_a2);
	rd.submit();
	rd.sync();
	CHECK(RenderingDeviceDriverMetal::get_total_compute_pso_cache_evictions() == evictions_before + 3u);
}
#endif

} // namespace TestExternalOutputBlock23
