#include "scene/resources/curve.h"
#include "scene/resources/gradient.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_resource_snapshot_bridge)

TEST_CASE("[ResourceSnapshot] Curve revision observes blocked-signal mutations and excludes baking") {
	Ref<Curve> curve;
	curve.instantiate();
	curve->set_block_signals(true);
	auto changed = [&](auto mutate) {const auto before=curve->get_content_revision();mutate();CHECK(curve->get_content_revision()>before); };
	CHECK(curve->get_content_revision() != 0);
	changed([&] { curve->add_point(Vector2(0, 0)); });
	changed([&] { curve->add_point(Vector2(1, 1)); });
	changed([&] { curve->set_point_value(0, 0.25); });
	changed([&] { curve->set_point_offset(0, 0.1); });
	changed([&] { curve->set_point_left_tangent(0, 0.5); });
	changed([&] { curve->set_point_right_tangent(0, 0.5); });
	changed([&] { curve->set_point_left_mode(1, Curve::TANGENT_LINEAR); });
	changed([&] { curve->set_point_right_mode(0, Curve::TANGENT_LINEAR); });
	changed([&] { curve->set_min_domain(-1); });
	changed([&] { curve->set_max_domain(2); });
	changed([&] { curve->set_min_value(-1); });
	changed([&] { curve->set_max_value(2); });
	auto data = curve->get_data();
	changed([&] { curve->set_data(data); });
	auto limits = curve->get_limits();
	limits[2] = -2.0;
	changed([&] { curve->set_limits(limits); });
	changed([&] { curve->set_point_count(3); });
	changed([&] { curve->remove_point(2); });
	const auto before = curve->get_content_revision();
	curve->set_bake_resolution(8);
	curve->bake();
	CHECK(curve->get_content_revision() == before);
	changed([&] { curve->clear_points(); });
	const auto empty = curve->get_content_revision();
	curve->clear_points();
	CHECK(curve->get_content_revision() == empty);
}

TEST_CASE("[ResourceSnapshot] Gradient revision observes blocked-signal mutation and excludes sorting") {
	Ref<Gradient> gradient;
	gradient.instantiate();
	gradient->set_block_signals(true);
	auto changed = [&](auto mutate) {const auto before=gradient->get_content_revision();mutate();CHECK(gradient->get_content_revision()>before); };
	changed([&] { gradient->add_point(0.5, Color(1, 0, 0)); });
	changed([&] { gradient->set_offset(0, 0.2); });
	const auto unsorted = gradient->get_content_revision();
	gradient->get_offset(0);
	(void)gradient->get_color_at_offset(0.5);
	CHECK(gradient->get_content_revision() == unsorted);
	changed([&] { gradient->set_color(0, Color(0, 1, 0)); });
	changed([&] { gradient->reverse(); });
	changed([&] { gradient->set_interpolation_mode(Gradient::GRADIENT_INTERPOLATE_CUBIC); });
	changed([&] { gradient->set_interpolation_color_space(Gradient::GRADIENT_COLOR_SPACE_OKLAB); });
	auto offsets = gradient->get_offsets();
	changed([&] { gradient->set_offsets(offsets); });
	auto colors = gradient->get_colors();
	changed([&] { gradient->set_colors(colors); });
	changed([&] { gradient->remove_point(1); });
}

#include "core/os/thread.h"
#include "scene/resources/curve_texture.h"
#include "scene/resources/gradient_texture.h"
#include "scene/resources/resource_snapshot_native.h"

#include <limits>
#include <thread>

class TestResourceSnapshotAccess {
public:
	static bool sorted(const Gradient *p) { return p->is_sorted; }
	static float first_offset(const Gradient *p) { return p->points[0].offset; }
	static void exhaust(Curve *p) { p->_content_revision = UINT64_MAX - 1; }
	static void exhaust(Gradient *p) { p->_content_revision = UINT64_MAX - 1; }
};

