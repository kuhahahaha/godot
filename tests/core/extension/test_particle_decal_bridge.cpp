/**************************************************************************/
/*  test_particle_decal_bridge.cpp                                        */
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

#include "core/extension/gdextension_interface.gen.h"
#include "core/math/quaternion.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_config.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_decal_bridge_rd.h"
#include "servers/rendering/rendering_device.h"
#include "tests/test_macros.h"

#include <cmath>
#include <cstring>
#include <limits>

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
#include "drivers/metal/rendering_context_driver_metal.h"
#endif

TEST_FORCE_LINK(test_particle_decal_bridge)

namespace TestParticleDecalBridge {

static PackedByteArray make_tag32(uint8_t p_seed) {
	PackedByteArray out;
	out.resize(32);
	for (int i = 0; i < 32; ++i) {
		out.write[i] = uint8_t(p_seed + i);
	}
	return out;
}

static Dictionary make_valid_decal_config_dict(uint32_t p_capacity = 32u) {
	Dictionary d;
	d["schema"] = int64_t(1);
	d["profile"] = int64_t(RendererRD::ExternalOutputConfig::PARTICLE_DECAL_128);
	d["capacity"] = int64_t(p_capacity);
	d["max_views"] = int64_t(2);
	d["max_inflight"] = int64_t(2);
	d["space"] = int64_t(RendererRD::ExternalOutputConfig::WORLD);
	d["pass_mask"] = int64_t(0);
	d["motion"] = int64_t(RendererRD::ExternalOutputConfig::DISABLED);
	d["surfaces"] = Array();
	Array tex_rids;
	tex_rids.push_back(RID::from_uint64(0x101u));
	tex_rids.push_back(RID());
	tex_rids.push_back(RID());
	tex_rids.push_back(RID::from_uint64(0x104u));
	d["surface_materials"] = tex_rids;
	d["bounds"] = AABB(Vector3(-10.0f, -10.0f, -10.0f), Vector3(20.0f, 20.0f, 20.0f));
	d["producer_tag"] = make_tag32(1);
	d["device_tag"] = make_tag32(2);
	d["layout_digest"] = make_tag32(3);
	d["mapping_digest"] = make_tag32(4);
	d["numeric_profile_digest"] = make_tag32(5);
	return d;
}

TEST_CASE("[particle_decal_bridge] ABI constants, config parsing, and deterministic CPU Top-K compaction") {
	using Bridge = RendererRD::ParticlesDecalBridgeRD;
	using DecalRec = RendererRD::ParticleDecalRecord128;

	CHECK(uint32_t(GDEXTENSION_OUTPUT_ROLE_PARTICLE_DECALS_V2) == 10u);
	CHECK(uint32_t(GDEXTENSION_OUTPUT_PROFILE_PARTICLE_DECAL_128_V2) == 5u);
	CHECK(uint32_t(RendererRD::ExternalOutputConfig::PARTICLE_DECAL_128) == 5u);
	CHECK(sizeof(DecalRec) == 128u);
	CHECK(alignof(DecalRec) == 16u);

	// Config parse validation:
	{
		RendererRD::ExternalOutputConfig parsed;
		StringName detail;
		Dictionary cfg = make_valid_decal_config_dict(64u);
		REQUIRE(RendererRD::ExternalOutputConfig::parse(cfg, parsed, detail) == OK);
		CHECK(parsed.profile == RendererRD::ExternalOutputConfig::PARTICLE_DECAL_128);
		CHECK(parsed.capacity == 64u);
		CHECK(parsed.surfaces.is_empty());
		CHECK(parsed.surface_materials.size() == 4);

		// Reject capacity > 256:
		Dictionary too_many = make_valid_decal_config_dict(257u);
		CHECK(RendererRD::ExternalOutputConfig::parse(too_many, parsed, detail) == ERR_INVALID_PARAMETER);
		CHECK(detail == StringName("LIMIT"));

		// Reject non-4 surface_materials span:
		Dictionary bad_span = make_valid_decal_config_dict(16u);
		Array short_arr;
		short_arr.push_back(RID::from_uint64(0x101u));
		bad_span["surface_materials"] = short_arr;
		CHECK(RendererRD::ExternalOutputConfig::parse(bad_span, parsed, detail) == ERR_INVALID_PARAMETER);

		// Reject non-empty surfaces, non-zero pass_mask, or REQUIRED motion:
		Dictionary bad_surf = make_valid_decal_config_dict(16u);
		Array surf_arr;
		surf_arr.push_back(RID::from_uint64(0x201u));
		bad_surf["surfaces"] = surf_arr;
		CHECK(RendererRD::ExternalOutputConfig::parse(bad_surf, parsed, detail) == ERR_INVALID_PARAMETER);

		Dictionary bad_pass = make_valid_decal_config_dict(16u);
		bad_pass["pass_mask"] = int64_t(1);
		CHECK(RendererRD::ExternalOutputConfig::parse(bad_pass, parsed, detail) == ERR_INVALID_PARAMETER);

		Dictionary bad_motion = make_valid_decal_config_dict(16u);
		bad_motion["motion"] = int64_t(RendererRD::ExternalOutputConfig::REQUIRED);
		CHECK(RendererRD::ExternalOutputConfig::parse(bad_motion, parsed, detail) == ERR_INVALID_PARAMETER);
	}

	// Deterministic CPU Top-K compaction & malformed candidate rejection:
	{
		LocalVector<DecalRec> candidates;
		auto make_rec = [](uint32_t p_idx, uint32_t p_tag, uint32_t p_imp) {
			DecalRec r;
			r.world_position[0] = float(p_idx);
			r.id_index = p_idx;
			r.id_acquire_tag = p_tag;
			r.screen_importance_q16 = p_imp;
			r.flags = DecalRec::FLAG_ACTIVE;
			return r;
		};

		candidates.push_back(make_rec(50u, 10u, 30000u)); // valid #3
		candidates.push_back(make_rec(12u, 4u, 30000u)); // valid #2 (lower tag wins tie)
		candidates.push_back(make_rec(8u, 4u, 30000u)); // valid #1 (lower id_index wins tie)
		candidates.push_back(make_rec(99u, 20u, 50000u)); // valid #0 (highest importance)

		// Invalid records that MUST be dropped:
		DecalRec bad_res = make_rec(1u, 1u, 65000u);
		bad_res.reserved0 = 1u;
		candidates.push_back(bad_res);

		DecalRec bad_nan = make_rec(2u, 1u, 65000u);
		bad_nan.world_position[1] = std::numeric_limits<float>::quiet_NaN();
		candidates.push_back(bad_nan);

		DecalRec bad_size = make_rec(3u, 1u, 65000u);
		bad_size.size_world[2] = 0.0f;
		candidates.push_back(bad_size);

		DecalRec bad_quat = make_rec(4u, 1u, 65000u);
		bad_quat.orientation_quat[0] = bad_quat.orientation_quat[1] = bad_quat.orientation_quat[2] = bad_quat.orientation_quat[3] = 0.0f;
		candidates.push_back(bad_quat);

		DecalRec bad_flags = make_rec(5u, 1u, 65000u);
		bad_flags.flags = 0x80u;
		candidates.push_back(bad_flags);

		DecalRec bad_subuv = make_rec(6u, 1u, 65000u);
		bad_subuv.flags = DecalRec::FLAG_ACTIVE | DecalRec::FLAG_HAS_SUBUV;
		bad_subuv.uv_subrect[2] = 0.0f;
		candidates.push_back(bad_subuv);

		LocalVector<DecalRec> selected;
		REQUIRE(Bridge::compact_particle_decals_deterministic(candidates, 3u, selected) == OK);
		REQUIRE(selected.size() == 3u);
		CHECK(selected[0].id_index == 99u);
		CHECK(selected[1].id_index == 8u);
		CHECK(selected[2].id_index == 12u);
	}
}

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
TEST_CASE("[particle_decal_bridge][LiveMetal] GPU harvest + analytic DecalData inverse + SubUV + ClusterBuilderRD box + isolation") {
	using Bridge = RendererRD::ParticlesDecalBridgeRD;
	using DecalRec = RendererRD::ParticleDecalRecord128;

	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	Bridge bridge;

	// Build a rotated + translated camera and a rotated + non-uniformly scaled decal to verify exact analytic math:
	const Quaternion cam_q = Quaternion(Vector3(0.0f, 1.0f, 0.0f), 0.45f) * Quaternion(Vector3(1.0f, 0.0f, 0.0f), -0.25f);
	const Transform3D cam_xform(Basis(cam_q.normalized()), Vector3(3.0f, 5.0f, 12.0f));
	const Transform3D cam_inv = cam_xform.affine_inverse();

	const Quaternion decal_q = (Quaternion(Vector3(0.0f, 0.0f, 1.0f), 0.35f) * Quaternion(Vector3(1.0f, 0.0f, 0.0f), 0.6f)).normalized();
	const Vector3 decal_pos(1.5f, 2.0f, -4.0f);
	const Vector3 decal_size(4.0f, 2.5f, 6.0f);

	DecalRec recs[4] = {};
	// Candidate 0: valid decal with SubUV & rotation & non-uniform size (highest importance = 50000)
	recs[0].world_position[0] = float(decal_pos.x);
	recs[0].world_position[1] = float(decal_pos.y);
	recs[0].world_position[2] = float(decal_pos.z);
	recs[0].albedo_mix = 0.75f;
	recs[0].orientation_quat[0] = float(decal_q.x);
	recs[0].orientation_quat[1] = float(decal_q.y);
	recs[0].orientation_quat[2] = float(decal_q.z);
	recs[0].orientation_quat[3] = float(decal_q.w);
	recs[0].size_world[0] = float(decal_size.x);
	recs[0].size_world[1] = float(decal_size.y);
	recs[0].size_world[2] = float(decal_size.z);
	recs[0].emission_energy = 2.5f;
	recs[0].color_rgba[0] = 0.8f;
	recs[0].color_rgba[1] = 0.6f;
	recs[0].color_rgba[2] = 0.4f;
	recs[0].color_rgba[3] = 0.9f;
	recs[0].upper_fade = 0.4f;
	recs[0].lower_fade = 0.2f;
	recs[0].normal_fade = 0.5f;
	recs[0].cull_mask = 0x00012345u;
	recs[0].id_index = 10u;
	recs[0].id_acquire_tag = 2u;
	recs[0].flags = DecalRec::FLAG_ACTIVE | DecalRec::FLAG_HAS_SUBUV;
	recs[0].screen_importance_q16 = 50000u;
	recs[0].uv_subrect[0] = 0.5f;
	recs[0].uv_subrect[1] = 0.25f;
	recs[0].uv_subrect[2] = 0.5f;
	recs[0].uv_subrect[3] = 0.5f;
	recs[0].distance_fade_begin = 0.0f;
	recs[0].distance_fade_length = 0.0f;

	// Candidate 1: non-zero reserved0 -> rejected on GPU
	recs[1] = recs[0];
	recs[1].id_index = 11u;
	recs[1].screen_importance_q16 = 60000u;
	recs[1].reserved0 = 7u;

	// Candidate 2: valid decal without SubUV, lower importance (20000) -> rank 1
	recs[2] = recs[0];
	recs[2].world_position[0] = -2.0f;
	recs[2].world_position[1] = 1.0f;
	recs[2].world_position[2] = -3.0f;
	recs[2].orientation_quat[0] = 0.0f;
	recs[2].orientation_quat[1] = 0.0f;
	recs[2].orientation_quat[2] = 0.0f;
	recs[2].orientation_quat[3] = 1.0f;
	recs[2].size_world[0] = 2.0f;
	recs[2].size_world[1] = 2.0f;
	recs[2].size_world[2] = 2.0f;
	recs[2].id_index = 12u;
	recs[2].id_acquire_tag = 3u;
	recs[2].flags = DecalRec::FLAG_ACTIVE;
	recs[2].screen_importance_q16 = 20000u;

	const uint32_t meta_words[8] = {
		3u, 0u, 0u, 0u, 4u, uint32_t(RendererRD::ExternalOutputConfig::PARTICLE_DECAL_128), 11u, 0u
	};
	Vector<uint8_t> meta_init;
	meta_init.resize(32);
	std::memcpy(meta_init.ptrw(), meta_words, 32);
	Vector<uint8_t> recs_init;
	recs_init.resize(sizeof(recs));
	std::memcpy(recs_init.ptrw(), recs, sizeof(recs));

	const RID meta_rid = rd.storage_buffer_create(32u, meta_init);
	const RID recs_rid = rd.storage_buffer_create(sizeof(recs), recs_init);
	const RID decal_buf = rd.storage_buffer_create(8u * 240u);
	const RID elem_buf = rd.storage_buffer_create(8u * 80u);
	rd.buffer_clear(decal_buf, 0, 8u * 240u);
	rd.buffer_clear(elem_buf, 0, 8u * 80u);

	const RID tex_albedo = RID::from_uint64(0xA100u);
	const RID tex_normal = RID::from_uint64(0xA200u);
	const RID tex_orm = RID::from_uint64(0xA300u);
	const RID tex_emission = RID::from_uint64(0xA400u);

	auto atlas_lookup = [](void *, RID p_tex) -> Rect2 {
		if (p_tex.get_id() == 0xA100u) {
			return Rect2(0.1f, 0.2f, 0.4f, 0.4f);
		}
		if (p_tex.get_id() == 0xA200u) {
			return Rect2(0.5f, 0.0f, 0.25f, 0.5f);
		}
		if (p_tex.get_id() == 0xA400u) {
			return Rect2(0.0f, 0.5f, 0.5f, 0.25f);
		}
		return Rect2();
	};

	const RID vp_a = RID::from_uint64(0x1111u);
	const RID vp_b = RID::from_uint64(0x2222u);
	const RID scen_a = RID::from_uint64(0xAAAAu);
	const RID scen_b = RID::from_uint64(0xBBBBu);

	Bridge::ExternalDecalSlotRegistration reg;
	reg.owner_key = 0xD001u;
	reg.generation = 1u;
	reg.slot_index = 0u;
	reg.lease_serial = 1u;
	reg.viewport_rid = vp_a;
	reg.scenario_rid = scen_a;
	reg.world_bounds = AABB(Vector3(-8.0f, -8.0f, -8.0f), Vector3(16.0f, 16.0f, 16.0f));
	reg.meta_rid = meta_rid;
	reg.records_rid = recs_rid;
	reg.textures[0] = tex_albedo;
	reg.textures[1] = tex_normal;
	reg.textures[2] = tex_orm;
	reg.textures[3] = tex_emission;
	reg.capacity = 4u;
	reg.present_seq = 11u;
	bridge.register_sealed_external_decal_slot(reg);

	LocalVector<Bridge::ReservedGpuDecalBounds> mob_bounds;
	// Isolation checks:
	CHECK(bridge.dispatch_registered_external_decals_gpu(&rd, vp_b, scen_a, false, cam_xform, decal_buf, 1u, 8u, atlas_lookup, nullptr, mob_bounds) == 0u);
	CHECK(bridge.dispatch_registered_external_decals_gpu(&rd, vp_a, scen_b, false, cam_xform, decal_buf, 1u, 8u, atlas_lookup, nullptr, mob_bounds) == 0u);
	CHECK(bridge.dispatch_registered_external_decals_gpu(&rd, vp_a, scen_a, true, cam_xform, decal_buf, 1u, 8u, atlas_lookup, nullptr, mob_bounds) == 0u);

	// Real dispatch at base_decal_index = 1 with cluster builder frame:
	const ClusterBuilderRD *dummy_builder = reinterpret_cast<const ClusterBuilderRD *>(uintptr_t(0x2000));
	bridge.begin_cluster_builder_frame(dummy_builder, cam_inv, 0.05f, 4000.0f, false, true, 32u);
	const uint32_t reserved = bridge.dispatch_registered_external_decals_gpu(&rd, vp_a, scen_a, false, cam_xform, decal_buf, 1u, 8u, atlas_lookup, nullptr, mob_bounds);
	REQUIRE(reserved == 4u);
	CHECK(bridge.get_last_dispatch_barrier_count() == 1u);
	REQUIRE(mob_bounds.size() == 1u);
	CHECK(mob_bounds[0].first_buffer_index == 1u);
	CHECK(mob_bounds[0].count == 4u);
	CHECK(bridge.get_pending_cluster_gpu_decal_count(dummy_builder) == 4u);
	bridge.blit_cluster_gpu_decal_elements(&rd, dummy_builder, elem_buf, 2u, 4u);
	bridge.clear_cluster_builder_frame(dummy_builder);

	// Verify exact CPU-reference DecalData vs GPU-written DecalData at slot 1 (rank 0 = Candidate 0):
	const Vector<uint8_t> decal_rb = rd.buffer_get_data(decal_buf);
	REQUIRE(decal_rb.size() == 8 * 240);
	const float *df = reinterpret_cast<const float *>(decal_rb.ptr());
	const uint32_t *du = reinterpret_cast<const uint32_t *>(decal_rb.ptr());

	Transform3D uv_xform;
	uv_xform.basis.scale(Vector3(2.0f, 1.0f, 2.0f));
	uv_xform.origin = Vector3(-1.0f, 0.0f, -1.0f);
	Transform3D scale_xform;
	scale_xform.basis.scale(decal_size * 0.5f);
	Transform3D decal_xform(Basis(decal_q), decal_pos);
	Transform3D expected_to_decal = (cam_inv * decal_xform * scale_xform * uv_xform).affine_inverse();
	float expected_xform[16] = {};
	RendererRD::MaterialStorage::store_transform(expected_to_decal, expected_xform);

	for (uint32_t k = 0; k < 16u; ++k) {
		CHECK(Math::is_equal_approx(df[1 * 60 + k], expected_xform[k], 1e-4f));
	}

	// Verify albedo_mix & SubUV-composed atlas rects:
	CHECK(Math::is_equal_approx(df[1 * 60 + 19], 0.75f));
	// albedo base (0.1, 0.2, 0.4, 0.4) * sub (0.5, 0.25, 0.5, 0.5) => (0.3, 0.3, 0.2, 0.2)
	CHECK(Math::is_equal_approx(df[1 * 60 + 20], 0.3f, 1e-5f));
	CHECK(Math::is_equal_approx(df[1 * 60 + 21], 0.3f, 1e-5f));
	CHECK(Math::is_equal_approx(df[1 * 60 + 22], 0.2f, 1e-5f));
	CHECK(Math::is_equal_approx(df[1 * 60 + 23], 0.2f, 1e-5f));
	// orm absent => (0,0,0,0)
	CHECK(df[1 * 60 + 28] == 0.0f);
	CHECK(df[1 * 60 + 30] == 0.0f);
	// emission base (0.0, 0.5, 0.5, 0.25) * sub (0.5, 0.25, 0.5, 0.5) => (0.25, 0.5625, 0.25, 0.125)
	CHECK(Math::is_equal_approx(df[1 * 60 + 32], 0.25f, 1e-5f));
	CHECK(Math::is_equal_approx(df[1 * 60 + 33], 0.5625f, 1e-5f));
	CHECK(Math::is_equal_approx(df[1 * 60 + 34], 0.25f, 1e-5f));
	CHECK(Math::is_equal_approx(df[1 * 60 + 35], 0.125f, 1e-5f));
	// modulate, emission_energy, mask, fades:
	CHECK(Math::is_equal_approx(df[1 * 60 + 36], 0.8f));
	CHECK(Math::is_equal_approx(df[1 * 60 + 39], 0.9f));
	CHECK(Math::is_equal_approx(df[1 * 60 + 40], 2.5f));
	CHECK(du[1 * 60 + 41] == 0x00012345u);
	CHECK(Math::is_equal_approx(df[1 * 60 + 42], 0.4f));
	CHECK(Math::is_equal_approx(df[1 * 60 + 43], 0.2f));

	// Verify normal_xform & normal:
	Basis expected_nx = cam_inv.basis * Basis(decal_q);
	float expected_nx_arr[12] = {};
	RendererRD::MaterialStorage::store_basis_3x4(expected_nx, expected_nx_arr);
	for (uint32_t k = 0; k < 12u; ++k) {
		CHECK(Math::is_equal_approx(df[1 * 60 + 44 + k], expected_nx_arr[k], 1e-4f));
	}
	const Vector3 expected_n = cam_inv.basis.xform(Basis(decal_q).get_column(Vector3::AXIS_Y).normalized());
	CHECK(Math::is_equal_approx(df[1 * 60 + 56], float(expected_n.x), 1e-4f));
	CHECK(Math::is_equal_approx(df[1 * 60 + 57], float(expected_n.y), 1e-4f));
	CHECK(Math::is_equal_approx(df[1 * 60 + 58], float(expected_n.z), 1e-4f));
	CHECK(Math::is_equal_approx(df[1 * 60 + 59], 0.5f));

	// Verify slot 2 (rank 1 = Candidate 2, no SubUV -> raw atlas rect):
	CHECK(Math::is_equal_approx(df[2 * 60 + 20], 0.1f, 1e-5f));
	CHECK(Math::is_equal_approx(df[2 * 60 + 21], 0.2f, 1e-5f));
	CHECK(Math::is_equal_approx(df[2 * 60 + 22], 0.4f, 1e-5f));
	CHECK(Math::is_equal_approx(df[2 * 60 + 23], 0.4f, 1e-5f));

	// Unused reserved slots 3 and 4 must be cleanly zeroed:
	CHECK(du[3 * 60 + 41] == 0u);
	CHECK(df[3 * 60 + 22] == 0.0f);
	CHECK(du[4 * 60 + 41] == 0u);
	CHECK(df[4 * 60 + 22] == 0.0f);

	// Verify blitted ClusterBuilderRD::RenderElementData at elem_buf[2..5]:
	const Vector<uint8_t> elem_rb = rd.buffer_get_data(elem_buf);
	REQUIRE(elem_rb.size() == 8 * 80);
	const uint32_t *eu = reinterpret_cast<const uint32_t *>(elem_rb.ptr());
	const float *ef = reinterpret_cast<const float *>(elem_rb.ptr());
	CHECK(eu[2 * 20 + 0] == 3u); // ELEMENT_TYPE_DECAL
	CHECK(eu[2 * 20 + 3] == 1u); // original_index = base_decal(1) + 0
	CHECK(Math::is_equal_approx(ef[2 * 20 + 16], 2.0f, 1e-4f)); // half_size.x = 4/2
	CHECK(Math::is_equal_approx(ef[2 * 20 + 17], 1.25f, 1e-4f)); // half_size.y = 2.5/2
	CHECK(Math::is_equal_approx(ef[2 * 20 + 18], 3.0f, 1e-4f)); // half_size.z = 6/2
	CHECK(eu[3 * 20 + 0] == 3u);
	CHECK(eu[3 * 20 + 3] == 2u); // original_index = base_decal(1) + 1

	// Fault-bit / 0-alive clean clear verification:
	const uint32_t meta_zero[8] = { 0u, 0u, 0u, 0u, 4u, uint32_t(RendererRD::ExternalOutputConfig::PARTICLE_DECAL_128), 12u, 0u };
	rd.buffer_update(meta_rid, 0, 32u, meta_zero);
	REQUIRE(bridge.update_external_decal_slot_present_seq(0xD001u, 1u, 0u, 1u, 12u));
	REQUIRE(bridge.dispatch_registered_external_decals_gpu(&rd, vp_a, scen_a, false, cam_xform, decal_buf, 1u, 8u, atlas_lookup, nullptr, mob_bounds) == 4u);
	const Vector<uint8_t> cleared_rb = rd.buffer_get_data(decal_buf);
	const uint32_t *cu = reinterpret_cast<const uint32_t *>(cleared_rb.ptr());
	CHECK(cu[1 * 60 + 41] == 0u);
	CHECK(cu[2 * 60 + 41] == 0u);

	bridge.unregister_external_decal_owner(0xD001u);
	CHECK(bridge.dispatch_registered_external_decals_gpu(&rd, vp_a, scen_a, false, cam_xform, decal_buf, 1u, 8u, atlas_lookup, nullptr, mob_bounds) == 0u);

	bridge.release_decal_gpu_bridge(&rd);
	rd.free_rid(elem_buf);
	rd.free_rid(decal_buf);
	rd.free_rid(recs_rid);
	rd.free_rid(meta_rid);
}
#endif

} // namespace TestParticleDecalBridge
