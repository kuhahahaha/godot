#pragma once

#include "modules/p01_probe/p01_public_snapshot.h"
#include "tests/test_macros.h"

#include <limits>

namespace TestP01PublicSnapshot {

static Dictionary baseline() {
	Dictionary ids;
	const char *names[] = { "source_node", "mesh_instance", "skeleton_node", "mesh", "skin_reference", "skin" };
	for (int i = 0; i < 6; i++) {
		ids[names[i]] = String::num_uint64(uint64_t(i) + 1);
	}
	Dictionary value;
	value["epoch"] = int64_t(1);
	value["request_id"] = int64_t(1);
	value["source_object_ids"] = ids;
	// Independent Python struct.pack('<9f12H12H', ...) / hashlib.sha256.
	value["source_content_digest"] = "3001e53048ff4ed4974ddfe220f58436c84b082235daa009d0bf675e091a6711";
	value["bind_digest"] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
	value["pose_matrices"] = PackedFloat32Array{ 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };
	value["source_to_world"] = Transform3D();
	value["positions"] = PackedVector3Array{ Vector3(0, 0, 0), Vector3(1, 0, 0), Vector3(0, 1, 0) };
	value["bind_indices"] = PackedInt32Array{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	value["weights_u16"] = PackedInt32Array{ 65535, 0, 0, 0, 65535, 0, 0, 0, 65535, 0, 0, 0 };
	value["sample_kind"] = "pose_signal";
	value["pose_signal_ordinal"] = int64_t(1);
	value["observed_signal_ordinal"] = int64_t(1);
	return value;
}

TEST_CASE("[P01] Profile") {
	Dictionary value = baseline();
	P01PublicSnapshot decoded;
	REQUIRE(p01_decode_public_snapshot(value, decoded) == OK);
	CHECK(decoded.epoch == 1);
	CHECK(decoded.source_object_ids[5] == 6);
	CHECK(decoded.positions[1] == Vector3(1, 0, 0));
	CHECK(decoded.pose_matrices[3] == 1);
	CHECK(decoded.weights_u16[0] == 65535);
	CHECK_FALSE(decoded.mesh_refresh);

	// Replacing caller-owned packed arrays and nested values cannot mutate the
	// fixed-size snapshot the render-side consumer will receive.
	PackedVector3Array positions = value["positions"];
	positions.set(1, Vector3(99, 0, 0));
	value["positions"] = positions;
	Dictionary ids = value["source_object_ids"];
	ids["skin"] = "99";
	CHECK(decoded.positions[1] == Vector3(1, 0, 0));
	CHECK(decoded.source_object_ids[5] == 6);

	const auto reject = [&](const Dictionary &p_bad) {
		decoded.request_id = 42;
		CHECK(p01_decode_public_snapshot(p_bad, decoded) == ERR_INVALID_DATA);
		CHECK(decoded.request_id == 42);
	};
	for (const Variant &key : baseline().keys()) {
		Dictionary bad = baseline();
		bad.erase(key);
		reject(bad);
		bad = baseline();
		bad[key] = Variant();
		reject(bad);
	}
	Dictionary bad = baseline();
	bad["extra"] = 1;
	reject(bad);
	bad = baseline();
	bad.erase("epoch");
	bad[StringName("epoch")] = int64_t(1);
	reject(bad);
	for (const char *key : { "epoch", "request_id", "pose_signal_ordinal", "observed_signal_ordinal" }) {
		for (const Variant &invalid : { Variant(true), Variant(1.0), Variant("1"), Variant(int64_t(0)), Variant(int64_t(-1)) }) {
			bad = baseline();
			bad[key] = invalid;
			reject(bad);
		}
	}
	bad = baseline();
	bad["request_id"] = 4;
	reject(bad);
	bad = baseline();
	bad["observed_signal_ordinal"] = 2;
	reject(bad);
	bad = baseline();
	bad["sample_kind"] = "other";
	reject(bad);
	bad = baseline();
	bad["sample_kind"] = StringName("pose_signal");
	reject(bad);
	for (const char *invalid : { "0", "01", "-1", "+1", " 1", "1 ", "1.0", "18446744073709551616", "", "１" }) {
		bad = baseline();
		Dictionary bad_ids = bad["source_object_ids"];
		bad_ids["skin"] = invalid;
		reject(bad);
	}
	bad = baseline();
	Dictionary bad_ids = bad["source_object_ids"];
	bad_ids["skin"] = int64_t(6);
	reject(bad);
	bad = baseline();
	bad_ids = bad["source_object_ids"];
	bad_ids["extra"] = "7";
	reject(bad);
	bad = baseline();
	bad_ids = bad["source_object_ids"];
	bad_ids.erase("skin");
	bad_ids[StringName("skin")] = "6";
	reject(bad);
	for (const char *key : { "source_content_digest", "bind_digest" }) {
		for (const char *invalid : { "", "ABCDEF6789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", "g123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" }) {
			bad = baseline();
			bad[key] = invalid;
			reject(bad);
		}
	}
	bad = baseline();
	bad["source_content_digest"] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
	reject(bad);
	for (int count : { 0, 2, 4 }) {
		bad = baseline();
		PackedVector3Array points;
		points.resize(count);
		bad["positions"] = points;
		reject(bad);
	}
	for (float invalid : { std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() }) {
		bad = baseline();
		positions = bad["positions"];
		positions.set(0, Vector3(invalid, 0, 0));
		bad["positions"] = positions;
		reject(bad);
		bad = baseline();
		PackedFloat32Array pose = bad["pose_matrices"];
		pose.set(23, invalid);
		bad["pose_matrices"] = pose;
		reject(bad);
		bad = baseline();
		Transform3D world;
		world.origin.y = invalid;
		bad["source_to_world"] = world;
		reject(bad);
	}
	for (const char *key : { "bind_indices", "weights_u16" }) {
		bad = baseline();
		PackedInt32Array array = bad[key];
		array.resize(11);
		bad[key] = array;
		reject(bad);
		for (int invalid : { -1, 65536 }) {
			bad = baseline();
			array = bad[key];
			array.set(1, invalid);
			bad[key] = array;
			reject(bad);
		}
	}
	bad = baseline();
	PackedInt32Array indices = bad["bind_indices"];
	indices.set(1, 2); // A zero-weight lane still must have a valid bind slot.
	bad["bind_indices"] = indices;
	bad["source_content_digest"] = "fef94952178e4f63ecd7bbd4ffb20f71b9b295ddccef496cba3b00f65905c790";
	reject(bad);
	bad = baseline();
	PackedInt32Array weights = bad["weights_u16"];
	weights.set(0, 65534);
	bad["weights_u16"] = weights;
	bad["source_content_digest"] = "43bb9f296ee43c2761381633323878365c54cbeeeeaa97e1faf8412544f27893";
	reject(bad);
	bad = baseline();
	PackedFloat32Array pose = bad["pose_matrices"];
	pose.resize(23);
	bad["pose_matrices"] = pose;
	reject(bad);

	// Canonical full-range ObjectID and signed Godot INT bounds are legal.
	value = baseline();
	ids = value["source_object_ids"];
	ids["skin"] = "18446744073709551615";
	value["epoch"] = std::numeric_limits<int64_t>::max();
	value["request_id"] = 3;
	value["sample_kind"] = "mesh_refresh";
	REQUIRE(p01_decode_public_snapshot(value, decoded) == OK);
	CHECK(decoded.source_object_ids[5] == std::numeric_limits<uint64_t>::max());
	CHECK(decoded.mesh_refresh);

	// Q2 encodes the sign bit of negative zero, and Q mixed weights retain the
	// exact packed 32768/32767 values rather than normalizing to equal halves.
	value = baseline();
	positions = value["positions"];
	positions.set(0, Vector3(-0.0f, 0, 0));
	value["positions"] = positions;
	reject(value); // Same numeric coordinates are not the same source bytes.
	value["source_content_digest"] = "a1d17d8e0be01d2555994def4a98ebc64887bb36fcd6c521526225bf6c285cde";
	REQUIRE(p01_decode_public_snapshot(value, decoded) == OK);
	CHECK(std::signbit(decoded.positions[0].x));
	value = baseline();
	indices = value["bind_indices"];
	indices.set(9, 1);
	weights = value["weights_u16"];
	weights.set(8, 32768);
	weights.set(9, 32767);
	value["bind_indices"] = indices;
	value["weights_u16"] = weights;
	value["source_content_digest"] = "21ec72419b375638b10d315bc59d6dfee7720474986f8173742fd58495a37907";
	REQUIRE(p01_decode_public_snapshot(value, decoded) == OK);
	CHECK(decoded.weights_u16[8] == 32768);
	CHECK(decoded.weights_u16[9] == 32767);
}

} // namespace TestP01PublicSnapshot
