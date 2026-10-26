/**************************************************************************/
/*  test_gpu_payload.h                                                    */
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

#include "core/io/marshalls.h"
#include "tests/test_macros.h"

#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_gpu_payload.h"

#include <limits>

namespace TestSkinnedMeshCaptureTestGpuPayload {
TEST_CASE("[P01] GPU payload has exact scalar offsets and independent guards") {
	SkinnedMeshCaptureTestPublicSnapshot snapshot;
	snapshot.positions[1] = Vector3(1, 0, 0);
	snapshot.positions[2] = Vector3(0, 1, 0);
	snapshot.pose_matrices[3] = 7;
	snapshot.pose_matrices[23] = -2;
	snapshot.bind_indices[0] = 1;
	snapshot.weights_u16[0] = 65535;
	snapshot.weights_u16[4] = 65535;
	snapshot.weights_u16[8] = 32768;
	snapshot.weights_u16[9] = 32767;
	PackedByteArray bytes;
	CHECK(skinned_mesh_capture_test_encode_gpu_payload(snapshot, bytes) == OK);
	CHECK(bytes.size() == 228);
	if (bytes.size() == 228) {
		CHECK(decode_float(bytes.ptr() + 12) == 1);
		CHECK(decode_float(bytes.ptr() + 48) == 7);
		CHECK(decode_float(bytes.ptr() + 128) == -2);
		CHECK(decode_uint32(bytes.ptr() + 132) == 1);
		CHECK(decode_uint32(bytes.ptr() + 180) == 65535);
		CHECK(decode_uint32(bytes.ptr() + 212) == 32768);
		CHECK(decode_uint32(bytes.ptr() + 216) == 32767);
		CHECK(bytes[14] == 0x80); // float32 1.0 = 00 00 80 3f, little endian.
		CHECK(bytes[15] == 0x3f);
		CHECK(bytes[180] == 0xff);
		CHECK(bytes[181] == 0xff);
		CHECK(bytes[182] == 0);
		CHECK(bytes[224] == 0);
	}
	const PackedByteArray valid = bytes;
	snapshot.bind_indices[1] = 2;
	CHECK(skinned_mesh_capture_test_encode_gpu_payload(snapshot, bytes) == ERR_INVALID_DATA);
	CHECK(bytes == valid);
	snapshot.bind_indices[1] = 0;
	snapshot.weights_u16[0] = 65534;
	CHECK(skinned_mesh_capture_test_encode_gpu_payload(snapshot, bytes) == ERR_INVALID_DATA);
	CHECK(bytes == valid);
	snapshot.weights_u16[0] = 65535;
	snapshot.pose_matrices[0] = std::numeric_limits<float>::infinity();
	CHECK(skinned_mesh_capture_test_encode_gpu_payload(snapshot, bytes) == ERR_INVALID_DATA);
	CHECK(bytes == valid);
	snapshot.pose_matrices[0] = 0;
	snapshot.positions[0].x = std::numeric_limits<float>::quiet_NaN();
	CHECK(skinned_mesh_capture_test_encode_gpu_payload(snapshot, bytes) == ERR_INVALID_DATA);
	CHECK(bytes == valid);
	snapshot.positions[0].x = -0.0f;
	CHECK(skinned_mesh_capture_test_encode_gpu_payload(snapshot, bytes) == OK);
	if (bytes.size() == 228) {
		CHECK(bytes[0] == 0);
		CHECK(bytes[3] == 0x80);
	}
	PackedByteArray first = skinned_mesh_capture_test_initial_guarded_buffer();
	PackedByteArray second = skinned_mesh_capture_test_initial_guarded_buffer();
	CHECK(first.size() == 64);
	CHECK(second.size() == 64);
	if (first.size() == 64 && second.size() == 64) {
		for (int i = 0; i < 16; i++) {
			CHECK(decode_uint32(first.ptr() + i * 4) == (i < 9 ? 0x7fc54321u : 0x7fc12345u));
		}
		first.set(0, 0);
		CHECK(decode_uint32(second.ptr()) == 0x7fc54321u);
	}
}
} //namespace TestSkinnedMeshCaptureTestGpuPayload
