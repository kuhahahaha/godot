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
#include "core/os/thread.h"
#include "servers/rendering/rendering_device.h"

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
constexpr GDExtensionDeviceExitAbi ABI = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
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
} //namespace
Error Registry::production_layout(const GDExtensionProductionCapacityV1 &c, GDExtensionProductionMetadataQuoteV1 &out, uint32_t current_commands, uint32_t workers) {
	if (!abi(c.abi) || !c.requests || !c.suboperations || !c.resources || c.requests > c.suboperations || c.requests > c.resources || c.resource_uses < c.resources || c.graph_memberships < c.suboperations || c.native_memberships < c.suboperations || c.return_cells < c.resources || c.readbacks > c.suboperations) {
		return ERR_INVALID_PARAMETER;
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
	out = { ABI, c, bytes, alignof(std::max_align_t), commands, frames, c.return_cells };
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
	const uint32_t slots = uint32_t(quote.graph_slots), frames = uint32_t(quote.graph_frames);
	const uint64_t main = uint64_t(current) * capacity.requests + capacity.graph_memberships + frames;
	if (main > slots) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	const uint32_t native = rd.driver->qualification_native_call_bound_for_profile(uint32_t(main), slots - uint32_t(main), frames);
	if (!native) {
		return ERR_UNAVAILABLE;
	}
	if (native > capacity.native_memberships) {
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
	if (!d.live || !d.epoch || !d.session || !rd || rd != RenderingDevice::get_singleton() || !rd->is_main_instance || d.execution_thread != rd->render_thread_id) {
		return ERR_UNAVAILABLE;
	}
	holder = nullptr;
	for (auto *h = record.holders; h; h = h->record_next) {
		if (!h->typed_code_hold || h->pending || h->handle_serial != record.producer_code_holder || h->generation != record.snapshot.library_generation || h->acquired_thread != record.owner_thread) {
			continue;
		}
		const auto &scene = record.levels[GDEXTENSION_INITIALIZATION_SCENE];
		if ((scene.binding_action.serial && h->terminal_action == scene.binding_action.serial && !scene.binding_action.returned) || (scene.user_action.serial && h->terminal_action == scene.user_action.serial && !scene.user_action.returned)) {
			holder = h;
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
	if (!aligned(a) || !abi(*a) || !aligned(input) || !abi(input->abi) || !aligned(out) || !aligned(report) || !separate(out, sizeof(*out), report, sizeof(*report)) || out->registry_instance || out->handle_serial) {
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
	if (slot == INVALID_SLOT || production_serials.exhausted || record.private_refs >= private_limit || d.refs == UINT32_MAX || holder->production_children == UINT32_MAX || quote.cleanup_slots > UINT32_MAX - record.cleanup_reservations || uint64_t(record.active_callbacks) + record.cleanup_reservations + quote.cleanup_slots > UINT32_MAX || quote.cleanup_slots > UINT32_MAX - record.cleanup_by_phase[uint8_t(CleanupPhase::BINDING)]) {
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
	attachment->quote = quote;
	attachment->arena = arena;
	++record.private_refs;
	++d.refs;
	++holder->production_children;
	record.cleanup_reservations += uint32_t(quote.cleanup_slots);
	record.cleanup_by_phase[uint8_t(CleanupPhase::BINDING)] += uint32_t(quote.cleanup_slots);
	production_attachments[slot] = attachment;
	*out = { instance, attachment->serial };
	*report = { ABI, GDEXTENSION_PRODUCTION_INSTALL_ACCEPTED_V1, 0, quote.allocation_bytes, 0, result(OK) };
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
			if (r.attachment != a || (admit && r.cancelled)) {
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
	// Session refs retain metadata, not the original K03 entity holder or
	// operation-window receiver. The accepted allocation remains closeable.
	return result(ERR_UNAVAILABLE, GDEXTENSION_DEVICE_SESSION_REASON_NATIVE_UNSUPPORTED_V1);
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
	if (c.requests != 1 || !capacity_fits(c, a.quote.capacity) || !abi(op.abi) || !op.owner_request_nonce || !op.record_generation || op.canonical_end <= op.canonical_begin || !aligned(actions) || !aligned(resources) || (use_count && !aligned(uses)) || (readback_count && !aligned(readbacks)) || !action_count || action_count > c.suboperations || !resource_count || resource_count > c.resources || use_count > c.resource_uses || readback_count > c.readbacks) {
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
		if (v.canonical_suboperation < op.canonical_begin || v.canonical_suboperation >= op.canonical_end || (i && v.canonical_suboperation <= actions[i - 1].canonical_suboperation) || v.action_kind < GDEXTENSION_PRODUCTION_DISPATCH_V1 || v.action_kind > GDEXTENSION_PRODUCTION_READBACK_V1 || v.list_boundary > GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1 || v.first_use > use_count || v.use_count > use_count - v.first_use) {
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
	if (!aligned(input) || !abi(input->abi) || input->reserved || input->push_bytes != bytes || (bytes && !push) || !input->groups_x || !input->groups_y || !input->groups_z) {
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
	if (!aligned(input) || !abi(input->abi) || !input->bytes) {
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
	if (!aligned(input) || !abi(input->abi) || input->reserved || !bytes || !data || bytes != input->bytes || bytes > SIZE_MAX) {
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
	if (!abi(value.abi) || value.reserved || !bytes || !data || bytes != value.bytes || bytes > SIZE_MAX) {
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
GDExtensionDeviceSessionResultV1 Registry::production_observe(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionServiceBudgetV1 *budget, GDExtensionProductionReportV1 *out) {
	if (!aligned(budget) || !abi(budget->abi) || budget->reserved || !aligned(out)) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	MutexLock lock(mutex);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_request_locked(ref, false, r, rd);
	if (e != OK) {
		return result(e, 16);
	}
	// Observation never initiates copy/upload or promotes retained unknown work.
	GDExtensionProductionReportV1 report = {};
	report.abi = ABI;
	report.operation = r->operation;
	report.revision = r->revision;
	report.recorded_end = r->operation.canonical_begin;
	report.cancelled = r->cancelled;
	report.possible_obligations = r->effects_pending ? 1 : 0;
	report.cause = result(r->effects_pending ? ERR_BUSY : OK);
	*out = report;
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
	if (!r->cancelled) {
		if (r->revision == UINT64_MAX) {
			return result(ERR_PARAMETER_RANGE_ERROR, 19);
		}
		r->cancelled = true;
		++r->revision;
	}
	return result(r->effects_pending ? ERR_BUSY : OK);
}
GDExtensionDeviceSessionResultV1 Registry::production_release(GDExtensionProductionRequestRefV1 ref) {
	MutexLock lock(mutex);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_request_locked(ref, false, r, rd);
	if (e != OK) {
		return result(e, 16);
	}
	if (r->effects_pending) {
		return result(ERR_BUSY, 16);
	}
	for (uint32_t i = 0; i < r->capacity.native_memberships; ++i) {
		if (r->natives[i].member.retained && !r->natives[i].member.owner_released) {
			return result(ERR_BUSY, 16);
		}
	}
	for (uint32_t i = 0; i < r->capacity.return_cells; ++i) {
		if (r->returns[i].pending) {
			return result(ERR_BUSY, 16);
		}
	}
	bool has_pins = false;
	for (uint32_t i = 0; i < r->resource_count; ++i) {
		has_pins |= r->resources[i].use_count != 0;
	}
	if (has_pins && (!rd || !rd->_thread_safe_.try_lock())) {
		return result(ERR_BUSY, 16);
	}
	e = production_unpin_resources_locked(*r);
	if (has_pins) {
		rd->_thread_safe_.unlock();
	}
	if (e != OK) {
		return result(e, 16);
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
	if (!aligned(budget) || !abi(budget->abi) || budget->reserved || !aligned(out)) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	MutexLock lock(mutex);
	ProductionAttachment *a = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_attachment_locked(ref, false, a, rd);
	if (e != OK) {
		return result(e, 16);
	}
	*out = { ABI, uint32_t(a->closing ? GDEXTENSION_PRODUCTION_INSTALL_RETURN_PENDING_V1 : GDEXTENSION_PRODUCTION_INSTALL_ACCEPTED_V1), 0, a->quote.allocation_bytes, 0, result(a->children ? ERR_BUSY : OK) };
	return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::production_close(GDExtensionProductionAttachmentRefV1 ref) {
	MutexLock lock(mutex);
	ProductionAttachment *a = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_attachment_locked(ref, false, a, rd);
	if (e != OK) {
		return result(e, 16);
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
	if (a->children) {
		return result(ERR_BUSY, 16);
	}
	const uint32_t library = a->library, device = a->device;
	auto &record = records[library];
	auto &d = device_records[device];
	CRASH_COND(!record.private_refs || !d.refs || !a->holder->production_children || record.cleanup_reservations < a->quote.cleanup_slots || record.cleanup_by_phase[uint8_t(CleanupPhase::BINDING)] < a->quote.cleanup_slots);
	--record.private_refs;
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
} // namespace GDExtensionDeviceExit
