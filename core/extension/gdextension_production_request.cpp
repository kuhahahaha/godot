/**************************************************************************/
/*  gdextension_production_request.cpp                                    */
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

#include "core/extension/gdextension_production_registration.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server_default.h"

#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>

namespace GDExtensionDeviceExit {
namespace {
GDExtensionDeviceSessionResultV1 result(Error e, uint32_t reason = 0) {
	return { int32_t(e), reason, 0, 0 };
}
template <class T>
bool aligned(const T *p) {
	return p && uintptr_t(p) % alignof(T) == 0;
}
bool abi(const GDExtensionDeviceExitAbi &p) {
	return p.abi_version == 1 && p.struct_size == sizeof(p) && !p.reserved;
}
bool separate(const void *a, size_t an, const void *b, size_t bn) {
	auto x = uintptr_t(a), y = uintptr_t(b);
	return x <= y ? an <= y - x : bn <= x - y;
}
bool add_layout(uint64_t &offset, uint64_t count, uint64_t bytes, uint64_t alignment) {
	const uint64_t padding = (alignment - offset % alignment) % alignment;
	if (padding > UINT64_MAX - offset) {
		return false;
	}
	offset += padding;
	if (count > (UINT64_MAX - offset) / bytes) {
		return false;
	}
	offset += count * bytes;
	return offset <= SIZE_MAX && offset <= INT64_MAX;
}
bool capacity_fits(const GDExtensionProductionCapacityV1 &a, const GDExtensionProductionCapacityV1 &b) {
	return a.requests <= b.requests && a.suboperations <= b.suboperations && a.resources <= b.resources && a.resource_uses <= b.resource_uses && a.graph_memberships <= b.graph_memberships && a.native_memberships <= b.native_memberships && a.readbacks <= b.readbacks && a.return_cells <= b.return_cells && a.retained_payload_bytes <= b.retained_payload_bytes && a.readback_bytes <= b.readback_bytes;
}
struct RdUnlock {
	Mutex &mutex;
	~RdUnlock() { mutex.unlock(); }
};
bool range(uint64_t offset, uint64_t bytes, uint64_t extent) {
	return offset <= extent && bytes <= extent - offset;
}
bool before_service_deadline(const GDExtensionProductionServiceBudgetV1 &budget) {
	return budget.deadline_micros && OS::get_singleton() && OS::get_singleton()->get_ticks_usec() < budget.deadline_micros;
}
bool service_budget_active(const GDExtensionProductionServiceBudgetV1 &budget) {
	return budget.controls && budget.receipts && budget.passes && before_service_deadline(budget);
}
bool same_service_budget(const GDExtensionProductionServiceBudgetV1 &a, const GDExtensionProductionServiceBudgetV1 &b) {
	return a.deadline_micros == b.deadline_micros && a.controls == b.controls && a.receipts == b.receipts && a.passes == b.passes;
}
bool retainable_native_source(const RDD::NativeCallFact &s) {
	return s.serial && s.queue_index && s.completion_serial && s.completion != RDD::CompletionCapability::Disabled && !s.presentation_only;
}
bool same_native_source(const RDD::NativeCallFact &a, const RDD::NativeCallFact &b) {
	return a.serial == b.serial && a.issue == b.issue && a.error == b.error && a.native_code == b.native_code && a.completion == b.completion && a.completion_serial == b.completion_serial && a.queue_index == b.queue_index && a.present == b.present && a.presentation_only == b.presentation_only && a.source_owner_returned == b.source_owner_returned && a.frame_observer_owned == b.frame_observer_owned;
}
bool same_native_observation(const NativeSourceObservation &a, const NativeSourceObservation &b) {
	return a.state == b.state && a.error == b.error && a.native_code == b.native_code && a.completion_native_code == b.completion_native_code && a.resolved_issue == b.resolved_issue && a.retention_error == b.retention_error && a.completion_error == b.completion_error;
}
} //namespace
Error Registry::production_layout(const GDExtensionProductionCapacityV1 &c, GDExtensionProductionMetadataQuoteV1 &out, uint32_t current_commands, uint32_t workers) {
	if (!production_dto_valid(c) || !c.requests || !c.suboperations || !c.resources || c.requests > c.suboperations || c.requests > c.resources || c.resource_uses < c.resources || c.graph_memberships < c.suboperations || c.native_memberships < c.suboperations || c.return_cells < c.resources || c.readbacks > c.suboperations) {
		return ERR_INVALID_PARAMETER;
	}
	// Report counters and the paged graph/native fact index use uint32_t.
	if (uint64_t(c.graph_memberships) + c.native_memberships + c.readbacks + c.return_cells + 2 > UINT32_MAX) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	// Each reserved request owns its command slice. Its pre-existing RD prefix
	// cannot be shared with another request, even when both observe the same
	// current frame. Keep that prefix once per request in the attachment pool.
	const uint64_t prefixes = uint64_t(current_commands) * c.requests;
	if (prefixes > UINT32_MAX) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	uint32_t commands = 0, frames = 0;
	Error e = record_source_slot_bound(c.requests, c.graph_memberships, uint32_t(prefixes), workers, commands, frames);
	if (e != OK) {
		return e;
	}
	uint64_t bytes = 0;
#define LAYOUT(type, count) \
	if (!add_layout(bytes, count, sizeof(type), alignof(type))) \
	return ERR_PARAMETER_RANGE_ERROR
	LAYOUT(ProductionAttachment, 1);
	LAYOUT(ProductionRequest, c.requests);
	LAYOUT(ProductionActionCell, c.suboperations);
	LAYOUT(ProductionResourceSlot, c.resources);
	LAYOUT(ProductionUseCell, c.resource_uses);
	LAYOUT(ProductionReadbackCell, c.readbacks);
	LAYOUT(ProductionGraphCell, c.graph_memberships);
	LAYOUT(ProductionNativeCell, c.native_memberships);
	LAYOUT(ProductionReturnCell, c.return_cells);
	LAYOUT(ProductionCaptureCell, commands);
	LAYOUT(ProductionCaptureCell, frames);
	LAYOUT(uint8_t, c.retained_payload_bytes);
	LAYOUT(uint8_t, c.readback_bytes);
#undef LAYOUT
	out = { production_dto_abi<GDExtensionProductionMetadataQuoteV1>(), c, bytes, alignof(std::max_align_t), commands, frames, uint64_t(c.return_cells) + 1 }; // One accepted metadata entry credit.
	return OK;
}
Error Registry::production_membership_bound_locked(RenderingDevice &rd, uint32_t graphs, uint32_t workers, uint32_t &out) const {
	out = 0;
	if (!rd.driver) {
		return ERR_UNAVAILABLE;
	}
	// A graph belongs to exactly one main slot, plus only the workers used by
	// its canonical action. The profile includes already committed wrapper
	// records and the pre-graph hidden source. Reserve the second post-graph
	// prepare source explicitly, preserving the original qualification profile.
	const uint32_t per_graph = rd.driver->qualification_native_call_bound_for_profile(1, workers, 1);
	uint32_t post_graph = 0;
	if (!per_graph || !rd.driver->native_issue_bound(RDD::NativeIssueKind::PREPARE_SEGMENT, 0, post_graph)) {
		return ERR_UNAVAILABLE;
	}
	const uint64_t per_edge = uint64_t(per_graph) + post_graph;
	if (graphs && per_edge > UINT32_MAX / graphs) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	out = uint32_t(uint64_t(graphs) * per_edge);
	return OK;
}
Error Registry::production_device_quote_locked(RenderingDevice &rd, const GDExtensionProductionCapacityV1 &capacity, GDExtensionProductionMetadataQuoteV1 &out) {
	if (!rd._thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	RdUnlock unlock{ rd._thread_safe_ };
	if (!rd.driver || rd.frames.is_empty()) {
		return ERR_UNAVAILABLE;
	}
	const uint32_t current = rd.draw_graph.qualification_command_count();
	GDExtensionProductionMetadataQuoteV1 quote = {};
	Error e = production_layout(capacity, quote, current, rd.transfer_worker_pool_max_size);
	if (e != OK) {
		return e;
	}
	uint32_t memberships = 0;
	e = production_membership_bound_locked(rd, capacity.graph_memberships, MIN(capacity.resources, rd.transfer_worker_pool_max_size), memberships);
	if (e != OK) {
		return e;
	}
	if (memberships > capacity.native_memberships) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	out = quote;
	return OK;
}
Error Registry::production_installation_locked(GDExtensionDeviceExitSceneReadyContextRef frame, GDExtensionProducerRegistrationRefV1 producer, GDExtensionDeviceRegistryContextRef context, GDExtensionOriginalDeviceSessionRef session, uint32_t &library, uint32_t &device, HolderRetirement *&holder) {
	View *view = nullptr;
	Error e = resolve_borrow_locked(frame.registry_instance, frame.handle_serial, ViewDomain::SCENE_READY, view);
	if (e != OK) {
		return e;
	}
	library = view->record;
	auto &record = records[library];
	const auto *current = RegistrationContext::current;
	if (!current || !current->reference || current->reference->registry != this || current->reference->slot != library || !current->admitted || current->ack || current->level_phase != LevelPhase::SCENE_READY || record.scene_ready_state != Record::SceneReadyState::IN_FRAME || record.owner_thread != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	if (!record.attached || record.quiescing || record.close_started || record.close_error != OK || record.guard_enrollment != GuardEnrollment::QUALIFIED) {
		return ERR_UNAVAILABLE;
	}
	if (record.scene_ready_flavor == Record::SceneReadyFlavor::V2 && (!(record.scene_install_plan.selected_features & GDEXTENSION_SCENE_INSTALL_RD_PAIR_V2) || !scene_install_plan_matches_locked(record))) {
		return ERR_UNAVAILABLE;
	}
	uint32_t producer_view = INVALID_SLOT;
	e = producer_view_locked(producer, producer_view);
	if (e != OK) {
		return e;
	}
	if (views[producer_view].record != library || !record.producer_attachment_identity || !record.producer_registration_serial) {
		return ERR_UNAUTHORIZED;
	}
	DeviceRef *c = nullptr, *s = nullptr;
	e = device_ref_locked(context.registry_instance, context.handle_serial, false, c);
	if (e != OK) {
		return e;
	}
	e = device_ref_locked(session.registry_instance, session.handle_serial, true, s);
	if (e != OK) {
		return e;
	}
	if (c->library != library || s->library != library || c->device != s->device) {
		return ERR_UNAUTHORIZED;
	}
	device = c->device;
	auto &d = device_records[device];
	auto *rd = Object::cast_to<RenderingDevice>(d.object);
	// Physical main-device comparison is independent of qualification_main.
	if (!d.live || !d.epoch || !d.session || !rd || rd != RenderingDevice::get_singleton() || !rd->is_main_instance || d.execution_thread != rd->render_thread_id || rd->production_finalization_started.load(std::memory_order_acquire) || rd->native_finalization_returned) {
		return ERR_UNAVAILABLE;
	}
	if (!(live_services & (1u << 5))) {
		return ERR_UNAVAILABLE;
	}
	Registry *entity_owner = rd->production_entity_registry.load(std::memory_order_acquire);
	if (entity_owner && entity_owner != this) {
		return ERR_UNAUTHORIZED;
	}
	holder = nullptr;
	for (auto *h = record.holders; h; h = h->record_next) {
		if (!h->typed_code_hold || h->pending || h->handle_serial != record.producer_code_holder || h->generation != record.snapshot.library_generation || h->acquired_thread != record.owner_thread) {
			continue;
		}
		const auto &scene = record.levels[GDEXTENSION_INITIALIZATION_SCENE];
		if (!scene.initialized || scene.finalized) {
			continue;
		}
		// The original Host currently acquires its SCENE USER terminal hold.
		// Either original action is valid only with its own declared RD service.
		for (const auto *action : { &scene.user_action, &scene.binding_action }) {
			if (action->serial && h->terminal_action == action->serial && !action->returned &&
					action->generation == h->generation && action->owner_thread == record.owner_thread &&
					action->level == GDEXTENSION_INITIALIZATION_SCENE &&
					action->kind == (action == &scene.user_action ? TerminalActionKind::LEVEL_USER : TerminalActionKind::LEVEL_BINDING) &&
					(action->services & (1u << 5)) &&
					action->wrapper && action->wrapper == record.original_initialization.deinitialize &&
					action->userdata == record.original_initialization.userdata) {
				holder = h;
			}
		}
	}
	return holder ? OK : ERR_UNAUTHORIZED;
}
GDExtensionDeviceSessionResultV1 Registry::production_quote(const GDExtensionDeviceExitAbi *a, GDExtensionDeviceExitSceneReadyContextRef frame, GDExtensionProducerRegistrationRefV1 producer, GDExtensionDeviceRegistryContextRef context, GDExtensionOriginalDeviceSessionRef session, const GDExtensionProductionCapacityV1 *capacity, GDExtensionProductionMetadataQuoteV1 *out) {
	if (!aligned(a) || !abi(*a) || !aligned(capacity) || !aligned(out)) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	MutexLock lock(mutex);
	uint32_t library = 0, device = 0;
	HolderRetirement *holder = nullptr;
	Error e = production_installation_locked(frame, producer, context, session, library, device, holder);
	if (e != OK) {
		return result(e, 16);
	}
	GDExtensionProductionMetadataQuoteV1 quote = {};
	e = production_device_quote_locked(*Object::cast_to<RenderingDevice>(device_records[device].object), *capacity, quote);
	if (e != OK) {
		return result(e, 19);
	}
	*out = quote;
	return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::production_accept(const GDExtensionDeviceExitAbi *a, GDExtensionDeviceExitSceneReadyContextRef frame, GDExtensionProducerRegistrationRefV1 producer, GDExtensionDeviceRegistryContextRef context, GDExtensionOriginalDeviceSessionRef session, GDExtensionDeviceExitLibraryCodeHoldRef code, const GDExtensionProductionMetadataQuoteV1 *input, GDExtensionProductionAttachmentRefV1 *out, GDExtensionProductionInstallationReportV1 *report) {
	if (!aligned(a) || !abi(*a) || !aligned(input) || !production_dto_valid(*input) || !aligned(out) || !aligned(report) || !separate(out, sizeof(*out), report, sizeof(*report)) || out->registry_instance || out->handle_serial) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	MutexLock lock(mutex);
	uint32_t library = 0, device = 0;
	HolderRetirement *holder = nullptr;
	Error e = production_installation_locked(frame, producer, context, session, library, device, holder);
	if (e != OK) {
		return result(e, 16);
	}
	if (code.registry_instance != instance || code.handle_serial != holder->handle_serial) {
		return result(ERR_UNAUTHORIZED, 16);
	}
	GDExtensionProductionMetadataQuoteV1 quote = {};
	e = production_device_quote_locked(*Object::cast_to<RenderingDevice>(device_records[device].object), input->capacity, quote);
	if (e != OK) {
		return result(e, 19);
	}
	if (input->allocation_bytes != quote.allocation_bytes || input->allocation_alignment != quote.allocation_alignment || input->graph_slots != quote.graph_slots || input->graph_frames != quote.graph_frames || input->cleanup_slots != quote.cleanup_slots) {
		return result(ERR_INVALID_DATA, 19);
	}
	uint32_t slot = INVALID_SLOT;
	for (uint32_t i = 0; i < RECORD_CAPACITY; ++i) {
		if (production_attachments[i] && production_attachments[i]->library == library && production_attachments[i]->device == device) {
			return result(ERR_ALREADY_IN_USE, 16);
		}
		if (!production_attachments[i]) {
			slot = i;
		}
	}
	auto &record = records[library];
	auto &d = device_records[device];
	if (slot == INVALID_SLOT || production_serials.exhausted || frame_serials.exhausted || record.private_refs >= private_limit || d.refs == UINT32_MAX || holder->production_children == UINT32_MAX || quote.cleanup_slots > UINT32_MAX - record.cleanup_reservations || uint64_t(record.active_callbacks) + record.cleanup_reservations + quote.cleanup_slots > UINT32_MAX || quote.cleanup_slots > UINT32_MAX - record.cleanup_by_phase[uint8_t(CleanupPhase::BINDING)]) {
		return result(ERR_PARAMETER_RANGE_ERROR, 19);
	}
	void *arena = Memory::alloc_static(quote.allocation_bytes);
	if (!arena) {
		return result(ERR_OUT_OF_MEMORY, 19);
	}
	uint64_t offset = 0;
	auto allocate = [&](auto *type, uint64_t count) {
		using T = std::remove_pointer_t<decltype(type)>;
		offset += (alignof(T) - offset % alignof(T)) % alignof(T);
		T *p = reinterpret_cast<T *>(static_cast<uint8_t *>(arena) + offset);
		for (uint64_t i = 0; i < count; ++i) {
			new (&p[i]) T();
		}
		offset += count * sizeof(T);
		return p;
	};
	auto *attachment = allocate(static_cast<ProductionAttachment *>(nullptr), 1);
	attachment->requests = allocate(static_cast<ProductionRequest *>(nullptr), quote.capacity.requests);
	attachment->actions = allocate(static_cast<ProductionActionCell *>(nullptr), quote.capacity.suboperations);
	attachment->resources = allocate(static_cast<ProductionResourceSlot *>(nullptr), quote.capacity.resources);
	attachment->uses = allocate(static_cast<ProductionUseCell *>(nullptr), quote.capacity.resource_uses);
	attachment->readbacks = allocate(static_cast<ProductionReadbackCell *>(nullptr), quote.capacity.readbacks);
	attachment->graphs = allocate(static_cast<ProductionGraphCell *>(nullptr), quote.capacity.graph_memberships);
	attachment->natives = allocate(static_cast<ProductionNativeCell *>(nullptr), quote.capacity.native_memberships);
	attachment->returns = allocate(static_cast<ProductionReturnCell *>(nullptr), quote.capacity.return_cells);
	attachment->commands = allocate(static_cast<ProductionCaptureCell *>(nullptr), quote.graph_slots);
	attachment->frames = allocate(static_cast<ProductionCaptureCell *>(nullptr), quote.graph_frames);
	attachment->payload = allocate(static_cast<uint8_t *>(nullptr), quote.capacity.retained_payload_bytes);
	attachment->readback_bytes = allocate(static_cast<uint8_t *>(nullptr), quote.capacity.readback_bytes);
	CRASH_COND(offset != quote.allocation_bytes);
	production_serials.take(attachment->serial);
	attachment->library = library;
	attachment->device = device;
	attachment->epoch = d.epoch;
	attachment->producer_serial = record.producer_registration_serial;
	attachment->library_generation = record.snapshot.library_generation;
	attachment->holder = holder;
	attachment->metadata_reference = RecordRef(this, library);
	CRASH_COND(!frame_serials.take(attachment->metadata_frame));
	attachment->quote = quote;
	attachment->arena = arena;
	attachment->held_device = Object::cast_to<RenderingDevice>(d.object);
	attachment->terminal_action = holder->terminal_action;
	attachment->entity_next = attachment->held_device->production_entity_holds;
	if (attachment->entity_next) {
		attachment->entity_next->entity_previous = attachment;
	}
	attachment->held_device->production_entity_holds = attachment;
	attachment->held_device->production_entity_registry.store(this, std::memory_order_release);
	++record.private_refs;
	++d.refs;
	++holder->production_children;
	record.cleanup_reservations += uint32_t(quote.cleanup_slots);
	record.cleanup_by_phase[uint8_t(CleanupPhase::BINDING)] += uint32_t(quote.cleanup_slots);
	production_attachments[slot] = attachment;
	*out = { instance, attachment->serial };
	*report = { production_dto_abi<GDExtensionProductionInstallationReportV1>(), GDEXTENSION_PRODUCTION_INSTALL_ACCEPTED_V1, 0, quote.allocation_bytes, 0, result(OK) };
	return result(OK);
}
Error Registry::production_attachment_locked(GDExtensionProductionAttachmentRefV1 ref, bool admit, ProductionAttachment *&out, RenderingDevice *&rd) {
	out = nullptr;
	rd = nullptr;
	if (ref.registry_instance != instance || !ref.handle_serial) {
		return ERR_INVALID_DATA;
	}
	for (auto *a : production_attachments) {
		if (!a || a->serial != ref.handle_serial) {
			continue;
		}
		auto &r = records[a->library];
		auto &d = device_records[a->device];
		if (!r.occupied || r.snapshot.library_generation != a->library_generation || r.producer_registration_serial != a->producer_serial || !a->holder || a->holder->pending || a->holder->generation != a->library_generation || a->holder->handle_serial != r.producer_code_holder || !r.producer_attachment_identity) {
			return ERR_INVALID_DATA;
		}
		if (r.owner_thread != Thread::get_caller_id()) {
			return ERR_UNAUTHORIZED;
		}
		if (admit && is_production_metadata_entry()) {
			return ERR_UNAUTHORIZED;
		}
		if (admit && (a->closing || !r.attached || r.quiescing || r.close_started || r.close_error != OK || r.guard_enrollment != GuardEnrollment::QUALIFIED)) {
			return ERR_BUSY;
		}
		if (d.epoch != a->epoch) {
			return ERR_INVALID_DATA;
		}
		rd = Object::cast_to<RenderingDevice>(d.object);
		if (admit && (!d.live || !rd || rd != RenderingDevice::get_singleton() || !rd->is_main_instance)) {
			return ERR_UNAVAILABLE;
		}
		if (admit && rd->production_finalization_started.load(std::memory_order_acquire)) {
			return ERR_BUSY;
		}
		out = a;
		return OK;
	}
	return ERR_DOES_NOT_EXIST;
}
Error Registry::production_request_locked(GDExtensionProductionRequestRefV1 ref, bool admit, ProductionRequest *&out, RenderingDevice *&rd) {
	out = nullptr;
	rd = nullptr;
	if (ref.registry_instance != instance || !ref.handle_serial) {
		return ERR_INVALID_DATA;
	}
	for (auto *a : production_attachments) {
		if (!a) {
			continue;
		}
		for (uint32_t i = 0; i < a->quote.capacity.requests; ++i) {
			auto &r = a->requests[i];
			if (r.serial != ref.handle_serial) {
				continue;
			}
			ProductionAttachment *checked = nullptr;
			Error e = production_attachment_locked({ instance, a->serial }, admit, checked, rd);
			if (e != OK) {
				return e;
			}
			if (r.attachment != a || (admit && (r.cancelled || r.release_started || r.control.state != ProductionControlState::OWNER_CONSUMED))) {
				return ERR_BUSY;
			}
			out = &r;
			return OK;
		}
	}
	return ERR_DOES_NOT_EXIST;
}
Error Registry::production_resource_slot_locked(ProductionRequest &r, uint32_t index, ProductionResourceSlot *&out) {
	out = nullptr;
	if (index >= r.resource_count || !r.serial || !r.attachment) {
		return ERR_INVALID_PARAMETER;
	}
	auto &cell = r.resources[index];
	if (cell.request != &r || cell.request_serial != r.serial) {
		return ERR_INVALID_DATA;
	}
	out = &cell;
	return OK;
}
GDExtensionDeviceSessionResultV1 Registry::production_open(GDExtensionDeviceExitSceneReadyContextRef frame, GDExtensionProductionAttachmentRefV1 ref) {
	MutexLock lock(mutex);
	ProductionAttachment *a = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_attachment_locked(ref, true, a, rd);
	if (e != OK) {
		return result(e, 16);
	}
	View *view = nullptr;
	e = resolve_borrow_locked(frame.registry_instance, frame.handle_serial, ViewDomain::SCENE_READY, view);
	if (e != OK) {
		return result(e, 14);
	}
	if (view->record != a->library || !RegistrationContext::current || RegistrationContext::current->level_phase != LevelPhase::SCENE_READY || records[a->library].scene_ready_state != Record::SceneReadyState::IN_FRAME) {
		return result(ERR_UNAUTHORIZED, 14);
	}
	if (!a->record_receiver) {
		return result(ERR_UNAVAILABLE, GDEXTENSION_DEVICE_SESSION_REASON_NATIVE_UNSUPPORTED_V1);
	}
	// This opens only the installed native transport. The original logical
	// operation and Runtime record window remain the receiver owner's duty.
	a->opened = true;
	a->metadata_pending = a->metadata_receiver != nullptr;
	return result(OK);
}
Error Registry::production_reserve_metadata_locked(ProductionAttachment &a, const GDExtensionProductionOperationV1 &op, const GDExtensionProductionCapacityV1 &c, const GDExtensionProductionActionV1 *actions, uint32_t action_count, const GDExtensionProductionResourceV1 *resources, uint32_t resource_count, const GDExtensionProductionUseV1 *uses, uint32_t use_count, const GDExtensionProductionReadbackV1 *readbacks, uint32_t readback_count, ProductionRequest *&out) {
	out = nullptr;
	// The private staging caller must already have authenticated this actual
	// attachment. This method stores associations; it does not import B's scope.
	ProductionAttachment *checked = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_attachment_locked({ instance, a.serial }, true, checked, rd);
	if (e != OK || checked != &a) {
		return e == OK ? ERR_INVALID_DATA : e;
	}
	GDExtensionProductionMetadataQuoteV1 unused = {};
	e = production_layout(c, unused);
	if (e != OK) {
		return e;
	}
	if (!production_dto_valid(c) || c.requests != 1 || !capacity_fits(c, a.quote.capacity) || !production_dto_valid(op) || !op.owner_request_nonce || !op.record_generation || op.canonical_end <= op.canonical_begin || !aligned(actions) || !aligned(resources) || (use_count && !aligned(uses)) || (readback_count && !aligned(readbacks)) || !action_count || op.canonical_end - op.canonical_begin != action_count || action_count > c.suboperations || !resource_count || resource_count > c.resources || use_count > c.resource_uses || readback_count > c.readbacks) {
		return ERR_INVALID_PARAMETER;
	}
	for (uint32_t i = 0; i < resource_count; ++i) {
		const auto &r = resources[i];
		if (r.resource_kind < GDEXTENSION_PRODUCTION_STORAGE_V1 || r.resource_kind > GDEXTENSION_PRODUCTION_SET_V1 || r.ownership < GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1 || r.ownership > GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1 || !r.original_generation || (r.resource_kind == GDEXTENSION_PRODUCTION_UNIT_V1 && r.ownership != GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1) || (r.resource_kind == GDEXTENSION_PRODUCTION_STORAGE_V1 && !r.physical_bytes)) {
			return ERR_INVALID_DATA;
		}
	}
	for (uint32_t i = 0; i < use_count; ++i) {
		const auto &u = uses[i];
		if (u.resource_slot >= resource_count || u.access < GDEXTENSION_PRODUCTION_READ_V1 || u.access > GDEXTENSION_PRODUCTION_ATOMIC_V1 || (u.required_usage & ~resources[u.resource_slot].required_usage) || !range(u.byte_offset, u.byte_size, resources[u.resource_slot].physical_bytes)) {
			return ERR_INVALID_DATA;
		}
	}
	uint64_t readback_bytes = 0;
	for (uint32_t i = 0; i < readback_count; ++i) {
		const auto &r = readbacks[i];
		if (r.reserved || r.resource_slot >= resource_count || resources[r.resource_slot].resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || !r.byte_size || !range(r.byte_offset, r.byte_size, resources[r.resource_slot].physical_bytes) || r.canonical_producer < op.canonical_begin || r.canonical_producer >= r.canonical_copy || r.canonical_copy >= op.canonical_end || r.byte_size > c.readback_bytes - readback_bytes) {
			return ERR_INVALID_DATA;
		}
		readback_bytes += r.byte_size;
	}
	for (uint32_t i = 0; i < action_count; ++i) {
		const auto &v = actions[i];
		if (v.canonical_suboperation != op.canonical_begin + i || (i && v.canonical_suboperation <= actions[i - 1].canonical_suboperation) || v.action_kind < GDEXTENSION_PRODUCTION_DISPATCH_V1 || v.action_kind > GDEXTENSION_PRODUCTION_READBACK_V1 || v.list_boundary > GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1 || v.first_use > use_count || v.use_count > use_count - v.first_use) {
			return ERR_INVALID_DATA;
		}
		if (v.action_kind == GDEXTENSION_PRODUCTION_DISPATCH_V1 && (v.physical_unit >= resource_count || resources[v.physical_unit].resource_kind != GDEXTENSION_PRODUCTION_UNIT_V1)) {
			return ERR_INVALID_DATA;
		}
		if (v.action_kind == GDEXTENSION_PRODUCTION_READBACK_V1 && (v.readback_slot >= readback_count || readbacks[v.readback_slot].canonical_copy != v.canonical_suboperation)) {
			return ERR_INVALID_DATA;
		}
	}
	for (uint32_t i = 0; i < readback_count; ++i) {
		bool producer = false, copy = false;
		for (uint32_t j = 0; j < action_count; ++j) {
			producer |= actions[j].canonical_suboperation == readbacks[i].canonical_producer;
			copy |= actions[j].canonical_suboperation == readbacks[i].canonical_copy && actions[j].action_kind == GDEXTENSION_PRODUCTION_READBACK_V1 && actions[j].readback_slot == i;
		}
		if (!producer || !copy) {
			return ERR_INVALID_DATA;
		}
	}
	// Check this request's own slice, not just the accepted attachment pool.
	// Distinct declared storage uses bound exact worker dependencies per action.
	uint32_t worker_bound = 0;
	for (uint32_t i = 0; i < action_count; ++i) {
		const auto &action = actions[i];
		uint32_t storages = 0;
		for (uint32_t u = 0; u < action.use_count; ++u) {
			const uint32_t slot = uses[action.first_use + u].resource_slot;
			if (resources[slot].resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1) {
				continue;
			}
			bool duplicate = false;
			for (uint32_t before = 0; before < u; ++before) {
				duplicate |= uses[action.first_use + before].resource_slot == slot;
			}
			storages += !duplicate;
		}
		worker_bound = MAX(worker_bound, storages);
	}
	if (!rd || !rd->_thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	{
		RdUnlock unlock{ rd->_thread_safe_ };
		worker_bound = MIN(worker_bound, rd->transfer_worker_pool_max_size);
		uint32_t memberships = 0;
		e = production_membership_bound_locked(*rd, c.graph_memberships, worker_bound, memberships);
		if (e != OK) {
			return e;
		}
		if (memberships > c.native_memberships) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
	}
	ProductionRequest *r = nullptr;
	for (uint32_t i = 0; i < a.quote.capacity.requests; ++i) {
		auto &candidate = a.requests[i];
		if (candidate.serial && candidate.operation.owner_request_nonce == op.owner_request_nonce && candidate.operation.record_generation == op.record_generation) {
			return ERR_ALREADY_IN_USE;
		}
		if (!candidate.serial) {
			r = &candidate;
		}
	}
	if (!r || production_serials.exhausted) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	auto free_cells = [](auto *cells, uint32_t total, uint32_t count) {
		using T = std::remove_pointer_t<decltype(cells)>;
		if (!count) {
			return cells;
		}
		uint32_t run = 0;
		for (uint32_t i = 0; i < total; ++i) {
			run = cells[i].request ? 0 : run + 1;
			if (run == count) {
				return &cells[i + 1 - count];
			}
		}
		return static_cast<T *>(nullptr);
	};
	auto *ac = free_cells(a.actions, a.quote.capacity.suboperations, c.suboperations);
	auto *rc = free_cells(a.resources, a.quote.capacity.resources, c.resources);
	auto *uc = free_cells(a.uses, a.quote.capacity.resource_uses, c.resource_uses);
	auto *bc = free_cells(a.readbacks, a.quote.capacity.readbacks, c.readbacks);
	auto *gc = free_cells(a.graphs, a.quote.capacity.graph_memberships, c.graph_memberships);
	auto *nc = free_cells(a.natives, a.quote.capacity.native_memberships, c.native_memberships);
	auto *tc = free_cells(a.returns, a.quote.capacity.return_cells, c.return_cells);
	GDExtensionProductionMetadataQuoteV1 request_quote = {};
	e = production_device_quote_locked(*rd, c, request_quote);
	if (e != OK) {
		return e;
	}
	auto *cc = free_cells(a.commands, uint32_t(a.quote.graph_slots), uint32_t(request_quote.graph_slots));
	auto *fc = free_cells(a.frames, uint32_t(a.quote.graph_frames), uint32_t(request_quote.graph_frames));
	if (!ac || !rc || !uc || !bc || !gc || !nc || !tc || !cc || !fc) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	auto free_bytes = [&](uint64_t bytes, uint64_t total, bool readback, uint64_t &offset) {
		offset = 0;
		for (;;) {
			if (!range(offset, bytes, total)) {
				return false;
			}
			bool moved = false;
			for (uint32_t i = 0; i < a.quote.capacity.requests; ++i) {
				const auto &q = a.requests[i];
				if (!q.serial) {
					continue;
				}
				uint64_t start = readback ? q.readback_offset : q.payload_offset, size = readback ? q.capacity.readback_bytes : q.capacity.retained_payload_bytes;
				if (bytes && size && offset < start + size && start < offset + bytes) {
					offset = start + size;
					moved = true;
					break;
				}
			}
			if (!moved) {
				return true;
			}
		}
	};
	uint64_t payload = 0, readback = 0;
	if (!free_bytes(c.retained_payload_bytes, a.quote.capacity.retained_payload_bytes, false, payload) || !free_bytes(c.readback_bytes, a.quote.capacity.readback_bytes, true, readback)) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	// All validation and every finite slice succeed before publishing any cell.
	production_serials.take(r->serial);
	r->attachment = &a;
	r->operation = op;
	r->capacity = c;
	r->native_workers_bound = worker_bound;
	r->actions = ac;
	r->resources = rc;
	r->uses = uc;
	r->readbacks = bc;
	r->graphs = gc;
	r->natives = nc;
	r->returns = tc;
	r->commands = cc;
	r->frames = fc;
	r->command_count = uint32_t(request_quote.graph_slots);
	r->frame_count = uint32_t(request_quote.graph_frames);
	r->action_count = action_count;
	r->resource_count = resource_count;
	r->use_count = use_count;
	r->readback_count = readback_count;
	r->payload_offset = payload;
	r->readback_offset = readback;
	auto claim = [&](auto *cells, uint32_t count) { for(uint32_t i=0;i<count;++i){cells[i].request=r;
} };
	claim(ac, c.suboperations);
	claim(rc, c.resources);
	claim(uc, c.resource_uses);
	claim(bc, c.readbacks);
	claim(gc, c.graph_memberships);
	claim(nc, c.native_memberships);
	claim(tc, c.return_cells);
	claim(cc, r->command_count);
	claim(fc, r->frame_count);
	for (uint32_t i = 0; i < action_count; ++i) {
		ac[i].declaration = actions[i];
	}
	for (uint32_t i = 0; i < resource_count; ++i) {
		rc[i].declaration = resources[i];
		rc[i].request_serial = r->serial;
	}
	for (uint32_t i = 0; i < use_count; ++i) {
		uc[i].declaration = uses[i];
	}
	uint64_t rb = readback;
	for (uint32_t i = 0; i < readback_count; ++i) {
		bc[i].declaration = readbacks[i];
		bc[i].bytes_offset = rb;
		bc[i].capture.state = &bc[i].source;
		bc[i].capture.registry = this;
		bc[i].capture.request_serial = r->serial;
		bc[i].capture.bytes = a.readback_bytes + rb;
		bc[i].capture.byte_capacity = readbacks[i].byte_size;
		bc[i].source.request_serial = r->serial;
		rb += readbacks[i].byte_size;
	}
	++a.children;
	out = r;
	return OK;
}
Error Registry::production_action_locked(ProductionRequest &r, uint64_t canonical, uint32_t kind, ProductionActionCell *&out) {
	out = nullptr;
	if (r.cancelled || r.frozen) {
		return ERR_BUSY;
	}
	for (uint32_t i = 0; i < r.action_count; ++i) {
		auto &a = r.actions[i];
		if (a.declaration.canonical_suboperation != canonical) {
			continue;
		}
		if (a.request != &r || a.declaration.action_kind != kind) {
			return ERR_INVALID_DATA;
		}
		out = &a;
		return OK;
	}
	return ERR_DOES_NOT_EXIST;
}
bool Registry::production_use_matches_locked(const ProductionRequest &r, const ProductionActionCell &a, uint32_t slot, uint64_t offset, uint64_t bytes, uint32_t access) const {
	const auto &d = a.declaration;
	if (d.first_use > r.use_count || d.use_count > r.use_count - d.first_use) {
		return false;
	}
	for (uint32_t i = 0; i < d.use_count; ++i) {
		const auto &cell = r.uses[d.first_use + i];
		const auto &u = cell.declaration;
		if (cell.request != &r || u.resource_slot != slot || (u.access != access && u.access != GDEXTENSION_PRODUCTION_READ_WRITE_V1) || offset < u.byte_offset) {
			continue;
		}
		if (range(offset - u.byte_offset, bytes, u.byte_size)) {
			return true;
		}
	}
	return false;
}
Error Registry::production_freeze_locked(ProductionRequest &r) {
	if (r.cancelled) {
		return ERR_BUSY;
	}
	if (r.frozen) {
		return OK;
	}
	for (uint32_t i = 0; i < r.action_count; ++i) {
		if (!r.actions[i].prepared) {
			return ERR_UNCONFIGURED;
		}
	}
	for (uint32_t i = 0; i < r.resource_count; ++i) {
		if (!r.resources[i].imported) {
			return ERR_UNCONFIGURED;
		}
	}
	r.frozen = true;
	return OK;
}
GDExtensionDeviceSessionResultV1 Registry::production_prepare_dispatch(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionDispatchV1 *input, const void *push, uint32_t bytes) {
	if (!aligned(input) || !production_dto_valid(*input) || input->reserved || input->push_bytes != bytes || (bytes && !push) || !input->groups_x || !input->groups_y || !input->groups_z) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	const auto value = *input;
	MutexLock lock(mutex);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_request_locked(ref, true, r, rd);
	if (e != OK) {
		return result(e, 16);
	}
	ProductionActionCell *a = nullptr;
	e = production_action_locked(*r, value.canonical_suboperation, GDEXTENSION_PRODUCTION_DISPATCH_V1, a);
	if (e != OK) {
		return result(e, 16);
	}
	if (value.unit_slot >= r->resource_count || value.unit_slot != a->declaration.physical_unit || r->resources[value.unit_slot].declaration.resource_kind != GDEXTENSION_PRODUCTION_UNIT_V1 || value.first_set_slot > r->resource_count || value.set_count > r->resource_count - value.first_set_slot) {
		return result(ERR_INVALID_DATA, 2);
	}
	for (uint32_t i = 0; i < value.set_count; ++i) {
		if (r->resources[value.first_set_slot + i].declaration.resource_kind != GDEXTENSION_PRODUCTION_SET_V1) {
			return result(ERR_INVALID_DATA, 2);
		}
	}
	if (!rd->_thread_safe_.try_lock()) {
		return result(ERR_BUSY, 16);
	}
	RdUnlock unlock{ rd->_thread_safe_ };
	if (bytes > rd->limit_get(RD::LIMIT_MAX_PUSH_CONSTANT_SIZE) || bytes > RenderingDevice::MAX_PUSH_CONSTANT_SIZE || value.groups_x > rd->limit_get(RD::LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X) || value.groups_y > rd->limit_get(RD::LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Y) || value.groups_z > rd->limit_get(RD::LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Z)) {
		return result(ERR_PARAMETER_RANGE_ERROR, 19);
	}
	e = production_dispatch_closure_locked(*r, *rd, *a, value);
	if (e != OK) {
		return result(e, 2);
	}
	if (a->prepared) {
		const auto &v = a->dispatch;
		bool same = v.unit_slot == value.unit_slot && v.first_set_slot == value.first_set_slot && v.set_count == value.set_count && v.groups_x == value.groups_x && v.groups_y == value.groups_y && v.groups_z == value.groups_z && v.push_bytes == bytes && (!bytes || !std::memcmp(r->attachment->payload + a->payload_offset, push, bytes));
		return result(same ? OK : ERR_INVALID_DATA, same ? 0 : 2);
	}
	if (!range(r->payload_used, bytes, r->capacity.retained_payload_bytes)) {
		return result(ERR_PARAMETER_RANGE_ERROR, 19);
	}
	a->payload_offset = r->payload_offset + r->payload_used;
	a->payload_bytes = bytes;
	if (bytes) {
		std::memcpy(r->attachment->payload + a->payload_offset, push, bytes);
	}
	r->payload_used += bytes;
	a->dispatch = value;
	a->prepared = true;
	return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::production_prepare_copy(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionCopyV1 *input) {
	if (!aligned(input) || !production_dto_valid(*input) || !input->bytes) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	const auto value = *input;
	MutexLock lock(mutex);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_request_locked(ref, true, r, rd);
	if (e != OK) {
		return result(e, 16);
	}
	ProductionActionCell *a = nullptr;
	e = production_action_locked(*r, value.canonical_suboperation, GDEXTENSION_PRODUCTION_COPY_V1, a);
	if (e != OK) {
		return result(e, 16);
	}
	if (value.source_slot >= r->resource_count || value.destination_slot >= r->resource_count) {
		return result(ERR_INVALID_DATA, 2);
	}
	const auto &source = r->resources[value.source_slot].declaration, &dest = r->resources[value.destination_slot].declaration;
	if (source.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || dest.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || !range(value.source_offset, value.bytes, source.physical_bytes) || !range(value.destination_offset, value.bytes, dest.physical_bytes)) {
		return result(ERR_INVALID_DATA, 2);
	}
	if (!production_use_matches_locked(*r, *a, value.source_slot, value.source_offset, value.bytes, GDEXTENSION_PRODUCTION_READ_V1) || !production_use_matches_locked(*r, *a, value.destination_slot, value.destination_offset, value.bytes, GDEXTENSION_PRODUCTION_WRITE_V1)) {
		return result(ERR_INVALID_DATA, 2);
	}
	if (value.source_slot == value.destination_slot && value.source_offset < value.destination_offset + value.bytes && value.destination_offset < value.source_offset + value.bytes) {
		return result(ERR_INVALID_DATA, 2);
	}
	if (a->prepared) {
		const auto &v = a->copy;
		return result(v.source_slot == value.source_slot && v.destination_slot == value.destination_slot && v.source_offset == value.source_offset && v.destination_offset == value.destination_offset && v.bytes == value.bytes ? OK : ERR_INVALID_DATA);
	}
	a->copy = value;
	a->prepared = true;
	return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::production_prepare_upload(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionUploadV1 *input, const void *data, uint64_t bytes) {
	if (!aligned(input) || !production_dto_valid(*input) || input->reserved || !bytes || !data || bytes != input->bytes || bytes > SIZE_MAX) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	const auto value = *input;
	MutexLock lock(mutex);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_request_locked(ref, true, r, rd);
	if (e != OK) {
		return result(e, 16);
	}
	return production_prepare_upload_locked(*r, value, data, bytes);
}
GDExtensionDeviceSessionResultV1 Registry::production_prepare_upload_locked(ProductionRequest &request, const GDExtensionProductionUploadV1 &value, const void *data, uint64_t bytes) {
	ProductionRequest *r = &request;
	if (!production_dto_valid(value) || value.reserved || !bytes || !data || bytes != value.bytes || bytes > SIZE_MAX) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	ProductionActionCell *a = nullptr;
	Error e = production_action_locked(*r, value.canonical_suboperation, GDEXTENSION_PRODUCTION_UPLOAD_V1, a);
	if (e != OK) {
		return result(e, 16);
	}
	if (value.destination_slot >= r->resource_count || r->resources[value.destination_slot].declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || !range(value.destination_offset, bytes, r->resources[value.destination_slot].declaration.physical_bytes)) {
		return result(ERR_INVALID_DATA, 2);
	}
	if (!production_use_matches_locked(*r, *a, value.destination_slot, value.destination_offset, bytes, GDEXTENSION_PRODUCTION_WRITE_V1)) {
		return result(ERR_INVALID_DATA, 2);
	}
	if (a->prepared) {
		const auto &v = a->upload;
		return result(v.destination_slot == value.destination_slot && v.destination_offset == value.destination_offset && v.bytes == bytes && !std::memcmp(r->attachment->payload + a->payload_offset, data, size_t(bytes)) ? OK : ERR_INVALID_DATA);
	}
	if (!range(r->payload_used, bytes, r->capacity.retained_payload_bytes)) {
		return result(ERR_PARAMETER_RANGE_ERROR, 19);
	}
	a->payload_offset = r->payload_offset + r->payload_used;
	a->payload_bytes = bytes;
	std::memcpy(r->attachment->payload + a->payload_offset, data, size_t(bytes));
	r->payload_used += bytes;
	a->upload = value;
	a->prepared = true;
	return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::production_prepare_readback(GDExtensionProductionRequestRefV1 ref, uint64_t canonical, uint32_t slot) {
	MutexLock lock(mutex);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_request_locked(ref, true, r, rd);
	if (e != OK) {
		return result(e, 16);
	}
	ProductionActionCell *a = nullptr;
	e = production_action_locked(*r, canonical, GDEXTENSION_PRODUCTION_READBACK_V1, a);
	if (e != OK) {
		return result(e, 16);
	}
	if (slot >= r->readback_count || a->declaration.readback_slot != slot || r->readbacks[slot].declaration.canonical_copy != canonical) {
		return result(ERR_INVALID_DATA, 2);
	}
	const auto &readback = r->readbacks[slot].declaration;
	if (!production_use_matches_locked(*r, *a, readback.resource_slot, readback.byte_offset, readback.byte_size, GDEXTENSION_PRODUCTION_READ_V1)) {
		return result(ERR_INVALID_DATA, 2);
	}
	if (a->prepared) {
		return result(a->readback_slot == slot ? OK : ERR_INVALID_DATA);
	}
	a->readback_slot = slot;
	a->prepared = true;
	return result(OK);
}
Error Registry::production_observe_service(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionServiceBudgetV1 &budget) {
	// Zero query has no queue admission, ref retention, or native service.
	if (!budget.controls || !budget.receipts || !budget.passes || !budget.deadline_micros) {
		return OK;
	}
	uint64_t serial = 0;
	bool returned = false, same_budget = false;
	{
		MutexLock lock(mutex);
		ProductionRequest *request = nullptr;
		RenderingDevice *rd = nullptr;
		const Error error = production_request_locked(ref, false, request, rd);
		if (error != OK) {
			return error;
		}
		const auto &cell = request->control;
		if (cell.state != ProductionControlState::OWNER_CONSUMED) {
			if (cell.input.kind != ProductionControlKind::OBSERVE) {
				return ERR_BUSY;
			}
			serial = cell.serial;
			returned = cell.state == ProductionControlState::RETURNED;
			same_budget = same_service_budget(cell.input.budget, budget);
		}
	}
	if (returned) {
		ProductionControlOutput output = {};
		const Error error = production_control_consume(ref, serial, &output);
		if (error != OK) {
			return error;
		}
		if (same_budget) {
			return Error(output.result.native_error);
		}
		// The old immutable turn is consumed even when its deadline expired or
		// the next reduction differs. A fresh turn uses the same fixed cell.
		serial = 0;
	}
	// The caller's current reduction gates publication even for an older
	// retained PENDING turn. Query/expired calls preserve that immutable cell.
	if (!service_budget_active(budget)) {
		return OK;
	}
	if (!serial) {
		const Error error = production_control_accept(ref, ProductionControlKind::OBSERVE, &serial, &budget);
		if (error != OK) {
			return error;
		}
	}
	// A live caller reduction may retry the retained immutable pending turn.
	// Even owner == RT publishes to the original FIFO and never executes inline.
	const Error scheduled = production_control_schedule(ref, serial);
	if (scheduled != OK) {
		return scheduled;
	}
	return ERR_BUSY;
}

GDExtensionDeviceSessionResultV1 Registry::production_observe(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionServiceBudgetV1 *budget, GDExtensionProductionReportV1 *out) {
	if (!aligned(budget) || !production_dto_valid(*budget) || budget->reserved || !aligned(out)) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	const Error service = production_observe_service(ref, *budget);
	MutexLock lock(mutex);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_request_locked(ref, false, r, rd);
	if (e != OK) {
		return result(e, 16);
	}
	production_report_locked(*r, *out);
	return result(service);
}
void Registry::production_report_locked(const ProductionRequest &r, GDExtensionProductionReportV1 &report) const {
	report = {};
	report.abi = production_dto_abi<GDExtensionProductionReportV1>();
	report.operation = r.operation;
	report.revision = r.revision;
	if (r.capture_inflight) {
		report.cause = result(ERR_BUSY, 16);
		return;
	}
	// Recording/segment/ACK frontiers remain owned by the original record window.
	report.recorded_end = r.operation.canonical_begin + r.next_action;
	report.captured_segment = r.segment.segment;
	report.acknowledged_segment = r.acknowledged_segment;
	report.cancelled = r.cancelled;
	report.possible_obligations = uint32_t(r.effects_pending) + uint32_t(r.source_device_lost);
	if (r.graph_count > r.capacity.graph_memberships || r.native_count > r.capacity.native_memberships || uint64_t(r.graph_count) + r.native_count > UINT32_MAX) {
		report.cause = result(ERR_INVALID_DATA, 16);
		return;
	}
	report.fact_count = r.graph_count + r.native_count;
	for (uint32_t i = 0; i < r.resource_count; ++i) {
		report.possible_obligations += r.resources[i].transfer_pending;
	}
	Error cause = OK;
	for (uint32_t i = 0; i < r.graph_count; ++i) {
		if (!r.graphs || r.graphs[i].request != &r) {
			report.cause = result(ERR_INVALID_DATA, 16);
			return;
		}
		report.possible_obligations += r.graphs[i].pending;
	}
	for (uint32_t i = 0; i < r.native_count; ++i) {
		const ProductionNativeCell *owner = nullptr;
		const Error error = production_native_owner_locked(r, i, owner);
		if (error != OK) {
			report.cause = result(error, 16);
			return;
		}
		if (r.natives[i].owner_index != i) {
			continue;
		}
		const bool complete = owner->observation.state == NativeSourceObservation::State::Complete;
		report.known_obligations += owner->member.source.issue == RDD::IssueFact::KnownAccepted || complete;
		report.possible_obligations += owner->member.source.issue == RDD::IssueFact::IssuedUnknown && !complete;
		report.complete_obligations += complete;
		if (cause == OK) {
			cause = owner->member.source.error != OK ? owner->member.source.error : owner->observation.completion_error != OK ? owner->observation.completion_error
					: owner->member.retention_error != OK																	  ? owner->member.retention_error
																															  : owner->release_error;
		}
	}
	for (uint32_t i = 0; i < r.capacity.return_cells; ++i) {
		report.physical_returns_pending += r.returns[i].pending;
	}
	for (uint32_t i = 0; i < r.readback_count; ++i) {
		const auto &readback = r.readbacks[i];
		if (readback.source.issued) {
			report.possible_obligations += readback.source.returned != readback.source.issued;
			if (cause == OK && readback.source.error != OK) {
				cause = readback.source.error;
			}
			if (readback.declaration.resource_slot < r.resource_count) {
				const auto &resource = r.resources[readback.declaration.resource_slot];
				report.ready_readbacks += resource.imported && same_request_readback_complete(readback.source, r.serial, resource.backing_serial, readback.declaration.byte_offset, readback.declaration.byte_size);
			}
		}
	}
	if (cause == OK && r.source_device_lost) {
		cause = ERR_UNAVAILABLE;
	}
	if (cause == OK && (report.possible_obligations || report.physical_returns_pending || report.known_obligations != report.complete_obligations)) {
		cause = ERR_BUSY;
	}
	report.cause = result(cause);
}
Error Registry::production_native_owner_locked(const ProductionRequest &r, uint32_t index, const ProductionNativeCell *&out) const {
	out = nullptr;
	if (r.native_count > r.capacity.native_memberships || r.graph_count > r.capacity.graph_memberships || index >= r.native_count || !r.natives) {
		return ERR_INVALID_DATA;
	}
	const auto &cell = r.natives[index];
	if (cell.request != &r || cell.owner_index > index || cell.owner_index >= r.native_count) {
		return ERR_INVALID_DATA;
	}
	const auto &owner = r.natives[cell.owner_index];
	if (owner.request != &r || owner.owner_index != cell.owner_index || !same_native_source(cell.member.source, owner.member.source) ||
			(cell.owner_index != index && (cell.member.retained || cell.member.owner_released))) {
		return ERR_INVALID_DATA;
	}
	if (cell.fact.membership_parent != UINT32_MAX && (cell.fact.membership_parent >= r.graph_count || !r.graphs || r.graphs[cell.fact.membership_parent].request != &r)) {
		return ERR_INVALID_DATA;
	}
	out = &owner;
	return OK;
}

Error Registry::production_native_fact_locked(const ProductionRequest &r, uint32_t index, GDExtensionProductionFactV1 &out) const {
	const ProductionNativeCell *owner = nullptr;
	Error error = production_native_owner_locked(r, index, owner);
	if (error != OK) {
		return error;
	}
	const auto &member = r.natives[index].member;
	out = r.natives[index].fact;
	out.native_serial = member.source.serial;
	out.completion_serial = member.source.completion_serial;
	out.backend_queue = member.source.queue_index;
	out.issue = uint32_t(member.source.issue); // Immutable original issue classification.
	out.issue_error = member.source.error;
	out.issue_native_code = member.source.native_code;
	out.retention_error = owner->member.retention_error;
	out.completion_native_code = owner->observation.completion_native_code;
	// Completion failure and issue failure have separate meanings. In particular
	// a quiet poll never turns the original issue error into a completion error.
	out.completion = member.source.issue == RDD::IssueFact::NotIssued ? 0 : owner->observation.state == NativeSourceObservation::State::Complete ? 2
			: owner->observation.completion_error != OK																							 ? 3
			: owner->observation.state == NativeSourceObservation::State::Unknown || owner->member.retention_error != OK						 ? 4
																																				 : 1;
	return OK;
}

Error Registry::production_poll_native_locked(ProductionRequest &r, RDD &driver, const GDExtensionProductionServiceBudgetV1 &budget, bool live, ProductionServiceUsage &used) {
	used = {};
	// These are caller reductions on already retained Core maintenance, not B's
	// missing installed owner-turn grant. Zero query does not even retain a ref.
	if (!service_budget_active(budget) || !r.native_count) {
		return OK;
	}
	if (r.native_count > r.capacity.native_memberships || !r.natives) {
		return ERR_INVALID_DATA;
	}
	if (r.revision > UINT64_MAX - uint64_t(r.capacity.graph_memberships) * 2 - uint64_t(r.capacity.native_memberships) * 2 - r.capacity.readbacks - r.capacity.resources - 8) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	used.controls = 1;
	if (!live) {
		if (!r.source_device_lost) {
			r.source_device_lost = true;
			++r.revision;
		}
		return ERR_UNAVAILABLE;
	}
	if (r.source_device_lost) {
		return ERR_UNAVAILABLE;
	}
	const auto origin = NativeSourceOrigin::production();
	// Fixed Core ceiling; caller values only reduce this maintenance turn.
	const uint32_t visits = MIN(r.native_count, 64u);
	uint32_t scanned = 0;
	const uint32_t receipts = MIN(budget.receipts, 128u);
	while (scanned < visits && used.passes < MIN(budget.passes, 64u) && used.receipts < receipts && before_service_deadline(budget)) {
		if (r.revision > UINT64_MAX - uint64_t(r.capacity.graph_memberships) * 2 - uint64_t(r.capacity.native_memberships) * 2 - r.capacity.readbacks - r.capacity.resources - 8) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
		const uint32_t index = r.observe_cursor % r.native_count;
		const ProductionNativeCell *owner = nullptr;
		Error error = production_native_owner_locked(r, index, owner);
		if (error != OK) {
			return error;
		}
		++scanned;
		r.observe_cursor = (index + 1) % r.native_count;
		auto &cell = r.natives[index];
		if (cell.owner_index != index || cell.member.owner_released || cell.member.source.issue == RDD::IssueFact::NotIssued || cell.observation.state == NativeSourceObservation::State::Complete) {
			continue;
		}
		// Terminal and alias rows consume only the fixed metadata scan bound.
		// Preserve this turn's service pass for actual observation or disposal.
		++used.passes;
		const auto old = cell.observation;
		const bool retained_before = cell.member.retained;
		const Error retention_before = cell.member.retention_error;
		const uint64_t request_serial = r.serial, control_serial = r.control.serial;
		auto *attachment = r.attachment;
		auto local_member = cell.member;
		auto local_observation = cell.observation;
		// EXECUTING prevents close/release/reuse of this fixed control and its
		// request. The existing entity edge and held RD lock retain the device.
		// Driver poll can synchronously notify this Registry of device loss:
		// never call it while holding the non-recursive Registry mutex.
		struct UnlockRegistry {
			BinaryMutex &mutex;
			explicit UnlockRegistry(BinaryMutex &p_mutex) : mutex(p_mutex) {
				mutex.unlock();
			}
			~UnlockRegistry() { mutex.lock(); }
		};
		auto validate_return = [&]() {
			CRASH_COND(r.serial != request_serial || r.attachment != attachment ||
					r.control.serial != control_serial ||
					r.control.state != ProductionControlState::EXECUTING ||
					cell.request != &r || cell.owner_index != index ||
					!same_native_source(cell.member.source, local_member.source));
		};
		if (!local_member.retained && before_service_deadline(budget)) {
			const bool driver_called = retainable_native_source(local_member.source);
			Error retained;
			{
				UnlockRegistry unlock(mutex);
				retained = retain_native_source(driver, local_member);
			}
			validate_return();
			used.receipts += driver_called ? 1 : 0;
			if (retained != OK) {
				local_observation.retention_error = retained;
				local_observation.error =
						retained == ERR_BUSY || local_member.source.error == OK
						? retained
						: local_member.source.error;
				local_observation.native_code = local_member.source.error == OK
						? 0
						: local_member.source.native_code;
				local_observation.state = retained == ERR_BUSY
						? NativeSourceObservation::State::Pending
						: NativeSourceObservation::State::Unknown;
			}
		}
		if (device_records[attachment->device].live && local_member.retained &&
				used.receipts < receipts && before_service_deadline(budget)) {
			++used.receipts;
			{
				UnlockRegistry unlock(mutex);
				// Already retained: exactly one original poll, no hidden retain.
				local_observation = poll_native_source(driver, local_member, origin);
			}
			validate_return();
		}
		const bool lost = !device_records[attachment->device].live;
		if (lost) {
			r.source_device_lost = true;
			local_observation.state = NativeSourceObservation::State::Unknown;
			if (local_observation.error == OK) {
				local_observation.error = ERR_UNAVAILABLE;
			}
		}
		// Account any actual retained reference even if its own callback marked
		// the device lost. No identity check or loss may silently drop that ref.
		cell.member = local_member;
		cell.observation = local_observation;
		if (lost || retained_before != cell.member.retained ||
				retention_before != cell.member.retention_error ||
				!same_native_observation(old, cell.observation)) {
			++r.revision;
		}
		if (lost) {
			return ERR_UNAVAILABLE;
		}
	}
	return OK;
}

GDExtensionDeviceSessionResultV1 Registry::production_facts(GDExtensionProductionRequestRefV1 ref, uint64_t revision, uint32_t first, GDExtensionProductionFactV1 *out, uint32_t capacity, uint32_t *written) {
	if (!aligned(written) || (capacity && (!aligned(out) || uint64_t(capacity) * sizeof(*out) > SIZE_MAX || !separate(out, size_t(capacity) * sizeof(*out), written, sizeof(*written))))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	MutexLock lock(mutex);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_request_locked(ref, false, r, rd);
	if (error != OK) {
		return result(error, 16);
	}
	if (r->capture_inflight) {
		return result(ERR_BUSY, 16);
	}
	if (revision != r->revision) {
		return result(ERR_DOES_NOT_EXIST, 16);
	}
	if (r->graph_count > r->capacity.graph_memberships || r->native_count > r->capacity.native_memberships || uint64_t(r->graph_count) + r->native_count > UINT32_MAX) {
		return result(ERR_INVALID_DATA, 16);
	}
	const uint32_t total = r->graph_count + r->native_count;
	if (first > total) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	const uint32_t count = MIN(capacity, total - first);
	// Prevalidate the entire requested page, so errors do not partially publish.
	for (uint32_t i = first; i < first + count; ++i) {
		if (i < r->graph_count) {
			if (!r->graphs || r->graphs[i].request != r) {
				return result(ERR_INVALID_DATA, 16);
			}
		} else {
			const ProductionNativeCell *owner = nullptr;
			error = production_native_owner_locked(*r, i - r->graph_count, owner);
			if (error != OK) {
				return result(error, 16);
			}
		}
	}
	for (uint32_t i = 0; i < count; ++i) {
		const uint32_t index = first + i;
		if (index < r->graph_count) {
			out[i] = r->graphs[index].fact;
		} else {
			production_native_fact_locked(*r, index - r->graph_count, out[i]);
		}
	}
	*written = count;
	return result(OK);
}

GDExtensionDeviceSessionResultV1 Registry::production_copy(GDExtensionProductionRequestRefV1 ref, uint64_t revision, uint32_t slot, void *destination, uint64_t capacity, uint64_t *written) {
	if (!aligned(written) || capacity > SIZE_MAX || (capacity && (!destination || !separate(destination, size_t(capacity), written, sizeof(*written))))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	MutexLock lock(mutex);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_request_locked(ref, false, request, rd);
	if (error != OK) {
		return result(error, 16);
	}
	if (request->capture_inflight) {
		return result(ERR_BUSY, 16);
	}
	if (revision != request->revision) {
		return result(ERR_DOES_NOT_EXIST, 16);
	}
	if (slot >= request->readback_count) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	const auto &cell = request->readbacks[slot];
	const auto &declaration = cell.declaration;
	if (cell.request != request || declaration.resource_slot >= request->resource_count) {
		return result(ERR_INVALID_DATA, 16);
	}
	const auto &resource = request->resources[declaration.resource_slot];
	if (!resource.imported || !same_request_readback_complete(cell.source, request->serial, resource.backing_serial, declaration.byte_offset, declaration.byte_size)) {
		return result(cell.source.error != OK ? cell.source.error : ERR_BUSY, 16);
	}
	bool covered = false;
	for (uint32_t g = 0; g < request->graph_count; ++g) {
		const auto &graph = request->graphs[g];
		if (graph.fact.canonical_suboperation != declaration.canonical_copy) {
			continue;
		}
		if (graph.pending || graph.fact.issue_error != OK) {
			return result(ERR_BUSY, 16);
		}
		bool issued = false;
		for (uint32_t n = 0; n < request->native_count; ++n) {
			if (request->natives[n].fact.membership_parent != g) {
				continue;
			}
			const ProductionNativeCell *owner = nullptr;
			if (production_native_owner_locked(*request, n, owner) != OK || owner->observation.state != NativeSourceObservation::State::Complete) {
				return result(ERR_BUSY, 16);
			}
			issued = true;
		}
		if (!issued) {
			return result(ERR_BUSY, 16);
		}
		covered = true;
	}
	if (!covered) {
		return result(ERR_BUSY, 16);
	}
	if (capacity < declaration.byte_size) {
		return result(ERR_PARAMETER_RANGE_ERROR, 19);
	}
	// Byte coverage and source association must both be exact. No native call,
	// progress, or implicit readback is permitted in this observation method.
	if (!range(cell.bytes_offset, declaration.byte_size, request->attachment->quote.capacity.readback_bytes) || cell.bytes_offset < request->readback_offset || !range(cell.bytes_offset - request->readback_offset, declaration.byte_size, request->capacity.readback_bytes) || !destination) {
		return result(ERR_INVALID_DATA, 16);
	}
	if (!separate(destination, size_t(declaration.byte_size), request->attachment->arena, size_t(request->attachment->quote.allocation_bytes)) || !separate(written, sizeof(*written), request->attachment->arena, size_t(request->attachment->quote.allocation_bytes))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	std::memcpy(destination, request->attachment->readback_bytes + cell.bytes_offset, size_t(declaration.byte_size));
	*written = declaration.byte_size;
	return result(OK);
}

GDExtensionDeviceSessionResultV1 Registry::production_cancel(GDExtensionProductionRequestRefV1 ref) {
	MutexLock lock(mutex);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_request_locked(ref, false, r, rd);
	if (e != OK) {
		return result(e, 16);
	}
	if (r->control.state != ProductionControlState::OWNER_CONSUMED) {
		return result(ERR_BUSY, 16);
	}
	return production_cancel_locked(*r);
}
GDExtensionDeviceSessionResultV1 Registry::production_cancel_locked(ProductionRequest &request) {
	auto *r = &request;
	if (!r->cancelled) {
		if (r->revision == UINT64_MAX) {
			return result(ERR_PARAMETER_RANGE_ERROR, 19);
		}
		r->cancelled = true;
		++r->revision;
	}
	return result(r->effects_pending ? ERR_BUSY : OK);
}
GDExtensionDeviceSessionResultV1 Registry::production_release(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionServiceBudgetV1 *input, GDExtensionProductionServiceBudgetV1 *remaining, GDExtensionProductionReportV1 *report) {
	if (!aligned(input) || !production_dto_valid(*input) || input->reserved || !aligned(remaining) || !aligned(report) || !separate(input, sizeof(*input), remaining, sizeof(*remaining)) || !separate(input, sizeof(*input), report, sizeof(*report)) || !separate(remaining, sizeof(*remaining), report, sizeof(*report))) {
		return result(ERR_INVALID_PARAMETER, 19);
	}
	const auto offered = *input;
	*remaining = offered;
	*report = {};
	GDExtensionProductionServiceBudgetV1 held = {};
	uint64_t serial = 0;
	bool accept = false, publish = false, consume = false;
	{
		MutexLock lock(mutex);
		ProductionRequest *request = nullptr;
		RenderingDevice *rd = nullptr;
		const Error error = production_request_locked(ref, false, request, rd);
		if (error != OK) {
			return result(error, 16);
		}
		production_report_locked(*request, *report);
		// Release is cleanup of an exhausted or explicitly cancelled plan. No
		// release preflight may unpin a still-declared, unrecorded suffix.
		if (!request->cancelled && request->next_action != request->action_count) {
			if (report->cause.native_error == OK) {
				report->cause = result(ERR_BUSY, 16);
			}
			return result(ERR_BUSY, 16);
		}
		auto &cell = request->control;
		if (cell.state != ProductionControlState::OWNER_CONSUMED) {
			if (cell.input.kind != ProductionControlKind::RELEASE) {
				return result(ERR_BUSY, 16); // Never overtake another immutable turn.
			}
			held = cell.input.budget;
			serial = cell.serial;
			if (cell.state == ProductionControlState::PENDING && !service_budget_active(held)) {
				// This exact cell never published. Expiry returns only a proven
				// no-effect metadata result; it does not renew the old authorization.
				cell.output = {};
				cell.output.request_serial = request->serial;
				cell.output.control_serial = cell.serial;
				cell.output.result = result(ERR_BUSY, 16);
				cell.output.report = *report;
				cell.state = ProductionControlState::RETURNED;
			}
			consume = cell.state == ProductionControlState::RETURNED;
			publish = cell.state == ProductionControlState::PENDING && service_budget_active(offered);
		} else {
			if (!service_budget_active(offered)) {
				return result(ERR_BUSY, 16); // Fresh zero/expired input is only metadata.
			}
			const auto ready = production_release_locked(*request, rd, false);
			if (ready.native_error != OK) {
				// A completed record can still own the original frame cursor.
				// The accepted RELEASE cell services only that exact owned tail;
				// the original reclaim preflight remains unchanged and strict.
				bool own_tail = false;
				if (rd && rd->_thread_safe_.try_lock()) {
					own_tail = request->maintenance_serial && rd->native_disposal_active && request->maintenance_serial == rd->native_disposal_continuation.serial && rd->native_disposal_continuation.production_owner == request;
					rd->_thread_safe_.unlock();
				}
				if (ready.native_error != ERR_BUSY || !own_tail || request->capture_inflight || request->compute_list_open || report->cause.native_error != OK || (!request->cancelled && request->next_action != request->action_count)) {
					return ready;
				}
			}
			accept = publish = true;
		}
		if (!consume && !accept) {
			*remaining = held;
			remaining->controls = remaining->receipts = remaining->passes = 0;
			report->service_control_serial = serial;
			report->service_pending = 1; // Entire immutable slice is still escrowed.
		}
	}
	if (consume) {
		ProductionControlOutput returned = {};
		const Error error = production_control_consume(ref, serial, &returned);
		if (error != OK) {
			*remaining = held;
			remaining->controls = remaining->receipts = remaining->passes = 0;
			report->service_control_serial = serial;
			report->service_pending = 1;
			return result(error, 16);
		}
		*report = returned.report;
		report->service_control_serial = serial;
		report->service_pending = 0;
		*remaining = held; // Only the old ABI/deadline/authorization is returned.
		const auto &rt = returned.service_usage;
		const auto &owner = returned.owner_usage;
		const uint64_t passes = uint64_t(rt.passes) + owner.passes;
		const uint64_t receipts = uint64_t(rt.receipts) + owner.receipts;
		// The RT dispatch and owner continuation are the same unique scalar
		// command. Owner controls is a continuation marker, never a second debit.
		if (returned.request_serial != ref.handle_serial || returned.control_serial != serial || rt.controls > 1 || owner.controls > 1 || (owner.controls && rt.controls != 1) || rt.controls > held.controls || passes > MIN(held.passes, 64u) || receipts > MIN(held.receipts, 128u)) {
			remaining->controls = remaining->receipts = remaining->passes = 0;
			report->cause = result(ERR_INVALID_DATA, 19);
			return report->cause;
		}
		remaining->controls -= rt.controls;
		remaining->passes -= uint32_t(passes);
		remaining->receipts -= uint32_t(receipts);
		const auto completed = returned.result.native_error == OK ? returned.owner_return : returned.result;
		if (completed.native_error != OK) {
			report->cause = completed;
		}
		return completed; // Never accept offered as a new turn in this same call.
	}
	if (accept) {
		const Error error = production_control_accept(ref, ProductionControlKind::RELEASE, &serial, &offered);
		if (error != OK) {
			return result(error, 16);
		}
		held = offered;
	}
	*remaining = held;
	remaining->controls = remaining->receipts = remaining->passes = 0;
	report->service_control_serial = serial;
	report->service_pending = 1;
	if (publish) {
		const Error scheduled = production_control_schedule(ref, serial);
		if (scheduled != OK && scheduled != ERR_BUSY && scheduled != ERR_UNAVAILABLE) {
			return result(scheduled, 16);
		}
	}
	return result(ERR_BUSY, 16); // Actual FIFO return is consumed on a later call.
}

GDExtensionDeviceSessionResultV1 Registry::production_release(GDExtensionProductionRequestRefV1 ref) {
	MutexLock lock(mutex);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_request_locked(ref, false, r, rd);
	if (e != OK) {
		return result(e, 16);
	}
	if (r->control.state != ProductionControlState::OWNER_CONSUMED) {
		return result(ERR_BUSY, 16);
	}
	return production_release_locked(*r, rd, true);
}
GDExtensionDeviceSessionResultV1 Registry::production_release_locked(ProductionRequest &request, RenderingDevice *rd, bool reclaim, const GDExtensionProductionServiceBudgetV1 *budget, ProductionServiceUsage *usage) {
	auto *r = &request;
	if (usage) {
		*usage = {};
	}
	if (r->maintenance_serial) {
		if (!rd || !rd->_thread_safe_.try_lock()) {
			return result(ERR_BUSY, 16);
		}
		const bool active = rd->native_disposal_active && r->maintenance_serial == rd->native_disposal_continuation.serial;
		rd->_thread_safe_.unlock();
		if (active) {
			return result(ERR_BUSY, 16); // Finish the original accepted cursor before reclaim.
		}
		r->maintenance_serial = 0;
	}
	if (r->capture_inflight || r->effects_pending || r->source_device_lost) {
		return result(ERR_BUSY, 16);
	}
	if (r->graph_count > r->capacity.graph_memberships || r->native_count > r->capacity.native_memberships) {
		return result(ERR_INVALID_DATA, 16);
	}
	for (uint32_t i = 0; i < r->graph_count; ++i) {
		if (!r->graphs || r->graphs[i].request != r) {
			return result(ERR_INVALID_DATA, 16);
		}
		if (r->graphs[i].pending) {
			return result(ERR_BUSY, 16);
		}
	}
	for (uint32_t i = 0; i < r->resource_count; ++i) {
		if (r->resources[i].transfer_pending) {
			return result(ERR_BUSY, 16);
		}
	}
	bool native_refs_pending = false, native_issued = false;
	for (uint32_t i = 0; i < r->native_count; ++i) {
		const ProductionNativeCell *owner = nullptr;
		const Error error = production_native_owner_locked(*r, i, owner);
		if (error != OK) {
			return result(error, 16);
		}
		if (r->natives[i].owner_index != i) {
			continue;
		}
		if (owner->member.source.issue == RDD::IssueFact::NotIssued) {
			if (owner->member.retained || owner->member.owner_released) {
				return result(ERR_INVALID_DATA, 16);
			}
			continue;
		}
		native_issued = true;
		if (owner->observation.state != NativeSourceObservation::State::Complete || !owner->member.retained) {
			return result(ERR_BUSY, 16);
		}
		native_refs_pending |= !owner->member.owner_released;
	}
	if (native_issued && (!rd || !r->attachment || !device_records[r->attachment->device].live)) {
		return result(ERR_UNAVAILABLE, 16);
	}
	for (uint32_t i = 0; i < r->capacity.return_cells; ++i) {
		if (r->returns[i].pending) {
			return result(ERR_BUSY, 16);
		}
	}
	for (uint32_t i = 0; i < r->readback_count; ++i) {
		if (r->readbacks[i].source.issued && r->readbacks[i].source.returned != r->readbacks[i].source.issued) {
			return result(ERR_BUSY, 16);
		}
	}
	// Native observer references may only be dropped in the original owner's
	// final-release phase. RT readiness neither releases them nor signs R01 ACK.
	if (!reclaim && native_issued) {
		return result(OK);
	}
	bool has_pins = false;
	for (uint32_t i = 0; i < r->resource_count; ++i) {
		has_pins |= r->resources[i].use_count != 0;
	}
	if ((has_pins || native_refs_pending) && (!rd || !rd->_thread_safe_.try_lock())) {
		return result(ERR_BUSY, 16);
	}
	const bool locked = has_pins || native_refs_pending;
	Error e = OK;
	if (native_refs_pending) {
		if (!rd->driver || !budget || !service_budget_active(*budget) || r->revision > UINT64_MAX - 2) {
			rd->_thread_safe_.unlock();
			return result(r->revision > UINT64_MAX - 2 ? ERR_PARAMETER_RANGE_ERROR : ERR_BUSY, 16);
		}
		ProductionServiceUsage used = { 1, 0, 0 };
		r->release_started = true;
		const uint32_t visits = MIN(MIN(budget->passes, r->native_count), 64u);
		const uint32_t receipts = MIN(budget->receipts, 128u);
		while (used.passes < visits && used.receipts < receipts && before_service_deadline(*budget)) {
			if (r->revision > UINT64_MAX - 2) {
				e = ERR_PARAMETER_RANGE_ERROR;
				break;
			}
			const uint32_t index = r->release_cursor % r->native_count;
			r->release_cursor = (index + 1) % r->native_count;
			++used.passes;
			auto &cell = r->natives[index];
			if (cell.owner_index != index || !cell.member.retained || cell.member.owner_released) {
				continue;
			}
			if (!before_service_deadline(*budget)) {
				break;
			}
			++used.receipts; // Exactly one original driver reference-return attempt.
			auto returned_member = cell.member;
			const uint64_t request_serial = r->serial;
			++r->capture_inflight;
			mutex.unlock();
			const Error returned = release_native_source(*rd->driver, returned_member);
			mutex.lock();
			CRASH_COND(r->serial != request_serial || !r->capture_inflight || cell.request != r);
			cell.member = returned_member;
			--r->capture_inflight;
			if (cell.release_error != returned || returned == OK) {
				++r->revision;
			}
			cell.release_error = returned;
			if (e == OK && returned != OK) {
				e = returned;
			}
		}
		if (usage) {
			*usage = used;
		}
		for (uint32_t i = 0; i < r->native_count; ++i) {
			const auto &cell = r->natives[i];
			if (cell.owner_index == i && cell.member.retained && !cell.member.owner_released && e == OK) {
				e = ERR_BUSY;
			}
		}
		if (e != OK) {
			rd->_thread_safe_.unlock();
			return result(e, 16);
		}
	}
	e = production_unpin_resources_locked(*r);
	if (locked) {
		rd->_thread_safe_.unlock();
	}
	if (e != OK) {
		return result(e, 16);
	}
	// RT metadata cleanup returns before its owner may reset the arena slot.
	if (!reclaim) {
		return result(OK);
	}
	auto reset = [](auto *cells, uint32_t count) { using T=std::remove_pointer_t<decltype(cells)>;for(uint32_t i=0;i<count;++i){cells[i].~T();new (&cells[i]) T();} };
	reset(r->actions, r->capacity.suboperations);
	reset(r->resources, r->capacity.resources);
	reset(r->uses, r->capacity.resource_uses);
	reset(r->readbacks, r->capacity.readbacks);
	reset(r->graphs, r->capacity.graph_memberships);
	reset(r->natives, r->capacity.native_memberships);
	reset(r->returns, r->capacity.return_cells);
	reset(r->commands, r->command_count);
	reset(r->frames, r->frame_count);
	CRASH_COND(!r->attachment->children);
	--r->attachment->children;
	r->~ProductionRequest();
	new (r) ProductionRequest();
	return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::production_installation_observe(GDExtensionProductionAttachmentRefV1 ref, const GDExtensionProductionServiceBudgetV1 *budget, GDExtensionProductionInstallationReportV1 *out) {
	if (!aligned(budget) || !production_dto_valid(*budget) || budget->reserved || !aligned(out)) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	MutexLock lock(mutex);
	ProductionAttachment *a = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_attachment_locked(ref, false, a, rd);
	if (e != OK) {
		return result(e, 16);
	}
	*out = { production_dto_abi<GDExtensionProductionInstallationReportV1>(), uint32_t(a->closing ? GDEXTENSION_PRODUCTION_INSTALL_RETURN_PENDING_V1 : GDEXTENSION_PRODUCTION_INSTALL_ACCEPTED_V1), 0, a->quote.allocation_bytes, 0, result(a->children ? ERR_BUSY : OK) };
	return result(OK);
}
Error Registry::production_entity_return_locked(ProductionAttachment &attachment) {
	auto *rd = attachment.held_device;
	if (attachment.children) {
		return ERR_BUSY;
	}
	if (!rd || device_records[attachment.device].object != rd || device_records[attachment.device].epoch != attachment.epoch ||
			rd->production_entity_registry.load(std::memory_order_acquire) != this || !attachment.holder ||
			attachment.holder->generation != attachment.library_generation || attachment.holder->terminal_action != attachment.terminal_action ||
			!attachment.holder->production_children) {
		return ERR_INVALID_DATA;
	}
	if ((attachment.entity_previous && attachment.entity_previous->entity_next != &attachment) ||
			(!attachment.entity_previous && rd->production_entity_holds != &attachment) ||
			(attachment.entity_next && attachment.entity_next->entity_previous != &attachment)) {
		return ERR_INVALID_DATA;
	}
	if (attachment.entity_previous) {
		attachment.entity_previous->entity_next = attachment.entity_next;
	} else {
		rd->production_entity_holds = attachment.entity_next;
	}
	if (attachment.entity_next) {
		attachment.entity_next->entity_previous = attachment.entity_previous;
	}
	if (!rd->production_entity_holds) {
		rd->production_entity_registry.store(nullptr, std::memory_order_release);
	}
	attachment.held_device = nullptr;
	attachment.entity_previous = nullptr;
	attachment.entity_next = nullptr;
	return OK;
}
GDExtensionDeviceSessionResultV1 Registry::production_close(GDExtensionProductionAttachmentRefV1 ref) {
	MutexLock lock(mutex);
	ProductionAttachment *a = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_attachment_locked(ref, false, a, rd);
	if (e != OK) {
		return result(e, 16);
	}
	if (a->metadata_active) {
		return result(ERR_BUSY, 16);
	}
	for (uint32_t i = 0; i < a->quote.capacity.requests; ++i) {
		if (a->requests[i].control.state != ProductionControlState::OWNER_CONSUMED) {
			return result(ERR_BUSY, 16);
		}
	}
	for (uint32_t i = 0; i < a->quote.capacity.requests; ++i) {
		const auto &request = a->requests[i];
		if (request.serial && !request.cancelled && request.revision == UINT64_MAX) {
			return result(ERR_PARAMETER_RANGE_ERROR, 19);
		}
	}
	a->closing = true;
	for (uint32_t i = 0; i < a->quote.capacity.requests; ++i) {
		auto &request = a->requests[i];
		if (request.serial && !request.cancelled) {
			request.cancelled = true;
			++request.revision;
		}
	}
	if (rd && rd->render_thread_id == Thread::get_caller_id() && rd->_thread_safe_.try_lock()) {
		RdUnlock unlock{ rd->_thread_safe_ };
		for (uint32_t i = 0; i < a->quote.capacity.requests; ++i) {
			if (a->requests[i].serial && a->requests[i].compute_list_open) {
				production_close_lists_locked(a->requests[i], *rd);
			}
		}
	}
	if (a->children) {
		return result(ERR_BUSY, 16);
	}
	e = production_entity_return_locked(*a);
	if (e != OK) {
		return result(e, 16);
	}
	const uint32_t library = a->library, device = a->device;
	auto &record = records[library];
	auto &d = device_records[device];
	CRASH_COND(!record.private_refs || !d.refs || !a->holder->production_children || record.cleanup_reservations < a->quote.cleanup_slots || record.cleanup_by_phase[uint8_t(CleanupPhase::BINDING)] < a->quote.cleanup_slots);
	--record.private_refs;
	a->metadata_reference.registry = nullptr; // Same original counted edge was returned above.
	--d.refs;
	--a->holder->production_children;
	record.cleanup_reservations -= uint32_t(a->quote.cleanup_slots);
	record.cleanup_by_phase[uint8_t(CleanupPhase::BINDING)] -= uint32_t(a->quote.cleanup_slots);
	for (auto *&entry : production_attachments) {
		if (entry == a) {
			entry = nullptr;
			break;
		}
	}
	auto destroy = [](auto *cells, uint32_t count) { using T=std::remove_pointer_t<decltype(cells)>;for(uint32_t i=0;i<count;++i){cells[i].~T();
} };
	destroy(a->requests, a->quote.capacity.requests);
	destroy(a->actions, a->quote.capacity.suboperations);
	destroy(a->resources, a->quote.capacity.resources);
	destroy(a->uses, a->quote.capacity.resource_uses);
	destroy(a->readbacks, a->quote.capacity.readbacks);
	destroy(a->graphs, a->quote.capacity.graph_memberships);
	destroy(a->natives, a->quote.capacity.native_memberships);
	destroy(a->returns, a->quote.capacity.return_cells);
	destroy(a->commands, uint32_t(a->quote.graph_slots));
	destroy(a->frames, uint32_t(a->quote.graph_frames));
	void *arena = a->arena;
	a->~ProductionAttachment();
	Memory::free_static(arena);
	reclaim_device_locked(device);
	reclaim(library);
	try_finalize();
	return result(OK);
}

static_assert(std::is_trivially_copyable_v<ProductionControlInput>);
static_assert(std::is_trivially_copyable_v<ProductionControlOutput>);
static_assert(std::is_trivially_copyable_v<ProductionControlCell>);

Error Registry::production_control_accept(GDExtensionProductionRequestRefV1 ref, ProductionControlKind kind, uint64_t *out, const GDExtensionProductionServiceBudgetV1 *budget) {
	GDExtensionProductionServiceBudgetV1 limit = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), 0, 0, 0, 0, 0 };
	if (budget) {
		if (!aligned(budget) || !production_dto_valid(*budget) || budget->reserved) {
			return ERR_INVALID_PARAMETER;
		}
		limit = *budget;
	}
	if (!aligned(out) || uint32_t(kind) > uint32_t(ProductionControlKind::RELEASE)) {
		return ERR_INVALID_PARAMETER;
	}
	MutexLock lock(mutex);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_request_locked(ref, false, r, rd);
	if (error != OK) {
		return error;
	}
	auto &cell = r->control;
	if (cell.state != ProductionControlState::OWNER_CONSUMED) {
		// A retry retrieves the same retained operation, never overwrites it.
		if (cell.input.kind != kind || !same_service_budget(cell.input.budget, limit)) {
			return ERR_BUSY;
		}
		*out = cell.serial;
		return OK;
	}
	if (!r->next_control_serial) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	cell = {};
	cell.serial = r->next_control_serial;
	r->next_control_serial = cell.serial == UINT64_MAX ? 0 : cell.serial + 1;
	cell.input = { r->serial, r->operation.record_generation, kind, limit, UINT32_MAX };
	cell.state = ProductionControlState::PENDING;
	// The existing request child retains this entire arena and original holder.
	// Close and direct Release refuse this cell until its owner consumes return.
	*out = cell.serial;
	return OK;
}

Error Registry::production_control_schedule(GDExtensionProductionRequestRefV1 ref, uint64_t serial) {
	MutexLock lock(mutex);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_request_locked(ref, false, r, rd);
	if (error != OK) {
		return error;
	}
	auto &cell = r->control;
	if (!serial || cell.serial != serial || cell.state == ProductionControlState::OWNER_CONSUMED) {
		return ERR_DOES_NOT_EXIST;
	}
	if (cell.state != ProductionControlState::PENDING) {
		return OK;
	}
	auto *server = Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton());
	if (!server || this != &get_singleton()) {
		return ERR_UNAVAILABLE;
	}
	// Publication holds the original Registry lock; CQMT does not execute inline
	// and releases its lock before dispatch. The scalar command owns no DSO code.
	cell.state = ProductionControlState::QUEUED;
	if (!server->_try_production_control_dispatch(instance, r->attachment->serial, r->serial, serial)) {
		cell.state = ProductionControlState::PENDING;
		return ERR_BUSY;
	}
	return OK;
}

void Registry::production_control_execute_locked(ProductionRequest &request, RenderingDevice *rd) {
	auto &cell = request.control;
	CRASH_COND(cell.state != ProductionControlState::QUEUED);
	cell.state = ProductionControlState::EXECUTING;
	cell.output = {};
	cell.output.request_serial = request.serial;
	cell.output.control_serial = cell.serial;
	switch (cell.input.kind) {
		case ProductionControlKind::RECORD:
			cell.output.result = result(rd ? production_record_receive_locked(request, *rd) : ERR_UNAVAILABLE);
			break;
		case ProductionControlKind::OBSERVE: {
			Error error = OK;
			if (request.native_count && service_budget_active(cell.input.budget)) {
				if (!rd || !rd->driver) {
					error = ERR_UNAVAILABLE;
				} else if (!rd->_thread_safe_.try_lock()) {
					error = ERR_BUSY;
				} else {
					RdUnlock unlock{ rd->_thread_safe_ };
					error = production_poll_native_locked(request, *rd->driver, cell.input.budget, device_records[request.attachment->device].live, cell.output.service_usage);
				}
			}
			if (rd && service_budget_active(cell.input.budget) && rd->_thread_safe_.try_lock()) {
				RdUnlock unlock{ rd->_thread_safe_ };
				bool progress = request.maintenance_serial && rd->native_disposal_active && request.maintenance_serial == rd->native_disposal_continuation.serial;
				for (uint32_t i = 0; i < request.resource_count; ++i) {
					const auto &slot = request.resources[i];
					MutexLock physical(slot.owned_physical.mutex);
					progress |= slot.retiring && slot.owned_physical.enqueued && !slot.owned_physical.free_event && !slot.returned;
				}
				for (uint32_t i = 0; i < request.graph_count; ++i) {
					progress |= request.graphs[i].pending;
				}
				for (uint32_t i = 0; i < request.readback_count; ++i) {
					progress |= request.readbacks[i].source.issued && request.readbacks[i].source.returned != request.readbacks[i].source.issued;
				}
				if (progress) {
					const Error maintenance = production_maintenance_locked(request, *rd, cell.input.budget, cell.output.service_usage);
					if (error == OK) {
						error = maintenance;
					}
				}
				const Error cleanup = production_sets_step_locked(request, *rd, cell.input.budget, cell.output.service_usage);
				if (error == OK) {
					error = cleanup;
				}
			}
			cell.output.result = result(error);
		} break;
		case ProductionControlKind::STORAGE_CREATE:
		case ProductionControlKind::SET_CREATE:
			cell.output.result = result(rd ? production_create_execute_locked(request, *rd, cell.input.kind, cell.input.resource_slot) : ERR_UNAVAILABLE);
			break;
		case ProductionControlKind::CANCEL:
			cell.output.result = production_cancel_locked(request);
			if (request.compute_list_open && rd && rd->_thread_safe_.try_lock()) {
				RdUnlock unlock{ rd->_thread_safe_ };
				const Error closed = production_close_lists_locked(request, *rd);
				if (closed != OK) {
					cell.output.result = result(closed);
				}
			}
			break;
		case ProductionControlKind::RELEASE: {
			cell.output.service_usage.controls = cell.input.budget.controls ? 1 : 0; // One authorized dispatch; zero-budget internal metadata keeps its original path.
			Error maintenance = OK;
			if (rd && request.maintenance_serial) {
				if (!service_budget_active(cell.input.budget) || !rd->_thread_safe_.try_lock()) {
					maintenance = ERR_BUSY;
				} else {
					RdUnlock unlock{ rd->_thread_safe_ };
					if (rd->native_disposal_active && request.maintenance_serial == rd->native_disposal_continuation.serial && rd->native_disposal_continuation.production_owner == &request) {
						maintenance = production_maintenance_locked(request, *rd, cell.input.budget, cell.output.service_usage);
					}
				}
			}
			cell.output.result = maintenance == OK ? production_release_locked(request, rd, false) : result(maintenance, 16);
		} break;
	}
	production_report_locked(request, cell.output.report);
	cell.state = ProductionControlState::RETURNED;
	request.attachment->metadata_pending = request.attachment->metadata_receiver != nullptr;
	// No recursive schedule, driver wait, native completion, or source ACK here.
}

void Registry::production_control_dispatch(uint64_t registry, uint64_t attachment, uint64_t request, uint64_t serial) {
	auto *server = Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton());
	if (!server || Thread::get_caller_id() != server->server_thread) {
		return;
	}
	MutexLock lock(mutex);
	if (registry != instance || !serial) {
		return;
	}
	for (auto *a : production_attachments) {
		if (!a || a->serial != attachment) {
			continue;
		}
		for (uint32_t i = 0; i < a->quote.capacity.requests; ++i) {
			auto &r = a->requests[i];
			auto &cell = r.control;
			if (!request || r.serial != request || cell.serial != serial || cell.state != ProductionControlState::QUEUED) {
				continue;
			}
			auto &owner = records[a->library];
			auto &device = device_records[a->device];
			// This validates the original enrolled object and retained queue admission.
			// It does not create a qualification TLS permission or a numeric grant.
			if (r.attachment != a || cell.input.request_serial != r.serial ||
					cell.input.record_generation != r.operation.record_generation ||
					!owner.occupied || owner.snapshot.library_generation != a->library_generation ||
					owner.producer_registration_serial != a->producer_serial ||
					!owner.producer_attachment_identity || !a->holder ||
					a->holder->handle_serial != owner.producer_code_holder ||
					a->holder->generation != a->library_generation ||
					a->holder->terminal_action != a->terminal_action || !a->holder->production_children ||
					device.epoch != a->epoch || device.execution_thread != Thread::get_caller_id() ||
					!a->held_device || device.object != a->held_device ||
					a->held_device->production_entity_registry.load(std::memory_order_acquire) != this) {
				cell.output = {};
				cell.output.request_serial = r.serial;
				cell.output.control_serial = cell.serial;
				cell.output.result = result(ERR_INVALID_DATA, 16);
				cell.state = ProductionControlState::RETURNED;
				return;
			}
			// A loss/sealed preflight does not erase already accepted cleanup. Pending
			// native effects are still checked by the real release helper and retained.
			production_control_execute_locked(r, a->held_device);
			return;
		}
		return;
	}
}

Error Registry::production_control_consume(GDExtensionProductionRequestRefV1 ref, uint64_t serial, ProductionControlOutput *out) {
	if (!aligned(out)) {
		return ERR_INVALID_PARAMETER;
	}
	MutexLock lock(mutex);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_request_locked(ref, false, r, rd);
	if (error != OK) {
		return error;
	}
	auto &cell = r->control;
	if (!serial || cell.serial != serial || cell.state == ProductionControlState::OWNER_CONSUMED) {
		return ERR_DOES_NOT_EXIST;
	}
	if (cell.state != ProductionControlState::RETURNED) {
		return ERR_BUSY;
	}
	auto retained = cell.output;
	if (cell.input.kind == ProductionControlKind::RELEASE && retained.result.native_error == OK) {
		// The RT has returned. Reclaim is now owned by the original owner; it must
		// succeed before the result is consumed or the request slot becomes reusable.
		auto owner_budget = cell.input.budget;
		const auto &rt = retained.service_usage;
		if (rt.controls > 1 || rt.controls > owner_budget.controls || rt.receipts > MIN(owner_budget.receipts, 128u) || rt.passes > MIN(owner_budget.passes, 64u)) {
			retained.owner_return = result(ERR_INVALID_DATA, 19);
			cell.state = ProductionControlState::OWNER_CONSUMED;
			*out = retained;
			return OK;
		}
		// One original authorization spans RT and its owner continuation. No
		// refill or fresh deadline is permitted after the asynchronous return.
		owner_budget.receipts = MIN(owner_budget.receipts, 128u) - rt.receipts;
		owner_budget.passes = MIN(owner_budget.passes, 64u) - rt.passes;
		if (!before_service_deadline(owner_budget)) {
			owner_budget.controls = owner_budget.receipts = owner_budget.passes = 0;
		}
		const auto released = production_release_locked(*r, rd, true, &owner_budget, &retained.owner_usage);
		retained.owner_return = released;
		if (released.native_error != OK) {
			// This owner's turn has consumed its bounded credit. Keep all unreturned
			// cells/edges and accept a fresh exact control turn for a later retry.
			cell.state = ProductionControlState::OWNER_CONSUMED;
		}
	} else {
		cell.state = ProductionControlState::OWNER_CONSUMED;
	}
	*out = retained;
	return OK;
}
} // namespace GDExtensionDeviceExit
