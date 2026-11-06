/**************************************************************************/
/*  gdextension_production_metadata.cpp                                    */
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

#include "core/extension/gdextension_library_exit_guard.h"
#include "core/extension/gdextension_production_request.h"
#include "core/os/thread.h"
#include "servers/rendering/rendering_device.h"

#include <cstddef>

namespace GDExtensionDeviceExit {
namespace {
bool metadata_layout(uint64_t bytes, uint32_t alignment) {
	return bytes && alignment && !(alignment & (alignment - 1)) && alignment <= alignof(std::max_align_t);
}
} // namespace

ProductionOutputMetadataLease::ProductionOutputMetadataLease(ProductionOutputMetadataLease &&other) noexcept {
	if (other.registry) {
		other.registry->production_output_metadata_move(*this, other);
	}
}
ProductionOutputMetadataLease &ProductionOutputMetadataLease::operator=(ProductionOutputMetadataLease &&other) noexcept {
	if (this != &other) {
		CRASH_COND(!is_empty());
		if (other.registry) {
			other.registry->production_output_metadata_move(*this, other);
		}
	}
	return *this;
}
ProductionOutputMetadataLease::~ProductionOutputMetadataLease() {
	CRASH_COND(!is_empty()); // Destruction is never evidence of an actual return.
}

void Registry::production_output_metadata_move(ProductionOutputMetadataLease &out, ProductionOutputMetadataLease &from) {
	MutexLock lock(mutex);
	CRASH_COND(!out.is_empty() || from.registry != this || !from.request || from.request->serial != from.request_serial || from.resource_slot >= from.request->resource_count);
	auto &resource = from.request->resources[from.resource_slot];
	CRASH_COND(resource.metadata_consumer != &from || resource.metadata_data != from.data || !from.data);
	out.registry = this;
	out.request = from.request;
	out.request_serial = from.request_serial;
	out.resource_slot = from.resource_slot;
	out.data = from.data;
	out.bytes = from.bytes;
	resource.metadata_consumer = &out;
	from.registry = nullptr;
	from.request = nullptr;
	from.request_serial = 0;
	from.resource_slot = UINT32_MAX;
	from.data = nullptr;
	from.bytes = 0;
}

Error Registry::production_output_metadata_create(ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref,
		uint32_t slot, RenderingDevice &expected, uint64_t bytes, uint32_t alignment, ProductionOutputMetadataLease &out) {
	MutexLock lock(mutex);
	if (!out.is_empty()) {
		return ERR_ALREADY_IN_USE;
	}
	if (!metadata_layout(bytes, alignment)) {
		return ERR_INVALID_PARAMETER;
	}
	ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	Error error = production_physical_request_locked(context, ref, request, device, true);
	if (error != OK) {
		return error;
	}
	if (device != &expected) {
		return ERR_UNAUTHORIZED;
	}
	if (slot >= request->resource_count) {
		return ERR_INVALID_PARAMETER;
	}
	auto &resource = request->resources[slot];
	const auto &declaration = resource.declaration;
	if (resource.request != request || resource.request_serial != request->serial ||
			declaration.resource_kind != GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1 ||
			declaration.ownership != GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1 || declaration.required_usage) {
		return ERR_INVALID_DATA;
	}
	if (bytes > declaration.physical_bytes || declaration.physical_bytes > UINT32_MAX) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	if (resource.metadata_created) {
		return ERR_ALREADY_IN_USE;
	}
	if (!request->frozen || request->next_action || request->graph_count || request->native_count || request->capture_inflight || request->effects_pending) {
		return ERR_BUSY;
	}
	if (request->revision > UINT64_MAX - uint64_t(request->capacity.graph_memberships) * 2 - uint64_t(request->capacity.native_memberships) * 2 - request->capacity.readbacks - uint64_t(request->capacity.resources) * 2 - 8) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	// This is a separate physical resource allocation. It is not an arena
	// payload slice, a spare use row, or a claim of Runtime account admission.
	void *data = Memory::alloc_static(size_t(declaration.physical_bytes));
	if (!data) {
		return ERR_OUT_OF_MEMORY;
	}
	CRASH_COND(uintptr_t(data) % alignment);
	out.registry = this;
	out.request = request;
	out.request_serial = request->serial;
	out.resource_slot = slot;
	out.data = data;
	out.bytes = declaration.physical_bytes;
	resource.metadata_created = true;
	resource.metadata_returned = false;
	resource.metadata_data = data;
	resource.metadata_consumer = &out;
	resource.imported = true;
	request->native_effect_entered = true;
	++request->revision;
	return OK;
}

Error Registry::production_output_metadata_validate(ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref,
		const ProductionOutputMetadataLease &lease, RenderingDevice &expected, uint64_t bytes, uint32_t alignment) {
	MutexLock lock(mutex);
	if (!metadata_layout(bytes, alignment)) {
		return ERR_INVALID_PARAMETER;
	}
	ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	Error error = production_physical_request_locked(context, ref, request, device, true);
	if (error != OK) {
		return error;
	}
	if (device != &expected) {
		return ERR_UNAUTHORIZED;
	}
	if (lease.registry != this || lease.request != request || lease.request_serial != request->serial || lease.resource_slot >= request->resource_count) {
		return ERR_INVALID_DATA;
	}
	const auto &resource = request->resources[lease.resource_slot];
	if (!resource.metadata_created || resource.metadata_returned || resource.metadata_consumer != &lease ||
			!resource.metadata_data || resource.metadata_data != lease.data || lease.bytes != resource.declaration.physical_bytes) {
		return ERR_INVALID_DATA;
	}
	return bytes <= lease.bytes && uintptr_t(lease.data) % alignment == 0 ? OK : ERR_PARAMETER_RANGE_ERROR;
}

Error Registry::production_output_metadata_bind(ProductionOutputMetadataLease &lease, const ProductionOutputPin &pin) {
	MutexLock lock(mutex);
	if (lease.registry != this || !lease.request || lease.request->serial != lease.request_serial || lease.resource_slot >= lease.request->resource_count ||
			pin.registry != this || pin.request != lease.request || pin.request_serial != lease.request_serial || !pin.serial) {
		return ERR_INVALID_DATA;
	}
	auto &request = *lease.request;
	auto &resource = request.resources[lease.resource_slot];
	RenderingDevice *device = Object::cast_to<RenderingDevice>(device_records[request.attachment->device].object);
	if (!device || !production_output_pin_matches_locked(pin, { instance, request.serial }, *device) ||
			resource.metadata_consumer != &lease || resource.metadata_data != lease.data || resource.metadata_returned) {
		return ERR_INVALID_DATA;
	}
	if (device_records[request.attachment->device].execution_thread != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	if (resource.metadata_root_serial && resource.metadata_root_serial != pin.serial) {
		return ERR_ALREADY_IN_USE;
	}
	resource.metadata_root_serial = pin.serial;
	return OK;
}

Error Registry::production_output_metadata_return(ProductionOutputMetadataLease &lease) {
	MutexLock lock(mutex);
	if (lease.is_empty()) {
		return ERR_DOES_NOT_EXIST;
	}
	if (lease.registry != this || !lease.request || lease.request->serial != lease.request_serial || lease.resource_slot >= lease.request->resource_count) {
		return ERR_INVALID_DATA;
	}
	auto &request = *lease.request;
	if (device_records[request.attachment->device].execution_thread != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	auto &resource = request.resources[lease.resource_slot];
	if (resource.metadata_consumer != &lease || resource.metadata_data != lease.data || !lease.data || resource.metadata_returned) {
		return ERR_INVALID_DATA;
	}
	const uintptr_t block = uintptr_t(lease.data);
	const uintptr_t consumer = uintptr_t(&lease);
	if ((consumer >= block && consumer - block < lease.bytes) ||
			(consumer < block && block - consumer < sizeof(lease))) {
		return ERR_INVALID_PARAMETER; // Move a placement lease out before freeing its block.
	}
	if (resource.metadata_root_serial) {
		for (uint32_t i = request.use_count + request.binding_used; i < request.capacity.resource_uses; ++i) {
			const auto &cell = request.uses[i];
			if (cell.output_role == ProductionUseCell::OutputRole::NATIVE_CONSUMER && cell.output_pin_serial == resource.metadata_root_serial) {
				return ERR_BUSY;
			}
		}
	}
	if (request.revision == UINT64_MAX) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	// Caller has destroyed all placement objects and moved the lease out of
	// this block before entry. No GPU completion or DSO callback is invented.
	Memory::free_static(lease.data);
	resource.metadata_data = nullptr;
	resource.metadata_consumer = nullptr;
	resource.metadata_returned = true;
	++request.revision;
	lease.registry = nullptr;
	lease.request = nullptr;
	lease.request_serial = 0;
	lease.resource_slot = UINT32_MAX;
	lease.data = nullptr;
	lease.bytes = 0;
	return OK;
}
} // namespace GDExtensionDeviceExit