TEST_CASE("[ResourceSnapshot] native Curve snapshot uses source sampling and preserves prefixes") {
	Ref<Curve> curve;
	curve.instantiate();
	curve->add_point(Vector2(0, 0));
	curve->add_point(Vector2(1, 1));
	double x[3] = { 0, 0.5, 1 }, samples[3] = {};
	AngeCurvePointV1 points[2] = {};
	uint32_t pw = 99, sw = 99;
	double lo = 0, hi = 0;
	uint64_t rev = 0;
	CHECK(ange_curve_snapshot_v1(curve.ptr(), x, 3, points, 2, samples, 3, &pw, &sw, &lo, &hi, &rev) == ANGE_CAPTURE_OK_V1);
	CHECK(pw == 2);
	CHECK(sw == 3);
	CHECK(samples[0] == 0);
	CHECK(samples[1] == doctest::Approx(0.5));
	CHECK(samples[2] == 1);
	CHECK(lo == 0);
	CHECK(hi == 1);
	CHECK(rev == curve->get_content_revision());
	CHECK(ange_curve_snapshot_v1(curve.ptr(), x, 3, points, 1, samples, 3, &pw, &sw, &lo, &hi, &rev) == ANGE_CAPTURE_LIMIT_V1);
	CHECK(pw == 0);
	CHECK(sw == 0);
	curve->set_point_right_tangent(1, std::numeric_limits<real_t>::infinity());
	CHECK(ange_curve_snapshot_v1(curve.ptr(), x, 3, points, 2, samples, 3, &pw, &sw, &lo, &hi, &rev) == ANGE_CAPTURE_BAD_ARGUMENT_V1);
	CHECK(pw == 1);
	CHECK(sw == 0);
}
TEST_CASE("[ResourceSnapshot] native Gradient snapshot sorts without revising and samples original mode") {
	Ref<Gradient> g;
	g.instantiate();
	g->set_offsets(Vector<float>{ 1, 0 });
	g->set_colors(Vector<Color>{ Color(1, 0, 0), Color(0, 0, 1) });
	g->set_interpolation_mode(Gradient::GRADIENT_INTERPOLATE_CONSTANT);
	const auto before = g->get_content_revision();
	double x[3] = { 0, 0.5, 1 }, rgba[12] = {};
	AngeGradientStopV1 stops[2] = {};
	uint32_t nw = 0, rw = 0;
	int32_t mode = -1, space = -1;
	uint64_t rev = 0;
	CHECK(ange_gradient_snapshot_v1(g.ptr(), x, 3, stops, 2, rgba, 12, &nw, &rw, &mode, &space, &rev) == ANGE_CAPTURE_OK_V1);
	CHECK(nw == 2);
	CHECK(rw == 12);
	CHECK(stops[0].offset == 0);
	CHECK(stops[1].offset == 1);
	CHECK(rgba[2] == 1);
	CHECK(rgba[6] == 1);
	CHECK(rgba[8] == 1);
	CHECK(mode == 1);
	CHECK(space == 0);
	CHECK(rev == before);
	CHECK(g->get_content_revision() == before);
	CHECK(ange_gradient_snapshot_v1(g.ptr(), x, 3, stops, 2, rgba, 11, &nw, &rw, &mode, &space, &rev) == ANGE_CAPTURE_LIMIT_V1);
	CHECK(nw == 0);
	CHECK(rw == 0);
	g->set_offsets(Vector<float>{ 0, 0 });
	CHECK(ange_gradient_snapshot_v1(g.ptr(), x, 3, stops, 2, rgba, 12, &nw, &rw, &mode, &space, &rev) == ANGE_CAPTURE_BAD_ARGUMENT_V1);
	CHECK(nw == 1);
	CHECK(rw == 0);
}
TEST_CASE("[ResourceSnapshot] native content revision rejects type thread and exhausted generation") {
	Ref<Curve> c;
	c.instantiate();
	Ref<Gradient> g;
	g.instantiate();
	uint64_t revision = 99;
	CHECK(ange_resource_content_revision_v1(c.ptr(), &revision) == ANGE_CAPTURE_OK_V1);
	CHECK(revision == c->get_content_revision());
	Ref<Resource> wrong;
	wrong.instantiate();
	CHECK(ange_resource_content_revision_v1(wrong.ptr(), &revision) == ANGE_CAPTURE_WRONG_TYPE_V1);
	CHECK(revision == 0);
	CHECK(ange_resource_content_revision_v1(nullptr, &revision) == ANGE_CAPTURE_BAD_ARGUMENT_V1);
	CHECK(revision == 0);
#ifdef THREADS_ENABLED
	AngeCaptureStatusV1 status = 99;
	std::thread other([&] { status = ange_resource_content_revision_v1(c.ptr(), &revision); });
	other.join();
	CHECK(status == ANGE_CAPTURE_WRONG_THREAD_V1);
	CHECK(revision == 0);
#endif
	TestResourceSnapshotAccess::exhaust(c.ptr());
	c->add_point(Vector2(0, 0));
	CHECK(c->get_content_revision() == UINT64_MAX);
	c->set_min_domain(-1);
	CHECK(c->get_content_revision() == UINT64_MAX);
	CHECK(ange_resource_content_revision_v1(c.ptr(), &revision) == ANGE_CAPTURE_LIMIT_V1);
	CHECK(revision == UINT64_MAX);
	TestResourceSnapshotAccess::exhaust(g.ptr());
	g->set_color(0, Color(1, 0, 0));
	g->reverse();
	CHECK(g->get_content_revision() == UINT64_MAX);
	CHECK(ange_resource_content_revision_v1(g.ptr(), &revision) == ANGE_CAPTURE_LIMIT_V1);
}

