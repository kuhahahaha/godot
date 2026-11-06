/**************************************************************************/
/*  gdextension_production_owned.cpp                                      */
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

#include "core/extension/gdextension_production_request.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "servers/rendering/rendering_device.h"

#include <cstring>

namespace GDExtensionDeviceExit {
#ifdef TESTS_ENABLED
thread_local Error (*test_production_before_maintenance)(ProductionRequest &, RenderingDevice &) = nullptr;
#endif
namespace {
constexpr GDExtensionDeviceExitAbi ABI = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
GDExtensionDeviceSessionResultV1 result(Error error) {
	return { int32_t(error), error == OK ? 0u : 16u, 0, 0 };
}
template <class T>
bool valid(const T *value) {
	return value && uintptr_t(value) % alignof(T) == 0;
}
bool abi(const GDExtensionDeviceExitAbi &value) {
	return value.abi_version == 1 && value.struct_size == sizeof(value) && !value.reserved;
}
struct RdUnlock {
	Mutex &mutex;
	~RdUnlock() { mutex.unlock(); }
};
} //namespace

Error Registry::production_create_accept_locked(ProductionRequest &request, ProductionControlKind kind, uint32_t index) {
	if (request.control.state != ProductionControlState::OWNER_CONSUMED || !request.next_control_serial) {
		return ERR_BUSY;
	}
	auto &cell = request.control;
	cell = {};
	cell.serial = request.next_control_serial;
	request.next_control_serial = cell.serial == UINT64_MAX ? 0 : cell.serial + 1;
	cell.input.request_serial = request.serial;
	cell.input.record_generation = request.operation.record_generation;
	cell.input.kind = kind;
	cell.input.resource_slot = index;
	cell.state = ProductionControlState::PENDING;
	return OK;
}

Error Registry::production_create_service(GDExtensionProductionRequestRefV1 ref, ProductionControlKind kind, uint32_t index) {
	uint64_t serial = 0;
	bool returned = false;
	{
		MutexLock lock(mutex);
		ProductionRequest *request = nullptr;
		RenderingDevice *rd = nullptr;
		const Error error = production_request_locked(ref, false, request, rd);
		if (error != OK) {
			return error;
		}
		const auto &cell = request->control;
		if (cell.state == ProductionControlState::OWNER_CONSUMED) {
			auto &slot = request->resources[index];
			if (!slot.creating) {
				return slot.handoff_error;
			}
			ProductionRequest *admitted = nullptr;
			const Error admission = production_request_locked(ref, true, admitted, rd);
			if (admission != OK) {
				slot.creating = false;
				slot.handoff_error = admission;
				request->returns[index].pending = false;
				return admission;
			}
			const Error accepted = production_create_accept_locked(*request, kind, index);
			if (accepted != OK) {
				return accepted;
			}
		}
		if (cell.input.kind != kind || cell.input.resource_slot != index) {
			return ERR_BUSY;
		}
		serial = cell.serial;
		returned = cell.state == ProductionControlState::RETURNED;
	}
	if (returned) {
		ProductionControlOutput output = {};
		const Error error = production_control_consume(ref, serial, &output);
		return error == OK ? Error(output.result.native_error) : error;
	}
	const Error scheduled = production_control_schedule(ref, serial);
	return scheduled == OK ? ERR_BUSY : scheduled;
}

GDExtensionDeviceSessionResultV1 Registry::production_storage_create(GDExtensionProductionRequestRefV1 ref, uint32_t index, const GDExtensionStorageCreateRequestV1 *input, GDExtensionStorageBackingV1 *out) {
	if (!valid(input) || !valid(out) || !abi(input->abi) || input->reserved || !input->bytes || input->bytes > UINT32_MAX || !input->allocation_generation || !input->create_attempt) {
		return result(ERR_INVALID_PARAMETER);
	}
	const auto origin = *input;
	{
		MutexLock lock(mutex);
		ProductionRequest *request = nullptr;
		RenderingDevice *rd = nullptr;
		Error error = production_request_locked(ref, false, request, rd);
		if (error != OK) {
			return result(error);
		}
		ProductionResourceSlot *slot = nullptr;
		if ((error = production_resource_slot_locked(*request, index, slot)) != OK) {
			return result(error);
		}
		const auto &declaration = slot->declaration;
		if (declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || declaration.ownership != GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1 || origin.allocation_slot != declaration.original_slot || origin.part != declaration.original_part || origin.allocation_generation != declaration.original_generation || origin.bytes != declaration.physical_bytes || index >= request->capacity.return_cells) {
			return result(ERR_INVALID_DATA);
		}
		const uint64_t native_usage = uint64_t(RDD::BUFFER_USAGE_TRANSFER_FROM_BIT | RDD::BUFFER_USAGE_TRANSFER_TO_BIT | RDD::BUFFER_USAGE_STORAGE_BIT);
		if (declaration.required_usage & ~native_usage) {
			return result(ERR_UNAVAILABLE);
		}
		if (slot->owned_serial) {
			if (std::memcmp(&slot->create_origin, &origin, sizeof(origin))) {
				return result(ERR_INVALID_DATA);
			}
			if (slot->returned) {
				return result(ERR_DOES_NOT_EXIST);
			}
		} else {
			// The existing NewWork gate remains the sole preparation admission.
			if ((error = production_request_locked(ref, true, request, rd)) != OK) {
				return result(error);
			}
			if (request->frozen || request->capture_inflight || request->revision > UINT64_MAX - uint64_t(request->capacity.graph_memberships) * 2 - uint64_t(request->capacity.native_memberships) * 2 - request->capacity.readbacks - request->capacity.resources - 8 || production_serials.exhausted || production_serials.next > UINT64_MAX - 1 || !request->next_control_serial) {
				return result(ERR_BUSY);
			}
			production_serials.take(slot->owned_serial);
			production_serials.take(slot->retire_serial);
			slot->create_origin = origin;
			slot->owned_physical.bridge = request->attachment->serial;
			slot->owned_physical.child = slot->owned_serial;
			slot->creating = true;
			request->returns[index].serial = slot->retire_serial;
			request->returns[index].pending = true;
			error = production_create_accept_locked(*request, ProductionControlKind::STORAGE_CREATE, index);
			CRASH_COND(error != OK);
		}
	}
	const Error service = production_create_service(ref, ProductionControlKind::STORAGE_CREATE, index);
	MutexLock lock(mutex);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	const Error error = production_request_locked(ref, false, request, rd);
	if (error != OK) {
		return result(error);
	}
	*out = request->resources[index].owned_backing;
	return result(service);
}

GDExtensionDeviceSessionResultV1 Registry::production_storage_retire(GDExtensionProductionRequestRefV1 ref, uint32_t index, const GDExtensionStorageRetireRequestV1 *input, GDExtensionStorageRetireRefV1 *out, GDExtensionStorageRetireReportV1 *report) {
	if (!valid(input) || !valid(out) || !valid(report) || !abi(input->abi) || input->reserved || !input->retire_attempt) {
		return result(ERR_INVALID_PARAMETER);
	}
	const auto origin = *input;
	MutexLock lock(mutex);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_request_locked(ref, false, request, rd);
	if (error != OK) {
		return result(error);
	}
	if (index >= request->resource_count || request->capture_inflight) {
		return result(ERR_BUSY);
	}
	auto &slot = request->resources[index];
	if (slot.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || slot.declaration.ownership != GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1 || !slot.owned_serial || slot.creating || slot.returned || origin.backing.registry_instance != instance || origin.backing.handle_serial != slot.owned_serial || origin.rid != slot.primary.get_id() || origin.observed_backing_serial != slot.backing_serial || origin.allocation_slot != slot.create_origin.allocation_slot || origin.part != slot.create_origin.part || origin.allocation_generation != slot.create_origin.allocation_generation) {
		return result(ERR_INVALID_DATA);
	}
	if (slot.retiring && std::memcmp(&slot.retire_origin, &origin, sizeof(origin))) {
		return result(ERR_INVALID_DATA);
	}
	if (!slot.retiring) {
		if (request->revision > UINT64_MAX - uint64_t(request->capacity.graph_memberships) * 2 - uint64_t(request->capacity.native_memberships) * 2 - request->capacity.readbacks - request->capacity.resources - 8) {
			return result(ERR_PARAMETER_RANGE_ERROR);
		}
		++request->revision;
		slot.retiring = true;
		slot.owned_physical.use_closed.store(true);
		slot.retire_origin = origin;
		auto &state = slot.retire_report;
		state.abi = ABI;
		state.rid = origin.rid;
		state.backing_serial = origin.observed_backing_serial;
		state.retire_cell_serial = slot.retire_serial;
		state.frame_slot = UINT32_MAX;
		state.allocation_generation = origin.allocation_generation;
		state.retire_attempt = origin.retire_attempt;
		state.allocation_slot = origin.allocation_slot;
		state.part = origin.part;
	}
	*out = { instance, slot.retire_serial };
	*report = slot.retire_report;
	return result(OK);
}

GDExtensionDeviceSessionResultV1 Registry::production_storage_observe(GDExtensionProductionRequestRefV1 ref, GDExtensionStorageRetireRefV1 retire, const GDExtensionProductionServiceBudgetV1 *budget, GDExtensionStorageRetireReportV1 *out) {
	if (!valid(budget) || !valid(out) || !production_dto_valid(*budget) || budget->reserved) {
		return result(ERR_INVALID_PARAMETER);
	}
	// Validate the exact retire identity before any request-wide maintenance.
	{
		MutexLock lock(mutex);
		ProductionRequest *request = nullptr;
		RenderingDevice *rd = nullptr;
		const Error error = production_request_locked(ref, false, request, rd);
		if (error != OK) {
			return result(error);
		}
		bool found = false;
		for (uint32_t i = 0; i < request->resource_count; ++i) {
			const auto &slot = request->resources[i];
			found |= retire.registry_instance == instance && retire.handle_serial && slot.retire_serial == retire.handle_serial && slot.retiring && !slot.returned;
		}
		if (!found) {
			return result(ERR_DOES_NOT_EXIST);
		}
	}
	const Error service = production_observe_service(ref, *budget);
	MutexLock lock(mutex);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	const Error error = production_request_locked(ref, false, request, rd);
	if (error != OK) {
		return result(error);
	}
	for (uint32_t i = 0; i < request->resource_count; ++i) {
		const auto &slot = request->resources[i];
		if (retire.registry_instance == instance && retire.handle_serial && slot.retire_serial == retire.handle_serial && slot.retiring && !slot.returned) {
			*out = slot.retire_report;
			return result(service);
		}
	}
	return result(ERR_DOES_NOT_EXIST);
}

GDExtensionDeviceSessionResultV1 Registry::production_storage_release(GDExtensionProductionRequestRefV1 ref, GDExtensionStorageRetireRefV1 retire) {
	MutexLock lock(mutex);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_request_locked(ref, false, request, rd);
	if (error != OK) {
		return result(error);
	}
	if (request->capture_inflight) {
		return result(ERR_BUSY);
	}
	for (uint32_t i = 0; i < request->resource_count; ++i) {
		auto &slot = request->resources[i];
		if (retire.registry_instance != instance || !retire.handle_serial || slot.retire_serial != retire.handle_serial) {
			continue;
		}
		if (!slot.retiring || slot.retire_report.state != 1 || !slot.owned_physical.free_event || slot.use_count) {
			return result(ERR_BUSY);
		}
		if (!slot.returned && request->revision > UINT64_MAX - uint64_t(request->capacity.graph_memberships) * 2 - uint64_t(request->capacity.native_memberships) * 2 - request->capacity.readbacks - request->capacity.resources - 8) {
			return result(ERR_PARAMETER_RANGE_ERROR);
		}
		if (!slot.returned) {
			++request->revision;
		}
		slot.returned = true;
		slot.imported = false;
		request->returns[i].pending = false;
		return result(OK);
	}
	return result(ERR_DOES_NOT_EXIST);
}

GDExtensionDeviceSessionResultV1 Registry::production_set_create(GDExtensionProductionRequestRefV1 ref, uint32_t index, uint32_t unit_index, uint32_t shader_set, const GDExtensionProductionSetBindingV1 *bindings, uint32_t count) {
	if (!count || !valid(bindings)) {
		return result(ERR_INVALID_PARAMETER);
	}
	{
		MutexLock lock(mutex);
		ProductionRequest *request = nullptr;
		RenderingDevice *rd = nullptr;
		Error error = production_request_locked(ref, false, request, rd);
		if (error != OK) {
			return result(error);
		}
		if (index >= request->resource_count || unit_index >= request->resource_count || index >= request->capacity.return_cells) {
			return result(ERR_INVALID_DATA);
		}
		auto &slot = request->resources[index];
		const auto &unit = request->resources[unit_index];
		if (slot.declaration.resource_kind != GDEXTENSION_PRODUCTION_SET_V1 || slot.declaration.ownership != GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1 || !unit.imported || unit.declaration.resource_kind != GDEXTENSION_PRODUCTION_UNIT_V1 || unit.use_count != 2 || slot.declaration.original_part != shader_set) {
			return result(ERR_INVALID_DATA);
		}
		if (slot.owned_serial) {
			if (slot.imported_unit_slot != unit_index || slot.shader_set != shader_set || slot.binding_count != count) {
				return result(ERR_INVALID_DATA);
			}
			for (uint32_t i = 0; i < count; ++i) {
				if (std::memcmp(&request->uses[slot.first_binding + i].binding, &bindings[i], sizeof(bindings[i]))) {
					return result(ERR_INVALID_DATA);
				}
			}
		} else {
			if ((error = production_request_locked(ref, true, request, rd)) != OK) {
				return result(error);
			}
			if (request->frozen || request->capture_inflight || request->binding_used > request->capacity.resource_uses - request->use_count || count > request->capacity.resource_uses - request->use_count - request->binding_used || request->revision > UINT64_MAX - uint64_t(request->capacity.graph_memberships) * 2 - uint64_t(request->capacity.native_memberships) * 2 - request->capacity.readbacks - request->capacity.resources - 8 || production_serials.exhausted || production_serials.next > UINT64_MAX - 2 || !request->next_control_serial) {
				return result(ERR_INVALID_DATA);
			}
			// Copy the complete finite binding origin before queue publication.
			// Native shader reflection is checked on RT before the first effect.
			for (uint32_t i = 0; i < count; ++i) {
				const auto &binding = bindings[i];
				if (!production_dto_valid(binding) || binding.set != shader_set || binding.array_element || binding.offset || binding.resource_slot >= request->resource_count || (i && bindings[i - 1].binding >= binding.binding)) {
					return result(ERR_INVALID_DATA);
				}
				const auto &resource = request->resources[binding.resource_slot];
				if (!resource.imported || resource.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || resource.use_count != 1 || resource.retiring || binding.bytes != resource.declaration.physical_bytes || !resource.uses[0].same_physical() || !resource.uses[0].linked()) {
					return result(ERR_INVALID_DATA);
				}
			}
			production_serials.take(slot.owned_serial);
			production_serials.take(slot.retire_serial);
			production_serials.take(slot.owned_physical.serial);
			slot.owned_physical.bridge = request->attachment->serial;
			slot.owned_physical.child = slot.owned_serial;
			slot.first_binding = request->use_count + request->binding_used;
			slot.binding_count = count;
			slot.imported_unit_slot = unit_index;
			slot.shader_set = shader_set;
			for (uint32_t i = 0; i < count; ++i) {
				request->uses[slot.first_binding + i].binding = bindings[i];
			}
			request->binding_used += count;
			slot.creating = true;
			request->returns[index].pending = true;
			request->returns[index].serial = slot.retire_serial;
			error = production_create_accept_locked(*request, ProductionControlKind::SET_CREATE, index);
			CRASH_COND(error != OK);
		}
	}
	return result(production_create_service(ref, ProductionControlKind::SET_CREATE, index));
}

void Registry::production_raster_prepare_report_locked(const ProductionRequest &request, GDExtensionProductionRasterPrepareReportV1 &out) const {
	out = {};
	out.abi = production_dto_abi<GDExtensionProductionRasterPrepareReportV1>();
	out.request_serial = request.serial;
	out.control_serial = request.raster_prepare_control;
	out.create_attempt = request.raster_prepare_origin.create_attempt;
	out.revision = request.revision;
	out.phase = request.raster_prepare_phase;
	out.result = result(request.resources[0].creating ? ERR_BUSY : request.resources[0].handoff_error);
	uint64_t *serials[4] = { &out.framebuffer_serial, &out.depth_serial, &out.shader_serial, &out.pipeline_serial };
	for (uint32_t i = 0; i < 4; ++i) {
		const auto &slot = request.resources[i];
		*serials[i] = slot.backing_serial;
		if (slot.primary.is_valid()) {
			out.known_mask |= 1u << i;
		}
		if (slot.returned && slot.primary.is_valid()) {
			out.returned_mask |= 1u << i;
		}
	}
}

GDExtensionDeviceSessionResultV1 Registry::production_raster_prepare_report(GDExtensionProductionRequestRefV1 ref, GDExtensionProductionRasterPrepareReportV1 *out) {
	if (!valid(out)) {
		return result(ERR_INVALID_PARAMETER);
	}
	*out = {};
	MutexLock lock(mutex);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_request_locked(ref, false, request, rd);
	if (error != OK) {
		return result(error);
	}
	if (!request->raster_reserved || request->resource_count != 4 || !request->raster_prepare_origin.create_attempt) {
		return result(ERR_DOES_NOT_EXIST);
	}
	production_raster_prepare_report_locked(*request, *out);
	return result(OK);
}

GDExtensionDeviceSessionResultV1 Registry::production_raster_prepare(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionRasterPrepareV1 *input, const void *binary, uint64_t bytes, GDExtensionProductionRasterPrepareReportV1 *out) {
	if (!valid(out)) {
		return result(ERR_INVALID_PARAMETER);
	}
	*out = {};
	if (!valid(input) || !production_dto_valid(*input) || input->reserved || !input->create_attempt || !input->width || !input->height || !binary || !bytes || bytes > INT32_MAX || input->shader_bytes != bytes) {
		return result(ERR_INVALID_PARAMETER);
	}
	{
		MutexLock lock(mutex);
		ProductionRequest *request = nullptr;
		RenderingDevice *rd = nullptr;
		Error error = production_request_locked(ref, false, request, rd);
		if (error != OK) {
			return result(error);
		}
		if (!request->raster_reserved || request->resource_count != 4 || request->capacity.return_cells < 4) {
			return result(ERR_INVALID_DATA);
		}
		auto &action = request->actions[0];
		if (request->raster_prepare_origin.create_attempt) {
			const auto &origin = request->raster_prepare_origin;
			if (origin.create_attempt != input->create_attempt || origin.width != input->width || origin.height != input->height || origin.shader_bytes != bytes || action.payload_bytes != bytes || std::memcmp(request->attachment->payload + action.payload_offset, binary, size_t(bytes))) {
				return result(ERR_INVALID_DATA);
			}
			if (!request->resources[0].creating && request->control.state == ProductionControlState::OWNER_CONSUMED) {
				production_raster_prepare_report_locked(*request, *out);
				return out->result;
			}
		} else {
			error = production_request_locked(ref, true, request, rd);
			if (error != OK) {
				return result(error);
			}
			if (request->frozen || request->capture_inflight || !request->next_control_serial || request->payload_used > request->capacity.retained_payload_bytes || bytes > request->capacity.retained_payload_bytes - request->payload_used || production_serials.exhausted || production_serials.next > UINT64_MAX - 8 || request->revision > UINT64_MAX - 32) {
				return result(ERR_PARAMETER_RANGE_ERROR);
			}
			uint64_t owner = 0;
			production_serials.take(owner);
			for (uint32_t i = 0; i < 4; ++i) {
				auto &slot = request->resources[i];
				slot.owned_serial = owner; // One raster owner, four distinct physical identities.
				production_serials.take(slot.retire_serial);
				production_serials.take(slot.owned_physical.serial);
				slot.owned_physical.bridge = request->attachment->serial;
				slot.owned_physical.child = owner;
				slot.handoff_error = ERR_BUSY;
				request->returns[i].serial = slot.retire_serial;
				request->returns[i].pending = true;
			}
			request->resources[0].creating = true;
			request->raster_prepare_origin = *input;
			action.payload_offset = request->payload_offset + request->payload_used;
			action.payload_bytes = bytes;
			std::memcpy(request->attachment->payload + action.payload_offset, binary, size_t(bytes));
			request->payload_used += bytes;
			error = production_create_accept_locked(*request, ProductionControlKind::RASTER_CREATE, 0);
			CRASH_COND(error != OK);
			request->raster_prepare_control = request->control.serial;
		}
	}
	const Error service = production_create_service(ref, ProductionControlKind::RASTER_CREATE, 0);
	const auto observed = production_raster_prepare_report(ref, out);
	return observed.native_error == OK ? result(service) : observed;
}

Error Registry::production_raster_prepare_finish_locked(ProductionRequest &request, Error error) {
	if (!request.raster_reserved || request.resource_count != 4 || request.capacity.return_cells < 4 || request.raster_prepare_control != request.control.serial) {
		return ERR_INVALID_DATA;
	}
	request.resources[0].creating = false;
	request.resources[0].handoff_error = error;
	for (uint32_t i = 0; i < 4; ++i) {
		if (!request.resources[i].primary.is_valid()) {
			request.returns[i].pending = false;
		}
	}
	if (error != OK) {
		request.actions[0].prepared = false;
	}
	if (request.revision != UINT64_MAX) {
		++request.revision;
	}
	return error;
}

Error Registry::production_raster_prepare_execute_locked(ProductionRequest &request, RenderingDevice &rd) {
	if (!request.raster_reserved || request.resource_count != 4 || request.capacity.return_cells < 4 || !request.resources[0].creating || request.raster_prepare_control != request.control.serial) {
		return ERR_INVALID_DATA;
	}
	auto finish = [&](Error error) { return production_raster_prepare_finish_locked(request, error); };
	const auto &attachment = *request.attachment;
	const auto &owner = records[attachment.library];
	auto admission_closed = [&]() {
		return attachment.closing || !owner.attached || owner.quiescing || owner.close_started || owner.close_error != OK || owner.guard_enrollment != GuardEnrollment::QUALIFIED || request.cancelled || request.release_started || request.frozen || !device_records[attachment.device].live || rd.production_finalization_started.load(std::memory_order_acquire) || !rd.driver;
	};
	if (admission_closed()) {
		return finish(ERR_UNAVAILABLE);
	}
	if (rd.render_thread_id != Thread::get_caller_id() || !rd._thread_safe_.try_lock()) {
		return finish(ERR_BUSY); // A returned attempt is never replayed under a new control.
	}
	RdUnlock unlock{ rd._thread_safe_ };
	if (rd._recording_error() != OK || rd.draw_list.active || rd.compute_list.active || rd.raytracing_list.active) {
		return finish(ERR_BUSY);
	}
	Vector<uint8_t> binary;
	if (binary.resize(int(request.raster_prepare_origin.shader_bytes)) != OK) {
		return finish(ERR_OUT_OF_MEMORY);
	}
	std::memcpy(binary.ptrw(), request.attachment->payload + request.actions[0].payload_offset, size_t(binary.size()));
	// All native return credits and RID cells exist before the first allocation.
	RID reserved[4];
	auto discard = [&](uint32_t i) {
		if (reserved[i].is_valid()) {
			switch (i) {
				case 0:
					rd.framebuffer_owner.initialize_rid(reserved[i], RenderingDevice::Framebuffer());
					rd.framebuffer_owner.free(reserved[i]);
					break;
				case 1:
					rd.texture_owner.initialize_rid(reserved[i], RenderingDevice::Texture());
					rd.texture_owner.free(reserved[i]);
					break;
				case 2:
					rd.shader_owner.initialize_rid(reserved[i], RenderingDevice::Shader());
					rd.shader_owner.free(reserved[i]);
					break;
				case 3:
					rd.render_pipeline_owner.initialize_rid(reserved[i], RenderingDevice::RenderPipeline());
					rd.render_pipeline_owner.free(reserved[i]);
					break;
			}
			reserved[i] = RID();
		}
		if (request.resources[i].owned_physical.event_capacity_reserved) {
			rd._qualification_release_free_event(&request.resources[i].owned_physical);
		}
	};
	for (uint32_t i = 0; i < 4; ++i) {
		if (!rd._qualification_reserve_free_event(&request.resources[i].owned_physical)) {
			for (uint32_t j = 0; j < i; ++j) {
				discard(j);
			}
			return finish(ERR_PARAMETER_RANGE_ERROR);
		}
	}
	reserved[0] = rd.framebuffer_owner.allocate_rid();
	reserved[1] = rd.texture_owner.allocate_rid();
	reserved[2] = rd.shader_owner.allocate_rid();
	reserved[3] = rd.render_pipeline_owner.allocate_rid();
	for (uint32_t i = 0; i < 4; ++i) {
		if (reserved[i].is_null()) {
			for (uint32_t j = 0; j < 4; ++j) {
				discard(j);
			}
			return finish(ERR_OUT_OF_MEMORY);
		}
	}
	RenderingDevice::TextureFormat format;
	format.texture_type = RenderingDevice::TEXTURE_TYPE_2D;
	format.format = RenderingDevice::DATA_FORMAT_D32_SFLOAT;
	format.width = request.raster_prepare_origin.width;
	format.height = request.raster_prepare_origin.height;
	format.usage_bits = RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	const uint32_t order[4] = { 1, 2, 0, 3 };
	Error error = OK;
	++request.capture_inflight;
	for (uint32_t step = 0; step < 4; ++step) {
		if (admission_closed()) {
			error = ERR_UNAVAILABLE;
			break;
		}
		const uint32_t i = order[step];
		auto &slot = request.resources[i];
		request.raster_prepare_phase = step + 1;
		production_native_effect_enter_locked(request); // Allocation is not append.
		mutex.unlock();
		RID created;
		switch (i) {
			case 1:
				created = rd._texture_create_internal(format, RenderingDevice::TextureView(), {}, &slot.owned_physical, reserved[i]);
				break;
			case 2:
				created = rd._qualification_shader_create(binary, RID(), {}, &slot.owned_physical, reserved[i]);
				break;
			case 0:
				created = rd._qualification_framebuffer_create(request.resources[1].primary, &slot.owned_physical, &request.resources[1].owned_physical, reserved[i]);
				break;
			case 3:
				created = rd._qualification_render_pipeline_create(request.resources[2].primary, rd.framebuffer_get_format(request.resources[0].primary), &slot.owned_physical, reserved[i]);
				break;
		}
		mutex.lock();
		if (created.is_null()) {
			error = ERR_CANT_CREATE;
			break;
		}
		reserved[i] = RID(); // The factory published this original reservation.
		slot.primary = created;
		slot.backing_serial = slot.owned_physical.serial;
		slot.owned_physical.rid = created.get_id();
		error = slot.uses[0].attach(slot.owned_physical, request, request.serial);
		if (error == OK) {
			slot.use_count = 1;
			slot.imported = true;
		}
		if (error != OK || request.source_device_lost || admission_closed()) {
			error = error != OK ? error : ERR_UNAVAILABLE;
			break;
		}
	}
	--request.capture_inflight;
	for (uint32_t i = 0; i < 4; ++i) {
		if (!request.resources[i].primary.is_valid()) {
			discard(i);
		}
	}
	if (error == OK) {
		request.raster_action.framebuffer_serial = request.resources[0].backing_serial;
		request.raster_action.shader_serial = request.resources[2].backing_serial;
		request.raster_action.pipeline_serial = request.resources[3].backing_serial;
		request.actions[0].prepared = true;
		request.raster_prepare_phase = 5;
	}
	return finish(error);
}

Error Registry::production_create_execute_locked(ProductionRequest &request, RenderingDevice &rd, ProductionControlKind kind, uint32_t index) {
	if (index >= request.resource_count || index >= request.capacity.return_cells) {
		return ERR_INVALID_DATA;
	}
	auto &slot = request.resources[index];
	if (!slot.creating || !slot.owned_serial || slot.request != &request || slot.request_serial != request.serial) {
		return ERR_INVALID_DATA;
	}
	auto finish = [&](Error error) {
		slot.creating = false;
		slot.handoff_error = error;
		if (!slot.primary.is_valid()) {
			request.returns[index].pending = false;
		}
		return error;
	};
	const auto &attachment = *request.attachment;
	const auto &owner = records[attachment.library];
	const auto &device = device_records[attachment.device];
	// Queuing preparation is not a grant to create after NewWork was sealed.
	if (attachment.closing || !owner.attached || owner.quiescing || owner.close_started || owner.close_error != OK || owner.guard_enrollment != GuardEnrollment::QUALIFIED || request.cancelled || request.release_started || request.frozen || !device.live || rd.production_finalization_started.load(std::memory_order_acquire)) {
		return finish(ERR_BUSY);
	}
	if (!rd.driver) {
		return finish(ERR_UNAVAILABLE);
	}
	if (request.revision > UINT64_MAX - uint64_t(request.capacity.graph_memberships) * 2 - uint64_t(request.capacity.native_memberships) * 2 - request.capacity.readbacks - request.capacity.resources - 8) {
		return finish(ERR_PARAMETER_RANGE_ERROR);
	}
	if (rd.render_thread_id != Thread::get_caller_id() || !rd._thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	RdUnlock unlock{ rd._thread_safe_ };
	if (const Error error = rd._recording_error(); error != OK) {
		return finish(error);
	}
	RID rid;
	if (kind == ProductionControlKind::STORAGE_CREATE) {
		const auto &origin = slot.create_origin;
		production_native_effect_enter_locked(request);
		++request.capture_inflight;
		mutex.unlock();
		rid = rd._qualification_storage_create(uint32_t(origin.bytes), &slot.owned_physical);
		mutex.lock();
		CRASH_COND(!request.capture_inflight || slot.request != &request);
		--request.capture_inflight;
		if (rid.is_null()) {
			slot.owned_backing = { ABI, { instance, slot.owned_serial }, 0, 0, origin.bytes, 0, FAILED, 0 };
			return finish(FAILED);
		}
		const auto *buffer = rd.storage_buffer_owner.get_or_null(rid);
		CRASH_COND(!buffer || buffer->qualification_return != &slot.owned_physical);
		slot.backing_serial = buffer->backing_serial;
		slot.owned_backing = { ABI, { instance, slot.owned_serial }, rid.get_id(), buffer->backing_serial, buffer->size, uint64_t(buffer->usage), OK, 1 };
	} else {
		if (kind != ProductionControlKind::SET_CREATE || slot.imported_unit_slot >= request.resource_count || slot.first_binding > request.capacity.resource_uses || slot.binding_count > request.capacity.resource_uses - slot.first_binding) {
			return finish(ERR_INVALID_DATA);
		}
		const auto &unit = request.resources[slot.imported_unit_slot];
		const auto *shader = rd.shader_owner.get_or_null(unit.secondary);
		if (!unit.imported || unit.use_count != 2 || !shader || slot.shader_set >= uint32_t(shader->uniform_sets.size()) || shader->uniform_sets[slot.shader_set].size() != slot.binding_count) {
			return finish(ERR_INVALID_DATA);
		}
		LocalVector<RenderingDevice::Uniform> uniforms;
		LocalVector<QualificationPhysicalReturn *> dependencies;
		uniforms.resize(slot.binding_count);
		dependencies.reserve(slot.binding_count + 1);
		dependencies.push_back(unit.uses[0].physical);
		for (uint32_t i = 0; i < slot.binding_count; ++i) {
			const auto &binding = request.uses[slot.first_binding + i].binding;
			const auto &resource = request.resources[binding.resource_slot];
			if (!resource.imported || resource.use_count != 1 || resource.retiring || !resource.uses[0].same_physical() || !resource.uses[0].linked()) {
				return finish(ERR_INVALID_DATA);
			}
			bool reflected = false;
			for (const auto &uniform : shader->uniform_sets[slot.shader_set]) {
				reflected |= uniform.binding == binding.binding && uniform.type == RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER;
			}
			if (!reflected) {
				return finish(ERR_INVALID_DATA);
			}
			uniforms[i].uniform_type = RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER;
			uniforms[i].binding = binding.binding;
			uniforms[i].append_id(resource.primary);
			if (!dependencies.has(resource.uses[0].physical)) {
				dependencies.push_back(resource.uses[0].physical);
			}
		}
		if (!rd._qualification_reserve_free_event(&slot.owned_physical)) {
			return finish(ERR_PARAMETER_RANGE_ERROR);
		}
		production_native_effect_enter_locked(request);
		++request.capture_inflight;
		mutex.unlock();
		rid = rd._uniform_set_create_qualification(uniforms, unit.secondary, slot.shader_set, false, &slot.owned_physical, &dependencies, nullptr, &request);
		mutex.lock();
		CRASH_COND(!request.capture_inflight);
		--request.capture_inflight;
		if (rid.is_null()) {
			rd._qualification_release_free_event(&slot.owned_physical);
			return finish(ERR_CANT_CREATE);
		}
		slot.backing_serial = slot.owned_physical.serial;
	}
	slot.primary = rid;
	const Error error = slot.uses[0].attach(slot.owned_physical, request, request.serial);
	if (error == OK) {
		slot.use_count = 1;
		slot.imported = true;
	}
	++request.revision;
	return finish(error != OK ? error : request.source_device_lost ? ERR_UNAVAILABLE
																   : OK);
}

Error Registry::production_sets_step_locked(ProductionRequest &request, RenderingDevice &rd, const GDExtensionProductionServiceBudgetV1 &budget, ProductionServiceUsage &usage) {
	if (request.capture_inflight || !budget.controls || !budget.receipts || !budget.passes || OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros) {
		return OK;
	}
	GDExtensionProductionReportV2 report;
	production_report_locked(request, report);
	if (report.possible_obligations || report.known_obligations != report.complete_obligations || request.compute_list_open || request.draw_list_open) {
		return ERR_BUSY;
	}
	const uint32_t visits = MIN(request.resource_count, 64u);
	for (uint32_t n = 0; n < visits; ++n) {
		const uint32_t i = request.retire_cursor % request.resource_count;
		request.retire_cursor = (i + 1) % request.resource_count;
		auto &slot = request.resources[i];
		const bool set = slot.declaration.resource_kind == GDEXTENSION_PRODUCTION_SET_V1;
		const bool storage = slot.declaration.resource_kind == GDEXTENSION_PRODUCTION_STORAGE_V1;
		const bool raster = request.raster_reserved && slot.declaration.resource_kind == GDEXTENSION_PRODUCTION_RASTER_PHYSICAL_V1;
		const bool raster_retire = raster && request.raster_prepare_origin.create_attempt && !request.resources[0].creating && (request.cancelled || request.next_action == request.action_count || request.resources[0].handoff_error != OK);
		if (slot.declaration.ownership != GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1 || !slot.primary.is_valid() || slot.returned ||
				(!set && !storage && !raster) || (raster && !raster_retire) || (storage && !slot.retiring) || (set && !request.cancelled && request.next_action != request.action_count)) {
			continue;
		}
		{
			MutexLock physical(slot.owned_physical.mutex);
			if (storage) {
				auto &state = slot.retire_report;
				state.frame_slot = slot.owned_physical.frame_slot;
				state.frame_epoch = slot.owned_physical.frame_epoch;
				state.event_seq = slot.owned_physical.free_event;
				if (slot.owned_physical.free_event) {
					state.state = state.return_status = 1;
					continue;
				}
			}
		}
		if (usage.passes >= MIN(budget.passes, 64u) || usage.receipts >= MIN(budget.receipts, 128u) || OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros) {
			return ERR_BUSY;
		}
		if (request.revision > UINT64_MAX - uint64_t(request.capacity.graph_memberships) * 2 - uint64_t(request.capacity.native_memberships) * 2 - request.capacity.readbacks - request.capacity.resources - 8) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
		if (slot.owned_physical.dependent_children.load()) {
			continue;
		}
		if (raster && i == 2 && !slot.owned_physical.enqueued && !request.raster_shader_retire_ready) {
			const auto *shader = rd.shader_owner.get_or_null(slot.primary);
			if (!shader || shader->qualification_return != &slot.owned_physical) {
				return ERR_INVALID_DATA;
			}
			++usage.passes;
			++usage.receipts;
			usage.controls = 1;
			if (!rd.driver->shader_retire_ready_nonblocking(shader->driver_id)) {
				continue;
			}
			request.raster_shader_retire_ready = true;
			// Readiness is monotonic after original preparation/recording closes;
			// a one-receipt budget may enqueue the free on its next owner turn.
			if (usage.passes >= MIN(budget.passes, 64u) || usage.receipts >= MIN(budget.receipts, 128u)) {
				return ERR_BUSY;
			}
		}
		if (slot.use_count) {
			const Error error = slot.uses[0].detach(request, request.serial);
			if (error != OK) {
				return error;
			}
			slot.use_count = 0;
		}
		slot.retiring = true;
		slot.owned_physical.use_closed.store(true);
		if (!slot.owned_physical.enqueued) {
			++usage.passes;
			++usage.receipts;
			usage.controls = 1;
			production_native_effect_enter_locked(request);
			++request.capture_inflight;
			mutex.unlock();
			const bool previous = rd.qualification_retiring;
			rd.qualification_retiring = true;
			// The executing cell/capture pin retains this slot while free runs
			// its synchronous callbacks. Do not inherit the service RD lock.
			rd._thread_safe_.unlock();
			rd.free_rid(slot.primary);
			rd._thread_safe_.lock();
			rd.qualification_retiring = previous;
			mutex.lock();
			CRASH_COND(!request.capture_inflight);
			--request.capture_inflight;
			++request.revision;
		}
		const Error maintenance = production_maintenance_locked(request, rd, budget, usage);
		{
			MutexLock physical(slot.owned_physical.mutex);
			if (storage) {
				auto &state = slot.retire_report;
				state.frame_slot = slot.owned_physical.frame_slot;
				state.frame_epoch = slot.owned_physical.frame_epoch;
				state.event_seq = slot.owned_physical.free_event;
				if (slot.owned_physical.free_event) {
					state.state = state.return_status = 1;
				}
				if (maintenance != OK && maintenance != ERR_BUSY) {
					state.native_error = maintenance;
				}
			} else if (slot.owned_physical.free_event) {
				slot.returned = true;
				slot.imported = false;
				request.returns[i].pending = false;
			}
		}
		if (maintenance != OK) {
			return maintenance;
		}
	}
	return OK;
}

Error Registry::production_maintenance_locked(ProductionRequest &request, RenderingDevice &rd, const GDExtensionProductionServiceBudgetV1 &budget, ProductionServiceUsage &usage) {
	if (!budget.controls || usage.passes >= MIN(budget.passes, 64u) || usage.receipts >= MIN(budget.receipts, 128u) || !budget.deadline_micros || OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros || request.capture_inflight || request.source_device_lost || request.revision > UINT64_MAX - uint64_t(request.capacity.graph_memberships) * 2 - uint64_t(request.capacity.native_memberships) * 2 - request.capacity.readbacks - request.capacity.resources - 8) {
		return ERR_BUSY;
	}
	bool closure_drop = false;
	for (uint32_t i = request.use_count + request.binding_used; i < request.capacity.resource_uses; ++i) {
		const auto &cell = request.uses[i];
		closure_drop |= cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP && cell.disposal_group_release;
	}
	// Finish the original same-owner native cursor before another hold-drop
	// step; drop preflight deliberately rejects an active cursor. Otherwise
	// queued physical returns could never reach their original continuation.
	if (closure_drop && !rd.native_disposal_active.load()) {
		RenderingDevice::NativeDisposalBudget slice;
		slice.receipts = MIN(budget.receipts, 128u) - usage.receipts;
		slice.passes = MIN(budget.passes, 64u) - usage.passes;
		slice.deadline_usec = budget.deadline_micros;
		RenderingDevice::NativeDisposalUsage actual;
		const uint64_t request_serial = request.serial, control_serial = request.control.serial;
		auto *const attachment = request.attachment;
		const uint64_t epoch = attachment->epoch;
		production_native_effect_enter_locked(request);
		++request.capture_inflight;
		// Native owner callbacks must inherit neither Registry nor RD locks.
		rd._thread_safe_.unlock();
		mutex.unlock();
		const Error dropped = rd._production_output_drop_step(request, slice, actual);
		// Native capture and factory return can acquire Registry while holding
		// RD. Restore that blocking order; Registry-held RD entry is try-only.
		rd._thread_safe_.lock();
		mutex.lock();
		CRASH_COND(!request.capture_inflight);
		--request.capture_inflight;
		usage.controls = 1;
		usage.passes += actual.passes;
		usage.receipts += actual.polls + actual.frees + actual.issues + actual.retains + actual.releases + actual.recycles;
		if (request.serial != request_serial || request.control.serial != control_serial || request.control.state != ProductionControlState::EXECUTING || request.attachment != attachment || attachment->held_device != &rd || attachment->epoch != epoch || device_records[attachment->device].epoch != epoch || device_records[attachment->device].object != &rd) {
			return ERR_INVALID_DATA;
		}
		if (dropped != OK) {
			return dropped;
		}
		if (usage.passes >= MIN(budget.passes, 64u) || usage.receipts >= MIN(budget.receipts, 128u)) {
			return ERR_BUSY;
		}
	}
	if (!rd.driver || rd.frames.is_empty() || rd.draw_list.active || rd.compute_list.active || rd.raytracing_list.active || rd.frame_wait_draining || rd.draw_graph.has_driver_callbacks()) {
		return ERR_BUSY;
	}
	if (request.maintenance_serial && (!rd.native_disposal_active || request.maintenance_serial != rd.native_disposal_continuation.serial)) {
		request.maintenance_serial = 0;
	}
	if (rd.native_disposal_active && !request.maintenance_serial) {
		return ERR_BUSY; // Never adopt another request's or qualification lane's cursor.
	}
	// Every graph command must be covered by an actual original production
	// capture. Other renderer/qualification work waits for its original lane;
	// this owner service cannot turn that work into a production receipt.
	for (uint32_t command = 0; command < rd.draw_graph.qualification_command_count(); ++command) {
		bool captured = false;
		for (auto *attachment : production_attachments) {
			if (!attachment || attachment->held_device != &rd) {
				continue;
			}
			for (uint32_t r = 0; r < attachment->quote.capacity.requests && !captured; ++r) {
				const auto &owner = attachment->requests[r];
				if (!owner.serial || (owner.compute_list_open || owner.draw_list_open) || owner.capture_inflight) {
					continue;
				}
				for (uint32_t g = 0; g < owner.graph_count && !captured; ++g) {
					uint32_t actual = UINT32_MAX;
					captured = rd.draw_graph._resolve_record_command_index(owner.graphs[g].handle, actual) && actual == command;
				}
			}
		}
		if (!captured) {
			return ERR_BUSY;
		}
	}
	auto owned_physical = [&](const QualificationPhysicalReturn *physical) {
		if (!physical) {
			return false;
		}
		for (auto *attachment : production_attachments) {
			if (!attachment || attachment->held_device != &rd) {
				continue;
			}
			for (uint32_t r = 0; r < attachment->quote.capacity.requests; ++r) {
				const auto &owner = attachment->requests[r];
				if (!owner.serial) {
					continue;
				}
				for (uint32_t i = 0; i < owner.resource_count; ++i) {
					if (&owner.resources[i].owned_physical == physical) {
						return true;
					}
				}
			}
		}
		return false;
	};
	auto exact_closure = [&](const ProductionDisposalUse *node, uint32_t kind, uint64_t native_driver_id, uint64_t backing = 0) {
		for (; node; node = node->next) {
			if (node->request == &request && node->request_serial == request.serial && node->kind == kind && node->rid && node->serial && node->native_driver_id == native_driver_id && (!backing || node->serial == backing) && node->state.load(std::memory_order_acquire) == 2) {
				return true;
			}
		}
		return false;
	};
	const bool resuming = rd.native_disposal_active && request.maintenance_serial == rd.native_disposal_continuation.serial;
	for (uint32_t frame_index = 0; frame_index < rd.frames.size(); ++frame_index) {
		const auto &frame = rd.frames[frame_index];
		if (!frame.qualification_tickets.is_empty() || !frame.download_texture_get_data_requests.is_empty() || !frame.swap_chains_to_present.is_empty() || (!resuming && (!frame.acceleration_structures_to_dispose_of.is_empty() || !frame.raytracing_pipelines_to_dispose_of.is_empty()))) {
			return ERR_BUSY;
		}
		uint32_t buffers = frame.buffers_to_dispose_of.size();
		uint32_t sets = frame.uniform_sets_to_dispose_of.size();
		if (resuming) {
			buffers = sets = 0;
			if (frame_index == rd.native_disposal_continuation.target && rd.native_disposal_continuation.phase < 5) {
				if (!frame.disposal.active) {
					return ERR_INVALID_DATA;
				}
				for (uint32_t kind = 0; kind < 10; ++kind) {
					if ((kind == 2 || kind == 3) && frame.disposal.remaining[kind]) {
						return ERR_INVALID_DATA;
					}
				}
				buffers = frame.disposal.remaining[9];
				sets = frame.disposal.remaining[4];
			}
		}
		const auto *buffer = frame.buffers_to_dispose_of.front();
		for (uint32_t i = 0; i < buffers; ++i) {
			if (!buffer || (!owned_physical(buffer->get().qualification_return) && !exact_closure(buffer->get().production_disposal_uses, 9, buffer->get().driver_id.id, buffer->get().backing_serial))) {
				return ERR_BUSY;
			}
			buffer = buffer->next();
		}
		const auto *set = frame.uniform_sets_to_dispose_of.front();
		for (uint32_t i = 0; i < sets; ++i) {
			if (!set || (!owned_physical(set->get().qualification_return) && !exact_closure(set->get().production_disposal_uses, 4, set->get().driver_id.id))) {
				return ERR_BUSY;
			}
			set = set->next();
		}
		auto closure_prefix = [&](const auto &list, uint32_t kind) {
			uint32_t count = list.size();
			if (resuming) {
				count = frame_index == rd.native_disposal_continuation.target && rd.native_disposal_continuation.phase < 5 ? frame.disposal.remaining[kind] : 0;
			}
			auto *item = list.front();
			for (uint32_t n = 0; n < count; ++n) {
				if (!item || (!owned_physical(item->get().qualification_return) && !exact_closure(item->get().production_disposal_uses, kind, item->get().driver_id.id))) {
					return false;
				}
				item = item->next();
			}
			return true;
		};
		// Framebuffers have no ProductionDisposalUse list. Admit only the exact
		// preallocated physical cell of a live accepted request on this RD.
		uint32_t framebuffers = frame.framebuffers_to_dispose_of.size();
		if (resuming) {
			framebuffers = frame_index == rd.native_disposal_continuation.target && rd.native_disposal_continuation.phase < 5 ? frame.disposal.remaining[7] : 0;
		}
		auto *framebuffer = frame.framebuffers_to_dispose_of.front();
		for (uint32_t i = 0; i < framebuffers; ++i) {
			if (!framebuffer || !owned_physical(framebuffer->get().qualification_return)) {
				return ERR_BUSY;
			}
			framebuffer = framebuffer->next();
		}
		if (!closure_prefix(frame.render_pipelines_to_dispose_of, 0) || !closure_prefix(frame.compute_pipelines_to_dispose_of, 1) || !closure_prefix(frame.shaders_to_dispose_of, 5) || !closure_prefix(frame.samplers_to_dispose_of, 6) || !closure_prefix(frame.textures_to_dispose_of, 8)) {
			return ERR_BUSY;
		}
		for (const auto &readback : frame.download_buffer_get_data_requests) {
			if (!readback.capture || readback.capture->registry != this) {
				return ERR_BUSY;
			}
		}
	}
	RenderingDevice::NativeDisposalBudget service;
	service.receipts = MIN(budget.receipts, 128u) - usage.receipts;
	service.passes = MIN(budget.passes, 64u) - usage.passes;
	service.deadline_usec = budget.deadline_micros;
	RenderingDevice::NativeDisposalUsage actual;
	usage.controls = 1;
	production_native_effect_enter_locked(request);
	++request.capture_inflight;
	mutex.unlock();
	const bool starting = !rd.native_disposal_active;
	const Error error = rd._native_disposal_maintenance_step(service, actual, &request);
	mutex.lock();
	if (starting && rd.native_disposal_active) {
		request.maintenance_serial = rd.native_disposal_continuation.serial;
	} else if (!rd.native_disposal_active) {
		request.maintenance_serial = 0;
	}
	CRASH_COND(!request.capture_inflight);
	--request.capture_inflight;
	usage.receipts += actual.polls + actual.frees + actual.issues + actual.retains + actual.releases + actual.recycles;
	usage.passes += actual.passes;
	return error;
}
} // namespace GDExtensionDeviceExit
