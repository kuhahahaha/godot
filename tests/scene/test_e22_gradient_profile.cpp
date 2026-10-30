/**************************************************************************/
/*  test_e22_gradient_profile.cpp                                         */
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
#include <limits>

TEST_FORCE_LINK(test_e22_gradient_profile)

namespace TestE22GradientProfile {

#define E22_PROFILE_CHECK_OR_RETURN(m_condition) \
	do { \
		const bool e22_profile_ok = bool(m_condition); \
		CHECK_MESSAGE(e22_profile_ok, #m_condition); \
		if (!e22_profile_ok) { \
			return; \
		} \
	} while (false)

constexpr int WIDTH = 33;
constexpr int MAX_OUTPUT = WIDTH * 16;
using Pixels = std::array<uint8_t, MAX_OUTPUT>;

struct Observer : Object {
	int changes = 0;
	void changed() { ++changes; }
};

// Scoped handler records unexpected diagnostics, without logging, assertions,
// or callbacks of its own. Print suppression does not bypass error handlers.
struct Diagnostics {
	ErrorHandlerList handler = {};
	bool previous_print = false;
	int calls = 0;
	static void on_error(void *p_user, const char *, const char *, int, const char *, const char *, bool, ErrorHandlerType) {
		++static_cast<Diagnostics *>(p_user)->calls;
	}
	Diagnostics() {
		previous_print = CoreGlobals::print_error_enabled;
		CoreGlobals::print_error_enabled = false;
		handler.errfunc = on_error;
		handler.userdata = this;
		add_error_handler(&handler);
	}
	~Diagnostics() {
		remove_error_handler(&handler);
		CoreGlobals::print_error_enabled = previous_print;
	}
};

float component(const Pixels &p_pixels, int p_pixel, int p_component) {
	float value = 0;
	std::memcpy(&value, p_pixels.data() + p_pixel * 16 + p_component * sizeof(float), sizeof(value));
	return value;
}

bool same_float_bits(float a, float b) {
	return std::memcmp(&a, &b, sizeof(float)) == 0;
}

bool same_raw_stops(const Vector<float> &a, const Vector<Color> &ac, const Vector<float> &b, const Vector<Color> &bc) {
	if (a.size() != b.size() || ac.size() != bc.size() || a.size() != ac.size()) {
		return false;
	}
	for (int i = 0; i < a.size(); ++i) {
		if (!same_float_bits(a[i], b[i]) || !same_float_bits(ac[i].r, bc[i].r) ||
				!same_float_bits(ac[i].g, bc[i].g) || !same_float_bits(ac[i].b, bc[i].b) || !same_float_bits(ac[i].a, bc[i].a)) {
			return false;
		}
	}
	return true;
}

Ref<Gradient> make_gradient(const Vector<float> &p_offsets, const Vector<Color> &p_colors) {
	Ref<Gradient> gradient;
	gradient.instantiate();
	gradient->set_offsets(p_offsets);
	gradient->set_colors(p_colors);
	return gradient;
}

Ref<GradientTexture1D> make_texture(const Ref<Gradient> &p_gradient, bool p_hdr) {
	Ref<GradientTexture1D> texture;
	texture.instantiate();
	texture->set_width(WIDTH);
	texture->set_use_hdr(p_hdr);
	texture->set_gradient(p_gradient);
	return texture;
}

TEST_CASE("[E22TextureSnapshot][GradientProfile] finite HDR range survives and LDR uses native clamp") {
	Vector<float> offsets;
	offsets.push_back(0.0f);
	offsets.push_back(1.0f);
	Vector<Color> colors;
	colors.push_back(Color(-0.25f, 1.5f, 0.5f, 2.0f));
	colors.push_back(Color(1.25f, -0.5f, 0.25f, -0.25f));
	for (bool hdr : { false, true }) {
		CAPTURE(hdr);
		Ref<Gradient> gradient = make_gradient(offsets, colors);
		Ref<Gradient> oracle = make_gradient(offsets, colors);
		Ref<GradientTexture1D> texture = make_texture(gradient, hdr);
		AngeTextureSnapshotPlanV1 plan = {};
		const uint32_t format = hdr ? ANGE_TEXTURE_FORMAT_RGBAF_V1 : ANGE_TEXTURE_FORMAT_RGBA8_V1;
		E22_PROFILE_CHECK_OR_RETURN(ange_texture_snapshot_plan_v1(texture.ptr(), format, &plan) == ANGE_TEXTURE_OK_V1);
		CHECK(plan.output_bytes == uint64_t(WIDTH * (hdr ? 16 : 4)));
		CHECK(plan.scratch_bytes == 0);
		Pixels pixels;
		pixels.fill(0xa5);
		AngeTextureSnapshotResultV1 result;
		std::memset(&result, 0xa5, sizeof(result));
		E22_PROFILE_CHECK_OR_RETURN(ange_texture_snapshot_v1(texture.ptr(), &plan, pixels.data(), plan.output_bytes, nullptr, 0, &result) == ANGE_TEXTURE_OK_V1);
		CHECK(result.status == ANGE_TEXTURE_OK_V1);
		CHECK(result.initialized_pixels == WIDTH);
		for (int i = 0; i < WIDTH; ++i) {
			const Color expected = oracle->get_color_at_offset(float(i) / float(WIDTH - 1));
			if (hdr) {
				CHECK(component(pixels, i, 0) == float(expected.r));
				CHECK(component(pixels, i, 1) == float(expected.g));
				CHECK(component(pixels, i, 2) == float(expected.b));
				CHECK(component(pixels, i, 3) == float(expected.a));
			} else {
				CHECK(pixels[i * 4] == uint8_t(expected.get_r8()));
				CHECK(pixels[i * 4 + 1] == uint8_t(expected.get_g8()));
				CHECK(pixels[i * 4 + 2] == uint8_t(expected.get_b8()));
				CHECK(pixels[i * 4 + 3] == uint8_t(expected.get_a8()));
			}
		}
		if (hdr) {
			CHECK(component(pixels, 0, 0) == -0.25f);
			CHECK(component(pixels, 0, 1) == 1.5f);
			CHECK(component(pixels, 0, 3) == 2.0f);
		} else {
			CHECK(pixels[0] == 0);
			CHECK(pixels[1] == 255);
			CHECK(pixels[3] == 255);
		}
	}
}

TEST_CASE("[E22TextureSnapshot][GradientProfile] 4097 stops fail limit before sorting or output") {
	Vector<float> offsets;
	Vector<Color> colors;
	offsets.resize(4097);
	colors.resize(4097);
	for (int i = 0; i < 4097; ++i) {
		offsets.write[i] = float(4096 - i) / 4096.0f;
		colors.write[i] = Color(0.1, 0.2, 0.3, 1.0);
	}
	Ref<Gradient> gradient = make_gradient(offsets, colors);
	Ref<GradientTexture1D> texture = make_texture(gradient, false);
	const uint64_t revision = gradient->get_content_revision();
	AngeTextureSnapshotPlanV1 plan = {};
	CHECK(ange_texture_snapshot_plan_v1(texture.ptr(), ANGE_TEXTURE_FORMAT_RGBA8_V1, &plan) == ANGE_TEXTURE_LIMIT_V1);
	CHECK(gradient->get_content_revision() == revision);
	CHECK(same_raw_stops(offsets, colors, gradient->get_offsets(), gradient->get_colors()));
}

// A valid plan can describe unsorted numeric inputs; snapshot must reject
// nonfinite source before any native sort, callbacks or output writes.
void invalid_numeric_case(int p_component, float p_bad, bool p_hdr) {
	CAPTURE(p_component);
	CAPTURE(p_bad);
	CAPTURE(p_hdr);
	Vector<float> offsets;
	offsets.push_back(0.8f);
	offsets.push_back(0.2f);
	offsets.push_back(0.5f);
	Vector<Color> colors;
	colors.push_back(Color(0.1, 0.2, 0.3, 1));
	colors.push_back(Color(0.4, 0.5, 0.6, 1));
	colors.push_back(Color(0.7, 0.8, 0.9, 1));
	if (p_component == -1) {
		offsets.write[1] = p_bad;
	} else {
		Color &color = colors.write[1];
		switch (p_component) {
			case 0:
				color.r = p_bad;
				break;
			case 1:
				color.g = p_bad;
				break;
			case 2:
				color.b = p_bad;
				break;
			case 3:
				color.a = p_bad;
				break;
		}
	}
	Ref<Gradient> gradient = make_gradient(offsets, colors);
	Ref<GradientTexture1D> texture = make_texture(gradient, p_hdr);
	Observer child_observer;
	Observer wrapper_observer;
	E22_PROFILE_CHECK_OR_RETURN(gradient->connect("changed", callable_mp(&child_observer, &Observer::changed)) == OK);
	E22_PROFILE_CHECK_OR_RETURN(texture->connect("changed", callable_mp(&wrapper_observer, &Observer::changed)) == OK);
	const uint64_t revision = gradient->get_content_revision();
	const uint64_t wrapper_revision = texture->get_content_revision();
	AngeTextureSnapshotPlanV1 plan = {};
	const uint32_t format = p_hdr ? ANGE_TEXTURE_FORMAT_RGBAF_V1 : ANGE_TEXTURE_FORMAT_RGBA8_V1;
	E22_PROFILE_CHECK_OR_RETURN(ange_texture_snapshot_plan_v1(texture.ptr(), format, &plan) == ANGE_TEXTURE_OK_V1);
	CHECK(same_raw_stops(offsets, colors, gradient->get_offsets(), gradient->get_colors()));
	Pixels output;
	output.fill(0xa5);
	AngeTextureSnapshotResultV1 result;
	std::memset(&result, 0xa5, sizeof(result));
	uint32_t status = ANGE_TEXTURE_OK_V1;
	uint64_t allocations = 0;
	int diagnostics = 0;
	{
		Diagnostics observer;
		MemoryAllocationTest::attempts = 0;
		MemoryAllocationTest::enabled = true;
		status = ange_texture_snapshot_v1(texture.ptr(), &plan, output.data(), plan.output_bytes, nullptr, 0, &result);
		MemoryAllocationTest::enabled = false;
		allocations = MemoryAllocationTest::attempts;
		diagnostics = observer.calls;
	}
	CHECK(status == ANGE_TEXTURE_INVALID_SOURCE_V1);
	CHECK(result.status == ANGE_TEXTURE_INVALID_SOURCE_V1);
	CHECK(result.initialized_pixels == 0);
	const AngeTextureSourceTupleV1 zero_source = {};
	CHECK(std::memcmp(&result.source, &zero_source, sizeof(zero_source)) == 0);
	CHECK(allocations == 0);
	CHECK(diagnostics == 0);
	CHECK(child_observer.changes == 0);
	CHECK(wrapper_observer.changes == 0);
	CHECK(gradient->get_content_revision() == revision);
	CHECK(texture->get_content_revision() == wrapper_revision);
	CHECK(same_raw_stops(offsets, colors, gradient->get_offsets(), gradient->get_colors()));
	for (uint8_t b : output) {
		CHECK(b == 0xa5);
	}
}

TEST_CASE("[E22TextureSnapshot][GradientProfile] nonfinite offsets or channels reject before sorting") {
	const float values[] = { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() };
	for (int component_index : { -1, 0, 1, 2, 3 }) {
		for (float value : values) {
			for (bool hdr : { false, true }) {
				invalid_numeric_case(component_index, value, hdr);
			}
		}
	}
}

// Enum value 3 is in the C++ enum's representable range, but is not a
// declared mode or color space. Use the real public setter, not forged memory.
void invalid_enum_case(bool p_invalid_mode, bool p_hdr) {
	CAPTURE(p_invalid_mode);
	CAPTURE(p_hdr);
	Vector<float> offsets;
	offsets.push_back(1.0f);
	offsets.push_back(0.0f);
	Vector<Color> colors;
	colors.push_back(Color(0.7, 0.2, 0.3, 1));
	colors.push_back(Color(0.1, 0.4, 0.6, 1));
	Ref<Gradient> gradient = make_gradient(offsets, colors);
	Ref<GradientTexture1D> texture = make_texture(gradient, p_hdr);
	const uint64_t before_set = gradient->get_content_revision();
	ERR_PRINT_OFF;
	if (p_invalid_mode) {
		gradient->set_interpolation_mode(Gradient::InterpolationMode(3));
	} else {
		gradient->set_interpolation_color_space(Gradient::ColorSpace(3));
	}
	ERR_PRINT_ON;
	const bool setter_accepted = p_invalid_mode ? int(gradient->get_interpolation_mode()) == 3 : int(gradient->get_interpolation_color_space()) == 3;
	if (!setter_accepted) {
		CHECK(gradient->get_content_revision() == before_set);
		return;
	}
	CHECK(gradient->get_content_revision() > before_set);
	AngeTextureSnapshotPlanV1 plan;
	std::memset(&plan, 0xa5, sizeof(plan));
	const uint32_t format = p_hdr ? ANGE_TEXTURE_FORMAT_RGBAF_V1 : ANGE_TEXTURE_FORMAT_RGBA8_V1;
	const uint32_t plan_status = ange_texture_snapshot_plan_v1(texture.ptr(), format, &plan);
	if (plan_status != ANGE_TEXTURE_OK_V1) {
		CHECK(plan_status == ANGE_TEXTURE_INVALID_SOURCE_V1);
		const AngeTextureSnapshotPlanV1 zero_plan = {};
		CHECK(std::memcmp(&plan, &zero_plan, sizeof(plan)) == 0);
		return;
	}
	Pixels output;
	output.fill(0xa5);
	AngeTextureSnapshotResultV1 result;
	std::memset(&result, 0xa5, sizeof(result));
	const uint32_t status = ange_texture_snapshot_v1(texture.ptr(), &plan, output.data(), plan.output_bytes, nullptr, 0, &result);
	CHECK(status == ANGE_TEXTURE_INVALID_SOURCE_V1);
	CHECK(result.status == ANGE_TEXTURE_INVALID_SOURCE_V1);
	CHECK(result.initialized_pixels == 0);
	const AngeTextureSourceTupleV1 zero_source = {};
	CHECK(std::memcmp(&result.source, &zero_source, sizeof(zero_source)) == 0);
	CHECK(same_raw_stops(offsets, colors, gradient->get_offsets(), gradient->get_colors()));
	for (uint8_t byte : output) {
		CHECK(byte == 0xa5);
	}
}

TEST_CASE("[E22TextureSnapshot][GradientProfile] invalid source enums cannot become native fallback") {
	for (bool invalid_mode : { false, true }) {
		for (bool hdr : { false, true }) {
			invalid_enum_case(invalid_mode, hdr);
		}
	}
}

} // namespace TestE22GradientProfile
