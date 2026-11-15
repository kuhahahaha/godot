/**************************************************************************/
/*  test_particle_ribbon_rd.h                                             */
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

#include "core/extension/gdextension_production_output_ribbon.h"
#include "core/io/marshalls.h"
#include "servers/rendering/renderer_rd/effects/particles_external_guard_indexed144.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_ribbon_rd.h"
#include "servers/rendering/rendering_device.h"
#include "tests/test_macros.h"

#include <cmath>
#include <cstring>

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
#include "drivers/metal/rendering_context_driver_metal.h"
#endif

namespace TestParticleRibbonK05 {

TEST_CASE("[particle_ribbon_k05] K05 INDEXED_144 V2 8-role prepare & acquire projection") {
	using RibbonRD = RendererRD::ParticlesRibbonRD;
	RibbonRD::Indexed144Layout layout_nomv;
	REQUIRE(RibbonRD::plan_indexed144(16, false, 1, layout_nomv) == OK);
	CHECK(layout_nomv.writable_count == 3);
	CHECK(layout_nomv.vertex_capacity == 64);
	CHECK(layout_nomv.index_capacity == 96);
	CHECK(layout_nomv.bytes[RibbonRD::CURRENT_VERTICES] == 64u * 144u);
	CHECK(layout_nomv.bytes[RibbonRD::ALIGNED_PREVIOUS_VERTICES] == 0u);
	CHECK(layout_nomv.bytes[RibbonRD::VERTEX_HISTORY_VALIDITY] == 0u);
	CHECK(layout_nomv.bytes[RibbonRD::INDEX_STAGING] == 96u * 4u);
	CHECK(layout_nomv.bytes[RibbonRD::OUTPUT_META] == 32u);

	RibbonRD::Indexed144Layout layout_mv;
	REQUIRE(RibbonRD::plan_indexed144(16, true, 1, layout_mv) == OK);
	CHECK(layout_mv.writable_count == 5);
	CHECK(layout_mv.bytes[RibbonRD::ALIGNED_PREVIOUS_VERTICES] == 64u * 144u);
	CHECK(layout_mv.bytes[RibbonRD::VERTEX_HISTORY_VALIDITY] == 64u * 4u);

	GDExtensionProductionOutputRoleV1 roles[8] = {};
	for (int i = 5; i < 8; i++) {
		roles[i].resource_slot = UINT32_MAX;
		roles[i].use_index = UINT32_MAX;
	}
	const uint32_t k05_roles[5] = {
		GDEXTENSION_OUTPUT_ROLE_CURRENT_VERTICES_V1,
		GDEXTENSION_OUTPUT_ROLE_ALIGNED_PREVIOUS_VERTICES_V1,
		GDEXTENSION_OUTPUT_ROLE_VERTEX_HISTORY_VALIDITY_V1,
		GDEXTENSION_OUTPUT_ROLE_INDEX_STAGING_V1,
		GDEXTENSION_OUTPUT_ROLE_OUTPUT_META_V1,
	};
	const uint32_t mv_bytes[5] = {
		layout_mv.bytes[RibbonRD::CURRENT_VERTICES],
		layout_mv.bytes[RibbonRD::ALIGNED_PREVIOUS_VERTICES],
		layout_mv.bytes[RibbonRD::VERTEX_HISTORY_VALIDITY],
		layout_mv.bytes[RibbonRD::INDEX_STAGING],
		layout_mv.bytes[RibbonRD::OUTPUT_META],
	};
	for (int i = 0; i < 5; i++) {
		roles[i].role = k05_roles[i];
		roles[i].resource_slot = uint32_t(i);
		roles[i].use_index = uint32_t(10 + i);
		roles[i].reserved = 0;
		roles[i].allocation_generation = 3;
		roles[i].bytes = mv_bytes[i];
	}
	CHECK(GDExtensionDeviceExit::validate_indexed144_prepare_v2_roles(roles, 16, true) == OK);
	// Non-zero trailing role[5] must be rejected:
	roles[5].role = 99;
	CHECK(GDExtensionDeviceExit::validate_indexed144_prepare_v2_roles(roles, 16, true) == ERR_INVALID_DATA);
	roles[5] = {};
	roles[5].resource_slot = UINT32_MAX;
	roles[5].use_index = UINT32_MAX;

	GDExtensionDeviceExit::Indexed144RoleBindingInput inputs[5] = {};
	for (int i = 0; i < 5; i++) {
		inputs[i].role = k05_roles[i];
		inputs[i].resource_slot = uint32_t(i);
		inputs[i].use_index = uint32_t(10 + i);
		inputs[i].primary_rid = uint64_t(1000 + i);
		inputs[i].allocation_generation = 3;
		inputs[i].bytes = mv_bytes[i];
	}
	GDExtensionOutputAcquireResultV2 acq = {};
	REQUIRE(GDExtensionDeviceExit::project_indexed144_acquire_v2_writables(inputs, 16, true, acq) == OK);
	CHECK(acq.writable_count == 5);
	CHECK(acq.writables[0].role == GDEXTENSION_OUTPUT_ROLE_CURRENT_VERTICES_V1);
	CHECK(acq.writables[4].role == GDEXTENSION_OUTPUT_ROLE_OUTPUT_META_V1);
	CHECK(acq.use_indices[2] == 12);
}

TEST_CASE("[particle_ribbon_k05] RIB-04 camera pullback normal under non-uniform scale & A05 history alignment") {
	using RibbonRD = RendererRD::ParticlesRibbonRD;
	const Basis host_non_uniform = Basis::from_scale(Vector3(4.0f, 0.5f, 2.0f));
	const RendererRD::RibbonSegmentEndpointKey32 key0 = { 7, 1, 102, 101 };

	RendererRD::Float144Vertex quad[4];
	uint32_t indices[6] = {};
	RibbonRD::build_ribbon_segment_quad(
			0,
			Vector3(0.0f, 1.0f, 0.0f),
			Vector3(0.0f, 0.0f, 0.0f),
			Vector3(0.0f, 0.5f, 5.0f),
			0.25f,
			0.0f,
			1.0f,
			Color(1.0f, 0.5f, 0.25f, 0.8f),
			host_non_uniform,
			key0,
			quad,
			indices);

	// Verify inverse_transpose(host_non_uniform) * pullback_normal normalizes to camera-facing +Z normal:
	const Basis inv_t = host_non_uniform.inverse().transposed();
	for (int v = 0; v < 4; v++) {
		const Vector3 n_local(quad[v].normal[0], quad[v].normal[1], quad[v].normal[2]);
		const Vector3 n_world = (inv_t.xform(n_local)).normalized();
		CHECK(n_world.distance_to(Vector3(0.0f, 0.0f, 1.0f)) < 1e-4f);
		CHECK(quad[v].get_endpoint_key() == key0);
	}
	CHECK(indices[0] == 0);
	CHECK(indices[1] == 1);
	CHECK(indices[2] == 2);
	CHECK(indices[3] == 2);
	CHECK(indices[4] == 1);
	CHECK(indices[5] == 3);

	// Collinear camera ray fallback must remain finite and unit-length:
	RibbonRD::build_ribbon_segment_quad(
			0,
			Vector3(0.0f, 0.0f, 1.0f),
			Vector3(0.0f, 0.0f, 0.0f),
			Vector3(0.0f, 0.0f, 5.0f),
			0.2f,
			0.0f,
			1.0f,
			Color(1, 1, 1, 1),
			Basis(),
			key0,
			quad,
			indices);
	for (int v = 0; v < 4; v++) {
		CHECK(Math::is_finite(quad[v].position[0]));
		CHECK(Math::is_finite(quad[v].normal[0]));
	}

	// A05 RIBBON_EXACT_SUCCESSOR alignment:
	LocalVector<RendererRD::Float144Vertex> prev_frame;
	for (int v = 0; v < 4; v++) {
		prev_frame.push_back(quad[v]);
	}
	// Current frame has 2 segments: segment 0 is a newly spawned head (no predecessor),
	// segment 1 is exact successor of previous segment 0 (`key0`).
	const RendererRD::RibbonSegmentEndpointKey32 key_new_head = { 7, 1, 103, 102 };
	RendererRD::Float144Vertex new_quad[4];
	uint32_t new_idx[6] = {};
	RibbonRD::build_ribbon_segment_quad(
			0, Vector3(0, 2, 0), Vector3(0, 1, 0), Vector3(0, 1, 5), 0.2f, 0.0f, 0.5f, Color(1, 1, 1, 1), Basis(), key_new_head, new_quad, new_idx);

	LocalVector<RendererRD::Float144Vertex> curr_frame;
	for (int v = 0; v < 4; v++) {
		curr_frame.push_back(new_quad[v]);
	}
	for (int v = 0; v < 4; v++) {
		RendererRD::Float144Vertex shifted = quad[v];
		shifted.position[1] -= 0.1f;
		curr_frame.push_back(shifted);
	}

	LocalVector<RendererRD::Float144Vertex> aligned_prev;
	LocalVector<uint32_t> history_valid;
	RibbonRD::align_previous_ribbon_vertices(curr_frame, prev_frame, aligned_prev, history_valid);
	REQUIRE(history_valid.size() == 8);
	for (int v = 0; v < 4; v++) {
		CHECK(history_valid[v] == 0u); // Newly spawned head segment has no previous history
	}
	for (int v = 4; v < 8; v++) {
		CHECK(history_valid[v] == 1u); // Exact 32B endpoint key match
		CHECK(aligned_prev[v].get_endpoint_key() == key0);
	}
}

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
TEST_CASE("[particle_ribbon_k05] Live Metal GPU ParticlesExternalGuardIndexed144 compute validation & fault zeroing") {
	using Guard = RendererRD::ParticlesExternalGuardIndexed144;
	using RibbonRD = RendererRD::ParticlesRibbonRD;

	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	Guard guard;
	REQUIRE(guard.prepare_pipelines_standalone_rd() == OK);
	REQUIRE(guard.is_prepared());

	RibbonRD::Indexed144Layout layout;
	REQUIRE(RibbonRD::plan_indexed144(2, true, 1, layout) == OK);
	RibbonRD::AllocatedBuffers bufs;
	REQUIRE(RibbonRD::allocate_buffers_rd(rd, layout, bufs) == OK);

	// Shared source gate buffer (8 uint32 words = 32B):
	// word 0 = active_segments (2u); gate words 1..4 = 0u (healthy/no source fault).
	Vector<uint8_t> source_bytes;
	source_bytes.resize(32);
	std::memset(source_bytes.ptrw(), 0, 32);
	encode_uint32(2u, source_bytes.ptrw() + 0);
	const RID source_rid = rd.storage_buffer_create(32, source_bytes);
	REQUIRE(source_rid.is_valid());

	// Populate GeometryProofHeader (240B) + 1 RibbonSurfaceProof (80B) = 320B:
	Guard::GeometryProofHeader proof = {};
	for (int k = 0; k < 3; k++) {
		proof.current_decl_min[k] = -10.0f;
		proof.current_decl_max[k] = 10.0f;
		proof.previous_decl_min[k] = -10.0f;
		proof.previous_decl_max[k] = 10.0f;
		proof.cull_min[k] = -20.0f;
		proof.cull_max[k] = 20.0f;
	}
	proof.source_accessor[0] = 0; // count word 0
	proof.source_accessor[3] = 1; // WORLD space
	proof.source_gate_words[0] = 1;
	proof.source_gate_words[1] = 2;
	proof.source_gate_words[2] = 3;
	proof.source_gate_words[3] = 4;
	proof.options[0] = 1; // motion_required = 1

	Guard::RibbonSurfaceProof surf = {};
	surf.command[0] = 1;
	surf.command[1] = 6;
	surf.command[2] = 0;
	surf.command[3] = 0;

	Vector<uint8_t> proof_bytes;
	proof_bytes.resize(sizeof(proof) + sizeof(surf));
	std::memcpy(proof_bytes.ptrw(), &proof, sizeof(proof));
	std::memcpy(proof_bytes.ptrw() + sizeof(proof), &surf, sizeof(surf));
	REQUIRE(rd.buffer_update(bufs.rids[RibbonRD::GEOMETRY_PROOF], 0, proof_bytes.size(), proof_bytes.ptr()) == OK);

	// Build 2 valid ribbon segments (8 vertices, 12 indices):
	LocalVector<RendererRD::Float144Vertex> curr_verts;
	curr_verts.resize(8);
	Vector<uint8_t> index_bytes;
	index_bytes.resize(12 * sizeof(uint32_t));
	for (uint32_t s = 0; s < 2; s++) {
		const RendererRD::RibbonSegmentEndpointKey32 key = { 1, 1, uint64_t(10 + s + 1), uint64_t(10 + s) };
		uint32_t seg_idx[6] = {};
		RibbonRD::build_ribbon_segment_quad(
				s,
				Vector3(0.0f, float(s + 1), 0.0f),
				Vector3(0.0f, float(s), 0.0f),
				Vector3(0.0f, 0.5f, 5.0f),
				0.2f,
				0.0f,
				1.0f,
				Color(1, 1, 1, 1),
				Basis(),
				key,
				&curr_verts[s * 4],
				seg_idx);
		for (uint32_t j = 0; j < 6; j++) {
			encode_uint32(seg_idx[j], index_bytes.ptrw() + (s * 6 + j) * 4);
		}
	}

	LocalVector<RendererRD::Float144Vertex> aligned_prev;
	LocalVector<uint32_t> hist_valid;
	RibbonRD::align_previous_ribbon_vertices(curr_verts, curr_verts, aligned_prev, hist_valid);

	// OutputMeta (8 uint32 words = 32B):
	// [0]=active_segments(2), [1..3]=0, [4]=capacity(2), [5]=profile(2=INDEXED_144), [6]=seq_lo, [7]=seq_hi
	Vector<uint8_t> meta_bytes;
	meta_bytes.resize(32);
	std::memset(meta_bytes.ptrw(), 0, 32);
	encode_uint32(2u, meta_bytes.ptrw() + 0);
	encode_uint32(2u, meta_bytes.ptrw() + 16);
	encode_uint32(2u, meta_bytes.ptrw() + 20);
	encode_uint32(0x11223344u, meta_bytes.ptrw() + 24);
	encode_uint32(0x55667788u, meta_bytes.ptrw() + 28);

	const auto upload_ribbon_inputs = [&](const LocalVector<RendererRD::Float144Vertex> &p_curr,
											  const LocalVector<RendererRD::Float144Vertex> &p_prev,
											  const LocalVector<uint32_t> &p_hist,
											  const Vector<uint8_t> &p_indices) {
		REQUIRE(rd.buffer_update(bufs.rids[RibbonRD::CURRENT_VERTICES], 0, p_curr.size() * sizeof(RendererRD::Float144Vertex), p_curr.ptr()) == OK);
		REQUIRE(rd.buffer_update(bufs.rids[RibbonRD::ALIGNED_PREVIOUS_VERTICES], 0, p_prev.size() * sizeof(RendererRD::Float144Vertex), p_prev.ptr()) == OK);
		REQUIRE(rd.buffer_update(bufs.rids[RibbonRD::VERTEX_HISTORY_VALIDITY], 0, p_hist.size() * sizeof(uint32_t), p_hist.ptr()) == OK);
		REQUIRE(rd.buffer_update(bufs.rids[RibbonRD::INDEX_STAGING], 0, p_indices.size(), p_indices.ptr()) == OK);
		REQUIRE(rd.buffer_update(bufs.rids[RibbonRD::OUTPUT_META], 0, meta_bytes.size(), meta_bytes.ptr()) == OK);
	};

	Guard::Bindings bindings;
	bindings.buffers[Guard::CURRENT_VERTICES] = { bufs.rids[RibbonRD::CURRENT_VERTICES], bufs.bytes[RibbonRD::CURRENT_VERTICES] };
	bindings.buffers[Guard::ALIGNED_PREVIOUS_VERTICES] = { bufs.rids[RibbonRD::ALIGNED_PREVIOUS_VERTICES], bufs.bytes[RibbonRD::ALIGNED_PREVIOUS_VERTICES] };
	bindings.buffers[Guard::VERTEX_HISTORY_VALIDITY] = { bufs.rids[RibbonRD::VERTEX_HISTORY_VALIDITY], bufs.bytes[RibbonRD::VERTEX_HISTORY_VALIDITY] };
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
	req.capacity = 2;
	req.sequence_low = 0x11223344u;
	req.sequence_high = 0x55667788u;
	req.surface_count = 1;
	req.segment_base = 0;
	req.segment_count = 2;

	Guard::LayoutPlan plan;
	REQUIRE(Guard::plan_layout(req, bindings, proof, 8, plan) == OK);

	const RID uniform_set = rd.uniform_set_create(plan.uniforms, guard.get_shader(Guard::INIT), 0);
	REQUIRE(uniform_set.is_valid());

	const auto run_guard_gpu = [&](uint32_t &r_fault_bits, uint32_t &r_index_count, uint32_t &r_instance_count) {
		RD::ComputeListID cl = rd.compute_list_begin();
		rd.compute_list_bind_compute_pipeline(cl, guard.get_pipeline(plan.initialize_mode));
		rd.compute_list_bind_uniform_set(cl, uniform_set, 0);
		rd.compute_list_set_push_constant(cl, &plan.initialize, sizeof(Guard::PushConstant));
		rd.compute_list_dispatch(cl, 1, 1, 1);
		rd.compute_list_add_barrier(cl);

		for (int c = 0; c < plan.validation_chunks.size(); c++) {
			rd.compute_list_bind_compute_pipeline(cl, guard.get_pipeline(Guard::VALIDATE));
			rd.compute_list_bind_uniform_set(cl, uniform_set, 0);
			rd.compute_list_set_push_constant(cl, &plan.validation_chunks[c], sizeof(Guard::PushConstant));
			rd.compute_list_dispatch(cl, plan.validation_groups_x[c], 1, 1);
		}
		rd.compute_list_add_barrier(cl);

		rd.compute_list_bind_compute_pipeline(cl, guard.get_pipeline(plan.finalize_mode));
		rd.compute_list_bind_uniform_set(cl, uniform_set, 0);
		rd.compute_list_set_push_constant(cl, &plan.finalize, sizeof(Guard::PushConstant));
		rd.compute_list_dispatch(cl, 1, 1, 1);
		rd.compute_list_end();

		rd.submit();
		rd.sync();

		const Vector<uint8_t> status_rb = rd.buffer_get_data(bufs.rids[RibbonRD::GUARD_STATUS]);
		const Vector<uint8_t> cmd_rb = rd.buffer_get_data(bufs.rids[RibbonRD::PROTECTED_COMMANDS]);
		REQUIRE(status_rb.size() >= 32);
		REQUIRE(cmd_rb.size() >= 20);
		r_fault_bits = decode_uint32(status_rb.ptr() + 0);
		r_index_count = decode_uint32(cmd_rb.ptr() + 0);
		r_instance_count = decode_uint32(cmd_rb.ptr() + 4);
	};

	// Case 1: Valid ribbon packet -> 0 faults, ProtectedCommands = {indexCount = 12, instanceCount = 1}.
	upload_ribbon_inputs(curr_verts, aligned_prev, hist_valid, index_bytes);
	uint32_t fault_bits = UINT32_MAX;
	uint32_t cmd_idx_count = 0;
	uint32_t cmd_inst_count = 0;
	run_guard_gpu(fault_bits, cmd_idx_count, cmd_inst_count);
	CHECK(fault_bits == 0u);
	CHECK(cmd_idx_count == 12u);
	CHECK(cmd_inst_count == 1u);

	// Case 2: Subnormal float in current vertex -> NUMERIC_FAULT (16u) & ProtectedCommands zeroed.
	LocalVector<RendererRD::Float144Vertex> bad_numeric(curr_verts);
	uint32_t subnormal_bits = 1u;
	std::memcpy(&bad_numeric[0].position[0], &subnormal_bits, sizeof(float));
	upload_ribbon_inputs(bad_numeric, aligned_prev, hist_valid, index_bytes);
	run_guard_gpu(fault_bits, cmd_idx_count, cmd_inst_count);
	CHECK((fault_bits & 16u) == 16u);
	CHECK(cmd_idx_count == 0u);
	CHECK(cmd_inst_count == 0u);

	// Case 3: Out-of-bounds quad index -> BOUNDS_FAULT (64u) & ProtectedCommands zeroed.
	Vector<uint8_t> bad_indices = index_bytes;
	encode_uint32(8u, bad_indices.ptrw() + 0); // index 8 >= 4 * active_segments (8)
	upload_ribbon_inputs(curr_verts, aligned_prev, hist_valid, bad_indices);
	run_guard_gpu(fault_bits, cmd_idx_count, cmd_inst_count);
	CHECK((fault_bits & 64u) == 64u);
	CHECK(cmd_idx_count == 0u);
	CHECK(cmd_inst_count == 0u);

	// Case 4: A05 motion-vector history mismatch (history_valid == 1 with altered previous endpoint key) -> HISTORY_FAULT (32u).
	LocalVector<RendererRD::Float144Vertex> bad_prev(aligned_prev);
	bad_prev[3].endpoint_key_words[0] ^= 0xdeadbeefu;
	upload_ribbon_inputs(curr_verts, bad_prev, hist_valid, index_bytes);
	run_guard_gpu(fault_bits, cmd_idx_count, cmd_inst_count);
	CHECK((fault_bits & 32u) == 32u);
	CHECK(cmd_idx_count == 0u);
	CHECK(cmd_inst_count == 0u);

	// Clean teardown on same Metal RD with 0 leaked RIDs:
	rd.free_rid(uniform_set);
	rd.free_rid(source_rid);
	RibbonRD::free_buffers_rd(rd, bufs);
	CHECK(bufs.live_count == 0);
	REQUIRE(guard.release_pipelines_standalone_rd() == OK);
}

TEST_CASE("[particle_ribbon_k05] D3-P0-3 Live Metal GPU ParticlesExternalGuardIndexed144 MOTION_DISABLED (0-byte optional roles 4 & 5)") {
	using Guard = RendererRD::ParticlesExternalGuardIndexed144;
	using RibbonRD = RendererRD::ParticlesRibbonRD;

	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	Guard guard;
	REQUIRE(guard.prepare_pipelines_standalone_rd() == OK);
	REQUIRE(guard.is_prepared());

	// Plan INDEXED_144 with motion_required = false -> ALIGNED_PREVIOUS_VERTICES and VERTEX_HISTORY_VALIDITY are 0 bytes!
	RibbonRD::Indexed144Layout layout;
	REQUIRE(RibbonRD::plan_indexed144(2, false, 1, layout) == OK);
	CHECK(layout.bytes[RibbonRD::ALIGNED_PREVIOUS_VERTICES] == 0u);
	CHECK(layout.bytes[RibbonRD::VERTEX_HISTORY_VALIDITY] == 0u);

	RibbonRD::AllocatedBuffers bufs;
	REQUIRE(RibbonRD::allocate_buffers_rd(rd, layout, bufs) == OK);
	CHECK(bufs.rids[RibbonRD::ALIGNED_PREVIOUS_VERTICES].is_null());
	CHECK(bufs.rids[RibbonRD::VERTEX_HISTORY_VALIDITY].is_null());

	Vector<uint8_t> source_bytes;
	source_bytes.resize(32);
	std::memset(source_bytes.ptrw(), 0, 32);
	encode_uint32(2u, source_bytes.ptrw() + 0);
	const RID source_rid = rd.storage_buffer_create(32, source_bytes);
	REQUIRE(source_rid.is_valid());

	Guard::GeometryProofHeader proof = {};
	for (int k = 0; k < 3; k++) {
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
	proof.options[0] = 0; // motion_required = 0 (MOTION_DISABLED)

	Guard::RibbonSurfaceProof surf = {};
	surf.command[0] = 1;
	surf.command[1] = 6;

	Vector<uint8_t> proof_bytes;
	proof_bytes.resize(sizeof(proof) + sizeof(surf));
	std::memcpy(proof_bytes.ptrw(), &proof, sizeof(proof));
	std::memcpy(proof_bytes.ptrw() + sizeof(proof), &surf, sizeof(surf));
	REQUIRE(rd.buffer_update(bufs.rids[RibbonRD::GEOMETRY_PROOF], 0, proof_bytes.size(), proof_bytes.ptr()) == OK);

	LocalVector<RendererRD::Float144Vertex> curr_verts;
	curr_verts.resize(8);
	Vector<uint8_t> index_bytes;
	index_bytes.resize(12 * sizeof(uint32_t));
	for (uint32_t s = 0; s < 2; s++) {
		const RendererRD::RibbonSegmentEndpointKey32 key = { 2, 1, uint64_t(20 + s + 1), uint64_t(20 + s) };
		uint32_t seg_idx[6] = {};
		RibbonRD::build_ribbon_segment_quad(
				s,
				Vector3(0.0f, float(s + 1), 0.0f),
				Vector3(0.0f, float(s), 0.0f),
				Vector3(0.0f, 0.5f, 5.0f),
				0.2f,
				0.0f,
				1.0f,
				Color(1, 1, 1, 1),
				Basis(),
				key,
				&curr_verts[s * 4],
				seg_idx);
		for (uint32_t j = 0; j < 6; j++) {
			encode_uint32(seg_idx[j], index_bytes.ptrw() + (s * 6 + j) * 4);
		}
	}

	Vector<uint8_t> meta_bytes;
	meta_bytes.resize(32);
	std::memset(meta_bytes.ptrw(), 0, 32);
	encode_uint32(2u, meta_bytes.ptrw() + 0);
	encode_uint32(2u, meta_bytes.ptrw() + 16);
	encode_uint32(2u, meta_bytes.ptrw() + 20);
	encode_uint32(0xaaaabbbbu, meta_bytes.ptrw() + 24);
	encode_uint32(0xccccddddu, meta_bytes.ptrw() + 28);

	REQUIRE(rd.buffer_update(bufs.rids[RibbonRD::CURRENT_VERTICES], 0, curr_verts.size() * sizeof(RendererRD::Float144Vertex), curr_verts.ptr()) == OK);
	REQUIRE(rd.buffer_update(bufs.rids[RibbonRD::INDEX_STAGING], 0, index_bytes.size(), index_bytes.ptr()) == OK);
	REQUIRE(rd.buffer_update(bufs.rids[RibbonRD::OUTPUT_META], 0, meta_bytes.size(), meta_bytes.ptr()) == OK);

	Guard::Bindings bindings;
	bindings.buffers[Guard::CURRENT_VERTICES] = { bufs.rids[RibbonRD::CURRENT_VERTICES], bufs.bytes[RibbonRD::CURRENT_VERTICES] };
	bindings.buffers[Guard::ALIGNED_PREVIOUS_VERTICES] = { RID(), 0 }; // Optional 0-byte motion role!
	bindings.buffers[Guard::VERTEX_HISTORY_VALIDITY] = { RID(), 0 }; // Optional 0-byte motion role!
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
	req.capacity = 2;
	req.sequence_low = 0xaaaabbbbu;
	req.sequence_high = 0xccccddddu;
	req.surface_count = 1;
	req.segment_base = 0;
	req.segment_count = 2;

	Guard::LayoutPlan plan;
	REQUIRE(Guard::plan_layout(req, bindings, proof, 8, plan) == OK);
	CHECK(plan.initialize_mode == Guard::INIT_NO_MOTION);
	CHECK(plan.validate_mode == Guard::VALIDATE_NO_MOTION);
	CHECK(plan.finalize_mode == Guard::FINALIZE_NO_MOTION);
	CHECK(plan.uniforms.size() == 12); // 14 bindings minus 2 omitted motion bindings

	const RID uniform_set = rd.uniform_set_create(plan.uniforms, guard.get_shader(plan.initialize_mode), 0);
	REQUIRE(uniform_set.is_valid());
	RD::ComputeListID cl = rd.compute_list_begin();
	REQUIRE(guard.record(cl, plan, uniform_set, uniform_set, uniform_set) == OK);
	rd.compute_list_end();
	rd.submit();
	rd.sync();

	const Vector<uint8_t> status_rb = rd.buffer_get_data(bufs.rids[RibbonRD::GUARD_STATUS]);
	const Vector<uint8_t> cmd_rb = rd.buffer_get_data(bufs.rids[RibbonRD::PROTECTED_COMMANDS]);
	REQUIRE(status_rb.size() >= 32);
	REQUIRE(cmd_rb.size() >= 20);
	CHECK(decode_uint32(status_rb.ptr() + 0) == 0u);
	CHECK(decode_uint32(cmd_rb.ptr() + 0) == 12u);
	CHECK(decode_uint32(cmd_rb.ptr() + 4) == 1u);

	rd.free_rid(uniform_set);
	rd.free_rid(source_rid);
	RibbonRD::free_buffers_rd(rd, bufs);
	CHECK(bufs.live_count == 0);
	REQUIRE(guard.release_pipelines_standalone_rd() == OK);
}
#endif

} // namespace TestParticleRibbonK05
