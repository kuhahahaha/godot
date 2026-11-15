/**************************************************************************/
/*  affine_f32.h                                                          */
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

#include <cmath>
#include <cstdint>

// Pure arithmetic for an original row-major affine[12]. Callers validate all
// lanes as finite f32 before entry; no Host/source qualification is issued.
namespace AffineF32 {
// Six signed triple products of F32 inputs. A product of two F32 values
// is exact in double; FMA retains the rounding residual of the third factor.
// F32 triple exponents fit the normal double range, including subnormals.
inline double exact_basis_determinant(const float *affine) {
	double expansion[12]{};
	uint32_t count = 0;
	auto grow = [&](double value) {
		double next[12]{};
		uint32_t next_count = 0;
		for (uint32_t i = 0; i < count; ++i) {
			const double sum = value + expansion[i];
			const double b_virtual = sum - value;
			const double a_virtual = sum - b_virtual;
			const double residual = (value - a_virtual) + (expansion[i] - b_virtual);
			if (residual != 0) {
				next[next_count++] = residual;
			}
			value = sum;
		}
		if (value != 0 || !next_count) {
			next[next_count++] = value;
		}
		count = next_count;
		for (uint32_t i = 0; i < count; ++i) {
			expansion[i] = next[i];
		}
	};
	auto triple = [&](uint32_t a, uint32_t b, uint32_t c, double sign) {
		const double pair = double(affine[a]) * double(affine[b]);
		const double product = pair * double(affine[c]);
		const double residual = std::fma(pair, double(affine[c]), -product);
		grow(sign * residual);
		grow(sign * product);
	};
	triple(0, 5, 10, 1);
	triple(1, 6, 8, 1);
	triple(2, 4, 9, 1);
	triple(2, 5, 8, -1);
	triple(1, 4, 10, -1);
	triple(0, 6, 9, -1);
	// A zero-eliminating expansion is exactly zero iff its highest component
	// is zero. Sum from small to large only after that exact classification.
	if (expansion[count - 1] == 0) {
		return 0;
	}
	double determinant = 0;
	for (uint32_t i = 0; i < count; ++i) {
		determinant += expansion[i];
	}
	return determinant;
}
} // namespace AffineF32