#include "core/extension/gdextension.h"

#include <type_traits>
static_assert(std::is_same_v<decltype(&ange_curve_snapshot_v1), AngeCurveSnapshotFnV1>);
static_assert(std::is_same_v<decltype(&ange_gradient_snapshot_v1), AngeGradientSnapshotFnV1>);
static_assert(std::is_same_v<decltype(&ange_resource_content_revision_v1), AngeResourceContentRevisionFnV1>);

TEST_CASE("[ResourceSnapshot] original GDExtension proc names resolve matching snapshot ABI") {
	CHECK(GDExtension::get_interface_function(ANGE_CURVE_SNAPSHOT_PROC_V1) == reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&ange_curve_snapshot_v1));
	CHECK(GDExtension::get_interface_function(ANGE_GRADIENT_SNAPSHOT_PROC_V1) == reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&ange_gradient_snapshot_v1));
	CHECK(GDExtension::get_interface_function(ANGE_RESOURCE_CONTENT_REVISION_PROC_V1) == reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&ange_resource_content_revision_v1));
}

TEST_CASE("[ResourceSnapshot] deferred changed delivery never delays Curve and Gradient source revision") {
	class DeferredCurve : public Curve {
	public:
		void block() { _block_emit_changed(); }
		void unblock() { _unblock_emit_changed(); }
	};
	class DeferredGradient : public Gradient {
	public:
		void block() { _block_emit_changed(); }
		void unblock() { _unblock_emit_changed(); }
	};
	Ref<DeferredCurve> c;
	c.instantiate();
	c->block();
	const auto cr = c->get_content_revision();
	c->add_point(Vector2(0, 0));
	CHECK(c->get_content_revision() > cr);
	const auto pending = c->get_content_revision();
	c->unblock();
	CHECK(c->get_content_revision() == pending);
	Ref<DeferredGradient> g;
	g.instantiate();
	g->block();
	const auto gr = g->get_content_revision();
	g->set_color(0, Color(1, 0, 0));
	CHECK(g->get_content_revision() > gr);
	const auto gp = g->get_content_revision();
	g->unblock();
	CHECK(g->get_content_revision() == gp);
}

