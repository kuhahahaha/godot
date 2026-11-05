/**************************************************************************/
/*  gdextension_production_window.cpp                                     */
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
#include "core/extension/gdextension_record_source.h"
#include "core/os/thread.h"
#include "servers/rendering/rendering_device.h"

#include <cstring>

namespace GDExtensionDeviceExit {
namespace {
GDExtensionDeviceSessionResultV1 window_result(Error error) {
	return { int32_t(error), 0, 0, 0 };
}
template <class T>
bool window_aligned(const T *p) {
	return p && uintptr_t(p) % alignof(T) == 0;
}
} //namespace
struct Registry::ProductionPhysicalFrame {
	ProductionPhysicalFrame *previous = nullptr;
	Registry *registry = nullptr;
	ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	const RegistrationContext *protected_entry = nullptr;
	uint64_t serial = 0, control = 0;
	uint32_t append_calls = 0;
};
thread_local Registry::ProductionPhysicalFrame *Registry::production_physical_frame = nullptr;

Error Registry::production_install_record_receiver(GDExtensionDeviceExitSceneReadyContextRef frame, GDExtensionProductionAttachmentRefV1 ref, ProductionRecordReceiver receiver, void *userdata, ProductionMetadataReceiver metadata_receiver) {
	if (!receiver) {
		return ERR_INVALID_PARAMETER;
	}
	MutexLock lock(mutex);
	ProductionAttachment *attachment = nullptr;
	RenderingDevice *device = nullptr;
	Error error = production_attachment_locked(ref, true, attachment, device);
	if (error != OK) {
		return error;
	}
	View *view = nullptr;
	error = resolve_borrow_locked(frame.registry_instance, frame.handle_serial, ViewDomain::SCENE_READY, view);
	if (error != OK) {
		return error;
	}
	if (view->record != attachment->library || !RegistrationContext::current || RegistrationContext::current->level_phase != LevelPhase::SCENE_READY || records[view->record].scene_ready_state != Record::SceneReadyState::IN_FRAME) {
		return ERR_UNAUTHORIZED;
	}
	if (attachment->record_receiver) {
		return attachment->record_receiver == receiver && attachment->record_receiver_userdata == userdata && attachment->metadata_receiver == metadata_receiver ? OK : ERR_ALREADY_IN_USE;
	}
	if (attachment->opened || attachment->children) {
		return ERR_BUSY;
	}
	// The accepted arena already retains this exact original CodeHold and its
	// terminal action. No Callable copy, second code-holder or timer is made.
	attachment->record_receiver = receiver;
	attachment->record_receiver_userdata = userdata;
	attachment->metadata_receiver = metadata_receiver;
	return OK;
}

GDExtensionDeviceSessionResultV1 Registry::production_reserve(GDExtensionProductionAttachmentRefV1 ref, const GDExtensionProductionOperationV1 *operation, const GDExtensionProductionCapacityV1 *capacity, const GDExtensionProductionActionV1 *actions, uint32_t action_count, const GDExtensionProductionResourceV1 *resources, uint32_t resource_count, const GDExtensionProductionUseV1 *uses, uint32_t use_count, const GDExtensionProductionReadbackV1 *readbacks, uint32_t readback_count, GDExtensionProductionRequestRefV1 *out) {
	if (!window_aligned(operation) || !window_aligned(capacity) || !window_aligned(out) || out->registry_instance || out->handle_serial) {
		return window_result(ERR_INVALID_PARAMETER);
	}
	MutexLock lock(mutex);
	ProductionAttachment *attachment = nullptr;
	RenderingDevice *device = nullptr;
	Error error = production_attachment_locked(ref, true, attachment, device);
	if (error != OK) {
		return window_result(error);
	}
	if (!attachment->opened || !attachment->record_receiver) {
		return window_result(ERR_UNAVAILABLE);
	}
	ProductionRequest *request = nullptr;
	error = production_reserve_metadata_locked(*attachment, *operation, *capacity, actions, action_count, resources, resource_count, uses, use_count, readbacks, readback_count, request);
	if (error == OK) {
		*out = { instance, request->serial };
	}
	return window_result(error);
}

Error Registry::production_record_schedule(GDExtensionProductionRequestRefV1 ref, uint64_t *out) {
	if (is_production_metadata_entry()) {
		return ERR_UNAUTHORIZED; // Even an old PENDING record cannot republish new work here.
	}
	if (!window_aligned(out)) {
		return ERR_INVALID_PARAMETER;
	}
	uint64_t serial = 0;
	{
		MutexLock lock(mutex);
		ProductionRequest *request = nullptr;
		RenderingDevice *device = nullptr;
		Error error = production_request_locked(ref, false, request, device);
		if (error != OK) {
			return error;
		}
		auto &cell = request->control;
		if (cell.state != ProductionControlState::OWNER_CONSUMED) {
			if (cell.input.kind != ProductionControlKind::RECORD) {
				return ERR_BUSY;
			}
			serial = cell.serial;
		} else {
			ProductionRequest *admitted = nullptr;
			error = production_request_locked(ref, true, admitted, device);
			if (error != OK) {
				return error;
			}
			if (!request->attachment->opened || !request->attachment->record_receiver || !request->next_control_serial) {
				return ERR_UNAVAILABLE;
			}
			error = production_freeze_locked(*request);
			if (error != OK) {
				return error;
			}
			cell = {};
			cell.serial = request->next_control_serial;
			request->next_control_serial = cell.serial == UINT64_MAX ? 0 : cell.serial + 1;
			cell.input.request_serial = request->serial;
			cell.input.record_generation = request->operation.record_generation;
			cell.input.kind = ProductionControlKind::RECORD;
			cell.input.resource_slot = UINT32_MAX;
			cell.state = ProductionControlState::PENDING;
			serial = cell.serial;
		}
		*out = serial;
	}
	// Even an owner who is the RT publishes through the original FIFO.
	return production_control_schedule(ref, serial);
}

GDExtensionDeviceSessionResultV1 Registry::production_record_consume(GDExtensionProductionRequestRefV1 ref, uint64_t serial, GDExtensionProductionRecordReportV1 *out) {
	if (!window_aligned(out)) {
		return window_result(ERR_INVALID_PARAMETER);
	}
	*out = {};
	{
		MutexLock lock(mutex);
		ProductionRequest *request = nullptr;
		RenderingDevice *device = nullptr;
		Error error = production_request_locked(ref, false, request, device);
		if (error != OK) {
			return window_result(error);
		}
		if (!serial || request->control.serial != serial || request->control.input.kind != ProductionControlKind::RECORD) {
			return window_result(ERR_DOES_NOT_EXIST);
		}
	}
	ProductionControlOutput returned = {};
	const Error error = production_control_consume(ref, serial, &returned);
	if (error != OK) {
		return window_result(error);
	}
	out->abi = production_dto_abi<GDExtensionProductionRecordReportV1>();
	out->request_serial = returned.request_serial;
	out->control_serial = returned.control_serial;
	out->result = returned.result;
	out->report = returned.report;
	out->append = returned.append;
	out->segment = returned.segment;
	static_assert(uint32_t(ProductionRecordPause::NONE) == GDEXTENSION_PRODUCTION_RECORD_PAUSE_NONE_V1);
	static_assert(uint32_t(ProductionRecordPause::SCOPE_RETURN) == GDEXTENSION_PRODUCTION_RECORD_PAUSE_SCOPE_RETURN_V1);
	static_assert(uint32_t(ProductionRecordPause::APPEND_LIMIT) == GDEXTENSION_PRODUCTION_RECORD_PAUSE_APPEND_LIMIT_V1);
	static_assert(uint32_t(ProductionRecordPause::UPLOAD_PREFIX) == GDEXTENSION_PRODUCTION_RECORD_PAUSE_UPLOAD_PREFIX_V1);
	out->pause = uint32_t(returned.record_pause);
	out->flags = (returned.append.abi.abi_version != 0 ? uint32_t(GDEXTENSION_PRODUCTION_RECORD_HAS_APPEND_V1) : 0u) | (returned.segment.segment != 0 ? uint32_t(GDEXTENSION_PRODUCTION_RECORD_HAS_SEGMENT_V1) : 0u) | (returned.report.abi.abi_version != 0 ? uint32_t(GDEXTENSION_PRODUCTION_RECORD_HAS_REPORT_V1) : 0u);
	return window_result(OK);
}

Error Registry::production_record_receive_locked(ProductionRequest &request, RenderingDevice &device) {
	auto &attachment = *request.attachment;
	auto &record = records[attachment.library];
	if (is_production_metadata_entry() || !attachment.opened || !attachment.record_receiver || !attachment.holder || attachment.holder->pending || !record.attached || record.guard_enrollment != GuardEnrollment::QUALIFIED || attachment.closing || request.cancelled || request.release_started || !device_records[attachment.device].live || device.production_finalization_started.load(std::memory_order_acquire) || device.render_thread_id != Thread::get_caller_id()) {
		return ERR_BUSY;
	}
	if (record.private_refs >= private_limit || record.active_callbacks >= UINT32_MAX - record.cleanup_reservations || frame_serials.exhausted) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	if (device.draw_list.active || device.raytracing_list.active || device.compute_list.active != request.compute_list_open || device.draw_graph.has_driver_callbacks()) {
		return ERR_BUSY;
	}
	const auto receiver = attachment.record_receiver;
	void *userdata = attachment.record_receiver_userdata;
	const auto operation = request.operation;
	const uint64_t serial = request.serial, control = request.control.serial;
	++record.private_refs;
	RecordRef reference(this, attachment.library);
	mutex.unlock();
	Error error = OK;
	{
		// Original normal-entry protection surrounds the actual DSO invocation;
		// arena/CodeHold stay retained by this same executing fixed control cell.
		CallbackScope protected_entry(reference);
		error = protected_entry.get_error();
		if (error == OK) {
			ProductionPhysicalFrame frame;
			frame.previous = production_physical_frame;
			frame.registry = this;
			frame.request = &request;
			frame.device = &device;
			frame.protected_entry = RegistrationContext::current;
			frame.serial = RegistrationContext::current->frame_id;
			frame.control = control;
			production_physical_frame = &frame;
			receiver(userdata, { instance, frame.serial }, { instance, serial }, &operation);
			// Actual protected frame return is a physical cutoff, not a caller
			// list split or cancellation. Keep entry/CodeHold live through closure.
			mutex.lock();
			error = production_close_lists_locked(request, device);
			if (error == OK && request.graph_count != request.segment_graph_begin && request.segment.segment == request.acknowledged_segment) {
				error = production_close_segment_locked(request, request.control.output.segment, true);
			} else {
				request.control.output.segment = request.segment;
			}
			if (error != OK || request.compute_list_open || device.compute_list.active) {
				request.effects_pending = true;
				if (error == OK) {
					error = ERR_BUSY;
				}
			}
			if (!request.effects_pending && request.next_action < request.action_count) {
				request.control.output.record_pause = request.control.output.append.state == GDEXTENSION_PRODUCTION_KNOWN_PREFIX_V1 ? ProductionRecordPause::UPLOAD_PREFIX : frame.append_calls >= 64 ? ProductionRecordPause::APPEND_LIMIT
																																																	  : ProductionRecordPause::SCOPE_RETURN;
			}
			mutex.unlock();
			production_physical_frame = frame.previous; // This physical context expires here.
		}
	}
	// The temporary original RecordRef is returned without holding Registry's
	// nonrecursive lock; the accepted request remains live through EXECUTING.
	reference.reset();
	mutex.lock();
	CRASH_COND(request.serial != serial || request.control.serial != control || request.control.state != ProductionControlState::EXECUTING);
	return error;
}

Error Registry::production_physical_request_locked(ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, ProductionRequest *&out, RenderingDevice *&device, bool append) {
	out = nullptr;
	device = nullptr;
	auto *frame = production_physical_frame;
	if (!frame || frame->registry != this || context.registry_instance != instance || context.frame_serial != frame->serial || !context.frame_serial || RegistrationContext::current != frame->protected_entry || !RegistrationContext::current->admitted || RegistrationContext::current->frame_id != context.frame_serial || ref.registry_instance != instance || !frame->request || ref.handle_serial != frame->request->serial) {
		return ERR_UNAUTHORIZED;
	}
	auto &request = *frame->request;
	auto &attachment = *request.attachment;
	auto &record = records[attachment.library];
	auto &native = device_records[attachment.device];
	if (!record.occupied || record.snapshot.library_generation != attachment.library_generation || record.producer_registration_serial != attachment.producer_serial || !attachment.holder || attachment.holder->generation != attachment.library_generation || attachment.holder->handle_serial != record.producer_code_holder || request.control.state != ProductionControlState::EXECUTING || request.control.serial != frame->control || request.control.input.kind != ProductionControlKind::RECORD || native.epoch != attachment.epoch || native.object != frame->device || native.execution_thread != Thread::get_caller_id() || frame->device->render_thread_id != Thread::get_caller_id()) {
		return ERR_INVALID_DATA;
	}
	if (append && (attachment.holder->pending || !record.attached || record.guard_enrollment != GuardEnrollment::QUALIFIED || attachment.closing || record.quiescing || record.close_started || request.cancelled || request.release_started || !native.live || frame->device->production_finalization_started.load(std::memory_order_acquire) || frame->append_calls >= 64)) {
		return ERR_BUSY;
	}
	out = &request;
	device = frame->device;
	return OK;
}

GDExtensionDeviceSessionResultV1 Registry::production_append(ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, uint64_t canonical, uint64_t revision, GDExtensionProductionAppendV1 *out) {
	if (!window_aligned(out)) {
		return window_result(ERR_INVALID_PARAMETER);
	}
	*out = {};
	MutexLock lock(mutex);
	ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	Error error = production_physical_request_locked(context, ref, request, device, true);
	if (error != OK) {
		return window_result(error);
	}
	out->abi = production_dto_abi<GDExtensionProductionAppendV1>();
	out->canonical_suboperation = canonical;
	out->revision = request->revision;
	if (revision != request->revision || request->next_action >= request->action_count || canonical != request->operation.canonical_begin + request->next_action) {
		return out->cause = window_result(ERR_INVALID_DATA);
	}
	if (!device->_thread_safe_.try_lock()) {
		return out->cause = window_result(ERR_BUSY);
	}
	OriginalOperationIdentity identity{ request->operation.owner_request_nonce, request->attachment->library_generation, device_records[request->attachment->device].session, request->operation.record_generation, request->operation.canonical_begin, request->operation.canonical_end };
	++production_physical_frame->append_calls;
	const uint32_t first_graph = request->graph_count;
	const uint32_t action_index = request->next_action;
	const auto observed = production_record_effect_locked(*request, *device, action_index, identity);
	device->_thread_safe_.unlock();
	out->revision = request->revision;
	out->state = observed.state == CheckedRecordAppend::State::Appended ? GDEXTENSION_PRODUCTION_APPENDED_V1 : observed.state == CheckedRecordAppend::State::Rejected ? GDEXTENSION_PRODUCTION_REJECTED_V1
			: observed.state == CheckedRecordAppend::State::KnownPrefix																								  ? GDEXTENSION_PRODUCTION_KNOWN_PREFIX_V1
																																									  : GDEXTENSION_PRODUCTION_UNKNOWN_V1;
	out->first_graph_fact = first_graph;
	out->graph_fact_count = request->graph_count - first_graph;
	if (request->actions[action_index].declaration.action_kind == GDEXTENSION_PRODUCTION_UPLOAD_V1) {
		out->parent_bytes = request->actions[action_index].upload.bytes;
		out->recorded_bytes = request->actions[action_index].recorded_bytes;
	}
	out->cause = window_result(observed.error);
	request->control.output.append = *out;
	return out->cause;
}

GDExtensionDeviceSessionResultV1 Registry::production_close_segment(ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, GDExtensionProductionSegmentV1 *out) {
	if (!window_aligned(out)) {
		return window_result(ERR_INVALID_PARAMETER);
	}
	MutexLock lock(mutex);
	ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	Error error = production_physical_request_locked(context, ref, request, device, false);
	if (error == OK) {
		if (!device->_thread_safe_.try_lock()) {
			return window_result(ERR_BUSY);
		}
		error = production_close_lists_locked(*request, *device);
		device->_thread_safe_.unlock();
		if (error == OK) {
			error = production_close_segment_locked(*request, *out);
		}
	}
	return window_result(error);
}
GDExtensionDeviceSessionResultV1 Registry::production_ack_segment(GDExtensionProductionRequestRefV1 ref, uint64_t segment, uint64_t revision) {
	MutexLock lock(mutex);
	ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	// Exact original owner/holder/producer/device association is still required.
	// ACK returns metadata; it never admits recording or reopens sealed work.
	Error error = production_request_locked(ref, false, request, device);
	if (error == OK) {
		const auto *frame = RegistrationContext::current;
		const auto &attachment = *request->attachment;
		if (!frame || !frame->admitted || !frame->frame_id || !frame->reference || frame->reference->registry != this || frame->reference->slot != attachment.library ||
				(frame->level_phase != LevelPhase::NONE && frame->level_phase != LevelPhase::DEINITIALIZE && frame->level_phase != LevelPhase::PRODUCTION_METADATA) ||
				!records[attachment.library].active_callbacks || !attachment.holder || !attachment.holder->typed_code_hold || attachment.holder->terminal_action != attachment.terminal_action || attachment.holder->acquired_thread != records[attachment.library].owner_thread || attachment.holder->generation != records[attachment.library].snapshot.library_generation || attachment.holder->handle_serial != records[attachment.library].producer_code_holder) {
			error = ERR_UNAUTHORIZED;
		}
	}
	if (error == OK && request->control.state != ProductionControlState::OWNER_CONSUMED) {
		error = ERR_BUSY;
	}
	if (error == OK) {
		error = production_ack_segment_locked(*request, segment, revision);
	}
	return window_result(error);
}

bool Registry::is_production_metadata_entry() {
	// A nested ordinary entry cannot erase this dynamic restriction; it is not
	// tied to a target RD, so changing device cannot recover normal work access.
	for (auto *frame = RegistrationContext::current; frame; frame = frame->previous) {
		if (frame->level_phase == LevelPhase::PRODUCTION_METADATA) {
			return true;
		}
	}
	return false;
}
bool Registry::production_metadata_engine_call_allowed(const StringName &class_name, const StringName &method) {
	if (!is_production_metadata_entry()) {
		return true;
	}
	// Original owner service clock and existing object/ref release remain their
	// original mechanisms. No ordinary engine work or deferred dispatch is part
	// of this accepted metadata service. Native returns use the typed ABI.
	return (class_name == "Time" && (method == "get_ticks_usec" || method == "get_ticks_msec")) ||
			(class_name == "RefCounted" && method == "unreference") || method == "free" || method == "get_instance_id";
}

void Registry::production_metadata_pump() {
	MutexLock lock(mutex);
	for (uint32_t slot = 0; slot < RECORD_CAPACITY; ++slot) {
		auto *attachment = production_attachments[slot];
		if (!attachment || !attachment->metadata_receiver || !attachment->metadata_pending || attachment->metadata_active) {
			continue;
		}
		auto &record = records[attachment->library];
		if (record.owner_thread != Thread::get_caller_id() || !record.occupied || record.guard_enrollment != GuardEnrollment::QUALIFIED || record.snapshot.library_generation != attachment->library_generation || record.producer_registration_serial != attachment->producer_serial || !attachment->holder || attachment->holder->pending || !attachment->holder->typed_code_hold || attachment->holder->handle_serial != record.producer_code_holder || attachment->holder->generation != attachment->library_generation || attachment->holder->terminal_action != attachment->terminal_action || !attachment->holder->production_children || !attachment->metadata_reference.is_valid() || !attachment->metadata_frame || !record.cleanup_reservations) {
			continue;
		}
		const auto &action = record.levels[GDEXTENSION_INITIALIZATION_SCENE].binding_action;
		if (action.serial != attachment->terminal_action || (action.services & ~live_services)) {
			continue;
		}
		// Borrow one already reserved accepted-cleanup slot; no normal admission,
		// new private/code hold, work quota or fresh serial is requested after seal.
		attachment->metadata_pending = false;
		attachment->metadata_active = true;
		--record.cleanup_reservations;
		++record.active_callbacks;
		++record.active_cleanup_by_phase[uint8_t(CleanupPhase::BINDING)];
		const auto receiver = attachment->metadata_receiver;
		void *userdata = attachment->record_receiver_userdata;
		const auto ref = GDExtensionProductionAttachmentRefV1{ instance, attachment->serial };
		RegistrationContext frame;
		frame.enter(attachment->metadata_reference, false);
		frame.frame_id = attachment->metadata_frame;
		frame.level_phase = LevelPhase::PRODUCTION_METADATA;
		mutex.unlock();
		receiver(userdata, ref);
		// The attachment refuses Close while this accepted entry is active.
		// Original borrowed views expire before its code/private credit returns.
		frame.leave();
		mutex.lock();
		CRASH_COND(production_attachments[slot] != attachment || !attachment->metadata_active);
		--record.active_cleanup_by_phase[uint8_t(CleanupPhase::BINDING)];
		--record.active_callbacks;
		++record.cleanup_reservations;
		attachment->metadata_active = false;
		for (uint32_t i = 0; i < attachment->quote.capacity.requests; ++i) {
			const auto &request = attachment->requests[i];
			if (request.serial) { // Accepted metadata/return work stays serviceable, including FIFO Busy retries.
				attachment->metadata_pending = true;
			}
		}
	}
}
} // namespace GDExtensionDeviceExit
