/**************************************************************************/
/*  resource_snapshot_native.cpp                                          */
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

#include "resource_snapshot_native.h"

#include "core/extension/gdextension.h"
#include "core/os/thread.h"
#include "scene/resources/curve.h"
#include "scene/resources/curve_texture.h"
#include "scene/resources/gradient.h"
#include "scene/resources/gradient_texture.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <limits>

// Internal access under the caller-retained wrapper; never acquire a child Ref.
class ResourceSnapshotAccess {
public:
	static Curve *child(const CurveTexture &p_texture) { return p_texture._curve.ptr(); }
	static Gradient *child(const GradientTexture1D &p_texture) { return p_texture.gradient.ptr(); }
	static void fill(const Curve &p_curve, real_t *p_values, int p_count) { p_curve._fill_baked_values(p_values, p_count); }
	static real_t sample(const Curve &p_curve, real_t p_offset, const real_t *p_values, int p_count) { return p_curve._sample_baked_values(p_offset, p_values, p_count); }
	static bool finite_points(const Gradient &p_gradient);
};

namespace {
template <class T>
bool aligned(const T *p) {
	return p && uintptr_t(p) % alignof(T) == 0;
}
bool finite_f32(double value) {
	return std::isfinite(value) && value <= std::numeric_limits<float>::max() && value >= -std::numeric_limits<float>::max();
}
bool finite_color(const Color &c) {
	return finite_f32(c.r) && finite_f32(c.g) && finite_f32(c.b) && finite_f32(c.a);
}
bool separate(const void *a, uint64_t as, const void *b, uint64_t bs) {
	if (!as || !bs) {
		return true;
	}
	const auto x = uintptr_t(a), y = uintptr_t(b);
	return x <= y ? as <= y - x : bs <= x - y;
}
struct SnapshotMemorySpan {
	const void *data;
	uint64_t bytes;
};
bool disjoint_spans(std::initializer_list<SnapshotMemorySpan> spans) {
	for (auto a = spans.begin(); a != spans.end(); ++a) {
		if (!a->bytes) {
			continue;
		}
		if (!a->data || a->bytes > UINTPTR_MAX - uintptr_t(a->data)) {
			return false;
		}
		for (auto b = spans.begin(); b != a; ++b) {
			if (!separate(a->data, a->bytes, b->data, b->bytes)) {
				return false;
			}
		}
	}
	return true;
}
bool sample_span(const double *x, uint32_t count, double lo, double hi) {
	for (uint32_t i = 0; i < count; ++i) {
		if (!finite_f32(x[i]) || x[i] < lo || x[i] > hi) {
			return false;
		}
	}
	return true;
}

struct TextureView {
	Object *wrapper = nullptr;
	Object *child = nullptr;
	uint64_t wrapper_id = 0;
	uint64_t wrapper_revision = 0;
	uint64_t child_id = 0;
	uint64_t child_revision = 0;
	uint32_t source_kind = 0;
	uint32_t width = 0;
	uint32_t native_mode = 0;
	uint32_t child_present = 0;
};

uint32_t texture_view(void *source, TextureView &view) {
	if (!Thread::is_main_thread()) {
		return ANGE_TEXTURE_WRONG_THREAD_V1;
	}
	if (!source) {
		return ANGE_TEXTURE_BAD_ARGUMENT_V1;
	}
	Object *object = static_cast<Object *>(source);
	if (auto *curve_texture = Object::cast_to<CurveTexture>(object)) {
		view.wrapper = curve_texture;
		view.wrapper_id = uint64_t(curve_texture->get_instance_id());
		view.wrapper_revision = curve_texture->get_content_revision();
		view.source_kind = ANGE_TEXTURE_SOURCE_CURVE_V1;
		view.width = uint32_t(curve_texture->get_width());
		view.native_mode = curve_texture->get_texture_mode() == CurveTexture::TEXTURE_MODE_RGB ? ANGE_TEXTURE_CURVE_RGB_V1 : ANGE_TEXTURE_CURVE_RED_V1;
		Curve *child = ResourceSnapshotAccess::child(*curve_texture);
		if (child) {
			view.child = child;
			view.child_id = uint64_t(child->get_instance_id());
			view.child_revision = child->get_content_revision();
			view.child_present = 1;
		}
	} else if (auto *gradient_texture = Object::cast_to<GradientTexture1D>(object)) {
		view.wrapper = gradient_texture;
		view.wrapper_id = uint64_t(gradient_texture->get_instance_id());
		view.wrapper_revision = gradient_texture->get_content_revision();
		view.source_kind = ANGE_TEXTURE_SOURCE_GRADIENT_1D_V1;
		view.width = uint32_t(gradient_texture->get_width());
		view.native_mode = gradient_texture->is_using_hdr() ? ANGE_TEXTURE_GRADIENT_HDR_V1 : ANGE_TEXTURE_GRADIENT_LDR_V1;
		Gradient *child = ResourceSnapshotAccess::child(*gradient_texture);
		if (child) {
			view.child = child;
			view.child_id = uint64_t(child->get_instance_id());
			view.child_revision = child->get_content_revision();
			view.child_present = 1;
		}
	} else {
		return ANGE_TEXTURE_WRONG_TYPE_V1;
	}
	if (!view.child_present && view.source_kind == ANGE_TEXTURE_SOURCE_GRADIENT_1D_V1) {
		return ANGE_TEXTURE_INVALID_SOURCE_V1;
	}
	if (!view.wrapper_id || view.wrapper_revision == UINT64_MAX || (view.child_present && (!view.child_id || view.child_revision == UINT64_MAX))) {
		return ANGE_TEXTURE_INVALID_SOURCE_V1;
	}
	return ANGE_TEXTURE_OK_V1;
}

AngeTextureSourceTupleV1 texture_source(const TextureView &view) {
	return { view.wrapper_id, view.wrapper_revision, view.child_id, view.child_revision,
		view.source_kind, view.width, view.native_mode, view.child_present };
}

bool same_texture_source(const AngeTextureSourceTupleV1 &a, const AngeTextureSourceTupleV1 &b) {
	return a.wrapper_id == b.wrapper_id && a.wrapper_revision == b.wrapper_revision &&
			a.child_id == b.child_id && a.child_revision == b.child_revision &&
			a.source_kind == b.source_kind && a.width == b.width &&
			a.native_mode == b.native_mode && a.child_present == b.child_present;
}

bool same_texture_plan(const AngeTextureSnapshotPlanV1 &a, const AngeTextureSnapshotPlanV1 &b) {
	return same_texture_source(a.source, b.source) && a.output_format == b.output_format &&
			a.real_bytes == b.real_bytes && a.point_count == b.point_count &&
			a.bake_resolution == b.bake_resolution && a.output_bytes == b.output_bytes &&
			a.scratch_bytes == b.scratch_bytes;
}

uint32_t build_texture_plan(const TextureView &view, uint32_t output_format, AngeTextureSnapshotPlanV1 &plan) {
	plan = {};
	plan.source = texture_source(view);
	if (view.width == 0 || view.width > 4096) {
		return ANGE_TEXTURE_LIMIT_V1;
	}
	if (view.source_kind == ANGE_TEXTURE_SOURCE_CURVE_V1) {
		if (output_format != ANGE_TEXTURE_FORMAT_RGBAF_V1 || (view.width < 32) || view.native_mode < ANGE_TEXTURE_CURVE_RGB_V1 || view.native_mode > ANGE_TEXTURE_CURVE_RED_V1) {
			return ANGE_TEXTURE_UNSUPPORTED_V1;
		}
		if (view.child_present) {
			const auto *curve = static_cast<const Curve *>(view.child);
			if (curve->get_point_count() > 4096) {
				return ANGE_TEXTURE_LIMIT_V1;
			}
			plan.point_count = curve->get_point_count();
			plan.bake_resolution = curve->get_bake_resolution();
			if (plan.bake_resolution < 1 || plan.bake_resolution > 1000) {
				return ANGE_TEXTURE_LIMIT_V1;
			}
			plan.scratch_bytes = uint64_t(plan.bake_resolution) * sizeof(real_t);
		}
		plan.real_bytes = sizeof(real_t);
	} else if (view.source_kind == ANGE_TEXTURE_SOURCE_GRADIENT_1D_V1) {
		if (!view.child_present || view.width < 2 || (view.native_mode == ANGE_TEXTURE_GRADIENT_HDR_V1 ? output_format != ANGE_TEXTURE_FORMAT_RGBAF_V1 : output_format != ANGE_TEXTURE_FORMAT_RGBA8_V1)) {
			return view.child_present ? ANGE_TEXTURE_UNSUPPORTED_V1 : ANGE_TEXTURE_INVALID_SOURCE_V1;
		}
		Gradient *gradient = static_cast<Gradient *>(view.child);
		if (gradient->get_point_count() > 4096) {
			return ANGE_TEXTURE_LIMIT_V1;
		}
		plan.point_count = gradient->get_point_count();
		plan.real_bytes = sizeof(real_t);
	} else {
		return ANGE_TEXTURE_WRONG_TYPE_V1;
	}
	plan.output_format = output_format;
	plan.output_bytes = uint64_t(view.width) * (output_format == ANGE_TEXTURE_FORMAT_RGBAF_V1 ? 16u : 4u);
	return ANGE_TEXTURE_OK_V1;
}

void write_f32(uint8_t *destination, float value) {
	std::memcpy(destination, &value, sizeof(value));
}

bool valid_float(float value) {
	return std::isfinite(value);
}

bool finite_curve_points(const Curve &curve) {
	if (!std::isfinite(curve.get_min_value()) || !std::isfinite(curve.get_max_value()) ||
			!std::isfinite(curve.get_min_domain()) || !std::isfinite(curve.get_max_domain()) ||
			!std::isfinite(curve.get_domain_range()) || curve.get_domain_range() <= 0) {
		return false;
	}
	for (uint32_t i = 0; i < uint32_t(curve.get_point_count()); ++i) {
		const auto point = curve.get_point(i);
		if (!std::isfinite(point.position.x) || !std::isfinite(point.position.y) ||
				!std::isfinite(point.left_tangent) || !std::isfinite(point.right_tangent) ||
				point.left_mode < Curve::TANGENT_FREE || point.left_mode >= Curve::TANGENT_MODE_COUNT ||
				point.right_mode < Curve::TANGENT_FREE || point.right_mode >= Curve::TANGENT_MODE_COUNT ||
				(i && point.position.x < curve.get_point(i - 1).position.x)) {
			return false;
		}
	}
	return true;
}
} //namespace

