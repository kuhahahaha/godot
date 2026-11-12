/**************************************************************************/
/*  gdextension_camera_ready.cpp                                          */
/**************************************************************************/

#include "gdextension_camera_ready.h"

#include "core/extension/gdextension_production_request.h"
#include "core/os/os.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server_globals.h"

namespace GDExtensionDeviceExit {
void ProductionCameraReadyObserver::camera_source_retained(const Ref<RendererViewport::Viewport::CameraCapture> &p_capture) {
	if (closed || delivery.is_valid() || p_capture.is_null()) {
		return;
	}
	// The renderer pins this observer and source across the complete callback.
	// Any delayed original request must explicitly retain into its paid action.
	delivery = p_capture;
	last_error = registry.camera_ready_source_retained(*this);
	delivery.unref();
}

void ProductionCameraReadyObserver::camera_source_closed() {
	closed = true;
	// No DSO close callback or deferred draw is necessary. An executing native
	// delivery remains locally pinned; accepted actions own their separate Ref.
}

void ProductionCameraReadySubscriber::camera_source_retained(const Ref<RendererViewport::Viewport::CameraCapture> &p_capture) {
	if (!registry || closed || released || delivery.is_valid() || p_capture.is_null()) {
		return;
	}
	delivery = p_capture;
	last_error = registry->camera_ready_source_retained(*this);
	delivery.unref();
	// Last member access: releasing this pin may permit owner arena reclaim.
	registry->camera_ready_finish_delivery(*this);
}

void ProductionCameraReadySubscriber::camera_source_native_invocation(const RendererViewport::CameraInvocationScope &invocation) {
	if (!registry || closed || released || !subscribed) {
		return;
	}
	// The original registration and executing RECORD control retain the arena.
	// This entry calls only the Core executor; it never delivers the DSO relay.
	ProductionCameraInvocationBorrow borrow(invocation);
	last_error = registry->camera_ready_native_invocation(*this, borrow);
}

void ProductionCameraReadySubscriber::camera_source_native_pre_cull(const RendererViewport::CameraInvocationScope &invocation) {
	if (!registry || closed || released || !subscribed) {
		return;
	}
	ProductionCameraInvocationBorrow borrow(invocation);
	last_error = registry->camera_ready_native_pre_cull(*this, borrow);
}

Error Registry::camera_ready_native_pre_cull(ProductionCameraReadySubscriber &observer, const ProductionCameraInvocationBorrow &borrow) {
	const auto &invocation = borrow.get_scope();
	const auto &capture = invocation.get_capture();
	RenderingDevice *device = invocation.get_device();
	if (!device || device != RenderingDevice::get_singleton() || !device->native_immutable_is_render_thread() ||
			!invocation.is_pre_cull() || !invocation.is_current(&observer, capture) || invocation.is_recordable(&observer, capture)) {
		return ERR_UNAUTHORIZED;
	}
	// Keep an older cull-only capture alive until both original locks return.
	Ref<RendererViewport::Viewport::CameraCapture> returning;
	MutexLock device_lock(device->_thread_safe_);
	Lock lock(*this);
	if (observer.registry != this || observer.closed || observer.released || !observer.subscribed ||
			observer.request.registry_instance != instance || observer.attachment.registry_instance != instance ||
			!invocation.is_current(&observer, capture)) {
		return ERR_UNAUTHORIZED;
	}
	ProductionAttachment *attachment = nullptr;
	for (auto *candidate : production_attachments) {
		if (candidate && candidate->serial == observer.attachment.handle_serial) {
			attachment = candidate;
			break;
		}
	}
	if (!attachment || attachment->held_device != device || attachment->camera_ready_closed || attachment->camera_ready_close_active) {
		return ERR_UNAVAILABLE;
	}
	ProductionRequest *request = nullptr;
	for (uint32_t i = 0; i < attachment->quote.capacity.requests; ++i) {
		if (attachment->requests[i].serial == observer.request.handle_serial) {
			request = &attachment->requests[i];
			break;
		}
	}
	if (!request || request->attachment != attachment || observer.action_ordinal >= request->action_count) {
		return ERR_INVALID_DATA;
	}
	// Other subscribers and already recorded prefixes do not mutate this cull.
	if (!request->native_prepared_executor || !request->native_static_sealed || request->frozen) {
		return OK;
	}
	auto &action = request->actions[observer.action_ordinal];
	if (action.request != request || action.declaration.action_ordinal != observer.action_ordinal ||
			action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || &action.camera_ready_subscriber != &observer ||
			request->native_staged_anchor != observer.action_ordinal || !action.output_camera_pending) {
		return ERR_INVALID_DATA;
	}
	returning = action.scene_cull_capture;
	action.scene_cull_prepared = false;
	if (request->cancelled || request->release_started || request->effects_pending || request->source_device_lost ||
			request->capture_inflight || request->continuation_inflight || request->native_prepared_unread ||
			request->control.state != ProductionControlState::QUEUED || request->control.input.kind != ProductionControlKind::RECORD ||
			request->control.input.output_cancel ||
			request->control.input.request_serial != request->serial || request->control.input.record_generation != request->operation.record_generation) {
		action.scene_cull_error = ERR_BUSY;
		return ERR_BUSY;
	}
	action.scene_cull_error = production_prepare_staged_cull_locked(*request, action, *device, borrow);
	return action.scene_cull_error;
}

Error Registry::camera_ready_native_invocation(ProductionCameraReadySubscriber &observer, const ProductionCameraInvocationBorrow &borrow) {
	const auto &invocation = borrow.get_scope();
	const auto &capture = invocation.get_capture();
	RenderingDevice *device = invocation.get_device();
	if (!device || device != RenderingDevice::get_singleton() || !device->native_immutable_is_render_thread() ||
			!invocation.is_recordable(&observer, capture)) {
		return ERR_UNAUTHORIZED;
	}
	// Match the renderer and native executor lock order. No mutable DSO code
	// executes while the renderer owns this exact selected invocation.
	MutexLock device_lock(device->_thread_safe_);
	Lock lock(*this);
	if (observer.registry != this || observer.closed || observer.released || !observer.subscribed ||
			observer.request.registry_instance != instance || observer.attachment.registry_instance != instance ||
			!invocation.is_recordable(&observer, capture)) {
		return ERR_UNAUTHORIZED;
	}
	ProductionAttachment *attachment = nullptr;
	for (auto *candidate : production_attachments) {
		if (candidate && candidate->serial == observer.attachment.handle_serial) {
			attachment = candidate;
			break;
		}
	}
	if (!attachment || attachment->held_device != device || attachment->camera_ready_closed || attachment->camera_ready_close_active) {
		return ERR_UNAVAILABLE;
	}
	ProductionRequest *request = nullptr;
	for (uint32_t i = 0; i < attachment->quote.capacity.requests; ++i) {
		if (attachment->requests[i].serial == observer.request.handle_serial) {
			request = &attachment->requests[i];
			break;
		}
	}
	if (!request || request->attachment != attachment || !request->native_prepared_executor || (!request->frozen && !request->native_static_sealed) ||
			request->cancelled || request->release_started || request->effects_pending || request->source_device_lost ||
			request->capture_inflight || request->continuation_inflight || request->native_prepared_unread ||
			request->control.state != ProductionControlState::QUEUED || request->control.input.kind != ProductionControlKind::RECORD ||
			request->control.input.request_serial != request->serial || request->control.input.record_generation != request->operation.record_generation ||
			observer.action_ordinal >= request->action_count) {
		return ERR_BUSY;
	}
	auto &action = request->actions[observer.action_ordinal];
	if (action.request != request || action.declaration.action_ordinal != observer.action_ordinal ||
			action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || &action.camera_ready_subscriber != &observer ||
			action.preinput_camera_returned || (!action.output_camera_pending && action.preinput_camera_source.is_null()) ||
			action.camera_ready_viewport != capture->get_viewport()) {
		return ERR_INVALID_DATA;
	}
	if (action.host_view_prepared && (!action.host_view_received || capture->get_invocation().camera != action.host_view_camera ||
			capture->get_presentation_key().view_id != action.host_view_key.view_id || capture->get_presentation_key().view_epoch != action.host_view_key.view_epoch)) {
		return ERR_INVALID_DATA;
	}
	if (action.output_camera_pending) {
		if (!request->native_static_sealed || request->frozen || request->native_staged_anchor != observer.action_ordinal ||
				!action.output_plan_prepared || action.preinput_camera_source.is_valid() || !action.preinput_host_prepared || action.preinput_host_returned) { return ERR_INVALID_DATA; }
		// Prefix turns use the actual native scope, but do not bind its camera.
		// Original owner consume/apply/ACK may therefore occur between captures.
	}

	// Every genuine staged RECORD control spends the same immutable native
	// slice, including continuation controls after the current camera was bound.
	if (request->native_static_sealed) {
		if (request->native_staged_anchor != observer.action_ordinal) { return ERR_INVALID_DATA; }
		if (production_native_stage_charge_locked(*request, true) != OK) {
			production_native_record_reject_wait_locked(*request); return ERR_BUSY;
		}
	}

	// Execute the accepted original control even when its retained capture has
	// expired. The receiver authenticates that exact source before any action
	// and returns the real rejection through RETURNED, so owner consume/cancel
	// remains possible. This does not authorize recording with the new capture.
	request->control.scene_waiting = false;
	production_control_execute_locked(*request, device, &borrow);
	return Error(request->control.output.result.native_error);
}

Error Registry::production_native_stage_charge_locked(ProductionRequest &request, bool control) {
	if (!Lock::owns(*this) || !request.native_static_sealed || request.control.input.kind != ProductionControlKind::RECORD ||
			(control ? request.control.state != ProductionControlState::QUEUED : request.control.state != ProductionControlState::EXECUTING)) { return ERR_UNAUTHORIZED; }
	const auto &budget = request.native_stage_budget;
	if ((control && request.native_stage_controls_used >= budget.controls) || request.native_stage_passes_used >= budget.passes ||
			request.native_stage_receipts_used >= budget.receipts || !budget.deadline_micros || OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros) { return ERR_BUSY; }
	if (control) { ++request.native_stage_controls_used; request.native_stage_control = request.control.serial; }
	++request.native_stage_passes_used; ++request.native_stage_receipts_used;
	return OK;
}

Error Registry::production_complete_staged_camera_locked(ProductionRequest &request, ProductionActionCell &action, const ProductionCameraInvocationBorrow &borrow) {
	const auto &invocation = borrow.get_scope();
	const auto &capture = invocation.get_capture();
	if (!Lock::owns(*this) || !request.native_prepared_executor || !request.native_static_sealed || request.frozen ||
			request.native_staged_anchor != action.declaration.action_ordinal || request.next_action > action.declaration.action_ordinal ||
			action.request != &request || !action.output_camera_pending || !action.output_plan_prepared ||
			action.preinput_camera_source.is_valid() || action.preinput_camera_returned || !action.preinput_host_prepared || action.preinput_host_returned ||
			request.control.state != ProductionControlState::EXECUTING || request.native_stage_control != request.control.serial ||
			request.control.input.kind != ProductionControlKind::RECORD || invocation.get_device() != request.attachment->held_device ||
			!invocation.is_recordable(&action.camera_ready_subscriber, capture) || capture->get_viewport() != action.camera_ready_viewport ||
			!action.host_view_received || capture->get_invocation().camera != action.host_view_camera ||
			capture->get_presentation_key().view_id != action.host_view_key.view_id || capture->get_presentation_key().view_epoch != action.host_view_key.view_epoch) { return ERR_UNAUTHORIZED; }
	if (!action.scene_cull_prepared || action.scene_cull_capture != capture) {
		return action.scene_cull_error == OK ? ERR_UNAVAILABLE : action.scene_cull_error;
	}
	const Error scene = production_output_scene_validate_locked(action, action.output_input);
	if (scene != OK) { return scene; }
	GDExtensionProductionCameraSampleV1 sample{};
	const Error sampled = production_camera_sample(*capture.ptr(), sample);
	if (sampled != OK) { return sampled; }
	auto input = action.output_input;
	const auto &token = capture->get_token();
	const auto &key = capture->get_presentation_key();
	const auto &previous = capture->get_previous_camera_token();
	input.common.current_camera = { token.view_id, token.view_epoch, token.capture_ordinal };
	input.common.present_seq = key.present_seq; input.view_epoch = key.view_epoch;
	input.common.has_actual_previous_camera = previous.is_valid() && capture->get_previous_camera_data();
	input.common.actual_previous_camera = input.common.has_actual_previous_camera ?
			GDExtensionOutputCameraV1{ previous.view_id, previous.view_epoch, previous.capture_ordinal } : GDExtensionOutputCameraV1{};

	const Error chosen = production_history_choose_locked(request, action, *request.attachment->held_device, input);
	if (chosen != OK) { return chosen; }

	// Codec revision 1 is the existing ANGE OutputPresentationPayload wire:
	// view24=[eye,forward,host affine12,right,up], sort18=first18,
	// control3=[needs_inverse,present low,present high]. Exact original paid
	// upload rows only; no allocation, new buffer, changed extent or DSO call.
	uint32_t view[24]{};
	for (uint32_t i = 0; i < 3; ++i) {
		memcpy(&view[i], &sample.eye[i], 4); memcpy(&view[3 + i], &sample.forward[i], 4);
		memcpy(&view[18 + i], &sample.right[i], 4); memcpy(&view[21 + i], &sample.up[i], 4);
	}
	for (uint32_t i = 0; i < 12; ++i) { memcpy(&view[6 + i], &action.preinput_host_transform.affine[i], 4); }
	const uint32_t control[] = { request.native_camera_needs_inverse, uint32_t(sample.present_seq), uint32_t(sample.present_seq >> 32) };
	const void *payloads[] = { view, view, control };
	const uint32_t bytes[] = { 96, 72, 12 };
	for (uint32_t i = 0; i < 3; ++i) {
		const auto ordinal = request.native_camera_uploads[i];
		if (ordinal == UINT64_MAX) { continue; }
		if (ordinal >= request.action_count || request.next_action > ordinal || request.actions[ordinal].recorded_bytes ||
				request.actions[ordinal].camera_payload_kind != i + 1 || request.actions[ordinal].upload.bytes != bytes[i]) { return ERR_INVALID_DATA; }
	}
	if (input.common.has_previous_consumed_tuple) {
		const Error history = production_output_history_validate_locked(request, action, *request.attachment->held_device, input);
		if (history != OK) { return history; }
	}
	for (uint32_t i = 0; i < 3; ++i) {
		const auto ordinal = request.native_camera_uploads[i];
		if (ordinal != UINT64_MAX) { memcpy(request.attachment->payload + request.actions[ordinal].payload_offset, payloads[i], bytes[i]); }
	}
	// Structural rows and finite graph/native suffix were sealed before draw.
	// Only when the original range actually reaches Acquire does this real
	// current capture become its fixed source; no old capture is ever rebound.
	action.output_input = input;
	action.output_plan.acquire.input = input;
	action.preinput_camera_sample = sample; action.preinput_camera_source = capture;
	action.output_camera_pending = false; request.frozen = true;
	return OK;
}

void ProductionCameraReadySubscriber::camera_source_closed() {
	closed = true;
	if (registry) { registry->camera_ready_source_closed(*this); }
	// Last member access: the original owner can consume/release after return.
}

void Registry::camera_ready_source_closed(ProductionCameraReadySubscriber &observer) {
	Lock lock(*this);
	if (observer.registry != this || !observer.closed || observer.request.registry_instance != instance || observer.attachment.registry_instance != instance) { return; }
	for (auto *attachment : production_attachments) {
		if (!attachment || attachment->serial != observer.attachment.handle_serial) { continue; }
		for (uint32_t i = 0; i < attachment->quote.capacity.requests; ++i) {
			auto &request = attachment->requests[i];
			if (request.serial != observer.request.handle_serial || observer.action_ordinal >= request.action_count) { continue; }
			auto &action = request.actions[observer.action_ordinal];
			if (request.attachment != attachment || action.request != &request || action.declaration.action_ordinal != observer.action_ordinal ||
					action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || !action.output_source_prepared || &action.camera_ready_subscriber != &observer) { return; }
			production_native_record_reject_wait_locked(request);
			return; // No DSO, driver operation, completion fact, or new schedule.
		}
		return;
	}
}

Error Registry::camera_ready_source_retained(ProductionCameraReadyObserver &observer) {
	if (&observer.registry != this || observer.closed || observer.delivery.is_null()) {
		return ERR_UNAUTHORIZED;
	}
	RenderingDevice *device = RenderingDevice::get_singleton();
	if (!device || !device->native_immutable_is_render_thread()) {
		return ERR_UNAUTHORIZED;
	}
	// The renderer callback pins the original main RD on its own execution
	// thread. Acquire RD before any Registry admission that reads RD state.
	MutexLock device_lock(device->_thread_safe_);
	GDExtensionProductionCameraSampleV1 sample{};
	const Error error = production_camera_sample(*observer.delivery.ptr(), sample);
	if (error != OK) {
		return error;
	}
	// The exact original Ref, not its numeric token, is the synchronous borrow.
	// The typed relay independently authenticates attachment/device/code owner.
	ProductionCameraReadySourceScope source(*this, observer.attachment, observer.delivery, sample, &observer.closed);
	return camera_ready_call_relay(observer.attachment, sample);
}

Error Registry::camera_ready_source_retained(ProductionCameraReadySubscriber &observer) {
	if (observer.registry != this || observer.closed || observer.delivery.is_null()) {
		return ERR_UNAUTHORIZED;
	}
	RenderingDevice *device = RenderingDevice::get_singleton();
	if (!device || !device->native_immutable_is_render_thread()) {
		return ERR_UNAUTHORIZED;
	}
	// The renderer callback pins the original main RD on its own execution
	// thread. Acquire RD before any Registry admission that reads RD state.
	MutexLock device_lock(device->_thread_safe_);
	GDExtensionProductionCameraSampleV1 sample{};
	const Error error = production_camera_sample(*observer.delivery.ptr(), sample);
	if (error != OK) {
		return error;
	}
	// The exact original Ref, not its numeric token, is the synchronous borrow.
	// The typed relay independently authenticates attachment/device/code owner.
	ProductionCameraReadySourceScope source(*this, observer.attachment, observer.delivery, sample, &observer.closed);
	return camera_ready_dispatch_observer(observer, sample);
}


GDExtensionDeviceSessionResultV1 Registry::production_prepare_host_view(GDExtensionProductionRequestRefV1 ref, uint64_t ordinal, const GDExtensionProductionHostViewInputV1 *input) {
 auto status = [](Error e) -> GDExtensionDeviceSessionResultV1 { return { int32_t(e), e == OK ? 0u : 16u, 0, 0 }; };
 if (!input || uintptr_t(input) % alignof(GDExtensionProductionHostViewInputV1) || !production_dto_valid(*input) || !input->viewport_rid || !input->camera_rid) { return status(ERR_INVALID_PARAMETER); }
 Lock lock(*this);
 ProductionRequest *request = nullptr;
 RenderingDevice *device = nullptr;
 const Error error = production_request_locked(ref, true, request, device);
 if (error != OK) { return status(error); }
 const auto *entry = RegistrationContext::current;
 if (is_production_metadata_entry() || !entry || !entry->admitted || !entry->reference || entry->reference->registry != this || !request->attachment || entry->reference->slot != request->attachment->library) { return status(ERR_UNAUTHORIZED); }
 if (request->contract_version != 2 || ordinal >= request->action_count || request->source_device_lost) { return status(ERR_INVALID_DATA); }
 if (request->cancelled || request->release_started || request->frozen || request->append_effect_entered || request->effects_pending || request->capture_inflight || request->continuation_inflight || request->control.state != ProductionControlState::OWNER_CONSUMED) { return status(ERR_BUSY); }
 auto &action = request->actions[ordinal];
 if (action.request != request || action.declaration.action_ordinal != ordinal || action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1) { return status(ERR_INVALID_DATA); }
 const RID viewport = RID::from_uint64(input->viewport_rid), camera = RID::from_uint64(input->camera_rid);
 if (action.preinput_camera_returned || action.camera_ready_subscriber.closed || action.camera_ready_subscriber.released) { return status(ERR_UNAVAILABLE); }
 if (action.host_view_prepared) { return status(action.camera_ready_viewport == viewport && action.host_view_camera == camera ? OK : ERR_ALREADY_IN_USE); }
 if (action.prepared || action.output_state != ProductionActionCell::OutputState::UNBOUND || action.camera_ready_subscriber.subscribed || action.preinput_camera_source.is_valid()) { return status(ERR_BUSY); }
 if (action.camera_ready_viewport.is_valid() && action.camera_ready_viewport != viewport) { return status(ERR_ALREADY_IN_USE); }
 // Original admitted Runtime library retains HostControlService/U05 typed custody.
 // Numeric RIDs attest none of that custody. Native relation is checked on RT.
 action.camera_ready_viewport = viewport;
 action.host_view_camera = camera;
 action.host_view_prepared = true;
 return status(OK);
}

GDExtensionDeviceSessionResultV1 Registry::production_read_host_view(GDExtensionProductionRequestRefV1 ref, uint64_t ordinal, GDExtensionProductionHostViewAssociationV1 *out) {
 auto status = [](Error e) -> GDExtensionDeviceSessionResultV1 { return { int32_t(e), e == OK ? 0u : 16u, 0, 0 }; };
 if (!out || uintptr_t(out) % alignof(GDExtensionProductionHostViewAssociationV1)) { return status(ERR_INVALID_PARAMETER); }
 Lock lock(*this);
 ProductionRequest *request = nullptr;
 RenderingDevice *device = nullptr;
 const Error error = production_request_locked(ref, true, request, device);
 if (error != OK) { return status(error); }
 const auto *entry = RegistrationContext::current;
 if (is_production_metadata_entry() || !entry || !entry->admitted || !entry->reference || entry->reference->registry != this || !request->attachment || entry->reference->slot != request->attachment->library) { return status(ERR_UNAUTHORIZED); }
 if (request->contract_version != 2 || ordinal >= request->action_count || request->source_device_lost) { return status(ERR_INVALID_DATA); }
 if (request->cancelled || request->release_started || request->effects_pending || request->capture_inflight || request->continuation_inflight || request->control.state != ProductionControlState::OWNER_CONSUMED) { return status(ERR_BUSY); }
 const auto &action = request->actions[ordinal];
 const auto &subscriber = action.camera_ready_subscriber;
 if (action.request != request || action.declaration.action_ordinal != ordinal || action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1) { return status(ERR_INVALID_DATA); }
 if (!action.host_view_prepared || !action.host_view_received || subscriber.registry != this || subscriber.closed || subscriber.released || !subscriber.subscribed || action.preinput_camera_returned) { return status(ERR_UNAVAILABLE); }
 GDExtensionProductionHostViewAssociationV1 result{};
 result.abi = production_dto_abi<GDExtensionProductionHostViewAssociationV1>();
 result.viewport_rid = action.camera_ready_viewport.get_id(); result.camera_rid = action.host_view_camera.get_id();
 result.view_id = action.host_view_key.view_id; result.view_epoch = action.host_view_key.view_epoch;
 // Pre-first-capture association deliberately has zero sequence/ordinal. Once
 // delivered use the exact retained source, never a newer live viewport key.
 if (action.preinput_camera_source.is_valid()) {
  const auto &sample = action.preinput_camera_sample;
  if (sample.camera.view_id != result.view_id || sample.camera.view_epoch != result.view_epoch) { return status(ERR_INVALID_DATA); }
  result.present_seq = sample.present_seq; result.capture_ordinal = sample.camera.capture_ordinal;
 }
 *out = result;
 return status(OK);
}

GDExtensionDeviceSessionResultV1 Registry::camera_ready_bind_request(GDExtensionProductionRequestRefV1 ref, uint64_t ordinal, NormalRelayToken token, RID viewport) {
	auto status = [](Error e) -> GDExtensionDeviceSessionResultV1 { return { int32_t(e), e == OK ? 0u : 16u, 0, 0 }; };
	if (!viewport.is_valid()) {
		return status(ERR_INVALID_PARAMETER);
	}
	{
		Lock lock(*this);
		ProductionRequest *request = nullptr;
		RenderingDevice *device = nullptr;
		const Error error = production_request_locked(ref, true, request, device);
		if (error != OK) {
			return status(error);
		}
		if (!request || !request->attachment || request->contract_version != 2 || ordinal >= request->action_count || request->cancelled || request->release_started || request->frozen || request->append_effect_entered || request->source_device_lost || request->effects_pending || request->capture_inflight == UINT32_MAX) {
			return status(ERR_BUSY);
		}
		ProductionAttachment *attachment = request->attachment;
		if (attachment->camera_ready_closed || attachment->camera_ready_close_active || !attachment->camera_ready_relay.serial || attachment->camera_ready_relay.registry_instance != token.registry_instance || attachment->camera_ready_relay.serial != token.serial) {
			return status(ERR_UNAVAILABLE);
		}
		// The relay was independently authenticated and bound to this attachment
		// by camera_ready_bind_relay_v1. Do not re-enter its Callable internals
		// while holding the Registry lock.
		auto &action = request->actions[ordinal];
		if (action.request != request || action.declaration.action_ordinal != ordinal || action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || action.preinput_camera_returned) {
			return status(ERR_INVALID_DATA);
		}
		if (action.host_view_prepared && action.camera_ready_viewport != viewport) {
			return status(ERR_ALREADY_IN_USE);
		}
		if (action.camera_ready_subscriber.registry) {
			if (action.camera_ready_viewport == viewport) {
				return status(OK);
			}
			return status(ERR_ALREADY_IN_USE);
		}
		auto &subscriber = action.camera_ready_subscriber;
		subscriber.registry = this;
		subscriber.attachment = { instance, attachment->serial };
		subscriber.request = ref;
		subscriber.action_ordinal = ordinal;
		action.camera_ready_viewport = viewport;
	}
	// Association only: the original bounded OBSERVE control installs this
	// observer on RT. Owner-thread admission never touches renderer lists.
	return status(OK);
}

Error Registry::camera_ready_subscribe_request_locked(ProductionRequest &request, RenderingDevice &device, const GDExtensionProductionServiceBudgetV1 &budget, ProductionServiceUsage &usage) {
	if (request.cancelled || request.release_started) {
		return OK; // RELEASE closes even an observer never installed on RT.
	}
	for (uint32_t i = 0; i < request.action_count; ++i) {
		auto &action = request.actions[i];
		auto *observer = &action.camera_ready_subscriber;
		if (!observer->registry || observer->released || observer->subscribed) {
			continue;
		}
		if (!device.native_immutable_is_render_thread() || !RSG::viewport || !request.attachment || request.attachment->held_device != &device || request.control.state != ProductionControlState::EXECUTING || request.control.input.kind != ProductionControlKind::OBSERVE) {
			return ERR_UNAUTHORIZED;
		}
		if (observer->closed || observer->request.registry_instance != instance || observer->request.handle_serial != request.serial || observer->action_ordinal != i || action.request != &request) {
			return ERR_UNAVAILABLE;
		}
		if (!budget.controls || !budget.passes || !budget.receipts || usage.passes >= MIN(budget.passes, 64u) || usage.receipts >= MIN(budget.receipts, 128u) || !budget.deadline_micros || OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros || request.capture_inflight || request.continuation_inflight) {
			return ERR_BUSY;
		}
		const RID viewport = action.camera_ready_viewport;
		++request.capture_inflight;
		usage.controls = 1; // The same original OBSERVE dispatch.
		++usage.passes;
		++usage.receipts;
		unlock_mutex_for_call();
		RendererViewport::Viewport::ViewPresentationKey native_key;
		const Error received = action.host_view_prepared ? RSG::viewport->viewport_receive_host_view(viewport, action.host_view_camera, native_key) : OK;
		const bool subscribed = received == OK && RSG::viewport->viewport_subscribe_camera_source(viewport, observer);
		relock_mutex_after_call();
		CRASH_COND(request.serial != observer->request.handle_serial || !request.capture_inflight || &action.camera_ready_subscriber != observer);
		if (subscribed && action.host_view_prepared) {
			action.host_view_key = native_key;
			action.host_view_received = true;
		}
		observer->subscribed = subscribed;
		--request.capture_inflight;
		return subscribed ? ERR_BUSY : ERR_UNAVAILABLE; // One paid installation per turn.
	}
	return OK;
}

Error Registry::camera_ready_dispatch_observer(ProductionCameraReadySubscriber &observer, const GDExtensionProductionCameraSampleV1 &sample) {
	// The embedded facet always names its original request and never falls
	// back to the legacy attachment relay after close or arena reuse.
	ProductionRequest *request = nullptr;
	{
		Lock lock(*this);
		if (observer.registry != this || observer.closed || observer.delivery.is_null() || observer.request.registry_instance != instance) {
			return ERR_UNAUTHORIZED;
		}
		ProductionAttachment *attachment = nullptr;
		for (auto *candidate : production_attachments) {
			if (candidate && candidate->serial == observer.attachment.handle_serial) {
				attachment = candidate;
				break;
			}
		}
		if (!attachment) {
			return ERR_DOES_NOT_EXIST;
		}
		for (uint32_t i = 0; i < attachment->quote.capacity.requests; ++i) {
			if (attachment->requests[i].serial == observer.request.handle_serial) {
				request = &attachment->requests[i];
				break;
			}
		}
		if (!request || request->attachment != attachment || request->cancelled || request->release_started || request->frozen || request->append_effect_entered || request->effects_pending || request->source_device_lost || request->continuation_inflight || request->control.state != ProductionControlState::OWNER_CONSUMED || observer.action_ordinal >= request->action_count) {
			return ERR_BUSY;
		}
		auto &action = request->actions[observer.action_ordinal];
		if (action.output_camera_pending || action.staged_predecessor_intent) { return ERR_BUSY; } // Explicit stage owns camera binding; never run the compatibility DSO relay.
		if (action.request != request || action.declaration.action_ordinal != observer.action_ordinal || action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || &action.camera_ready_subscriber != &observer || action.preinput_camera_returned || observer.delivery->get_viewport() != action.camera_ready_viewport) {
			return ERR_INVALID_DATA;
		}
		if (action.host_view_prepared && (!action.host_view_received || observer.delivery->get_invocation().camera != action.host_view_camera || sample.camera.view_id != action.host_view_key.view_id || sample.camera.view_epoch != action.host_view_key.view_epoch || !sample.present_seq)) {
			return ERR_INVALID_DATA;
		}
		if (request->capture_inflight == UINT32_MAX) {
			return ERR_BUSY;
		}
		// Pin the original arena across CallbackScope admission and DSO delivery.
		// Only production_retain_camera_ready writes the source, after admission.
		++request->capture_inflight;
		observer.delivery_pinned = true;
	}
	return camera_ready_call_relay(observer.attachment, sample, observer.request, observer.action_ordinal);
}

void Registry::camera_ready_finish_delivery(ProductionCameraReadySubscriber &observer) {
	Lock lock(*this);
	if (!observer.delivery_pinned) {
		return;
	}
	for (auto *attachment : production_attachments) {
		if (!attachment || attachment->serial != observer.attachment.handle_serial) {
			continue;
		}
		for (uint32_t i = 0; i < attachment->quote.capacity.requests; ++i) {
			auto &request = attachment->requests[i];
			if (request.serial != observer.request.handle_serial) {
				continue;
			}
			CRASH_COND(!request.capture_inflight || observer.action_ordinal >= request.action_count || &request.actions[observer.action_ordinal].camera_ready_subscriber != &observer);
			observer.delivery_pinned = false;
			--request.capture_inflight;
			return; // Do not access observer after releasing its final arena pin.
		}
	}
	CRASH_NOW_MSG("Original camera subscriber arena disappeared while delivery was pinned.");
}
} // namespace GDExtensionDeviceExit
