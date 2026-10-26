/**************************************************************************/
/*  skinned_mesh_capture_test_gpu_payload.cpp                             */
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

#include "skinned_mesh_capture_test_gpu_payload.h"

#include "core/io/marshalls.h"

Error skinned_mesh_capture_test_encode_gpu_payload(const SkinnedMeshCaptureTestPublicSnapshot &p_snapshot, PackedByteArray &r_bytes) {
	PackedByteArray bytes;
	bytes.resize(228);
	uint8_t *out = bytes.ptrw();
	for (int vertex = 0; vertex < 3; vertex++) {
		for (int axis = 0; axis < 3; axis++) {
			const float value = p_snapshot.positions[vertex][axis];
			if (!Math::is_finite(value)) {
				return ERR_INVALID_DATA;
			}
			encode_float(value, out + (vertex * 3 + axis) * 4);
		}
		uint32_t sum = 0;
		for (int lane = 0; lane < 4; lane++) {
			const int i = vertex * 4 + lane;
			const int32_t index = p_snapshot.bind_indices[i];
			const int32_t weight = p_snapshot.weights_u16[i];
			if (index < 0 || index > 1 || weight < 0 || weight > 65535) {
				return ERR_INVALID_DATA;
			}
			sum += weight;
			encode_uint32(index, out + 132 + i * 4);
			encode_uint32(weight, out + 180 + i * 4);
		}
		if (sum != 65535) {
			return ERR_INVALID_DATA;
		}
	}
	for (int i = 0; i < 24; i++) {
		if (!Math::is_finite(p_snapshot.pose_matrices[i])) {
			return ERR_INVALID_DATA;
		}
		encode_float(p_snapshot.pose_matrices[i], out + 36 + i * 4);
	}
	r_bytes = bytes;
	return OK;
}

PackedByteArray skinned_mesh_capture_test_initial_guarded_buffer() {
	PackedByteArray bytes;
	bytes.resize(64);
	for (int i = 0; i < 16; i++) {
		encode_uint32(i < 9 ? 0x7fc54321u : 0x7fc12345u, bytes.ptrw() + i * 4);
	}
	return bytes;
}
