#include "p01_public_snapshot.h"

#include "core/crypto/crypto_core.h"
#include "core/io/marshalls.h"
#include "core/variant/variant.h"

#include <limits>

namespace {

bool canonical_object_id(const String &p_text, uint64_t &r_id) {
	if (p_text.is_empty() || p_text.length() > 20 || p_text[0] < '1' || p_text[0] > '9') {
		return false;
	}
	uint64_t value = 0;
	for (int i = 0; i < p_text.length(); i++) {
		const char32_t c = p_text[i];
		if (c < '0' || c > '9') {
			return false;
		}
		const uint64_t digit = c - '0';
		if (value > (std::numeric_limits<uint64_t>::max() - digit) / 10) {
			return false;
		}
		value = value * 10 + digit;
	}
	r_id = value;
	return true;
}

bool digest_string(const String &p_text) {
	if (p_text.length() != 64) {
		return false;
	}
	for (int i = 0; i < 64; i++) {
		if (!((p_text[i] >= '0' && p_text[i] <= '9') || (p_text[i] >= 'a' && p_text[i] <= 'f'))) {
			return false;
		}
	}
	return true;
}

} // namespace

Error p01_decode_public_snapshot(const Dictionary &p_value, P01PublicSnapshot &r_snapshot) {
	static const struct Field {
		const char *name;
		Variant::Type type;
	} fields[] = {
		{ "epoch", Variant::INT },
		{ "request_id", Variant::INT },
		{ "source_object_ids", Variant::DICTIONARY },
		{ "source_content_digest", Variant::STRING },
		{ "bind_digest", Variant::STRING },
		{ "pose_matrices", Variant::PACKED_FLOAT32_ARRAY },
		{ "source_to_world", Variant::TRANSFORM3D },
		{ "positions", Variant::PACKED_VECTOR3_ARRAY },
		{ "bind_indices", Variant::PACKED_INT32_ARRAY },
		{ "weights_u16", Variant::PACKED_INT32_ARRAY },
		{ "sample_kind", Variant::STRING },
		{ "pose_signal_ordinal", Variant::INT },
		{ "observed_signal_ordinal", Variant::INT },
	};
	if (p_value.size() != 13) {
		return ERR_INVALID_DATA;
	}
	// Dictionary treats StringName and String as equal lookup keys. Inspect
	// stored keys as well, before any conversion can hide the wrong wire type.
	for (const KeyValue<Variant, Variant> &entry : p_value) {
		if (entry.key.get_type() != Variant::STRING) {
			return ERR_INVALID_DATA;
		}
	}
	for (const Field &field : fields) {
		const Variant *value = p_value.getptr(field.name);
		if (!value || value->get_type() != field.type) {
			return ERR_INVALID_DATA;
		}
	}

	P01PublicSnapshot candidate;
	candidate.epoch = p_value["epoch"];
	candidate.request_id = p_value["request_id"];
	candidate.pose_signal_ordinal = p_value["pose_signal_ordinal"];
	candidate.observed_signal_ordinal = p_value["observed_signal_ordinal"];
	const String kind = p_value["sample_kind"];
	if (candidate.epoch <= 0 || candidate.request_id < 1 || candidate.request_id > 3 ||
			candidate.pose_signal_ordinal <= 0 || candidate.pose_signal_ordinal != candidate.observed_signal_ordinal ||
			(kind != "pose_signal" && kind != "mesh_refresh")) {
		return ERR_INVALID_DATA;
	}
	candidate.mesh_refresh = kind == "mesh_refresh";
	const Dictionary ids = p_value["source_object_ids"];
	static const char *id_names[] = { "source_node", "mesh_instance", "skeleton_node", "mesh", "skin_reference", "skin" };
	if (ids.size() != 6) {
		return ERR_INVALID_DATA;
	}
	for (const KeyValue<Variant, Variant> &entry : ids) {
		if (entry.key.get_type() != Variant::STRING || entry.value.get_type() != Variant::STRING) {
			return ERR_INVALID_DATA;
		}
	}
	for (int i = 0; i < 6; i++) {
		const Variant *value = ids.getptr(id_names[i]);
		if (!value || !canonical_object_id(*value, candidate.source_object_ids[i])) {
			return ERR_INVALID_DATA;
		}
	}
	candidate.source_content_digest = p_value["source_content_digest"];
	candidate.bind_digest = p_value["bind_digest"];
	if (!digest_string(candidate.source_content_digest) || !digest_string(candidate.bind_digest)) {
		return ERR_INVALID_DATA;
	}
	candidate.source_to_world = p_value["source_to_world"];
	const PackedFloat32Array matrices = p_value["pose_matrices"];
	const PackedVector3Array positions = p_value["positions"];
	const PackedInt32Array indices = p_value["bind_indices"];
	const PackedInt32Array weights = p_value["weights_u16"];
	if (!candidate.source_to_world.is_finite() || matrices.size() != 24 || positions.size() != 3 || indices.size() != 12 || weights.size() != 12) {
		return ERR_INVALID_DATA;
	}
	for (int i = 0; i < 24; i++) {
		if (!Math::is_finite(matrices[i])) {
			return ERR_INVALID_DATA;
		}
		candidate.pose_matrices[i] = matrices[i];
	}
	uint8_t source_bytes[84];
	for (int vertex = 0; vertex < 3; vertex++) {
		if (!positions[vertex].is_finite()) {
			return ERR_INVALID_DATA;
		}
		candidate.positions[vertex] = positions[vertex];
		for (int axis = 0; axis < 3; axis++) {
			const float component = positions[vertex][axis];
			if (!Math::is_finite(component)) { // Also reject double->float overflow.
				return ERR_INVALID_DATA;
			}
			encode_float(component, source_bytes + (vertex * 3 + axis) * 4);
		}
		uint32_t sum = 0;
		for (int lane = 0; lane < 4; lane++) {
			const int i = vertex * 4 + lane;
			if (indices[i] < 0 || indices[i] > 1 || weights[i] < 0 || weights[i] > 65535) {
				return ERR_INVALID_DATA;
			}
			candidate.bind_indices[i] = indices[i];
			candidate.weights_u16[i] = weights[i];
			sum += weights[i];
			encode_uint16(indices[i], source_bytes + 36 + i * 2);
			encode_uint16(weights[i], source_bytes + 60 + i * 2);
		}
		if (sum != 65535) {
			return ERR_INVALID_DATA;
		}
	}
	uint8_t digest[32];
	const Error hash_error = CryptoCore::sha256(source_bytes, sizeof(source_bytes), digest);
	if (hash_error != OK) {
		return hash_error;
	}
	if (String::hex_encode_buffer(digest, sizeof(digest)) != candidate.source_content_digest) {
		return ERR_INVALID_DATA;
	}
	r_snapshot = candidate;
	return OK;
}
