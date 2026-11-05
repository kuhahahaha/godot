/**************************************************************************/
/*  external_output_config.cpp                                            */
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

#include "external_output_config.h"

#include "core/variant/variant.h"

namespace RendererRD {
namespace {

bool read_u32(const Dictionary &p_config, const char *p_key, uint32_t &r_value) {
	const Variant *value = p_config.getptr(p_key);
	if (!value || value->get_type() != Variant::INT) {
		return false;
	}
	const int64_t integer = *value;
	if (integer < 0 || uint64_t(integer) > UINT32_MAX) {
		return false;
	}
	r_value = uint32_t(integer);
	return true;
}

bool read_tag(const Dictionary &p_config, const char *p_key, PackedByteArray &r_value) {
	const Variant *value = p_config.getptr(p_key);
	if (!value || value->get_type() != Variant::PACKED_BYTE_ARRAY) {
		return false;
	}
	r_value = *value;
	return r_value.size() == 32;
}

bool valid_bounds(const AABB &p_bounds) {
	// Check the endpoint too: finite position and size can add to infinity.
	const Vector3 end = p_bounds.get_end();
	return p_bounds.is_finite() && end.is_finite() &&
			p_bounds.size.x > 0 && p_bounds.size.y > 0 && p_bounds.size.z > 0 &&
			end.x > p_bounds.position.x && end.y > p_bounds.position.y && end.z > p_bounds.position.z;
}

bool read_bounds(const Dictionary &p_config, const char *p_key, AABB &r_value) {
	const Variant *value = p_config.getptr(p_key);
	if (!value || value->get_type() != Variant::AABB) {
		return false;
	}
	r_value = *value;
	return valid_bounds(r_value);
}

bool read_rids(const Dictionary &p_config, const char *p_key, Vector<RID> &r_value) {
	const Variant *value = p_config.getptr(p_key);
	if (!value || value->get_type() != Variant::ARRAY) {
		return false;
	}
	const Array array = *value;
	for (int i = 0; i < array.size(); i++) {
		if (array[i].get_type() != Variant::RID) {
			return false;
		}
		r_value.push_back(array[i]);
	}
	return true;
}

} // namespace

Error ExternalOutputConfig::checked_buffer_bytes(uint64_t p_count, uint64_t p_stride, uint64_t p_device_limit, uint32_t &r_bytes) {
	if (p_count >= UINT32_MAX || (p_stride != 0 && p_count > UINT64_MAX / p_stride)) {
		return ERR_INVALID_PARAMETER;
	}
	const uint64_t bytes = p_count * p_stride;
	if (bytes > UINT32_MAX || bytes > p_device_limit) {
		return ERR_INVALID_PARAMETER;
	}
	r_bytes = uint32_t(bytes);
	return OK;
}

Error ExternalOutputConfig::decode_u64(const Variant &p_value, uint64_t &r_value) {
	if (p_value.get_type() != Variant::PACKED_BYTE_ARRAY) {
		return ERR_INVALID_PARAMETER;
	}
	const PackedByteArray bytes = p_value;
	if (bytes.size() != 8) {
		return ERR_INVALID_PARAMETER;
	}
	uint64_t value = 0;
	for (uint32_t i = 0; i < 8; i++) {
		value |= uint64_t(bytes[i]) << (8 * i);
	}
	r_value = value;
	return OK;
}

Error ExternalOutputConfig::parse(const Dictionary &p_config, ExternalOutputConfig &r_config, StringName &r_detail) {
	r_detail = "CONTRACT";
	const char *fields[] = { "schema", "profile", "capacity", "max_views", "max_inflight", "surfaces", "surface_materials", "bounds", "space", "pass_mask", "motion", "producer_tag", "device_tag", "layout_digest", "mapping_digest", "numeric_profile_digest", "geometry_bounds", "query_enabled", "query_bounds" };
	uint32_t seen = 0;
	for (const Variant *key = p_config.next(nullptr); key; key = p_config.next(key)) {
		if (key->get_type() != Variant::STRING && key->get_type() != Variant::STRING_NAME) {
			return ERR_INVALID_PARAMETER;
		}
		const StringName name = *key;
		bool known = false;
		for (uint32_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
			if (name == fields[i]) {
				if (seen & (1u << i)) {
					return ERR_INVALID_PARAMETER;
				}
				seen |= 1u << i;
				known = true;
				break;
			}
		}
		if (!known) {
			return ERR_INVALID_PARAMETER;
		}
	}

	ExternalOutputConfig config;
	uint32_t schema, profile, space, motion;
	if (!read_u32(p_config, "schema", schema) || schema != 1 ||
			!read_u32(p_config, "profile", profile) || profile < INSTANCE_192 || profile > SCENE_ONLY ||
			!read_u32(p_config, "space", space) || space < WORLD || space > LOCAL ||
			!read_u32(p_config, "motion", motion) || motion > REQUIRED ||
			!read_u32(p_config, "capacity", config.capacity) ||
			!read_u32(p_config, "max_views", config.max_views) ||
			!read_u32(p_config, "max_inflight", config.max_inflight) ||
			!read_u32(p_config, "pass_mask", config.pass_mask)) {
		return ERR_INVALID_PARAMETER;
	}
	config.profile = Profile(profile);
	config.space = Space(space);
	config.motion = Motion(motion);
	if (config.pass_mask & ~PASS_KNOWN_MASK) {
		return ERR_INVALID_PARAMETER;
	}
	if (config.capacity > (1u << 20) || config.max_views == 0 || config.max_views > 8 || config.max_inflight == 0 || config.max_inflight > 4) {
		r_detail = "LIMIT";
		return ERR_INVALID_PARAMETER;
	}
	if (!read_bounds(p_config, "bounds", config.bounds) ||
			!read_rids(p_config, "surfaces", config.surfaces) ||
			!read_rids(p_config, "surface_materials", config.surface_materials) ||
			!read_tag(p_config, "producer_tag", config.producer_tag) ||
			!read_tag(p_config, "device_tag", config.device_tag) ||
			!read_tag(p_config, "layout_digest", config.layout_digest) ||
			!read_tag(p_config, "mapping_digest", config.mapping_digest) ||
			!read_tag(p_config, "numeric_profile_digest", config.numeric_profile_digest)) {
		return ERR_INVALID_PARAMETER;
	}
	if (const Variant *query = p_config.getptr("query_enabled")) {
		if (query->get_type() != Variant::BOOL) {
			return ERR_INVALID_PARAMETER;
		}
		config.query_enabled = *query;
	}
	if (config.query_enabled) {
		if (!read_bounds(p_config, "query_bounds", config.query_bounds)) {
			return ERR_INVALID_PARAMETER;
		}
	} else if (const Variant *query_bounds = p_config.getptr("query_bounds")) {
		if (query_bounds->get_type() != Variant::AABB) {
			return ERR_INVALID_PARAMETER;
		}
		config.query_bounds = *query_bounds;
		if (!config.query_bounds.is_finite() || !config.query_bounds.get_end().is_finite()) {
			return ERR_INVALID_PARAMETER;
		}
	}
	if (config.profile == INDEXED_144) {
		if (!config.surfaces.is_empty() || config.surface_materials.is_empty() || !read_bounds(p_config, "geometry_bounds", config.geometry_bounds)) {
			return ERR_INVALID_PARAMETER;
		}
	} else if (p_config.has("geometry_bounds")) {
		return ERR_INVALID_PARAMETER;
	}
	if (config.profile == INSTANCE_192 && (config.surfaces.is_empty() || config.surfaces.size() > MAX_MESH_RIDS || config.surface_materials.is_empty())) {
		return ERR_INVALID_PARAMETER;
	}
	if (config.profile == SCENE_ONLY && (config.capacity != 0 || !config.surfaces.is_empty() || !config.surface_materials.is_empty() || config.pass_mask != 0 || config.motion != DISABLED || !config.query_enabled)) {
		return ERR_INVALID_PARAMETER;
	}
	// Mesh existence, exact flattened material/surface count and pipeline
	// qualification require native storage admission after this pure parse.
	r_config = config;
	r_detail = StringName();
	return OK;
}

} // namespace RendererRD
