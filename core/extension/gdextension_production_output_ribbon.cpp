/**************************************************************************/
/*  gdextension_production_output_ribbon.cpp                              */
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

#include "gdextension_production_output_ribbon.h"

#include "servers/rendering/renderer_rd/storage_rd/particles_ribbon_rd.h"

namespace GDExtensionDeviceExit {

namespace {
constexpr uint32_t EXPECTED_INDEXED144_ROLES[5] = {
	GDEXTENSION_OUTPUT_ROLE_CURRENT_VERTICES_V1,
	GDEXTENSION_OUTPUT_ROLE_ALIGNED_PREVIOUS_VERTICES_V1,
	GDEXTENSION_OUTPUT_ROLE_VERTEX_HISTORY_VALIDITY_V1,
	GDEXTENSION_OUTPUT_ROLE_INDEX_STAGING_V1,
	GDEXTENSION_OUTPUT_ROLE_OUTPUT_META_V1,
};

constexpr uint32_t EXPECTED_INDEXED144_STRIDES[5] = {
	144u,
	144u,
	4u,
	4u,
	4u,
};
} // namespace

Error validate_indexed144_prepare_v2_roles(
		const GDExtensionProductionOutputRoleV1 p_roles[8],
		uint32_t p_max_segments,
		bool p_motion_required) {
	if (!p_roles) {
		return ERR_INVALID_PARAMETER;
	}
	RendererRD::ParticlesRibbonRD::Indexed144Layout layout;
	const Error plan_err = RendererRD::ParticlesRibbonRD::plan_indexed144(p_max_segments, p_motion_required, 1u, layout);
	if (plan_err != OK) {
		return plan_err;
	}

	auto inactive_locators_valid = [](uint32_t p_slot, uint32_t p_use) -> bool {
		return (p_slot == 0u && p_use == 0u) || (p_slot == UINT32_MAX && p_use == UINT32_MAX);
	};

	GDExtensionProductionOutputRoleV1 canonical[8] = {};
	const bool is_compacted_nomv = !p_motion_required &&
			p_roles[0].role == GDEXTENSION_OUTPUT_ROLE_CURRENT_VERTICES_V1 &&
			p_roles[1].role == GDEXTENSION_OUTPUT_ROLE_INDEX_STAGING_V1 &&
			p_roles[2].role == GDEXTENSION_OUTPUT_ROLE_OUTPUT_META_V1;
	if (is_compacted_nomv) {
		for (uint32_t i = 3; i < 8; ++i) {
			const GDExtensionProductionOutputRoleV1 &unused = p_roles[i];
			if (unused.role != 0 || unused.reserved != 0 || unused.bytes != 0 || unused.allocation_generation != 0 ||
					!inactive_locators_valid(unused.resource_slot, unused.use_index)) {
				return ERR_INVALID_DATA;
			}
		}
		canonical[0] = p_roles[0];
		canonical[1] = { GDEXTENSION_OUTPUT_ROLE_ALIGNED_PREVIOUS_VERTICES_V1, UINT32_MAX, UINT32_MAX, 0, 0, 0 };
		canonical[2] = { GDEXTENSION_OUTPUT_ROLE_VERTEX_HISTORY_VALIDITY_V1, UINT32_MAX, UINT32_MAX, 0, 0, 0 };
		canonical[3] = p_roles[1];
		canonical[4] = p_roles[2];
		for (uint32_t i = 5; i < 8; ++i) {
			canonical[i] = { 0, UINT32_MAX, UINT32_MAX, 0, 0, 0 };
		}
	} else {
		for (uint32_t i = 0; i < 8; ++i) {
			canonical[i] = p_roles[i];
		}
	}

	for (uint32_t i = 5; i < 8; ++i) {
		const GDExtensionProductionOutputRoleV1 &unused = canonical[i];
		if (unused.role != 0 || unused.reserved != 0 || unused.bytes != 0 || unused.allocation_generation != 0 ||
				!inactive_locators_valid(unused.resource_slot, unused.use_index)) {
			return ERR_INVALID_DATA;
		}
	}

	for (uint32_t i = 0; i < 5; ++i) {
		const GDExtensionProductionOutputRoleV1 &role = canonical[i];
		if (role.role != EXPECTED_INDEXED144_ROLES[i] || role.reserved != 0) {
			return ERR_INVALID_DATA;
		}
		const uint64_t expected_bytes = p_max_segments ? uint64_t(layout.bytes[i]) : (i == 4 ? 32ull : 0ull);
		if (role.bytes != expected_bytes) {
			return ERR_INVALID_DATA;
		}
		if (role.bytes == 0) {
			if (!inactive_locators_valid(role.resource_slot, role.use_index) ||
					role.allocation_generation != 0) {
				return ERR_INVALID_DATA;
			}
		} else {
			if (role.resource_slot == UINT32_MAX || role.use_index == UINT32_MAX || role.allocation_generation == 0) {
				return ERR_INVALID_DATA;
			}
			for (uint32_t prior = 0; prior < i; ++prior) {
				if (canonical[prior].bytes != 0 &&
						(canonical[prior].resource_slot == role.resource_slot || canonical[prior].use_index == role.use_index)) {
					return ERR_INVALID_DATA;
				}
			}
		}
	}
	return OK;
}

Error project_indexed144_acquire_v2_writables(
		const Indexed144RoleBindingInput p_roles[5],
		uint32_t p_max_segments,
		bool p_motion_required,
		GDExtensionOutputAcquireResultV2 &r_out) {
	if (!p_roles) {
		return ERR_INVALID_PARAMETER;
	}
	RendererRD::ParticlesRibbonRD::Indexed144Layout layout;
	const Error plan_err = RendererRD::ParticlesRibbonRD::plan_indexed144(p_max_segments, p_motion_required, 1u, layout);
	if (plan_err != OK) {
		return plan_err;
	}

	GDExtensionOutputAcquireResultV2 staged = r_out;
	staged.writable_count = 0;
	for (uint32_t i = 0; i < 8; ++i) {
		staged.writables[i] = {};
		staged.resource_slots[i] = UINT32_MAX;
		staged.use_indices[i] = UINT32_MAX;
	}

	for (uint32_t i = 0; i < 5; ++i) {
		const Indexed144RoleBindingInput &role = p_roles[i];
		const uint64_t expected_bytes = p_max_segments ? uint64_t(layout.bytes[i]) : (i == 4 ? 32ull : 0ull);
		if (role.role != EXPECTED_INDEXED144_ROLES[i] || role.bytes != expected_bytes) {
			return ERR_INVALID_DATA;
		}
		if (role.bytes == 0) {
			const bool inactive_ok = role.primary_rid == 0 && role.allocation_generation == 0 &&
					((role.resource_slot == 0u && role.use_index == 0u) ||
							(role.resource_slot == UINT32_MAX && role.use_index == UINT32_MAX));
			if (!inactive_ok) {
				return ERR_INVALID_DATA;
			}
			continue;
		}
		if (role.primary_rid == 0 || role.allocation_generation == 0 || role.resource_slot == UINT32_MAX || role.use_index == UINT32_MAX) {
			return ERR_INVALID_DATA;
		}
		for (uint32_t prior = 0; prior < staged.writable_count; ++prior) {
			if (staged.writables[prior].rid == role.primary_rid ||
					staged.resource_slots[prior] == role.resource_slot ||
					staged.use_indices[prior] == role.use_index) {
				return ERR_INVALID_DATA;
			}
		}
		const uint32_t idx = staged.writable_count++;
		staged.writables[idx] = {
			role.role,
			GDEXTENSION_OUTPUT_ACCESS_WRITE_ONLY_V1,
			role.primary_rid,
			role.allocation_generation,
			uint32_t(role.bytes),
			EXPECTED_INDEXED144_STRIDES[i],
		};
		staged.resource_slots[idx] = role.resource_slot;
		staged.use_indices[idx] = role.use_index;
	}
	r_out = staged;
	return OK;
}

} // namespace GDExtensionDeviceExit
