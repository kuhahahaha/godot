/**************************************************************************/
/*  test_external_output_guard.cpp                                        */
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

#include "servers/rendering/renderer_rd/effects/particles_external_guard.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_config.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_external_output_guard)

namespace TestExternalOutputGuard {

using Guard = RendererRD::ParticlesExternalGuard;

// Test-local RIDs exercise pure layout rejection, never real native backing.
static Guard::Bindings bindings_for(uint32_t p_capacity, uint32_t p_surfaces) {
	Guard::Bindings bindings;
	const uint64_t bytes[Guard::BINDING_COUNT] = { uint64_t(p_capacity) * 192, uint64_t(p_capacity) * 192, 32, 16, 64, 32, uint64_t(p_surfaces) * 20, 240 + uint64_t(p_surfaces) * 144, 16, 20, 24, 28 };
	for (uint32_t i = 0; i < Guard::BINDING_COUNT; i++) {
		bindings.buffers[i].byte_length = bytes[i];
		if (bytes[i] != 0) {
			bindings.buffers[i].rid = RID::from_uint64(100 + i);
		}
	}
	return bindings;
}

static Guard::GeometryProofHeader source_header() {
	Guard::GeometryProofHeader proof;
	proof.source_accessor[0] = 1;
	proof.source_gate_words[0] = 2;
	proof.source_gate_words[1] = 3;
	proof.source_gate_words[2] = 4;
	proof.source_gate_words[3] = 5;
	proof.source_accessor[3] = 1;
	return proof;
}

static Guard::PushConstant request_for(uint32_t p_capacity, uint32_t p_surfaces = 1) {
	Guard::PushConstant request;
	request.capacity = p_capacity;
	request.record_count = p_capacity;
	request.surface_count = p_surfaces;
	request.sequence_low = 0x12345678;
	request.sequence_high = 0xabcdef01;
	return request;
}

static void rejects_unchanged(const Guard::PushConstant &p_request, const Guard::Bindings &p_bindings, const Guard::GeometryProofHeader &p_proof, uint32_t p_groups = 8) {
	Guard::LayoutPlan plan;
	plan.initialize.capacity = 999;
	plan.finalize.sequence_high = 777;
	CHECK(Guard::plan_layout(p_request, p_bindings, p_proof, p_groups, plan) == ERR_INVALID_PARAMETER);
	CHECK(plan.initialize.capacity == 999);
	CHECK(plan.finalize.sequence_high == 777);
	CHECK(plan.uniforms.is_empty());
	CHECK(plan.validation_chunks.is_empty());
}

TEST_CASE("[ExternalOutputGuard] bounded chunks cover every capacity lane once before separate finalize") {
	const auto request = request_for(257, 8);
	Guard::LayoutPlan plan;
	REQUIRE(Guard::plan_layout(request, bindings_for(257, 8), source_header(), 2, plan) == OK);
	REQUIRE(plan.validation_chunks.size() == 3);
	REQUIRE(plan.validation_groups_x.size() == 3);
	CHECK(plan.validation_chunks[0].record_base == 0);
	CHECK(plan.validation_chunks[0].record_count == 128);
	CHECK(plan.validation_chunks[1].record_base == 128);
	CHECK(plan.validation_chunks[1].record_count == 128);
	CHECK(plan.validation_chunks[2].record_base == 256);
	CHECK(plan.validation_chunks[2].record_count == 1);
	CHECK(plan.validation_groups_x[0] == 2);
	CHECK(plan.validation_groups_x[1] == 2);
	CHECK(plan.validation_groups_x[2] == 1);
	CHECK(plan.initialize.record_count == 0);
	CHECK(plan.finalize.record_count == 0);
	CHECK(plan.finalize.sequence_low == 0x12345678);
	CHECK(plan.finalize.sequence_high == 0xabcdef01);
	CHECK(plan.uniforms.size() == 12);
	CHECK(plan.initialize_mode == Guard::INIT);
	CHECK(plan.finalize_mode == Guard::FINALIZE);
	// A caller cannot silently validate a suffix/subset and then finalize.
	auto partial = request;
	partial.record_base = 1;
	rejects_unchanged(partial, bindings_for(257, 8), source_header());
	partial = request;
	partial.record_count = 256;
	rejects_unchanged(partial, bindings_for(257, 8), source_header());
}

TEST_CASE("[ExternalOutputGuard] zero capacity retains actual source validation without dummy records") {
	const auto request = request_for(0);
	const auto bindings = bindings_for(0, 1);
	Guard::LayoutPlan plan;
	REQUIRE(Guard::plan_layout(request, bindings, source_header(), 1, plan) == OK);
	CHECK(plan.validation_chunks.is_empty());
	CHECK(plan.uniforms.size() == 10);
	CHECK(plan.initialize_mode == Guard::INIT_EMPTY);
	CHECK(plan.finalize_mode == Guard::FINALIZE_EMPTY);
	for (const auto &uniform : plan.uniforms) {
		CHECK(uniform.binding >= Guard::OUTPUT_META);
	}
	auto missing_source = bindings;
	missing_source.buffers[Guard::SOURCE_COUNT] = Guard::BufferBinding();
	rejects_unchanged(request, missing_source, source_header());
	auto dummy_record = bindings;
	dummy_record.buffers[Guard::CURRENT_RECORDS].rid = RID::from_uint64(500);
	dummy_record.buffers[Guard::CURRENT_RECORDS].byte_length = 192;
	rejects_unchanged(request, dummy_record, source_header());
}

TEST_CASE("[ExternalOutputGuard] protected bindings remain exclusive while source READ bindings may share") {
	const auto request = request_for(4, 2);
	const auto bindings = bindings_for(4, 2);
	for (uint32_t i = 0; i < Guard::BINDING_COUNT; i++) {
		auto malformed = bindings;
		malformed.buffers[i].rid = RID();
		rejects_unchanged(request, malformed, source_header());
		malformed = bindings;
		malformed.buffers[i].byte_length = UINT64_C(0x100000000);
		rejects_unchanged(request, malformed, source_header());
		malformed = bindings;
		malformed.buffers[i].byte_length--;
		rejects_unchanged(request, malformed, source_header());
		if (i != Guard::SOURCE_COUNT && i < Guard::SOURCE_COUNT_VALIDITY) {
			malformed = bindings;
			malformed.buffers[i].byte_length += 4;
			rejects_unchanged(request, malformed, source_header());
		}
		for (uint32_t j = 0; j < i; j++) {
			malformed = bindings;
			malformed.buffers[i].rid = malformed.buffers[j].rid;
			const bool source_i = i == Guard::SOURCE_COUNT || i >= Guard::SOURCE_COUNT_VALIDITY;
			const bool source_j = j == Guard::SOURCE_COUNT || j >= Guard::SOURCE_COUNT_VALIDITY;
			if (source_i && source_j) {
				// A shared backing has one physical byte length even for different scalar offsets.
				malformed.buffers[i].byte_length = malformed.buffers[j].byte_length = 32;
				Guard::LayoutPlan plan;
				CHECK(Guard::plan_layout(request, malformed, source_header(), 8, plan) == OK);
				malformed.buffers[i].byte_length += 4;
				rejects_unchanged(request, malformed, source_header()); // Same RID cannot have conflicting allocation extents.
			} else {
				rejects_unchanged(request, malformed, source_header());
			}
		}
	}
}

TEST_CASE("[ExternalOutputGuard] source accessor and reserved header fields reject before producing a plan") {
	const auto request = request_for(1);
	const auto bindings = bindings_for(1, 1);
	const auto proof = source_header();
	auto malformed = proof;
	malformed.source_accessor[0] = 4; // First word outside Count's own 16-byte buffer.
	rejects_unchanged(request, bindings, malformed);
	for (int index = 0; index < 4; index++) {
		malformed = proof;
		malformed.source_gate_words[index] = uint32_t(bindings.buffers[Guard::SOURCE_COUNT_VALIDITY + index].byte_length / 4);
		rejects_unchanged(request, bindings, malformed);
		malformed.source_gate_words[index] = UINT32_MAX;
		rejects_unchanged(request, bindings, malformed);
	}
	for (int index : { 1, 2 }) {
		for (uint32_t invalid : { 1u, 2u, UINT32_MAX }) {
			malformed = proof;
			malformed.source_accessor[index] = invalid; // No selectable validity rule/mask.
			rejects_unchanged(request, bindings, malformed);
		}
	}
	for (uint32_t invalid : { 0u, 3u, UINT32_MAX }) {
		malformed = proof;
		malformed.source_accessor[3] = invalid;
		rejects_unchanged(request, bindings, malformed);
	}
	for (int index = 0; index < 4; index++) {
		malformed = proof;
		malformed.options[index] = index == 0 ? 2 : 1;
		rejects_unchanged(request, bindings, malformed);
	}
	for (uint32_t space : { 1u, 2u }) {
		auto supported = proof;
		supported.source_accessor[3] = space;
		supported.options[0] = 1;
		Guard::LayoutPlan plan;
		CHECK(Guard::plan_layout(request, bindings, supported, UINT32_MAX, plan) == OK);
	}
}

TEST_CASE("[ExternalOutputGuard] five source roles retain nonzero offsets and shared backing even at N0") {
	for (uint32_t capacity : { 0u, 3u }) {
		auto bindings = bindings_for(capacity, 1);
		auto proof = source_header();
		const RID shared = RID::from_uint64(900);
		for (uint32_t role : { uint32_t(Guard::SOURCE_COUNT), uint32_t(Guard::SOURCE_COUNT_VALIDITY), uint32_t(Guard::SOURCE_ATTRIBUTES_VALIDITY), uint32_t(Guard::SOURCE_DENSE_IDS_VALIDITY), uint32_t(Guard::SOURCE_INSTANCE_GATE) }) {
			bindings.buffers[role].rid = shared;
			bindings.buffers[role].byte_length = 32;
		}
		Guard::LayoutPlan plan;
		REQUIRE(Guard::plan_layout(request_for(capacity), bindings, proof, 8, plan) == OK);
		CHECK(plan.uniforms.size() == (capacity == 0 ? 10 : 12));
		auto last_word = proof;
		last_word.source_accessor[0] = 7;
		for (uint32_t &word : last_word.source_gate_words) {
			word = 7;
		}
		CHECK(Guard::plan_layout(request_for(capacity), bindings, last_word, 8, plan) == OK);
		last_word.source_accessor[0] = 8;
		rejects_unchanged(request_for(capacity), bindings, last_word);
		// All four semantic gates may resolve to the same actual persistent scalar.
		for (uint32_t &word : proof.source_gate_words) {
			word = 6;
		}
		CHECK(Guard::plan_layout(request_for(capacity), bindings, proof, 8, plan) == OK);
		// One member's larger allocation cannot authorize another member's out-of-range scalar.
		bindings.buffers[Guard::SOURCE_ATTRIBUTES_VALIDITY].rid = RID::from_uint64(901);
		bindings.buffers[Guard::SOURCE_ATTRIBUTES_VALIDITY].byte_length = 24;
		rejects_unchanged(request_for(capacity), bindings, proof);
		for (uint32_t role : { uint32_t(Guard::SOURCE_COUNT), uint32_t(Guard::SOURCE_COUNT_VALIDITY), uint32_t(Guard::SOURCE_ATTRIBUTES_VALIDITY), uint32_t(Guard::SOURCE_DENSE_IDS_VALIDITY), uint32_t(Guard::SOURCE_INSTANCE_GATE) }) {
			auto missing = bindings_for(capacity, 1);
			missing.buffers[role] = Guard::BufferBinding();
			rejects_unchanged(request_for(capacity), missing, source_header());
		}
	}
}

TEST_CASE("[ExternalOutputGuard] invalid shape cannot enlarge dispatch or expose partial plan") {
	const auto valid = request_for(1);
	const auto bindings = bindings_for(1, 1);
	for (uint32_t count : { 0u, 9u, UINT32_MAX }) {
		auto request = valid;
		request.surface_count = count;
		rejects_unchanged(request, bindings, source_header());
	}
	auto request = valid;
	request.capacity = (1u << 20) + 1;
	request.record_count = request.capacity;
	rejects_unchanged(request, bindings, source_header());
	request = valid;
	request.reserved0 = 1;
	rejects_unchanged(request, bindings, source_header());
	request = valid;
	request.reserved1 = 1;
	rejects_unchanged(request, bindings, source_header());
	rejects_unchanged(valid, bindings, source_header(), 0);
}

TEST_CASE("[ExternalOutputGuard] flattened native surfaces exceed mesh RID count with exact protected extents") {
	using Config = RendererRD::ExternalOutputConfig;
	CHECK(Config::MAX_MESH_RIDS == 8);
	CHECK(Config::MAX_FLATTENED_SURFACES == Config::MAX_MESH_RIDS * RenderingServerEnums::MAX_MESH_SURFACES);
	for (uint32_t surfaces : { 9u, Config::MAX_FLATTENED_SURFACES }) {
		for (uint32_t capacity : { 0u, 3u }) {
			const auto request = request_for(capacity, surfaces);
			const auto bindings = bindings_for(capacity, surfaces);
			Guard::LayoutPlan plan;
			REQUIRE(Guard::plan_layout(request, bindings, source_header(), 1, plan) == OK);
			CHECK(plan.initialize.surface_count == surfaces);
			CHECK(plan.finalize.surface_count == surfaces);
			CHECK(plan.initialize_mode == (capacity == 0 ? Guard::INIT_EMPTY : Guard::INIT));
			CHECK(plan.validation_chunks.size() == (capacity == 0 ? 0 : 1));
			for (uint32_t binding : { uint32_t(Guard::PROTECTED_COMMANDS), uint32_t(Guard::GEOMETRY_PROOF) }) {
				auto truncated = bindings;
				truncated.buffers[binding].byte_length -= 4;
				rejects_unchanged(request, truncated, source_header());
			}
		}
	}
	for (uint32_t count : { Config::MAX_FLATTENED_SURFACES + 1, UINT32_MAX / 5 + 1, UINT32_MAX }) {
		// Correct u64 sizes cannot admit a table beyond the native owner bound.
		rejects_unchanged(request_for(0, count), bindings_for(0, count), source_header());
	}
}

} // namespace TestExternalOutputGuard
