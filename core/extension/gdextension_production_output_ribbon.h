/**************************************************************************/
/*  gdextension_production_output_ribbon.h                                */
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

#include <cstdint>

namespace GDExtensionDeviceExit {

struct Indexed144RoleBindingInput {
	uint32_t role = 0;
	uint32_t resource_slot = UINT32_MAX;
	uint32_t use_index = UINT32_MAX;
	uint64_t primary_rid = 0;
	uint64_t allocation_generation = 0;
	uint64_t bytes = 0;
};

// Validates V2 8-role prepare table for K05 INDEXED_144 (roles 0..4 active, 5..7 zeroed)
// against exact segment capacity and motion requirement.
Error validate_indexed144_prepare_v2_roles(
		const GDExtensionProductionOutputRoleV1 p_roles[8],
		uint32_t p_max_segments,
		bool p_motion_required);

// Projects active K05 INDEXED_144 writable role descriptors into GDExtensionOutputAcquireResultV2
// (writable_count == 3 when motion is disabled, writable_count == 5 when motion is required).
Error project_indexed144_acquire_v2_writables(
		const Indexed144RoleBindingInput p_roles[5],
		uint32_t p_max_segments,
		bool p_motion_required,
		GDExtensionOutputAcquireResultV2 &r_out);

} // namespace GDExtensionDeviceExit
