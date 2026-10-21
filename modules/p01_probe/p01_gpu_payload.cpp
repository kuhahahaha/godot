#include "p01_gpu_payload.h"
#include "core/io/marshalls.h"

Error p01_encode_gpu_payload(const P01PublicSnapshot &p_snapshot, PackedByteArray &r_bytes) {
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

PackedByteArray p01_initial_guarded_buffer() {
	PackedByteArray bytes;
	bytes.resize(64);
	for (int i = 0; i < 16; i++) {
		encode_uint32(i < 9 ? 0x7fc54321u : 0x7fc12345u, bytes.ptrw() + i * 4);
	}
	return bytes;
}