// Raw finite validation precedes Gradient's first sorting accessor. A NaN
// offset must never reach the native sort comparator; no duplicate array.
bool ResourceSnapshotAccess::finite_points(const Gradient &g) {
	for (const auto &p : g.points) {
		if (!finite_f32(p.offset) || !finite_color(p.color)) {
			return false;
		}
	}
	return true;
}

AngeCaptureStatusV1 ange_resource_content_revision_v1(void *source, uint64_t *revision) {
	if (!aligned(revision)) {
		return ANGE_CAPTURE_BAD_ARGUMENT_V1;
	}
	*revision = 0;
	if (!Thread::is_main_thread()) {
		return ANGE_CAPTURE_WRONG_THREAD_V1;
	}
	if (!source) {
		return ANGE_CAPTURE_BAD_ARGUMENT_V1;
	}
	auto *object = static_cast<Object *>(source);
	if (const auto *curve = Object::cast_to<Curve>(object)) {
		*revision = curve->get_content_revision();
	} else if (const auto *gradient = Object::cast_to<Gradient>(object)) {
		*revision = gradient->get_content_revision();
	} else {
		return ANGE_CAPTURE_WRONG_TYPE_V1;
	}
	return *revision == UINT64_MAX ? ANGE_CAPTURE_LIMIT_V1 : ANGE_CAPTURE_OK_V1;
}

