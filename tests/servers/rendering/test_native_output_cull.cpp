/**************************************************************************/
/*  test_native_output_cull.cpp                                           */
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

#include "servers/rendering/native_output_cull.h"
#include "tests/test_macros.h"

#include <limits>

TEST_FORCE_LINK(test_native_output_cull)

namespace TestNativeOutputCull {
static constexpr float IDENTITY[12] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };

static void contains(const AABB &outer, const AABB &inner) {
	for (int axis = 0; axis < 3; ++axis) {
		CHECK(double(outer.position[axis]) <= double(inner.position[axis]));
		CHECK(double(outer.position[axis] + outer.size[axis]) >= double(inner.position[axis]) + double(inner.size[axis]));
	}
}

static Transform3D transform(const float *rows) {
	Transform3D result;
	for (int row = 0; row < 3; ++row) {
		for (int column = 0; column < 3; ++column) {
			result.basis[row][column] = rows[row * 4 + column];
		}
		result.origin[row] = rows[row * 4 + 3];
	}
	return result;
}

TEST_CASE("[Rendering][NativeOutputCull] world current exact prior and query share conservative native cull") {
	const AABB current(Vector3(-1, -2, -3), Vector3(2, 4, 6));
	const AABB previous(Vector3(-100, 10, -4), Vector3(3, 5, 8));
	const AABB query(Vector3(20, -40, -1), Vector3(4, 2, 2));
	NativeOutputCull::Envelope result;
	REQUIRE(NativeOutputCull::compose(current, &previous, &query, IDENTITY, IDENTITY, result) == OK);
	contains(result.world, current);
	contains(result.world, previous);
	contains(result.world, query);
	contains(result.local, result.world);
	CHECK(result.world.position.x == -100);
	CHECK(result.world.position.y == -40);
	CHECK(result.world.get_end().x >= 24);
	CHECK(result.world.get_end().y >= 15);
	NativeOutputCull::Envelope reset;
	REQUIRE(NativeOutputCull::compose(current, nullptr, nullptr, IDENTITY, IDENTITY, reset) == OK);
	contains(reset.world, current);
	CHECK(reset.world.get_end() == current.get_end());
	CHECK(reset.world.position.x > previous.position.x);
}

TEST_CASE("[Rendering][NativeOutputCull] translation scale shear and mirror use qualified inverse and native xform") {
	const AABB world(Vector3(-1, -1, -1), Vector3(2, 2, 2));
	const float hosts[][12] = {
		{ 1, 0, 0, 3, 0, 1, 0, -4, 0, 0, 1, 2 },
		{ 2, 0, 0, 0, 0, 4, 0, 0, 0, 0, .5f, 0 },
		{ 1, .5f, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 },
		{ -2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 },
	};
	const float inverses[][12] = {
		{ 1, 0, 0, -3, 0, 1, 0, 4, 0, 0, 1, -2 },
		{ .5f, 0, 0, 0, 0, .25f, 0, 0, 0, 0, 2, 0 },
		{ 1, -.5f, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 },
		{ -.5f, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 },
	};
	const AABB expected[] = {
		AABB(Vector3(-4, 3, -3), Vector3(2, 2, 2)),
		AABB(Vector3(-.5f, -.25f, -2), Vector3(1, .5f, 4)),
		AABB(Vector3(-1.5f, -1, -1), Vector3(3, 2, 2)),
		AABB(Vector3(-.5f, -1, -1), Vector3(1, 2, 2)),
	};
	for (int i = 0; i < 4; ++i) {
		NativeOutputCull::Envelope result;
		REQUIRE(NativeOutputCull::compose(world, nullptr, nullptr, hosts[i], inverses[i], result) == OK);
		contains(result.local, expected[i]);
		contains(transform(hosts[i]).xform(result.local), result.world);
	}
}

TEST_CASE("[Rendering][NativeOutputCull] invalid inputs and inverse preserve previous output atomically") {
	const AABB valid(Vector3(-1, -1, -1), Vector3(2, 2, 2));
	const NativeOutputCull::Envelope sentinel{ AABB(Vector3(7, 8, 9), Vector3(1, 2, 3)), AABB(Vector3(4, 5, 6), Vector3(3, 2, 1)) };
	auto reject = [&](const AABB &current, const AABB *prior, const float *host, const float *inverse) {
		auto out = sentinel;
		CHECK(NativeOutputCull::compose(current, prior, nullptr, host, inverse, out) != OK);
		CHECK(out.world == sentinel.world);
		CHECK(out.local == sentinel.local);
	};
	reject(AABB(Vector3(), Vector3(0, 1, 1)), nullptr, IDENTITY, IDENTITY);
	reject(AABB(Vector3(), Vector3(-1, 1, 1)), nullptr, IDENTITY, IDENTITY);
	AABB invalid = valid;
	invalid.position.x = std::numeric_limits<float>::infinity();
	reject(invalid, nullptr, IDENTITY, IDENTITY);
	invalid = valid;
	invalid.size.y = std::numeric_limits<float>::quiet_NaN();
	reject(valid, &invalid, IDENTITY, IDENTITY);
	{
		auto out = sentinel;
		CHECK(NativeOutputCull::compose(valid, nullptr, &invalid, IDENTITY, IDENTITY, out) != OK);
		CHECK(out.world == sentinel.world);
		CHECK(out.local == sentinel.local);
	}
	reject(valid, nullptr, nullptr, IDENTITY);
	reject(valid, nullptr, IDENTITY, nullptr);
	float wrong[12];
	for (int i = 0; i < 12; ++i) { wrong[i] = IDENTITY[i]; }
	wrong[0] = 2; // An oversized wrong inverse must not pass merely by enclosing.
	reject(valid, nullptr, IDENTITY, wrong);
	wrong[0] = 1;
	wrong[3] = 100;
	reject(valid, nullptr, IDENTITY, wrong);
	wrong[3] = 0;
	wrong[5] = 0; // Singular host.
	reject(valid, nullptr, wrong, IDENTITY);
	const float dependent[12] = { 1e10f, 1e10f, 0, 0, 1e10f, 1e10f, 0, 0, 0, 0, 1, 0 };
	reject(valid, nullptr, dependent, IDENTITY);
	wrong[5] = std::numeric_limits<float>::infinity();
	reject(valid, nullptr, wrong, IDENTITY);
	const float largest = std::numeric_limits<float>::max();
	reject(AABB(Vector3(largest, 0, 0), Vector3(largest, 1, 1)), nullptr, IDENTITY, IDENTITY);
	const AABB far(Vector3(largest / 2, 0, 0), Vector3(largest / 4, 1, 1));
	reject(AABB(Vector3(-largest, 0, 0), Vector3(largest / 4, 1, 1)), &far, IDENTITY, IDENTITY);
}

