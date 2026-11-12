/**************************************************************************/
/*  native_output_cull.h                                                  */
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

#pragma once

#include "core/error/error_list.h"
#include "core/math/affine_f32.h"
#include "core/math/transform_3d.h"

#include <cmath>
#include <limits>

// Numeric conversion only. The caller retains/authenticates all source,
// history, query and qualified Host inputs; this creates no view/record grant.
namespace NativeOutputCull {
struct Envelope {
	AABB world;
	AABB local;
};

namespace Detail {
struct Interval {
	double low;
	double high;
};

inline double down(double value) {
	return std::nextafter(value, -std::numeric_limits<double>::infinity());
}
inline double up(double value) {
	return std::nextafter(value, std::numeric_limits<double>::infinity());
}
inline Interval add(Interval a, Interval b) {
	return { down(a.low + b.low), up(a.high + b.high) };
}
inline Interval multiply(double a, double b) {
	const double value = a * b;
	return { down(value), up(value) };
}

inline bool nonsingular(const float *host) {
	// Use the original finite-f32 Host classifier. A widened determinant
	// interval containing zero is inconclusive, not proof of singularity.
	const double determinant = AffineF32::exact_basis_determinant(host);
	return std::isfinite(determinant) && determinant != 0;
}

inline bool valid_box(const AABB &box) {
	for (int axis = 0; axis < 3; ++axis) {
		const double low = box.position[axis];
		const double size = box.size[axis];
		const real_t native_end = box.position[axis] + box.size[axis];
		if (!std::isfinite(low) || !std::isfinite(size) || size <= 0 ||
				!std::isfinite(double(native_end)) || native_end <= box.position[axis]) {
			return false;
		}
	}
	return true;
}

inline bool pack(const double *low, const double *high, AABB &out) {
	AABB packed;
	for (int axis = 0; axis < 3; ++axis) {
		if (!std::isfinite(low[axis]) || !std::isfinite(high[axis]) || low[axis] >= high[axis] ||
				low[axis] < -double(std::numeric_limits<float>::max()) || high[axis] > double(std::numeric_limits<float>::max())) {
			return false;
		}
		float minimum = float(low[axis]);
		float maximum = float(high[axis]);
		if (double(minimum) > low[axis]) {
			minimum = std::nextafter(minimum, -std::numeric_limits<float>::infinity());
		}
		if (double(maximum) < high[axis]) {
			maximum = std::nextafter(maximum, std::numeric_limits<float>::infinity());
		}
		if (!std::isfinite(minimum) || !std::isfinite(maximum)) {
			return false;
		}
		const double width = double(maximum) - double(minimum);
		float size = float(width);
		if (double(size) < width) {
			size = std::nextafter(size, std::numeric_limits<float>::infinity());
		}
		// AABB stores position/size, not two endpoints. Check the actual native
		// rounded sum as well, so packing cannot lose the outward upper bound.
		for (int attempt = 0; attempt < 2 && double(real_t(minimum) + real_t(size)) < double(maximum); ++attempt) {
			size = std::nextafter(size, std::numeric_limits<float>::infinity());
		}
		const real_t end = real_t(minimum) + real_t(size);
		if (!std::isfinite(size) || size <= 0 || !std::isfinite(double(end)) || double(end) < high[axis]) {
			return false;
		}
		packed.position[axis] = minimum;
		packed.size[axis] = size;
	}
	if (!valid_box(packed)) {
		return false;
	}
	out = packed;
	return true;
}

inline bool inverse_consistent(const float *host, const float *inverse) {
	// Sanity-check the provided *already qualified* f32 inverse in both
	// directions. This is not inverse qualification or a bounds tolerance:
	// final containment below is a hard native comparison without epsilon.
	for (int order = 0; order < 2; ++order) {
		const float *a = order ? inverse : host;
		const float *b = order ? host : inverse;
		for (int row = 0; row < 3; ++row) {
			for (int column = 0; column < 4; ++column) {
				double sum = column == 3 ? a[row * 4 + 3] : 0;
				double magnitude = std::abs(sum);
				for (int k = 0; k < 3; ++k) {
					const double product = double(a[row * 4 + k]) * double(b[k * 4 + column]);
					sum += product;
					magnitude += std::abs(product);
				}
				const double expected = row == column ? 1 : 0;
				const double budget = 16 * double(std::numeric_limits<float>::epsilon()) * (magnitude > 1 ? magnitude : 1);
				if (!std::isfinite(sum) || std::abs(sum - expected) > budget) {
					return false;
				}
			}
		}
	}
	return true;
}
} // namespace Detail

// Local declaration to World only. Its own original qualified Host is a
// caller precondition, independently for current and retained prior. This
// helper neither requalifies Host nor authenticates either source.
inline Error transform_bounds(const AABB &declared, const float *own_host_affine12, AABB &world) {
	if (sizeof(real_t) != sizeof(float)) {
		return ERR_UNAVAILABLE;
	}
	if (!own_host_affine12 || !Detail::valid_box(declared)) {
		return ERR_INVALID_PARAMETER;
	}
	for (int i = 0; i < 12; ++i) {
		if (!std::isfinite(own_host_affine12[i])) {
			return ERR_INVALID_DATA;
		}
	}
	double input_high[3];
	for (int axis = 0; axis < 3; ++axis) {
		const double mathematical_end = double(declared.position[axis]) + double(declared.size[axis]);
		const double native_end = declared.position[axis] + declared.size[axis];
		input_high[axis] = mathematical_end > native_end ? mathematical_end : native_end;
	}
	double low[3] = { INFINITY, INFINITY, INFINITY };
	double high[3] = { -INFINITY, -INFINITY, -INFINITY };
	for (int corner = 0; corner < 8; ++corner) {
		for (int row = 0; row < 3; ++row) {
			Detail::Interval value{ own_host_affine12[row * 4 + 3], own_host_affine12[row * 4 + 3] };
			for (int axis = 0; axis < 3; ++axis) {
				const double coordinate = (corner & (1 << axis)) ? input_high[axis] : double(declared.position[axis]);
				value = Detail::add(value, Detail::multiply(own_host_affine12[row * 4 + axis], coordinate));
			}
			low[row] = value.low < low[row] ? value.low : low[row];
			high[row] = value.high > high[row] ? value.high : high[row];
		}
	}
	AABB candidate;
	if (!Detail::pack(low, high, candidate)) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	world = candidate;
	return OK;
}

inline Error compose(const AABB &current_world, const AABB *authenticated_candidate_previous_world, const AABB *query_world,
		const float *current_host_affine12, const float *qualified_current_host_inverse12, Envelope &out) {
	if (sizeof(real_t) != sizeof(float)) {
		return ERR_UNAVAILABLE;
	}
	if (!current_host_affine12 || !qualified_current_host_inverse12 || !Detail::valid_box(current_world)) {
		return ERR_INVALID_PARAMETER;
	}
	for (int i = 0; i < 12; ++i) {
		if (!std::isfinite(current_host_affine12[i]) || !std::isfinite(qualified_current_host_inverse12[i])) {
			return ERR_INVALID_DATA;
		}
	}
	if (!Detail::nonsingular(current_host_affine12) || !Detail::inverse_consistent(current_host_affine12, qualified_current_host_inverse12)) {
		return ERR_INVALID_DATA;
	}
	double world_low[3];
	double world_high[3];
	for (int axis = 0; axis < 3; ++axis) {
		world_low[axis] = current_world.position[axis];
		world_high[axis] = double(current_world.position[axis]) + double(current_world.size[axis]);
	}
	const AABB *additional[] = { authenticated_candidate_previous_world, query_world };
	for (const AABB *box : additional) {
		if (!box) {
			continue;
		}
		if (!Detail::valid_box(*box)) {
			return ERR_INVALID_PARAMETER;
		}
		for (int axis = 0; axis < 3; ++axis) {
			const double low = box->position[axis];
			const double high = double(box->position[axis]) + double(box->size[axis]);
			world_low[axis] = low < world_low[axis] ? low : world_low[axis];
			world_high[axis] = high > world_high[axis] ? high : world_high[axis];
		}
	}
	Envelope candidate;
	if (!Detail::pack(world_low, world_high, candidate.world)) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	// Use the outward native world envelope's endpoints, not an inward
	// float inverse/xform. Each corner multiply and addition is widened.
	double local_low[3] = { INFINITY, INFINITY, INFINITY };
	double local_high[3] = { -INFINITY, -INFINITY, -INFINITY };
	for (int corner = 0; corner < 8; ++corner) {
		for (int row = 0; row < 3; ++row) {
			Detail::Interval value{ qualified_current_host_inverse12[row * 4 + 3], qualified_current_host_inverse12[row * 4 + 3] };
			for (int axis = 0; axis < 3; ++axis) {
				const double coordinate = (corner & (1 << axis)) ? candidate.world.position[axis] + candidate.world.size[axis] : candidate.world.position[axis];
				value = Detail::add(value, Detail::multiply(qualified_current_host_inverse12[row * 4 + axis], coordinate));
			}
			local_low[row] = value.low < local_low[row] ? value.low : local_low[row];
			local_high[row] = value.high > local_high[row] ? value.high : local_high[row];
		}
	}
	if (!Detail::pack(local_low, local_high, candidate.local)) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	Transform3D host;
	for (int row = 0; row < 3; ++row) {
		for (int column = 0; column < 3; ++column) {
			host.basis[row][column] = current_host_affine12[row * 4 + column];
		}
		host.origin[row] = current_host_affine12[row * 4 + 3];
	}
	// This is the exact ordinary native cull calculation. A conservative
	// inverse interval alone does not prove the actual f32 result encloses.
	const AABB reapplied = host.xform(candidate.local);
	if (!Detail::valid_box(reapplied)) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	for (int axis = 0; axis < 3; ++axis) {
		if (reapplied.position[axis] > candidate.world.position[axis] ||
				reapplied.position[axis] + reapplied.size[axis] < candidate.world.position[axis] + candidate.world.size[axis]) {
			return ERR_INVALID_DATA;
		}
	}
	out = candidate;
	return OK;
}
} // namespace NativeOutputCull
