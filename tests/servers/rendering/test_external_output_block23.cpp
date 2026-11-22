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
#include "drivers/metal/rendering_context_driver_metal.h"
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

	Vector<uint8_t> zero_init;
	zero_init.resize(16);
	memset(zero_init.ptrw(), 0, 16);
	const RID ssbo = rd.storage_buffer_create(16u, zero_init);
	REQUIRE(ssbo.is_valid());
	Vector<uint8_t> indirect_init;
	indirect_init.resize(16);
	memset(indirect_init.ptrw(), 0, 16);
	uint32_t *ind = reinterpret_cast<uint32_t *>(indirect_init.ptrw());
	ind[0] = 1u;
	ind[1] = 1u;
	ind[2] = 1u;
	const RID indirect_buf = rd.storage_buffer_create(16u, indirect_init, RD::STORAGE_BUFFER_USAGE_DISPATCH_INDIRECT);
	REQUIRE(indirect_buf.is_valid());

	Vector<RD::Uniform> uniforms;
	RD::Uniform u;
	u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
	u.binding = 0;
	u.append_id(ssbo);
	uniforms.push_back(u);
	const RID uniform_set = rd.uniform_set_create(uniforms, shader, 0);
	REQUIRE(uniform_set.is_valid());

	// Case 1: writable SSBO held immutable causes compute_list_bind_uniform_set to fail;
	// subsequent dispatch / dispatch_indirect must return cleanly without dereferencing nullptr.
	Vector<RID> roots;
	roots.push_back(ssbo);
	RD::NativeImmutableHold hold;
	REQUIRE(rd.native_immutable_hold_acquire(roots, hold, 8u) == OK);
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
#endif

} // namespace TestExternalOutputBlock23
