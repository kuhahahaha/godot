/**************************************************************************/
/*  test_scene_connect_config.cpp                                              */
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

#include "servers/rendering/renderer_rd/storage_rd/scene_connect_config.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_scene_connect_config)

namespace TestSceneConnectConfig {
using RendererRD::SceneConnectConfig;

static Dictionary config_for(int p_kind = 1) {
	Dictionary config;
	config["schema"] = 1;
	config["kind"] = p_kind;
	PackedByteArray generation;
	generation.resize(8);
	for (int i = 0; i < 8; i++) {
		generation.set(i, i + 1);
	}
	config["external_generation"] = generation;
	PackedByteArray tag;
	tag.resize(32);
	config["owner_tag"] = tag;
	config["device_tag"] = tag;
	config["max_sources"] = p_kind == 1 ? 32 : 1;
	config["max_resolution"] = p_kind == 1 ? 0 : 1024;
	return config;
}

static void rejects(const Dictionary &p_config, const char *p_detail) {
	SceneConnectConfig parsed;
	parsed.max_sources = 999;
	parsed.external_generation = 777;
	StringName detail;
	CHECK_FALSE(SceneConnectConfig::parse(p_config, parsed, detail));
	CHECK(detail == StringName(p_detail));
	CHECK(parsed.max_sources == 999);
	CHECK(parsed.external_generation == 777);
}

TEST_CASE("[SceneConnectConfig] exact seven fields and wire types") {
	const Dictionary valid = config_for();
	for (const char *field : { "schema", "kind", "external_generation", "owner_tag", "device_tag", "max_sources", "max_resolution" }) {
		Dictionary bad = valid.duplicate();
		bad.erase(field);
		rejects(bad, "BadField");
		bad = valid.duplicate();
		bad[field] = true;
		rejects(bad, "BadType");
	}
	for (const Variant &key : { Variant("extra"), Variant(123) }) {
		Dictionary bad = valid.duplicate();
		bad[key] = 0;
		rejects(bad, "BadField");
	}
	for (const char *field : { "schema", "kind", "max_sources", "max_resolution" }) {
		for (const Variant &value : { Variant(-1), Variant(int64_t(UINT32_MAX) + 1), Variant(1.0) }) {
			Dictionary bad = valid.duplicate();
			bad[field] = value;
			rejects(bad, value.get_type() == Variant::INT ? "BadField" : "BadType");
		}
	}
	for (const char *field : { "external_generation", "owner_tag", "device_tag" }) {
		const int required = String(field) == "external_generation" ? 8 : 32;
		for (int size : { 0, required - 1, required + 1 }) {
			Dictionary bad = valid.duplicate();
			PackedByteArray bytes;
			bytes.resize(size);
			bad[field] = bytes;
			rejects(bad, "BadField");
		}
	}
}

TEST_CASE("[SceneConnectConfig] exact schema kinds and per-kind limits") {
	SceneConnectConfig parsed;
	StringName detail = "old";
	Dictionary config = config_for();
	REQUIRE(SceneConnectConfig::parse(config, parsed, detail));
	CHECK(detail.is_empty());
	CHECK(parsed.kind == SceneConnectConfig::SPHERE);
	CHECK(parsed.external_generation == UINT64_C(0x0807060504030201));
	CHECK(parsed.owner_tag == PackedByteArray(config["owner_tag"]));
	CHECK(parsed.device_tag == PackedByteArray(config["device_tag"]));
	for (int sources : { 1, 1024 }) {
		config["max_sources"] = sources;
		CHECK(SceneConnectConfig::parse(config, parsed, detail));
	}
	for (int sources : { 0, 1025 }) {
		config["max_sources"] = sources;
		rejects(config, "SourceLimit");
	}
	config = config_for();
	config["schema"] = 2;
	rejects(config, "BadSchema");
	for (int kind : { 0, 3 }) {
		config = config_for();
		config["kind"] = kind;
		rejects(config, "BadField");
	}
	config = config_for();
	config["max_resolution"] = 256;
	rejects(config, "BadField");
	config = config_for(2);
	for (int resolution : { 256, 512, 1024, 2048, 4096, 8192 }) {
		config["max_resolution"] = resolution;
		REQUIRE(SceneConnectConfig::parse(config, parsed, detail));
		CHECK(parsed.kind == SceneConnectConfig::HEIGHT);
		CHECK(parsed.max_resolution == uint32_t(resolution));
	}
	for (int resolution : { 0, 5, 255, 16384 }) {
		config["max_resolution"] = resolution;
		rejects(config, "BadField");
	}
	for (int sources : { 0, 2 }) {
		config = config_for(2);
		config["max_sources"] = sources;
		rejects(config, "SourceLimit");
	}
}

static PackedByteArray wire_token(uint8_t p_first = 0) {
	PackedByteArray token;
	token.resize(32);
	token.set(0, p_first);
	return token;
}

static Dictionary phase_request(bool p_sample) {
	Dictionary request;
	request["schema"] = 1;
	request["phase_token"] = wire_token(7);
	request[p_sample ? "request_tag" : "record_request"] = wire_token(9);
	return request;
}

TEST_CASE("[SceneWireInput] sample and reborrow exact maps preserve output on rejection") {
	for (bool sample : { true, false }) {
		const char *identity = sample ? "request_tag" : "record_request";
		const Dictionary valid = phase_request(sample);
		RendererRD::SceneSampleRequest sample_out;
		RendererRD::SceneReborrowRequest reborrow_out;
		StringName detail;
		auto parse = [&](const Dictionary &request) {
			return sample ? RendererRD::SceneSampleRequest::parse(request, sample_out, detail) : RendererRD::SceneReborrowRequest::parse(request, reborrow_out, detail);
		};
		REQUIRE(parse(valid));
		CHECK(detail.is_empty());
		Dictionary named;
		named[StringName("schema")] = 1;
		named[StringName("phase_token")] = wire_token(7);
		named[StringName(identity)] = wire_token(9);
		CHECK(parse(named));
		CHECK((sample ? sample_out.phase_token : reborrow_out.phase_token) == wire_token(7));
		CHECK((sample ? sample_out.request_tag : reborrow_out.record_request) == wire_token(9));
		auto reject = [&](const Dictionary &request, const char *expected) {
			CHECK_FALSE(parse(request));
			CHECK(detail == StringName(expected));
			CHECK((sample ? sample_out.phase_token : reborrow_out.phase_token) == wire_token(7));
			CHECK((sample ? sample_out.request_tag : reborrow_out.record_request) == wire_token(9));
		};
		for (const char *field : { "schema", "phase_token", identity }) {
			Dictionary bad = valid.duplicate();
			bad.erase(field);
			reject(bad, "BadField");
			bad = valid.duplicate();
			bad[field] = true;
			reject(bad, "BadType");
		}
		for (const Variant &key : { Variant("extra"), Variant(123), Variant(sample ? "record_request" : "request_tag") }) {
			Dictionary bad = valid.duplicate();
			bad[key] = wire_token();
			reject(bad, "BadField");
		}
		for (const Variant &schema : { Variant(0), Variant(2), Variant(-1), Variant(int64_t(UINT32_MAX) + 1), Variant(1.0) }) {
			Dictionary bad = valid.duplicate();
			bad["schema"] = schema;
			reject(bad, schema.get_type() != Variant::INT ? "BadType" : int64_t(schema) < 0 || uint64_t(int64_t(schema)) > UINT32_MAX ? "BadField" : "BadSchema");
		}
		for (const char *field : { "phase_token", identity }) {
			for (int length : { 0, 31, 33 }) {
				Dictionary bad = valid.duplicate();
				PackedByteArray bytes;
				bytes.resize(length);
				bad[field] = bytes;
				reject(bad, "BadField");
			}
			for (const Variant &value : { Variant(), Variant(1), Variant(1.0), Variant("00"), Variant(Array()) }) {
				Dictionary bad = valid.duplicate();
				bad[field] = value;
				reject(bad, "BadType");
			}
		}
	}
}

TEST_CASE("[SceneWireInput] token is exact opaque PBA32 and does not grant identity") {
	PackedByteArray parsed = wire_token(11);
	StringName detail;
	CHECK(RendererRD::SceneWireInput::token(Variant(wire_token()), parsed, detail));
	CHECK(parsed == wire_token()); // Syntax accepts opaque bytes; original owner authenticates them.
	for (const Variant &bad : { Variant(), Variant(true), Variant(1), Variant(1.0), Variant("00"), Variant(Array()) }) {
		CHECK_FALSE(RendererRD::SceneWireInput::token(bad, parsed, detail));
		CHECK(detail == StringName("BadType"));
		CHECK(parsed == wire_token());
	}
	for (int length : { 0, 8, 31, 33 }) {
		PackedByteArray bytes;
		bytes.resize(length);
		CHECK_FALSE(RendererRD::SceneWireInput::token(Variant(bytes), parsed, detail));
		CHECK(detail == StringName("BadField"));
		CHECK(parsed == wire_token());
	}
}

TEST_CASE("[SceneWireInput] poll exact token and integer event limits") {
	RendererRD::ScenePollInput parsed;
	StringName detail;
	for (int events : { 1, 256 }) {
		REQUIRE(RendererRD::ScenePollInput::parse(Variant(wire_token(13)), Variant(events), parsed, detail));
		CHECK(detail.is_empty());
		CHECK(parsed.token == wire_token(13));
		CHECK(parsed.max_events == uint32_t(events));
	}
	for (const Variant &events : { Variant(-1), Variant(0), Variant(257), Variant(int64_t(UINT32_MAX) + 1), Variant(true), Variant(1.0), Variant("1") }) {
		CHECK_FALSE(RendererRD::ScenePollInput::parse(Variant(wire_token(14)), events, parsed, detail));
		CHECK(detail == StringName(events.get_type() == Variant::INT ? "BadField" : "BadType"));
		CHECK(parsed.token == wire_token(13));
		CHECK(parsed.max_events == 256);
	}
	CHECK_FALSE(RendererRD::ScenePollInput::parse(Variant("00"), Variant(1), parsed, detail));
	CHECK(detail == StringName("BadType"));
	CHECK(parsed.token == wire_token(13));
	CHECK(parsed.max_events == 256);
}
} // namespace TestSceneConnectConfig