TEST_CASE("[ResourceSnapshot] empty constant source sample limits and invalid queries are explicit") {
	Ref<Curve> c;
	c.instantiate();
	double x[2] = { 0, 1 }, samples[2] = { 9, 9 };
	AngeCurvePointV1 points[2] = {};
	uint32_t pw = 9, sw = 9;
	double lo = 9, hi = 9;
	uint64_t rev = 0;
	CHECK(ange_curve_snapshot_v1(c.ptr(), x, 2, nullptr, 0, samples, 2, &pw, &sw, &lo, &hi, &rev) == ANGE_CAPTURE_OK_V1);
	CHECK(pw == 0);
	CHECK(sw == 2);
	CHECK(samples[0] == 0);
	CHECK(samples[1] == 0);
	c->add_point(Vector2(0.25, 0.75));
	CHECK(ange_curve_snapshot_v1(c.ptr(), x, 2, points, 2, samples, 2, &pw, &sw, &lo, &hi, &rev) == ANGE_CAPTURE_OK_V1);
	CHECK(pw == 1);
	CHECK(samples[0] == 0.75);
	CHECK(samples[1] == 0.75);
	x[1] = std::numeric_limits<double>::infinity();
	CHECK(ange_curve_snapshot_v1(c.ptr(), x, 2, points, 2, samples, 2, &pw, &sw, &lo, &hi, &rev) == ANGE_CAPTURE_BAD_ARGUMENT_V1);
	CHECK(pw == 0);
	CHECK(sw == 0);
	x[1] = 2;
	CHECK(ange_curve_snapshot_v1(c.ptr(), x, 2, points, 2, samples, 2, &pw, &sw, &lo, &hi, &rev) == ANGE_CAPTURE_BAD_ARGUMENT_V1);
	CHECK(sw == 0);
	x[1] = 1;
	CHECK(ange_curve_snapshot_v1(c.ptr(), x, 2, points, 2, x, 2, &pw, &sw, &lo, &hi, &rev) == ANGE_CAPTURE_BAD_ARGUMENT_V1);
	CHECK(sw == 0);
	CHECK(ange_curve_snapshot_v1(c.ptr(), nullptr, 4097, points, 2, samples, 2, &pw, &sw, &lo, &hi, &rev) == ANGE_CAPTURE_LIMIT_V1);
	CHECK(pw == 0);
	CHECK(sw == 0);
	Ref<Gradient> g;
	g.instantiate();
	g->set_offsets(Vector<float>());
	AngeGradientStopV1 stops[2] = {};
	double rgba[8] = {};
	int32_t mode = 0, space = 0;
	CHECK(ange_gradient_snapshot_v1(g.ptr(), x, 2, nullptr, 0, rgba, 8, &pw, &sw, &mode, &space, &rev) == ANGE_CAPTURE_OK_V1);
	CHECK(pw == 0);
	CHECK(sw == 8);
	CHECK(rgba[3] == 1);
	CHECK(rgba[7] == 1);
	g->set_offsets(Vector<float>{ 0, std::numeric_limits<float>::quiet_NaN() });
	CHECK(ange_gradient_snapshot_v1(g.ptr(), x, 2, stops, 2, rgba, 8, &pw, &sw, &mode, &space, &rev) == ANGE_CAPTURE_BAD_ARGUMENT_V1);
	CHECK(pw == 0);
	CHECK(sw == 0);
}

