/**************************************************************************/
/*  test_particle_mesh_subuv_light.h                                      */
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

#include "core/io/marshalls.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_config.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_mesh_light_sort_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/light_storage.h"
#include "servers/rendering/rendering_device.h"
#include "tests/test_macros.h"

#include <cmath>
#include <cstring>
#include <limits>

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
#include "drivers/metal/rendering_context_driver_metal.h"
#endif

namespace TestParticleMeshSubuvLight {

TEST_CASE("[particle_mesh_subuv_light] Instance192 FTZ canonicalization & multi-surface indirect command planner") {
	using Lane = RendererRD::ParticlesMeshLightSortRD;
	CHECK(sizeof(RendererRD::Instance192Record) == 192);
	CHECK(sizeof(RendererRD::ParticleLightRecord64) == 64);
	CHECK(sizeof(RendererRD::MeshSurfaceIndirectCommand20) == 20);

	uint32_t subnormal_bits = 1u;
	float subnormal_f = 0.0f;
	std::memcpy(&subnormal_f, &subnormal_bits, sizeof(float));
	const float flushed = Lane::canonicalize_finite_float(subnormal_f);
	uint32_t flushed_bits = 0;
	std::memcpy(&flushed_bits, &flushed, sizeof(uint32_t));
	CHECK(flushed_bits == 0u); // +0.0f exact bit pattern

	const float neg_zero = -0.0f;
	const float pos_zero = Lane::canonicalize_finite_float(neg_zero);
	std::memcpy(&flushed_bits, &pos_zero, sizeof(uint32_t));
	CHECK(flushed_bits == 0u);

	// Multi-surface mesh command table (up to MAX_FLATTENED_SURFACES):
	LocalVector<uint32_t> surf_indices;
	surf_indices.push_back(36);
	surf_indices.push_back(120);
	surf_indices.push_back(60);

	LocalVector<RendererRD::MeshSurfaceIndirectCommand20> cmds;
	REQUIRE(Lane::plan_multisurface_mesh_commands(surf_indices, 42, false, cmds) == OK);
	REQUIRE(cmds.size() == 3);
	CHECK(cmds[0].index_count == 36);
	CHECK(cmds[0].instance_count == 42);
	CHECK(cmds[1].index_count == 120);
	CHECK(cmds[1].instance_count == 42);
	CHECK(cmds[2].index_count == 60);
	CHECK(cmds[2].instance_count == 42);

	// Guard fault zeros all surface commands deterministically:
	REQUIRE(Lane::plan_multisurface_mesh_commands(surf_indices, 42, true, cmds) == OK);
	for (uint32_t i = 0; i < cmds.size(); i++) {
		CHECK(cmds[i].index_count == 0);
		CHECK(cmds[i].instance_count == 0);
	}

	// Exceeding MAX_FLATTENED_SURFACES fails closed:
	LocalVector<uint32_t> too_many;
	too_many.resize(RendererRD::ExternalOutputConfig::MAX_FLATTENED_SURFACES + 1);
	for (uint32_t i = 0; i < too_many.size(); i++) {
		too_many[i] = 12;
	}
	CHECK(Lane::plan_multisurface_mesh_commands(too_many, 1, false, cmds) == ERR_INVALID_PARAMETER);
}

TEST_CASE("[particle_mesh_subuv_light] Sub-UV domain bounds (<= 64x64) & dual-frame NONE/STEPPED/LINEAR_BLEND") {
	using Lane = RendererRD::ParticlesMeshLightSortRD;

	RendererRD::SubUVFlipbookSpec spec;
	spec.cols = 64;
	spec.rows = 64;
	CHECK(Lane::validate_subuv_domain(spec) == OK);

	spec.cols = 65;
	spec.rows = 1;
	CHECK(Lane::validate_subuv_domain(spec) == ERR_INVALID_PARAMETER);

	spec.cols = 0;
	spec.rows = 4;
	CHECK(Lane::validate_subuv_domain(spec) == ERR_INVALID_PARAMETER);

	// 4x2 atlas (8 frames), LINEAR_BLEND at frame_coord = 2.75 with CLAMP vs REPEAT at boundary 7.5:
	spec.cols = 4;
	spec.rows = 2;
	spec.blend_mode = RendererRD::SubUVBlendMode::LINEAR_BLEND;
	spec.wrap_mode = RendererRD::SubUVWrapMode::CLAMP;

	RendererRD::SubUVSamplePair sample;
	REQUIRE(Lane::evaluate_subuv_sample(spec, Vector2(0.5f, 0.5f), 2.75f, sample) == OK);
	CHECK(sample.frame0 == 2);
	CHECK(sample.frame1 == 3);
	CHECK(Math::is_equal_approx(sample.blend_alpha, 0.75f));
	CHECK(sample.uv0.is_equal_approx(Vector2((2.0f + 0.5f) / 4.0f, 0.25f)));
	CHECK(sample.uv1.is_equal_approx(Vector2((3.0f + 0.5f) / 4.0f, 0.25f)));

	// Boundary clamp vs repeat at frame_coord = 7.5:
	REQUIRE(Lane::evaluate_subuv_sample(spec, Vector2(0.5f, 0.5f), 7.5f, sample) == OK);
	CHECK(sample.frame0 == 7);
	CHECK(sample.frame1 == 7);

	spec.wrap_mode = RendererRD::SubUVWrapMode::REPEAT;
	REQUIRE(Lane::evaluate_subuv_sample(spec, Vector2(0.5f, 0.5f), 7.5f, sample) == OK);
	CHECK(sample.frame0 == 7);
	CHECK(sample.frame1 == 0);
	CHECK(Math::is_equal_approx(sample.blend_alpha, 0.5f));
}

TEST_CASE("[particle_mesh_subuv_light] A06 per-view E14-E16 ConsumedTuple & back-to-front sort isolation") {
	using Lane = RendererRD::ParticlesMeshLightSortRD;
	Lane manager;

	LocalVector<RendererRD::Instance192Record> frame1;
	frame1.push_back(Lane::make_instance192_record(Transform3D(Basis(), Vector3(0, 0, -2.0f)), Vector3(), Color(1, 1, 1, 1), 0.0f, 101, false));
	frame1.push_back(Lane::make_instance192_record(Transform3D(Basis(), Vector3(0, 0, -10.0f)), Vector3(), Color(1, 1, 1, 1), 0.0f, 102, false));
	frame1.push_back(Lane::make_instance192_record(Transform3D(Basis(), Vector3(0, 0, -5.0f)), Vector3(), Color(1, 1, 1, 1), 0.0f, 103, false));

	// View A looks along -Z from z = +5: farthest is z = -10 (idx 1), then z = -5 (idx 2), then z = -2 (idx 0).
	LocalVector<RendererRD::Instance192Record> prev_a;
	LocalVector<uint32_t> sort_a;
	REQUIRE(manager.submit_view_frame(1, 1, 1, 10, Vector3(0, 0, 5.0f), Vector3(0, 0, -1.0f), frame1, prev_a, sort_a) == OK);
	REQUIRE(sort_a.size() == 3);
	CHECK(sort_a[0] == 1);
	CHECK(sort_a[1] == 2);
	CHECK(sort_a[2] == 0);

	// View B looks along +Z from z = -20: farthest along +Z is z = -2 (idx 0), then z = -5 (idx 2), then z = -10 (idx 1).
	LocalVector<RendererRD::Instance192Record> prev_b;
	LocalVector<uint32_t> sort_b;
	REQUIRE(manager.submit_view_frame(2, 1, 1, 20, Vector3(0, 0, -20.0f), Vector3(0, 0, 1.0f), frame1, prev_b, sort_b) == OK);
	REQUIRE(sort_b.size() == 3);
	CHECK(sort_b[0] == 0);
	CHECK(sort_b[1] == 2);
	CHECK(sort_b[2] == 1);

	// Pause, resize/epoch-bump, and destroy View B: View A's retained history & sort state must remain 100% intact!
	REQUIRE(manager.set_view_paused(2, true) == OK);
	CHECK(manager.submit_view_frame(2, 1, 2, 21, Vector3(0, 0, -20.0f), Vector3(0, 0, 1.0f), frame1, prev_b, sort_b) == ERR_BUSY);
	REQUIRE(manager.resize_or_epoch_bump_view(2, 2) == OK);
	REQUIRE(manager.destroy_view(2) == OK);
	CHECK(manager.get_view_state(2) == nullptr);

	// Advance View A to frame 2: all 3 particles match View A's retained frame 1 history!
	REQUIRE(manager.submit_view_frame(1, 1, 2, 11, Vector3(0, 0, 5.0f), Vector3(0, 0, -1.0f), frame1, prev_a, sort_a) == OK);
	for (uint32_t i = 0; i < 3; i++) {
		CHECK((prev_a[i].flags & RendererRD::Instance192Record::FLAG_HAS_PREVIOUS_RECORD) != 0);
	}
}

TEST_CASE("[particle_mesh_subuv_light] Deterministic top-K particle light compaction & live Metal RD shader/buffer seam") {
	using Lane = RendererRD::ParticlesMeshLightSortRD;

	LocalVector<RendererRD::ParticleLightRecord64> lights_a;
	for (uint32_t i = 0; i < 6; i++) {
		RendererRD::ParticleLightRecord64 l;
		l.position_world[0] = float(i);
		l.radius = 3.0f;
		l.color_hdr[0] = 2.0f;
		l.color_hdr[1] = 1.0f;
		l.color_hdr[2] = 0.5f;
		l.id_index = 100 - i;
		l.id_acquire_tag = 1;
		l.screen_importance_q16 = (i < 4) ? 50000u : 10000u; // 4 tied high-importance candidates -> tie-break by lowest (id_acquire_tag, id_index)
		lights_a.push_back(l);
	}
	LocalVector<RendererRD::ParticleLightRecord64> lights_b;
	for (int i = int(lights_a.size()) - 1; i >= 0; i--) {
		lights_b.push_back(lights_a[i]);
	}

	LocalVector<RendererRD::ParticleLightRecord64> sel_a;
	LocalVector<RendererRD::ParticleLightRecord64> sel_b;
	REQUIRE(Lane::compact_particle_lights_deterministic(lights_a, 2, sel_a) == OK);
	REQUIRE(Lane::compact_particle_lights_deterministic(lights_b, 2, sel_b) == OK);
	REQUIRE(sel_a.size() == 2);
	REQUIRE(sel_b.size() == 2);
	// Out of id_index {100, 99, 98, 97} with equal importance 50000, lowest id_index 97 and 98 win deterministically:
	CHECK(sel_a[0].id_index == 97);
	CHECK(sel_a[1].id_index == 98);
	CHECK(sel_b[0].id_index == 97);
	CHECK(sel_b[1].id_index == 98);

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	Lane lane;
	REQUIRE(lane.prepare_translucent_seed_shader_standalone_rd() == OK);
	CHECK(lane.get_translucent_seed_shader_variant(RendererRD::SubUVBlendMode::NONE).is_valid());
	CHECK(lane.get_translucent_seed_shader_variant(RendererRD::SubUVBlendMode::STEPPED).is_valid());
	CHECK(lane.get_translucent_seed_shader_variant(RendererRD::SubUVBlendMode::LINEAR_BLEND).is_valid());

	Lane::LightClusterBufferRD gpu_lights;
	REQUIRE(Lane::upload_particle_lights_rd(rd, 8, sel_a, gpu_lights) == OK);
	CHECK(gpu_lights.header_buffer.is_valid());
	CHECK(gpu_lights.records_buffer.is_valid());
	CHECK(gpu_lights.active_count == 2);

	const Vector<uint8_t> hdr_rb = rd.buffer_get_data(gpu_lights.header_buffer);
	REQUIRE(hdr_rb.size() == 16);
	CHECK(decode_uint32(hdr_rb.ptr() + 0) == 2u);
	CHECK(decode_uint32(hdr_rb.ptr() + 4) == 8u);

	Lane::free_particle_lights_rd(rd, gpu_lights);
	CHECK(gpu_lights.header_buffer.is_null());
	CHECK(gpu_lights.records_buffer.is_null());
	lane.release_translucent_seed_shader();
#endif
}

TEST_CASE("[particle_mesh_subuv_light] D3-P0-4/D3-P0-5/D3-P1-4a ParticleLight64 profile 4, NaN view_depth sort safety, O(N) persistent-ID & active light publication") {
	using Lane = RendererRD::ParticlesMeshLightSortRD;

	// 1. D3-P0-4: ExternalOutputConfig::PARTICLE_LIGHT_64 (profile = 4) parsing & validation:
	Dictionary cfg_dict;
	cfg_dict["schema"] = int64_t(1);
	cfg_dict["profile"] = int64_t(RendererRD::ExternalOutputConfig::PARTICLE_LIGHT_64);
	cfg_dict["capacity"] = int64_t(64);
	cfg_dict["max_views"] = int64_t(2);
	cfg_dict["max_inflight"] = int64_t(2);
	cfg_dict["surfaces"] = Array();
	cfg_dict["surface_materials"] = Array();
	cfg_dict["space"] = int64_t(RendererRD::ExternalOutputConfig::WORLD);
	cfg_dict["pass_mask"] = int64_t(0);
	cfg_dict["motion"] = int64_t(RendererRD::ExternalOutputConfig::DISABLED);
	cfg_dict["query_enabled"] = false;
	cfg_dict["bounds"] = AABB(Vector3(-10, -10, -10), Vector3(20, 20, 20));
	PackedByteArray tag32;
	tag32.resize(32);
	std::memset(tag32.ptrw(), 0x5Au, 32);
	cfg_dict["producer_tag"] = tag32;
	cfg_dict["device_tag"] = tag32;
	cfg_dict["layout_digest"] = tag32;
	cfg_dict["mapping_digest"] = tag32;
	cfg_dict["numeric_profile_digest"] = tag32;

	RendererRD::ExternalOutputConfig parsed_cfg;
	StringName parse_detail;
	REQUIRE(RendererRD::ExternalOutputConfig::parse(cfg_dict, parsed_cfg, parse_detail) == OK);
	CHECK(parsed_cfg.profile == RendererRD::ExternalOutputConfig::PARTICLE_LIGHT_64);
	CHECK(parsed_cfg.capacity == 64u);

	// Exceeding MAX_PARTICLE_LIGHTS (256) must reject with LIMIT:
	Dictionary over_dict = cfg_dict.duplicate();
	over_dict["capacity"] = 257;
	RendererRD::ExternalOutputConfig bad_cfg;
	CHECK(RendererRD::ExternalOutputConfig::parse(over_dict, bad_cfg, parse_detail) == ERR_INVALID_PARAMETER);

	// 2. D3-P0-5 & D3-P1-4a: NaN/Inf particle coordinates & non-finite camera_forward must never violate strict-weak-ordering,
	// and O(N) persistent-ID HashMap aligns 512 shuffled instances accurately:
	Lane manager;
	LocalVector<RendererRD::Instance192Record> f0;
	for (uint32_t i = 0; i < 512; i++) {
		f0.push_back(Lane::make_instance192_record(
				Transform3D(Basis(), Vector3(float(i) * 0.1f, 0.0f, -float(i))),
				Vector3(),
				Color(1, 1, 1, 1),
				0.0f,
				uint64_t(10000 + i),
				false));
	}
	LocalVector<RendererRD::Instance192Record> prev_out;
	LocalVector<uint32_t> sorted_out;
	const float qnan = std::numeric_limits<float>::quiet_NaN();
	const float pinf = std::numeric_limits<float>::infinity();
	REQUIRE(manager.submit_view_frame(1, 1, 1, 1, Vector3(0, 0, 10), Vector3(qnan, 0, -1), f0, prev_out, sorted_out) == OK);
	REQUIRE(sorted_out.size() == 512);

	// Frame 2: reverse order + inject raw NaN & +Inf translations into records to test std::sort strict-weak-ordering:
	LocalVector<RendererRD::Instance192Record> f1;
	for (int i = 511; i >= 0; i--) {
		RendererRD::Instance192Record rec = f0[i];
		rec.transform_rows[2][3] -= 0.5f;
		if (i == 250) {
			rec.transform_rows[0][3] = qnan; // NaN translation -> must sort deterministically without UB
		} else if (i == 125) {
			rec.transform_rows[2][3] = pinf; // +Inf translation
		}
		f1.push_back(rec);
	}
	REQUIRE(manager.submit_view_frame(1, 1, 2, 2, Vector3(0, 0, 10), Vector3(0, 0, -1), f1, prev_out, sorted_out) == OK);
	REQUIRE(prev_out.size() == 512);
	REQUIRE(sorted_out.size() == 512);
	for (uint32_t i = 0; i < 512; i++) {
		CHECK((prev_out[i].flags & RendererRD::Instance192Record::FLAG_HAS_PREVIOUS_RECORD) != 0);
		CHECK(prev_out[i].get_particle_id() == f1[i].get_particle_id());
	}

	// 3. Active particle light publication & deterministic compaction on ParticlesMeshLightSortRD:
	LocalVector<RendererRD::ParticleLightRecord64> pub_in;
	for (uint32_t i = 0; i < 5; i++) {
		RendererRD::ParticleLightRecord64 l;
		l.position_world[0] = float(i) * 2.0f;
		l.radius = 4.0f;
		l.color_hdr[0] = 3.0f;
		l.id_index = 50 + i;
		l.id_acquire_tag = 1;
		l.screen_importance_q16 = 40000u + i * 100u;
		pub_in.push_back(l);
	}
	REQUIRE(manager.publish_active_particle_lights(pub_in, 3) == OK);
	CHECK(manager.get_active_particle_light_count() == 3u);
	LocalVector<RendererRD::ParticleLightRecord64> pub_out;
	manager.get_active_particle_lights(pub_out);
	REQUIRE(pub_out.size() == 3);
	CHECK(pub_out[0].id_index == 54u);
	manager.clear_active_particle_lights();
	CHECK(manager.get_active_particle_light_count() == 0u);
}

} // namespace TestParticleMeshSubuvLight
