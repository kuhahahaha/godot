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

namespace RendererRD {

Error ExternalOutputStorage::plan_instance192(uint32_t p_capacity, uint32_t p_surface_count, Instance192Layout &r_layout) {
	if (p_capacity > (1u << 20) || p_surface_count == 0 || p_surface_count > ExternalOutputConfig::MAX_FLATTENED_SURFACES) {
		return ERR_INVALID_PARAMETER;
	}
	// Compute every extent before any native creation. RD has no public maximum
	// storage-buffer-byte limit; actual allocation failure stays a native error.
	// Fixed control extents match ParticlesExternalGuard's Instance192 ABI.
	Instance192Layout layout;
	const uint64_t counts[RESOURCE_COUNT] = { p_capacity, p_capacity, 8, 16, 8, p_surface_count, 224 + uint64_t(p_surface_count) * 144 };
	const uint64_t strides[RESOURCE_COUNT] = { 192, 192, 4, 4, 4, 20, 1 };

	for (uint32_t i = 0; i < RESOURCE_COUNT; ++i) {
		const Error size_error = ExternalOutputConfig::checked_buffer_bytes(counts[i], strides[i], UINT32_MAX, layout.bytes[i]);
		if (size_error != OK) {
			return size_error;
		}
		// Seven u32 extents sum safely in u64; no narrowing of total occurs.
		layout.total_bytes += layout.bytes[i];
		layout.resource_count += layout.bytes[i] != 0;
	}
	r_layout = layout;
	return OK;
}

Error ExternalOutputStorage::allocate_instance192(uint32_t p_capacity, uint32_t p_surface_count) {
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
	const Error planned = plan_instance192(p_capacity, p_surface_count, layout);
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
			continue; // Capacity zero has no current/previous record allocation.
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
		GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t &group, uint32_t capacity, uint32_t surface_count) {
	if (attempted || retirement_requested || group) {
		return ERR_ALREADY_IN_USE;
	}
	Instance192Layout layout;
	Error error = plan_instance192(capacity, surface_count, layout);
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
