/**************************************************************************/
/*  external_output_storage.cpp                                           */
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

#include "external_output_storage.h"

#include "servers/rendering/renderer_rd/storage_rd/external_output_config.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_ribbon_rd.h"
#include "servers/rendering/renderer_rd/effects/particles_external_guard.h"

namespace RendererRD {

Error ExternalOutputStorage::plan_instance192(uint32_t p_capacity, uint32_t p_surface_count, Instance192Layout &r_layout, bool p_motion_required, bool p_aux_params_enabled) {
	if (p_capacity > (1u << 20) || p_surface_count == 0 || p_surface_count > ExternalOutputConfig::MAX_FLATTENED_SURFACES) {
		return ERR_INVALID_PARAMETER;
	}
	// Compute every extent before any native creation. RD has no public maximum
	// storage-buffer-byte limit; actual allocation failure stays a native error.
	// Fixed control extents match ParticlesExternalGuard's Instance192 ABI.
	Instance192Layout layout;
	const uint64_t previous_count = p_motion_required ? p_capacity : 0u;
	const uint64_t counts[INSTANCE192_RESOURCE_COUNT] = { p_capacity, previous_count, 8, 16, 8, p_surface_count, sizeof(ParticlesExternalGuard::GeometryProofHeader) + uint64_t(p_surface_count) * sizeof(ParticlesExternalGuard::SurfaceProof) };
	const uint64_t strides[INSTANCE192_RESOURCE_COUNT] = { 192, 192, 4, 4, 4, 20, 1 };

	for (uint32_t i = 0; i < INSTANCE192_RESOURCE_COUNT; ++i) {
		const Error size_error = ExternalOutputConfig::checked_buffer_bytes(counts[i], strides[i], UINT32_MAX, layout.bytes[i]);
		if (size_error != OK) {
			return size_error;
		}
		layout.total_bytes += layout.bytes[i];
		layout.resource_count += layout.bytes[i] != 0;
	}
	if (p_aux_params_enabled && p_capacity > 0) {
		const Error aux_error = ExternalOutputConfig::checked_buffer_bytes(p_capacity, 32u, UINT32_MAX, layout.bytes[INSTANCE_AUX_PARAMS]);
		if (aux_error != OK) {
			return aux_error;
		}
		layout.total_bytes += layout.bytes[INSTANCE_AUX_PARAMS];
		layout.resource_count += layout.bytes[INSTANCE_AUX_PARAMS] != 0;
	}
	r_layout = layout;
	return OK;
}

Error ExternalOutputStorage::allocate_instance192(uint32_t p_capacity, uint32_t p_surface_count, bool p_motion_required, bool p_aux_params_enabled) {
	if (attempted || retirement_requested) {
		return ERR_ALREADY_IN_USE;
	}
	if (p_capacity > (1u << 20) || p_surface_count == 0 || p_surface_count > ExternalOutputConfig::MAX_FLATTENED_SURFACES) {
		return ERR_INVALID_PARAMETER;
	}
	const Error ready = device._native_disposal_preflight();
	if (ready != OK) {
		return ready;
	}
	Instance192Layout layout;
	const Error planned = plan_instance192(p_capacity, p_surface_count, layout, p_motion_required, p_aux_params_enabled);
	if (planned != OK) {
		return planned;
	}
	for (uint32_t i = 0; i < RESOURCE_COUNT; ++i) {
		resources[i].bytes = layout.bytes[i];
	}
	total_bytes = layout.total_bytes;
	attempted = true;
	for (uint32_t i = 0; i < RESOURCE_COUNT; ++i) {
		if (resources[i].bytes == 0) {
			continue; // Capacity zero or disabled motion omits unallocated record roles.
		}
		const BitField<RenderingDevice::StorageBufferUsage> usage = i == PROTECTED_COMMANDS ? BitField<RenderingDevice::StorageBufferUsage>(RenderingDevice::STORAGE_BUFFER_USAGE_DISPATCH_INDIRECT) : BitField<RenderingDevice::StorageBufferUsage>();
		creation_error = device._native_disposal_storage_create(resources[i].bytes, usage, resources[i].rid, resources[i].witness);
		if (creation_error != OK) {
			// Preserve original creation error and every completed prefix entry.
			// A busy/failed disposal is retryable; it cannot lose or reuse storage.
			request_retirement();
			return creation_error;
		}
	}
	return OK;
}

Error ExternalOutputStorage::allocate_instance192_owned(GDExtensionProductionPhysicalContextRefV1 context,
		GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t &group, uint32_t capacity, uint32_t surface_count, bool p_motion_required, bool p_aux_params_enabled) {
	if (attempted || retirement_requested || group) {
		return ERR_ALREADY_IN_USE;
	}
	Instance192Layout layout;
	Error error = plan_instance192(capacity, surface_count, layout, p_motion_required, p_aux_params_enabled);
	if (error != OK) {
		return error;
	}
	// Entire original group capacity is reserved before the first native call.
	// Failure here leaves this stable storage untouched and retryable.
	error = device._production_output_owned_reserve(context, pin, layout.resource_count, group);
	if (error != OK) {
		return error;
	}
	for (uint32_t i = 0; i < RESOURCE_COUNT; ++i) {
		resources[i].bytes = layout.bytes[i];
	}
	total_bytes = layout.total_bytes;
	attempted = true;
	uint32_t ordinal = 0;
	for (uint32_t i = 0; i < RESOURCE_COUNT; ++i) {
		if (!resources[i].bytes) {
			continue;
		}
		const BitField<RenderingDevice::StorageBufferUsage> usage = i == PROTECTED_COMMANDS ? BitField<RenderingDevice::StorageBufferUsage>(RenderingDevice::STORAGE_BUFFER_USAGE_DISPATCH_INDIRECT) : BitField<RenderingDevice::StorageBufferUsage>();
		creation_error = device._production_output_owned_storage_create(context, pin, group, ordinal++, resources[i].bytes, usage, resources[i].rid, resources[i].witness);
		if (creation_error != OK) {
			if (creation_error == ERR_BUSY && ordinal == 1 && resources[i].rid.is_null()) {
				// No original native factory succeeded. Restore only the exact
				// metadata reservation; its consumed group serial stays consumed.
				const Error rolled_back = device._production_output_owned_rollback(context, pin, group);
				if (rolled_back == OK) {
					for (uint32_t j = 0; j < RESOURCE_COUNT; ++j) {
						resources[j].bytes = 0;
					}
					total_bytes = 0;
					attempted = false;
					creation_error = OK;
					return ERR_BUSY;
				}
			}
			// Successful factories bind under the same original Registry and
			// recursive RD locks. Preserve that real prefix; only reservations
			// for factories that never created resources become UNCREATED.
			device._production_output_owned_cancel_unbound(pin, group);
			return creation_error;
		}
	}
	return OK;
}

Error ExternalOutputStorage::allocate_indexed144_owned(GDExtensionProductionPhysicalContextRefV1 context,
		GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t &group, uint32_t capacity, bool motion_required, uint32_t surface_count, bool p_generated_geometry) {
	if (p_generated_geometry && (!capacity || motion_required)) {
		return ERR_INVALID_PARAMETER;
	}
	if (attempted || retirement_requested || group) {
		return ERR_ALREADY_IN_USE;
	}
	ParticlesRibbonRD::Indexed144Layout ribbon_layout;
	Error error = ParticlesRibbonRD::plan_indexed144(capacity, motion_required, surface_count, ribbon_layout);
	if (error != OK) {
		return error;
	}
	uint32_t role_bytes[RESOURCE_COUNT] = {};
	role_bytes[CURRENT_RECORDS] = ribbon_layout.bytes[ParticlesRibbonRD::CURRENT_VERTICES];
	role_bytes[PREVIOUS_RECORDS] = ribbon_layout.bytes[ParticlesRibbonRD::ALIGNED_PREVIOUS_VERTICES];
	role_bytes[OUTPUT_META] = ribbon_layout.bytes[ParticlesRibbonRD::OUTPUT_META];
	role_bytes[GUARD_SCRATCH] = ribbon_layout.bytes[ParticlesRibbonRD::GUARD_SCRATCH];
	role_bytes[GUARD_STATUS] = ribbon_layout.bytes[ParticlesRibbonRD::GUARD_STATUS];
	role_bytes[PROTECTED_COMMANDS] = ribbon_layout.bytes[ParticlesRibbonRD::PROTECTED_COMMANDS];
	role_bytes[GEOMETRY_PROOF] = ribbon_layout.bytes[ParticlesRibbonRD::GEOMETRY_PROOF];
	role_bytes[INDEXED_VERTEX_HISTORY_VALIDITY] = ribbon_layout.bytes[ParticlesRibbonRD::VERTEX_HISTORY_VALIDITY];
	role_bytes[INDEXED_STAGING] = ribbon_layout.bytes[ParticlesRibbonRD::INDEX_STAGING];

	error = device._production_output_owned_reserve(context, pin, ribbon_layout.resource_count, group);
	if (error != OK) {
		return error;
	}
	for (uint32_t i = 0; i < RESOURCE_COUNT; ++i) {
		resources[i].bytes = role_bytes[i];
	}
	total_bytes = ribbon_layout.total_bytes;
	attempted = true;
	uint32_t ordinal = 0;
	for (uint32_t i = 0; i < RESOURCE_COUNT; ++i) {
		if (!resources[i].bytes) {
			continue;
		}
		const BitField<RenderingDevice::StorageBufferUsage> usage = i == PROTECTED_COMMANDS ? BitField<RenderingDevice::StorageBufferUsage>(RenderingDevice::STORAGE_BUFFER_USAGE_DISPATCH_INDIRECT) : BitField<RenderingDevice::StorageBufferUsage>();
		auto &resource = resources[i];
		if (p_generated_geometry && (i == CURRENT_RECORDS || i == INDEXED_STAGING)) {
			const uint32_t role = i == CURRENT_RECORDS ? GDEXTENSION_OUTPUT_ROLE_CURRENT_VERTICES_V1 : GDEXTENSION_OUTPUT_ROLE_INDEX_STAGING_V1;
			creation_error = device._production_output_owned_indexed144_buffer_create(context, pin, group, ordinal, role, resource.bytes, resource.rid, resource.witness);
		} else {
			creation_error = device._production_output_owned_storage_create(context, pin, group, ordinal, resource.bytes, usage, resource.rid, resource.witness);
		}
		++ordinal;
		if (creation_error != OK) {
			if (creation_error == ERR_BUSY && ordinal == 1 && resources[i].rid.is_null()) {
				const Error rolled_back = device._production_output_owned_rollback(context, pin, group);
				if (rolled_back == OK) {
					for (uint32_t j = 0; j < RESOURCE_COUNT; ++j) {
						resources[j].bytes = 0;
					}
					total_bytes = 0;
					attempted = false;
					creation_error = OK;
					return ERR_BUSY;
				}
			}
			device._production_output_owned_cancel_unbound(pin, group);
			return creation_error;
		}
	}
	return OK;
}

Error ExternalOutputStorage::allocate_light64_owned(GDExtensionProductionPhysicalContextRefV1 context,
		GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t &group, uint32_t capacity) {
	if (attempted || retirement_requested || group) {
		return ERR_ALREADY_IN_USE;
	}
	if (capacity > 256) {
		return ERR_INVALID_PARAMETER;
	}
	uint32_t role_bytes[RESOURCE_COUNT] = {};
	Error error = ExternalOutputConfig::checked_buffer_bytes(capacity, 64, UINT32_MAX, role_bytes[CURRENT_RECORDS]);
	if (error != OK) {
		return error;
	}
	role_bytes[OUTPUT_META] = 32;
	role_bytes[GUARD_STATUS] = 32;
	uint64_t planned_total = 0;
	uint32_t resource_count = 0;
	for (uint32_t i = 0; i < RESOURCE_COUNT; ++i) {
		planned_total += role_bytes[i];
		resource_count += role_bytes[i] != 0;
	}
	error = device._production_output_owned_reserve(context, pin, resource_count, group);
	if (error != OK) {
		return error;
	}
	for (uint32_t i = 0; i < RESOURCE_COUNT; ++i) {
		resources[i].bytes = role_bytes[i];
	}
	total_bytes = planned_total;
	attempted = true;
	uint32_t ordinal = 0;
	for (uint32_t i = 0; i < RESOURCE_COUNT; ++i) {
		if (!resources[i].bytes) {
			continue;
		}
		creation_error = device._production_output_owned_storage_create(context, pin, group, ordinal++, resources[i].bytes, BitField<RenderingDevice::StorageBufferUsage>(), resources[i].rid, resources[i].witness);
		if (creation_error != OK) {
			if (creation_error == ERR_BUSY && ordinal == 1 && resources[i].rid.is_null()) {
				const Error rolled_back = device._production_output_owned_rollback(context, pin, group);
				if (rolled_back == OK) {
					for (uint32_t j = 0; j < RESOURCE_COUNT; ++j) {
						resources[j].bytes = 0;
					}
					total_bytes = 0;
					attempted = false;
					creation_error = OK;
					return ERR_BUSY;
				}
			}
			device._production_output_owned_cancel_unbound(pin, group);
			return creation_error;
		}
	}
	return OK;
}

Error ExternalOutputStorage::request_retirement() {
	if (!device.is_main_instance || device.render_thread_id != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	// Mark future use closed even if the RD is temporarily busy. This component
	// never records native uses itself; owner retirement of actual uses is extra.
	retirement_requested = true;
	Error first_error = OK;
	for (uint32_t i = RESOURCE_COUNT; i > 0; --i) {
		Resource &resource = resources[i - 1];
		if (resource.rid.is_null()) {
			continue;
		}
		const Error error = device._native_disposal_free(resource.witness);
		if (error != OK && first_error == OK) {
			first_error = error;
		}
	}
	Report report;
	const Error observation = observe(report);
	return observation != OK ? observation : first_error;
}

Error ExternalOutputStorage::observe(Report &r_report) {
	if (!device.is_main_instance || device.render_thread_id != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	Report report;
	report.total_bytes = total_bytes;
	report.creation_error = creation_error;
	report.retirement_requested = retirement_requested;
	Error first_error = OK;
	for (uint32_t i = 0; i < RESOURCE_COUNT; ++i) {
		report.resources[i].bytes = resources[i].bytes;
		if (resources[i].rid.is_null()) {
			continue;
		}
		++report.created_count;
		const Error error = device._native_disposal_observe(resources[i].witness, report.resources[i].native);
		if (error != OK && first_error == OK) {
			first_error = error;
		}
		switch (report.resources[i].native.state) {
			case NativeDisposalWitness::State::LIVE:
				++report.live_count;
				break;
			case NativeDisposalWitness::State::ENQUEUED:
				++report.enqueued_count;
				break;
			case NativeDisposalWitness::State::RETURNED:
				++report.returned_count;
				break;
			case NativeDisposalWitness::State::UNKNOWN:
				++report.unknown_count;
				unknown = true;
				break;
			case NativeDisposalWitness::State::EMPTY:
				break; // Failed/unauthorized observation.
		}
	}
	report.all_driver_returned = !unknown && first_error == OK && report.returned_count == report.created_count;
	r_report = report;
	return unknown ? ERR_INVALID_DATA : first_error;
}

Error ExternalOutputStorage::maintenance() {
	return device._native_disposal_maintenance();
}

Error ExternalOutputStorage::destroy_if_returned(ExternalOutputStorage *&p_storage) {
	if (!p_storage) {
		return OK;
	}
	Report report;
	const Error error = p_storage->observe(report);
	if (error != OK) {
		return error;
	}
	if (!report.all_driver_returned) {
		return ERR_BUSY;
	}
	delete p_storage;
	p_storage = nullptr;
	return OK;
}
} // namespace RendererRD
