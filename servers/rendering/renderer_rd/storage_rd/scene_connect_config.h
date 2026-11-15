/**************************************************************************/
/*  scene_connect_config.h                                              */
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

#include "servers/rendering/renderer_rd/storage_rd/external_output_config.h"

namespace RendererRD {

// Syntax only: original Registry still authenticates opaque identities and
// RT/phase/ownership. No decoder creates a connection, sample or permit.
struct SceneWireInput {
	static bool fields(const Dictionary &p_map, const char *const *p_fields, uint32_t p_count, StringName &r_detail) {
		r_detail = "BadField";
		uint32_t seen = 0;
		for (const Variant *key = p_map.next(nullptr); key; key = p_map.next(key)) {
			if (key->get_type() != Variant::STRING && key->get_type() != Variant::STRING_NAME) return false;
			const StringName name = *key;
			bool known = false;
			for (uint32_t i = 0; i < p_count; ++i) {
				if (name == p_fields[i]) {
					if (seen & (1u << i)) return false;
					seen |= 1u << i;
					known = true;
					break;
				}
			}
			if (!known) return false;
		}
		return seen == (1u << p_count) - 1;
	}

	static bool u32(const Variant &p_value, uint32_t &r_value, StringName &r_detail) {
		if (p_value.get_type() != Variant::INT) {
			r_detail = "BadType";
			return false;
		}
		const int64_t value = p_value;
		if (value < 0 || uint64_t(value) > UINT32_MAX) {
			r_detail = "BadField";
			return false;
		}
		r_value = uint32_t(value);
		return true;
	}

	static bool token(const Variant &p_value, PackedByteArray &r_token, StringName &r_detail) {
		if (p_value.get_type() != Variant::PACKED_BYTE_ARRAY) {
			r_detail = "BadType";
			return false;
		}
		const PackedByteArray bytes = p_value;
		if (bytes.size() != 32) {
			r_detail = "BadField";
			return false;
		}
		r_token = bytes;
		r_detail = StringName();
		return true;
	}

	static bool phase_request(const Dictionary &p_request, const char *p_identity_field, PackedByteArray &r_phase, PackedByteArray &r_identity, StringName &r_detail) {
		const char *fields[] = { "schema", "phase_token", p_identity_field };
		if (!SceneWireInput::fields(p_request, fields, 3, r_detail)) return false;
		uint32_t schema = 0;
		if (!u32(p_request["schema"], schema, r_detail)) return false;
		if (schema != 1) {
			r_detail = "BadSchema";
			return false;
		}
		PackedByteArray phase, identity;
		if (!token(p_request["phase_token"], phase, r_detail) || !token(p_request[p_identity_field], identity, r_detail)) return false;
		r_phase = phase;
		r_identity = identity;
		return true;
	}
};

struct SceneSampleRequest {
	PackedByteArray phase_token, request_tag;
	static bool parse(const Dictionary &p_request, SceneSampleRequest &r_request, StringName &r_detail) {
		return SceneWireInput::phase_request(p_request, "request_tag", r_request.phase_token, r_request.request_tag, r_detail);
	}
};

struct SceneReborrowRequest {
	PackedByteArray phase_token, record_request;
	static bool parse(const Dictionary &p_request, SceneReborrowRequest &r_request, StringName &r_detail) {
		return SceneWireInput::phase_request(p_request, "record_request", r_request.phase_token, r_request.record_request, r_detail);
	}
};

struct ScenePollInput {
	PackedByteArray token;
	uint32_t max_events = 0;
	static bool parse(const Variant &p_token, const Variant &p_max_events, ScenePollInput &r_input, StringName &r_detail) {
		ScenePollInput parsed;
		if (!SceneWireInput::token(p_token, parsed.token, r_detail) || !SceneWireInput::u32(p_max_events, parsed.max_events, r_detail)) return false;
		if (parsed.max_events < 1 || parsed.max_events > 256) {
			r_detail = "BadField";
			return false;
		}
		r_input = parsed;
		r_detail = StringName();
		return true;
	}
};

// K06 CPU wire parsing only. This is neither a connection nor proof of
// receiver/base/generation/device ownership. The wrapper maps closed details
// to its frozen error contract; no C01 numeric error ABI is defined here.
struct SceneConnectConfig {
	enum Kind : uint32_t {
		SPHERE = 1,
		HEIGHT = 2,
	};

	Kind kind = SPHERE;
	uint64_t external_generation = 0;
	PackedByteArray owner_tag;
	PackedByteArray device_tag;
	uint32_t max_sources = 0;
	uint32_t max_resolution = 0;

	// Rejection leaves r_config unchanged. Success validates syntax only.
	static bool parse(const Dictionary &p_config, SceneConnectConfig &r_config, StringName &r_detail) {
		const char *fields[] = { "schema", "kind", "external_generation", "owner_tag", "device_tag", "max_sources", "max_resolution" };
		if (!SceneWireInput::fields(p_config, fields, 7, r_detail)) {
			return false;
		}

		uint32_t integers[4];
		const char *integer_fields[] = { "schema", "kind", "max_sources", "max_resolution" };
		for (int i = 0; i < 4; i++) {
			if (!SceneWireInput::u32(p_config[integer_fields[i]], integers[i], r_detail)) {
				return false;
			}
		}

		if (integers[0] != 1) {
			r_detail = "BadSchema";
			return false;
		}
		if (integers[1] != SPHERE && integers[1] != HEIGHT) {
			return false;
		}
		for (const char *field : { "external_generation", "owner_tag", "device_tag" }) {
			if (p_config[field].get_type() != Variant::PACKED_BYTE_ARRAY) {
				r_detail = "BadType";
				return false;
			}
		}
		SceneConnectConfig parsed;
		if (ExternalOutputConfig::decode_u64(p_config["external_generation"], parsed.external_generation) != OK) {
			return false;
		}
		parsed.owner_tag = p_config["owner_tag"];
		parsed.device_tag = p_config["device_tag"];
		if (parsed.owner_tag.size() != 32 || parsed.device_tag.size() != 32) {
			return false;
		}
		parsed.kind = Kind(integers[1]);
		parsed.max_sources = integers[2];
		parsed.max_resolution = integers[3];
		if (parsed.max_sources < 1 || parsed.max_sources > (parsed.kind == SPHERE ? 1024u : 1u)) {
			r_detail = "SourceLimit";
			return false;
		}
		if (parsed.kind == SPHERE) {
			if (parsed.max_resolution != 0) {
				return false;
			}
		} else {
			switch (parsed.max_resolution) {
				case 256:
				case 512:
				case 1024:
				case 2048:
				case 4096:
				case 8192:
					break;
				default:
					return false;
			}
		}
		r_config = parsed;
		r_detail = StringName();
		return true;
	}
};

} // namespace RendererRD