TEST_CASE("[ResourceSnapshot] malformed overlapping outputs reject before any source or payload write") {
	Ref<Curve> c;
	c.instantiate();
	c->add_point(Vector2(0, 0));
	c->add_point(Vector2(1, 1));
	double x[2] = { 0, 1 }, samples[2] = { 7, 8 };
	AngeCurvePointV1 points[2] = {};
	uint32_t shared = 0, sw = 0;
	double lo = 9, hi = 9;
	uint64_t rev = 0;
	CHECK(ange_curve_snapshot_v1(c.ptr(), x, 2, points, 2, samples, 2, &shared, &shared, &lo, &hi, &rev) == ANGE_CAPTURE_BAD_ARGUMENT_V1);
	CHECK(shared == 0);
	CHECK(samples[0] == 7);
	CHECK(ange_curve_snapshot_v1(c.ptr(), x, 2, points, 2, samples, 2, &shared, &sw, samples, &hi, &rev) == ANGE_CAPTURE_BAD_ARGUMENT_V1);
	CHECK(samples[0] == 7);
	CHECK(ange_curve_snapshot_v1(c.ptr(), x, 2, points, 2, samples, 2, &shared, &sw, x + 1, &hi, &rev) == ANGE_CAPTURE_BAD_ARGUMENT_V1);
	CHECK(x[1] == 1);
	Ref<Gradient> g;
	g.instantiate();
	AngeGradientStopV1 stops[2] = {};
	double rgba[8] = {};
	int32_t mode = 0, space = 0;
	shared = 0;
	CHECK(ange_gradient_snapshot_v1(g.ptr(), x, 2, stops, 2, rgba, 8, &shared, &shared, &mode, &space, &rev) == ANGE_CAPTURE_BAD_ARGUMENT_V1);
	CHECK(shared == 0);
}
TEST_CASE("[ResourceSnapshot] nonlinear Curve sampling and admitted provenance count remain independent") {
	Ref<Curve> c;
	c.instantiate();
	c->add_point(Vector2(0, 0), 0, 6);
	c->add_point(Vector2(1, 0));
	double x[3] = { 0, 0.5, 1 }, samples[3] = {};
	AngeCurvePointV1 points[3] = {};
	uint32_t pw = 0, sw = 0;
	double lo = 0, hi = 0;
	uint64_t rev = 0;
	CHECK(ange_curve_snapshot_v1(c.ptr(), x, 3, points, 3, samples, 3, &pw, &sw, &lo, &hi, &rev) == ANGE_CAPTURE_OK_V1);
	CHECK(samples[1] == doctest::Approx(0.75));
	const double endpoints[2] = { 0, 1 };
	CHECK(ange_curve_snapshot_v1(c.ptr(), endpoints, 2, points, 3, samples, 3, &pw, &sw, &lo, &hi, &rev) == ANGE_CAPTURE_OK_V1);
	CHECK(samples[0] == 0);
	CHECK(samples[1] == 0);
	c->add_point(Vector2(0.25, 0.25));
	CHECK(ange_curve_snapshot_v1(c.ptr(), endpoints, 2, points, 3, samples, 3, &pw, &sw, &lo, &hi, &rev) == ANGE_CAPTURE_OK_V1);
	CHECK(pw == 3);
	CHECK(sw == 2);
}
TEST_CASE("[ResourceSnapshot] finite Curve inputs can fail sampling with an owned initialized prefix") {
	Ref<Curve> c;
	c.instantiate();
	const real_t max = std::numeric_limits<float>::max();
	c->set_max_domain(max);
	c->add_point(Vector2(0, 0), 0, max);
	c->add_point(Vector2(max, 0));
	double x[3] = { 0, double(max) * 0.5, double(max) }, samples[3] = { 7, 8, 9 };
	AngeCurvePointV1 points[2] = {};
	uint32_t pw = 0, sw = 0;
	double lo = 0, hi = 0;
	uint64_t rev = 0;
	CHECK(ange_curve_snapshot_v1(c.ptr(), x, 3, points, 2, samples, 3, &pw, &sw, &lo, &hi, &rev) == ANGE_CAPTURE_BAD_ARGUMENT_V1);
	CHECK(pw == 2);
	CHECK(sw == 1);
	CHECK(samples[0] == 0);
	CHECK(samples[1] == 8);
}
TEST_CASE("[ResourceSnapshot] Gradient interpolation and color spaces match analytical references") {
	Ref<Gradient> g;
	g.instantiate();
	double x[3] = { 0, 0.25, 1 }, rgba[12] = {};
	AngeGradientStopV1 stops[2] = {};
	uint32_t nw = 0, rw = 0;
	int32_t mode = 0, space = 0;
	uint64_t rev = 0;
	auto capture = [&] { return ange_gradient_snapshot_v1(g.ptr(), x, 3, stops, 2, rgba, 12, &nw, &rw, &mode, &space, &rev); };
	CHECK(capture() == ANGE_CAPTURE_OK_V1);
	CHECK(rgba[4] == doctest::Approx(0.25));
	g->set_interpolation_mode(Gradient::GRADIENT_INTERPOLATE_CUBIC);
	CHECK(capture() == ANGE_CAPTURE_OK_V1);
	CHECK(rgba[4] == doctest::Approx(0.203125));
	x[1] = 0.5;
	g->set_interpolation_mode(Gradient::GRADIENT_INTERPOLATE_LINEAR);
	g->set_interpolation_color_space(Gradient::GRADIENT_COLOR_SPACE_LINEAR_SRGB);
	CHECK(capture() == ANGE_CAPTURE_OK_V1);
	CHECK(rgba[4] == doctest::Approx(0.735356983).epsilon(0.000001));
	CHECK(rgba[7] == 1);
	g->set_interpolation_color_space(Gradient::GRADIENT_COLOR_SPACE_OKLAB);
	CHECK(capture() == ANGE_CAPTURE_OK_V1);
	CHECK(rgba[4] == doctest::Approx(0.388572859).epsilon(0.000005));
	CHECK(rgba[7] == 1);
}

