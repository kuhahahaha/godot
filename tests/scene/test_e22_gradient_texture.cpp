/**************************************************************************/
/*  test_e22_gradient_texture.cpp                                         */
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
#include "scene/resources/gradient.h"
#include "scene/resources/gradient_texture.h"
#include "scene/resources/resource_snapshot_native.h"
#include "tests/test_macros.h"

#include <array>
#include <cstring>

TEST_FORCE_LINK(test_e22_gradient_texture)

namespace TestE22GradientTexture {

#define E22_GRADIENT_CHECK_OR_RETURN(m_condition) \
	do { \
		const bool e22_gradient_ok = bool(m_condition); \
		CHECK_MESSAGE(e22_gradient_ok, #m_condition); \
		if (!e22_gradient_ok) { \
			return; \
		} \
	} while (false)

constexpr int WIDTH = 33;
constexpr int MAX_BYTES = WIDTH * 4 * sizeof(float);
constexpr uint8_t POISON = 0xa5;
using Pixels = std::array<uint8_t, MAX_BYTES>;

struct ChangeObserver : Object {
	int count = 0;
	void changed() { ++count; }
};

struct Stops {
	Vector<float> offsets;
	Vector<Color> colors;
};

Stops make_stops(int p_kind) {
	Stops stops;
	const int count = p_kind == 0 ? 0 : p_kind == 1 ? 1
			: p_kind == 4							? 4096
			: p_kind == 3							? 5
													: 4;
	stops.offsets.resize(count);
	stops.colors.resize(count);
	const float normal_offsets[] = { 0.85f, 0.0f, 1.0f, 0.25f };
	const float duplicate_offsets[] = { 0.75f, 0.5f, 0.5f, 0.0f, 0.25f };
	for (int i = 0; i < count; ++i) {
		float offset = p_kind == 1 ? 0.5f : p_kind == 4 ? float(count - 1 - i) / float(count - 1)
				: p_kind == 3							? duplicate_offsets[i]
														: normal_offsets[i];
		stops.offsets.write[i] = offset;
		stops.colors.write[i] = Color(float((i * 37 + 7) % 101) / 100.0f,
				float((i * 53 + 19) % 101) / 100.0f,
				float((i * 71 + 31) % 101) / 100.0f,
				float((i * 17 + 37) % 101) / 100.0f);
	}
	return stops;
}

Ref<Gradient> make_gradient(const Stops &p_stops, Gradient::InterpolationMode p_mode, Gradient::ColorSpace p_space) {
	Ref<Gradient> gradient;
	gradient.instantiate();
	gradient->set_offsets(p_stops.offsets);
	gradient->set_colors(p_stops.colors);
	gradient->set_interpolation_mode(p_mode);
	gradient->set_interpolation_color_space(p_space);
	return gradient;
}

bool same_source(const AngeTextureSourceTupleV1 &a, const AngeTextureSourceTupleV1 &b) {
	return a.wrapper_id == b.wrapper_id && a.wrapper_revision == b.wrapper_revision &&
			a.child_id == b.child_id && a.child_revision == b.child_revision &&
			a.source_kind == b.source_kind && a.width == b.width &&
			a.native_mode == b.native_mode && a.child_present == b.child_present;
}

float f32_component(const Pixels &p_pixels, int p_pixel, int p_channel) {
	float value = 0;
	std::memcpy(&value, p_pixels.data() + p_pixel * 16 + p_channel * sizeof(float), sizeof(value));
	return value;
}

// p_kind is empty, single, unsorted, duplicate or 4096 reverse-ordered stops.
// Independent twin starts in the identical raw order, then follows the normal
// public native sampler after capture, preserving native duplicate ordering.
void exercise_case(int p_kind, Gradient::InterpolationMode p_mode, Gradient::ColorSpace p_space, bool p_hdr) {
	CAPTURE(p_kind);
	CAPTURE(int(p_mode));
	CAPTURE(int(p_space));
	CAPTURE(p_hdr);
	const Stops stops = make_stops(p_kind);
	Ref<Gradient> gradient = make_gradient(stops, p_mode, p_space);
	Ref<Gradient> oracle = make_gradient(stops, p_mode, p_space);
	Ref<GradientTexture1D> texture;
	texture.instantiate();
	texture->set_width(WIDTH);
	texture->set_use_hdr(p_hdr);
	texture->set_gradient(gradient);
	ChangeObserver child_changes;
	ChangeObserver wrapper_changes;
	E22_GRADIENT_CHECK_OR_RETURN(gradient->connect("changed", callable_mp(&child_changes, &ChangeObserver::changed)) == OK);
	E22_GRADIENT_CHECK_OR_RETURN(texture->connect("changed", callable_mp(&wrapper_changes, &ChangeObserver::changed)) == OK);
	const Vector<float> source_order = gradient->get_offsets();
	const Vector<Color> source_colors = gradient->get_colors();
	CHECK(source_order == stops.offsets);
	CHECK(source_colors == stops.colors);
	CHECK(oracle->get_offsets() == source_order);
	CHECK(oracle->get_colors() == source_colors);
	if (p_kind >= 2) {
		E22_GRADIENT_CHECK_OR_RETURN(source_order.size() >= 2);
		CHECK(source_order[0] > source_order[source_order.size() - 1]);
	}
	if (p_kind == 3) {
		CHECK(source_order[1] == source_order[2]);
		CHECK(source_colors[1] != source_colors[2]);
	}
	const uint64_t revision = gradient->get_content_revision();
	const uint64_t texture_revision = texture->get_content_revision();
	const uint32_t format = p_hdr ? ANGE_TEXTURE_FORMAT_RGBAF_V1 : ANGE_TEXTURE_FORMAT_RGBA8_V1;
	AngeTextureSnapshotPlanV1 plan = {};
	E22_GRADIENT_CHECK_OR_RETURN(ange_texture_snapshot_plan_v1(texture.ptr(), format, &plan) == ANGE_TEXTURE_OK_V1);
	CHECK(plan.source.source_kind == ANGE_TEXTURE_SOURCE_GRADIENT_1D_V1);
	CHECK(plan.source.native_mode == (p_hdr ? ANGE_TEXTURE_GRADIENT_HDR_V1 : ANGE_TEXTURE_GRADIENT_LDR_V1));
	CHECK(plan.source.child_present == 1);
	CHECK(plan.source.child_revision == revision);
	CHECK(plan.source.wrapper_revision == texture_revision);
	CHECK(plan.point_count == uint32_t(stops.offsets.size()));
	CHECK(plan.real_bytes == sizeof(real_t));
	CHECK(plan.scratch_bytes == 0);
	const size_t output_bytes = WIDTH * (p_hdr ? 16 : 4);
	CHECK(plan.output_bytes == output_bytes);
	// A plan must not perform the later lazy sort.
	CHECK(gradient->get_offsets() == source_order);
	CHECK(gradient->get_colors() == source_colors);
	CHECK(gradient->get_content_revision() == revision);
	Pixels pixels;
	pixels.fill(POISON);
	AngeTextureSnapshotResultV1 result = {};
	MemoryAllocationTest::attempts = 0;
	MemoryAllocationTest::enabled = true;
	const uint32_t status = ange_texture_snapshot_v1(texture.ptr(), &plan, pixels.data(), output_bytes, nullptr, 0, &result);
	MemoryAllocationTest::enabled = false;
	const uint64_t allocations = MemoryAllocationTest::attempts;
	E22_GRADIENT_CHECK_OR_RETURN(status == ANGE_TEXTURE_OK_V1);
	CHECK(result.status == ANGE_TEXTURE_OK_V1);
	CHECK(result.initialized_pixels == WIDTH);
	CHECK(same_source(result.source, plan.source));
	CHECK(allocations == 0);
	CHECK(gradient->get_content_revision() == revision);
	CHECK(texture->get_content_revision() == texture_revision);
	CHECK(child_changes.count == 0);
	CHECK(wrapper_changes.count == 0);
	for (size_t i = output_bytes; i < pixels.size(); ++i) {
		CHECK(pixels[i] == POISON);
	}

	// Trigger the untouched twin's native sort, then compare the complete
	// ordering (including duplicate colors) to the captured source's sort.
	for (int i = 0; i < WIDTH; ++i) {
		const float offset = float(i) / float(WIDTH - 1);
		const Color expected = oracle->get_color_at_offset(offset);
		if (p_hdr) {
			CHECK(f32_component(pixels, i, 0) == float(expected.r));
			CHECK(f32_component(pixels, i, 1) == float(expected.g));
			CHECK(f32_component(pixels, i, 2) == float(expected.b));
			CHECK(f32_component(pixels, i, 3) == float(expected.a));
		} else {
			CHECK(pixels[i * 4 + 0] == uint8_t(expected.get_r8()));
			CHECK(pixels[i * 4 + 1] == uint8_t(expected.get_g8()));
			CHECK(pixels[i * 4 + 2] == uint8_t(expected.get_b8()));
			CHECK(pixels[i * 4 + 3] == uint8_t(expected.get_a8()));
		}
	}
	CHECK(gradient->get_offsets() == oracle->get_offsets());
	CHECK(gradient->get_colors() == oracle->get_colors());
	CHECK(gradient->get_content_revision() == revision);
	CHECK(oracle->get_content_revision() == revision);
	CHECK(child_changes.count == 0);
	CHECK(wrapper_changes.count == 0);
}

TEST_CASE("[E22TextureSnapshot][Gradient] native interpolation, color spaces, sorting and allocation") {
	for (int kind : { 0, 1, 2, 3, 4 }) {
		for (int mode : { int(Gradient::GRADIENT_INTERPOLATE_LINEAR), int(Gradient::GRADIENT_INTERPOLATE_CONSTANT), int(Gradient::GRADIENT_INTERPOLATE_CUBIC) }) {
			for (int space : { int(Gradient::GRADIENT_COLOR_SPACE_SRGB), int(Gradient::GRADIENT_COLOR_SPACE_LINEAR_SRGB), int(Gradient::GRADIENT_COLOR_SPACE_OKLAB) }) {
				for (bool hdr : { false, true }) {
					exercise_case(kind, Gradient::InterpolationMode(mode), Gradient::ColorSpace(space), hdr);
				}
			}
		}
	}
}

TEST_CASE("[E22TextureSnapshot][Gradient] null child is invalid and mutations invalidate the old plan") {
	Ref<GradientTexture1D> texture;
	texture.instantiate();
	texture->set_width(WIDTH);
	AngeTextureSnapshotPlanV1 missing = {};
	CHECK(ange_texture_snapshot_plan_v1(texture.ptr(), ANGE_TEXTURE_FORMAT_RGBA8_V1, &missing) == ANGE_TEXTURE_INVALID_SOURCE_V1);

	const Stops stops = make_stops(2);
	Ref<Gradient> gradient = make_gradient(stops, Gradient::GRADIENT_INTERPOLATE_LINEAR, Gradient::GRADIENT_COLOR_SPACE_SRGB);
	texture->set_gradient(gradient);
	AngeTextureSnapshotPlanV1 old_plan = {};
	E22_GRADIENT_CHECK_OR_RETURN(ange_texture_snapshot_plan_v1(texture.ptr(), ANGE_TEXTURE_FORMAT_RGBA8_V1, &old_plan) == ANGE_TEXTURE_OK_V1);
	Pixels captured;
	captured.fill(POISON);
	AngeTextureSnapshotResultV1 result = {};
	E22_GRADIENT_CHECK_OR_RETURN(ange_texture_snapshot_v1(texture.ptr(), &old_plan, captured.data(), WIDTH * 4, nullptr, 0, &result) == ANGE_TEXTURE_OK_V1);
	CHECK(result.status == ANGE_TEXTURE_OK_V1);
	CHECK(result.initialized_pixels == WIDTH);
	CHECK(same_source(result.source, old_plan.source));
	const Pixels retained = captured;
	const uint64_t revision = gradient->get_content_revision();
	gradient->set_color(0, Color(0.3, 0.4, 0.5, 0.6));
	CHECK(gradient->get_content_revision() > revision);
	Pixels stale;
	stale.fill(POISON);
	AngeTextureSnapshotResultV1 stale_result = {};
	CHECK(ange_texture_snapshot_v1(texture.ptr(), &old_plan, stale.data(), WIDTH * 4, nullptr, 0, &stale_result) == ANGE_TEXTURE_STALE_SOURCE_V1);
	CHECK(stale_result.status == ANGE_TEXTURE_STALE_SOURCE_V1);
	CHECK(stale_result.initialized_pixels == 0);
	for (uint8_t byte : stale) {
		CHECK(byte == POISON);
	}
	CHECK(captured == retained);
	AngeTextureSnapshotPlanV1 fresh_plan = {};
	E22_GRADIENT_CHECK_OR_RETURN(ange_texture_snapshot_plan_v1(texture.ptr(), ANGE_TEXTURE_FORMAT_RGBA8_V1, &fresh_plan) == ANGE_TEXTURE_OK_V1);
	CHECK(fresh_plan.source.child_revision > old_plan.source.child_revision);
	Pixels fresh;
	fresh.fill(POISON);
	AngeTextureSnapshotResultV1 fresh_result = {};
	E22_GRADIENT_CHECK_OR_RETURN(ange_texture_snapshot_v1(texture.ptr(), &fresh_plan, fresh.data(), WIDTH * 4, nullptr, 0, &fresh_result) == ANGE_TEXTURE_OK_V1);
	CHECK(fresh_result.status == ANGE_TEXTURE_OK_V1);
	CHECK(fresh_result.initialized_pixels == WIDTH);
	CHECK(same_source(fresh_result.source, fresh_plan.source));
	CHECK(fresh != captured);
	CHECK(captured == retained);
}

} // namespace TestE22GradientTexture
