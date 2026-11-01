/**************************************************************************/
/*  test_e22_curve_mutation.cpp                                           */
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
#include "scene/resources/curve.h"
#include "scene/resources/curve_texture.h"
#include "scene/resources/resource_snapshot_native.h"
#include "tests/test_macros.h"

#include <array>
#include <cstring>

TEST_FORCE_LINK(test_e22_curve_mutation)

namespace TestE22CurveMutation {

#define E22_MUTATION_CHECK_OR_RETURN(m_condition) \
	do { \
		const bool e22_ok = bool(m_condition); \
		CHECK_MESSAGE(e22_ok, #m_condition); \
		if (!e22_ok) { \
			return; \
		} \
	} while (false)

struct Event {
	char kind = 0;
	int points = 0;
	uint64_t revision = 0;
	Vector2 last;
	Vector2 middle;
	real_t middle_left_tangent = 0;
	real_t middle_right_tangent = 0;
};

struct Trace : Object {
	Curve *curve = nullptr;
	std::array<Event, 64> events = {};
	int count = 0;
	bool overflow = false;

	void record(char p_kind) {
		if (count >= int(events.size())) {
			overflow = true;
			return;
		}
		Event &event = events[count++];
		event.kind = p_kind;
		event.points = curve->get_point_count();
		event.revision = curve->get_content_revision();
		if (event.points) {
			event.last = curve->get_point_position(event.points - 1);
			const int middle = event.points / 2;
			event.middle = curve->get_point_position(middle);
			event.middle_left_tangent = curve->get_point_left_tangent(middle);
			event.middle_right_tangent = curve->get_point_right_tangent(middle);
		}
	}
	void changed() { record('C'); }
	void property() { record('P'); }
	void range() { record('R'); }
	void domain() { record('D'); }
	bool connect_curve(Curve *p_curve) {
		curve = p_curve;
		return curve->connect("changed", callable_mp(this, &Trace::changed)) == OK &&
				curve->connect("property_list_changed", callable_mp(this, &Trace::property)) == OK &&
				curve->connect("range_changed", callable_mp(this, &Trace::range)) == OK &&
				curve->connect("domain_changed", callable_mp(this, &Trace::domain)) == OK;
	}
	void clear() {
		count = 0;
		overflow = false;
	}
};

Array limits(real_t p_low, real_t p_high, real_t p_domain_low, real_t p_domain_high) {
	Array a;
	a.push_back(p_low);
	a.push_back(p_high);
	a.push_back(p_domain_low);
	a.push_back(p_domain_high);
	return a;
}

// Returns the red channel at pixel 4, corresponding to native offset 4/32.
bool snapshot_pixel(CurveTexture *p_texture, float &r_value) {
	AngeTextureSnapshotPlanV1 plan = {};
	if (ange_texture_snapshot_plan_v1(p_texture, ANGE_TEXTURE_FORMAT_RGBAF_V1, &plan) != ANGE_TEXTURE_OK_V1 || plan.output_bytes != 32 * 16 || plan.scratch_bytes > 1000 * sizeof(real_t)) {
		return false;
	}
	std::array<uint8_t, 32 * 16> pixels = {};
	std::array<real_t, 1000> scratch = {};
	AngeTextureSnapshotResultV1 result = {};
	if (ange_texture_snapshot_v1(p_texture, &plan, pixels.data(), pixels.size(), scratch.data(), sizeof(scratch), &result) != ANGE_TEXTURE_OK_V1 || result.initialized_pixels != 32) {
		return false;
	}
	std::memcpy(&r_value, pixels.data() + 4 * 16, sizeof(r_value));
	return true;
}

TEST_CASE("[E22TextureSnapshot][Mutation] set_limits updates a warm cache without adding notifications") {
	Ref<Curve> curve;
	curve.instantiate();
	curve->add_point(Vector2(0, 0));
	curve->add_point(Vector2(1, 1));
	curve->set_bake_resolution(8);
	curve->bake();
	const real_t original = curve->sample_baked(real_t(0.125));
	Ref<CurveTexture> texture;
	texture.instantiate();
	texture->set_width(32);
	texture->set_texture_mode(CurveTexture::TEXTURE_MODE_RED);
	texture->set_curve(curve);
	Trace trace;
	E22_MUTATION_CHECK_OR_RETURN(trace.connect_curve(curve.ptr()));
	const uint64_t before = curve->get_content_revision();
	const Array extended = limits(-1, 2, -2, 2);
	curve->set_limits(extended);
	CHECK(trace.count == 0);
	CHECK(curve->get_content_revision() > before);
	const real_t changed = curve->sample_baked(real_t(0.125));
	float captured = -1;
	E22_MUTATION_CHECK_OR_RETURN(snapshot_pixel(texture.ptr(), captured));
	CHECK(float(changed) == captured);
	curve->bake();
	CHECK(changed == curve->sample_baked(real_t(0.125)));
	CHECK(changed != original);
	const uint64_t extended_revision = curve->get_content_revision();
	curve->set_limits(extended);
	CHECK(curve->get_content_revision() == extended_revision);
	CHECK(trace.count == 0);

	ERR_PRINT_OFF;
	curve->set_limits(Array());
	ERR_PRINT_ON;
	CHECK(trace.count == 0);
	CHECK(curve->get_content_revision() > extended_revision);
	CHECK(curve->get_min_domain() == 0);
	CHECK(curve->get_max_domain() == 1);
	const real_t restored = curve->sample_baked(real_t(0.125));
	E22_MUTATION_CHECK_OR_RETURN(snapshot_pixel(texture.ptr(), captured));
	CHECK(float(restored) == captured);
	curve->bake();
	CHECK(restored == curve->sample_baked(real_t(0.125)));
	CHECK(restored == original);
	const uint64_t defaults_revision = curve->get_content_revision();
	ERR_PRINT_OFF;
	curve->set_limits(Array());
	ERR_PRINT_ON;
	CHECK(curve->get_content_revision() == defaults_revision);
	CHECK(trace.count == 0);
}

// The error callback only observes the real source; it does not assert, log,
// alter handler registration, or enter another error path.
struct ReentrantWarning {
	Curve *curve = nullptr;
	CurveTexture *texture = nullptr;
	ErrorHandlerList handler = {};
	AngeTextureSnapshotPlanV1 plan = {};
	uint32_t plan_status = ANGE_TEXTURE_BAD_ARGUMENT_V1;
	real_t sample = 0;
	int warnings = 0;
	int other_errors = 0;

	static void on_error(void *p_userdata, const char *, const char *, int, const char *, const char *, bool, ErrorHandlerType p_type) {
		ReentrantWarning *self = static_cast<ReentrantWarning *>(p_userdata);
		if (p_type != ERR_HANDLER_WARNING) {
			++self->other_errors;
			return;
		}
		++self->warnings;
		if (self->warnings == 1) {
			self->plan_status = ange_texture_snapshot_plan_v1(self->texture, ANGE_TEXTURE_FORMAT_RGBAF_V1, &self->plan);
			self->sample = self->curve->sample_baked(real_t(0.125));
		}
	}
	ReentrantWarning(Curve *p_curve, CurveTexture *p_texture) : curve(p_curve), texture(p_texture) {
		handler.errfunc = on_error;
		handler.userdata = this;
		add_error_handler(&handler);
	}
	~ReentrantWarning() { remove_error_handler(&handler); }
};

TEST_CASE("[E22TextureSnapshot][Mutation] fallback warning reentry cannot preserve an old plan or bake") {
	Ref<Curve> curve;
	curve.instantiate();
	curve->add_point(Vector2(0, 0));
	curve->add_point(Vector2(1, 1));
	curve->set_bake_resolution(8);
	curve->bake();
	curve->set_limits(limits(-1, 2, -2, 2)); // Leave the nondefault bake dirty for the callback.
	Ref<CurveTexture> texture;
	texture.instantiate();
	texture->set_width(32);
	texture->set_texture_mode(CurveTexture::TEXTURE_MODE_RED);
	texture->set_curve(curve);
	Trace trace;
	E22_MUTATION_CHECK_OR_RETURN(trace.connect_curve(curve.ptr()));
	AngeTextureSnapshotPlanV1 reentrant_plan = {};
	uint32_t reentrant_status = ANGE_TEXTURE_BAD_ARGUMENT_V1;
	real_t reentrant_sample = 0;
	int warnings = 0;
	int other_errors = 0;
	{
		ReentrantWarning observer(curve.ptr(), texture.ptr());
		const bool previous_print_setting = CoreGlobals::print_error_enabled;
		CoreGlobals::print_error_enabled = false;
		curve->set_limits(Array());
		CoreGlobals::print_error_enabled = previous_print_setting;
		reentrant_plan = observer.plan;
		reentrant_status = observer.plan_status;
		reentrant_sample = observer.sample;
		warnings = observer.warnings;
		other_errors = observer.other_errors;
	}
	CHECK(warnings == 1);
	CHECK(other_errors == 0);
	E22_MUTATION_CHECK_OR_RETURN(reentrant_status == ANGE_TEXTURE_OK_V1);
	CHECK(curve->get_min_domain() == 0);
	CHECK(curve->get_max_domain() == 1);
	CHECK(curve->get_content_revision() > reentrant_plan.source.child_revision);
	CHECK(trace.count == 0);
	std::array<uint8_t, 32 * 16> stale_output;
	stale_output.fill(0xa5);
	std::array<real_t, 1000> scratch = {};
	AngeTextureSnapshotResultV1 stale_result = {};
	const uint32_t stale = ange_texture_snapshot_v1(texture.ptr(), &reentrant_plan, stale_output.data(), stale_output.size(), scratch.data(), sizeof(scratch), &stale_result);
	CHECK(stale == ANGE_TEXTURE_STALE_SOURCE_V1);
	CHECK(stale_result.status == ANGE_TEXTURE_STALE_SOURCE_V1);
	CHECK(stale_result.initialized_pixels == 0);
	for (uint8_t byte : stale_output) {
		CHECK(byte == 0xa5);
	}
	const real_t ordinary = curve->sample_baked(real_t(0.125));
	CHECK(ordinary != reentrant_sample);
	float captured = -1;
	E22_MUTATION_CHECK_OR_RETURN(snapshot_pixel(texture.ptr(), captured));
	CHECK(float(ordinary) == captured);
	curve->bake();
	CHECK(ordinary == curve->sample_baked(real_t(0.125)));
	CHECK(trace.count == 0);
}

TEST_CASE("[E22TextureSnapshot][Mutation] direct auto tangents refresh a warm cache without notifications") {
	Ref<Curve> curve;
	curve.instantiate();
	// The public serialized form deliberately carries stale tangents in linear
	// modes. set_data preserves them; the real public writer must repair them.
	Array data;
	for (const Vector2 &point : { Vector2(0, 0), Vector2(1, 1) }) {
		data.push_back(point);
		data.push_back(0.0);
		data.push_back(0.0);
		data.push_back(int(Curve::TANGENT_LINEAR));
		data.push_back(int(Curve::TANGENT_LINEAR));
	}
	curve->set_data(data);
	E22_MUTATION_CHECK_OR_RETURN(curve->get_point_count() == 2);
	CHECK(curve->get_point_right_tangent(0) == 0);
	CHECK(curve->get_point_left_tangent(1) == 0);
	curve->set_bake_resolution(8);
	curve->bake();
	const real_t before_sample = curve->sample_baked(real_t(0.125));
	Ref<CurveTexture> texture;
	texture.instantiate();
	texture->set_width(32);
	texture->set_texture_mode(CurveTexture::TEXTURE_MODE_RED);
	texture->set_curve(curve);
	Trace trace;
	E22_MUTATION_CHECK_OR_RETURN(trace.connect_curve(curve.ptr()));
	const uint64_t before_revision = curve->get_content_revision();
	curve->update_auto_tangents(0);
	CHECK(trace.count == 0);
	CHECK(curve->get_content_revision() > before_revision);
	CHECK(curve->get_point_right_tangent(0) == doctest::Approx(1.0));
	CHECK(curve->get_point_left_tangent(1) == doctest::Approx(1.0));
	const real_t after_sample = curve->sample_baked(real_t(0.125));
	CHECK(after_sample != before_sample);
	float captured = -1;
	E22_MUTATION_CHECK_OR_RETURN(snapshot_pixel(texture.ptr(), captured));
	CHECK(float(after_sample) == captured);
	curve->bake();
	CHECK(after_sample == curve->sample_baked(real_t(0.125)));
	const uint64_t revision = curve->get_content_revision();
	curve->update_auto_tangents(0);
	CHECK(curve->get_content_revision() == revision);
	CHECK(trace.count == 0);
}

TEST_CASE("[E22TextureSnapshot][Mutation] unchanged or illegal bake resolution preserves revision and notifications") {
	Ref<Curve> curve;
	curve.instantiate();
	curve->set_bake_resolution(8);
	curve->bake();
	Trace trace;
	E22_MUTATION_CHECK_OR_RETURN(trace.connect_curve(curve.ptr()));
	const uint64_t before = curve->get_content_revision();
	curve->set_bake_resolution(8);
	ERR_PRINT_OFF;
	curve->set_bake_resolution(0);
	curve->set_bake_resolution(1001);
	ERR_PRINT_ON;
	CHECK(curve->get_bake_resolution() == 8);
	CHECK(curve->get_content_revision() == before);
	CHECK(trace.count == 0);
}

TEST_CASE("[E22TextureSnapshot][MutationTrace] no_update insertion publishes its existing changed callback") {
	Ref<Curve> curve;
	curve.instantiate();
	curve->add_point(Vector2(0, 0));
	curve->add_point(Vector2(1, 1));
	curve->set_bake_resolution(8);
	curve->bake();
	const real_t old_value = curve->sample_baked(real_t(0.5));
	Trace trace;
	E22_MUTATION_CHECK_OR_RETURN(trace.connect_curve(curve.ptr()));
	const uint64_t previous = curve->get_content_revision();
	const int index = curve->add_point_no_update(Vector2(0.5, 1));
	CHECK(index == 1);
	E22_MUTATION_CHECK_OR_RETURN(!trace.overflow && trace.count == 1);
	CHECK(trace.events[0].kind == 'C');
	CHECK(trace.events[0].points == 3);
	CHECK(trace.events[0].revision > previous);
	CHECK(trace.events[0].middle == Vector2(0.5, 1));
	CHECK(trace.events[0].middle_left_tangent == 0);
	CHECK(trace.events[0].middle_right_tangent == 0);
	CHECK(curve->get_point_position(1) == Vector2(0.5, 1));
	CHECK(curve->sample_baked(real_t(0.5)) != old_value);
}

TEST_CASE("[E22TextureSnapshot][MutationTrace] point count and default setup retain original callback sequence") {
	Ref<Curve> curve;
	curve.instantiate();
	Trace trace;
	E22_MUTATION_CHECK_OR_RETURN(trace.connect_curve(curve.ptr()));
	const uint64_t initial = curve->get_content_revision();
	curve->set_point_count(3);
	E22_MUTATION_CHECK_OR_RETURN(!trace.overflow && trace.count == 4);
	for (int i = 0; i < 3; ++i) {
		CHECK(trace.events[i].kind == 'C');
		CHECK(trace.events[i].points == i + 1);
		CHECK(trace.events[i].last == Vector2(0, 0));
		CHECK(trace.events[i].revision > (i ? trace.events[i - 1].revision : initial));
	}
	CHECK(trace.events[3].kind == 'P');
	CHECK(trace.events[3].points == 3);
	CHECK(trace.events[3].revision == trace.events[2].revision);
	trace.clear();
	const uint64_t before_shrink = curve->get_content_revision();
	curve->set_point_count(1);
	E22_MUTATION_CHECK_OR_RETURN(!trace.overflow && trace.count == 2);
	CHECK(trace.events[0].kind == 'C');
	CHECK(trace.events[0].points == 1);
	CHECK(trace.events[0].revision > before_shrink);
	CHECK(trace.events[1].kind == 'P');
	CHECK(trace.events[1].points == 1);
	CHECK(trace.events[1].revision == trace.events[0].revision);
	trace.clear();
	const uint64_t unchanged = curve->get_content_revision();
	curve->set_point_count(1);
	CHECK(trace.count == 0);
	CHECK(curve->get_content_revision() == unchanged);

	Ref<Curve> defaults;
	defaults.instantiate();
	Trace default_trace;
	E22_MUTATION_CHECK_OR_RETURN(default_trace.connect_curve(defaults.ptr()));
	const uint64_t before_defaults = defaults->get_content_revision();
	defaults->ensure_default_setup(-1, 2);
	E22_MUTATION_CHECK_OR_RETURN(!default_trace.overflow && default_trace.count == 6);
	const char expected[] = { 'C', 'P', 'C', 'P', 'R', 'R' };
	const int counts[] = { 1, 1, 2, 2, 2, 2 };
	for (int i = 0; i < 6; ++i) {
		CHECK(default_trace.events[i].kind == expected[i]);
		CHECK(default_trace.events[i].points == counts[i]);
		CHECK(default_trace.events[i].revision > before_defaults);
		if (i) {
			CHECK(default_trace.events[i].revision >= default_trace.events[i - 1].revision);
		}
	}
	CHECK(default_trace.events[2].revision > default_trace.events[1].revision);
	default_trace.clear();
	const uint64_t after_defaults = defaults->get_content_revision();
	defaults->ensure_default_setup(-1, 2);
	CHECK(default_trace.count == 0);
	CHECK(defaults->get_content_revision() == after_defaults);
}

} // namespace TestE22CurveMutation