TEST_CASE("[ResourceSnapshot] insufficient stop capacity refuses before native sorting") {
	Ref<Gradient> g;
	g.instantiate();
	g->set_offsets(Vector<float>{ 1, 0 });
	CHECK_FALSE(TestResourceSnapshotAccess::sorted(g.ptr()));
	CHECK(TestResourceSnapshotAccess::first_offset(g.ptr()) == 1);
	const auto before = g->get_content_revision();
	double x[2] = { 0, 1 }, rgba[8] = { 7 };
	AngeGradientStopV1 stop = {};
	uint32_t nw = 9, rw = 9;
	int32_t mode = 0, space = 0;
	uint64_t rev = 0;
	CHECK(ange_gradient_snapshot_v1(g.ptr(), x, 2, &stop, 1, rgba, 8, &nw, &rw, &mode, &space, &rev) == ANGE_CAPTURE_LIMIT_V1);
	CHECK(nw == 0);
	CHECK(rw == 0);
	CHECK(rgba[0] == 7);
	CHECK_FALSE(TestResourceSnapshotAccess::sorted(g.ptr()));
	CHECK(TestResourceSnapshotAccess::first_offset(g.ptr()) == 1);
	CHECK(g->get_content_revision() == before);
}

TEST_CASE("[ResourceSnapshot] texture plan and capture use real CurveTexture and GradientTexture1D objects") {
	Ref<CurveTexture> curve_texture;
	curve_texture.instantiate();
	curve_texture->set_width(32);
	curve_texture->set_texture_mode(CurveTexture::TEXTURE_MODE_RED);
	Ref<Curve> curve;
	curve.instantiate();
	curve->add_point(Vector2(0, 0));
	curve->add_point(Vector2(1, 1));
	curve_texture->set_curve(curve);

	AngeTextureSnapshotPlanV1 curve_plan = {};
	CHECK(ange_texture_snapshot_plan_v1(curve_texture.ptr(), ANGE_TEXTURE_FORMAT_RGBAF_V1, &curve_plan) == ANGE_TEXTURE_OK_V1);
	CHECK(curve_plan.source.source_kind == ANGE_TEXTURE_SOURCE_CURVE_V1);
	CHECK(curve_plan.source.width == 32);
	CHECK(curve_plan.source.native_mode == ANGE_TEXTURE_CURVE_RED_V1);
	CHECK(curve_plan.source.child_present == 1);
	CHECK(curve_plan.output_format == ANGE_TEXTURE_FORMAT_RGBAF_V1);
	CHECK(curve_plan.output_bytes == 32 * 16);
	CHECK(curve_plan.real_bytes == sizeof(real_t));
	CHECK(curve_plan.scratch_bytes == uint64_t(curve_plan.bake_resolution) * curve_plan.real_bytes);
	Vector<uint8_t> curve_output;
	curve_output.resize(curve_plan.output_bytes);
	Vector<uint8_t> curve_scratch;
	curve_scratch.resize(curve_plan.scratch_bytes);
	AngeTextureSnapshotResultV1 curve_result = {};
	CHECK(ange_texture_snapshot_v1(curve_texture.ptr(), &curve_plan, curve_output.ptrw(), curve_output.size(), curve_scratch.ptrw(), curve_scratch.size(), &curve_result) == ANGE_TEXTURE_OK_V1);
	CHECK(curve_result.status == ANGE_TEXTURE_OK_V1);
	CHECK(curve_result.initialized_pixels == 32);
	float curve_first = 0;
	memcpy(&curve_first, curve_output.ptr(), sizeof(curve_first));
	CHECK(curve_first == doctest::Approx(0));
	const auto curve_child_revision = curve_plan.source.child_revision;
	curve->set_point_value(1, 0.5);
	CHECK(curve->get_content_revision() != curve_child_revision);
	curve_result = {};
	CHECK(ange_texture_snapshot_v1(curve_texture.ptr(), &curve_plan, curve_output.ptrw(), curve_output.size(), curve_scratch.ptrw(), curve_scratch.size(), &curve_result) == ANGE_TEXTURE_STALE_SOURCE_V1);
	CHECK(curve_result.initialized_pixels == 0);

	Ref<GradientTexture1D> gradient_texture;
	gradient_texture.instantiate();
	gradient_texture->set_width(4);
	gradient_texture->set_use_hdr(false);
	Ref<Gradient> gradient;
	gradient.instantiate();
	gradient->set_colors(Vector<Color>{ Color(0, 0, 0, 1), Color(1, 0, 0, 1) });
	gradient_texture->set_gradient(gradient);
	AngeTextureSnapshotPlanV1 gradient_plan = {};
	CHECK(ange_texture_snapshot_plan_v1(gradient_texture.ptr(), ANGE_TEXTURE_FORMAT_RGBA8_V1, &gradient_plan) == ANGE_TEXTURE_OK_V1);
	CHECK(gradient_plan.source.source_kind == ANGE_TEXTURE_SOURCE_GRADIENT_1D_V1);
	CHECK(gradient_plan.source.native_mode == ANGE_TEXTURE_GRADIENT_LDR_V1);
	CHECK(gradient_plan.output_format == ANGE_TEXTURE_FORMAT_RGBA8_V1);
	CHECK(gradient_plan.output_bytes == 16);
	CHECK(gradient_plan.scratch_bytes == 0);
	Vector<uint8_t> gradient_output;
	gradient_output.resize(gradient_plan.output_bytes);
	AngeTextureSnapshotResultV1 gradient_result = {};
	CHECK(ange_texture_snapshot_v1(gradient_texture.ptr(), &gradient_plan, gradient_output.ptrw(), gradient_output.size(), nullptr, 0, &gradient_result) == ANGE_TEXTURE_OK_V1);
	CHECK(gradient_result.initialized_pixels == 4);
	CHECK(gradient_output[0] == 0);
	CHECK(gradient_output[4] == 85);
	CHECK(gradient_output[8] == 170);
	CHECK(gradient_output[12] == 255);
	CHECK(GDExtension::get_interface_function(ANGE_TEXTURE_SNAPSHOT_PLAN_PROC_V1) == reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&ange_texture_snapshot_plan_v1));
	CHECK(GDExtension::get_interface_function(ANGE_TEXTURE_SNAPSHOT_PROC_V1) == reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&ange_texture_snapshot_v1));
	Ref<CurveTexture> missing_curve;
	missing_curve.instantiate();
	missing_curve->set_width(32);
	AngeTextureSnapshotPlanV1 missing_plan = {};
	CHECK(ange_texture_snapshot_plan_v1(missing_curve.ptr(), ANGE_TEXTURE_FORMAT_RGBAF_V1, &missing_plan) == ANGE_TEXTURE_INVALID_SOURCE_V1);
	CHECK(ange_texture_snapshot_plan_v1(curve.ptr(), ANGE_TEXTURE_FORMAT_RGBAF_V1, &missing_plan) == ANGE_TEXTURE_WRONG_TYPE_V1);
}
