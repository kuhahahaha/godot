/**************************************************************************/
/*  external_output_request.cpp                                           */
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

#include "external_output_request.h"

namespace RendererRD {
namespace {

bool exact_keys(const Dictionary &p_dictionary, const char *const *p_fields, uint32_t p_count) {
	uint32_t seen = 0;
	for (const Variant *key = p_dictionary.next(nullptr); key; key = p_dictionary.next(key)) {
		if (key->get_type() != Variant::STRING && key->get_type() != Variant::STRING_NAME) {
			return false;
		}
		const StringName name = *key;
		bool known = false;
		for (uint32_t i = 0; i < p_count; i++) {
			if (name == p_fields[i]) {
				if (seen & (1u << i)) {
					return false;
				}
				seen |= 1u << i;
				known = true;
				break;
			}
		}
		if (!known) {
			return false;
		}
	}
	return seen == (1u << p_count) - 1u;
}

bool read_request_u32(const Dictionary &p_dictionary, const char *p_key, uint32_t &r_value) {
	const Variant *value = p_dictionary.getptr(p_key);
	if (!value || value->get_type() != Variant::INT) {
		return false;
	}
	const int64_t number = *value;
	if (number < 0 || uint64_t(number) > UINT32_MAX) {
		return false;
	}
	r_value = uint32_t(number);
	return true;
}

bool read_u64(const Dictionary &p_dictionary, const char *p_key, uint64_t &r_value) {
	const Variant *value = p_dictionary.getptr(p_key);
	return value && ExternalOutputConfig::decode_u64(*value, r_value) == OK;
}

bool read_token(const Dictionary &p_dictionary, const char *p_key, PackedByteArray &r_value) {
	const Variant *value = p_dictionary.getptr(p_key);
	if (!value || value->get_type() != Variant::PACKED_BYTE_ARRAY) {
		return false;
	}
	const PackedByteArray bytes = *value;
	if (bytes.size() != 32) {
		return false;
	}
	r_value = bytes;
	return true;
}

bool read_camera(const Variant &p_value, ExternalOutputRequestCommon::CameraSourceToken &r_camera) {
	if (p_value.get_type() != Variant::DICTIONARY) {
		return false;
	}
	const Dictionary dictionary = p_value;
	const char *const fields[] = { "view_id", "view_epoch", "capture_ordinal" };
	ExternalOutputRequestCommon::CameraSourceToken camera;
	if (!exact_keys(dictionary, fields, 3) ||
			!read_u64(dictionary, "view_id", camera.view_id) ||
			!read_u64(dictionary, "view_epoch", camera.view_epoch) ||
			!read_u64(dictionary, "capture_ordinal", camera.capture_ordinal) ||
			!camera.view_id || !camera.view_epoch || !camera.capture_ordinal) {
		return false;
	}
	r_camera = camera;
	return true;
}

bool read_common(const Dictionary &p_dictionary, ExternalOutputRequestCommon &r_common) {
	uint32_t schema;
	ExternalOutputRequestCommon common;
	if (!read_request_u32(p_dictionary, "schema", schema) || schema != 1 ||
			!read_u64(p_dictionary, "generation", common.generation) ||
			!read_u64(p_dictionary, "present_seq", common.present_seq) ||
			!read_token(p_dictionary, "output_tag", common.output_tag) ||
			!read_token(p_dictionary, "view_tag", common.view_tag) ||
			!read_token(p_dictionary, "phase_token", common.phase_token) ||
			!read_token(p_dictionary, "current_host_token", common.current_host_token) ||
			!read_request_u32(p_dictionary, "history_reset", common.history_reset) || common.history_reset > 10) {
		return false;
	}
	const Variant *bounds = p_dictionary.getptr("bounds");
	if (!bounds || bounds->get_type() != Variant::AABB) {
		return false;
	}
	common.bounds = *bounds;
	const Vector3 end = common.bounds.get_end();
	if (!common.bounds.is_finite() || !end.is_finite() ||
			common.bounds.size.x <= 0 || common.bounds.size.y <= 0 || common.bounds.size.z <= 0 ||
			end.x <= common.bounds.position.x || end.y <= common.bounds.position.y || end.z <= common.bounds.position.z) {
		return false;
	}
	const Variant *current = p_dictionary.getptr("current_camera_token");
	const Variant *previous = p_dictionary.getptr("actual_previous_camera_token");
	const Variant *tuple = p_dictionary.getptr("previous_consumed_tuple_token");
	if (!current || !read_camera(*current, common.current_camera_token) || !previous || !tuple) {
		return false;
	}
	if (previous->get_type() != Variant::NIL) {
		if (!read_camera(*previous, common.actual_previous_camera_token)) {
			return false;
		}
		common.has_actual_previous_camera = true;
	}
	if (tuple->get_type() != Variant::NIL) {
		if (!read_token(p_dictionary, "previous_consumed_tuple_token", common.previous_consumed_tuple_token)) {
			return false;
		}
		common.has_previous_consumed_tuple = true;
	}
	r_common = common;
	return true;
}

} // namespace

bool ExternalOutputRequestCommon::matches(const ExternalOutputRequestCommon &p_other) const {
	return generation == p_other.generation && output_tag == p_other.output_tag && view_tag == p_other.view_tag &&
			present_seq == p_other.present_seq && phase_token == p_other.phase_token && bounds == p_other.bounds &&
			history_reset == p_other.history_reset && current_camera_token == p_other.current_camera_token &&
			has_actual_previous_camera == p_other.has_actual_previous_camera &&
			(!has_actual_previous_camera || actual_previous_camera_token == p_other.actual_previous_camera_token) &&
			current_host_token == p_other.current_host_token && has_previous_consumed_tuple == p_other.has_previous_consumed_tuple &&
			(!has_previous_consumed_tuple || previous_consumed_tuple_token == p_other.previous_consumed_tuple_token);
}

bool ExternalOutputRequest::matches(const ExternalOutputRequest &p_other) const {
	return common.matches(p_other.common) && content_version == p_other.content_version && view_epoch == p_other.view_epoch &&
			pass_mask == p_other.pass_mask && layout_digest == p_other.layout_digest && mapping_digest == p_other.mapping_digest && source_token == p_other.source_token;
}

Error ExternalOutputRequest::parse(const Dictionary &p_request, ExternalOutputRequest &r_request, StringName &r_detail) {
	r_detail = "CONTRACT";
	const char *const fields[] = { "schema", "generation", "output_tag", "content_version", "view_tag", "view_epoch", "present_seq", "pass_mask", "phase_token", "layout_digest", "mapping_digest", "source_token", "bounds", "history_reset", "current_camera_token", "actual_previous_camera_token", "current_host_token", "previous_consumed_tuple_token" };
	ExternalOutputRequest request;
	if (!exact_keys(p_request, fields, 18) || !read_common(p_request, request.common) ||
			!read_u64(p_request, "content_version", request.content_version) ||
			!read_u64(p_request, "view_epoch", request.view_epoch) ||
			!read_request_u32(p_request, "pass_mask", request.pass_mask) || (request.pass_mask & ~ExternalOutputConfig::PASS_KNOWN_MASK) ||
			!read_token(p_request, "layout_digest", request.layout_digest) ||
			!read_token(p_request, "mapping_digest", request.mapping_digest) ||
			!read_token(p_request, "source_token", request.source_token)) {
		return ERR_INVALID_PARAMETER;
	}
	r_request = request;
	r_detail = StringName();
	return OK;
}

bool ExternalOutputSeal::matches(const ExternalOutputSeal &p_other) const {
	return common.matches(p_other.common) && record_token == p_other.record_token;
}

Error ExternalOutputSeal::parse(const Dictionary &p_seal, ExternalOutputSeal &r_seal, StringName &r_detail) {
	r_detail = "CONTRACT";
	const char *const fields[] = { "schema", "generation", "output_tag", "view_tag", "present_seq", "phase_token", "record_token", "bounds", "history_reset", "current_camera_token", "actual_previous_camera_token", "current_host_token", "previous_consumed_tuple_token" };
	ExternalOutputSeal seal;
	if (!exact_keys(p_seal, fields, 13) || !read_common(p_seal, seal.common) || !read_token(p_seal, "record_token", seal.record_token)) {
		return ERR_INVALID_PARAMETER;
	}
	r_seal = seal;
	r_detail = StringName();
	return OK;
}

} // namespace RendererRD