AngeCaptureStatusV1 ange_curve_snapshot_v1(void *source, const double *x, uint32_t count,
		AngeCurvePointV1 *points, uint32_t point_capacity, double *samples, uint32_t sample_capacity,
		uint32_t *points_written, uint32_t *samples_written, double *domain_min, double *domain_max, uint64_t *revision) {
	if (!aligned(points_written) || !aligned(samples_written) || !aligned(domain_min) || !aligned(domain_max) || !aligned(revision)) {
		return ANGE_CAPTURE_BAD_ARGUMENT_V1;
	}
	// Invalid/overlapping metadata cannot represent independent prefix counts.
	// Reject such malformed spans before touching outputs or aliased input.
	if (!disjoint_spans({ { points_written, sizeof(*points_written) }, { samples_written, sizeof(*samples_written) },
				{ domain_min, sizeof(*domain_min) }, { domain_max, sizeof(*domain_max) }, { revision, sizeof(*revision) },
				{ points, uint64_t(point_capacity) * sizeof(*points) }, { samples, uint64_t(sample_capacity) * sizeof(*samples) },
				{ x, count <= 4096 ? uint64_t(count) * sizeof(*x) : 0 } })) {
		return ANGE_CAPTURE_BAD_ARGUMENT_V1;
	}
	*points_written = *samples_written = 0;
	*domain_min = *domain_max = 0;
	*revision = 0;
	auto status = ange_resource_content_revision_v1(source, revision);
	if (status != ANGE_CAPTURE_OK_V1) {
		return status;
	}
	const auto *curve = Object::cast_to<Curve>(static_cast<Object *>(source));
	if (!curve) {
		return ANGE_CAPTURE_WRONG_TYPE_V1;
	}
	const uint32_t size = curve->get_point_count();
	*domain_min = curve->get_min_domain();
	*domain_max = curve->get_max_domain();
	if (count > 4096 || size > point_capacity || count > sample_capacity) {
		return ANGE_CAPTURE_LIMIT_V1;
	}
	if ((size && !aligned(points)) || (count && (!aligned(x) || !aligned(samples))) ||
			!separate(points, uint64_t(size) * sizeof(*points), samples, uint64_t(count) * sizeof(*samples)) ||
			!separate(x, uint64_t(count) * sizeof(*x), samples, uint64_t(count) * sizeof(*samples)) ||
			!separate(x, uint64_t(count) * sizeof(*x), points, uint64_t(size) * sizeof(*points))) {
		return ANGE_CAPTURE_BAD_ARGUMENT_V1;
	}
	if ((size > 1 && count < 2) || !finite_f32(*domain_min) || !finite_f32(*domain_max) || !(*domain_min < *domain_max) || !sample_span(x, count, *domain_min, *domain_max)) {
		return ANGE_CAPTURE_BAD_ARGUMENT_V1;
	}
	for (uint32_t i = 0; i < size; ++i) {
		const auto p = curve->get_point(i);
		if (!finite_f32(p.position.x) || !finite_f32(p.position.y) || !finite_f32(p.left_tangent) || !finite_f32(p.right_tangent) ||
				p.left_mode < Curve::TANGENT_FREE || p.left_mode >= Curve::TANGENT_MODE_COUNT || p.right_mode < Curve::TANGENT_FREE || p.right_mode >= Curve::TANGENT_MODE_COUNT ||
				(i && p.position.x < points[i - 1].x)) {
			return ANGE_CAPTURE_BAD_ARGUMENT_V1;
		}
		points[i] = { double(p.position.x), double(p.position.y), double(p.left_tangent), double(p.right_tangent), int32_t(p.left_mode), int32_t(p.right_mode) };
		++*points_written;
	}
	for (uint32_t i = 0; i < count; ++i) {
		const double value = curve->sample(real_t(x[i]));
		if (!finite_f32(value)) {
			return ANGE_CAPTURE_BAD_ARGUMENT_V1;
		}
		samples[i] = value;
		++*samples_written;
	}
	return curve->get_content_revision() == *revision ? ANGE_CAPTURE_OK_V1 : ANGE_CAPTURE_SOURCE_BUSY_V1;
}

