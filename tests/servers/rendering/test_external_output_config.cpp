/**************************************************************************/
/*  test_external_output_config.cpp                                       */
/**************************************************************************/

#include "servers/rendering/renderer_rd/storage_rd/external_output_config.h"
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

} // namespace TestExternalOutputConfig
