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
#include "core/io/native_drain_diagnostic.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_collision_scene_rd.h"
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
using RdUnlock = MutexScopeUnlock<Mutex>;
} //namespace

Error Registry::production_create_accept_locked(ProductionRequest &request, ProductionControlKind kind, uint32_t index) {
	if (request.control.state != ProductionControlState::OWNER_CONSUMED || !request.next_control_serial) {
		return ERR_BUSY;
	}
	auto &cell = request.control;
	request.release_ready = {};
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
		Lock lock(*this);
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
		if (kind == ProductionControlKind::SET_CREATE && cell.state == ProductionControlState::PENDING && rd && rd->render_thread_id == Thread::get_caller_id()) {
			auto &mcell = request->control;
			mcell.state = ProductionControlState::EXECUTING;
			const Error exec_err = production_set_create_execute_batch_locked(*request, *rd, index);
			mcell.output.result = result(exec_err);
			mcell.state = ProductionControlState::RETURNED;
		}
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
		Lock lock(*this);
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
	Lock lock(*this);
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
	Lock lock(*this);
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
		bump_resource_permission_epoch();
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
		Lock lock(*this);
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
	Lock lock(*this);
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
	Lock lock(*this);
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

bool Registry::production_persistent_set_build_key_locked(const ProductionRequest &p_request, uint32_t p_unit_index, uint32_t p_shader_set, const GDExtensionProductionSetBindingV1 *p_bindings, uint32_t p_count, PersistentSetKey &r_key) const {
	if (!p_bindings || !p_count || p_unit_index >= p_request.resource_count) {
		return false;
	}
	const auto &unit = p_request.resources[p_unit_index];
	if (unit.declaration.resource_kind != GDEXTENSION_PRODUCTION_UNIT_V1 || !unit.imported || unit.retiring ||
			unit.use_count != 2 || !unit.uses[0].same_physical() || !unit.uses[0].linked() ||
			!unit.uses[0].physical || unit.uses[0].physical->use_closed.load() ||
			unit.secondary.is_null() || !unit.uses[0].physical->serial) {
		return false;
	}
	r_key.shader_rid = unit.secondary.get_id();
	r_key.shader_serial = unit.uses[0].physical->serial;
	r_key.shader_set = p_shader_set;
	r_key.bindings.resize(p_count);
	for (uint32_t i = 0; i < p_count; ++i) {
		const auto &binding = p_bindings[i];
		if (binding.resource_slot >= p_request.resource_count) {
			r_key.bindings.clear();
			return false;
		}
		const auto &resource = p_request.resources[binding.resource_slot];
		if (resource.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 ||
				resource.declaration.ownership != GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1 ||
				!resource.imported || resource.retiring || resource.output_alias_prepared ||
				!resource.output_predecessor_view.is_empty() || resource.output_guard_readonly || resource.output_guard_history ||
				resource.use_count != 1 || !resource.uses[0].same_physical() || !resource.uses[0].linked() ||
				!resource.uses[0].physical || resource.uses[0].physical->use_closed.load() ||
				resource.primary.is_null() || !resource.backing_serial) {
			r_key.bindings.clear();
			return false;
		}
		r_key.bindings[i] = { binding.binding, resource.primary.get_id(), resource.backing_serial };
	}
	return true;
}

bool Registry::production_persistent_set_build_key_from_slot_locked(const ProductionRequest &p_request, const ProductionResourceSlot &p_slot, PersistentSetKey &r_key) const {
	if (p_slot.scene_depth_set1 || p_slot.output_set_deferred || !p_slot.binding_count ||
			p_slot.first_binding > p_request.capacity.resource_uses ||
			p_slot.binding_count > p_request.capacity.resource_uses - p_slot.first_binding ||
			p_slot.imported_unit_slot >= p_request.resource_count) {
		return false;
	}
	const auto &unit = p_request.resources[p_slot.imported_unit_slot];
	if (unit.declaration.resource_kind != GDEXTENSION_PRODUCTION_UNIT_V1 || !unit.imported || unit.retiring ||
			unit.use_count != 2 || !unit.uses[0].same_physical() || !unit.uses[0].linked() ||
			!unit.uses[0].physical || unit.uses[0].physical->use_closed.load() ||
			unit.secondary.is_null() || !unit.uses[0].physical->serial) {
		return false;
	}
	r_key.shader_rid = unit.secondary.get_id();
	r_key.shader_serial = unit.uses[0].physical->serial;
	r_key.shader_set = p_slot.shader_set;
	r_key.bindings.resize(p_slot.binding_count);
	for (uint32_t i = 0; i < p_slot.binding_count; ++i) {
		const auto &binding = p_request.uses[p_slot.first_binding + i].binding;
		if (binding.resource_slot >= p_request.resource_count) {
			r_key.bindings.clear();
			return false;
		}
		const auto &resource = p_request.resources[binding.resource_slot];
		if (resource.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 ||
				resource.declaration.ownership != GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1 ||
				!resource.imported || resource.retiring || resource.output_alias_prepared ||
				!resource.output_predecessor_view.is_empty() || resource.output_guard_readonly || resource.output_guard_history ||
				resource.use_count != 1 || !resource.uses[0].same_physical() || !resource.uses[0].linked() ||
				!resource.uses[0].physical || resource.uses[0].physical->use_closed.load() ||
				resource.primary.is_null() || !resource.backing_serial) {
			r_key.bindings.clear();
			return false;
		}
		r_key.bindings[i] = { binding.binding, resource.primary.get_id(), resource.backing_serial };
	}
	return true;
}

void Registry::production_persistent_set_index_deps_locked(ProductionAttachment &p_attachment, PersistentSetEntry &p_entry) {
	for (uint32_t i = 0; i < p_entry.dependencies.size(); ++i) {
		QualificationPhysicalReturn *dep = p_entry.dependencies[i];
		if (!dep) {
			continue;
		}
		LocalVector<PersistentSetEntry *> &bucket = p_attachment.persistent_sets_by_dep[dep];
		if (!bucket.has(&p_entry)) {
			bucket.push_back(&p_entry);
		}
	}
}

void Registry::production_persistent_set_unindex_deps_locked(ProductionAttachment &p_attachment, PersistentSetEntry &p_entry) {
	for (uint32_t i = 0; i < p_entry.dependencies.size(); ++i) {
		QualificationPhysicalReturn *dep = p_entry.dependencies[i];
		if (!dep) {
			continue;
		}
		LocalVector<PersistentSetEntry *> *bucket = p_attachment.persistent_sets_by_dep.getptr(dep);
		if (!bucket) {
			continue;
		}
		for (uint32_t j = 0; j < bucket->size();) {
			if ((*bucket)[j] == &p_entry) {
				bucket->remove_at_unordered(j);
			} else {
				++j;
			}
		}
		if (bucket->is_empty()) {
			p_attachment.persistent_sets_by_dep.erase(dep);
		}
	}
}

void Registry::production_persistent_sets_evict_lru_locked(ProductionAttachment &p_attachment) {
	PersistentSetEntry *cursor = p_attachment.persistent_set_lru_tail;
	while (p_attachment.persistent_set_lookup.size() > ProductionAttachment::PERSISTENT_SET_MAX_LIVE && cursor != nullptr) {
		PersistentSetEntry *prev = cursor->lru_prev;
		if (!cursor->evicted && cursor->physical.production_uses == nullptr) {
			p_attachment.persistent_set_mark_evicted(cursor);
		}
		cursor = prev;
	}
}

void Registry::production_persistent_sets_reap_and_retire_rt(RenderingDevice *p_rd, bool p_unlock_rd_around_free) {
	if (!p_rd || p_rd->render_thread_id != Thread::get_caller_id()) {
		return;
	}
	const bool can_free = p_rd->driver && !p_rd->frames.is_empty() && p_rd->_recording_error() == OK &&
			!p_rd->draw_list.active && !p_rd->compute_list.active && !p_rd->raytracing_list.active &&
			!p_rd->frame_wait_draining && !p_rd->draw_graph.has_driver_callbacks() &&
			p_rd->frames[p_rd->frame].qualification_epoch != 0;
	LocalVector<RID> to_free;
	{
		Lock lock(*this);
		for (auto *attachment : production_attachments) {
			if (!attachment || attachment->held_device != p_rd || attachment->persistent_sets_pending_reap.is_empty()) {
				continue;
			}
			uint32_t i = 0;
			while (i < attachment->persistent_sets_pending_reap.size()) {
				PersistentSetEntry *entry = attachment->persistent_sets_pending_reap[i];
				if (!entry) {
					attachment->persistent_sets_pending_reap.remove_at_unordered(i);
					if (i < attachment->persistent_sets_pending_reap.size() && attachment->persistent_sets_pending_reap[i]) {
						attachment->persistent_sets_pending_reap[i]->pending_reap_index = i;
					}
					continue;
				}
				++attachment->persistent_set_reap_entries_scanned;
				uint64_t free_event = 0;
				{
					MutexLock phys_lock(entry->physical.mutex);
					free_event = entry->physical.free_event;
				}
				if (entry->evicted && entry->physical.production_uses == nullptr && free_event != 0) {
					attachment->persistent_set_lookup.erase(entry->key);
					production_persistent_set_unindex_deps_locked(*attachment, *entry);
					attachment->persistent_set_remove_owned(entry);
					memdelete(entry);
					continue;
				}
				if (can_free && entry->evicted && !entry->retire_enqueued &&
						entry->physical.production_uses == nullptr &&
						p_rd->_qualification_free_event_available(entry->physical.event_capacity_reserved) &&
						(attachment->closing || to_free.size() < 256u)) {
					entry->physical.use_closed.store(true);
					bump_resource_permission_epoch();
					entry->retire_enqueued = true;
					to_free.push_back(entry->rid);
				}
				++i;
			}
		}
	}
	if (to_free.is_empty()) {
		return;
	}
	const bool previous_retiring = p_rd->qualification_retiring;
	p_rd->qualification_retiring = true;
	if (p_unlock_rd_around_free) {
		p_rd->_thread_safe_.unlock();
	}
	for (uint32_t i = 0; i < to_free.size(); ++i) {
		p_rd->free_rid(to_free[i]);
	}
	if (p_unlock_rd_around_free) {
		p_rd->_thread_safe_.lock();
	}
	p_rd->qualification_retiring = previous_retiring;
	{
		Lock lock(*this);
		for (auto *attachment : production_attachments) {
			if (!attachment || attachment->held_device != p_rd || attachment->persistent_sets_pending_reap.is_empty()) {
				continue;
			}
			uint32_t i = 0;
			while (i < attachment->persistent_sets_pending_reap.size()) {
				PersistentSetEntry *entry = attachment->persistent_sets_pending_reap[i];
				if (!entry) {
					attachment->persistent_sets_pending_reap.remove_at_unordered(i);
					if (i < attachment->persistent_sets_pending_reap.size() && attachment->persistent_sets_pending_reap[i]) {
						attachment->persistent_sets_pending_reap[i]->pending_reap_index = i;
					}
					continue;
				}
				++attachment->persistent_set_reap_entries_scanned;
				bool enqueued = false;
				uint64_t free_event = 0;
				{
					MutexLock phys_lock(entry->physical.mutex);
					enqueued = entry->physical.enqueued;
					free_event = entry->physical.free_event;
				}
				if (entry->evicted && entry->physical.production_uses == nullptr && free_event != 0) {
					attachment->persistent_set_lookup.erase(entry->key);
					production_persistent_set_unindex_deps_locked(*attachment, *entry);
					attachment->persistent_set_remove_owned(entry);
					memdelete(entry);
					continue;
				}
				if (entry->retire_enqueued && !enqueued && free_event == 0) {
					entry->retire_enqueued = false;
				}
				++i;
			}
		}
	}
}

void Registry::production_persistent_sets_evict_for_dependency_rt(RenderingDevice *p_rd, QualificationPhysicalReturn *p_dep) {
	if (!p_rd || !p_dep || p_rd->render_thread_id != Thread::get_caller_id()) {
		return;
	}
	bool any_evicted = false;
	{
		Lock lock(*this);
		for (auto *attachment : production_attachments) {
			if (!attachment || attachment->held_device != p_rd || attachment->persistent_sets_by_dep.is_empty()) {
				continue;
			}
			LocalVector<PersistentSetEntry *> *bucket = attachment->persistent_sets_by_dep.getptr(p_dep);
			if (!bucket || bucket->is_empty()) {
				continue;
			}
			for (uint32_t i = 0; i < bucket->size(); ++i) {
				PersistentSetEntry *entry = (*bucket)[i];
				if (!entry) {
					continue;
				}
				if (!entry->evicted) {
					attachment->persistent_set_mark_evicted(entry);
				}
				any_evicted = true;
			}
		}
	}
	if (any_evicted) {
		production_persistent_sets_reap_and_retire_rt(p_rd, true);
	}
}

void Registry::production_persistent_sets_close_drain_rt(uint64_t p_registry_instance, uint64_t p_attachment_serial) {
	if (p_registry_instance != instance) {
		return;
	}
	RenderingDevice *rd = nullptr;
	{
		Lock lock(*this);
		for (auto *attachment : production_attachments) {
			if (attachment && attachment->serial == p_attachment_serial) {
				attachment->persistent_set_close_drain_queued = false;
				rd = attachment->held_device;
				break;
			}
		}
	}
	if (!rd || rd->render_thread_id != Thread::get_caller_id() || !rd->_thread_safe_.try_lock()) {
		return;
	}
	MutexScopeUnlock<Mutex> unlock(rd->_thread_safe_);
	production_persistent_sets_reap_and_retire_rt(rd, true);
}

GDExtensionDeviceSessionResultV1 Registry::production_set_create(GDExtensionProductionRequestRefV1 ref, uint32_t index, uint32_t unit_index, uint32_t shader_set, const GDExtensionProductionSetBindingV1 *bindings, uint32_t count) {
	if (!count || !valid(bindings)) {
		return result(ERR_INVALID_PARAMETER);
	}
	uint64_t schedule_leader_serial = 0;
	uint64_t drain_returned_serial = 0;
	{
		Lock lock(*this);
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
		auto has_earlier_creating_set = [&]() {
			for (uint32_t j = 0; j < index; ++j) {
				const auto &prior = request->resources[j];
				if (prior.declaration.resource_kind == GDEXTENSION_PRODUCTION_SET_V1 && prior.creating && !prior.primary.is_valid()) {
					return true;
				}
			}
			return false;
		};
		const bool mismatched_returned_set_create =
				request->control.state == ProductionControlState::RETURNED &&
				request->control.input.kind == ProductionControlKind::SET_CREATE &&
				request->control.input.resource_slot != index;
		const bool can_batch_with_leader =
				(request->control.state == ProductionControlState::PENDING || request->control.state == ProductionControlState::QUEUED) &&
				request->control.input.kind == ProductionControlKind::SET_CREATE &&
				rd && rd->render_thread_id != Thread::get_caller_id();
		if (slot.owned_serial) {
			if (slot.imported_unit_slot != unit_index || slot.shader_set != shader_set || slot.binding_count != count) {
				return result(ERR_INVALID_DATA);
			}
			for (uint32_t i = 0; i < count; ++i) {
				if (std::memcmp(&request->uses[slot.first_binding + i].binding, &bindings[i], sizeof(bindings[i]))) {
					return result(ERR_INVALID_DATA);
				}
			}
			if (slot.output_set_deferred) {
				return result(OK);
			}
			const bool is_active_leader =
					request->control.state != ProductionControlState::OWNER_CONSUMED &&
					request->control.input.kind == ProductionControlKind::SET_CREATE &&
					request->control.input.resource_slot == index;
			if (!is_active_leader) {
				if (!slot.creating && !slot.batched_set_create) {
					return result(slot.handoff_error);
				}
				// BUG-77A: If another SET_CREATE leader finished in RETURNED while
				// this slot is still creating, consume that completed control cell
				// back to OWNER_CONSUMED so neither retry nor production_cancel can wedge.
				if (mismatched_returned_set_create) {
					drain_returned_serial = request->control.serial;
				} else if (slot.batched_set_create && slot.creating) {
					if (request->control.state == ProductionControlState::OWNER_CONSUMED) {
						slot.batched_set_create = false;
						if (has_earlier_creating_set()) {
							return result(ERR_BUSY);
						}
					} else if (request->control.state == ProductionControlState::PENDING && request->control.input.kind == ProductionControlKind::SET_CREATE) {
						schedule_leader_serial = request->control.serial;
					} else {
						return result(ERR_BUSY);
					}
				} else if (slot.creating && can_batch_with_leader) {
					ProductionAttachment *checked = nullptr;
					if ((error = production_attachment_locked({ instance, request->attachment->serial }, true, checked, rd)) != OK) {
						slot.creating = false;
						slot.handoff_error = error;
						request->returns[index].pending = false;
						return result(error);
					}
					if (request->cancelled || request->release_started || (request->native_static_sealed && !request->frozen) || request->continuation_inflight) {
						slot.creating = false;
						slot.handoff_error = ERR_BUSY;
						request->returns[index].pending = false;
						return result(ERR_BUSY);
					}
					slot.batched_set_create = true;
					if (request->control.state == ProductionControlState::PENDING) {
						schedule_leader_serial = request->control.serial;
					} else {
						return result(ERR_BUSY);
					}
				} else if (slot.creating && has_earlier_creating_set()) {
					// BUG-77A: Never let a later follower hijack SET_CREATE leadership
					// while an earlier slot is still creating (e.g. right after the
					// leader consumed a transient ERR_BUSY control back to OWNER_CONSUMED).
					return result(ERR_BUSY);
				}
			}
		} else {
			if (mismatched_returned_set_create) {
				drain_returned_serial = request->control.serial;
			} else if (can_batch_with_leader) {
				ProductionAttachment *checked = nullptr;
				if ((error = production_attachment_locked({ instance, request->attachment->serial }, true, checked, rd)) != OK) {
					return result(error);
				}
				if (request->cancelled || request->release_started || (request->native_static_sealed && !request->frozen) || request->continuation_inflight) {
					return result(ERR_BUSY);
				}
			} else {
				if ((error = production_request_locked(ref, true, request, rd)) != OK) {
					return result(error);
				}
				// BUG-77A: Do not promote a later follower to active SET_CREATE leader
				// while an earlier SET_V1 slot is still creating.
				if (has_earlier_creating_set()) {
					return result(ERR_BUSY);
				}
			}
			if (!drain_returned_serial) {
				if (request->frozen || request->capture_inflight || request->binding_used > request->capacity.resource_uses - request->use_count || count > request->capacity.resource_uses - request->use_count - request->binding_used || request->revision > UINT64_MAX - uint64_t(request->capacity.graph_memberships) * 2 - uint64_t(request->capacity.native_memberships) * 2 - request->capacity.readbacks - request->capacity.resources - 8 || production_serials.exhausted || production_serials.next > UINT64_MAX - 2 || !request->next_control_serial) {
					return result(ERR_INVALID_DATA);
				}
				// Copy the complete finite binding origin before queue publication.
				// Native shader reflection is checked on RT before the first effect.
				// Set 1 may combine leading scene-depth sentinel pairs (0xFFFFFFFE)
				// with trailing GDEXTENSION_PRODUCTION_STORAGE_V1 SSBOs (e.g. neighbor_grid).
				const bool scene_depth_set1 = (shader_set == 1u && bindings[0].resource_slot == SCENE_DEPTH_SNAPSHOT_PAIR_RESOURCE_SLOT);
				if (scene_depth_set1) {
					uint32_t sd_pair_count = 0;
					while (sd_pair_count < count && bindings[sd_pair_count].resource_slot == SCENE_DEPTH_SNAPSHOT_PAIR_RESOURCE_SLOT) {
						++sd_pair_count;
					}
					if (sd_pair_count < 1u || sd_pair_count > MAX_SCENE_DEPTH_SET1_PAIRS) {
						return result(ERR_INVALID_DATA);
					}
					for (uint32_t i = 0; i < sd_pair_count; ++i) {
						const auto &binding = bindings[i];
						float slab_m = 0.0f;
						float jump_m = 0.0f;
						if (!production_dto_valid(binding) || binding.set != 1u || binding.binding != 2u * i ||
								binding.array_element > 1u || binding.resource_slot != SCENE_DEPTH_SNAPSHOT_PAIR_RESOURCE_SLOT ||
								!decode_scene_depth_pair_params(binding.bytes, slab_m, jump_m)) {
							return result(ERR_INVALID_DATA);
						}
					}
					for (uint32_t i = sd_pair_count; i < count; ++i) {
						const auto &binding = bindings[i];
						if (!production_dto_valid(binding) || binding.set != 1u || binding.array_element || binding.offset ||
								binding.resource_slot == SCENE_DEPTH_SNAPSHOT_PAIR_RESOURCE_SLOT ||
								binding.resource_slot >= request->resource_count ||
								binding.binding < 2u * sd_pair_count ||
								(i > sd_pair_count && bindings[i - 1].binding >= binding.binding)) {
							return result(ERR_INVALID_DATA);
						}
						const auto &resource = request->resources[binding.resource_slot];
						const bool original_physical = resource.imported && ((resource.use_count == 1 && resource.uses[0].same_physical() && resource.uses[0].linked()) || !resource.output_predecessor_view.is_empty() || (resource.output_guard_readonly && resource.output_guard_history));
						if (!original_physical || resource.output_alias_prepared || resource.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || resource.retiring || binding.bytes != resource.declaration.physical_bytes) {
							return result(ERR_INVALID_DATA);
						}
					}
				} else {
					for (uint32_t i = 0; i < count; ++i) {
						const auto &binding = bindings[i];
						if (!production_dto_valid(binding) || binding.set != shader_set || binding.array_element || binding.offset || binding.resource_slot >= request->resource_count || (i && bindings[i - 1].binding >= binding.binding)) {
							return result(ERR_INVALID_DATA);
						}
						const auto &resource = request->resources[binding.resource_slot];
						const bool original_physical = resource.imported && ((resource.use_count == 1 && resource.uses[0].same_physical() && resource.uses[0].linked()) || !resource.output_predecessor_view.is_empty() || (resource.output_guard_readonly && resource.output_guard_history));
						const bool deferred_alias = resource.output_alias_prepared && !resource.imported && resource.output_action_ordinal < request->action_count && request->actions[resource.output_action_ordinal].prepared;
						if ((!original_physical && !deferred_alias) || resource.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || resource.retiring || binding.bytes != resource.declaration.physical_bytes) {
							return result(ERR_INVALID_DATA);
						}
					}
				}
				PersistentSetKey cache_key;
				bool cache_eligible = false;
				if (!scene_depth_set1) {
					bool has_output_alias = false;
					for (uint32_t i = 0; i < count; ++i) {
						has_output_alias |= request->resources[bindings[i].resource_slot].output_alias_prepared;
					}
					if (!has_output_alias && !request->attachment->closing &&
							production_persistent_set_build_key_locked(*request, unit_index, shader_set, bindings, count, cache_key)) {
						cache_eligible = true;
						PersistentSetEntry **found = request->attachment->persistent_set_lookup.getptr(cache_key);
						if (found && *found && !(*found)->evicted && !(*found)->physical.use_closed.load()) {
							PersistentSetEntry *entry = *found;
							const Error attach_err = slot.uses[0].attach(entry->physical, *request, request->serial);
							if (attach_err == OK) {
								slot.owned_serial = entry->serial;
								slot.primary = entry->rid;
								slot.backing_serial = entry->physical.serial;
								slot.imported_unit_slot = unit_index;
								slot.shader_set = shader_set;
								slot.scene_depth_set1 = false;
								slot.first_binding = request->use_count + request->binding_used;
								slot.binding_count = count;
								for (uint32_t i = 0; i < count; ++i) {
									request->uses[slot.first_binding + i].binding = bindings[i];
								}
								request->binding_used += count;
								request->note_use_touched_end(request->use_count + request->binding_used);
								slot.use_count = 1;
								slot.imported = true;
								slot.cached_persistent_set = true;
								slot.creating = false;
								slot.handoff_error = OK;
								request->returns[index].pending = false;
								request->returns[index].serial = 0;
								entry->last_used_tick = ++request->attachment->persistent_set_tick;
								request->attachment->persistent_set_lru_touch(entry);
								return result(OK);
							}
						}
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
				slot.scene_depth_set1 = scene_depth_set1;
				slot.cached_persistent_set = cache_eligible;
				for (uint32_t i = 0; i < count; ++i) {
					request->uses[slot.first_binding + i].binding = bindings[i];
				}
				request->binding_used += count;
				request->note_use_touched_end(request->use_count + request->binding_used);
				if (!scene_depth_set1) {
					for (uint32_t i = 0; i < count; ++i) {
						slot.output_set_deferred |= request->resources[bindings[i].resource_slot].output_alias_prepared;
					}
				}
				if (slot.output_set_deferred) {
					slot.cached_persistent_set = false;
					// Preparation only. The original output action creates this exact
					// set after its actual aliases exist, inside the same RECORD.
					return result(OK);
				}
				slot.creating = true;
				request->returns[index].pending = true;
				request->returns[index].serial = slot.retire_serial;
				if (can_batch_with_leader) {
					slot.batched_set_create = true;
					if (request->control.state == ProductionControlState::PENDING) {
						schedule_leader_serial = request->control.serial;
					} else {
						return result(ERR_BUSY);
					}
				} else {
					error = production_create_accept_locked(*request, ProductionControlKind::SET_CREATE, index);
					CRASH_COND(error != OK);
				}
			}
		}
	}
	if (drain_returned_serial) {
		ProductionControlOutput drained = {};
		(void)production_control_consume(ref, drain_returned_serial, &drained);
		return result(ERR_BUSY);
	}
	if (schedule_leader_serial) {
		const Error scheduled = production_control_schedule(ref, schedule_leader_serial);
		return result(scheduled == OK ? ERR_BUSY : scheduled);
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
	uint64_t *output_serials[4] = { &out.framebuffer_serial, &out.depth_serial, &out.shader_serial, &out.pipeline_serial };
	for (uint32_t i = 0; i < 4; ++i) {
		const auto &slot = request.resources[i];
		*output_serials[i] = slot.backing_serial;
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
	Lock lock(*this);
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
		Lock lock(*this);
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
		unlock_mutex_for_call();
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
		relock_mutex_after_call();
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
	bool output_creation = kind == ProductionControlKind::SET_CREATE && slot.output_set_deferred && request.frozen && request.control.state == ProductionControlState::EXECUTING && request.control.input.kind == ProductionControlKind::RECORD && request.next_action < request.action_count;
	if (output_creation) {
		const auto &action = request.actions[request.next_action];
		output_creation = action.request == &request && action.output_source_prepared && action.output_state == ProductionActionCell::OutputState::RESERVING && action.output_root;
	}
	// Queuing preparation is not a grant to create after NewWork was sealed.
	if (attachment.closing || !owner.attached || owner.quiescing || owner.close_started || owner.close_error != OK || owner.guard_enrollment != GuardEnrollment::QUALIFIED || request.cancelled || request.release_started || (request.frozen && !output_creation) || !device.live || rd.production_finalization_started.load(std::memory_order_acquire)) {
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
		// No native creation has begun. Keep this same resource eligible after
		// its original returned control is consumed and the frame gate clears.
		return error == ERR_BUSY ? error : finish(error);
	}
	RID rid;
	if (kind == ProductionControlKind::STORAGE_CREATE) {
		const auto &origin = slot.create_origin;
		production_native_effect_enter_locked(request);
		++request.capture_inflight;
		unlock_mutex_for_call();
		rid = rd._qualification_storage_create(uint32_t(origin.bytes), &slot.owned_physical);
		relock_mutex_after_call();
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
		if (slot.scene_depth_set1) {
			uint32_t pair_count = 0;
			while (pair_count < slot.binding_count &&
					request.uses[slot.first_binding + pair_count].binding.resource_slot == SCENE_DEPTH_SNAPSHOT_PAIR_RESOURCE_SLOT) {
				++pair_count;
			}
			const uint32_t storage_count = slot.binding_count - pair_count;
			if (!unit.imported || unit.use_count != 2 || !shader || slot.shader_set != 1u ||
					pair_count < 1u || pair_count > MAX_SCENE_DEPTH_SET1_PAIRS ||
					uint32_t(shader->uniform_sets.size()) <= 1u ||
					uint32_t(shader->uniform_sets[1].size()) != 2u * pair_count + storage_count) {
				return finish(ERR_INVALID_DATA);
			}
			struct SceneDepthPairSpec {
				uint64_t viewport_rid = 0ull;
				float slab_m = 0.50f;
				float jump_m = 0.50f;
			};
			SceneDepthPairSpec specs[MAX_SCENE_DEPTH_SET1_PAIRS];
			for (uint32_t d = 0; d < pair_count; ++d) {
				const auto &binding = request.uses[slot.first_binding + d].binding;
				if (!production_dto_valid(binding) || binding.set != 1u || binding.binding != 2u * d ||
						binding.array_element > 1u || binding.resource_slot != SCENE_DEPTH_SNAPSHOT_PAIR_RESOURCE_SLOT ||
						!decode_scene_depth_pair_params(binding.bytes, specs[d].slab_m, specs[d].jump_m)) {
					return finish(ERR_INVALID_DATA);
				}
				specs[d].viewport_rid = binding.offset;
				uint32_t meta_matches = 0;
				uint32_t tex_matches = 0;
				for (const auto &uniform : shader->uniform_sets[1]) {
					if (uniform.binding == 2u * d + 0u) {
						if (uniform.type != RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER || uniform.writable ||
								(uniform.length > 0 && uint32_t(uniform.length) != sizeof(RendererRD::SceneDepthSnapshotMeta256))) {
							return finish(ERR_INVALID_DATA);
						}
						++meta_matches;
					} else if (uniform.binding == 2u * d + 1u) {
						if (uniform.type != RenderingDevice::UNIFORM_TYPE_TEXTURE || uniform.writable || uniform.length != 1) {
							return finish(ERR_INVALID_DATA);
						}
						++tex_matches;
					}
				}
				if (meta_matches != 1u || tex_matches != 1u) {
					return finish(ERR_INVALID_DATA);
				}
			}
			LocalVector<RenderingDevice::Uniform> trailing_uniforms;
			LocalVector<QualificationPhysicalReturn *> dependencies;
			trailing_uniforms.resize(storage_count);
			dependencies.reserve(storage_count + 1);
			dependencies.push_back(unit.uses[0].physical);
			for (uint32_t s = 0; s < storage_count; ++s) {
				const auto &binding = request.uses[slot.first_binding + pair_count + s].binding;
				if (!production_dto_valid(binding) || binding.set != 1u || binding.array_element || binding.offset ||
						binding.resource_slot == SCENE_DEPTH_SNAPSHOT_PAIR_RESOURCE_SLOT ||
						binding.resource_slot >= request.resource_count ||
						binding.binding < 2u * pair_count ||
						(s > 0 && request.uses[slot.first_binding + pair_count + s - 1].binding.binding >= binding.binding)) {
					return finish(ERR_INVALID_DATA);
				}
				const auto &resource = request.resources[binding.resource_slot];
				const bool predecessor = production_output_predecessor_matches_locked(request, resource, rd) ||
						(resource.output_guard_history && production_output_guard_matches_locked(request, resource, rd));
				if (!resource.imported || resource.retiring || resource.output_alias_prepared ||
						(!predecessor && (resource.use_count != 1 || !resource.uses[0].same_physical() || !resource.uses[0].linked()))) {
					return finish(ERR_INVALID_DATA);
				}
				bool reflected = false;
				for (const auto &uniform : shader->uniform_sets[1]) {
					reflected |= uniform.binding == binding.binding && uniform.type == RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER && (!predecessor || !uniform.writable);
				}
				if (!reflected) {
					return finish(ERR_INVALID_DATA);
				}
				trailing_uniforms[s].uniform_type = RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER;
				trailing_uniforms[s].binding = binding.binding;
				trailing_uniforms[s].append_id(resource.primary);
				if (!predecessor && !dependencies.has(resource.uses[0].physical)) {
					dependencies.push_back(resource.uses[0].physical);
				}
			}
			if (!rd._qualification_reserve_free_event(&slot.owned_physical)) {
				return finish(ERR_PARAMETER_RANGE_ERROR);
			}
			const RID unit_shader_rid = unit.secondary;

			Ref<RendererRD::ParticlesCollisionSceneRD::DepthReadLease> acquired_leases[MAX_SCENE_DEPTH_SET1_PAIRS];
			Error acq_err = OK;
			production_native_effect_enter_locked(request);
			++request.capture_inflight;
			rd._thread_safe_.unlock();
			unlock_mutex_for_call();
			{
				RendererRD::ParticlesCollisionSceneRD *collision_scene = RendererRD::ParticlesCollisionSceneRD::get_for_device(&rd);
				if (!collision_scene || !collision_scene->is_initialized()) {
					acq_err = ERR_UNAVAILABLE;
				} else {
					LocalVector<RenderingDevice::Uniform> uniforms;
					uniforms.resize(2u * pair_count + storage_count);
					for (uint32_t d = 0; d < pair_count; ++d) {
						GDExtensionSceneOpaque32V1 sample_token = {};
						acq_err = collision_scene->acquire_depth_snapshot_read_lease(
								RID::from_uint64(specs[d].viewport_rid),
								specs[d].slab_m,
								specs[d].jump_m,
								acquired_leases[d],
								sample_token);
						if (acq_err != OK || acquired_leases[d].is_null()) {
							if (acq_err == OK) {
								acq_err = ERR_CANT_CREATE;
							}
							break;
						}
						const Ref<RendererRD::ParticlesCollisionSceneRD::DepthSnapshotGeneration> gen = acquired_leases[d]->get_generation();
						if (gen.is_null() || gen->meta_buffer.is_null() || gen->depth_texture.is_null()) {
							acq_err = ERR_CANT_CREATE;
							break;
						}
						uniforms[2u * d + 0u].uniform_type = RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER;
						uniforms[2u * d + 0u].binding = 2u * d + 0u;
						uniforms[2u * d + 0u].append_id(gen->meta_buffer);
						uniforms[2u * d + 1u].uniform_type = RenderingDevice::UNIFORM_TYPE_TEXTURE;
						uniforms[2u * d + 1u].binding = 2u * d + 1u;
						uniforms[2u * d + 1u].append_id(gen->depth_texture);
					}
					for (uint32_t s = 0; s < storage_count; ++s) {
						uniforms[2u * pair_count + s] = trailing_uniforms[s];
					}
					if (acq_err == OK) {
						rid = rd._uniform_set_create_qualification(uniforms, unit_shader_rid, 1u, false, &slot.owned_physical, &dependencies, nullptr, &request);
						if (rid.is_null()) {
							acq_err = rd.get_last_uniform_set_create_error() == ERR_BUSY ? ERR_BUSY : ERR_CANT_CREATE;
						}
					}
					if (acq_err != OK) {
						for (uint32_t d = 0; d < pair_count; ++d) {
							if (acquired_leases[d].is_valid()) {
								(void)collision_scene->release_depth_snapshot_read_lease(acquired_leases[d]);
							}
						}
					}
				}
			}
			rd._thread_safe_.lock();
			relock_mutex_after_call();
			CRASH_COND(!request.capture_inflight || slot.request != &request);
			--request.capture_inflight;
			if (acq_err != OK || rid.is_null()) {
				rd._qualification_release_free_event(&slot.owned_physical);
				const Error fail_err = acq_err != OK ? acq_err : ERR_CANT_CREATE;
				return fail_err == ERR_BUSY ? ERR_BUSY : finish(fail_err);
			}
			slot.backing_serial = slot.owned_physical.serial;
			slot.scene_depth_lease_count = pair_count;
			for (uint32_t d = 0; d < pair_count; ++d) {
				slot.scene_depth_leases[d] = acquired_leases[d];
			}
		} else {
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
				const bool exact_alias = resource.output_alias_prepared && production_output_alias_matches_locked(request, resource, rd);
				const bool predecessor = production_output_predecessor_matches_locked(request, resource, rd) || (resource.output_guard_history && production_output_guard_matches_locked(request, resource, rd));
				if (!resource.imported || resource.retiring || (!exact_alias && !predecessor && (resource.use_count != 1 || !resource.uses[0].same_physical() || !resource.uses[0].linked()))) {
					return finish(ERR_INVALID_DATA);
				}
				bool reflected = false;
				for (const auto &uniform : shader->uniform_sets[slot.shader_set]) {
					reflected |= uniform.binding == binding.binding && uniform.type == RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER && (!predecessor || !uniform.writable);
				}
				if (!reflected) {
					return finish(ERR_INVALID_DATA);
				}
				uniforms[i].uniform_type = RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER;
				uniforms[i].binding = binding.binding;
				uniforms[i].append_id(resource.primary);
				if (!exact_alias && !predecessor && !dependencies.has(resource.uses[0].physical)) {
					dependencies.push_back(resource.uses[0].physical);
				}
			}
			if (slot.cached_persistent_set) {
				PersistentSetKey cache_key;
				if (production_persistent_set_build_key_from_slot_locked(request, slot, cache_key)) {
					PersistentSetEntry **existing = request.attachment->persistent_set_lookup.getptr(cache_key);
					if (existing && *existing && !(*existing)->evicted && !(*existing)->physical.use_closed.load()) {
						PersistentSetEntry *entry = *existing;
						slot.owned_serial = entry->serial;
						slot.primary = entry->rid;
						slot.backing_serial = entry->physical.serial;
						const Error attach_err = slot.uses[0].attach(entry->physical, request, request.serial);
						if (attach_err == OK) {
							slot.use_count = 1;
							slot.imported = true;
							request.returns[index].pending = false;
							entry->last_used_tick = ++request.attachment->persistent_set_tick;
							request.attachment->persistent_set_lru_touch(entry);
						}
						++request.revision;
						return finish(attach_err != OK ? attach_err : request.source_device_lost ? ERR_UNAVAILABLE
																								 : OK);
					}
					if (production_serials.exhausted || production_serials.next > UINT64_MAX - 2) {
						return finish(ERR_PARAMETER_RANGE_ERROR);
					}
					PersistentSetEntry *entry = memnew(PersistentSetEntry);
					production_serials.take(entry->serial);
					production_serials.take(entry->physical.serial);
					entry->physical.bridge = request.attachment->serial;
					entry->physical.child = entry->serial;
					entry->key = cache_key;
					entry->dependencies = dependencies;
					if (!rd._qualification_reserve_free_event(&entry->physical)) {
						memdelete(entry);
						return finish(ERR_PARAMETER_RANGE_ERROR);
					}
					production_native_effect_enter_locked(request);
					++request.capture_inflight;
					unlock_mutex_for_call();
					rid = rd._uniform_set_create_qualification(uniforms, unit.secondary, slot.shader_set, false, &entry->physical, &dependencies, nullptr, &request);
					relock_mutex_after_call();
					CRASH_COND(!request.capture_inflight);
					--request.capture_inflight;
					if (rid.is_null()) {
						rd._qualification_release_free_event(&entry->physical);
						memdelete(entry);
						const Error fail_err = rd.get_last_uniform_set_create_error() == ERR_BUSY ? ERR_BUSY : ERR_CANT_CREATE;
						return fail_err == ERR_BUSY ? ERR_BUSY : finish(fail_err);
					}
					entry->rid = rid;
					entry->physical.rid = rid.get_id();
					entry->last_used_tick = ++request.attachment->persistent_set_tick;
					request.attachment->persistent_set_push_live(entry);
					production_persistent_set_index_deps_locked(*request.attachment, *entry);
					if (!request.attachment->closing && !request.attachment->persistent_set_lookup.has(entry->key)) {
						request.attachment->persistent_set_lookup.insert(entry->key, entry);
						request.attachment->persistent_set_lru_touch(entry);
						production_persistent_sets_evict_lru_locked(*request.attachment);
					} else {
						request.attachment->persistent_set_mark_evicted(entry);
					}
					slot.owned_serial = entry->serial;
					slot.primary = entry->rid;
					slot.backing_serial = entry->physical.serial;
					const Error attach_err = slot.uses[0].attach(entry->physical, request, request.serial);
					if (attach_err == OK) {
						slot.use_count = 1;
						slot.imported = true;
						request.returns[index].pending = false;
					}
					++request.revision;
					return finish(attach_err != OK ? attach_err : request.source_device_lost ? ERR_UNAVAILABLE
																							 : OK);
				}
				slot.cached_persistent_set = false;
			}
			if (!rd._qualification_reserve_free_event(&slot.owned_physical)) {
				return finish(ERR_PARAMETER_RANGE_ERROR);
			}
			production_native_effect_enter_locked(request);
			++request.capture_inflight;
			unlock_mutex_for_call();
			rid = rd._uniform_set_create_qualification(uniforms, unit.secondary, slot.shader_set, false, &slot.owned_physical, &dependencies, nullptr, &request);
			relock_mutex_after_call();
			CRASH_COND(!request.capture_inflight);
			--request.capture_inflight;
			if (rid.is_null()) {
				rd._qualification_release_free_event(&slot.owned_physical);
				const Error fail_err = rd.get_last_uniform_set_create_error() == ERR_BUSY ? ERR_BUSY : ERR_CANT_CREATE;
				return fail_err == ERR_BUSY ? ERR_BUSY : finish(fail_err);
			}
			slot.backing_serial = slot.owned_physical.serial;
		}
	}
	slot.primary = rid;
	const Error error = slot.uses[0].attach(slot.owned_physical, request, request.serial);
	if (error == OK) {
		slot.use_count = 1;
		slot.imported = true;
		bump_resource_permission_epoch();
	}
	++request.revision;
	return finish(error != OK ? error : request.source_device_lost ? ERR_UNAVAILABLE
																   : OK);
}

void Registry::production_set_create_cancel_batched_locked(ProductionRequest &request, Error p_error) {
	for (uint32_t i = 0; i < request.resource_count; ++i) {
		auto &slot = request.resources[i];
		if (!slot.creating && !slot.batched_set_create) {
			continue;
		}
		const bool is_create_slot = slot.declaration.resource_kind == GDEXTENSION_PRODUCTION_SET_V1 ||
				slot.declaration.resource_kind == GDEXTENSION_PRODUCTION_STORAGE_V1;
		slot.batched_set_create = false;
		if (is_create_slot && slot.creating && !slot.primary.is_valid()) {
			slot.creating = false;
			slot.cached_persistent_set = false;
			slot.handoff_error = p_error;
			if (i < request.capacity.return_cells) {
				request.returns[i].pending = false;
			}
		}
	}
}

Error Registry::production_set_create_execute_batch_locked(ProductionRequest &request, RenderingDevice &rd, uint32_t leader_index) {
	const Error leader_err = production_create_execute_locked(request, rd, ProductionControlKind::SET_CREATE, leader_index);
	for (uint32_t other = 0; other < request.resource_count; ++other) {
		if (other == leader_index) {
			continue;
		}
		auto &other_slot = request.resources[other];
		if (!other_slot.batched_set_create || !other_slot.creating) {
			continue;
		}
		other_slot.batched_set_create = false;
		(void)production_create_execute_locked(request, rd, ProductionControlKind::SET_CREATE, other);
	}
	return leader_err;
}

bool Registry::production_maintenance_budget_ready_locked(const ProductionRequest &request, const GDExtensionProductionServiceBudgetV1 &budget, const ProductionServiceUsage &usage) const {
	return !(!budget.controls || usage.passes >= MIN(budget.passes, 64u) || usage.receipts >= MIN(budget.receipts, 128u) || !budget.deadline_micros || OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros || request.capture_inflight || request.continuation_inflight || request.source_device_lost || request.revision > UINT64_MAX - uint64_t(request.capacity.graph_memberships) * 2 - uint64_t(request.capacity.native_memberships) * 2 - request.capacity.readbacks - request.capacity.resources - 8);
}

Error Registry::production_maintenance_frame_admission_locked(ProductionRequest &request, RenderingDevice &rd, NativeDrainDiagnostic::Span &drain_trace) {
	if (!rd.driver || rd.frames.is_empty() || rd.draw_list.active || rd.compute_list.active || rd.raytracing_list.active || rd.frame_wait_draining || rd.draw_graph.has_driver_callbacks()) {
		return drain_trace.result(ERR_BUSY, "active_list_or_driver_callback");
	}
	if (request.maintenance_serial && (!rd.native_disposal_active || request.maintenance_serial != rd.native_disposal_continuation.serial)) {
		request.maintenance_serial = 0;
	}
	if (rd.native_disposal_active && !request.maintenance_serial) {
		return drain_trace.result(ERR_BUSY, "foreign_cursor"); // Never adopt another request's or qualification lane's cursor.
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
			drain_trace.state(request.serial, request.control.serial, rd.frame, command, rd.draw_graph.qualification_command_count(), 0);
			return drain_trace.result(ERR_BUSY, "uncaptured_graph");
		}
	}
	return OK;
}

Error Registry::production_maintenance_foreign_frame_admission_locked(const ProductionRequest &request, RenderingDevice &rd, uint32_t frame_index, bool resuming, NativeDrainDiagnostic::Span &drain_trace) {
	const auto &frame = rd.frames[frame_index];
	if (!frame.qualification_tickets.is_empty() || !frame.download_texture_get_data_requests.is_empty() || !frame.swap_chains_to_present.is_empty() || (!resuming && (!frame.acceleration_structures_to_dispose_of.is_empty() || !frame.raytracing_pipelines_to_dispose_of.is_empty()))) {
		drain_trace.state(request.serial, request.control.serial, frame_index, frame.swap_chains_to_present.size(), frame.qualification_tickets.size(), frame.download_texture_get_data_requests.size());
		return drain_trace.result(ERR_BUSY, "foreign_frame_work");
	}
	for (const auto &readback : frame.download_buffer_get_data_requests) {
		if (!readback.capture || readback.capture->registry != this) {
			return drain_trace.result(ERR_BUSY, "foreign_readback");
		}
	}
	return OK;
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
	bool pending_set_returns = false;
	bool set_pass_charged = false;
	const uint32_t visits = MIN(request.resource_count, 64u);
	for (uint32_t n = 0; n < visits; ++n) {
		const uint32_t i = request.retire_cursor % request.resource_count;
		request.retire_cursor = (i + 1) % request.resource_count;
		auto &slot = request.resources[i];
		const bool set = slot.declaration.resource_kind == GDEXTENSION_PRODUCTION_SET_V1;
		const bool storage = slot.declaration.resource_kind == GDEXTENSION_PRODUCTION_STORAGE_V1;
		const bool raster = request.raster_reserved && slot.declaration.resource_kind == GDEXTENSION_PRODUCTION_RASTER_PHYSICAL_V1;
		const bool raster_retire = raster && request.raster_prepare_origin.create_attempt && !request.resources[0].creating && (request.cancelled || request.next_action == request.action_count || request.resources[0].handoff_error != OK);
		if (slot.declaration.ownership != GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1 || !slot.primary.is_valid() || slot.returned || slot.cached_persistent_set ||
				(!set && !storage && !raster) || (raster && !raster_retire) || (storage && !slot.retiring) || (set && !request.cancelled && request.next_action != request.action_count)) {
			continue;
		}
		// An ordinary frame or another authenticated owner may have returned
		// this exact physical object since this request's last service turn.
		const auto publish_physical_return = [&](Error maintenance) {
			MutexLock physical(slot.owned_physical.mutex);
			const uint64_t event = slot.owned_physical.free_event;
			if (storage) {
				auto &state = slot.retire_report;
				state.frame_slot = slot.owned_physical.frame_slot;
				state.frame_epoch = slot.owned_physical.frame_epoch;
				state.event_seq = event;
				if (event) {
					// STORAGE still requires the original consumer's explicit ACK.
					state.state = state.return_status = 1;
				}
				if (maintenance != OK && maintenance != ERR_BUSY) {
					state.native_error = maintenance;
				}
			} else if (event) {
				slot.returned = true;
				slot.imported = false;
				request.returns[i].pending = false;
			}
			return event != 0;
		};
		if (publish_physical_return(OK)) {
			continue;
		}
		const bool set_within_pass = set && (set_pass_charged || usage.passes <= MIN(budget.passes, 64u));
		if ((!set_within_pass && (usage.passes >= MIN(budget.passes, 64u) || usage.receipts >= MIN(budget.receipts, 128u))) || OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros) {
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
		bump_resource_permission_epoch();
		if (!slot.owned_physical.enqueued) {
			if (set) {
				if (!set_pass_charged) {
					if (usage.passes < MIN(budget.passes, 64u)) {
						++usage.passes;
					}
					if (usage.receipts < MIN(budget.receipts, 128u)) {
						++usage.receipts;
					}
					set_pass_charged = true;
				}
			} else {
				++usage.passes;
				++usage.receipts;
			}
			usage.controls = 1;
			production_native_effect_enter_locked(request);
			++request.capture_inflight;
			unlock_mutex_for_call();
			const bool previous = rd.qualification_retiring;
			rd.qualification_retiring = true;
			// The executing cell/capture pin retains this slot while free runs
			// its synchronous callbacks. Do not inherit the service RD lock.
			rd._thread_safe_.unlock();
			rd.free_rid(slot.primary);
			rd._thread_safe_.lock();
			rd.qualification_retiring = previous;
			relock_mutex_after_call();
			CRASH_COND(!request.capture_inflight);
			--request.capture_inflight;
			++request.revision;
		}
		if (set) {
			// Each original free above retains its own pass, receipt and physical
			// identity. Share maintenance after the eligible SET prefix is queued.
			pending_set_returns = true;
			continue;
		}
		const Error maintenance = production_maintenance_locked(request, rd, budget, usage);
		publish_physical_return(maintenance);
		if (maintenance != OK) {
			return maintenance;
		}
	}

	if (!pending_set_returns) {
		return OK;
	}
	// An accepted whole-frame cursor keeps its exact captured prefix. Otherwise
	// retire only this original request's enqueued SETs, preserving unrelated
	// ordinary disposals rather than adopting them into a production receipt.
	NativeDrainDiagnostic::Span admission_trace("maintenance.set_return.preflight", request.serial, request.control.serial, request.maintenance_serial, rd.frame, uint64_t(rd.native_disposal_active.load()), rd.draw_graph.qualification_command_count());
	if (!production_maintenance_budget_ready_locked(request, budget, usage)) {
		return admission_trace.result(ERR_BUSY, "budget_or_inflight_or_loss");
	}
	if (rd.native_disposal_active.load()) {
		return production_maintenance_locked(request, rd, budget, usage);
	}
	const bool is_closing_exit = request.attachment && (request.attachment->closing ||
			(request.attachment->library < RECORD_CAPACITY && records[request.attachment->library].natural_close_draining));
	const bool live_window_set_drain = !rd.screen_swap_chains.is_empty() && rd.driver && !rd.frames.is_empty() &&
			!rd.draw_list.active && !rd.compute_list.active && !rd.raytracing_list.active &&
			!rd.frame_wait_draining && !rd.draw_graph.has_driver_callbacks();
	const bool allow_set_busy_bypass = is_closing_exit || live_window_set_drain;
	const Error frame_admission = production_maintenance_frame_admission_locked(request, rd, admission_trace);
	if (frame_admission != OK && !(allow_set_busy_bypass && frame_admission == ERR_BUSY)) {
		return frame_admission;
	}
	for (uint32_t frame_index = 0; frame_index < rd.frames.size(); ++frame_index) {
		const Error foreign_frame = production_maintenance_foreign_frame_admission_locked(request, rd, frame_index, false, admission_trace);
		if (foreign_frame != OK && !(allow_set_busy_bypass && foreign_frame == ERR_BUSY)) {
			return foreign_frame;
		}
	}
	const uint32_t avail_rec = MIN(budget.receipts, 128u) > usage.receipts ? (MIN(budget.receipts, 128u) - usage.receipts) : 0u;
	const uint32_t avail_pas = MIN(budget.passes, 64u) > usage.passes ? (MIN(budget.passes, 64u) - usage.passes) : 0u;
	if (!avail_rec || !avail_pas || OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros) {
		return ERR_BUSY;
	}
	RenderingDevice::NativeDisposalBudget drain_budget;
	drain_budget.receipts = is_closing_exit ? MAX(avail_rec, 64u) : avail_rec;
	drain_budget.passes = is_closing_exit ? MAX(avail_pas, 16u) : avail_pas;
	drain_budget.deadline_usec = budget.deadline_micros;
	RenderingDevice::NativeDisposalUsage drain_usage;
	const uint64_t request_serial = request.serial, control_serial = request.control.serial;
	auto *const attachment = request.attachment;
	const uint64_t epoch = attachment->epoch;
	production_native_effect_enter_locked(request);
	++request.capture_inflight;
	rd._thread_safe_.unlock();
	unlock_mutex_for_call();
	Error drained = rd._production_mq_drain_owned_disposals_step(request, nullptr, 0, drain_budget, drain_usage);
	if (drained == ERR_BUSY && is_closing_exit) {
		const uint64_t settle_deadline = MIN(drain_budget.deadline_usec, OS::get_singleton()->get_ticks_usec() + 50000ull);
		while (drained == ERR_BUSY && OS::get_singleton()->get_ticks_usec() < settle_deadline) {
			OS::get_singleton()->delay_usec(150);
			RenderingDevice::NativeDisposalBudget retry_budget;
			retry_budget.receipts = 64u;
			retry_budget.passes = 16u;
			retry_budget.deadline_usec = settle_deadline;
			RenderingDevice::NativeDisposalUsage retry_usage;
			drained = rd._production_mq_drain_owned_disposals_step(request, nullptr, 0, retry_budget, retry_usage);
			drain_usage.polls += retry_usage.polls;
			drain_usage.frees += retry_usage.frees;
			drain_usage.issues += retry_usage.issues;
			drain_usage.retains += retry_usage.retains;
			drain_usage.releases += retry_usage.releases;
			drain_usage.recycles += retry_usage.recycles;
			drain_usage.passes += retry_usage.passes;
		}
	}
	rd._thread_safe_.lock();
	relock_mutex_after_call();
	CRASH_COND(!request.capture_inflight);
	--request.capture_inflight;
	usage.controls = 1;
	const uint64_t total_rec = uint64_t(drain_usage.polls) + drain_usage.frees + drain_usage.issues + drain_usage.retains + drain_usage.releases + drain_usage.recycles;
	usage.receipts += uint32_t(MIN(total_rec, uint64_t(avail_rec)));
	usage.passes += MIN(drain_usage.passes, avail_pas);
	if (request.serial != request_serial || request.control.serial != control_serial || request.control.state != ProductionControlState::EXECUTING || request.attachment != attachment || attachment->held_device != &rd || attachment->epoch != epoch || device_records[attachment->device].epoch != epoch || device_records[attachment->device].object != &rd) {
		return ERR_INVALID_DATA;
	}
	for (uint32_t i = 0; i < request.resource_count; ++i) {
		auto &slot = request.resources[i];
		if (slot.declaration.resource_kind == GDEXTENSION_PRODUCTION_SET_V1 && slot.declaration.ownership == GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1 && !slot.returned && !slot.cached_persistent_set) {
			MutexLock physical(slot.owned_physical.mutex);
			if (slot.owned_physical.free_event) {
				slot.returned = true;
				slot.imported = false;
				request.returns[i].pending = false;
			}
		}
	}
	return drained;
}

Error Registry::production_maintenance_locked(ProductionRequest &request, RenderingDevice &rd, const GDExtensionProductionServiceBudgetV1 &budget, ProductionServiceUsage &usage) {
	NativeDrainDiagnostic::Span drain_trace("maintenance.preflight", request.serial, request.control.serial, request.maintenance_serial, rd.frame, uint64_t(rd.native_disposal_active.load()), rd.draw_graph.qualification_command_count());
	if (!production_maintenance_budget_ready_locked(request, budget, usage)) {
		return drain_trace.result(ERR_BUSY, "budget_or_inflight_or_loss");
	}
	bool closure_drop = false;
	for (uint32_t i = request.disposal_group_head; i != UINT32_MAX; i = request.uses[i].disposal_group_next) {
		const auto &cell = request.uses[i];
		if (cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP && cell.disposal_group_release) {
			closure_drop = true;
			break;
		}
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
		unlock_mutex_for_call();
		const Error dropped = rd._production_output_drop_step(request, slice, actual);
		// Native capture and factory return can acquire Registry while holding
		// RD. Restore that blocking order; Registry-held RD entry is try-only.
		rd._thread_safe_.lock();
		relock_mutex_after_call();
		CRASH_COND(!request.capture_inflight);
		--request.capture_inflight;
		usage.controls = 1;
		usage.passes += actual.passes;
		usage.receipts += actual.polls + actual.frees + actual.issues + actual.retains + actual.releases + actual.recycles;
		if (request.serial != request_serial || request.control.serial != control_serial || request.control.state != ProductionControlState::EXECUTING || request.attachment != attachment || attachment->held_device != &rd || attachment->epoch != epoch || device_records[attachment->device].epoch != epoch || device_records[attachment->device].object != &rd) {
			return drain_trace.result(ERR_INVALID_DATA, "drop_identity_changed");
		}
		if (dropped != OK) {
			return drain_trace.result(dropped, "drop_step");
		}
		if (usage.passes >= MIN(budget.passes, 64u) || usage.receipts >= MIN(budget.receipts, 128u)) {
			return drain_trace.result(ERR_BUSY, "drop_budget_exhausted");
		}
	}
	const bool mq_in_place_early = production_request_is_pure_mq_in_place_locked(request);
	const bool own_or_idle_disposal = !rd.native_disposal_active || (!request.maintenance_serial && rd.native_disposal_continuation.production_owner == nullptr);
	bool any_graph_pending = false;
	for (uint32_t i = 0; i < request.graph_count; ++i) {
		any_graph_pending |= request.graphs[i].pending;
	}
	bool has_retiring_disposals = false;
	for (uint32_t i = 0; i < request.resource_count && !has_retiring_disposals; ++i) {
		const auto &slot = request.resources[i];
		MutexLock physical(slot.owned_physical.mutex);
		has_retiring_disposals |= slot.retiring && slot.owned_physical.enqueued && !slot.owned_physical.free_event && !slot.returned;
	}
	bool has_retiring_output_groups = false;
	const uint32_t member_end = MIN(request.disposal_use_high_water, request.capacity.resource_uses);
	for (uint32_t g = request.disposal_group_head; g != UINT32_MAX && !has_retiring_output_groups; g = request.uses[g].disposal_group_next) {
		const auto &group = request.uses[g];
		if (group.output_role != ProductionUseCell::OutputRole::DISPOSAL_GROUP || !group.disposal_group_release) {
			continue;
		}
		has_retiring_output_groups |= !group.disposal_group_dropped;
		for (uint32_t i = request.use_count + request.binding_used; i < member_end && !has_retiring_output_groups; ++i) {
			const auto &member = request.uses[i];
			if (member.output_role == ProductionUseCell::OutputRole::DISPOSAL_MEMBER && member.disposal_group_serial == group.disposal_group_serial) {
				const uint8_t st = member.disposal.state.load(std::memory_order_acquire);
				has_retiring_output_groups |= (st == 1 || st == 2);
			}
		}
	}
	has_retiring_disposals |= has_retiring_output_groups;
	const bool is_closing_exit = request.attachment && (request.attachment->closing ||
			(request.attachment->library < RECORD_CAPACITY && records[request.attachment->library].natural_close_draining));
	if (own_or_idle_disposal && (mq_in_place_early || request.graph_count > 0)) {
		if (!any_graph_pending) {
			// Godot's normal _end_frame() already submitted this request's GPU work + readback.
			// Poll/deliver the earliest frame holding this request's readback capture (matching
			// returned offset) only when every capture on that frame is in-order, avoiding
			// out-of-order segment delivery on split multi-frame readbacks (BUG-120).
			auto frame_readbacks_in_order = [&](uint32_t fi) -> bool {
				for (const auto &other_rb : rd.frames[fi].download_buffer_get_data_requests) {
					if (!other_rb.capture) {
						continue;
					}
					if (!other_rb.capture->state || other_rb.result_offset != other_rb.capture->state->returned) {
						return false;
					}
				}
				return true;
			};
			uint32_t rb_frame = UINT32_MAX;
			for (uint32_t f = 0; f < rd.frames.size() && rb_frame == UINT32_MAX; ++f) {
				for (const auto &rb : rd.frames[f].download_buffer_get_data_requests) {
					if (rb.capture && rb.capture->registry == this && rb.capture->request_serial == request.serial &&
							rb.capture->state && rb.result_offset == rb.capture->state->returned) {
						rb_frame = f;
						break;
					}
				}
			}
			if (rb_frame != UINT32_MAX) {
				if (!frame_readbacks_in_order(rb_frame)) {
					return drain_trace.result(ERR_BUSY, "split_readback_order");
				}
				if (!rd.frames[rb_frame].fence_signaled && !rd.frame_wait_draining) {
					if (!rd.frames[rb_frame].primary_command_buffer_begun && !rd.frames[rb_frame].disposal.active) {
						usage.controls = 1;
						production_native_effect_enter_locked(request);
						++request.capture_inflight;
						unlock_mutex_for_call();
						const Error delivered = rd._stall_for_frame(rb_frame, true);
						relock_mutex_after_call();
						CRASH_COND(!request.capture_inflight);
						--request.capture_inflight;
						return delivered;
					}
					return drain_trace.result(ERR_BUSY, "rb_frame_not_submitted");
				}
				RenderingDevice::NativeDisposalBudget slice;
				slice.receipts = 256u;
				slice.passes = 256u;
				slice.deadline_usec = budget.deadline_micros;
				RenderingDevice::NativeDisposalUsage actual;
				usage.controls = 1;
				production_native_effect_enter_locked(request);
				++request.capture_inflight;
				unlock_mutex_for_call();
				const auto *prev_budget = rd.native_disposal_budget;
				auto *prev_usage = rd.native_disposal_usage;
				rd.native_disposal_budget = &slice;
				rd.native_disposal_usage = &actual;
				const Error stalled = rd._native_disposal_stall_step(rb_frame);
				rd.native_disposal_budget = prev_budget;
				rd.native_disposal_usage = prev_usage;
				relock_mutex_after_call();
				CRASH_COND(!request.capture_inflight);
				--request.capture_inflight;
				const uint32_t avail_passes = MIN(budget.passes, 64u) > usage.passes ? (MIN(budget.passes, 64u) - usage.passes) : 0u;
				const uint32_t avail_receipts = MIN(budget.receipts, 128u) > usage.receipts ? (MIN(budget.receipts, 128u) - usage.receipts) : 0u;
				const uint64_t total_actual = uint64_t(actual.polls) + actual.frees + actual.issues + actual.retains + actual.releases + actual.recycles;
				usage.passes += MIN(actual.passes, avail_passes);
				usage.receipts += uint32_t(MIN(total_actual, uint64_t(avail_receipts)));
				return stalled;
			}
			// No pending graphs and no pending readback capture for this request.
			// On live viewports (!is_closing_exit), normal swap_buffers()/_begin_frame()
			// drains retiring resources; only hold OBSERVE for retiring disposals.
			if (!has_retiring_disposals && (mq_in_place_early || !rd.draw_graph.qualification_command_count() || production_maintenance_frame_admission_locked(request, rd, drain_trace) != OK)) {
				return OK;
			}
		}
	}
	const bool live_window_retire = !rd.screen_swap_chains.is_empty() && has_retiring_disposals && !any_graph_pending;
	auto drain_owned_on_close = [&](Error fallback_err) -> Error {
		if (fallback_err != ERR_BUSY || !(is_closing_exit || mq_in_place_early || has_retiring_output_groups || live_window_retire) || !has_retiring_disposals || any_graph_pending ||
				rd.native_disposal_active.load() || !rd.driver || rd.frames.is_empty() ||
				rd.draw_list.active || rd.compute_list.active || rd.raytracing_list.active ||
				rd.frame_wait_draining || rd.draw_graph.has_driver_callbacks() ||
				(!is_closing_exit && !mq_in_place_early && !has_retiring_output_groups && !live_window_retire && rd.draw_graph.qualification_command_count())) {
			return fallback_err;
		}
		for (uint32_t fi = 0; fi < rd.frames.size(); ++fi) {
			if (rd.frames[fi].disposal.active) {
				return fallback_err;
			}
			for (const auto &rb : rd.frames[fi].download_buffer_get_data_requests) {
				if (!rb.capture || rb.capture->registry != this || (rb.capture->request_serial == request.serial && !rd.frames[fi].fence_signaled)) {
					return fallback_err;
				}
			}
		}
		const uint32_t avail_rec = MIN(budget.receipts, 128u) > usage.receipts ? (MIN(budget.receipts, 128u) - usage.receipts) : 0u;
		const uint32_t avail_pas = MIN(budget.passes, 64u) > usage.passes ? (MIN(budget.passes, 64u) - usage.passes) : 0u;
		if (!avail_rec || !avail_pas || OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros) {
			return fallback_err;
		}
		RenderingDevice::NativeDisposalBudget drain_budget;
		drain_budget.receipts = MAX(avail_rec, 64u);
		drain_budget.passes = MAX(avail_pas, 16u);
		drain_budget.deadline_usec = budget.deadline_micros;
		RenderingDevice::NativeDisposalUsage drain_usage;
		const uint64_t request_serial = request.serial, control_serial = request.control.serial;
		auto *const attachment = request.attachment;
		const uint64_t epoch = attachment->epoch;
		production_native_effect_enter_locked(request);
		++request.capture_inflight;
		rd._thread_safe_.unlock();
		unlock_mutex_for_call();
		Error drained = rd._production_mq_drain_owned_disposals_step(request, nullptr, 0, drain_budget, drain_usage);
		if (drained == ERR_BUSY && (is_closing_exit || mq_in_place_early)) {
			const uint64_t settle_deadline = MIN(drain_budget.deadline_usec, OS::get_singleton()->get_ticks_usec() + 1500ull);
			while (drained == ERR_BUSY && OS::get_singleton()->get_ticks_usec() < settle_deadline) {
				OS::get_singleton()->delay_usec(150);
				RenderingDevice::NativeDisposalBudget retry_budget;
				retry_budget.receipts = 64u;
				retry_budget.passes = 16u;
				retry_budget.deadline_usec = settle_deadline;
				RenderingDevice::NativeDisposalUsage retry_usage;
				drained = rd._production_mq_drain_owned_disposals_step(request, nullptr, 0, retry_budget, retry_usage);
				drain_usage.polls += retry_usage.polls;
				drain_usage.frees += retry_usage.frees;
				drain_usage.issues += retry_usage.issues;
				drain_usage.retains += retry_usage.retains;
				drain_usage.releases += retry_usage.releases;
				drain_usage.recycles += retry_usage.recycles;
				drain_usage.passes += retry_usage.passes;
			}
		}
		rd._thread_safe_.lock();
		relock_mutex_after_call();
		CRASH_COND(!request.capture_inflight);
		--request.capture_inflight;
		usage.controls = 1;
		const uint64_t total_rec = uint64_t(drain_usage.polls) + drain_usage.frees + drain_usage.issues + drain_usage.retains + drain_usage.releases + drain_usage.recycles;
		usage.receipts += uint32_t(MIN(total_rec, uint64_t(avail_rec)));
		usage.passes += MIN(drain_usage.passes, avail_pas);
		if (request.serial != request_serial || request.control.serial != control_serial || request.control.state != ProductionControlState::EXECUTING || request.attachment != attachment || attachment->held_device != &rd || attachment->epoch != epoch || device_records[attachment->device].epoch != epoch || device_records[attachment->device].object != &rd) {
			return ERR_INVALID_DATA;
		}
		return drained;
	};
	const Error frame_admission = production_maintenance_frame_admission_locked(request, rd, drain_trace);
	if (frame_admission != OK) {
		return drain_owned_on_close(frame_admission);
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
				for (uint32_t a = 0; a < owner.action_count; ++a) {
					if (production_material_qualification_owns_framebuffer_physical(owner.actions[a], physical)) {
						return true;
					}
				}
			}
			for (uint32_t p = 0; p < attachment->persistent_sets.size(); ++p) {
				if (attachment->persistent_sets[p] && &attachment->persistent_sets[p]->physical == physical) {
					return true;
				}
			}
		}
		return false;
	};
	auto exact_closure = [&](const ProductionDisposalUse *node, uint32_t kind, uint64_t native_driver_id, uint64_t backing = 0) {
		for (; node; node = node->next) {
			if (!node->request || !node->request_serial || node->kind != kind || !node->rid || !node->serial || node->native_driver_id != native_driver_id || (backing && node->serial != backing) || node->state.load(std::memory_order_acquire) != 2) {
				continue;
			}
			if (node->request == &request && node->request_serial == request.serial) {
				return true;
			}
			// Mirror owned_physical: admit live sibling production requests on this same RD
			// so concurrent slots do not mutually starve on each other's output closures (BUG-120).
			for (auto *attachment : production_attachments) {
				if (!attachment || attachment->held_device != &rd) {
					continue;
				}
				for (uint32_t r = 0; r < attachment->quote.capacity.requests; ++r) {
					const auto &owner = attachment->requests[r];
					if (owner.serial && node->request == &owner && node->request_serial == owner.serial) {
						return true;
					}
				}
			}
		}
		return false;
	};
	const bool resuming = rd.native_disposal_active && request.maintenance_serial == rd.native_disposal_continuation.serial;
	if (has_retiring_output_groups && !resuming && !rd.screen_swap_chains.is_empty()) {
		return drain_owned_on_close(ERR_BUSY);
	}
	const bool mq_in_place = production_request_is_pure_mq_in_place_locked(request);
	if (mq_in_place) {
		const uint32_t target_frame = resuming ? rd.native_disposal_continuation.target : rd.frame;
		if (target_frame >= rd.frames.size()) {
			return drain_trace.result(ERR_INVALID_DATA, "bad_target_frame");
		}
		const auto &frame = rd.frames[target_frame];
		if (!frame.qualification_tickets.is_empty() || !frame.download_texture_get_data_requests.is_empty() || !frame.swap_chains_to_present.is_empty()) {
			drain_trace.state(request.serial, request.control.serial, target_frame, frame.swap_chains_to_present.size(), frame.qualification_tickets.size(), frame.download_texture_get_data_requests.size());
			return drain_trace.result(ERR_BUSY, "foreign_frame_work");
		}
		for (const auto &readback : frame.download_buffer_get_data_requests) {
			if (!readback.capture || readback.capture->registry != this) {
				return drain_trace.result(ERR_BUSY, "foreign_readback");
			}
			// Forbid in-place stall if this frame carries a non-initial segment of a split
			// readback or a capture that also has an undelivered segment on another frame (BUG-120).
			if (readback.result_offset > 0 || !readback.capture->state || readback.result_offset != readback.capture->state->returned) {
				return drain_trace.result(ERR_BUSY, "split_readback_order");
			}
			for (uint32_t other_f = 0; other_f < rd.frames.size(); ++other_f) {
				if (other_f == target_frame) {
					continue;
				}
				for (const auto &other_rb : rd.frames[other_f].download_buffer_get_data_requests) {
					if (other_rb.capture == readback.capture) {
						return drain_trace.result(ERR_BUSY, "split_readback_other_frame");
					}
				}
			}
		}
	} else {
		// A fresh RD cursor captures only this frame's buffer-disposal prefix.
		// An interrupted ordinary advance has already selected the current frame.
		const uint32_t fresh_buffer_target = rd.qualification_frame_advance_pending ?
				uint32_t(rd.frame) : (rd.frame + 1) % rd.frames.size();
		for (uint32_t frame_index = 0; frame_index < rd.frames.size(); ++frame_index) {
			if (resuming && frame_index != rd.native_disposal_continuation.target) {
				continue;
			}
			const auto &frame = rd.frames[frame_index];
			const Error foreign_frame = production_maintenance_foreign_frame_admission_locked(request, rd, frame_index, resuming, drain_trace);
			if (foreign_frame != OK) {
				return drain_owned_on_close(foreign_frame);
			}
			uint32_t buffers = frame_index == fresh_buffer_target ? frame.buffers_to_dispose_of.size() : 0;
			uint32_t sets = frame.uniform_sets_to_dispose_of.size();
			if (resuming) {
				buffers = sets = 0;
				if (frame_index == rd.native_disposal_continuation.target && rd.native_disposal_continuation.phase < 5) {
					if (!frame.disposal.active) {
						return drain_trace.result(ERR_INVALID_DATA, "missing_disposal_cursor");
					}
					for (uint32_t kind = 0; kind < 10; ++kind) {
						if ((kind == 2 || kind == 3) && frame.disposal.remaining[kind]) {
							return drain_trace.result(ERR_INVALID_DATA, "unsupported_disposal_kind");
						}
					}
					buffers = frame.disposal.remaining[9];
					sets = frame.disposal.remaining[4];
				}
			}
			const auto *buffer = frame.buffers_to_dispose_of.front();
			for (uint32_t i = 0; i < buffers; ++i) {
				if (!buffer || (!owned_physical(buffer->get().qualification_return) && !exact_closure(buffer->get().production_disposal_uses, 9, buffer->get().driver_id.id, buffer->get().backing_serial))) {
					return drain_owned_on_close(drain_trace.result(ERR_BUSY, "foreign_buffer"));
				}
				buffer = buffer->next();
			}
			const auto *set = frame.uniform_sets_to_dispose_of.front();
			for (uint32_t i = 0; i < sets; ++i) {
				if (!set || (!owned_physical(set->get().qualification_return) && !exact_closure(set->get().production_disposal_uses, 4, set->get().driver_id.id))) {
					return drain_owned_on_close(drain_trace.result(ERR_BUSY, "foreign_uniform_set"));
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
					return drain_owned_on_close(drain_trace.result(ERR_BUSY, "foreign_framebuffer"));
				}
				framebuffer = framebuffer->next();
			}
			if (!closure_prefix(frame.render_pipelines_to_dispose_of, 0) || !closure_prefix(frame.compute_pipelines_to_dispose_of, 1) || !closure_prefix(frame.shaders_to_dispose_of, 5) || !closure_prefix(frame.samplers_to_dispose_of, 6) || !closure_prefix(frame.textures_to_dispose_of, 8)) {
				return drain_owned_on_close(drain_trace.result(ERR_BUSY, "foreign_closure_prefix"));
			}

		}
	}
	const uint32_t avail_receipts = MIN(budget.receipts, 128u) - usage.receipts;
	const uint32_t avail_passes = MIN(budget.passes, 64u) - usage.passes;
	RenderingDevice::NativeDisposalBudget service;
	service.receipts = MAX(avail_receipts, 64u);
	service.passes = MAX(avail_passes, 16u);
	service.deadline_usec = budget.deadline_micros;
	RenderingDevice::NativeDisposalUsage actual;
	usage.controls = 1;
	production_native_effect_enter_locked(request);
	++request.capture_inflight;
	unlock_mutex_for_call();
	const bool starting = !rd.native_disposal_active;
	Error error = rd._native_disposal_maintenance_step(service, actual, &request, nullptr, mq_in_place);
	if (error == ERR_BUSY && rd.native_disposal_active) {
		const uint64_t settle_deadline = MIN(service.deadline_usec, OS::get_singleton()->get_ticks_usec() + 2000ull);
		while (error == ERR_BUSY && rd.native_disposal_active && OS::get_singleton()->get_ticks_usec() < settle_deadline) {
			OS::get_singleton()->delay_usec(50);
			RenderingDevice::NativeDisposalBudget retry_service;
			retry_service.receipts = 64u;
			retry_service.passes = 16u;
			retry_service.deadline_usec = settle_deadline;
			RenderingDevice::NativeDisposalUsage step_actual;
			error = rd._native_disposal_maintenance_step(retry_service, step_actual, &request, nullptr, mq_in_place);
			actual.polls += step_actual.polls;
			actual.frees += step_actual.frees;
			actual.issues += step_actual.issues;
			actual.retains += step_actual.retains;
			actual.releases += step_actual.releases;
			actual.recycles += step_actual.recycles;
			actual.passes += step_actual.passes;
		}
	}
	relock_mutex_after_call();
	if (starting && rd.native_disposal_active) {
		request.maintenance_serial = rd.native_disposal_continuation.serial;
	} else if (!rd.native_disposal_active) {
		request.maintenance_serial = 0;
	}
	CRASH_COND(!request.capture_inflight);
	--request.capture_inflight;
	const uint64_t total_actual_receipts = uint64_t(actual.polls) + actual.frees + actual.issues + actual.retains + actual.releases + actual.recycles;
	usage.receipts += uint32_t(MIN(total_actual_receipts, uint64_t(avail_receipts)));
	usage.passes += MIN(actual.passes, avail_passes);
	return drain_trace.result(error, "native_step_returned");
}

bool Registry::has_natural_close_pending_work(const RenderingDevice *p_rd) {
	if (!p_rd || !mutex.try_lock()) {
		return false;
	}
	bool pending = false;
	bool has_closing_attachment = false;
	for (const auto *attachment : production_attachments) {
		if (!attachment || attachment->held_device != p_rd || attachment->library >= RECORD_CAPACITY) {
			continue;
		}
		if (!attachment->closing && !records[attachment->library].natural_close_draining) {
			continue;
		}
		has_closing_attachment = true;
		if (!attachment->requests) {
			continue;
		}
		for (uint32_t r = 0; r < attachment->quote.capacity.requests && !pending; ++r) {
			const auto &req = attachment->requests[r];
			if (!req.serial) {
				continue;
			}
			for (uint32_t g = 0; g < req.graph_count; ++g) {
				if (req.graphs[g].pending) {
					pending = true;
					break;
				}
			}
			if (!pending) {
				for (uint32_t i = 0; i < req.resource_count; ++i) {
					if (req.resources[i].retiring && !req.resources[i].returned) {
						pending = true;
						break;
					}
				}
			}
			if (!pending && req.disposal_group_head != UINT32_MAX) {
				for (uint32_t g = req.disposal_group_head; g != UINT32_MAX; g = req.uses[g].disposal_group_next) {
					const auto &group = req.uses[g];
					if (group.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP && group.disposal_group_release) {
						pending = true;
						break;
					}
				}
			}
		}
		if (pending) {
			break;
		}
	}
	if (!pending && has_closing_attachment) {
		if (p_rd->draw_graph.qualification_command_count() > 0) {
			pending = true;
		} else {
			for (uint32_t f = 0; f < p_rd->frames.size(); ++f) {
				const auto &fr = p_rd->frames[f];
				if (!fr.fence_signaled || !fr.buffers_to_dispose_of.is_empty() || !fr.uniform_sets_to_dispose_of.is_empty() ||
						!fr.render_pipelines_to_dispose_of.is_empty() || !fr.compute_pipelines_to_dispose_of.is_empty() ||
						!fr.shaders_to_dispose_of.is_empty() || !fr.textures_to_dispose_of.is_empty() ||
						!fr.framebuffers_to_dispose_of.is_empty()) {
					pending = true;
					break;
				}
			}
		}
	}
	mutex.unlock();
	return pending;
}
} // namespace GDExtensionDeviceExit
