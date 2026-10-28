#include "resource_snapshot_native.h"

#include "core/extension/gdextension.h"
#include "core/os/thread.h"
#include "scene/resources/curve.h"
#include "scene/resources/gradient.h"

#include <cmath>
#include <initializer_list>
#include <limits>

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
struct Span {
	const void *data;
	uint64_t bytes;
};
bool disjoint_spans(std::initializer_list<Span> spans) {
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
} //namespace

// Raw finite validation precedes Gradient's first sorting accessor. A NaN
// offset must never reach the native sort comparator; no duplicate array.
class ResourceSnapshotAccess {
public:
	static bool finite_points(const Gradient &g) {
		for (const auto &p : g.points) {
			if (!finite_f32(p.offset) || !finite_color(p.color)) {
				return false;
			}
		}
		return true;
	}
};

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

void register_resource_snapshot_bridge() {
	GDExtension::register_interface_function(ANGE_RESOURCE_CONTENT_REVISION_PROC_V1, reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&ange_resource_content_revision_v1));
	GDExtension::register_interface_function(ANGE_CURVE_SNAPSHOT_PROC_V1, reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&ange_curve_snapshot_v1));
	GDExtension::register_interface_function(ANGE_GRADIENT_SNAPSHOT_PROC_V1, reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&ange_gradient_snapshot_v1));
}
