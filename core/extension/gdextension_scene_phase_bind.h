/**************************************************************************/
/*  gdextension_scene_phase_bind.h                                        */
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

#include "core/error/error_list.h"
#include "core/extension/gdextension_interface.gen.h"

#include <type_traits>

namespace GDExtensionDeviceExit {

static_assert(std::is_standard_layout_v<GDExtensionSceneOperationKeyV1>);
static_assert(std::is_trivially_copyable_v<GDExtensionSceneOperationKeyV1>);
static_assert(sizeof(GDExtensionSceneOperationKeyV1) == 200);

bool scene_opaque32_is_zero(const GDExtensionSceneOpaque32V1 &p_token);
bool scene_opaque32_equal(const GDExtensionSceneOpaque32V1 &p_lhs, const GDExtensionSceneOpaque32V1 &p_rhs);

Error validate_scene_operation_key_envelope(const GDExtensionSceneOperationKeyV1 &p_key);

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
		bool p_owner_consumed);

Error commit_scene_operation_key_slot(
		GDExtensionSceneOperationKeyV1 &r_bound_key,
		bool &r_bound_flag,
		const GDExtensionSceneOperationKeyV1 &p_incoming_key,
		bool p_can_mutate);

} // namespace GDExtensionDeviceExit
