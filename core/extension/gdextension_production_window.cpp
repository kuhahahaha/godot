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

#include "core/extension/gdextension_production_legacy_projection.h"
#include "core/extension/gdextension_production_request.h"
#include "core/extension/gdextension_record_source.h"
#include "core/os/thread.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server_default.h"

#include <cstring>

namespace GDExtensionDeviceExit {
// Defined beside the original CallableCustomExtension; no extension code runs.
bool scene_phase_callable_metadata(const Callable &, NormalRelayToken, const CleanupAction *&, const RecordRef *&);
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
	uint64_t scene_occurrence = 0;
};
thread_local Registry::ProductionPhysicalFrame *Registry::production_physical_frame = nullptr;

Error Registry::production_install_record_receiver(GDExtensionDeviceExitSceneReadyContextRef frame, GDExtensionProductionAttachmentRefV1 ref, ProductionRecordReceiver receiver, void *userdata, ProductionMetadataReceiver metadata_receiver) {
	if (!receiver) {
		return ERR_INVALID_PARAMETER;
	}
	Lock lock(*this);
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
	Lock lock(*this);
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

GDExtensionDeviceSessionResultV1 Registry::production_raster_reserve(GDExtensionProductionAttachmentRefV1 ref, const GDExtensionProductionOperationV1 *operation, const GDExtensionProductionCapacityV1 *capacity, const GDExtensionProductionRasterActionV1 *action, GDExtensionProductionRequestRefV1 *out) {
	if (!window_aligned(out)) {
		return window_result(ERR_INVALID_PARAMETER);
	}
	*out = {};
	if (!window_aligned(operation) || !window_aligned(capacity) || !window_aligned(action) || !window_aligned(out) || !operation || !capacity || !action || !out || out->registry_instance || out->handle_serial || !production_dto_valid(*operation) || !production_dto_valid(*capacity) || !production_dto_valid(*action) || action->reserved || action->target_content_revision || action->action_ordinal != 0 || action->framebuffer_resource_slot != 0 || action->depth_resource_slot != 1 || action->shader_serial || action->pipeline_serial || action->framebuffer_serial || capacity->suboperations < 1 || capacity->resources < 4 || capacity->return_cells < 4 || capacity->graph_memberships < 8 || action->canonical_suboperation < operation->canonical_begin || action->canonical_suboperation >= operation->canonical_end) {
		return window_result(ERR_INVALID_PARAMETER);
	}
	Lock lock(*this);
	ProductionAttachment *attachment = nullptr;
	RenderingDevice *device = nullptr;
	Error error = production_attachment_locked(ref, true, attachment, device);
	if (error != OK) {
		return window_result(error);
	}
	if (!attachment->opened || !attachment->record_receiver) {
		return window_result(ERR_UNAVAILABLE);
	}
	// Reserve one real accepted arena request through the same metadata
	// allocator. Raster action ownership remains typed and independent; no
	// dispatch row is synthesized and no qualification ticket is consulted.
	GDExtensionProductionActionV1 no_actions[1] = {};
	GDExtensionProductionResourceV1 no_resources[1] = {};
	GDExtensionProductionUseV1 no_uses[1] = {};
	GDExtensionProductionReadbackV1 no_readbacks[1] = {};
	ProductionRequest *request = nullptr;
	error = production_reserve_metadata_rows_locked(*attachment, *operation, *capacity, { no_actions, nullptr }, 0, no_resources, 0, no_uses, 0, { no_readbacks, nullptr, operation->canonical_begin }, 0, request, 2);
	if (error != OK || !request) {
		return window_result(error);
	}
	request->raster_action = *action;
	request->raster_reserved = true;
	// A raster reservation is unfinished work, never an empty successful record.
	// Independent request-owned raster preparation must fill these exact cells;
	// no qualification resource import can prepare this action.
	request->action_count = 1;
	request->resource_count = 4;
	auto &row = request->actions[0].declaration;
	row.action_ordinal = 0;
	row.canonical_suboperation = action->canonical_suboperation;
	row.action_kind = GDEXTENSION_PRODUCTION_RASTER_FRAME_V1;
	row.list_boundary = GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1;
	for (uint32_t i = 0; i < 4; ++i) {
		request->resources[i].request_serial = request->serial;
		request->resources[i].declaration.resource_kind = GDEXTENSION_PRODUCTION_RASTER_PHYSICAL_V1;
		request->resources[i].declaration.ownership = GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1;
	}
	*out = { instance, request->serial };
	return window_result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::production_raster_begin(ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, uint64_t revision) {
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_physical_request_locked(context, ref, request, rd, true);
	if (error != OK) {
		return window_result(error);
	}
	if (!request->raster_reserved || request->action_count != 1 || request->resource_count != 4 || !request->frozen || !request->actions[0].prepared || request->raster_started || request->next_action || revision != request->revision || request->capture_inflight || request->effects_pending || request->source_device_lost || request->revision > UINT64_MAX - 32 || request->graph_count > request->capacity.graph_memberships || request->capacity.graph_memberships - request->graph_count < 8) {
		return window_result(ERR_INVALID_DATA);
	}
	if (!rd->_thread_safe_.try_lock()) {
		return window_result(ERR_BUSY);
	}
	if (rd->production_recording_request || rd->draw_list.active || rd->compute_list.active || rd->raytracing_list.active || rd->_recording_error() != OK || rd->draw_graph.has_driver_callbacks()) {
		rd->_thread_safe_.unlock();
		return window_result(ERR_BUSY);
	}
	for (uint32_t i = 0; i < 4; ++i) {
		const auto &slot = request->resources[i];
		if (!slot.imported || slot.declaration.ownership != GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1 || slot.uses[0].physical != &slot.owned_physical || !slot.owned_serial || slot.owned_physical.bridge != request->attachment->serial || slot.owned_physical.child != slot.owned_serial || slot.use_count != 1 || !slot.uses[0].same_physical() || !slot.uses[0].linked() || slot.uses[0].physical->use_closed.load()) {
			rd->_thread_safe_.unlock();
			return window_result(ERR_UNAVAILABLE);
		}
	}
	const auto &fb_slot = request->resources[0];
	const auto &depth_slot = request->resources[1];
	const auto &shader_slot = request->resources[2];
	const auto &pipeline_slot = request->resources[3];
	const auto *framebuffer = rd->framebuffer_owner.get_or_null(fb_slot.primary);
	const auto *texture = rd->texture_owner.get_or_null(depth_slot.primary);
	const auto *shader = rd->shader_owner.get_or_null(shader_slot.primary);
	const auto *pipeline = rd->render_pipeline_owner.get_or_null(pipeline_slot.primary);
	if (fb_slot.backing_serial != request->raster_action.framebuffer_serial || shader_slot.backing_serial != request->raster_action.shader_serial || pipeline_slot.backing_serial != request->raster_action.pipeline_serial || !framebuffer || framebuffer->qualification_return != &fb_slot.owned_physical || framebuffer->qualification_dependency != &depth_slot.owned_physical || framebuffer->texture_ids.size() != 1 || framebuffer->texture_ids[0] != depth_slot.primary || !texture || texture->qualification_return != &depth_slot.owned_physical || texture->format != RenderingDevice::DATA_FORMAT_D32_SFLOAT || !shader || shader->qualification_return != &shader_slot.owned_physical || !pipeline || pipeline->qualification_return != &pipeline_slot.owned_physical || pipeline->qualification_dependency != &shader_slot.owned_physical) {
		rd->_thread_safe_.unlock();
		return window_result(ERR_INVALID_DATA);
	}
	// The captured begin has at most one preparation, two prefix, begin and
	// clear handles; the current checked end adds one handle. Eight prequoted
	// cells cover that entire frame before the first effect.
	++production_physical_frame->append_calls;
	++request->capture_inflight;
	production_append_effect_enter_locked(*request);
	rd->production_recording_request = request;
	const OriginalOperationIdentity identity{ request->operation.owner_request_nonce, request->attachment->library_generation, device_records[request->attachment->device].session, request->operation.record_generation, request->operation.canonical_begin, request->operation.canonical_end };
	unlock_mutex_for_call();
	const auto begun = rd->_draw_list_begin_recorded(request->resources[0].primary, RenderingDevice::DRAW_CLEAR_DEPTH, {}, 1.0f, 0, Rect2(), 0);
	relock_mutex_after_call();
	rd->production_recording_request = nullptr;
	--request->capture_inflight;
	request->draw_list_open = rd->draw_list.active;
	request->raster_started = request->draw_list_open || begun.effect != RenderingDevice::NativeAppendEffect::NoNewEffect;
	auto capture = [&](const RDG::RecordHandle &handle) {
		if (!handle.is_valid()) {
			return;
		}
		CheckedRecordAppend observed;
		observed.state = CheckedRecordAppend::State::Appended;
		observed.handle = handle;
		if (bind_checked_record_append(observed, identity) != OK || production_capture_graph_locked(*request, *rd, 0, observed) != OK) {
			request->effects_pending = true;
		}
	};
	for (uint32_t i = 0; i < begun.preparation_count; ++i) {
		capture(begun.preparation_handles[i]);
	}
	for (uint32_t i = 0; i < begun.prefix_count; ++i) {
		capture(begun.prefix_handles[i]);
	}
	capture(begun.begin_handle);
	capture(begun.clear_handle);
	if (request->raster_started && (begun.error != OK || !begun.capture_complete || begun.effect == RenderingDevice::NativeAppendEffect::Unknown || !begun.begin_handle.is_valid() || !begun.clear_handle.is_valid())) {
		request->effects_pending = true;
	}
	++request->revision;
	rd->_thread_safe_.unlock();
	return window_result(request->effects_pending ? ERR_BUSY : begun.error);
}

GDExtensionDeviceSessionResultV1 Registry::production_raster_end(ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref) {
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_physical_request_locked(context, ref, request, rd, false);
	if (error != OK) {
		return window_result(error);
	}
	if (!request->raster_reserved || !request->draw_list_open) {
		return window_result(ERR_INVALID_DATA);
	}
	if (!rd->_thread_safe_.try_lock()) {
		return window_result(ERR_BUSY);
	}
	error = production_raster_close_locked(*request, *rd);
	rd->_thread_safe_.unlock();
	return window_result(error);
}

Error Registry::production_raster_close_locked(ProductionRequest &request, RenderingDevice &rd) {
	if (!request.raster_reserved || !request.draw_list_open || request.capture_inflight || rd.render_thread_id != Thread::get_caller_id() || !rd.draw_list.active || rd.compute_list.active || rd.raytracing_list.active || rd.production_recording_request || request.revision > UINT64_MAX - 4 || request.graph_count >= request.capacity.graph_memberships) {
		request.effects_pending = true;
		return ERR_BUSY;
	}
	const OriginalOperationIdentity identity{ request.operation.owner_request_nonce, request.attachment->library_generation, device_records[request.attachment->device].session, request.operation.record_generation, request.operation.canonical_begin, request.operation.canonical_end };
	++request.capture_inflight;
	production_append_effect_enter_locked(request);
	rd.production_recording_request = &request;
	unlock_mutex_for_call();
	const auto ended = rd._draw_list_end_recorded();
	relock_mutex_after_call();
	rd.production_recording_request = nullptr;
	--request.capture_inflight;
	request.draw_list_open = rd.draw_list.active;
	auto capture = [&](const RDG::RecordHandle &handle) {
		if (!handle.is_valid()) {
			return;
		}
		CheckedRecordAppend observed;
		observed.state = CheckedRecordAppend::State::Appended;
		observed.handle = handle;
		if (bind_checked_record_append(observed, identity) != OK || production_capture_graph_locked(request, rd, 0, observed) != OK) {
			request.effects_pending = true;
		}
	};
	for (uint32_t i = 0; i < ended.prefix_count; ++i) {
		capture(ended.prefix_handles[i]);
	}
	capture(ended.draw_handle);
	if (ended.error != OK || ended.effect != RenderingDevice::NativeAppendEffect::Recorded || !ended.capture_complete || !ended.draw_handle.is_valid() || request.draw_list_open) {
		request.effects_pending = true;
	}
	if (!request.effects_pending) {
		request.next_action = 1;
	}
	++request.revision;
	return request.effects_pending ? ERR_BUSY : OK;
}

GDExtensionDeviceSessionResultV1 Registry::production_reserve_v2(GDExtensionProductionAttachmentRefV1 ref, const GDExtensionProductionOperationV1 *operation, const GDExtensionProductionCapacityV1 *capacity, const GDExtensionProductionActionV2 *actions, uint32_t action_count, const GDExtensionProductionResourceV1 *resources, uint32_t resource_count, const GDExtensionProductionUseV1 *uses, uint32_t use_count, const GDExtensionProductionReadbackV2 *readbacks, uint32_t readback_count, GDExtensionProductionRequestRefV1 *out) {
	if (!window_aligned(operation) || !window_aligned(capacity) || !window_aligned(out) || out->registry_instance || out->handle_serial) {
		return window_result(ERR_INVALID_PARAMETER);
	}
	Lock lock(*this);
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
	error = production_reserve_metadata_rows_locked(*attachment, *operation, *capacity, { nullptr, actions }, action_count, resources, resource_count, uses, use_count, { nullptr, readbacks, operation->canonical_begin }, readback_count, request, 2);
	if (error == OK) {
		*out = { instance, request->serial };
	}
	return window_result(error);
}

// Native attempts are monotonic request history. A later empty control cannot
// turn earlier work into a no-effect certificate. Call with Registry locked,
// immediately before the original effect-capable call, never at admission.
void Registry::production_native_effect_enter_locked(ProductionRequest &request) {
	request.native_effect_entered = true;
	if (request.control.input.kind == ProductionControlKind::RECORD && request.control.state == ProductionControlState::EXECUTING) {
		request.control.record_history.control_effect_entered = true;
	}
}

// Only original append-capable recording paths call this. Allocation, native
// preparation and disposal keep their separate native obligation history.
void Registry::production_append_effect_enter_locked(ProductionRequest &request) {
	production_native_effect_enter_locked(request);
	request.append_effect_entered = true;
	if (request.control.input.kind == ProductionControlKind::RECORD && request.control.state == ProductionControlState::EXECUTING) {
		request.control.record_history.control_append_entered = true;
	}
}

GDExtensionDeviceSessionResultV1 Registry::production_record_history(GDExtensionProductionRequestRefV1 ref, uint64_t serial, GDExtensionProductionRecordHistoryV1 *out) {
	if (!window_aligned(out)) {
		return window_result(ERR_INVALID_PARAMETER);
	}
	*out = {};
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	const Error error = production_request_locked(ref, false, request, device);
	if (error != OK) {
		return window_result(error);
	}
	const auto &cell = request->control;
	if (!serial || cell.serial != serial || cell.input.kind != ProductionControlKind::RECORD || cell.input.request_serial != request->serial || cell.input.record_generation != request->operation.record_generation) {
		return window_result(ERR_DOES_NOT_EXIST);
	}
	out->abi = production_dto_abi<GDExtensionProductionRecordHistoryV1>();
	out->request_serial = request->serial;
	out->control_serial = serial;
	out->record_generation = request->operation.record_generation;
	switch (cell.state) {
		case ProductionControlState::PENDING:
			out->state = GDEXTENSION_PRODUCTION_RECORD_HISTORY_PENDING_V1;
			break;
		case ProductionControlState::QUEUED:
			out->state = GDEXTENSION_PRODUCTION_RECORD_HISTORY_QUEUED_V1;
			break;
		case ProductionControlState::EXECUTING:
			out->state = GDEXTENSION_PRODUCTION_RECORD_HISTORY_EXECUTING_V1;
			break;
		case ProductionControlState::RETURNED:
			out->state = GDEXTENSION_PRODUCTION_RECORD_HISTORY_RETURNED_STATE_V1;
			break;
		case ProductionControlState::OWNER_CONSUMED:
			out->state = GDEXTENSION_PRODUCTION_RECORD_HISTORY_OWNER_CONSUMED_STATE_V1;
			break;
	}
	const auto &history = cell.record_history;
	const bool returned = cell.state == ProductionControlState::RETURNED || cell.state == ProductionControlState::OWNER_CONSUMED;
	if (history.schedule_attempted) {
		out->flags |= GDEXTENSION_PRODUCTION_RECORD_HISTORY_SCHEDULE_ATTEMPTED_V1;
	}
	if (history.queue_possible && !returned) {
		out->flags |= GDEXTENSION_PRODUCTION_RECORD_HISTORY_QUEUE_POSSIBLE_V1;
	}
	if (history.admitted) {
		out->flags |= GDEXTENSION_PRODUCTION_RECORD_HISTORY_ADMITTED_V1;
	}
	if (history.schedule_result_valid) {
		out->flags |= GDEXTENSION_PRODUCTION_RECORD_HISTORY_SCHEDULE_RESULT_VALID_V1;
		out->last_schedule_result = history.last_schedule_result;
	}
	if (history.control_effect_entered) {
		out->flags |= GDEXTENSION_PRODUCTION_RECORD_HISTORY_CONTROL_EFFECT_ENTERED_V1;
	}
	if (request->native_effect_entered) {
		out->flags |= GDEXTENSION_PRODUCTION_RECORD_HISTORY_REQUEST_EFFECT_ENTERED_V1;
	}
	if (history.control_append_entered) {
		out->flags |= GDEXTENSION_PRODUCTION_RECORD_HISTORY_CONTROL_APPEND_ENTERED_V1;
	}
	if (request->append_effect_entered) {
		out->flags |= GDEXTENSION_PRODUCTION_RECORD_HISTORY_REQUEST_APPEND_ENTERED_V1;
	}
	if (returned) {
		// Preparation-only effects may still require drain/release. Their native
		// latch does not imply business append, and no-append does not reset the
		// original Runtime's pre-activation AppendStarted/once transition.
		if (!history.control_append_entered && !request->append_effect_entered && !request->capture_inflight && !request->compute_list_open && !request->draw_list_open && !request->graph_count && !request->native_count) {
			out->flags |= GDEXTENSION_PRODUCTION_RECORD_HISTORY_TERMINAL_NO_APPEND_V1;
		}
		out->flags |= GDEXTENSION_PRODUCTION_RECORD_HISTORY_RETURNED_V1;
		out->record_result = cell.output.result;
		// Extra unresolved-state guards are conservative only. The affirmative
		// fact comes from the original latches and exact terminal control.
		if (!history.control_effect_entered && !request->native_effect_entered && !request->capture_inflight && !request->effects_pending && !request->source_device_lost && !request->compute_list_open && !request->draw_list_open && !request->graph_count && !request->native_count) {
			out->flags |= GDEXTENSION_PRODUCTION_RECORD_HISTORY_TERMINAL_NO_EFFECT_V1;
		}
	}
	if (cell.state == ProductionControlState::OWNER_CONSUMED) {
		out->flags |= GDEXTENSION_PRODUCTION_RECORD_HISTORY_OWNER_CONSUMED_V1;
	}
	return window_result(OK);
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
		Lock lock(*this);
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

GDExtensionDeviceSessionResultV1 Registry::production_record_consume_v2(GDExtensionProductionRequestRefV1 ref, uint64_t serial, GDExtensionProductionRecordReportV2 *out, uint32_t expected_contract) {
	const Error contract = production_contract_check(ref, expected_contract);
	if (contract != OK) {
		return { int32_t(contract), 0, 0, 0 };
	}
	if (!window_aligned(out)) {
		return window_result(ERR_INVALID_PARAMETER);
	}
	*out = {};
	{
		Lock lock(*this);
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
	out->abi = production_dto_abi<GDExtensionProductionRecordReportV2>();
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
	if (attachment.scene_registration && (attachment.scene_closed || !attachment.scene_occurrence)) {
		return ERR_UNAVAILABLE;
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
	unlock_mutex_for_call();
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
			if (attachment.scene_registration) {
				frame.scene_occurrence = attachment.scene_occurrence;
				GDExtensionNativeScenePhaseV1 phase{};
				phase.abi = { 1, sizeof(phase), 0 };
				phase.device_capability = attachment.scene_config.device_capability;
				phase.phase_token = scene_phase_opaque(instance, attachment.scene_registration, frame.scene_occurrence, frame.serial);
				phase.physical_context = scene_phase_opaque(instance, frame.serial, serial, control);
				phase.occurrence = frame.scene_occurrence;
				phase.expiry_serial = frame.serial;
				error = scene_phase_call_relay(attachment.scene_relay, phase, { instance, serial }, operation);
			} else {
				receiver(userdata, { instance, frame.serial }, { instance, serial }, &operation);
			}
			// Actual protected frame return is a physical cutoff, not a caller
			// list split or cancellation. Keep entry/CodeHold live through closure.
			relock_mutex_after_call();
			if (request.compute_list_open || request.draw_list_open) {
				if (!device._thread_safe_.try_lock()) {
					error = ERR_BUSY;
				} else {
					error = production_close_lists_locked(request, device);
					device._thread_safe_.unlock();
				}
			}
			if (error == OK && request.graph_count != request.segment_graph_begin && request.segment.segment == request.acknowledged_segment) {
				error = production_close_segment_locked(request, request.control.output.segment, true);
			} else {
				request.control.output.segment = request.segment;
			}
			if (error != OK || (request.compute_list_open || request.draw_list_open) || device.compute_list.active || device.draw_list.active) {
				request.effects_pending = true;
				if (error == OK) {
					error = ERR_BUSY;
				}
			}
			if (!request.effects_pending && request.next_action < request.action_count) {
				request.control.output.record_pause = request.control.output.append.state == GDEXTENSION_PRODUCTION_KNOWN_PREFIX_V1 ? ProductionRecordPause::UPLOAD_PREFIX : frame.append_calls >= 64 ? ProductionRecordPause::APPEND_LIMIT
																																																	  : ProductionRecordPause::SCOPE_RETURN;
			}
			unlock_mutex_for_call();
			if (frame.scene_occurrence) {
				relock_mutex_after_call();
				for (uint32_t i = 0; i < request.action_count; ++i) {
					auto &attempt = request.actions[i].scene_attempt;
					if (!attempt.consumed) {
						attempt.occurrence = attempt.physical_frame = 0;
					}
				}
				unlock_mutex_for_call();
			}
			production_physical_frame = frame.previous; // This physical context expires here.
		}
	}
	// The temporary original RecordRef is returned without holding Registry's
	// nonrecursive lock; the accepted request remains live through EXECUTING.
	reference.reset();
	relock_mutex_after_call();
	CRASH_COND(request.serial != serial || request.control.serial != control || request.control.state != ProductionControlState::EXECUTING);
	return error;
}

Error Registry::scene_phase_encode_relay(NormalRelayToken token, GDExtensionNormalRelayRefV1 &out) {
	Lock lock(*this);
	const auto *relay = find_normal_relay_locked(token);
	if (!relay || relay->cancelled || relay->releasing || relay->release_requested) {
		return ERR_UNAVAILABLE;
	}
	const CleanupAction *cleanup = nullptr;
	const RecordRef *owner = nullptr;
	if (!scene_phase_callable_metadata(relay->callback, token, cleanup, owner) || !scene_phase_holder_matches_locked(*cleanup, *owner, token, false)) {
		return ERR_UNAUTHORIZED;
	}
	out.token = scene_phase_opaque(instance, relay->serial, SCENE_RELAY_KIND, relay->generation);
	return OK;
}

Error Registry::scene_phase_capability(GDExtensionProductionAttachmentRefV1 ref, GDExtensionSceneOpaque32V1 &out) {
	Lock lock(*this);
	ProductionAttachment *attachment = nullptr;
	RenderingDevice *device = nullptr;
	const Error error = production_attachment_locked(ref, true, attachment, device);
	if (error != OK) {
		return error;
	}
	out = scene_phase_opaque(instance, device_records[attachment->device].session, attachment->epoch, attachment->library_generation);
	return OK;
}

Error Registry::scene_phase_register(GDExtensionNormalRelayRefV1 relay_ref, GDExtensionProductionAttachmentRefV1 ref, const GDExtensionScenePhaseSinkConfigV1 &config, GDExtensionScenePhaseRegistrationRefV1 &out) {
	const GDExtensionScenePhaseRegistrationRefV1 empty{};
	if (!scene_phase_config_valid(config) || memcmp(&out, &empty, sizeof(out))) {
		return ERR_INVALID_PARAMETER;
	}
	Lock lock(*this);
	ProductionAttachment *attachment = nullptr;
	RenderingDevice *device = nullptr;
	Error error = production_attachment_locked(ref, true, attachment, device);
	if (error != OK) {
		return error;
	}
	const NormalRelayToken relay_token{ scene_phase_word(relay_ref.token, 0), scene_phase_word(relay_ref.token, 1) };
	auto *relay = find_normal_relay_locked(relay_token);
	const CleanupAction *cleanup = nullptr;
	const RecordRef *owner = nullptr;
	if (!relay || scene_phase_word(relay_ref.token, 2) != SCENE_RELAY_KIND || scene_phase_word(relay_ref.token, 3) != relay->generation || relay->record != attachment->library || relay->generation != attachment->library_generation || relay->cancelled || relay->releasing || relay->release_requested || relay->callback.is_null() || !scene_phase_callable_metadata(relay->callback, relay_token, cleanup, owner) || !scene_phase_holder_matches_locked(*cleanup, *owner, relay_token, false)) {
		return ERR_UNAUTHORIZED;
	}
	// Opaque bytes are compared with this original device owner, never accepted
	// as proof by themselves. Exact encoding is exposed by the registration view.
	const auto expected = scene_phase_opaque(instance, device_records[attachment->device].session, attachment->epoch, attachment->library_generation);
	if (memcmp(&config.device_capability, &expected, sizeof(expected))) {
		return ERR_INVALID_DATA;
	}
	for (auto *existing : production_attachments) {
		if (!existing || !existing->scene_registration || existing->scene_relay.serial != relay_token.serial || existing->held_device != device || existing->scene_config.phase != config.phase) {
			continue;
		}
		if (existing->scene_closed || memcmp(&existing->scene_config, &config, sizeof(config))) {
			return ERR_ALREADY_IN_USE;
		}
		out.token = scene_phase_opaque(instance, existing->scene_registration, SCENE_REGISTRATION_KIND, existing->library_generation);
		return OK; // Exact same relay/device/phase never installs another sink.
	}
	if (attachment->scene_registration) {
		if (attachment->scene_closed || attachment->scene_relay.serial != relay_token.serial || memcmp(&attachment->scene_config, &config, sizeof(config))) {
			return ERR_ALREADY_IN_USE;
		}
		out.token = scene_phase_opaque(instance, attachment->scene_registration, SCENE_REGISTRATION_KIND, attachment->library_generation);
		return OK;
	}
	uint64_t serial = 0;
	if (!serials.take(serial)) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	attachment->scene_relay = relay_token;
	attachment->scene_config = config;
	attachment->scene_registration = serial;
	out.token = scene_phase_opaque(instance, serial, SCENE_REGISTRATION_KIND, attachment->library_generation);
	return OK;
}

Error Registry::scene_phase_unregister(GDExtensionScenePhaseRegistrationRefV1 ref, GDExtensionScenePhaseCloseV1 &out) {
	out = {};
	NormalRelayToken token;
	{
		Lock lock(*this);
		if (scene_phase_word(ref.token, 0) != instance || !scene_phase_word(ref.token, 1) || scene_phase_word(ref.token, 2) != SCENE_REGISTRATION_KIND) {
			return ERR_INVALID_DATA;
		}
		ProductionAttachment *found = nullptr;
		for (auto *attachment : production_attachments) {
			if (attachment && attachment->scene_registration == scene_phase_word(ref.token, 1) && attachment->library_generation == scene_phase_word(ref.token, 3)) {
				found = attachment;
				break;
			}
		}
		if (!found) {
			return ERR_DOES_NOT_EXIST;
		}
		found->scene_closed = true; // No new delivery or unused attempt can append.
		out.abi = { 1, sizeof(out), 0 };
		out.registration_closed = 1;
		out.active_deliveries = found->scene_active;
		// Wake waiting original RECORD mailboxes through their existing FIFO.
		// They return refusal without another draw or a new control serial.
		auto *server = Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton());
		for (uint32_t i = 0; i < found->quote.capacity.requests; ++i) {
			auto &request = found->requests[i];
			if (request.serial && request.control.scene_waiting && request.control.state == ProductionControlState::QUEUED && request.control.input.kind == ProductionControlKind::RECORD) {
				if (!server || !server->_try_production_control_dispatch(instance, found->serial, request.serial, request.control.serial)) {
					return ERR_BUSY;
				}
				return ERR_BUSY; // Do not acknowledge close until every original mailbox returned.
			}
		}
		if (found->scene_relay_closed) {
			out.abi = { 1, sizeof(out), 0 };
			out.registration_closed = out.relay_cell_released = out.relay_holder_closed = 1;
			return OK; // Cached original observed acknowledgement, not a new claim.
		}
		token = found->scene_relay;
		out.abi = { 1, sizeof(out), 0 };
		out.registration_closed = 1;
		out.active_deliveries = found->scene_active;
	}
	// Reuse the original Callable/holder destruction-thread acknowledgement.
	const Error cancelled = cancel_normal_relay(token);
	if (cancelled != OK) {
		return cancelled;
	}
	const Error released = release_normal_relay_holder(token);
	NormalRelaySnapshot observation;
	const Error observed = observe_normal_relay(token, observation);
	if (observed != OK) {
		return observed;
	}
	out.active_deliveries = MAX(out.active_deliveries, observation.active_deliveries);
	out.relay_cell_released = observation.cell_released;
	out.relay_holder_closed = observation.holder_closed;
	if (released != OK || out.active_deliveries || !observation.cell_released || !observation.holder_closed) {
		return ERR_BUSY;
	}
	const Error forgotten = forget_normal_relay(token);
	if (forgotten != OK) {
		return forgotten;
	}
	Lock lock(*this);
	for (auto *attachment : production_attachments) {
		if (attachment && attachment->scene_registration == scene_phase_word(ref.token, 1) && attachment->library_generation == scene_phase_word(ref.token, 3)) {
			attachment->scene_relay_closed = true;
			return OK;
		}
	}
	return ERR_DOES_NOT_EXIST;
}

void Registry::scene_phase_dispatch(RenderingDevice &device) {
	if (device.render_thread_id != Thread::get_caller_id() || !device.is_main_instance || device.production_finalization_started.load(std::memory_order_acquire) || device._recording_error() != OK) {
		return;
	}
	Lock lock(*this);
	if (scene_phase_occurrence == UINT64_MAX) {
		return;
	} // Exhaustion never wraps.
	const uint64_t occurrence = ++scene_phase_occurrence;
	for (auto *attachment : production_attachments) {
		if (!attachment || !attachment->scene_registration || attachment->held_device != &device || attachment->scene_active) {
			continue;
		}
		uint32_t delivered = 0;
		for (uint32_t i = 0; i < attachment->quote.capacity.requests && delivered < attachment->scene_config.max_operations; ++i) {
			auto &request = attachment->requests[i];
			auto &control = request.control;
			if (!request.serial || !control.scene_waiting || control.state != ProductionControlState::QUEUED || control.input.kind != ProductionControlKind::RECORD) {
				continue;
			}
			// Original FIFO admission has already validated and retained this
			// mailbox. Actual phase is issued only here, never by that admission.
			attachment->scene_occurrence = occurrence;
			control.scene_waiting = false;
			++attachment->scene_active;
			production_control_execute_locked(request, &device);
			--attachment->scene_active;
			attachment->scene_occurrence = 0;
			++delivered;
		}
	}
}

// Called by the original E09 FullState exporter after it has checked its actual
// FullRecordingJoin, accepted settlement cursor and mapping schedule_digest.
// D retains the exact transport postimage; it does not issue logical identity.
Error Registry::scene_phase_bind_operation(GDExtensionProductionRequestRefV1 ref, uint64_t ordinal, const GDExtensionSceneOperationKeyV1 &key) {
	if (key.abi.abi_version != 1 || key.abi.struct_size != sizeof(key) || key.abi.reserved || !key.owner_generation) {
		return ERR_INVALID_PARAMETER;
	}
	const GDExtensionSceneOpaque32V1 empty{};
	if (!memcmp(&key.record_request, &empty, sizeof(empty)) || !memcmp(&key.operation_ref, &empty, sizeof(empty)) || !memcmp(&key.cursor_ref, &empty, sizeof(empty)) || !memcmp(&key.schedule_digest, &empty, sizeof(empty))) {
		return ERR_INVALID_PARAMETER;
	}
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	const Error error = production_request_locked(ref, true, request, device);
	if (error != OK) {
		return error;
	}
	const auto *entry = RegistrationContext::current;
	if (!entry || !entry->admitted || !entry->reference || entry->reference->registry != this || entry->reference->slot != request->attachment->library) {
		return ERR_UNAUTHORIZED;
	}
	if (!production_dto_valid(key.transport_operation) || memcmp(&key.transport_operation, &request->operation, sizeof(request->operation))) {
		return ERR_INVALID_DATA;
	}
	if (ordinal >= request->action_count || !request->attachment->scene_registration || request->attachment->scene_closed) {
		return ERR_INVALID_DATA;
	}
	auto &action = request->actions[ordinal];
	if (action.scene_key_bound) {
		return memcmp(&action.scene_key, &key, sizeof(key)) ? ERR_INVALID_DATA : OK;
	}
	if (request->frozen || request->control.state != ProductionControlState::OWNER_CONSUMED) {
		return ERR_BUSY;
	}
	// The existing original E09 FullState exporter has no callable opaque
	// operation/cursor postimage yet. Arbitrary caller bytes are not that grant.
	// Keep real Scene read admission closed until its original issuer is joined.
	return ERR_UNAVAILABLE;
}

Error Registry::scene_phase_borrow(ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, uint64_t ordinal, const GDExtensionSceneOperationKeyV1 &key, const GDExtensionSceneOpaque32V1 &sample, const Ref<SceneReadContent> &content, GDExtensionSceneReadOutcomeV1 &out) {
	out = {};
	out.abi = { 1, sizeof(out), 0 };
	auto fail = [&](Error error) { out.native_error = error; return error; };
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	Error error = production_physical_request_locked(context, ref, request, device, false);
	if (error != OK) {
		return fail(error);
	}
	if (ordinal >= request->action_count) {
		return fail(ERR_INVALID_DATA);
	}
	auto &action = request->actions[ordinal];
	if (!action.scene_key_bound || memcmp(&action.scene_key, &key, sizeof(key))) {
		return fail(ERR_INVALID_DATA);
	}
	// Query the original accepted append before minting a phase attempt, even
	// while owner report observation/cursor settlement is delayed.
	if (action.original_append.state != CheckedRecordAppend::State::Rejected) {
		if (action.original_append.state != CheckedRecordAppend::State::Appended) {
			out.detail = 1;
			return fail(ERR_CANT_CREATE);
		} // UnknownExecutionOutcome; original failure retirement, no permit.
		out.outcome = 2; // AlreadyRecorded: only the immutable original fact.
		out.record_fact = scene_phase_opaque(instance, request->serial, ordinal, action.original_append_fact_serial);
		return OK;
	}
	error = production_physical_request_locked(context, ref, request, device, true);
	if (error != OK) {
		return fail(error);
	}
	const auto &frame = *production_physical_frame;
	if (!frame.scene_occurrence || request->attachment->scene_closed || !content.is_valid()) {
		return fail(ERR_UNAUTHORIZED);
	}
	if (ordinal != request->next_action) {
		return fail(ERR_INVALID_DATA);
	}
	auto &attempt = action.scene_attempt;
	if (attempt.content.is_valid() && (attempt.content != content || memcmp(&attempt.sample, &sample, sizeof(sample)))) {
		return fail(ERR_INVALID_DATA);
	}
	error = content->validate(request->attachment->scene_config.device_capability, sample);
	if (error != OK) {
		return fail(error);
	}
	if (attempt.consumed) {
		return fail(ERR_UNAUTHORIZED);
	}
	attempt.content = content;
	attempt.sample = sample;
	attempt.occurrence = frame.scene_occurrence;
	attempt.physical_frame = context.frame_serial;
	out.outcome = 1; // Permit: repeated exact attempt returns identical bytes.
	out.phase_borrow = scene_phase_opaque(instance, request->attachment->scene_registration, frame.scene_occurrence, frame.serial);
	out.record_permit = scene_phase_opaque(instance, request->serial, ordinal, frame.serial);
	return OK;
}

Error Registry::scene_phase_query_operation(GDExtensionProductionRequestRefV1 ref, uint64_t ordinal, const GDExtensionSceneOperationKeyV1 &key, GDExtensionSceneReadOutcomeV1 &out) {
	out = {};
	out.abi = { 1, sizeof(out), 0 };
	auto fail = [&](Error error) { out.native_error = error; return error; };
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	const Error error = production_request_locked(ref, false, request, device);
	if (error != OK) {
		return fail(error);
	}
	if (ordinal >= request->action_count) {
		return fail(ERR_INVALID_DATA);
	}
	const auto &action = request->actions[ordinal];
	if (!action.scene_key_bound || memcmp(&action.scene_key, &key, sizeof(key))) {
		return fail(ERR_INVALID_DATA);
	}
	if (action.original_append.state == CheckedRecordAppend::State::Rejected) {
		return fail(ERR_BUSY);
	} // NotIssued needs a real new phase.
	if (action.original_append.state != CheckedRecordAppend::State::Appended) {
		out.detail = 1;
		return fail(ERR_CANT_CREATE);
	}
	out.outcome = 2;
	out.record_fact = scene_phase_opaque(instance, request->serial, ordinal, action.original_append_fact_serial);
	return OK; // No permit, borrow, cursor mutation or native call.
}

Error Registry::production_output_pin_current(RenderingDevice &expected_device, ProductionOutputPin &pin) {
	if (!pin.is_empty()) {
		return ERR_ALREADY_IN_USE;
	}
	const auto *frame = production_physical_frame;
	if (!frame || frame->registry != this || !frame->request) {
		return ERR_UNAUTHORIZED;
	}
	// This thread's original executing control retains the request across the
	// callback. Do not acquire mutex here: production_output_pin owns its lock
	// and authenticates the exact protected entry, control, device and thread.
	return production_output_pin({ instance, frame->serial }, { instance, frame->request->serial }, expected_device, pin);
}

// Caller holds Registry mutex. Authenticate only; never mint another pin.
Error Registry::production_output_owner_context_locked(const ProductionOutputPin &pin, RenderingDevice &expected_device) {
	auto *frame = production_physical_frame;
	if (!frame || frame->registry != this || !frame->request) {
		return ERR_UNAUTHORIZED;
	}
	ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	const Error error = production_physical_request_locked({ instance, frame->serial }, { instance, pin.request_serial }, request, device, true);
	if (error != OK) {
		return error;
	}
	return request == pin.request && device == &expected_device ? OK : ERR_UNAUTHORIZED;
}

Error Registry::production_output_pin(ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, RenderingDevice &expected_device, ProductionOutputPin &pin) {
	Lock lock(*this);
	if (!pin.is_empty()) {
		return ERR_ALREADY_IN_USE;
	}
	ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	const Error error = production_physical_request_locked(context, ref, request, device, true);
	if (error != OK) {
		return error;
	}
	// Schedule already froze all prepared rows before entering this callback.
	// This reservation precedes the first native effect; no graph fact is needed.
	if (device != &expected_device || !request->frozen || request->next_action || request->graph_count || request->native_count || request->capture_inflight || !request->next_output_pin_serial || request->use_count > request->capacity.resource_uses || request->binding_used > request->capacity.resource_uses - request->use_count) {
		return ERR_BUSY;
	}
	const uint32_t begin = request->use_count + request->binding_used;
	for (uint32_t i = begin; i < request->capacity.resource_uses; ++i) {
		auto &cell = request->uses[i];
		if (cell.request != request || cell.dependency.physical || cell.output_role != ProductionUseCell::OutputRole::UNUSED || cell.output_consumer || cell.output_pin_serial) {
			continue;
		}
		const uint64_t serial = request->next_output_pin_serial;
		request->next_output_pin_serial = serial == UINT64_MAX ? 0 : serial + 1;
		pin.registry = this;
		pin.request = request;
		pin.request_serial = request->serial;
		pin.serial = serial;
		pin.cell_index = i;
		cell.output_role = ProductionUseCell::OutputRole::NATIVE_CONSUMER;
		cell.output_consumer = &pin;
		cell.output_pin_serial = serial;
		return OK;
	}
	return ERR_OUT_OF_MEMORY; // No spare prequoted use cell; never allocate or borrow payload.
}

bool Registry::production_output_graph_command_locked(const ProductionRequest &request, const RenderingDevice &device, uint32_t graph, uint32_t command) const {
	uint32_t actual = UINT32_MAX;
	return graph < request.graph_count && device.draw_graph._resolve_record_command_index(request.graphs[graph].handle, actual) && actual == command;
}

bool Registry::production_output_recorded_ordered_locked(const ProductionRequest &request, const RenderingDevice &device) const {
	if (request.cancelled || request.capture_inflight || request.next_action != request.action_count || !request.graph_count || (request.compute_list_open || request.draw_list_open) || request.effects_pending) {
		return false;
	}
	for (uint32_t i = 0; i < request.graph_count; ++i) {
		uint32_t command = UINT32_MAX;
		if (!device.draw_graph._resolve_record_command_index(request.graphs[i].handle, command)) {
			return false;
		}
	}
	return true;
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
	if (append && (attachment.holder->pending || !record.attached || record.guard_enrollment != GuardEnrollment::QUALIFIED || attachment.closing || record.quiescing || record.close_started || request.cancelled || request.release_started || !native.live || frame->device->production_finalization_started.load(std::memory_order_acquire) || frame->append_calls >= (attachment.scene_registration ? attachment.scene_config.max_operations : 64u))) {
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
	uint64_t begin = 0;
	{
		Lock lock(*this);
		ProductionRequest *request = nullptr;
		RenderingDevice *device = nullptr;
		const Error error = production_physical_request_locked(context, ref, request, device, true);
		if (error != OK) {
			*out = {};
			return window_result(error);
		}
		if (request->contract_version != 1) {
			return window_result(ERR_INVALID_DATA);
		}
		begin = request->operation.canonical_begin;
	}
	GDExtensionProductionAppendV2 value = {};
	const auto result = production_append_v2(context, ref, canonical >= begin ? canonical - begin : UINT64_MAX, revision, &value, 1);
	*out = production_legacy_projection(value, begin);
	if (value.abi.abi_version) {
		out->canonical_suboperation = canonical;
	}
	return result;
}

GDExtensionDeviceSessionResultV1 Registry::production_append_v2(ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, uint64_t ordinal, uint64_t revision, GDExtensionProductionAppendV2 *out, uint32_t expected_contract) {
	if (!window_aligned(out)) {
		return window_result(ERR_INVALID_PARAMETER);
	}
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	Error error = production_physical_request_locked(context, ref, request, device, true);
	if (error != OK) {
		return window_result(error);
	}
	if (request->contract_version != expected_contract) {
		return window_result(ERR_INVALID_DATA);
	}
	*out = {};
	out->abi = production_dto_abi<GDExtensionProductionAppendV2>();
	out->action_ordinal = ordinal;
	out->revision = request->revision;
	if (request->raster_reserved || revision != request->revision || request->next_action >= request->action_count || ordinal != request->next_action) {
		return out->cause = window_result(ERR_INVALID_DATA);
	}
	out->canonical_suboperation = request->actions[ordinal].declaration.canonical_suboperation;
	out->source_recipe_substep = request->actions[ordinal].declaration.source_recipe_substep;
	if (!device->_thread_safe_.try_lock()) {
		return out->cause = window_result(ERR_BUSY);
	}
	OriginalOperationIdentity identity{ request->operation.owner_request_nonce, request->attachment->library_generation, device_records[request->attachment->device].session, request->operation.record_generation, request->operation.canonical_begin, request->operation.canonical_end };
	++production_physical_frame->append_calls;
	const uint32_t first_graph = request->graph_count;
	const uint32_t action_index = request->next_action;
	auto &original_action = request->actions[action_index];
	auto &attempt = original_action.scene_attempt;
	if (original_action.scene_key_bound && !attempt.content.is_valid()) {
		device->_thread_safe_.unlock();
		return out->cause = window_result(ERR_UNAUTHORIZED);
	}
	if (attempt.content.is_valid()) {
		if (attempt.consumed || !production_physical_frame->scene_occurrence || request->attachment->scene_closed || attempt.occurrence != production_physical_frame->scene_occurrence || attempt.physical_frame != context.frame_serial) {
			device->_thread_safe_.unlock();
			return out->cause = window_result(ERR_UNAUTHORIZED);
		}
		if (!original_action.original_append_fact_serial && !serials.take(original_action.original_append_fact_serial)) {
			device->_thread_safe_.unlock();
			return out->cause = window_result(ERR_PARAMETER_RANGE_ERROR);
		}
		attempt.consumed = true; // Same original RT bridge; no other append can race.
	}
	const auto observed = production_record_effect_locked(*request, *device, action_index, identity);
	if (attempt.content.is_valid() && observed.state == CheckedRecordAppend::State::Rejected) {
		attempt.consumed = false; // Not issued: this original attempt may retry.
	}
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
	uint64_t begin = 0;
	{
		Lock lock(*this);
		ProductionRequest *request = nullptr;
		RenderingDevice *device = nullptr;
		const Error error = production_physical_request_locked(context, ref, request, device, false);
		if (error != OK) {
			return window_result(error);
		}
		if (request->contract_version != 1) {
			return window_result(ERR_INVALID_DATA);
		}
		begin = request->operation.canonical_begin;
	}
	GDExtensionProductionSegmentV2 value = {};
	const auto result = production_close_segment_v2(context, ref, &value, 1);
	if (value.abi.abi_version) {
		*out = production_legacy_projection(value, begin);
	}
	return result;
}

GDExtensionDeviceSessionResultV1 Registry::production_close_segment_v2(ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, GDExtensionProductionSegmentV2 *out, uint32_t expected_contract) {
	if (!window_aligned(out)) {
		return window_result(ERR_INVALID_PARAMETER);
	}
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	Error error = production_physical_request_locked(context, ref, request, device, false);
	if (error == OK && request->contract_version != expected_contract) {
		return window_result(ERR_INVALID_DATA);
	}
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
	Lock lock(*this);
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

thread_local ProductionCleanupExtent *ProductionCleanupExtent::current = nullptr;
ProductionCleanupExtent::ProductionCleanupExtent() : previous(current) {
	current = this;
}
ProductionCleanupExtent::~ProductionCleanupExtent() {
	CRASH_COND(current != this);
	current = previous;
}

bool Registry::is_production_metadata_entry() {
	if (ProductionCleanupExtent::denies_new_work()) {
		return true;
	}
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
	Lock lock(*this);
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
		unlock_mutex_for_call();
		receiver(userdata, ref);
		// The attachment refuses Close while this accepted entry is active.
		// Original borrowed views expire before its code/private credit returns.
		frame.leave();
		relock_mutex_after_call();
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