TEST_CASE("[Rendering][NativeOutputCull] finite f32 extremes and cancellation retain containment") {
	const float large = 0x1p100f;
	const float unit = 0x1p80f;
	const AABB world(Vector3(large, -large, 0), Vector3(unit, unit, 0x1p-120f));
	NativeOutputCull::Envelope result;
	REQUIRE(NativeOutputCull::compose(world, nullptr, nullptr, IDENTITY, IDENTITY, result) == OK);
	contains(result.world, world);
	contains(transform(IDENTITY).xform(result.local), result.world);
	const float host[12] = { 1, 0, 0, large, 0, 1, 0, -large, 0, 0, 1, 0 };
	const float inverse[12] = { 1, 0, 0, -large, 0, 1, 0, large, 0, 0, 1, 0 };
	REQUIRE(NativeOutputCull::compose(world, nullptr, nullptr, host, inverse, result) == OK);
	contains(transform(host).xform(result.local), result.world);
}

TEST_CASE("[Rendering][NativeOutputCull] Local declarations use each own Host exactly once") {
	const AABB declaration(Vector3(-1, -1, -1), Vector3(2, 2, 2));
	const float current_host[12] = { -2, .5f, 0, 10, 0, 1, 0, 0, 0, 0, 1, 0 };
	const float current_inverse[12] = { -.5f, .25f, 0, 5, 0, 1, 0, 0, 0, 0, 1, 0 };
	const float prior_host[12] = { 1, 0, 0, -20, 0, -1, 0, 3, 0, 0, 1, 0 };
	AABB current_world;
	AABB prior_world;
	REQUIRE(NativeOutputCull::transform_bounds(declaration, current_host, current_world) == OK);
	REQUIRE(NativeOutputCull::transform_bounds(declaration, prior_host, prior_world) == OK);
	contains(current_world, AABB(Vector3(7.5f, -1, -1), Vector3(5, 2, 2)));
	contains(prior_world, AABB(Vector3(-21, 2, -1), Vector3(2, 2, 2)));
	CHECK(current_world.position.x > 7);
	CHECK(current_world.get_end().x < 13);
	CHECK(prior_world.get_end().x < -18);
	NativeOutputCull::Envelope combined;
	REQUIRE(NativeOutputCull::compose(current_world, &prior_world, nullptr, current_host, current_inverse, combined) == OK);
	contains(combined.world, current_world);
	contains(combined.world, prior_world);
	contains(transform(current_host).xform(combined.local), combined.world);
	// Counterexample: applying current Host to the old declaration loses the
	// actual prior extent. Equal declaration numbers do not mean equal Host.
	CHECK(current_world.position.x > prior_world.get_end().x);
}

TEST_CASE("[Rendering][NativeOutputCull] Local outward conversion rejects invalid input without changing output") {
	const AABB declaration(Vector3(-1, -1, -1), Vector3(2, 2, 2));
	const AABB sentinel(Vector3(4, 5, 6), Vector3(1, 2, 3));
	AABB out = sentinel;
	CHECK(NativeOutputCull::transform_bounds(declaration, nullptr, out) != OK);
	CHECK(out == sentinel);
	const AABB empty(Vector3(), Vector3(0, 1, 1));
	CHECK(NativeOutputCull::transform_bounds(empty, IDENTITY, out) != OK);
	CHECK(out == sentinel);
	float nonfinite[12];
	for (int i = 0; i < 12; ++i) { nonfinite[i] = IDENTITY[i]; }
	nonfinite[3] = std::numeric_limits<float>::infinity();
	CHECK(NativeOutputCull::transform_bounds(declaration, nonfinite, out) != OK);
	CHECK(out == sentinel);
	const float overflow[12] = { std::numeric_limits<float>::max(), 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };
	CHECK(NativeOutputCull::transform_bounds(AABB(Vector3(2, -1, -1), Vector3(2, 2, 2)), overflow, out) != OK);
	CHECK(out == sentinel);
}
} // namespace TestNativeOutputCull
