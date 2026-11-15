/**************************************************************************/
/*  test_external_output_config.cpp                                       */
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

#include "servers/rendering/renderer_rd/storage_rd/external_output_config.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_request.h"
#include "tests/test_macros.h"

#include <limits>

TEST_FORCE_LINK(test_external_output_config)

namespace TestExternalOutputConfig {

using RendererRD::ExternalOutputConfig;

static Dictionary config_for(ExternalOutputConfig::Profile p_profile) {
	Dictionary config;
	config["schema"] = 1;
	config["profile"] = int(p_profile);
	config["capacity"] = p_profile == ExternalOutputConfig::SCENE_ONLY ? 0 : 32;
	config["max_views"] = 2;
	config["max_inflight"] = 2;
	Array surfaces;
	Array materials;
	if (p_profile == ExternalOutputConfig::INSTANCE_192) {
		surfaces.push_back(RID::from_uint64(123));
	}
	if (p_profile != ExternalOutputConfig::SCENE_ONLY) {
		materials.push_back(RID::from_uint64(456));
	}
	config["surfaces"] = surfaces;
	config["surface_materials"] = materials;
	config["bounds"] = AABB(Vector3(-10, -10, -10), Vector3(20, 20, 20));
	config["space"] = 1;
	config["pass_mask"] = p_profile == ExternalOutputConfig::SCENE_ONLY ? 0 : 0x10;
	config["motion"] = 0;
	PackedByteArray tag;
	tag.resize(32);
	for (const char *field : { "producer_tag", "device_tag", "layout_digest", "mapping_digest", "numeric_profile_digest" }) {
		config[field] = tag;
	}
	if (p_profile == ExternalOutputConfig::INDEXED_144) {
		config["geometry_bounds"] = config["bounds"];
	}
	if (p_profile == ExternalOutputConfig::SCENE_ONLY) {
		config["query_enabled"] = true;
		config["query_bounds"] = config["bounds"];
	}
	return config;
}

static void rejects_unchanged(const Dictionary &p_config) {
	ExternalOutputConfig parsed;
	parsed.capacity = 999;
	parsed.max_views = 7;
	StringName detail;
	CHECK(ExternalOutputConfig::parse(p_config, parsed, detail) == ERR_INVALID_PARAMETER);
	CHECK(parsed.capacity == 999);
	CHECK(parsed.max_views == 7);
	CHECK_FALSE(detail.is_empty());
}

TEST_CASE("[ExternalOutputConfig] strict create schema rejects malformed fields without publishing partial config") {
	const Dictionary valid = config_for(ExternalOutputConfig::INSTANCE_192);
	const char *required[] = { "schema", "profile", "capacity", "max_views", "max_inflight", "surfaces", "surface_materials", "bounds", "space", "pass_mask", "motion", "producer_tag", "device_tag", "layout_digest", "mapping_digest", "numeric_profile_digest" };
	for (const char *field : required) {
		Dictionary missing = valid.duplicate();
		missing.erase(field);
		rejects_unchanged(missing);
		Dictionary wrong_type = valid.duplicate();
		wrong_type[field] = true;
		rejects_unchanged(wrong_type);
	}
	for (const char *field : { "schema", "profile", "capacity", "max_views", "max_inflight", "space", "pass_mask", "motion" }) {
		for (const Variant &bad : { Variant(-1), Variant(int64_t(UINT32_MAX) + 1), Variant(1.0) }) {
			Dictionary config = valid.duplicate();
			config[field] = bad;
			rejects_unchanged(config);
		}
	}
	for (const Variant &unknown_key : { Variant("future_field"), Variant(123) }) {
		Dictionary config = valid.duplicate();
		config[unknown_key] = 1;
		rejects_unchanged(config);
	}
	for (const char *field : { "producer_tag", "device_tag", "layout_digest", "mapping_digest", "numeric_profile_digest" }) {
		for (int size : { 0, 8, 31, 33 }) {
			Dictionary config = valid.duplicate();
			PackedByteArray tag;
			tag.resize(size);
			config[field] = tag;
			rejects_unchanged(config);
		}
	}
	Dictionary invalid_array = valid.duplicate();
	Array invalid_surfaces;
	invalid_surfaces.push_back(123);
	invalid_array["surfaces"] = invalid_surfaces;
	rejects_unchanged(invalid_array);
	invalid_array = valid.duplicate();
	invalid_array["surface_materials"] = invalid_surfaces;
	rejects_unchanged(invalid_array);
}

TEST_CASE("[ExternalOutputConfig] profile limits and conditional fields are exact") {
	for (ExternalOutputConfig::Profile profile : { ExternalOutputConfig::INSTANCE_192, ExternalOutputConfig::INDEXED_144, ExternalOutputConfig::SCENE_ONLY }) {
		ExternalOutputConfig parsed;
		StringName detail = "old_error";
		const Dictionary valid = config_for(profile);
		REQUIRE(ExternalOutputConfig::parse(valid, parsed, detail) == OK);
		CHECK(parsed.profile == profile);
		CHECK(detail.is_empty());
		Dictionary config = valid.duplicate();
		config["capacity"] = (1 << 20) + 1;
		rejects_unchanged(config);
		for (const char *field : { "max_views", "max_inflight" }) {
			config = valid.duplicate();
			config[field] = 0;
			rejects_unchanged(config);
			config[field] = field == String("max_views") ? 9 : 5;
			rejects_unchanged(config);
		}
		for (const char *field : { "schema", "profile", "space", "motion" }) {
			config = valid.duplicate();
			config[field] = 99;
			rejects_unchanged(config);
		}
		config = valid.duplicate();
		config["pass_mask"] = 0x20; // First reserved schema1 bit.
		rejects_unchanged(config);
		config = valid.duplicate();
		config["query_enabled"] = 1;
		rejects_unchanged(config);
		config["query_enabled"] = true;
		config.erase("query_bounds");
		rejects_unchanged(config);
	}
	Dictionary scene = config_for(ExternalOutputConfig::SCENE_ONLY);
	for (const char *field : { "capacity", "motion", "pass_mask" }) {
		Dictionary config = scene.duplicate();
		config[field] = field == String("pass_mask") ? 0x10 : 1;
		rejects_unchanged(config);
	}
	scene["query_enabled"] = false;
	rejects_unchanged(scene);
	Dictionary indexed = config_for(ExternalOutputConfig::INDEXED_144);
	indexed.erase("geometry_bounds");
	rejects_unchanged(indexed);
	Dictionary instance = config_for(ExternalOutputConfig::INSTANCE_192);
	instance["geometry_bounds"] = instance["bounds"];
	rejects_unchanged(instance);
	instance.erase("geometry_bounds");
	Array too_many;
	too_many.resize(9);
	too_many.fill(RID::from_uint64(1));
	instance["surfaces"] = too_many;
	rejects_unchanged(instance);
	// Zero capacity must remain legal for ordinary selected zero output.
	instance = config_for(ExternalOutputConfig::INSTANCE_192);
	instance["capacity"] = 0;
	ExternalOutputConfig parsed;
	StringName detail;
	CHECK(ExternalOutputConfig::parse(instance, parsed, detail) == OK);
	CHECK_FALSE(parsed.query_enabled); // The one explicitly permitted default.
	instance["query_bounds"] = AABB();
	CHECK(ExternalOutputConfig::parse(instance, parsed, detail) == OK);
	instance["query_bounds"] = 0;
	rejects_unchanged(instance);
}

TEST_CASE("[ExternalOutputConfig] every accepted bound is finite and nonempty") {
	const real_t infinity = std::numeric_limits<real_t>::infinity();
	const real_t nan = std::numeric_limits<real_t>::quiet_NaN();
	for (const AABB &bad : { AABB(), AABB(Vector3(), Vector3(1, -1, 1)), AABB(Vector3(nan, 0, 0), Vector3(1, 1, 1)), AABB(Vector3(), Vector3(1, infinity, 1)), AABB(Vector3(std::numeric_limits<real_t>::max(), 0, 0), Vector3(std::numeric_limits<real_t>::max(), 1, 1)) }) {
		for (const char *field : { "bounds", "geometry_bounds", "query_bounds" }) {
			Dictionary config = config_for(ExternalOutputConfig::INDEXED_144);
			config["query_enabled"] = true;
			config["query_bounds"] = config["bounds"];
			config[field] = bad;
			rejects_unchanged(config);
		}
	}
}

TEST_CASE("[ExternalOutputConfig] checked buffer sizing rejects wrap and respects actual device limit") {
	uint32_t bytes = 777;
	CHECK(ExternalOutputConfig::checked_buffer_bytes(22369622, 192, UINT32_MAX, bytes) == ERR_INVALID_PARAMETER);
	CHECK(bytes == 777); // A u32 product would have wrapped to 128.
	CHECK(ExternalOutputConfig::checked_buffer_bytes(2, UINT64_MAX, UINT64_MAX, bytes) == ERR_INVALID_PARAMETER);
	CHECK(bytes == 777);
	CHECK(ExternalOutputConfig::checked_buffer_bytes(UINT32_MAX, 0, UINT64_MAX, bytes) == ERR_INVALID_PARAMETER);
	CHECK(ExternalOutputConfig::checked_buffer_bytes(2, 192, 383, bytes) == ERR_INVALID_PARAMETER);
	CHECK(bytes == 777);
	CHECK(ExternalOutputConfig::checked_buffer_bytes(2, 192, 384, bytes) == OK);
	CHECK(bytes == 384);
	CHECK(ExternalOutputConfig::checked_buffer_bytes(0, 192, 0, bytes) == OK);
	CHECK(bytes == 0);
}

TEST_CASE("[ExternalOutputConfig] schema1 pass namespace accepts every known combination and rejects reserved bits") {
	CHECK(ExternalOutputConfig::PASS_OPAQUE == 0x01);
	CHECK(ExternalOutputConfig::PASS_DEPTH == 0x02);
	CHECK(ExternalOutputConfig::PASS_SHADOW == 0x04);
	CHECK(ExternalOutputConfig::PASS_TRANSPARENT == 0x08);
	CHECK(ExternalOutputConfig::PASS_PROBE == 0x10);
	CHECK(ExternalOutputConfig::PASS_KNOWN_MASK == 0x1f);
	for (ExternalOutputConfig::Profile profile : { ExternalOutputConfig::INSTANCE_192, ExternalOutputConfig::INDEXED_144 }) {
		for (int mask = 0; mask <= 0x1f; mask++) {
			Dictionary config = config_for(profile);
			config["pass_mask"] = mask;
			ExternalOutputConfig parsed;
			StringName detail;
			REQUIRE(ExternalOutputConfig::parse(config, parsed, detail) == OK);
			CHECK(parsed.pass_mask == uint32_t(mask));
			config["pass_mask"] = mask | 0x20;
			rejects_unchanged(config);
		}
	}
	for (int mask = 1; mask <= 0x1f; mask++) {
		Dictionary scene = config_for(ExternalOutputConfig::SCENE_ONLY);
		scene["pass_mask"] = mask;
		rejects_unchanged(scene);
	}
	// Mask syntax confers no material eligibility; that native admission check
	// belongs after real retained material/pipeline resolution, not this parser.
}

TEST_CASE("[ExternalOutputConfig] u64 identities decode exactly from eight little endian bytes") {
	uint64_t value = 42;
	for (const Variant &bad : { Variant(9007199254740992.0), Variant(int64_t(1)), Variant(PackedByteArray()) }) {
		CHECK(ExternalOutputConfig::decode_u64(bad, value) == ERR_INVALID_PARAMETER);
		CHECK(value == 42);
	}
	PackedByteArray bytes;
	bytes.resize(8);
	bytes.fill(255);
	CHECK(ExternalOutputConfig::decode_u64(bytes, value) == OK);
	CHECK(value == UINT64_MAX);
	bytes.fill(0);
	bytes.set(0, 1);
	bytes.set(7, 0x80);
	CHECK(ExternalOutputConfig::decode_u64(bytes, value) == OK);
	CHECK(value == UINT64_C(0x8000000000000001));
}

static PackedByteArray wire_u64(uint64_t p_value) {
	PackedByteArray bytes;
	bytes.resize(8);
	for (uint32_t i = 0; i < 8; i++) {
		bytes.set(i, uint8_t(p_value >> (i * 8)));
	}
	return bytes;
}

static PackedByteArray wire_token(uint8_t p_byte = 7) {
	PackedByteArray bytes;
	bytes.resize(32);
	bytes.fill(p_byte);
	return bytes;
}

static Dictionary wire_camera() {
	Dictionary camera;
	camera["view_id"] = wire_u64(UINT64_C(0x8000000000000001));
	camera["view_epoch"] = wire_u64(2);
	camera["capture_ordinal"] = wire_u64(UINT64_MAX);
	return camera;
}

static Dictionary wire_request(bool p_seal, bool p_previous = true, bool p_tuple = true) {
	Dictionary value;
	value["schema"] = 1;
	value["generation"] = wire_u64(UINT64_MAX);
	value["present_seq"] = wire_u64(UINT64_C(0x8000000000000001));
	value["bounds"] = AABB(Vector3(-1, -2, -3), Vector3(2, 4, 6));
	value["history_reset"] = 0;
	value["current_camera_token"] = wire_camera();
	value["actual_previous_camera_token"] = p_previous ? Variant(wire_camera()) : Variant();
	value["previous_consumed_tuple_token"] = p_tuple ? Variant(wire_token()) : Variant();
	for (const char *field : { "output_tag", "view_tag", "phase_token", "current_host_token" }) {
		value[field] = wire_token();
	}
	if (p_seal) {
		value["record_token"] = wire_token();
	} else {
		value["content_version"] = wire_u64(3);
		value["view_epoch"] = wire_u64(2);
		value["pass_mask"] = int(ExternalOutputConfig::PASS_KNOWN_MASK);
		for (const char *field : { "layout_digest", "mapping_digest", "source_token" }) {
			value[field] = wire_token();
		}
	}
	return value;
}

static void request_rejects_unchanged(const Dictionary &p_value, bool p_seal) {
	StringName detail;
	if (p_seal) {
		RendererRD::ExternalOutputSeal output;
		REQUIRE(RendererRD::ExternalOutputSeal::parse(wire_request(true), output, detail) == OK);
		output.common.generation = 991;
		const auto before = output;
		CHECK(RendererRD::ExternalOutputSeal::parse(p_value, output, detail) == ERR_INVALID_PARAMETER);
		CHECK(output.common.matches(before.common));
		CHECK(output.record_token == before.record_token);
	} else {
		RendererRD::ExternalOutputRequest output;
		REQUIRE(RendererRD::ExternalOutputRequest::parse(wire_request(false), output, detail) == OK);
		output.common.generation = 991;
		const auto before = output;
		CHECK(RendererRD::ExternalOutputRequest::parse(p_value, output, detail) == ERR_INVALID_PARAMETER);
		CHECK(output.common.matches(before.common));
		CHECK(output.content_version == before.content_version);
		CHECK(output.view_epoch == before.view_epoch);
		CHECK(output.pass_mask == before.pass_mask);
		CHECK(output.layout_digest == before.layout_digest);
		CHECK(output.mapping_digest == before.mapping_digest);
		CHECK(output.source_token == before.source_token);
	}
	CHECK(detail == StringName("CONTRACT"));
}

TEST_CASE("[ExternalOutputConfig] acquire and seal parse the complete original wire without owner authority") {
	using RendererRD::ExternalOutputRequest;
	using RendererRD::ExternalOutputSeal;
	SUBCASE("every field is required and strictly typed with transactional failure") {
		for (bool seal : { false, true }) {
			const Dictionary valid = wire_request(seal);
			CHECK(valid.size() == (seal ? 13 : 18));
			for (const Variant *key = valid.next(nullptr); key; key = valid.next(key)) {
				INFO("field ", String(*key), ", seal ", seal);
				Dictionary bad = valid.duplicate(true);
				bad.erase(*key);
				request_rejects_unchanged(bad, seal);
				bad = valid.duplicate(true);
				bad[*key] = true;
				request_rejects_unchanged(bad, seal);
			}
			for (const Variant &key : { Variant("unknown"), Variant(9) }) {
				Dictionary bad = valid.duplicate(true);
				bad[key] = 1;
				request_rejects_unchanged(bad, seal);
			}
			for (const char *field : { "schema", "history_reset" }) {
				for (const Variant &value : { Variant(-1), Variant(1.0), Variant(int64_t(UINT32_MAX) + 1) }) {
					Dictionary bad = valid.duplicate(true);
					bad[field] = value;
					request_rejects_unchanged(bad, seal);
				}
			}
			for (const Variant &value : { Variant(0), Variant(2) }) {
				Dictionary bad = valid.duplicate(true);
				bad["schema"] = value;
				request_rejects_unchanged(bad, seal);
			}
			Dictionary bad_reset = valid.duplicate(true);
			bad_reset["history_reset"] = 11;
			request_rejects_unchanged(bad_reset, seal);
			for (const Variant *key = valid.next(nullptr); key; key = valid.next(key)) {
				const Variant original = valid[*key];
				if (original.get_type() != Variant::PACKED_BYTE_ARRAY) {
					continue;
				}
				const PackedByteArray bytes = original;
				for (int size : { 0, 7, 9, 31, 33 }) {
					PackedByteArray malformed;
					malformed.resize(size);
					Dictionary bad = valid.duplicate(true);
					bad[*key] = malformed;
					request_rejects_unchanged(bad, seal);
				}
				if (bytes.size() == 8) {
					Dictionary bad = valid.duplicate(true);
					bad[*key] = int64_t(1);
					request_rejects_unchanged(bad, seal);
				}
			}
			Dictionary wrong_namespace = valid.duplicate(true);
			wrong_namespace[seal ? "source_token" : "record_token"] = wire_token();
			request_rejects_unchanged(wrong_namespace, seal);
		}
	}
	SUBCASE("camera dictionaries require exactly three nonzero little endian fields") {
		for (bool seal : { false, true }) {
			for (const char *field : { "current_camera_token", "actual_previous_camera_token" }) {
				for (const char *part : { "view_id", "view_epoch", "capture_ordinal" }) {
					for (int fault = 0; fault < 5; fault++) {
						Dictionary camera = wire_camera();
						if (fault == 0) {
							camera.erase(part);
						} else if (fault == 1) {
							camera[part] = wire_u64(0);
						} else if (fault == 2) {
							camera[part] = 1;
						} else if (fault == 3) {
							camera[part] = 1.0;
						} else {
							PackedByteArray wrong_size;
							wrong_size.resize(7);
							camera[part] = wrong_size;
						}
						Dictionary bad = wire_request(seal);
						bad[field] = camera;
						request_rejects_unchanged(bad, seal);
					}
				}
				for (const Variant &extra : { Variant("schema"), Variant(1) }) {
					Dictionary camera = wire_camera();
					camera[extra] = wire_u64(1);
					Dictionary bad = wire_request(seal);
					bad[field] = camera;
					request_rejects_unchanged(bad, seal);
				}
			}
		}
	}
	SUBCASE("None is explicit and every reset and known pass combination parses") {
		StringName detail;
		for (bool previous : { false, true }) {
			for (bool tuple : { false, true }) {
				for (int reset = 0; reset <= 10; reset++) {
					Dictionary request = wire_request(false, previous, tuple);
					Dictionary seal = wire_request(true, previous, tuple);
					request["history_reset"] = reset;
					seal["history_reset"] = reset;
					ExternalOutputRequest parsed;
					ExternalOutputSeal sealed;
					CHECK(ExternalOutputRequest::parse(request, parsed, detail) == OK);
					CHECK(detail.is_empty());
					CHECK(ExternalOutputSeal::parse(seal, sealed, detail) == OK);
					CHECK(detail.is_empty());
					CHECK(parsed.common.matches(sealed.common));
					CHECK(parsed.common.has_actual_previous_camera == previous);
					CHECK(parsed.common.has_previous_consumed_tuple == tuple);
					CHECK(parsed.common.generation == UINT64_MAX);
					CHECK(parsed.common.present_seq == UINT64_C(0x8000000000000001));
					CHECK(parsed.common.current_camera_token.view_id == UINT64_C(0x8000000000000001));
					CHECK(parsed.common.current_camera_token.capture_ordinal == UINT64_MAX);
				}
			}
		}
		// Scalar wire zero and all-zero opaque tokens carry no owner admission.
		for (bool seal : { false, true }) {
			Dictionary value = wire_request(seal);
			for (const Variant *key = value.next(nullptr); key; key = value.next(key)) {
				if (value[*key].get_type() == Variant::PACKED_BYTE_ARRAY) {
					PackedByteArray bytes = value[*key];
					bytes.fill(0);
					value[*key] = bytes;
				}
			}
			if (seal) {
				ExternalOutputSeal parsed;
				CHECK(ExternalOutputSeal::parse(value, parsed, detail) == OK);
				CHECK(parsed.common.generation == 0);
			} else {
				ExternalOutputRequest parsed;
				CHECK(ExternalOutputRequest::parse(value, parsed, detail) == OK);
				CHECK(parsed.common.generation == 0);
				CHECK(parsed.content_version == 0);
				CHECK(parsed.view_epoch == 0);
			}
		}
		for (int mask = 0; mask <= int(ExternalOutputConfig::PASS_KNOWN_MASK); mask++) {
			Dictionary request = wire_request(false);
			request["pass_mask"] = mask;
			ExternalOutputRequest parsed;
			CHECK(ExternalOutputRequest::parse(request, parsed, detail) == OK);
			CHECK(parsed.pass_mask == uint32_t(mask));
		}
		for (const Variant &mask : { Variant(0x20), Variant(0x21), Variant(int64_t(UINT32_MAX)), Variant(-1), Variant(true), Variant(1.0), Variant(int64_t(UINT32_MAX) + 1) }) {
			Dictionary request = wire_request(false);
			request["pass_mask"] = mask;
			request_rejects_unchanged(request, false);
		}
	}
	SUBCASE("bounds reject nonfinite empty inverted and overflow endpoints") {
		const real_t inf = std::numeric_limits<real_t>::infinity();
		const real_t nan = std::numeric_limits<real_t>::quiet_NaN();
		const real_t maximum = std::numeric_limits<real_t>::max();
		for (bool seal : { false, true }) {
			for (const AABB &bounds : { AABB(), AABB(Vector3(), Vector3(-1, 1, 1)), AABB(Vector3(nan, 0, 0), Vector3(1, 1, 1)), AABB(Vector3(), Vector3(1, inf, 1)), AABB(Vector3(maximum, 0, 0), Vector3(maximum, 1, 1)) }) {
				Dictionary bad = wire_request(seal);
				bad["bounds"] = bounds;
				request_rejects_unchanged(bad, seal);
			}
		}
	}
	SUBCASE("StringName keys share the native Dictionary schema without inventing duplicate history") {
		for (bool seal : { false, true }) {
			const Dictionary original = wire_request(seal);
			Dictionary named;
			for (const Variant *key = original.next(nullptr); key; key = original.next(key)) {
				named[StringName(*key)] = original[*key];
			}
			const int count = named.size();
			named[String("schema")] = 1;
			CHECK(named.size() == count); // Godot coalesces String/StringName keys.
			StringName detail;
			if (seal) {
				ExternalOutputSeal parsed;
				CHECK(ExternalOutputSeal::parse(named, parsed, detail) == OK);
			} else {
				ExternalOutputRequest parsed;
				CHECK(ExternalOutputRequest::parse(named, parsed, detail) == OK);
			}
			named[String("schema")] = 2;
			request_rejects_unchanged(named, seal);
		}
	}
	SUBCASE("each common original value is retained for exact acquire seal comparison") {
		const Dictionary original = wire_request(true);
		StringName detail;
		ExternalOutputSeal baseline;
		REQUIRE(ExternalOutputSeal::parse(original, baseline, detail) == OK);
		for (const Variant *key = original.next(nullptr); key; key = original.next(key)) {
			if (StringName(*key) == StringName("schema") || StringName(*key) == StringName("record_token")) {
				continue;
			}
			Dictionary changed = original.duplicate(true);
			const Variant value = original[*key];
			if (value.get_type() == Variant::PACKED_BYTE_ARRAY) {
				PackedByteArray bytes = value;
				bytes.set(0, bytes[0] ^ 1u);
				changed[*key] = bytes;
			} else if (value.get_type() == Variant::DICTIONARY) {
				Dictionary camera = value;
				camera = camera.duplicate();
				camera["view_epoch"] = wire_u64(5);
				changed[*key] = camera;
			} else if (value.get_type() == Variant::AABB) {
				changed[*key] = AABB(Vector3(), Vector3(2, 2, 2));
			} else {
				changed[*key] = 1;
			}
			ExternalOutputSeal parsed;
			REQUIRE(ExternalOutputSeal::parse(changed, parsed, detail) == OK);
			CHECK_FALSE(parsed.common.matches(baseline.common));
			CHECK_FALSE(baseline.common.matches(parsed.common));
		}
		for (const char *field : { "actual_previous_camera_token", "previous_consumed_tuple_token" }) {
			Dictionary changed = original.duplicate(true);
			changed[field] = Variant();
			ExternalOutputSeal parsed;
			REQUIRE(ExternalOutputSeal::parse(changed, parsed, detail) == OK);
			CHECK_FALSE(parsed.common.matches(baseline.common));
			CHECK_FALSE(baseline.common.matches(parsed.common));
		}
	}
	SUBCASE("whole parsed acquire and seal comparison retains their boundary-specific values") {
		StringName detail;
		const Dictionary original = wire_request(false);
		ExternalOutputRequest baseline;
		REQUIRE(ExternalOutputRequest::parse(original, baseline, detail) == OK);
		CHECK(baseline.matches(baseline));
		for (const char *field : { "content_version", "view_epoch", "pass_mask", "layout_digest", "mapping_digest", "source_token" }) {
			Dictionary changed = original.duplicate(true);
			if (StringName(field) == StringName("pass_mask")) {
				changed[field] = int(ExternalOutputConfig::PASS_OPAQUE);
			} else {
				PackedByteArray bytes = changed[field];
				bytes.set(0, bytes[0] ^ 1u);
				changed[field] = bytes;
			}
			ExternalOutputRequest parsed;
			REQUIRE(ExternalOutputRequest::parse(changed, parsed, detail) == OK);
			CHECK(parsed.common.matches(baseline.common));
			CHECK_FALSE(parsed.matches(baseline));
			CHECK_FALSE(baseline.matches(parsed));
		}
		for (uint64_t value : { uint64_t(0), uint64_t(UINT64_MAX) }) {
			Dictionary changed = original.duplicate(true);
			for (const char *field : { "generation", "present_seq", "content_version", "view_epoch" }) {
				changed[field] = wire_u64(value);
			}
			ExternalOutputRequest parsed;
			REQUIRE(ExternalOutputRequest::parse(changed, parsed, detail) == OK);
			CHECK(parsed.common.generation == value);
			CHECK(parsed.common.present_seq == value);
			CHECK(parsed.content_version == value);
			CHECK(parsed.view_epoch == value);
		}
		ExternalOutputSeal sealed;
		REQUIRE(ExternalOutputSeal::parse(wire_request(true), sealed, detail) == OK);
		CHECK(sealed.matches(sealed));
		Dictionary changed = wire_request(true);
		changed["record_token"] = wire_token(9);
		ExternalOutputSeal other;
		REQUIRE(ExternalOutputSeal::parse(changed, other, detail) == OK);
		CHECK(other.common.matches(sealed.common));
		CHECK_FALSE(other.matches(sealed));
		CHECK_FALSE(sealed.matches(other));
		for (const char *field : { "actual_previous_camera_token", "previous_consumed_tuple_token" }) {
			Dictionary absent = original.duplicate(true);
			absent[field] = Variant();
			ExternalOutputRequest parsed;
			REQUIRE(ExternalOutputRequest::parse(absent, parsed, detail) == OK);
			CHECK_FALSE(parsed.matches(baseline));
			CHECK_FALSE(baseline.matches(parsed));
		}
	}
}

} // namespace TestExternalOutputConfig
