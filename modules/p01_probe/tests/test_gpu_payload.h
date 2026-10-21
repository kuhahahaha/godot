#pragma once

#include "modules/p01_probe/p01_gpu_payload.h"
#include "core/io/marshalls.h"
#include "tests/test_macros.h"
#include <limits>

namespace TestP01GpuPayload {
TEST_CASE("[P01] GPU payload has exact scalar offsets and independent guards") {
	P01PublicSnapshot snapshot;
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
	CHECK(p01_encode_gpu_payload(snapshot, bytes) == OK);
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
	CHECK(p01_encode_gpu_payload(snapshot, bytes) == ERR_INVALID_DATA);
	CHECK(bytes == valid);
	snapshot.bind_indices[1] = 0;
	snapshot.weights_u16[0] = 65534;
	CHECK(p01_encode_gpu_payload(snapshot, bytes) == ERR_INVALID_DATA);
	CHECK(bytes == valid);
	snapshot.weights_u16[0] = 65535;
	snapshot.pose_matrices[0] = std::numeric_limits<float>::infinity();
	CHECK(p01_encode_gpu_payload(snapshot, bytes) == ERR_INVALID_DATA);
	CHECK(bytes == valid);
	snapshot.pose_matrices[0] = 0;
	snapshot.positions[0].x = std::numeric_limits<float>::quiet_NaN();
	CHECK(p01_encode_gpu_payload(snapshot, bytes) == ERR_INVALID_DATA);
	CHECK(bytes == valid);
	snapshot.positions[0].x = -0.0f;
	CHECK(p01_encode_gpu_payload(snapshot, bytes) == OK);
	if (bytes.size() == 228) {
		CHECK(bytes[0] == 0);
		CHECK(bytes[3] == 0x80);
	}
	PackedByteArray first = p01_initial_guarded_buffer();
	PackedByteArray second = p01_initial_guarded_buffer();
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
}