AngeCaptureStatusV1 ange_gradient_snapshot_v1(void *source, const double *x, uint32_t count,
		AngeGradientStopV1 *stops, uint32_t stop_capacity, double *rgba, uint32_t rgba_capacity,
		uint32_t *stops_written, uint32_t *rgba_written, int32_t *mode, int32_t *space, uint64_t *revision) {
	if (!aligned(stops_written) || !aligned(rgba_written) || !aligned(mode) || !aligned(space) || !aligned(revision)) {
		return ANGE_CAPTURE_BAD_ARGUMENT_V1;
	}
	if (!disjoint_spans({ { stops_written, sizeof(*stops_written) }, { rgba_written, sizeof(*rgba_written) },
				{ mode, sizeof(*mode) }, { space, sizeof(*space) }, { revision, sizeof(*revision) },
				{ stops, uint64_t(stop_capacity) * sizeof(*stops) }, { rgba, uint64_t(rgba_capacity) * sizeof(*rgba) },
				{ x, count <= 4096 ? uint64_t(count) * sizeof(*x) : 0 } })) {
		return ANGE_CAPTURE_BAD_ARGUMENT_V1;
	}
	*stops_written = *rgba_written = 0;
	*mode = *space = 0;
	*revision = 0;
	auto status = ange_resource_content_revision_v1(source, revision);
	if (status != ANGE_CAPTURE_OK_V1) {
		return status;
	}
	auto *gradient = Object::cast_to<Gradient>(static_cast<Object *>(source));
	if (!gradient) {
		return ANGE_CAPTURE_WRONG_TYPE_V1;
	}
	const uint32_t size = gradient->get_point_count();
	*mode = gradient->get_interpolation_mode();
	*space = gradient->get_interpolation_color_space();
	if (size > 4096 || count > 4096 || size > stop_capacity || uint64_t(count) * 4 > rgba_capacity) {
		return ANGE_CAPTURE_LIMIT_V1;
	}
	if ((size && !aligned(stops)) || (count && (!aligned(x) || !aligned(rgba))) ||
			!separate(stops, uint64_t(size) * sizeof(*stops), rgba, uint64_t(count) * 4 * sizeof(*rgba)) ||
			!separate(x, uint64_t(count) * sizeof(*x), rgba, uint64_t(count) * 4 * sizeof(*rgba)) ||
			!separate(x, uint64_t(count) * sizeof(*x), stops, uint64_t(size) * sizeof(*stops))) {
		return ANGE_CAPTURE_BAD_ARGUMENT_V1;
	}
	if (*mode < Gradient::GRADIENT_INTERPOLATE_LINEAR || *mode > Gradient::GRADIENT_INTERPOLATE_CUBIC ||
			*space < Gradient::GRADIENT_COLOR_SPACE_SRGB || *space > Gradient::GRADIENT_COLOR_SPACE_OKLAB ||
			(count == 1) || (size >= 2 && *mode != Gradient::GRADIENT_INTERPOLATE_CONSTANT && count < 2) ||
			!sample_span(x, count, 0, 1) || !ResourceSnapshotAccess::finite_points(*gradient)) {
		return ANGE_CAPTURE_BAD_ARGUMENT_V1;
	}
	for (uint32_t i = 0; i < size; ++i) {
		const double offset = gradient->get_offset(i);
		const auto color = gradient->get_color(i);
		if (i && !(stops[i - 1].offset < offset)) {
			return ANGE_CAPTURE_BAD_ARGUMENT_V1;
		}
		stops[i] = { offset, double(color.r), double(color.g), double(color.b), double(color.a) };
		++*stops_written;
	}
	for (uint32_t i = 0; i < count; ++i) {
		const auto color = gradient->get_color_at_offset(float(x[i]));
		if (!finite_color(color)) {
			return ANGE_CAPTURE_BAD_ARGUMENT_V1;
		}
		const uint32_t base = i * 4;
		rgba[base] = color.r;
		rgba[base + 1] = color.g;
		rgba[base + 2] = color.b;
		rgba[base + 3] = color.a;
		*rgba_written += 4;
	}
	return gradient->get_content_revision() == *revision ? ANGE_CAPTURE_OK_V1 : ANGE_CAPTURE_SOURCE_BUSY_V1;
}

