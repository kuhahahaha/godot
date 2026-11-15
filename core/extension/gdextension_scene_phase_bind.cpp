/**************************************************************************/
/*  gdextension_scene_phase_bind.cpp                                      */
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

#include "gdextension_scene_phase_bind.h"

#include "core/extension/gdextension_production_request.h"

#include <cstring>

namespace GDExtensionDeviceExit {

bool scene_opaque32_is_zero(const GDExtensionSceneOpaque32V1 &p_token) {
	const GDExtensionSceneOpaque32V1 empty{};
	return memcmp(&p_token, &empty, sizeof(empty)) == 0;
}

bool scene_opaque32_equal(const GDExtensionSceneOpaque32V1 &p_lhs, const GDExtensionSceneOpaque32V1 &p_rhs) {
	return memcmp(&p_lhs, &p_rhs, sizeof(p_lhs)) == 0;
}

Error validate_scene_operation_key_envelope(const GDExtensionSceneOperationKeyV1 &p_key) {
	if (p_key.abi.abi_version != 1 || p_key.abi.struct_size != sizeof(GDExtensionSceneOperationKeyV1) || p_key.abi.reserved != 0 || p_key.owner_generation == 0) {
		return ERR_INVALID_PARAMETER;
	}
	if (scene_opaque32_is_zero(p_key.record_request) ||
			scene_opaque32_is_zero(p_key.operation_ref) ||
			scene_opaque32_is_zero(p_key.cursor_ref) ||
			scene_opaque32_is_zero(p_key.schedule_digest)) {
		return ERR_INVALID_PARAMETER;
	}
	return OK;
}

Error validate_scene_operation_key_authority(
		const GDExtensionSceneOperationKeyV1 &p_key,
		const GDExtensionProductionOperationV1 &p_expected_operation,
		uint64_t p_attachment_library_generation,
		uint64_t p_active_record_generation,
		bool p_scene_registered,
		bool p_scene_closed,
		bool p_request_frozen,
		bool p_request_cancelled,
		bool p_request_releasing,
		bool p_owner_consumed) {
	const Error envelope_err = validate_scene_operation_key_envelope(p_key);
	if (envelope_err != OK) {
		return envelope_err;
	}
	if (p_attachment_library_generation == 0 ||
			p_key.owner_generation != p_attachment_library_generation ||
			p_key.owner_generation != p_active_record_generation) {
		return ERR_UNAUTHORIZED;
	}
	if (!production_dto_valid(p_key.transport_operation) ||
			memcmp(&p_key.transport_operation, &p_expected_operation, sizeof(p_expected_operation)) != 0) {
		return ERR_INVALID_DATA;
	}
	if (!p_scene_registered || p_scene_closed) {
		return ERR_INVALID_DATA;
	}
	if (p_request_cancelled || p_request_releasing) {
		return ERR_UNAUTHORIZED;
	}
	if (p_request_frozen || !p_owner_consumed) {
		return ERR_BUSY;
	}
	return OK;
}

Error commit_scene_operation_key_slot(
		GDExtensionSceneOperationKeyV1 &r_bound_key,
		bool &r_bound_flag,
		const GDExtensionSceneOperationKeyV1 &p_incoming_key,
		bool p_can_mutate) {
	if (r_bound_flag) {
		return memcmp(&r_bound_key, &p_incoming_key, sizeof(GDExtensionSceneOperationKeyV1)) == 0 ? OK : ERR_INVALID_DATA;
	}
	if (!p_can_mutate) {
		return ERR_BUSY;
	}
	r_bound_key = p_incoming_key;
	r_bound_flag = true;
	return OK;
}

} // namespace GDExtensionDeviceExit
