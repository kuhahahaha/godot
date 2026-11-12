/**************************************************************************/
/*  test_gdextension_qualification_depth.cpp                              */
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

#include "core/extension/gdextension_qualification_depth.h"
#include "core/extension/gdextension_qualification_rt_depth.h"
#include "tests/test_macros.h"

#include <cstdint>
#include <cstring>
#include <vector>

TEST_FORCE_LINK(test_gdextension_qualification_depth)

// These tests call the shared production validators. They issue no resource,
// source, session, native draw or completion authority.
namespace TestGDExtensionQualificationDepth {
using namespace GDExtensionDeviceExit;
struct Payload {
	std::vector<uint64_t> words;
	uint64_t bytes;
	explicit Payload(uint32_t count, uint64_t push) : words((32 + uint64_t(count) * sizeof(GDExtensionQualificationDepthDrawV1) + push + 7) / 8), bytes(32 + uint64_t(count) * sizeof(GDExtensionQualificationDepthDrawV1) + push) {
		const GDExtensionQualificationTicketRefV1 ticket = { 11, 17 };
		memcpy(data(), &ticket, sizeof(ticket));
		memcpy(data() + 16, &count, sizeof(count));
		memcpy(data() + 24, &push, sizeof(push));
	}
	uint8_t *data() { return reinterpret_cast<uint8_t *>(words.data()); }
};

TEST_CASE("[D32][Qualification] texture shape preserves D32 exact usage and RGBA controls") {
	GDExtensionQualificationTextureCreateV1 q = {};
	q.abi = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
	q.source.session_serial = 1;
	q.source.generation = 2;
	q.library_generation = 3;
	q.create_attempt = 4;
	q.texture_type = 2;
	q.width = 2;
	q.height = 2;
	q.depth = 1;
	q.format = 3;
	q.usage = 5;
	unsigned checks = 0;
	auto check = [&](const GDExtensionQualificationTextureCreateV1 &v, bool expected) {
		++checks;
		const bool accepted = validate_qualification_texture_shape(v) == OK;
		CHECK(accepted == expected);
	};
	check(q, true);
	auto v = q;
	v.width = 256;
	v.height = 128;
	check(v, true);
	for (unsigned usage = 0; usage < 8; ++usage) {
		v = q;
		v.usage = usage;
		check(v, usage == 5);
	}
	for (unsigned type = 0; type < 5; ++type) {
		v = q;
		v.texture_type = type;
		check(v, type == 2);
	}
	v = q;
	v.depth = 2;
	check(v, false);
	v = q;
	v.width = 0;
	check(v, false);
	v = q;
	v.width = 4097;
	check(v, false);
	v = q;
	v.height = 0;
	check(v, false);
	v = q;
	v.height = 4097;
	check(v, false);
	v = q;
	v.width = 4096;
	v.height = 4096;
	check(v, false);
	for (unsigned format = 1; format <= 2; ++format) {
		for (unsigned usage = 1; usage <= 3; ++usage) {
			v = q;
			v.format = format;
			v.usage = usage;
			check(v, true);
		}
	}
	v = q;
	v.format = 0;
	check(v, false);
	v = q;
	v.format = 4;
	check(v, false);
	v = q;
	v.abi.reserved = 1;
	check(v, false);
	v = q;
	v.source.generation = 0;
	check(v, false);
	v = q;
	v.create_attempt = 0;
	check(v, false);
	CHECK(checks == 32);
}
TEST_CASE("[D32][Qualification] depth descriptors bound clear-only scissor and push ranges") {
	GDExtensionQualificationDepthDrawV1 good = {};
	good.abi = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
	good.scissor_width = 16;
	good.scissor_height = 8;
	good.procedural_vertices = 3;
	good.instances = 1;
	good.push_size = 4;
	unsigned checks = 0;
	uint32_t payload[2] = {};
	auto check = [&](const GDExtensionQualificationDepthDrawV1 *d, uint32_t n, const void *p, uint64_t bytes, uint32_t cap, uint64_t pc, Error expected) {
		++checks;
		const Error actual = validate_qualification_depth_draws(d, n, p, bytes, 16, 8, 4, cap, pc);
		CHECK(actual == expected);
	};
	check(nullptr, 0, nullptr, 0, 0, 0, OK);
	check(&good, 1, payload, 4, 1, 4, OK);
	// Counts use admitted storage independently of logical sample/operation spans.
	check(&good, 1, payload, 4, 0, 4, ERR_PARAMETER_RANGE_ERROR);
	check(&good, 1, payload, 4, 1, 3, ERR_PARAMETER_RANGE_ERROR);
	check(nullptr, 1, payload, 4, 1, 4, ERR_INVALID_PARAMETER);
	check(&good, 1, nullptr, 4, 1, 4, ERR_INVALID_PARAMETER);
	auto v = good;
	v.scissor_x = UINT32_MAX;
	check(&v, 1, payload, 4, 1, 4, ERR_INVALID_PARAMETER);
	v = good;
	v.scissor_width = UINT32_MAX;
	check(&v, 1, payload, 4, 1, 4, ERR_INVALID_PARAMETER);
	v = good;
	v.scissor_y = 8;
	check(&v, 1, payload, 4, 1, 4, ERR_INVALID_PARAMETER);
	v = good;
	v.scissor_height = UINT32_MAX;
	check(&v, 1, payload, 4, 1, 4, ERR_INVALID_PARAMETER);
	v = good;
	v.scissor_x = 8;
	v.scissor_width = 8;
	check(&v, 1, payload, 4, 1, 4, OK);
	v = good;
	v.push_offset = UINT64_MAX - 3;
	check(&v, 1, payload, 4, 1, 4, ERR_INVALID_PARAMETER);
	v = good;
	v.push_offset = 4;
	check(&v, 1, payload, 4, 1, 4, ERR_INVALID_PARAMETER);
	check(&v, 1, payload, 8, 1, 8, OK);
	v = good;
	v.push_offset = 1;
	check(&v, 1, payload, 8, 1, 8, ERR_INVALID_PARAMETER);
	v = good;
	v.push_size = 0;
	check(&v, 1, payload, 4, 1, 4, ERR_INVALID_PARAMETER);
	v = good;
	v.abi.struct_size = 0;
	check(&v, 1, payload, 4, 1, 4, ERR_INVALID_PARAMETER);
	v = good;
	v.abi.reserved = 1;
	check(&v, 1, payload, 4, 1, 4, ERR_INVALID_PARAMETER);
	v = good;
	v.reserved = 1;
	check(&v, 1, payload, 4, 1, 4, ERR_INVALID_PARAMETER);
	v = good;
	v.procedural_vertices = 0;
	check(&v, 1, payload, 4, 1, 4, ERR_INVALID_PARAMETER);
	v = good;
	v.instances = 0;
	check(&v, 1, payload, 4, 1, 4, ERR_INVALID_PARAMETER);
	v = good;
	v.scissor_width = 0;
	check(&v, 1, payload, 4, 1, 4, ERR_INVALID_PARAMETER);
	v = good;
	v.scissor_height = 0;
	check(&v, 1, payload, 4, 1, 4, ERR_INVALID_PARAMETER);
	CHECK(checks == 23);
}
TEST_CASE("[D32][Qualification] copied RT payload requires exact bounded aligned layout") {
	QualificationDepthRtPayload out;
	Payload empty(0, 0);
	CHECK(qualification_depth_rt_payload(empty.data(), empty.bytes, out) == OK);
	CHECK((out.ticket.registry_instance == 11 && out.ticket.handle_serial == 17));
	CHECK((!out.draw_count && !out.push_count && !out.draws && !out.push));
	CHECK(qualification_depth_rt_payload(nullptr, 32, out) == ERR_INVALID_PARAMETER);
	CHECK(qualification_depth_rt_payload(empty.data(), 31, out) == ERR_INVALID_PARAMETER);
	Payload full(2, 8);
	CHECK(qualification_depth_rt_payload(full.data(), full.bytes, out) == OK);
	CHECK((out.draw_count == 2 && out.push_count == 8));
	CHECK(reinterpret_cast<const uint8_t *>(out.draws) == full.data() + 32);
	CHECK(out.push == full.data() + 32 + 2 * sizeof(GDExtensionQualificationDepthDrawV1));
	uint64_t zero = 0;
	memcpy(full.data(), &zero, sizeof(zero));
	CHECK(qualification_depth_rt_payload(full.data(), full.bytes, out) == ERR_INVALID_PARAMETER);
	const uint64_t registry = 11;
	memcpy(full.data(), &registry, sizeof(registry));
	memcpy(full.data() + 8, &zero, sizeof(zero));
	CHECK(qualification_depth_rt_payload(full.data(), full.bytes, out) == ERR_INVALID_PARAMETER);
	const uint64_t serial = 17;
	memcpy(full.data() + 8, &serial, sizeof(serial));
	const uint32_t reserved = 1;
	memcpy(full.data() + 20, &reserved, sizeof(reserved));
	CHECK(qualification_depth_rt_payload(full.data(), full.bytes, out) == ERR_INVALID_PARAMETER);
	const uint32_t reserved_zero = 0;
	memcpy(full.data() + 20, &reserved_zero, sizeof(reserved_zero));
	const uint32_t too_many = UINT32_MAX;
	memcpy(full.data() + 16, &too_many, sizeof(too_many));
	CHECK(qualification_depth_rt_payload(full.data(), full.bytes, out) == ERR_INVALID_PARAMETER);
	const uint32_t two = 2;
	memcpy(full.data() + 16, &two, sizeof(two));
	const uint64_t too_much_push = UINT64_MAX;
	memcpy(full.data() + 24, &too_much_push, sizeof(too_much_push));
	CHECK(qualification_depth_rt_payload(full.data(), full.bytes, out) == ERR_INVALID_PARAMETER);
	const uint64_t short_push = 7;
	memcpy(full.data() + 24, &short_push, sizeof(short_push));
	CHECK(qualification_depth_rt_payload(full.data(), full.bytes, out) == ERR_INVALID_PARAMETER);
	Payload trailing(0, 1);
	memcpy(trailing.data() + 24, &zero, sizeof(zero));
	CHECK(qualification_depth_rt_payload(trailing.data(), trailing.bytes, out) == ERR_INVALID_PARAMETER);
	Payload no_push(1, 0);
	CHECK((qualification_depth_rt_payload(no_push.data(), no_push.bytes, out) == OK && !out.push && out.draws));
	std::vector<uint64_t> unaligned((no_push.bytes + 8) / 8);
	auto *shifted = reinterpret_cast<uint8_t *>(unaligned.data()) + 1;
	memcpy(shifted, no_push.data(), no_push.bytes);
	CHECK(qualification_depth_rt_payload(shifted, no_push.bytes, out) == ERR_INVALID_PARAMETER);
	GDExtensionQualificationDepthDrawV1 draw = {};
	draw.abi = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
	draw.scissor_width = draw.scissor_height = 8;
	draw.procedural_vertices = 3;
	draw.instances = 1;
	draw.push_size = 4;
	const uint32_t push = 7;
	CHECK(validate_qualification_depth_draws(&draw, 1, &push, sizeof(push), 8, 8, 4, 1, 4) == OK);
	CHECK(validate_qualification_depth_draws(&draw, 1, &push, sizeof(push), 8, 8, 4, 0, 4) == ERR_PARAMETER_RANGE_ERROR);
	draw.push_offset = UINT64_MAX - 3;
	CHECK(validate_qualification_depth_draws(&draw, 1, &push, sizeof(push), 8, 8, 4, 1, 4) == ERR_INVALID_PARAMETER);
}
} // namespace TestGDExtensionQualificationDepth
