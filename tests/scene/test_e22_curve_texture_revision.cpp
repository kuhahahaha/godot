/**************************************************************************/
/*  test_e22_curve_texture_revision.cpp                                   */
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

#include "core/object/callable_mp.h"
#include "core/os/memory_allocation_test_hooks.h"
#include "scene/resources/curve.h"
#include "scene/resources/curve_texture.h"
#include "scene/resources/gradient_texture.h"
#include "scene/resources/resource_snapshot_native.h"
#include "tests/test_macros.h"

#include <array>
#include <cstring>

TEST_FORCE_LINK(test_e22_curve_texture_revision)

namespace TestE22CurveTextureRevision {

#define E22_CHECK_OR_RETURN(m_condition) \
	do { \
		const bool e22_ok = bool(m_condition); \
		CHECK_MESSAGE(e22_ok, #m_condition); \
		if (!e22_ok) { \
			return; \
		} \
	} while (false)

constexpr int WIDTH = 32;
constexpr int BYTES = WIDTH * 4 * sizeof(float);
constexpr real_t POISON = real_t(12345);
using Pixels = std::array<uint8_t, BYTES>;
using Scratch = std::array<real_t, 1000>;

struct ChangeObserver : Object {
	int count = 0;
	void observe() { ++count; }
};

float component(const Pixels &p_pixels, int p_pixel, int p_channel) {
	float value = 0;
	std::memcpy(&value, p_pixels.data() + p_pixel * 16 + p_channel * sizeof(float), sizeof(value));
	return value;
}

Ref<Curve> three_point_curve() {
	Ref<Curve> curve;
	curve.instantiate();
	curve->add_point(Vector2(0, 0));
	curve->add_point(Vector2(0.5, 1));
	curve->add_point(Vector2(1, 0));
	return curve;
}

Ref<CurveTexture> red_texture(const Ref<Curve> &p_curve) {
	Ref<CurveTexture> texture;
	texture.instantiate();
	texture->set_width(WIDTH);
	texture->set_texture_mode(CurveTexture::TEXTURE_MODE_RED);
	texture->set_curve(p_curve);
	return texture;
}

void check_pixels(const Pixels &p_pixels, const Curve &p_curve) {
	for (int i = 0; i < WIDTH; ++i) {
		const float offset = float(i) / float(WIDTH);
		const float expected = float(p_curve.sample_baked(real_t(offset)));
		CHECK(component(p_pixels, i, 0) == expected);
		CHECK(component(p_pixels, i, 1) == 0.0f);
		CHECK(component(p_pixels, i, 2) == 0.0f);
		CHECK(component(p_pixels, i, 3) == 1.0f);
	}
}

TEST_CASE("[E22TextureSnapshot][Revision] silent bake resolution changes invalidate the old snapshot") {
	Ref<Curve> curve = three_point_curve();
	curve->set_bake_resolution(8);
	Ref<CurveTexture> texture = red_texture(curve);
	ChangeObserver child_changes;
	ChangeObserver wrapper_changes;
	E22_CHECK_OR_RETURN(curve->connect("changed", callable_mp(&child_changes, &ChangeObserver::observe)) == OK);
	E22_CHECK_OR_RETURN(texture->connect("changed", callable_mp(&wrapper_changes, &ChangeObserver::observe)) == OK);

	AngeTextureSnapshotPlanV1 old_plan = {};
	E22_CHECK_OR_RETURN(ange_texture_snapshot_plan_v1(texture.ptr(), ANGE_TEXTURE_FORMAT_RGBAF_V1, &old_plan) == ANGE_TEXTURE_OK_V1);
	CHECK(old_plan.bake_resolution == 8);
	CHECK(old_plan.real_bytes == sizeof(real_t));
	CHECK(old_plan.scratch_bytes == 8 * sizeof(real_t));
	CHECK(old_plan.output_bytes == BYTES);
	Pixels old_pixels = {};
	Scratch scratch;
	scratch.fill(POISON);
	AngeTextureSnapshotResultV1 old_result = {};
	E22_CHECK_OR_RETURN(ange_texture_snapshot_v1(texture.ptr(), &old_plan, old_pixels.data(), old_pixels.size(), scratch.data(), sizeof(scratch), &old_result) == ANGE_TEXTURE_OK_V1);
	CHECK(old_result.status == ANGE_TEXTURE_OK_V1);
	CHECK(old_result.initialized_pixels == WIDTH);
	const Pixels retained = old_pixels;
	const float old_mid = component(old_pixels, 16, 0);
	const int old_child_changes = child_changes.count;
	const int old_wrapper_changes = wrapper_changes.count;

	curve->set_bake_resolution(2);
	CHECK(child_changes.count == old_child_changes);
	CHECK(wrapper_changes.count == old_wrapper_changes);
	AngeTextureSnapshotPlanV1 new_plan = {};
	E22_CHECK_OR_RETURN(ange_texture_snapshot_plan_v1(texture.ptr(), ANGE_TEXTURE_FORMAT_RGBAF_V1, &new_plan) == ANGE_TEXTURE_OK_V1);
	CHECK(new_plan.source.wrapper_id == old_plan.source.wrapper_id);
	CHECK(new_plan.source.wrapper_revision == old_plan.source.wrapper_revision);
	CHECK(new_plan.source.child_id == old_plan.source.child_id);
	CHECK(new_plan.source.child_revision > old_plan.source.child_revision);
	CHECK(new_plan.bake_resolution == 2);
	CHECK(new_plan.scratch_bytes == 2 * sizeof(real_t));

	Pixels stale_pixels;
	stale_pixels.fill(0xa5);
	AngeTextureSnapshotResultV1 stale_result = {};
	const uint32_t stale_status = ange_texture_snapshot_v1(texture.ptr(), &old_plan, stale_pixels.data(), stale_pixels.size(), scratch.data(), sizeof(scratch), &stale_result);
	CHECK(stale_status == ANGE_TEXTURE_STALE_SOURCE_V1);
	CHECK(stale_result.status == ANGE_TEXTURE_STALE_SOURCE_V1);
	CHECK(stale_result.initialized_pixels == 0);
	for (uint8_t byte : stale_pixels) {
		CHECK(byte == 0xa5);
	}

	Pixels new_pixels = {};
	scratch.fill(POISON);
	AngeTextureSnapshotResultV1 new_result = {};
	E22_CHECK_OR_RETURN(ange_texture_snapshot_v1(texture.ptr(), &new_plan, new_pixels.data(), new_pixels.size(), scratch.data(), sizeof(scratch), &new_result) == ANGE_TEXTURE_OK_V1);
	CHECK(new_result.initialized_pixels == WIDTH);
	CHECK(component(new_pixels, 16, 0) != old_mid);
	CHECK(old_pixels == retained);
	check_pixels(new_pixels, *curve.ptr());

	const uint64_t new_revision = curve->get_content_revision();
	curve->bake();
	CHECK(curve->get_content_revision() == new_revision);
	curve->set_bake_resolution(2);
	CHECK(curve->get_content_revision() == new_revision);
	CHECK(child_changes.count == old_child_changes);
	CHECK(wrapper_changes.count == old_wrapper_changes);
}

TEST_CASE("[E22TextureSnapshot][CacheIsolation] dirty resolution uses caller scratch without allocation or cache mutation") {
	Ref<Curve> curve = three_point_curve();
	curve->set_bake_resolution(8);
	curve->bake();
	const real_t warm_cache = curve->_test_get_baked_value(1);
	Ref<CurveTexture> texture = red_texture(curve);
	curve->set_bake_resolution(2);
	AngeTextureSnapshotPlanV1 plan = {};
	E22_CHECK_OR_RETURN(ange_texture_snapshot_plan_v1(texture.ptr(), ANGE_TEXTURE_FORMAT_RGBAF_V1, &plan) == ANGE_TEXTURE_OK_V1);
	CHECK(plan.bake_resolution == 2);
	CHECK(plan.scratch_bytes == 2 * sizeof(real_t));
	Pixels pixels = {};
	alignas(real_t) Scratch scratch;
	scratch.fill(POISON);
	AngeTextureSnapshotResultV1 result = {};
	MemoryAllocationTest::attempts = 0;
	MemoryAllocationTest::enabled = true;
	const uint32_t status = ange_texture_snapshot_v1(texture.ptr(), &plan, pixels.data(), pixels.size(), scratch.data(), sizeof(scratch), &result);
	MemoryAllocationTest::enabled = false;
	const uint64_t allocations = MemoryAllocationTest::attempts;
	E22_CHECK_OR_RETURN(status == ANGE_TEXTURE_OK_V1);
	CHECK(result.initialized_pixels == WIDTH);
	CHECK(allocations == 0);
	CHECK(curve->_test_get_baked_value(1) == warm_cache);
	CHECK(scratch[0] == real_t(0));
	CHECK(scratch[1] == real_t(0));
	CHECK(scratch[2] == POISON);
	check_pixels(pixels, *curve.ptr());
}

TEST_CASE("[E22TextureSnapshot][PoisonedBake] resolutions one two and one thousand fill caller scratch") {
	for (int point_count : { 0, 1, 3 }) {
		for (int resolution : { 1, 2, 1000 }) {
			CAPTURE(point_count);
			CAPTURE(resolution);
			Ref<Curve> curve;
			curve.instantiate();
			if (point_count == 1) {
				curve->add_point(Vector2(0.5, 0.75));
			} else if (point_count == 3) {
				curve->add_point(Vector2(0, 0.25));
				curve->add_point(Vector2(0.5, 1));
				curve->add_point(Vector2(1, 0.75));
			}
			curve->set_bake_resolution(resolution);
			Ref<CurveTexture> texture = red_texture(curve);
			AngeTextureSnapshotPlanV1 plan = {};
			E22_CHECK_OR_RETURN(ange_texture_snapshot_plan_v1(texture.ptr(), ANGE_TEXTURE_FORMAT_RGBAF_V1, &plan) == ANGE_TEXTURE_OK_V1);
			CHECK(plan.bake_resolution == uint32_t(resolution));
			CHECK(plan.scratch_bytes == uint64_t(resolution) * sizeof(real_t));
			Pixels pixels = {};
			Scratch scratch;
			scratch.fill(POISON);
			AngeTextureSnapshotResultV1 result = {};
			E22_CHECK_OR_RETURN(ange_texture_snapshot_v1(texture.ptr(), &plan, pixels.data(), pixels.size(), scratch.data(), sizeof(scratch), &result) == ANGE_TEXTURE_OK_V1);
			CHECK(result.initialized_pixels == WIDTH);
			curve->bake();
			for (int i = 0; i < resolution; ++i) {
				CHECK(scratch[i] != POISON);
				CHECK(scratch[i] == curve->_test_get_baked_value(i));
				if (point_count == 0) {
					CHECK(scratch[i] == real_t(0));
				} else if (point_count == 1) {
					CHECK(scratch[i] == real_t(0.75));
				}
			}
			if (point_count == 3) {
				CHECK(scratch[0] == (resolution == 1 ? real_t(0.75) : real_t(0.25)));
				CHECK(scratch[resolution - 1] == real_t(0.75));
			}
			if (resolution < 1000) {
				CHECK(scratch[resolution] == POISON);
			}
			check_pixels(pixels, *curve.ptr());
		}
	}
}

TEST_CASE("[E22TextureSnapshot][NullChild] Curve emits zero RGB and opaque alpha") {
	for (CurveTexture::TextureMode mode : { CurveTexture::TEXTURE_MODE_RED, CurveTexture::TEXTURE_MODE_RGB }) {
		CAPTURE(int(mode));
		Ref<CurveTexture> texture;
		texture.instantiate();
		texture->set_width(WIDTH);
		texture->set_texture_mode(mode);
		AngeTextureSnapshotPlanV1 plan = {};
		E22_CHECK_OR_RETURN(ange_texture_snapshot_plan_v1(texture.ptr(), ANGE_TEXTURE_FORMAT_RGBAF_V1, &plan) == ANGE_TEXTURE_OK_V1);
		CHECK(plan.source.child_present == 0);
		CHECK(plan.source.child_id == 0);
		CHECK(plan.source.child_revision == 0);
		CHECK(plan.scratch_bytes == 0);
		Pixels pixels;
		pixels.fill(0xa5);
		AngeTextureSnapshotResultV1 result = {};
		E22_CHECK_OR_RETURN(ange_texture_snapshot_v1(texture.ptr(), &plan, pixels.data(), pixels.size(), nullptr, 0, &result) == ANGE_TEXTURE_OK_V1);
		CHECK(result.initialized_pixels == WIDTH);
		for (int i = 0; i < WIDTH; ++i) {
			CHECK(component(pixels, i, 0) == 0.0f);
			CHECK(component(pixels, i, 1) == 0.0f);
			CHECK(component(pixels, i, 2) == 0.0f);
			CHECK(component(pixels, i, 3) == 1.0f);
		}
	}
}

TEST_CASE("[E22TextureSnapshot][NullChild] Gradient is invalid") {
	Ref<GradientTexture1D> gradient;
	gradient.instantiate();
	gradient->set_width(WIDTH);
	AngeTextureSnapshotPlanV1 gradient_plan = {};
	CHECK(ange_texture_snapshot_plan_v1(gradient.ptr(), ANGE_TEXTURE_FORMAT_RGBA8_V1, &gradient_plan) == ANGE_TEXTURE_INVALID_SOURCE_V1);
}

} // namespace TestE22CurveTextureRevision