uint32_t ange_texture_snapshot_plan_v1(void *source, uint32_t output_format, AngeTextureSnapshotPlanV1 *out) {
	if (!aligned(out) || !disjoint_spans({ { out, sizeof(*out) } })) {
		return ANGE_TEXTURE_BAD_ARGUMENT_V1;
	}
	*out = {};
	TextureView view;
	const uint32_t status = texture_view(source, view);
	if (status != ANGE_TEXTURE_OK_V1) {
		return status;
	}
	AngeTextureSnapshotPlanV1 plan;
	const uint32_t plan_status = build_texture_plan(view, output_format, plan);
	if (plan_status == ANGE_TEXTURE_OK_V1) {
		*out = plan;
	}
	return plan_status;
}

uint32_t ange_texture_snapshot_v1(void *source, const AngeTextureSnapshotPlanV1 *expected,
		uint8_t *output, uint64_t output_capacity, void *scratch, uint64_t scratch_capacity,
		AngeTextureSnapshotResultV1 *out) {
	// Validate caller storage before reading a plan or clearing any result.
	// The output is a byte span; RGBAF is packed with memcpy and needs no alignment.
	if (!aligned(expected) || !aligned(out) ||
			(scratch_capacity && uintptr_t(scratch) % alignof(real_t)) ||
			!disjoint_spans({ { expected, sizeof(*expected) }, { out, sizeof(*out) },
					{ output, output_capacity }, { scratch, scratch_capacity } })) {
		return ANGE_TEXTURE_BAD_ARGUMENT_V1;
	}
	*out = {};
	out->status = ANGE_TEXTURE_BAD_ARGUMENT_V1;
	const auto fail = [&](uint32_t status) {
		if (status != ANGE_TEXTURE_OK_V1) {
			*out = {};
		}
		out->status = status;
		return status;
	};
	TextureView view;
	const uint32_t source_status = texture_view(source, view);
	if (source_status != ANGE_TEXTURE_OK_V1) {
		return fail(source_status);
	}
	AngeTextureSnapshotPlanV1 current;
	const uint32_t plan_status = build_texture_plan(view, expected->output_format, current);
	if (plan_status != ANGE_TEXTURE_OK_V1) {
		return fail(plan_status);
	}
	if (!same_texture_source(expected->source, current.source)) {
		return fail(ANGE_TEXTURE_STALE_SOURCE_V1);
	}
	if (!same_texture_plan(*expected, current)) {
		return fail(ANGE_TEXTURE_BAD_ARGUMENT_V1);
	}
	if (current.output_bytes > output_capacity || (current.output_bytes && !output) ||
			current.scratch_bytes > scratch_capacity || (current.scratch_bytes && !scratch)) {
		return fail(current.output_bytes > output_capacity || current.scratch_bytes > scratch_capacity ? ANGE_TEXTURE_LIMIT_V1 : ANGE_TEXTURE_BAD_ARGUMENT_V1);
	}

	if (view.source_kind == ANGE_TEXTURE_SOURCE_CURVE_V1) {
		const auto *texture = static_cast<const CurveTexture *>(view.wrapper);
		const auto *curve = static_cast<const Curve *>(view.child);
		real_t *baked = static_cast<real_t *>(scratch);
		if (curve) {
			if (!finite_curve_points(*curve)) {
				return fail(ANGE_TEXTURE_INVALID_SOURCE_V1);
			}
			// Check the exact native bake positions and floor-to-int conversion
			// before invoking shared helpers; invalid inputs must not log callbacks.
			for (uint32_t i = 1; i + 1 < current.bake_resolution; ++i) {
				const real_t x = curve->get_domain_range() * i / static_cast<real_t>(current.bake_resolution - 1) + curve->get_min_domain();
				if (!std::isfinite(x)) {
					return fail(ANGE_TEXTURE_INVALID_SOURCE_V1);
				}
			}
			if (current.bake_resolution > 1) {
				for (uint32_t i = 0; i < view.width; ++i) {
					const real_t offset = i / static_cast<float>(view.width);
					const real_t fi = (offset - curve->get_min_domain()) / curve->get_domain_range() * (current.bake_resolution - 1);
					if (!std::isfinite(fi) || double(Math::floor(fi)) < std::numeric_limits<int>::min() || double(Math::floor(fi)) > std::numeric_limits<int>::max()) {
						return fail(ANGE_TEXTURE_INVALID_SOURCE_V1);
					}
				}
			}
			ResourceSnapshotAccess::fill(*curve, baked, current.bake_resolution);
			for (uint32_t i = 0; i < current.bake_resolution; ++i) {
				if (!std::isfinite(baked[i])) {
					return fail(ANGE_TEXTURE_INVALID_SOURCE_V1);
				}
			}
		}
		for (uint32_t i = 0; i < view.width; ++i) {
			const real_t offset = i / static_cast<float>(view.width);
			const float value = curve ? float(ResourceSnapshotAccess::sample(*curve, offset, baked, current.bake_resolution)) : 0.0f;
			if (!valid_float(value)) {
				return fail(ANGE_TEXTURE_INVALID_SOURCE_V1);
			}
			uint8_t *pixel = output + uint64_t(i) * 16;
			if (texture->get_texture_mode() == CurveTexture::TEXTURE_MODE_RGB) {
				write_f32(pixel + 0, value);
				write_f32(pixel + 4, value);
				write_f32(pixel + 8, value);
			} else {
				write_f32(pixel + 0, value);
				write_f32(pixel + 4, 0.0f);
				write_f32(pixel + 8, 0.0f);
			}
			write_f32(pixel + 12, 1.0f);
			++out->initialized_pixels;
		}
	} else {
		const auto *texture = static_cast<const GradientTexture1D *>(view.wrapper);
		Gradient *gradient = static_cast<Gradient *>(view.child);
		const auto mode = gradient->get_interpolation_mode();
		const auto space = gradient->get_interpolation_color_space();
		if (mode < Gradient::GRADIENT_INTERPOLATE_LINEAR || mode > Gradient::GRADIENT_INTERPOLATE_CUBIC ||
				space < Gradient::GRADIENT_COLOR_SPACE_SRGB || space > Gradient::GRADIENT_COLOR_SPACE_OKLAB ||
				!ResourceSnapshotAccess::finite_points(*gradient)) {
			return fail(ANGE_TEXTURE_INVALID_SOURCE_V1);
		}
		for (uint32_t i = 0; i < view.width; ++i) {
			const Color color = gradient->get_color_at_offset(float(i) / float(view.width - 1));
			if (!finite_color(color)) {
				return fail(ANGE_TEXTURE_INVALID_SOURCE_V1);
			}
			uint8_t *pixel = output + uint64_t(i) * (current.output_format == ANGE_TEXTURE_FORMAT_RGBAF_V1 ? 16 : 4);
			if (texture->is_using_hdr()) {
				write_f32(pixel + 0, float(color.r));
				write_f32(pixel + 4, float(color.g));
				write_f32(pixel + 8, float(color.b));
				write_f32(pixel + 12, float(color.a));
			} else {
				pixel[0] = color.get_r8();
				pixel[1] = color.get_g8();
				pixel[2] = color.get_b8();
				pixel[3] = color.get_a8();
			}
			++out->initialized_pixels;
		}
	}
	TextureView after;
	const uint32_t after_status = texture_view(source, after);
	if (after_status != ANGE_TEXTURE_OK_V1) {
		return fail(after_status);
	}
	out->source = texture_source(after);
	if (!same_texture_source(out->source, expected->source)) {
		return fail(ANGE_TEXTURE_STALE_SOURCE_V1);
	}
	return fail(ANGE_TEXTURE_OK_V1);
}

void register_resource_snapshot_bridge() {
	GDExtension::register_interface_function(ANGE_RESOURCE_CONTENT_REVISION_PROC_V1, reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&ange_resource_content_revision_v1));
	GDExtension::register_interface_function(ANGE_CURVE_SNAPSHOT_PROC_V1, reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&ange_curve_snapshot_v1));
	GDExtension::register_interface_function(ANGE_GRADIENT_SNAPSHOT_PROC_V1, reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&ange_gradient_snapshot_v1));
	GDExtension::register_interface_function(ANGE_TEXTURE_SNAPSHOT_PLAN_PROC_V1, reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&ange_texture_snapshot_plan_v1));
	GDExtension::register_interface_function(ANGE_TEXTURE_SNAPSHOT_PROC_V1, reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&ange_texture_snapshot_v1));
}
