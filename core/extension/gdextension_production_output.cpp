/**************************************************************************/
/*  gdextension_production_output.cpp                                     */
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
#include "core/extension/gdextension_output_history_guard_uses.h"
#include "core/extension/gdextension_output_history_fallback.h"
#include "core/os/os.h"
#include "servers/rendering/native_output_cull.h"
#include "servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.h"
#include "servers/rendering/renderer_rd/forward_mobile/render_forward_mobile.h"
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_owner.h"
#include "servers/rendering/renderer_rd/effects/particles_external_guard.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server_globals.h"
#include "servers/rendering/renderer_scene_cull.h"

namespace GDExtensionDeviceExit {
thread_local ProductionOutputPreparationScope *Registry::production_output_preparation_scope = nullptr;
thread_local ProductionCameraReadySourceScope *ProductionCameraReadySourceScope::current = nullptr;

ProductionCameraReadySourceScope::ProductionCameraReadySourceScope(Registry &p_registry, GDExtensionProductionAttachmentRefV1 p_attachment, const Ref<RendererViewport::Viewport::CameraCapture> &p_source, const GDExtensionProductionCameraSampleV1 &p_sample, const bool *p_admissions_closed) :
		registry(&p_registry), attachment(p_attachment), source(p_source), sample(p_sample), admissions_closed(p_admissions_closed), previous(current) {
	current = this;
}
ProductionCameraReadySourceScope::~ProductionCameraReadySourceScope() {
	CRASH_COND(current != this);
	current = previous;
}

Error production_camera_sample(const RendererViewport::Viewport::CameraCapture &source, GDExtensionProductionCameraSampleV1 &out) {
	const auto &token = source.get_token();
	const auto &key = source.get_presentation_key();
	const auto &invocation = source.get_invocation();
	const auto &camera = source.get_camera_data();
	if (!token.is_valid() || !source.get_viewport().is_valid() || key.view_id != token.view_id || key.view_epoch != token.view_epoch ||
			camera.source_viewport != source.get_viewport() || camera.source_view_id != token.view_id || camera.source_view_epoch != token.view_epoch || camera.source_capture_ordinal != token.capture_ordinal ||
			invocation.camera.is_null() || invocation.render_buffers.is_null()) {
		return ERR_INVALID_DATA;
	}
	if (invocation.use_xr || invocation.view_count != 1 || camera.view_count != 1) {
		return ERR_UNAVAILABLE;
	}
	const real_t determinant = camera.main_transform.basis.determinant();
	if (!Math::is_finite(determinant) || determinant == 0) {
		return ERR_INVALID_DATA;
	}
	const Vector3 axes[] = { camera.main_transform.origin, -camera.main_transform.basis.get_column(2), camera.main_transform.basis.get_column(0), camera.main_transform.basis.get_column(1) };
	GDExtensionProductionCameraSampleV1 sample{};
	sample.abi = production_dto_abi<GDExtensionProductionCameraSampleV1>();
	sample.camera = { token.view_id, token.view_epoch, token.capture_ordinal };
	sample.present_seq = key.present_seq;
	float *destinations[] = { sample.eye, sample.forward, sample.right, sample.up };
	for (uint32_t axis = 0; axis < 4; ++axis) {
		if (!axes[axis].is_finite() || (axis && (!Math::is_finite(axes[axis].length_squared()) || axes[axis].length_squared() <= 0))) {
			return ERR_INVALID_DATA;
		}
		const Vector3 value = axis ? axes[axis].normalized() : axes[axis];
		for (uint32_t component = 0; component < 3; ++component) {
			destinations[axis][component] = float(value[component]);
			if (!Math::is_finite(destinations[axis][component])) {
				return ERR_INVALID_DATA;
			}
		}
	}
	// Validate the F32 values actually delivered, preserving +X/+Y/-Z and
	// either nonzero handedness without introducing an epsilon.
	const Vector3 rounded_right(sample.right[0], sample.right[1], sample.right[2]);
	const Vector3 rounded_up(sample.up[0], sample.up[1], sample.up[2]);
	const Vector3 rounded_forward(sample.forward[0], sample.forward[1], sample.forward[2]);
	const real_t rounded_triple = rounded_right.dot(rounded_up.cross(rounded_forward));
	if (!Math::is_finite(rounded_triple) || rounded_triple == 0) {
		return ERR_INVALID_DATA;
	}
	out = sample;
	return OK;
}

GDExtensionDeviceSessionResultV1 Registry::production_retain_camera_ready(GDExtensionProductionRequestRefV1 ref, uint64_t ordinal) {
	auto status = [](Error e) -> GDExtensionDeviceSessionResultV1 { return { int32_t(e), e == OK ? 0u : 16u, 0, 0 }; };
	Lock lock(*this);
	const auto *scope = ProductionCameraReadySourceScope::get();
	const auto *entry = RegistrationContext::current;
	if (is_production_metadata_entry() || !scope || scope->registry != this || scope->source.is_null() || !entry || !entry->admitted || !entry->frame_id || !entry->reference || entry->reference->registry != this) {
		return status(ERR_UNAUTHORIZED);
	}
	if (!scope->admissions_closed || *scope->admissions_closed) {
		return status(ERR_BUSY);
	}
	if (ref.registry_instance != instance || !ref.handle_serial || scope->attachment.registry_instance != instance || !scope->attachment.handle_serial) {
		return status(ERR_INVALID_DATA);
	}
	ProductionAttachment *attachment = nullptr;
	for (auto *candidate : production_attachments) {
		if (candidate && candidate->serial == scope->attachment.handle_serial) {
			attachment = candidate;
			break;
		}
	}
	if (!attachment || attachment->library != entry->reference->slot || !attachment->camera_ready_active || attachment->closing || !attachment->opened || !attachment->held_device) {
		return status(ERR_UNAUTHORIZED);
	}
	auto *rd = attachment->held_device;
	const auto &record = records[attachment->library];
	const auto &native = device_records[attachment->device];
	if (!rd->native_immutable_is_render_thread() || rd != RenderingDevice::get_singleton() || !rd->is_main_instance || rd->production_finalization_started.load(std::memory_order_acquire) ||
			!record.occupied || !record.attached || record.quiescing || record.close_started || record.close_error != OK || record.guard_enrollment != GuardEnrollment::QUALIFIED || record.snapshot.library_generation != attachment->library_generation || record.producer_registration_serial != attachment->producer_serial ||
			!attachment->holder || attachment->holder->pending || attachment->holder->generation != attachment->library_generation || attachment->holder->handle_serial != record.producer_code_holder || !record.producer_attachment_identity ||
			!native.live || native.epoch != attachment->epoch || native.object != rd) {
		return status(ERR_UNAUTHORIZED);
	}
	// Resolve only this exact source delivery's original finite request arena.
	// The normal owner-only resolver cannot authorize a distinct render thread.
	ProductionRequest *request = nullptr;
	for (uint32_t i = 0; i < attachment->quote.capacity.requests; ++i) {
		if (attachment->requests[i].serial == ref.handle_serial) {
			request = &attachment->requests[i];
			break;
		}
	}
	if (!request || request->attachment != attachment) {
		return status(ERR_DOES_NOT_EXIST);
	}
	if (request->contract_version != 2 || ordinal >= request->action_count || request->source_device_lost || request->effects_pending) {
		return status(ERR_INVALID_DATA);
	}
	if (request->cancelled || request->release_started || request->continuation_inflight || request->control.state != ProductionControlState::OWNER_CONSUMED || request->frozen || request->append_effect_entered) {
		return status(ERR_BUSY);
	}
	auto &action = request->actions[ordinal];
	if (action.request != request || action.declaration.action_ordinal != ordinal || action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1) {
		return status(ERR_INVALID_DATA);
	}
	if (action.preinput_camera_returned || action.output_camera_pending || action.staged_predecessor_intent) {
		return status(ERR_BUSY);
	}
	if (action.preinput_camera_source.is_valid()) {
		return status(action.preinput_camera_source == scope->source && !memcmp(&action.preinput_camera_sample, &scope->sample, sizeof(scope->sample)) ? OK : ERR_INVALID_DATA);
	}
	action.preinput_camera_source = scope->source;
	action.preinput_camera_sample = scope->sample;
	return status(OK);
}

GDExtensionDeviceSessionResultV1 Registry::production_read_camera_ready(GDExtensionProductionRequestRefV1 ref, uint64_t ordinal, GDExtensionProductionCameraSampleV1 *out) {
	auto status = [](Error e) -> GDExtensionDeviceSessionResultV1 { return { int32_t(e), e == OK ? 0u : 16u, 0, 0 }; };
	if (!out || uintptr_t(out) % alignof(GDExtensionProductionCameraSampleV1)) {
		return status(ERR_INVALID_PARAMETER);
	}
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	const Error error = production_request_locked(ref, true, request, rd);
	if (error != OK) {
		return status(error);
	}
	if (request->contract_version != 2 || ordinal >= request->action_count || request->source_device_lost) {
		return status(ERR_INVALID_DATA);
	}
	const auto &action = request->actions[ordinal];
	if (action.request != request || action.declaration.action_ordinal != ordinal || action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1) {
		return status(ERR_INVALID_DATA);
	}
	if (action.preinput_camera_source.is_null()) {
		return status(ERR_UNAVAILABLE);
	}
	*out = action.preinput_camera_sample;
	return status(OK);
}

GDExtensionDeviceSessionResultV1 Registry::production_capture_camera(GDExtensionProductionRequestRefV1 ref, uint64_t ordinal, uint64_t viewport_rid) {
	auto status = [](Error e) -> GDExtensionDeviceSessionResultV1 { return { int32_t(e), e == OK ? 0u : 16u, 0, 0 }; };
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_request_locked(ref, true, request, rd);
	if (error != OK) {
		return status(error);
	}
	if (!rd || !rd->native_immutable_is_render_thread() || rd != RenderingDevice::get_singleton() || !RSG::viewport) {
		return status(ERR_UNAUTHORIZED);
	}
	if (request->contract_version != 2 || ordinal >= request->action_count || !viewport_rid || request->source_device_lost || request->effects_pending) {
		return status(ERR_INVALID_DATA);
	}
	auto &action = request->actions[ordinal];
	if (action.request != request || action.declaration.action_ordinal != ordinal || action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || !action.prepared || !action.output_source_prepared) {
		return status(ERR_UNCONFIGURED);
	}
	// Prepared output_input is the only frozen input. There is no shadow DTO.
	const auto &input = action.output_input;
	if (action.preinput_camera_returned) {
		return status(ERR_BUSY);
	}
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	auto *particles = storage ? storage->particles_owner.get_or_null(RID::from_uint64(input.particles)) : nullptr;
	if (!particles || !particles->external || !particles->external->admits_work() || particles->external->device != rd || particles->external->generation_registry != this || particles->external->generation != input.common.generation) {
		return status(ERR_UNAUTHORIZED);
	}
	if (action.preinput_camera_source.is_valid()) {
		// A ready source can precede preparation. Retry must match the complete
		// frozen output tuple before accepting that same immutable source.
		const auto &source = action.preinput_camera_source;
		const auto &token = source->get_token();
		const auto &key = source->get_presentation_key();
		const auto &sample = action.preinput_camera_sample;
		const auto &frozen = input.common.current_camera;
		return status(source->get_viewport().get_id() == viewport_rid &&
				token.view_id == frozen.view_id && token.view_epoch == frozen.view_epoch && token.capture_ordinal == frozen.capture_ordinal &&
				key.view_id == token.view_id && key.view_epoch == token.view_epoch && key.view_epoch == input.view_epoch && key.present_seq == input.common.present_seq &&
				sample.camera.view_id == frozen.view_id && sample.camera.view_epoch == frozen.view_epoch && sample.camera.capture_ordinal == frozen.capture_ordinal && sample.present_seq == input.common.present_seq ? OK : ERR_INVALID_DATA);
	}
	// Original storage/pipeline preparation may already own native effects.
	// Only business freeze/append closes this pre-RECORD capture window;
	// production_record_schedule freezes before admitting the RECORD control.
	if (request->frozen || request->append_effect_entered) {
		return status(ERR_BUSY);
	}
	const RID viewport = RID::from_uint64(viewport_rid);
	const auto *native = RSG::viewport->viewport_owner.get_or_null(viewport);
	// Noncreating source borrow: real RenderDataRD must already retain it.
	// Never call the lazy memnew getter from this accepted output arm.
	if (!native || !native->presentation_registered || !native->captured_camera_data_valid || native->captured_camera.is_null()) {
		return status(ERR_UNAVAILABLE);
	}
	const auto &source = native->captured_camera;
	const auto &token = source->get_token();
	const auto &key = source->get_presentation_key();
	const auto &invocation = source->get_invocation();
	const auto &camera = source->get_camera_data();
	const auto &frozen = input.common.current_camera;
	if (source->get_viewport() != viewport || token != native->current_camera_token ||
			token.view_id != frozen.view_id || token.view_epoch != frozen.view_epoch || token.capture_ordinal != frozen.capture_ordinal ||
			key.view_id != token.view_id || key.view_epoch != token.view_epoch || key.view_epoch != input.view_epoch || key.present_seq != input.common.present_seq ||
			native->presentation_key.view_id != key.view_id || native->presentation_key.view_epoch != key.view_epoch || native->presentation_key.present_seq != key.present_seq ||
			camera.source_viewport != viewport || camera.source_view_id != token.view_id || camera.source_view_epoch != token.view_epoch || camera.source_capture_ordinal != token.capture_ordinal ||
			invocation.camera.is_null() || invocation.camera != native->camera || invocation.render_buffers.is_null() || invocation.render_buffers != native->render_buffers ||
			invocation.render_target != native->render_target || invocation.target_size != native->size || invocation.internal_size != native->internal_size) {
		return status(ERR_INVALID_DATA);
	}
	if (native->use_xr || native->view_count != 1) {
		return status(ERR_UNAVAILABLE);
	}
	GDExtensionProductionCameraSampleV1 sample{};
	error = production_camera_sample(*source.ptr(), sample);
	if (error != OK) {
		return status(error);
	}
	// Same original source owner keeps allocation provenance. Only this Ref and
	// immutable value row are new; both are inside the quoted original action.
	action.preinput_camera_source = source;
	action.preinput_camera_sample = sample;
	return status(OK);
}

GDExtensionDeviceSessionResultV1 Registry::production_read_camera(GDExtensionProductionRequestRefV1 ref, uint64_t ordinal, GDExtensionProductionCameraSampleV1 *out) {
	auto status = [](Error e) -> GDExtensionDeviceSessionResultV1 { return { int32_t(e), e == OK ? 0u : 16u, 0, 0 }; };
	if (!out || uintptr_t(out) % alignof(GDExtensionProductionCameraSampleV1)) {
		return status(ERR_INVALID_PARAMETER);
	}
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	const Error error = production_request_locked(ref, true, request, rd);
	if (error != OK) {
		return status(error);
	}
	if (request->contract_version != 2 || ordinal >= request->action_count || request->source_device_lost) {
		return status(ERR_INVALID_DATA);
	}
	const auto &action = request->actions[ordinal];
	if (action.request != request || action.declaration.action_ordinal != ordinal || action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || !action.prepared || !action.output_source_prepared) {
		return status(ERR_INVALID_DATA);
	}
	if (action.preinput_camera_source.is_null()) {
		return status(ERR_UNAVAILABLE);
	}
	*out = action.preinput_camera_sample;
	return status(OK);
}

ProductionOutputPreparationScope::~ProductionOutputPreparationScope() {
	if (registry) {
		registry->production_output_preparation_end(*this);
	}
}

Error Registry::production_output_preparation_validate_locked(ProductionOutputPin &pin, RenderingDevice &rd, uint32_t action_ordinal, uint32_t surface_ordinal, uint64_t group, RID *&root, RendererRD::ExternalPipelinePreparation *&preparation, const ProductionOutputPreparationScope *scope, uint32_t preparation_index, const RendererRD::ExternalPacketViewLease *selected_view, bool retained_tuple) {
	if (!rd.is_main_instance || RenderingDevice::get_singleton() != &rd || rd.render_thread_id != Thread::get_caller_id() ||
			!production_output_pin_matches_locked(pin, { instance, pin.request_serial }, rd)) {
		return ERR_UNAUTHORIZED;
	}
	auto &request = *pin.request;
	auto &attachment = *request.attachment;
	const auto &record = records[attachment.library];
	const auto &native = device_records[attachment.device];
	const auto &control = request.control;
	const auto &segment = control.output.segment;
	// The selected renderer may prepare its paid row before the original owner
	// consumes this Core RECORD. The return and its segment stay owner-visible.
	const bool staged_return = control.state == ProductionControlState::RETURNED && selected_view && preparation_index != UINT32_MAX &&
			!production_physical_frame && request.native_prepared_executor && request.native_static_sealed && action_ordinal == request.native_staged_anchor &&
			control.serial && control.serial == request.native_stage_control && (!request.native_batch_enabled || control.serial == request.native_batch_control) &&
			control.input.kind == ProductionControlKind::RECORD && !control.input.output_cancel && control.input.request_serial == request.serial &&
			control.input.record_generation == request.operation.record_generation && control.output.request_serial == request.serial &&
			control.output.control_serial == control.serial && control.output.result.native_error == OK && control.record_history.control_append_entered &&
			segment.segment && segment.segment == request.segment.segment && segment.segment_revision == request.segment.segment_revision &&
			segment.end_action_ordinal == request.action_count && segment.incomplete_upload_action_ordinal == UINT64_MAX;
	if (phase != Phase::RUNNING || !request.frozen || request.cancelled || request.release_started || request.source_device_lost || request.effects_pending || request.capture_inflight ||
			attachment.closing || !attachment.opened || !attachment.holder || attachment.holder->pending ||
			attachment.holder->generation != attachment.library_generation || attachment.holder->handle_serial != record.producer_code_holder ||
			!record.occupied || !record.attached || record.quiescing || record.close_started || record.close_error != OK ||
			record.guard_enrollment != GuardEnrollment::QUALIFIED || record.snapshot.library_generation != attachment.library_generation ||
			record.producer_registration_serial != attachment.producer_serial || !record.producer_attachment_identity || !native.live || rd.production_finalization_started.load(std::memory_order_acquire) ||
			(control.state != ProductionControlState::OWNER_CONSUMED && !staged_return) ||
			request.draw_list_open || request.compute_list_open) {
		return ERR_BUSY;
	}
	// Keep the same retirement/revision margin as original native preparation.
	if (request.revision > UINT64_MAX - uint64_t(request.capacity.graph_memberships) * 2 - uint64_t(request.capacity.native_memberships) * 2 - request.capacity.readbacks - request.capacity.resources - 8) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	if (action_ordinal >= request.action_count || !group) {
		return ERR_INVALID_PARAMETER;
	}
	const auto &action = request.actions[action_ordinal];
	const auto &row = request.uses[pin.cell_index];
	if (action.request != &request || action.declaration.action_ordinal != action_ordinal || action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 ||
			action.output_state != ProductionActionCell::OutputState::BOUND || action.output_root != pin.serial || !action.output_read_capacity || !action.output_read_graph_capacity ||
			row.output_owner_kind != ProductionOutputOwnerKind::SLOT || row.output_particles != action.output_particles || row.output_generation != action.output_generation ||
			row.output_slot != action.output_slot || row.output_lease != action.output_lease) {
		return ERR_UNAUTHORIZED;
	}
	if (!rd._thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	const Error native_ready = rd._native_disposal_preflight();
	const bool source_matches = native_ready == OK && production_output_source_matches_locked(request, action, rd) && production_output_recorded_ordered_locked(request, rd);
	rd._thread_safe_.unlock();
	if (native_ready != OK || !source_matches) {
		return native_ready != OK ? native_ready : ERR_BUSY;
	}
	RID *original_root = nullptr;
	if (!RendererRD::ParticlesStorage::_production_output_preparation_matches(action.output_particles, action.output_generation, action.output_slot,
				action.output_lease, surface_ordinal, group, pin, rd, original_root, preparation, scope, false, preparation_index) ||
			!original_root || preparation->owner->slots[action.output_slot].original_registry != this || preparation->owner->slots[action.output_slot].original_action_ordinal != action_ordinal || !RendererRD::ParticlesStorage::_production_output_owner_matches(ProductionOutputOwnerKind::SLOT, action.output_particles, action.output_generation, action.output_slot, action.output_lease, pin, rd, { instance, request.serial })) {
		return ERR_UNAUTHORIZED;
	}
	const auto &owner = *preparation->owner;
	const auto &slot = owner.slots[action.output_slot];
	const bool packet = slot.state == RendererRD::ExternalOutputOwner::LeaseState::SEALED_RECORDED || slot.state == RendererRD::ExternalOutputOwner::LeaseState::ADOPTED;
	if ((packet && !selected_view) || (selected_view && !owner.preparation_view_matches(*selected_view, action.output_slot))) {
		return ERR_UNAUTHORIZED;
	}
	if (staged_return) {
		using Backend = RendererRD::ExternalPacketPipelineRequest::Backend;
		using RendererSceneRenderImplementation::RenderForwardClustered;
		using RendererSceneRenderImplementation::RenderForwardMobile;
		auto *compositor = RendererCompositorRD::get_singleton();
		const auto *scene = compositor && compositor == RSG::rasterizer ? compositor->get_scene() : nullptr;
		const auto backend = preparation->selection.backend;
		// The live compositor owns the scene selected after any backend fallback.
		const bool backend_matches = scene &&
				((backend == Backend::MOBILE && scene == RenderForwardMobile::get_singleton()) ||
						(backend == Backend::CLUSTERED && scene == RenderForwardClustered::get_singleton()));
		const auto &capture = action.preinput_camera_source;
		const uint64_t seal_ordinal = action.output_plan.seal_action_ordinal;
		if (!packet || slot.publication_closed || !slot.guard_resources_ready || !action.output_plan_prepared || !action.output_guard_complete ||
				action.output_camera_pending || action.preinput_camera_returned || seal_ordinal <= action_ordinal || seal_ordinal >= request.action_count ||
				!backend_matches || selected_view->flattened_surface != surface_ordinal ||
				capture.is_null() || capture != slot.native_camera_source || capture != owner.view_cells[selected_view->view_cell].camera_source ||
				capture->get_viewport() != action.camera_ready_viewport) {
			return ERR_UNAUTHORIZED;
		}
		const auto &seal = request.actions[seal_ordinal];
		const OriginalOperationIdentity operation{ request.operation.owner_request_nonce, attachment.library_generation, native.session,
			request.operation.record_generation, request.operation.canonical_begin, request.operation.canonical_end };
		if (seal.request != &request || seal.output_acquire_action_ordinal != action_ordinal ||
				seal.original_append.state != CheckedRecordAppend::State::Appended || seal.original_append.error != OK ||
				!seal.original_append.operation_bound || !same_original_operation_identity(seal.original_append.operation, operation)) {
			return ERR_UNAUTHORIZED;
		}
		if (!request.native_stage_budget.deadline_micros || OS::get_singleton()->get_ticks_usec() >= request.native_stage_budget.deadline_micros) {
			return ERR_BUSY;
		}
		// Compare the already cached Ref without creating a new camera source.
		const auto *viewport = RSG::viewport ? RSG::viewport->viewport_owner.get_or_null(capture->get_viewport()) : nullptr;
		const auto &key = capture->get_presentation_key();
		const auto &invocation = capture->get_invocation();
		if (!viewport || !viewport->presentation_registered || !viewport->captured_camera_data_valid || viewport->captured_camera != capture ||
				viewport->current_camera_token != capture->get_token() || viewport->presentation_key.view_id != key.view_id ||
				viewport->presentation_key.view_epoch != key.view_epoch || viewport->presentation_key.present_seq != key.present_seq ||
				viewport->camera != invocation.camera || viewport->render_buffers != invocation.render_buffers || viewport->render_target != invocation.render_target ||
				viewport->size != invocation.target_size || viewport->internal_size != invocation.internal_size ||
				viewport->view_count != invocation.view_count || viewport->use_xr != invocation.use_xr) {
			return ERR_UNAUTHORIZED;
		}
	}
	if (retained_tuple) {
		if (!selected_view || scope || preparation->tuple_state == RendererRD::ExternalPipelinePreparation::TupleState::UNSELECTED) {
			return ERR_UNAUTHORIZED;
		}
		root = original_root;
		return OK; // Reuse authenticates current authority; it grants no factory.
	}
	if (preparation->finish_complete) {
		return ERR_ALREADY_IN_USE;
	}
	const bool recheck_created_pipeline = scope && original_root->is_valid() &&
			!preparation->pipeline_sealed && preparation->created_pipeline == *original_root;
	const ProductionDisposalUse *created_member = nullptr;
	uint32_t headers = 0, members = 0;
	for (uint32_t i = request.use_count + request.binding_used; i < request.capacity.resource_uses; ++i) {
		const auto &cell = request.uses[i];
		if (cell.disposal_group_serial != group) {
			continue;
		}
		if (cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP && cell.disposal_group_pin_serial == pin.serial &&
				cell.disposal_group_count == 1 && !cell.disposal_group_release && !cell.disposal_group_dropped && !cell.disposal_group_native_hold) {
			++headers;
		} else if (cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_RESERVED && cell.disposal_ordinal == 0 &&
				cell.disposal.request == &request && cell.disposal.request_serial == request.serial && cell.disposal.group == group && cell.disposal.owned &&
				!cell.disposal.rid && !cell.disposal.serial && !cell.disposal.native_driver_id && !cell.disposal.next && cell.disposal.counter_dropped &&
				cell.disposal.state.load(std::memory_order_acquire) == 0) {
			++members;
		} else if (recheck_created_pipeline && cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_MEMBER && cell.disposal_ordinal == 0 &&
				cell.disposal.request == &request && cell.disposal.request_serial == request.serial && cell.disposal.group == group && cell.disposal.owned &&
				cell.disposal.kind == 0 && cell.disposal.rid == original_root->get_id() && cell.disposal.serial == original_root->get_id() &&
				cell.disposal.native_driver_id && !cell.disposal.next && cell.disposal.counter_dropped && cell.disposal.state.load(std::memory_order_acquire) == 1) {
			created_member = &cell.disposal;
			++members;
		} else {
			return ERR_ALREADY_IN_USE;
		}
	}
	if (headers != 1 || members != 1 || (original_root->is_valid() && !created_member)) {
		return ERR_ALREADY_IN_USE;
	}
	if (created_member) {
		// A later factory in the same scope may use only the pipeline published
		// by its original reserved member, before the row is sealed.
		if (!rd._thread_safe_.try_lock()) {
			return ERR_BUSY;
		}
		const auto *pipeline = rd.render_pipeline_owner.get_or_null(*original_root);
		const bool matches = pipeline && pipeline->driver_id.id == created_member->native_driver_id &&
				pipeline->production_disposal_uses == created_member && !pipeline->native_immutable_retire_requested;
		rd._thread_safe_.unlock();
		if (!matches) {
			return ERR_ALREADY_IN_USE;
		}
	}
	root = original_root;
	return OK;
}

Error Registry::production_output_preparation_begin(ProductionOutputPin &pin, RenderingDevice &rd, uint32_t action_ordinal, uint32_t surface_ordinal, uint64_t group, ProductionOutputPreparationScope &scope) {
	return production_output_preparation_begin_internal(pin, rd, action_ordinal, surface_ordinal, group, scope, UINT32_MAX, nullptr);
}
Error Registry::production_output_preparation_begin_row(ProductionOutputPin &pin, RenderingDevice &rd, uint32_t action_ordinal, uint32_t source_surface, uint32_t preparation_index, uint64_t group, ProductionOutputPreparationScope &scope, const RendererRD::ExternalPacketViewLease *view) {
	return production_output_preparation_begin_internal(pin, rd, action_ordinal, source_surface, group, scope, preparation_index, view);
}
Error Registry::production_output_preparation_begin_internal(ProductionOutputPin &pin, RenderingDevice &rd, uint32_t action_ordinal, uint32_t surface_ordinal, uint64_t group, ProductionOutputPreparationScope &scope, uint32_t preparation_index, const RendererRD::ExternalPacketViewLease *selected_view) {
	Lock lock(*this);
	if (!rd.native_immutable_is_render_thread()) {
		return ERR_UNAUTHORIZED;
	}
	if (scope.registry) {
		return ERR_ALREADY_IN_USE;
	}
	if (production_physical_frame || (pin.request && pin.request->continuation_inflight)) {
		return ERR_BUSY;
	}
	RID *root = nullptr;
	RendererRD::ExternalPipelinePreparation *row = nullptr;
	const Error error = production_output_preparation_validate_locked(pin, rd, action_ordinal, surface_ordinal, group, root, row, nullptr, preparation_index, selected_view);
	if (error != OK) {
		return error;
	}
	scope.registry = this;
	scope.request = pin.request;
	scope.pin = &pin;
	scope.device = &rd;
	scope.previous = production_output_preparation_scope;
	scope.original_root = root;
	scope.row = row;
	const auto &action = pin.request->actions[action_ordinal];
	scope.original_source = action.output_source;
	scope.source_generation = action.output_source_generation;
	scope.source_use = action.output_source_use;
	scope.first_source_use = action.output_first_source_use;
	scope.source_use_count = action.output_source_use_count;
	scope.original_particles = action.output_particles;
	scope.original_generation = action.output_generation;
	scope.original_slot = action.output_slot;
	scope.original_lease = action.output_lease;
	scope.original_geometry = row->source_geometry;
	scope.original_material = row->source_material;
	scope.original_material_generation = row->source_material_generation;
	scope.request_serial = pin.request_serial;
	scope.root_serial = pin.serial;
	scope.group = group;
	scope.thread = Thread::get_caller_id();
	scope.action_ordinal = action_ordinal;
	scope.surface_ordinal = surface_ordinal;
	scope.preparation_index = uint32_t(row - row->owner->preparation_rows);
	scope.selected_view = selected_view;
	// Only the exact authenticated closed original row is opened. Its root,
	// pin, group and native prefixes remain in their original owner storage.
	row->pipeline_scope = &scope;
	row->source_sealed = false;
	++scope.request->continuation_inflight;
	production_output_preparation_scope = &scope;
	return OK;
}

Error Registry::production_output_preparation_cleanup_matches_locked(const ProductionOutputPreparationScope &scope, RenderingDevice &rd, ProductionRequest *&request) {
	// CloseOldWork authenticates only custody already retained by this exact
	// synchronous scope. No native factory or admission may use this proof.
	if (scope.registry != this || production_output_preparation_scope != &scope || scope.device != &rd || scope.thread != Thread::get_caller_id() ||
			!rd.is_main_instance || RenderingDevice::get_singleton() != &rd || rd.render_thread_id != Thread::get_caller_id() ||
			!scope.pin || scope.pin->request != scope.request || scope.pin->request_serial != scope.request_serial || scope.pin->serial != scope.root_serial ||
			!scope.request || scope.request->serial != scope.request_serial || scope.request->continuation_inflight != 1 ||
			!scope.row || !scope.original_root || !scope.group || !production_output_pin_matches_locked(*scope.pin, { instance, scope.request_serial }, rd)) {
		return ERR_UNAUTHORIZED;
	}
	auto &original = *scope.request;
	if (scope.action_ordinal >= original.action_count || !scope.original_source) {
		return ERR_UNAUTHORIZED;
	}
	const auto &action = original.actions[scope.action_ordinal];
	const auto &pin_row = original.uses[scope.pin->cell_index];
	if (action.request != &original || action.declaration.action_ordinal != scope.action_ordinal ||
			action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || (action.output_state != ProductionActionCell::OutputState::BOUND && action.output_state != ProductionActionCell::OutputState::RETIRING && action.output_state != ProductionActionCell::OutputState::UNKNOWN) ||
			action.output_root != scope.root_serial || action.output_particles != scope.original_particles || action.output_generation != scope.original_generation ||
			action.output_slot != scope.original_slot || action.output_lease != scope.original_lease ||
			!action.output_source_prepared || action.output_source != scope.original_source || action.output_source_generation != scope.source_generation ||
			action.output_source_use != scope.source_use || action.output_first_source_use != scope.first_source_use || action.output_source_use_count != scope.source_use_count ||
			scope.original_source->request != &original || scope.original_source->request_serial != scope.request_serial ||
			scope.original_source->declaration.original_generation != scope.source_generation ||
			pin_row.output_owner_kind != ProductionOutputOwnerKind::SLOT || pin_row.output_particles != scope.original_particles ||
			pin_row.output_generation != scope.original_generation || pin_row.output_slot != scope.original_slot || pin_row.output_lease != scope.original_lease) {
		return ERR_UNAUTHORIZED;
	}
	RID *root = nullptr;
	RendererRD::ExternalPipelinePreparation *row = nullptr;
	if (!RendererRD::ParticlesStorage::_production_output_preparation_matches(scope.original_particles, scope.original_generation, scope.original_slot,
				scope.original_lease, scope.surface_ordinal, scope.group, *scope.pin, rd, root, row, &scope, true, scope.preparation_index) ||
			!RendererRD::ParticlesStorage::_production_output_owner_matches(ProductionOutputOwnerKind::SLOT, scope.original_particles, scope.original_generation, scope.original_slot, scope.original_lease, *scope.pin, rd, { instance, scope.request_serial }) ||
			root != scope.original_root || row != scope.row || row->owner->slots[scope.original_slot].original_registry != this ||
			row->owner->slots[scope.original_slot].original_action_ordinal != scope.action_ordinal || row->source_geometry != scope.original_geometry ||
			row->source_material != scope.original_material || row->source_material_generation != scope.original_material_generation) {
		return ERR_UNAUTHORIZED;
	}
	if (scope.selected_view && !row->owner->preparation_view_matches(*scope.selected_view, scope.original_slot, true)) {
		return ERR_UNAUTHORIZED;
	}
	request = &original;
	return OK;
}

Error Registry::production_output_preparation_matches_locked(const ProductionOutputPreparationScope &scope, RenderingDevice &rd, ProductionRequest *&request) {
	ProductionRequest *retained = nullptr;
	const Error cleanup = production_output_preparation_cleanup_matches_locked(scope, rd, retained);
	if (cleanup != OK) {
		return cleanup;
	}
	// Re-run all NewWork/source/order/effect gates immediately before each
	// factory. CloseOldWork remains usable after cancellation or withdrawal.
	RID *root = nullptr;
	RendererRD::ExternalPipelinePreparation *row = nullptr;
	const Error error = production_output_preparation_validate_locked(*scope.pin, rd, scope.action_ordinal, scope.surface_ordinal, scope.group, root, row, &scope, scope.preparation_index, scope.selected_view);
	if (error != OK) {
		return error;
	}
	if (root != scope.original_root || row != scope.row) {
		return ERR_UNAUTHORIZED;
	}
	request = retained;
	return OK;
}

Error ProductionOutputPreparationScope::get_readonly_source(uint32_t p_source_index, ReadonlySource &r_source) const {
	if (!registry || !device) {
		return ERR_UNAUTHORIZED;
	}
	Registry::Lock lock(*registry);
	ProductionRequest *original = nullptr;
	const Error authenticated = registry->production_output_preparation_matches_locked(*this, *device, original);
	if (authenticated != OK) {
		return authenticated;
	}
	if (p_source_index >= source_use_count) {
		return ERR_INVALID_PARAMETER;
	}
	// NewWork validation above rechecks the complete captured READ span under
	// the RD mutex. Same RT confinement and scope pin keep these original rows
	// and immutable physical bindings stable throughout this observation.
	const uint32_t index = first_source_use + p_source_index;
	const auto &use = original->uses[index];
	const auto &resource = original->resources[use.declaration.resource_slot];
	ReadonlySource source;
	source.buffer = resource.primary;
	source.request_serial = request_serial;
	source.output_generation = original_generation;
	source.allocation_generation = resource.declaration.original_generation;
	source.byte_offset = use.declaration.byte_offset;
	source.byte_size = use.declaration.byte_size;
	source.action_ordinal = action_ordinal;
	source.use_index = index;
	source.resource_slot = use.declaration.resource_slot;
	source.source_count = source_use_count;
	r_source = source;
	return OK;
}

Error Registry::production_output_preparation_finish(ProductionOutputPreparationScope &scope) {
	{
		Lock lock(*this);
		ProductionRequest *request = nullptr;
		if (!scope.device) {
			return ERR_UNAUTHORIZED;
		}
		const Error authenticated = production_output_preparation_cleanup_matches_locked(scope, *scope.device, request);
		if (authenticated != OK) {
			return authenticated;
		}
	}
	// The retained scope pins request and row throughout the lock-free directory
	// handshake. Mesh VA lock ordering forbids freezing under Registry/RD locks.
	const Error prepared = RendererRD::ParticlesStorage::_production_output_preparation_prepare_finish_row(*scope.row);
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	const Error authenticated = production_output_preparation_cleanup_matches_locked(scope, *scope.device, request);
	if (authenticated != OK) {
		return authenticated;
	}
	scope.row->finish_error = prepared != OK ? prepared : RendererRD::ParticlesStorage::_production_output_preparation_finish_locked(scope, *scope.device, *request);
	return scope.row->finish_error;
}

void Registry::production_output_preparation_end(ProductionOutputPreparationScope &scope) {
	// Finish authenticates before and after its lock-free Mesh handshake.
	production_output_preparation_finish(scope);
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	CRASH_COND(!scope.device || production_output_preparation_cleanup_matches_locked(scope, *scope.device, request) != OK);
	// CloseOldWork is mandatory even after cancellation, backend failure or a
	// caller omitting explicit finish. Refusal preserves every stable wrapper.
	scope.row->source_sealed = true;
	scope.row->pipeline_scope = nullptr;
	production_output_preparation_scope = scope.previous;
	--request->continuation_inflight;
	scope.registry = nullptr;
	scope.request = nullptr;
	scope.pin = nullptr;
	scope.device = nullptr;
	scope.previous = nullptr;
	scope.original_root = nullptr;
	scope.row = nullptr;
	scope.original_source = nullptr;
	scope.selected_view = nullptr;
	scope.preparation_index = UINT32_MAX;
}

namespace {
template <size_t N>
GDExtensionOutputStatusV1 output_status(Error error, const char (&detail)[N]) {
	return { int32_t(error), uint32_t(N - 1), detail };
}
bool output_camera_equal(const GDExtensionOutputCameraV1 &a, const GDExtensionOutputCameraV1 &b) {
	return a.view_id == b.view_id && a.view_epoch == b.view_epoch && a.capture_ordinal == b.capture_ordinal;
}
bool output_input_equal(const GDExtensionOutputAcquireInputV1 &a, const GDExtensionOutputAcquireInputV1 &b) {
	const auto &x = a.common;
	const auto &y = b.common;
	if (a.particles != b.particles || a.content_version != b.content_version || a.view_epoch != b.view_epoch || a.pass_mask != b.pass_mask || a.reserved != b.reserved || x.generation != y.generation || x.present_seq != y.present_seq || x.history_reset != y.history_reset || x.reserved != y.reserved || x.has_actual_previous_camera != y.has_actual_previous_camera || x.has_previous_consumed_tuple != y.has_previous_consumed_tuple || !output_camera_equal(x.current_camera, y.current_camera) || !output_camera_equal(x.actual_previous_camera, y.actual_previous_camera)) {
		return false;
	}
	for (uint32_t i = 0; i < 3; ++i) {
		if (x.bounds.position[i] != y.bounds.position[i] || x.bounds.size[i] != y.bounds.size[i]) {
			return false;
		}
	}
	return memcmp(x.output_tag.bytes, y.output_tag.bytes, 32) == 0 && memcmp(x.view_tag.bytes, y.view_tag.bytes, 32) == 0 && memcmp(x.current_host_token.bytes, y.current_host_token.bytes, 32) == 0 && memcmp(x.previous_consumed_tuple_token.bytes, y.previous_consumed_tuple_token.bytes, 32) == 0 && memcmp(a.layout_digest.bytes, b.layout_digest.bytes, 32) == 0 && memcmp(a.mapping_digest.bytes, b.mapping_digest.bytes, 32) == 0 && memcmp(a.source_token.bytes, b.source_token.bytes, 32) == 0;
}
Error output_decode(const GDExtensionOutputAcquireInputV1 &input, RendererRD::ExternalOutputRequest &out, bool staged = false) {
	const auto &common = input.common;
	if (!production_dto_valid(input) || input.reserved || common.reserved || !input.particles || !common.generation || common.history_reset > 10 || common.has_actual_previous_camera > 1 || common.has_previous_consumed_tuple > 1 || (input.pass_mask & ~RendererRD::ExternalOutputConfig::PASS_KNOWN_MASK) || (!staged && (!common.current_camera.view_id || !common.current_camera.view_epoch || !common.current_camera.capture_ordinal))) {
		return ERR_INVALID_PARAMETER;
	}
	if (staged && (common.current_camera.view_id || common.current_camera.view_epoch || common.current_camera.capture_ordinal ||
			common.present_seq || input.view_epoch || common.has_actual_previous_camera)) {
		return ERR_INVALID_PARAMETER;
	}
	if (common.has_actual_previous_camera && (!common.actual_previous_camera.view_id || !common.actual_previous_camera.view_epoch || !common.actual_previous_camera.capture_ordinal)) {
		return ERR_INVALID_PARAMETER;
	}
	if (!common.has_actual_previous_camera && (common.actual_previous_camera.view_id || common.actual_previous_camera.view_epoch || common.actual_previous_camera.capture_ordinal)) {
		return ERR_INVALID_PARAMETER;
	}
	if (!common.has_previous_consumed_tuple) {
		for (uint8_t byte : common.previous_consumed_tuple_token.bytes) {
			if (byte) {
				return ERR_INVALID_PARAMETER;
			}
		}
	}
	for (uint32_t i = 0; i < 3; ++i) {
		if (!Math::is_finite(common.bounds.position[i]) || !Math::is_finite(common.bounds.size[i]) || common.bounds.size[i] <= 0) {
			return ERR_INVALID_PARAMETER;
		}
		out.common.bounds.position[i] = real_t(common.bounds.position[i]);
		out.common.bounds.size[i] = real_t(common.bounds.size[i]);
		if (double(out.common.bounds.position[i]) != common.bounds.position[i] || double(out.common.bounds.size[i]) != common.bounds.size[i]) {
			return ERR_INVALID_PARAMETER;
		}
	}
	if (!out.common.bounds.is_finite() || !out.common.bounds.get_end().is_finite()) {
		return ERR_INVALID_PARAMETER;
	}
	auto token = [](const GDExtensionOutputTokenV1 &in, PackedByteArray &value) {
		value.resize(32);
		memcpy(value.ptrw(), in.bytes, 32);
	};
	out.common.generation = common.generation;
	out.common.present_seq = common.present_seq;
	out.common.history_reset = common.history_reset;
	out.common.current_camera_token = { common.current_camera.view_id, common.current_camera.view_epoch, common.current_camera.capture_ordinal };
	out.common.has_actual_previous_camera = common.has_actual_previous_camera;
	out.common.actual_previous_camera_token = { common.actual_previous_camera.view_id, common.actual_previous_camera.view_epoch, common.actual_previous_camera.capture_ordinal };
	out.common.has_previous_consumed_tuple = common.has_previous_consumed_tuple;
	token(common.output_tag, out.common.output_tag);
	token(common.view_tag, out.common.view_tag);
	token(common.current_host_token, out.common.current_host_token);
	if (common.has_previous_consumed_tuple) {
		token(common.previous_consumed_tuple_token, out.common.previous_consumed_tuple_token);
	}
	token(input.layout_digest, out.layout_digest);
	token(input.mapping_digest, out.mapping_digest);
	token(input.source_token, out.source_token);
	out.content_version = input.content_version;
	out.view_epoch = input.view_epoch;
	out.pass_mask = input.pass_mask;
	// The typed entry authenticates its actual RECORD physical context. It
	// does not synthesize a facade phase token from any numeric identity.
	return OK;
}
} // namespace

Error Registry::production_output_view_retain_locked(const RendererRD::ExternalPacketViewLease &source, RendererRD::ExternalPacketViewLease &retained, RenderingDevice &rd) {
	if (!Lock::owns(*this) || !retained.is_empty() || !source.owner || source.owner->device != &rd || !rd.native_immutable_is_render_thread()) {
		return ERR_INVALID_PARAMETER;
	}
	auto &owner = *source.owner;
	if (!owner.preparation_view_matches(source, source.slot, false) || !owner.next_view_serial || owner.slots[source.slot].original_registry != this) {
		return ERR_UNAUTHORIZED;
	}
	uint32_t selected = UINT32_MAX;
	for (uint32_t i = 0; i < owner.view_capacity; ++i) {
		const auto &cell = owner.view_cells[i];
		if (!cell.consumer && cell.pin.is_empty() && cell.camera_source.is_null()) {
			selected = i;
			break;
		}
	}
	if (selected == UINT32_MAX) {
		return ERR_OUT_OF_MEMORY;
	}
	auto &cell = owner.view_cells[selected];
	const auto &original = owner.view_cells[source.view_cell];
	const Error error = production_output_pin_borrow_locked(original.pin, rd, cell.pin);
	if (error != OK) {
		return error;
	}
	const uint64_t serial = owner.next_view_serial;
	owner.next_view_serial = serial == UINT64_MAX ? 0 : serial + 1;
	cell.camera_source = original.camera_source;
	cell.consumer = &retained;
	cell.serial = serial;
	cell.slot = source.slot;
	retained.owner = &owner;
	retained.serial = serial;
	retained.slot = source.slot;
	retained.view_cell = selected;
	retained.flattened_surface = source.flattened_surface;
	retained.profile = source.profile;
	retained.space = source.space;
	retained.current_host = source.current_host;
	retained.previous_host = source.previous_host;
	retained.retained_host_snapshot = source.retained_host_snapshot;
	return OK;
}

Error Registry::production_output_view_return_locked(RendererRD::ExternalPacketViewLease &view) {
	if (!Lock::owns(*this) || !view.owner || view.view_cell >= view.owner->view_capacity) {
		return ERR_INVALID_PARAMETER;
	}
	auto &owner = *view.owner;
	auto &cell = owner.view_cells[view.view_cell];
	if (view.slot >= owner.slot_count || owner.slots[view.slot].original_registry != this || cell.consumer != &view ||
			!view.serial || cell.serial != view.serial || cell.slot != view.slot || cell.pin.is_empty()) {
		return ERR_INVALID_DATA;
	}
	const Error error = production_output_unpin_locked(cell.pin);
	if (error != OK) {
		return error;
	}
	cell.camera_source.unref();
	cell.consumer = nullptr;
	cell.serial = 0;
	cell.slot = UINT32_MAX;
	view.owner = nullptr;
	view.serial = 0;
	view.slot = view.view_cell = UINT32_MAX;
	view.retained_host_snapshot = false;
	return OK;
}

bool Registry::production_output_predecessor_matches_locked(const ProductionRequest &request, const ProductionResourceSlot &resource, RenderingDevice &rd) const {
	const auto &view = resource.output_predecessor_view;
	if (view.is_empty() || !view.owner || view.owner->device != &rd || view.slot >= view.owner->slot_count ||
			view.view_cell >= view.owner->view_capacity || resource.request != &request || resource.request_serial != request.serial ||
			!resource.imported || resource.output_alias_prepared || resource.owned_serial || resource.use_count || resource.retiring || resource.returned ||
			resource.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || resource.declaration.ownership != GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1) {
		return false;
	}
	const auto &owner = *view.owner;
	const auto &cell = owner.view_cells[view.view_cell];
	const auto &slot = owner.slots[view.slot];
	if (cell.consumer != &view || cell.serial != view.serial || cell.slot != view.slot || cell.pin.is_empty() ||
			slot.original_registry != this || !slot.storage || slot.original_action_ordinal >= cell.pin.request->action_count ||
			!const_cast<Registry *>(this)->production_output_pin_identity_matches_locked(cell.pin, slot.original_request, rd) ||
			!owner.native_color_consumption_matches(view.slot, resource.output_predecessor_token) ||
			cell.camera_source.is_null() || cell.camera_source != slot.native_camera_source ||
			!request.attachment || cell.pin.request->attachment != request.attachment || cell.pin.request == &request) {
		return false;
	}
	const auto &source_action = cell.pin.request->actions[slot.original_action_ordinal];
	const auto &role = source_action.output_roles[0];
	// Explicit cap0 anchor: retain the same consumed ViewLease and old K status.
	// Its sole Acquire READ32 grants no shader binding or predecessor records.
	if (resource.output_predecessor_no_records) {
		if (owner.config.capacity || owner.config.profile != RendererRD::ExternalOutputConfig::INSTANCE_192 ||
				role.bytes || role.resource_slot != UINT32_MAX || resource.output_predecessor_action >= request.action_count ||
				resource.output_predecessor_use >= request.use_count) { return false; }
		for (uint32_t r : { uint32_t(RendererRD::ExternalOutputStorage::CURRENT_RECORDS), uint32_t(RendererRD::ExternalOutputStorage::PREVIOUS_RECORDS) }) {
			const auto &records = slot.storage->resources[r];
			if (records.bytes || records.rid.is_valid() || records.witness.cell.is_valid()) { return false; }
		}
		ProductionOutputGuardSource guard;
		if (!production_output_guard_source_locked(*cell.pin.request, slot.original_action_ordinal, rd, guard) ||
				!production_output_guard_origin_matches_locked(*cell.pin.request, resource, source_action) ||
				resource.primary != guard.buffer || resource.backing_serial != guard.backing || !guard.witness ||
				resource.output_predecessor_witness.cell != guard.witness->cell || resource.declaration.physical_bytes != 32 ||
				(resource.declaration.required_usage & ~uint64_t(rd.storage_buffer_owner.get_or_null(guard.buffer)->usage))) { return false; }
		const auto &action = request.actions[resource.output_predecessor_action].declaration;
		if (action.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || resource.output_predecessor_use < action.first_use ||
				resource.output_predecessor_use - action.first_use >= action.use_count) { return false; }
		const uint32_t index = uint32_t(&resource - request.resources);
		for (uint32_t i = 0; i < request.use_count; ++i) {
			const auto &use = request.uses[i];
			if (use.declaration.resource_slot != index) { continue; }
			if (i != resource.output_predecessor_use || use.request != &request || use.declaration.access != GDEXTENSION_PRODUCTION_READ_V1 ||
					use.declaration.byte_offset || use.declaration.byte_size != 32) { return false; }
		}
		const auto &use = request.uses[resource.output_predecessor_use];
		return use.request == &request && use.declaration.resource_slot == index && use.declaration.access == GDEXTENSION_PRODUCTION_READ_V1 &&
				!use.declaration.byte_offset && use.declaration.byte_size == 32;
	}
	if (!role.bytes || role.resource_slot >= cell.pin.request->resource_count) {
		return false;
	}
	const auto &original = cell.pin.request->resources[role.resource_slot];
	const auto &storage = slot.storage->resources[RendererRD::ExternalOutputStorage::CURRENT_RECORDS];
	const auto *buffer = rd.storage_buffer_owner.get_or_null(resource.primary);
	if (resource.output_predecessor_witness.cell.is_null() || resource.output_predecessor_witness.cell != storage.witness.cell ||
			!buffer || buffer->native_disposal.cell != storage.witness.cell || resource.primary != storage.rid || resource.backing_serial != buffer->backing_serial ||
			buffer->size != storage.bytes || buffer->size != resource.declaration.physical_bytes ||
			resource.declaration.original_generation != original.declaration.original_generation || resource.declaration.original_slot != original.declaration.original_slot ||
			resource.declaration.original_part != original.declaration.original_part || (resource.declaration.required_usage & ~uint64_t(buffer->usage))) {
		return false;
	}
	const auto &physical = **storage.witness.cell;
	if (physical.production_request != cell.pin.request || physical.production_request_serial != slot.original_request.handle_serial ||
			physical.production_pin_serial != slot.original_pin.serial || physical.production_group_serial != slot.owned_group ||
			physical.state != NativeDisposalWitness::State::LIVE || physical.device != uint64_t(rd.get_instance_id()) ||
			!physical.matches(storage.rid.get_id(), buffer->backing_serial, buffer->driver_id.id)) {
		return false;
	}
	const uint32_t index = uint32_t(&resource - request.resources);
	bool used = false;
	for (uint32_t i = 0; i < request.use_count; ++i) {
		const auto &use = request.uses[i];
		if (use.declaration.resource_slot != index) {
			continue;
		}
		if (use.request != &request || use.declaration.access != GDEXTENSION_PRODUCTION_READ_V1 || !use.declaration.byte_size ||
				use.declaration.byte_offset > buffer->size || use.declaration.byte_size > buffer->size - use.declaration.byte_offset) {
			return false;
		}
		used = true;
	}
	return used;
}

Error Registry::production_output_predecessor_accept_locked(ProductionRequest &request, uint64_t action_index, uint32_t resource_index, uint32_t use_index,
		const RendererRD::ExternalPacketViewLease &source, RenderingDevice &rd, const GDExtensionOutputAcquireInputV1 *current, bool staged) {
	if (!Lock::owns(*this) || source.is_empty() || !current || !production_dto_valid(*current) || current->common.reserved ||
			!rd.native_immutable_is_render_thread() || request.frozen || request.capture_inflight || request.cancelled || request.release_started ||
			action_index >= request.action_count || resource_index >= request.resource_count || use_index >= request.use_count || !request.attachment) {
		return ERR_INVALID_PARAMETER;
	}

	if (staged) {
		RendererRD::ExternalOutputRequest parsed;
		if (!request.actions[action_index].staged_predecessor_intent || !request.actions[action_index].host_view_prepared ||
				output_decode(*current, parsed, true) != OK || current->common.history_reset || !current->common.has_previous_consumed_tuple) { return ERR_INVALID_DATA; }
	}
	RendererRD::ExternalConsumedColorProofSnapshot observed;
	const Error checked = source.copy_native_color_consumption(observed);
	if (checked != OK) {
		return checked;
	}
	if (!source.owner || source.slot >= source.owner->slot_count || source.view_cell >= source.owner->view_capacity ||
			source.owner->device != &rd || source.owner->slots[source.slot].original_registry != this ||
			source.owner->config.profile != RendererRD::ExternalOutputConfig::INSTANCE_192) {
		return ERR_UNAUTHORIZED;
	}
	const auto &old_slot = source.owner->slots[source.slot];
	const auto &old_cell = source.owner->view_cells[source.view_cell];
	if (old_cell.pin.is_empty() || old_cell.pin.request == &request || old_cell.pin.request->attachment != request.attachment ||
			observed.get_particles().get_id() != current->particles || observed.get_generation() != current->common.generation ||
			(!staged && observed.get_view_epoch() != current->view_epoch) || current->common.history_reset || !current->common.has_previous_consumed_tuple ||
			(!staged && (!current->common.has_actual_previous_camera || current->common.present_seq <= observed.get_common().present_seq)) ||
			memcmp(current->common.view_tag.bytes, observed.get_common().view_tag.ptr(), 32) ||
			memcmp(current->common.output_tag.bytes, observed.get_common().output_tag.ptr(), 32) ||
			memcmp(current->common.previous_consumed_tuple_token.bytes, observed.get_token().bytes, 32)) {
		return ERR_INVALID_DATA;
	}
	const auto &camera = observed.get_common().current_camera_token;
	if (!staged && (current->common.actual_previous_camera.view_id != camera.view_id || current->common.actual_previous_camera.view_epoch != camera.view_epoch ||
			current->common.actual_previous_camera.capture_ordinal != camera.capture_ordinal)) {
		return ERR_INVALID_DATA;
	}
	auto &action = request.actions[action_index];
	auto &resource = request.resources[resource_index];
	const auto &use = request.uses[use_index];
	if (action.request != &request || action.declaration.action_ordinal != action_index || action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 ||
			use_index < action.declaration.first_use || use_index - action.declaration.first_use >= action.declaration.use_count ||
			use.request != &request || use.declaration.resource_slot != resource_index || use.declaration.access != GDEXTENSION_PRODUCTION_READ_V1 ||
			!old_slot.storage || old_slot.original_action_ordinal >= old_cell.pin.request->action_count) {
		return ERR_INVALID_DATA;
	}
	if (!resource.output_predecessor_view.is_empty()) {
		return resource.output_predecessor_action == action_index && resource.output_predecessor_view.owner == source.owner &&
				resource.output_predecessor_view.slot == source.slot && production_output_predecessor_matches_locked(request, resource, rd) ? OK : ERR_ALREADY_IN_USE;
	}
	if (action.prepared) { return ERR_INVALID_DATA; }
	const auto &role = old_cell.pin.request->actions[old_slot.original_action_ordinal].output_roles[0];
	if (!source.owner->config.capacity) {
		if (role.bytes || role.resource_slot != UINT32_MAX || !old_slot.guard_resources_ready) { return ERR_INVALID_DATA; }
		for (uint32_t r : { uint32_t(RendererRD::ExternalOutputStorage::CURRENT_RECORDS), uint32_t(RendererRD::ExternalOutputStorage::PREVIOUS_RECORDS) }) {
			const auto &records = old_slot.storage->resources[r];
			if (records.bytes || records.rid.is_valid() || records.witness.cell.is_valid()) { return ERR_INVALID_DATA; }
		}
		ProductionOutputGuardSource guard;
		const auto &old_action = old_cell.pin.request->actions[old_slot.original_action_ordinal];
		if (!production_output_guard_source_locked(*old_cell.pin.request, old_slot.original_action_ordinal, rd, guard) ||
				!production_output_guard_origin_matches_locked(*old_cell.pin.request, resource, old_action)) { return ERR_UNAUTHORIZED; }
		const auto *buffer = rd.storage_buffer_owner.get_or_null(guard.buffer);
		if (resource.request != &request || resource.request_serial != request.serial || resource.imported || resource.owned_serial || resource.use_count || resource.output_alias_prepared ||
				resource.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || resource.declaration.ownership != GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1 ||
				resource.declaration.physical_bytes != 32 || !buffer || (resource.declaration.required_usage & ~uint64_t(buffer->usage)) ||
				use.declaration.byte_offset || use.declaration.byte_size != 32) { return ERR_INVALID_DATA; }
		for (uint32_t i = 0; i < request.use_count; ++i) {
			if (request.uses[i].declaration.resource_slot == resource_index && i != use_index) { return ERR_INVALID_DATA; }
		}
		const Error retained = production_output_view_retain_locked(source, resource.output_predecessor_view, rd);
		if (retained != OK) { return retained; }
		resource.primary = guard.buffer;
		resource.backing_serial = guard.backing;
		resource.output_predecessor_witness = *guard.witness;
		resource.output_predecessor_no_records = true;
		resource.output_predecessor_use = use_index;
		resource.output_predecessor_token = observed.get_token();
		resource.output_predecessor_action = action_index;
		resource.output_predecessor_input = *current;
		resource.output_predecessor_host = old_action.preinput_host_transform;
		resource.imported = true;
		return OK;
	}
	if (!role.bytes || role.resource_slot >= old_cell.pin.request->resource_count) {
		return ERR_INVALID_DATA;
	}
	const auto &origin = old_cell.pin.request->resources[role.resource_slot].declaration;
	const auto &storage = old_slot.storage->resources[RendererRD::ExternalOutputStorage::CURRENT_RECORDS];
	const auto *buffer = rd.storage_buffer_owner.get_or_null(storage.rid);
	if (resource.request != &request || resource.request_serial != request.serial || resource.imported || resource.owned_serial || resource.use_count || resource.output_alias_prepared ||
			resource.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || resource.declaration.ownership != GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1 ||
			resource.declaration.original_generation != origin.original_generation || resource.declaration.original_slot != origin.original_slot || resource.declaration.original_part != origin.original_part ||
			resource.declaration.physical_bytes != storage.bytes || uint64_t(storage.bytes) != uint64_t(source.owner->config.capacity) * 192 ||
			storage.witness.cell.is_null() || !buffer ||
			buffer->native_disposal.cell != storage.witness.cell || buffer->size != storage.bytes || (resource.declaration.required_usage & ~uint64_t(buffer->usage))) {
		return ERR_INVALID_DATA;
	}
	// Reject every writable declaration before taking any source pin.
	bool declared = false;
	for (uint32_t i = 0; i < request.use_count; ++i) {
		const auto &row = request.uses[i];
		if (row.declaration.resource_slot == resource_index) {
			if (row.request != &request || row.declaration.access != GDEXTENSION_PRODUCTION_READ_V1 || !row.declaration.byte_size ||
					row.declaration.byte_offset > storage.bytes || row.declaration.byte_size > storage.bytes - row.declaration.byte_offset) {
				return ERR_INVALID_DATA;
			}
			declared = true;
		}
	}
	if (!declared) {
		return ERR_INVALID_DATA;
	}
	const Error retained = production_output_view_retain_locked(source, resource.output_predecessor_view, rd);
	if (retained != OK) {
		return retained;
	}
	resource.primary = storage.rid;
	resource.backing_serial = buffer->backing_serial;
	resource.output_predecessor_witness = storage.witness;
	resource.output_predecessor_token = observed.get_token();
	resource.output_predecessor_action = action_index;
	resource.output_predecessor_input = *current;
	// Freeze the genuine color-consumed slot's actual Host value in the same
	// retained predecessor resource. The old source action may later return its
	// own Host edge while this independent ViewLease remains legitimately live.
	const auto &old_action = old_cell.pin.request->actions[old_slot.original_action_ordinal];
	resource.output_predecessor_host = old_action.preinput_host_transform;
	resource.imported = true;
	return OK;
}

bool Registry::production_output_source_matches_locked(const ProductionRequest &request, const ProductionActionCell &action, RenderingDevice &rd) const {
	const auto &declaration = action.declaration;
	if (!action.output_source_prepared || !action.output_source || !action.output_source_use_count || action.output_first_source_use < declaration.first_use || action.output_first_source_use - declaration.first_use > declaration.use_count || action.output_source_use_count > declaration.use_count - (action.output_first_source_use - declaration.first_use) || action.output_first_source_use > request.use_count || action.output_source_use_count > request.use_count - action.output_first_source_use || action.output_source_use < action.output_first_source_use || action.output_source_use - action.output_first_source_use >= action.output_source_use_count) {
		return false;
	}
	for (uint32_t i = 0; i < action.output_source_use_count; ++i) {
		const auto &use = request.uses[action.output_first_source_use + i];
		if (use.request != &request || use.declaration.access != GDEXTENSION_PRODUCTION_READ_V1 || use.declaration.resource_slot >= request.resource_count || !use.declaration.byte_size) {
			return false;
		}
		const auto &resource = request.resources[use.declaration.resource_slot];
		const auto *buffer = rd.storage_buffer_owner.get_or_null(resource.primary);
		const bool history_guard = resource.output_guard_history;
		if (history_guard) {
			if (!production_output_guard_matches_locked(request, resource, rd) || resource.output_action_ordinal != declaration.action_ordinal ||
					resource.output_guard_anchor >= request.resource_count || !output_input_equal(request.resources[resource.output_guard_anchor].output_predecessor_input, action.output_input) ||
					use.declaration.byte_offset || use.declaration.byte_size != 32) { return false; }
			continue;
		}
		const bool predecessor = !resource.output_predecessor_view.is_empty();
		if (predecessor) {
			if (!production_output_predecessor_matches_locked(request, resource, rd) || resource.output_predecessor_action != declaration.action_ordinal || !output_input_equal(resource.output_predecessor_input, action.output_input) ||
					!buffer || use.declaration.byte_offset > buffer->size || use.declaration.byte_size > buffer->size - use.declaration.byte_offset) {
				return false;
			}
		} else if (resource.request != &request || resource.request_serial != request.serial || resource.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || !resource.declaration.original_generation || resource.output_alias_prepared || !resource.imported || resource.retiring || resource.returned || resource.use_count != 1 || !resource.uses[0].same_physical() || !resource.uses[0].linked() || resource.uses[0].physical->use_closed.load() || !buffer || buffer->qualification_return != resource.uses[0].physical || buffer->backing_serial != resource.backing_serial || buffer->size != resource.declaration.physical_bytes || use.declaration.byte_offset > buffer->size || use.declaration.byte_size > buffer->size - use.declaration.byte_offset) {
			return false;
		}
		if (action.output_first_source_use + i == action.output_source_use && (&resource != action.output_source || resource.declaration.original_generation != action.output_source_generation)) {
			return false;
		}
	}
	for (uint32_t i = 0; i < declaration.use_count; ++i) {
		const uint32_t index = declaration.first_use + i;
		if (index >= request.use_count) {
			return false;
		}
		const auto &use = request.uses[index].declaration;
		if (use.resource_slot >= request.resource_count) {
			return false;
		}
		if (use.access == GDEXTENSION_PRODUCTION_READ_V1 && request.resources[use.resource_slot].declaration.resource_kind == GDEXTENSION_PRODUCTION_STORAGE_V1 && (index < action.output_first_source_use || index - action.output_first_source_use >= action.output_source_use_count)) {
			return false;
		}
	}
	return true;
}

// Both projections resolve the real K storage member. This stack source never
// owns a RID, invents a return cell, or turns recorded append into completion.
bool Registry::production_output_guard_origin_matches_locked(const ProductionRequest &request, const ProductionResourceSlot &resource, const ProductionActionCell &acquire) const {
	const auto &role = acquire.output_roles[0];
	if (!role.bytes) {
		// Explicit zero-capacity source, not a fallback for missing records.
		auto *storage = RendererRD::ParticlesStorage::get_singleton();
		const auto *particles = storage ? storage->particles_owner.get_or_null(RID::from_uint64(acquire.output_input.particles)) : nullptr;
		const auto *owner = particles ? particles->external : nullptr;
		return request.attachment && owner && owner->device == request.attachment->held_device && owner->generation_registry == this &&
				owner->generation == acquire.output_input.common.generation && !owner->config.capacity && acquire.output_plan_prepared &&
				role.resource_slot == UINT32_MAX && !acquire.output_plan.acquire.roles[0].bytes && acquire.output_plan.acquire.roles[0].resource_slot == UINT32_MAX &&
				resource.declaration.original_generation == acquire.output_input.common.generation && !resource.declaration.original_slot;
	}
	if (role.resource_slot >= request.resource_count) { return false; }
	const auto &origin = request.resources[role.resource_slot].declaration;
	return resource.declaration.original_generation == origin.original_generation && resource.declaration.original_slot == origin.original_slot;
}

bool Registry::production_output_guard_source_locked(const ProductionRequest &owner_request, uint64_t acquire_index, RenderingDevice &rd, ProductionOutputGuardSource &out) const {
	const ProductionRequest *original = &owner_request;
	if (acquire_index >= original->action_count || original->source_device_lost) { return false; }
	const auto &source_action = original->actions[acquire_index];
	auto *particles_storage = RendererRD::ParticlesStorage::get_singleton();
	const auto *particles = particles_storage ? particles_storage->particles_owner.get_or_null(source_action.output_particles) : nullptr;
	if (!particles || !particles->external || particles->external->device != &rd || particles->external->generation_registry != this || particles->external->generation != source_action.output_generation || source_action.output_slot >= particles->external->slot_count) { return false; }
	const auto *slot = &particles->external->slots[source_action.output_slot];
	if (slot->original_registry != this || slot->original_request.registry_instance != instance || slot->original_request.handle_serial != original->serial ||
			slot->original_action_ordinal != acquire_index || slot->lease_serial != source_action.output_lease || !slot->owned_group ||
			slot->original_pin.serial != source_action.output_root || slot->request.common.present_seq != source_action.output_input.common.present_seq) { return false; }
	if (!slot->storage) { return false; }
	const auto &acquire = original->actions[acquire_index];
	if (acquire.request != original || !acquire.output_plan_prepared || !acquire.output_guard_complete ||
			acquire.output_guard_append.error != OK || acquire.output_guard_append.state != CheckedRecordAppend::State::Appended || !acquire.output_guard_append.operation_bound ||
			acquire.output_plan.finalize_action_ordinal >= original->action_count) { return false; }
	if (!original->attachment) { return false; }
	const OriginalOperationIdentity operation{ original->operation.owner_request_nonce, original->attachment->library_generation,
			device_records[original->attachment->device].session, original->operation.record_generation, original->operation.canonical_begin, original->operation.canonical_end };
	if (!same_original_operation_identity(acquire.output_guard_append.operation, operation)) { return false; }
	const auto &finalizer = original->actions[acquire.output_plan.finalize_action_ordinal];
	if (finalizer.request != original || !finalizer.prepared || finalizer.output_acquire_action_ordinal != acquire_index ||
			finalizer.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1) { return false; }
	const auto &status = slot->storage->resources[RendererRD::ExternalOutputStorage::GUARD_STATUS];
	const auto *buffer = rd.storage_buffer_owner.get_or_null(status.rid);
	if (status.bytes != 32 || status.witness.cell.is_null() || !buffer || buffer->size != 32 || buffer->native_disposal.cell != status.witness.cell) { return false; }
	const auto &physical = **status.witness.cell;
	if (physical.state != NativeDisposalWitness::State::LIVE || physical.device != uint64_t(rd.get_instance_id()) ||
			physical.production_request != original || physical.production_request_serial != original->serial || physical.production_pin_serial != acquire.output_root ||
			physical.production_group_serial != slot->owned_group || !physical.matches(status.rid.get_id(), buffer->backing_serial, buffer->driver_id.id)) { return false; }
	bool member = false;
	for (uint32_t i = original->use_count + original->binding_used; i < original->capacity.resource_uses; ++i) {
		const auto &row = original->uses[i];
		if (row.output_role != ProductionUseCell::OutputRole::DISPOSAL_MEMBER || row.disposal_group_serial != slot->owned_group || row.disposal_ordinal != physical.production_ordinal) { continue; }
		member = row.disposal.owned && row.disposal.request == original && row.disposal.request_serial == original->serial &&
				row.disposal.rid == status.rid.get_id() && row.disposal.serial == buffer->backing_serial && row.disposal.state.load(std::memory_order_acquire) == 1;
		break;
	}
	if (!member) { return false; }
	ProductionOutputGuardSource source;
	source.buffer = status.rid; source.witness = &status.witness; source.request = original;
	source.root = acquire.output_root; source.group = slot->owned_group; source.member = physical.production_ordinal;
	source.backing = buffer->backing_serial; source.sequence = acquire.output_input.common.present_seq;
	out = source;
	return true;
}

// GuardStatus is a physical K source. The original typed caller proves the
// semantic HistoryGather mapping; Core checks every actual accepted use/chunk.
bool Registry::production_output_history_guard_uses_locked(const ProductionRequest &request, const ProductionResourceSlot &resource, uint64_t selected) const {
	if (!resource.output_guard_history || !resource.output_guard_readonly || resource.output_action_ordinal >= request.action_count) { return false; }
	const auto &acquire = request.actions[resource.output_action_ordinal];
	const auto &plan = acquire.output_plan;
	if (!acquire.staged_predecessor_intent || acquire.staged_history_guard != uint32_t(&resource - request.resources) ||
			!production_dto_valid(plan) || plan.acquire.action_ordinal != resource.output_action_ordinal ||
			plan.finalize_action_ordinal >= request.action_count || plan.finalize_action_ordinal <= resource.output_action_ordinal ||
			plan.seal_action_ordinal >= request.action_count || plan.seal_action_ordinal <= plan.finalize_action_ordinal ||
			plan.acquire.first_source_use > request.use_count || plan.acquire.source_use_count > request.use_count - plan.acquire.first_source_use) { return false; }
	const uint32_t slot = uint32_t(&resource - request.resources);
	for (uint32_t u = 0; u < request.use_count; ++u) {
		if (request.uses[u].declaration.resource_slot == slot && request.uses[u].request != &request) { return false; }
	}
	return production_history_guard_use_coverage(slot, resource.output_action_ordinal, plan.finalize_action_ordinal, plan.seal_action_ordinal,
			plan.acquire.first_source_use, plan.acquire.source_use_count, request.use_count, request.action_count,
			[&](uint32_t u) -> const GDExtensionProductionUseV1 & { return request.uses[u].declaration; },
			[&](uint32_t a) -> const GDExtensionProductionActionV2 & { return request.actions[a].declaration; }, selected);
}

Error Registry::production_output_history_guard_accept_locked(ProductionRequest &request, uint64_t acquire, uint32_t anchor_index, uint32_t guard_index, RenderingDevice &rd) {
	if (!Lock::owns(*this) || !rd.native_immutable_is_render_thread() || request.frozen || request.native_static_sealed ||
			request.cancelled || request.release_started || acquire >= request.action_count || anchor_index >= request.resource_count || guard_index >= request.resource_count || anchor_index == guard_index) { return ERR_INVALID_DATA; }
	const auto &anchor = request.resources[anchor_index];
	auto &guard = request.resources[guard_index];
	const auto &view = anchor.output_predecessor_view;
	if (!production_output_predecessor_matches_locked(request, anchor, rd) || anchor.output_predecessor_action != acquire || anchor.output_predecessor_no_records ||
			view.is_empty() || !view.owner || !view.owner->config.capacity || view.owner->config.profile != RendererRD::ExternalOutputConfig::INSTANCE_192 ||
			guard.request != &request || guard.request_serial != request.serial || guard.output_alias_prepared || guard.owned_serial || guard.use_count ||
			guard.retiring || guard.returned || !guard.output_predecessor_view.is_empty() || guard.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 ||
			guard.declaration.ownership != GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1 || guard.declaration.physical_bytes != 32) { return ERR_INVALID_DATA; }
	if (guard.output_guard_readonly) {
		return guard.output_guard_history && guard.output_action_ordinal == acquire && guard.output_guard_anchor == anchor_index &&
				production_output_guard_matches_locked(request, guard, rd) ? OK : ERR_ALREADY_IN_USE;
	}
	if (guard.imported || guard.primary.is_valid() || guard.backing_serial || guard.output_witness.cell.is_valid()) { return ERR_INVALID_DATA; }
	guard.output_guard_readonly = true; guard.output_guard_history = true; guard.output_action_ordinal = acquire; guard.output_guard_anchor = anchor_index;
	const Error bound = production_output_guard_bind_locked(request, guard, rd);
	if (bound != OK) { guard.output_guard_readonly = false; guard.output_guard_history = false; guard.output_action_ordinal = UINT64_MAX; guard.output_guard_anchor = UINT32_MAX; }
	return bound;
}

bool Registry::production_output_guard_resolve_locked(const ProductionRequest &request, const ProductionResourceSlot &resource, RenderingDevice &rd, ProductionOutputGuardSource &out) const {
	if (resource.output_guard_history) {
		if (!resource.output_guard_readonly || resource.output_alias_prepared || resource.request != &request || resource.request_serial != request.serial ||
				resource.owned_serial || resource.use_count || resource.retiring || resource.returned || request.source_device_lost ||
				resource.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || resource.declaration.ownership != GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1 ||
				resource.declaration.physical_bytes != 32 || resource.output_guard_anchor >= request.resource_count ||
				!production_output_history_guard_uses_locked(request, resource, UINT64_MAX)) { return false; }
		const auto &anchor = request.resources[resource.output_guard_anchor];
		if (!production_output_predecessor_matches_locked(request, anchor, rd) || anchor.output_predecessor_action != resource.output_action_ordinal || anchor.output_predecessor_no_records) { return false; }
		const auto &view = anchor.output_predecessor_view;
		if (!view.owner || !view.owner->config.capacity || view.owner->config.profile != RendererRD::ExternalOutputConfig::INSTANCE_192 || view.slot >= view.owner->slot_count || view.view_cell >= view.owner->view_capacity) { return false; }
		const auto &cell = view.owner->view_cells[view.view_cell]; const auto &slot = view.owner->slots[view.slot];
		if (cell.pin.is_empty() || !cell.pin.request || slot.original_action_ordinal >= cell.pin.request->action_count ||
				!production_output_guard_origin_matches_locked(*cell.pin.request, resource, cell.pin.request->actions[slot.original_action_ordinal])) { return false; }
		ProductionOutputGuardSource source;
		if (!production_output_guard_source_locked(*cell.pin.request, slot.original_action_ordinal, rd, source) || !source.witness ||
				source.sequence != slot.request.common.present_seq) { return false; }
		const auto *buffer = rd.storage_buffer_owner.get_or_null(source.buffer);
		if (!buffer || (resource.declaration.required_usage & ~uint64_t(buffer->usage))) { return false; }
		out = source; return true;
	}
	if (!resource.output_guard_readonly || resource.output_alias_prepared || resource.request != &request || resource.request_serial != request.serial ||
			resource.owned_serial || resource.use_count || resource.retiring || resource.returned || resource.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 ||
			resource.declaration.physical_bytes != 32 || resource.output_guard_consumer >= request.action_count || request.source_device_lost ||
			resource.declaration.ownership != GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1) { return false; }
	const uint32_t resource_index = uint32_t(&resource - request.resources);
	const auto &consumer = request.actions[resource.output_guard_consumer];
	if (consumer.request != &request || consumer.declaration.action_ordinal != resource.output_guard_consumer ||
			consumer.declaration.action_kind != GDEXTENSION_PRODUCTION_READBACK_V1) { return false; }
	bool used = false;
	for (uint32_t i = 0; i < request.use_count; ++i) {
		const auto &use = request.uses[i];
		if (use.declaration.resource_slot != resource_index) { continue; }
		if (use.request != &request || use.declaration.access != GDEXTENSION_PRODUCTION_READ_V1 || use.declaration.byte_offset || use.declaration.byte_size != 32 ||
				i < consumer.declaration.first_use || i - consumer.declaration.first_use >= consumer.declaration.use_count) { return false; }
		used = true;
	}
	if (!used) { return false; }
	const ProductionRequest *original = &request;
	const RendererRD::ExternalOutputOwner::Slot *slot = nullptr;
	uint64_t acquire_index = UINT64_MAX;
	{
		if (resource.declaration.ownership != GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1 || resource.output_guard_finalizer >= request.action_count ||
				resource.output_guard_finalizer >= resource.output_guard_consumer) { return false; }
		const auto &finalizer = request.actions[resource.output_guard_finalizer];
		acquire_index = finalizer.output_acquire_action_ordinal;
		if (acquire_index >= resource.output_guard_finalizer) { return false; }
		const auto &acquire = request.actions[acquire_index];
		auto *particles_storage = RendererRD::ParticlesStorage::get_singleton();
		const auto *particles = particles_storage ? particles_storage->particles_owner.get_or_null(acquire.output_particles) : nullptr;
		if (!particles || !particles->external || particles->external->device != &rd || particles->external->generation != acquire.output_generation ||
				!particles->external->admits_work() || acquire.output_slot >= particles->external->slot_count) { return false; }
		slot = &particles->external->slots[acquire.output_slot];
		if (slot->publication_closed || slot->pass_uses_closed || slot->lease_serial != acquire.output_lease ||
				slot->original_action_ordinal != acquire_index || slot->original_request.registry_instance != instance || slot->original_request.handle_serial != request.serial ||
				!const_cast<Registry *>(this)->production_output_pin_identity_matches_locked(slot->original_pin, slot->original_request, rd)) { return false; }
		if (!production_output_guard_origin_matches_locked(request, resource, acquire)) { return false; }
	}
	if (!original || !slot || original->actions[acquire_index].output_plan.finalize_action_ordinal != resource.output_guard_finalizer) { return false; }
	ProductionOutputGuardSource source;
	if (!production_output_guard_source_locked(*original, acquire_index, rd, source)) { return false; }
	const auto *buffer = rd.storage_buffer_owner.get_or_null(source.buffer);
	if (!buffer || (resource.declaration.required_usage & ~uint64_t(buffer->usage))) { return false; }
	out = source;
	return true;
}

bool Registry::production_output_guard_matches_locked(const ProductionRequest &request, const ProductionResourceSlot &resource, RenderingDevice &rd) const {
	ProductionOutputGuardSource source;
	return resource.imported && production_output_guard_resolve_locked(request, resource, rd, source) &&
			resource.primary == source.buffer && resource.backing_serial == source.backing && resource.output_witness.cell == source.witness->cell &&
			resource.output_group_serial == source.group && resource.output_root_serial == source.root && resource.output_member_ordinal == source.member;
}

Error Registry::production_output_guard_bind_locked(ProductionRequest &request, ProductionResourceSlot &resource, RenderingDevice &rd) {
	ProductionOutputGuardSource source;
	if (!production_output_guard_resolve_locked(request, resource, rd, source)) { return ERR_UNAUTHORIZED; }
	if (resource.imported) { return production_output_guard_matches_locked(request, resource, rd) ? OK : ERR_INVALID_DATA; }
	if (resource.primary.is_valid() || resource.backing_serial || !resource.output_witness.cell.is_null()) { return ERR_INVALID_DATA; }
	resource.primary = source.buffer; resource.backing_serial = source.backing; resource.output_witness = *source.witness;
	resource.output_root_serial = source.root; resource.output_group_serial = source.group; resource.output_member_ordinal = source.member;
	resource.imported = true;
	return OK;
}



bool Registry::production_output_alias_matches_locked(const ProductionRequest &request, const ProductionResourceSlot &slot, RenderingDevice &rd) const {
	if (!slot.output_alias_prepared || !slot.imported || slot.request != &request || slot.request_serial != request.serial || slot.output_action_ordinal >= request.action_count || slot.output_witness.cell.is_null() || !slot.output_root_serial || !slot.output_group_serial || slot.retiring || slot.returned) {
		return false;
	}
	const auto &action = request.actions[slot.output_action_ordinal];
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	const auto *particles = storage ? storage->particles_owner.get_or_null(action.output_particles) : nullptr;
	if (!particles || !particles->external || particles->external->device != &rd || particles->external->generation != action.output_generation || !particles->external->admits_work() || action.output_slot >= particles->external->slot_count) {
		return false;
	}
	const auto &owner_slot = particles->external->slots[action.output_slot];
	if (owner_slot.publication_closed || owner_slot.pass_uses_closed || owner_slot.original_action_ordinal != slot.output_action_ordinal || owner_slot.original_request.handle_serial != request.serial || owner_slot.lease_serial != action.output_lease) {
		return false;
	}
	const auto &physical = **slot.output_witness.cell;
	const auto *buffer = rd.storage_buffer_owner.get_or_null(slot.primary);
	if (action.request != &request || action.output_root != slot.output_root_serial ||
			(action.output_state != ProductionActionCell::OutputState::RESERVING && action.output_state != ProductionActionCell::OutputState::BOUND) ||
			!buffer || buffer->native_disposal.cell != slot.output_witness.cell ||
			physical.state != NativeDisposalWitness::State::LIVE || physical.device != uint64_t(rd.get_instance_id()) ||
			physical.production_request != &request || physical.production_request_serial != request.serial ||
			physical.production_group_serial != slot.output_group_serial || physical.production_pin_serial != slot.output_root_serial || physical.production_ordinal != slot.output_member_ordinal ||
			!physical.matches(slot.primary.get_id(), buffer->backing_serial, buffer->driver_id.id) || slot.backing_serial != buffer->backing_serial || buffer->size != slot.declaration.physical_bytes || (slot.declaration.required_usage & ~uint64_t(buffer->usage))) {
		return false;
	}
	for (uint32_t i = request.use_count + request.binding_used; i < request.capacity.resource_uses; ++i) {
		const auto &member = request.uses[i];
		if (member.output_role == ProductionUseCell::OutputRole::DISPOSAL_MEMBER && member.disposal_group_serial == slot.output_group_serial && member.disposal_ordinal == slot.output_member_ordinal) {
			return member.disposal.owned && member.disposal.request == &request && member.disposal.request_serial == request.serial &&
					member.disposal.rid == slot.primary.get_id() && member.disposal.serial == slot.backing_serial && member.disposal.state.load(std::memory_order_acquire) == 1;
		}
	}
	return false;
}

Error Registry::production_output_alias_bind_locked(ProductionRequest &request, ProductionActionCell &action, uint32_t role_index, const ProductionOutputPin &pin, uint64_t group, uint32_t ordinal, const NativeDisposalWitness &witness, RenderingDevice &rd) {
	if (role_index >= 3 || action.request != &request || action.declaration.action_ordinal >= request.action_count || &request.actions[action.declaration.action_ordinal] != &action || action.output_state != ProductionActionCell::OutputState::RESERVING || action.output_root != pin.serial || !production_output_pin_matches_locked(pin, { instance, request.serial }, rd) || witness.cell.is_null()) {
		return ERR_UNAUTHORIZED;
	}
	const auto &role = action.output_roles[role_index];
	if (!role.bytes || role.resource_slot >= request.resource_count || role.use_index >= request.use_count) {
		return ERR_INVALID_DATA;
	}
	auto &slot = request.resources[role.resource_slot];
	const auto &use = request.uses[role.use_index];
	const auto &physical = **witness.cell;
	const auto *buffer = rd.storage_buffer_owner.get_or_null(RID::from_uint64(physical.rid));
	if (!slot.output_alias_prepared || slot.output_action_ordinal != action.declaration.action_ordinal || slot.imported || slot.owned_serial || slot.use_count ||
			slot.declaration.original_generation != role.generation || slot.declaration.physical_bytes != role.bytes || use.declaration.resource_slot != role.resource_slot ||
			!buffer || buffer->size != role.bytes || (slot.declaration.required_usage & ~uint64_t(buffer->usage)) || buffer->native_disposal.cell != witness.cell || physical.state != NativeDisposalWitness::State::LIVE ||
			physical.production_request != &request || physical.production_request_serial != request.serial || physical.production_group_serial != group || physical.production_pin_serial != pin.serial || physical.production_ordinal != ordinal) {
		return ERR_INVALID_DATA;
	}
	slot.primary = RID::from_uint64(physical.rid);
	slot.backing_serial = buffer->backing_serial;
	slot.output_witness = witness;
	slot.output_group_serial = group;
	slot.output_root_serial = pin.serial;
	slot.output_member_ordinal = ordinal;
	slot.imported = true;
	if (!production_output_alias_matches_locked(request, slot, rd)) {
		// Keep the exact association for cleanup; never discard a factory prefix.
		action.output_state = ProductionActionCell::OutputState::UNKNOWN;
		return ERR_INVALID_DATA;
	}
	return OK;
}

bool Registry::production_output_action_pin_matches_locked(const ProductionRequest &request, const ProductionOutputPin &pin, RenderingDevice &rd) const {
	if (request.next_action >= request.action_count || request.capture_inflight || request.cancelled || request.release_started || request.control.state != ProductionControlState::EXECUTING || request.control.input.kind != ProductionControlKind::RECORD || pin.request != &request || pin.request_serial != request.serial || pin.registry != this || !pin.serial || pin.cell_index >= request.capacity.resource_uses) {
		return false;
	}
	const auto &action = request.actions[request.next_action];
	const auto &row = request.uses[pin.cell_index];
	return action.request == &request && action.declaration.action_ordinal == request.next_action && action.declaration.action_kind == GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 && action.output_state == ProductionActionCell::OutputState::RESERVING && action.output_root == pin.serial && row.output_consumer == &pin && row.output_pin_serial == pin.serial && row.output_owner_kind == ProductionOutputOwnerKind::SLOT && row.output_particles == action.output_particles && row.output_generation == action.output_generation && row.output_slot == action.output_slot && row.output_lease == action.output_lease && production_output_source_matches_locked(request, action, rd);
}

bool Registry::production_output_factory_pin_matches_locked(const ProductionRequest &request, const ProductionOutputPin &pin, RenderingDevice &rd) {
	if (production_output_action_pin_matches_locked(request, pin, rd)) {
		return true;
	}
	if (request.next_action >= request.action_count || request.capture_inflight || request.cancelled || request.release_started || request.effects_pending || request.source_device_lost || !request.frozen || request.control.state != ProductionControlState::EXECUTING || request.control.input.kind != ProductionControlKind::RECORD || request.attachment->held_device != &rd || !production_output_pin_matches_locked(pin, { instance, request.serial }, rd)) {
		return false;
	}
	const auto &finalizer = request.actions[request.next_action];
	if (finalizer.request != &request || finalizer.declaration.action_ordinal != request.next_action || finalizer.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || !finalizer.prepared || finalizer.output_acquire_action_ordinal >= request.next_action) {
		return false;
	}
	const auto &acquire = request.actions[finalizer.output_acquire_action_ordinal];
	const auto &row = request.uses[pin.cell_index];
	return acquire.request == &request && acquire.output_plan_prepared && acquire.output_plan.finalize_action_ordinal == request.next_action && acquire.output_plan.finalize_canonical_suboperation == finalizer.declaration.canonical_suboperation && acquire.output_plan.finalize_source_recipe_substep == finalizer.declaration.source_recipe_substep && !acquire.output_guard_entered && acquire.output_state == ProductionActionCell::OutputState::BOUND && acquire.output_root == pin.serial && row.output_consumer == &pin && row.output_pin_serial == pin.serial && row.output_owner_kind == ProductionOutputOwnerKind::SLOT && row.output_particles == acquire.output_particles && row.output_generation == acquire.output_generation && row.output_slot == acquire.output_slot && row.output_lease == acquire.output_lease && production_output_source_matches_locked(request, acquire, rd);
}

Error Registry::production_output_action_pin_locked(ProductionRequest &request, ProductionActionCell &action, RenderingDevice &rd, ProductionOutputPin &pin) {
	if (!pin.is_empty() || !request.frozen || request.cancelled || request.release_started || request.capture_inflight || request.control.state != ProductionControlState::EXECUTING || request.control.input.kind != ProductionControlKind::RECORD ||
			action.request != &request || action.declaration.action_ordinal >= request.action_count || &request.actions[action.declaration.action_ordinal] != &action || request.next_action != action.declaration.action_ordinal || !action.prepared || !action.output_source_prepared || action.output_state != ProductionActionCell::OutputState::UNBOUND || request.attachment->held_device != &rd || !request.next_output_pin_serial) {
		return ERR_UNAUTHORIZED;
	}
	if (request.use_count > request.capacity.resource_uses || request.binding_used > request.capacity.resource_uses - request.use_count) {
		return ERR_INVALID_DATA;
	}
	for (uint32_t i = request.use_count + request.binding_used; i < request.capacity.resource_uses; ++i) {
		auto &cell = request.uses[i];
		if (cell.request != &request || cell.dependency.physical || cell.output_role != ProductionUseCell::OutputRole::UNUSED || cell.output_consumer || cell.output_pin_serial) {
			continue;
		}
		const uint64_t serial = request.next_output_pin_serial;
		request.next_output_pin_serial = serial == UINT64_MAX ? 0 : serial + 1;
		pin.registry = this;
		pin.request = &request;
		pin.request_serial = request.serial;
		pin.serial = serial;
		pin.cell_index = i;
		cell.output_role = ProductionUseCell::OutputRole::NATIVE_CONSUMER;
		cell.output_consumer = &pin;
		cell.output_pin_serial = serial;
		action.output_root = serial;
		action.output_state = ProductionActionCell::OutputState::RESERVING;
		return OK;
	}
	return ERR_OUT_OF_MEMORY;
}

Error Registry::production_output_action_owner_bind_locked(ProductionRequest &request, ProductionActionCell &action, ProductionOutputPin &pin, RenderingDevice &rd, RID particles, uint64_t generation, uint32_t slot, uint64_t lease) {
	if (!production_output_pin_matches_locked(pin, { instance, request.serial }, rd) || action.request != &request || action.declaration.action_ordinal >= request.action_count || &request.actions[action.declaration.action_ordinal] != &action || request.next_action != action.declaration.action_ordinal || action.output_state != ProductionActionCell::OutputState::RESERVING || action.output_root != pin.serial || !generation || !lease || slot == UINT32_MAX || request.capture_inflight || request.cancelled || request.release_started) {
		return ERR_UNAUTHORIZED;
	}
	const Error context = production_output_owner_context_locked(pin, rd);
	if (context != OK) {
		return context;
	}
	if (!rd._production_output_owner_matches(ProductionOutputOwnerKind::SLOT, particles, generation, slot, lease, pin, { instance, request.serial })) {
		return ERR_UNAUTHORIZED;
	}
	auto &row = request.uses[pin.cell_index];
	if (row.output_generation) {
		return row.output_owner_kind == ProductionOutputOwnerKind::SLOT && row.output_particles == particles && row.output_generation == generation && row.output_slot == slot && row.output_lease == lease ? OK : ERR_ALREADY_IN_USE;
	}
	row.output_owner_kind = ProductionOutputOwnerKind::SLOT;
	row.output_particles = particles;
	row.output_generation = generation;
	row.output_slot = slot;
	row.output_lease = lease;
	row.output_prev = UINT32_MAX;
	row.output_next = request.output_owner_head;
	if (row.output_next != UINT32_MAX) {
		request.uses[row.output_next].output_prev = pin.cell_index;
	}
	request.output_owner_head = pin.cell_index;
	if (request.output_owner_cursor == UINT32_MAX) {
		request.output_owner_cursor = pin.cell_index;
	}
	action.output_particles = particles;
	action.output_generation = generation;
	action.output_slot = slot;
	action.output_lease = lease;
	return OK;
}

// Called only by the original generation creation transaction after all
// original mesh/material selections and surface-source holds succeeded.
Error Registry::production_output_demand_publish(ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, RendererRD::ExternalOutputOwner &owner, RenderingDevice &rd) {
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	Error error = production_physical_request_locked(context, ref, request, device, true);
	if (error != OK) {
		return error;
	}
	const auto &pin = owner.generation_pin;
	if (device != &rd || !rd.native_immutable_is_render_thread() || !owner.generation_root_matches(pin) || !production_output_pin_matches_locked(pin, ref, rd) || pin.request != request || owner.device != &rd || !owner.admits_work() || !owner.metadata_directory_installed || owner.metadata_lease.is_empty() || !owner.generation_config_inline || !owner.generation_selection_complete || !owner.generation_surfaces_complete || owner.config.profile != RendererRD::ExternalOutputConfig::INSTANCE_192) {
		return ERR_UNAUTHORIZED;
	}
	auto &row = request->uses[pin.cell_index];
	if (row.output_owner_kind != ProductionOutputOwnerKind::GENERATION || row.output_generation != owner.generation || row.output_slot != UINT32_MAX || row.output_lease || row.output_demand_ready || !rd._production_output_owner_matches(ProductionOutputOwnerKind::GENERATION, row.output_particles, row.output_generation, UINT32_MAX, 0, pin, ref)) {
		return ERR_INVALID_DATA;
	}
	RendererRD::ExternalOutputStorage::Instance192Layout layout;
	error = RendererRD::ExternalOutputStorage::plan_instance192(owner.config.capacity, owner.generation_material_count, layout);
	if (error != OK) {
		return error;
	}
	row.output_demand = { owner.config.capacity, owner.generation_material_count, owner.view_capacity, owner.read_capacity, owner.surface_capacity };
	row.output_demand_ready = true;
	row.output_demand_live.store(true, std::memory_order_release);
	return OK;
}

// Registry and this exact RD are held. No admission/selection or ownership is created.
Error Registry::production_output_demand_locked(ProductionAttachment &attachment, RenderingDevice &rd, const GDExtensionProductionOutputDemandInputV1 &input, GDExtensionProductionOutputGuardDemandV1 &out) {
	if (!production_dto_valid(input) || input.reserved || !input.particles || !input.generation || (input.source_space != 1 && input.source_space != 2) || ((!input.read_capacity) != (!input.read_graph_capacity))) {
		return ERR_INVALID_PARAMETER;
	}
	const ProductionUseCell *root = nullptr;
	for (uint32_t i = 0; i < attachment.quote.capacity.requests; ++i) {
		const auto &generation_request = attachment.requests[i];
		if (!generation_request.serial || (input.generation_request.handle_serial && (input.generation_request.registry_instance != instance || input.generation_request.handle_serial != generation_request.serial))) {
			continue;
		}
		if (generation_request.attachment != &attachment || generation_request.cancelled || generation_request.release_started || generation_request.source_device_lost) {
			continue;
		}
		uint32_t visits = 0;
		for (uint32_t cell = generation_request.output_owner_head; cell != UINT32_MAX; cell = generation_request.uses[cell].output_next) {
			if (cell >= generation_request.capacity.resource_uses || ++visits > generation_request.capacity.resource_uses) {
				return ERR_INVALID_DATA;
			}
			const auto &row = generation_request.uses[cell];
			if (row.output_owner_kind != ProductionOutputOwnerKind::GENERATION || row.output_particles.get_id() != input.particles || row.output_generation != input.generation) {
				continue;
			}
			if (root || !row.output_consumer || !production_output_pin_identity_matches_locked(*row.output_consumer, { instance, generation_request.serial }, rd) || !row.output_demand_ready || !row.output_demand_live.load(std::memory_order_acquire)) {
				return ERR_UNAVAILABLE;
			}
			root = &row;
		}
	}
	if (!root) {
		return ERR_UNAUTHORIZED;
	}
	const auto &facts = root->output_demand;
	const uint32_t surfaces = facts.surfaces;
	RendererRD::ExternalOutputStorage::Instance192Layout layout;
	Error error = RendererRD::ExternalOutputStorage::plan_instance192(facts.capacity, surfaces, layout);
	if (error != OK) {
		return error;
	}
	if (input.read_capacity > facts.read_capacity || (input.read_capacity && input.read_capacity < surfaces)) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	using Guard = RendererRD::ParticlesExternalGuard;
	const uint64_t proof_bytes = sizeof(Guard::GeometryProofHeader) + uint64_t(surfaces) * sizeof(Guard::SurfaceProof);
	ProductionOutputGuardBound bound;
	const uint32_t max_groups = rd.limit_get(RD::LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X);
	error = production_output_guard_bound(facts.capacity, surfaces, max_groups, proof_bytes, rd.upload_staging_buffers.block_size, bound);
	if (error != OK) {
		return error;
	}
	uint32_t native = 0;
	error = production_membership_bound_locked(rd, bound.guard_graphs, rd.transfer_worker_pool_max_size, native);
	if (error != OK) {
		return error;
	}
	const bool pipelines = layout.resource_count && input.read_capacity;
	const uint64_t borrower = pipelines ? uint64_t(facts.view_capacity) + facts.surface_capacity + facts.read_capacity : 0;
	const uint64_t per_row = uint64_t(input.read_graph_capacity) * 2 + 6;
	const uint64_t fixed = 1 + (layout.resource_count ? uint64_t(layout.resource_count) + 1 : 0) + borrower + bound.guard_owned_resources + 1;
	if (pipelines && input.read_capacity > (UINT64_MAX - fixed) / per_row) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	const uint64_t read_graphs = uint64_t(input.read_capacity) * input.read_graph_capacity;
	if (read_graphs > UINT32_MAX) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	GDExtensionProductionOutputGuardDemandV1 demand = {};
	demand.abi = production_dto_abi<GDExtensionProductionOutputGuardDemandV1>();
	demand.acquire_action_ordinal = input.acquire_action_ordinal;
	demand.finalize_action_ordinal = input.finalize_action_ordinal;
	demand.particles = input.particles;
	demand.generation = root->output_generation;
	demand.capacity = facts.capacity;
	demand.surfaces = surfaces;
	demand.max_groups_x = max_groups;
	demand.native_workers = rd.transfer_worker_pool_max_size;
	demand.guard_work = bound.work;
	demand.proof_bytes = proof_bytes;
	demand.guard_dispatches = bound.dispatches;
	demand.proof_upload_graphs = bound.proof_upload_graphs;
	demand.guard_graphs = bound.guard_graphs;
	demand.guard_native_memberships = native;
	demand.guard_owned_resources = bound.guard_owned_resources;
	demand.required_output_use_cells = fixed + (pipelines ? uint64_t(input.read_capacity) * per_row : 0);
	demand.required_owned_groups = (layout.resource_count ? 1 : 0) + (pipelines ? uint64_t(input.read_capacity) * 4 : 0) + 1;
	demand.read_graphs = uint32_t(read_graphs);
	out = demand;
	return OK;
}

// Single graph schedule used for freeze and finalizer revalidation. All byte
// counts come from original prepared rows; accepted guard limits stay local.
Error Registry::production_schedule_locked(ProductionRequest &request, RenderingDevice &rd, bool publish) {
	// Temporary finite scratch only; retained schedule lives in the paid
	// original action cells. No field publishes until every check succeeds.
	Vector<uint32_t> bounds;
	if (publish && bounds.resize(request.action_count) != OK) {
		return ERR_OUT_OF_MEMORY;
	}
	uint32_t suffix = 0;
	for (uint32_t index = request.action_count; index > 0; --index) {
		auto &action = request.actions[index - 1];
		uint32_t graphs = 1;
		Error error = OK;
		if (action.declaration.action_kind == GDEXTENSION_PRODUCTION_UPLOAD_V1) {
			error = production_staging_graph_bound(action.upload.bytes, rd.upload_staging_buffers.block_size, graphs);
		} else if (action.declaration.action_kind == GDEXTENSION_PRODUCTION_READBACK_V1) {
			if (action.readback_slot >= request.readback_count) {
				return ERR_INVALID_DATA;
			}
			error = production_staging_graph_bound(request.readbacks[action.readback_slot].declaration.byte_size, rd.download_staging_buffers.block_size, graphs);
		} else if (action.declaration.action_kind == GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1) {
			// Preserve the original Acquire-only preparation path. It has no
			// guard; only its original finite read graph reservation is paid.
			if (!action.output_plan_prepared && action.output_acquire_action_ordinal == UINT64_MAX && action.output_source_prepared) {
				const uint64_t reads = uint64_t(action.output_read_capacity) * action.output_read_graph_capacity;
				if (reads > UINT32_MAX || reads > UINT32_MAX - suffix) {
					return ERR_PARAMETER_RANGE_ERROR;
				}
				graphs = uint32_t(reads);
				if (request.frozen && (action.graph_schedule_bound != graphs || action.graph_suffix_bound != suffix)) {
					return ERR_INVALID_DATA;
				}
				if (publish) {
					bounds.write[index - 1] = graphs;
				}
				suffix += graphs;
				continue;
			}
			const uint64_t acquire_index = action.output_plan_prepared ? index - 1 : action.output_acquire_action_ordinal;
			if (acquire_index >= request.action_count) {
				return ERR_UNCONFIGURED;
			}
			const auto &acquire = request.actions[acquire_index];
			if (!acquire.output_plan_prepared) {
				return ERR_UNCONFIGURED;
			}
			const auto &plan = acquire.output_plan;
			if (plan.finalize_action_ordinal >= request.action_count || plan.seal_action_ordinal >= request.action_count) {
				return ERR_INVALID_DATA;
			}
			GDExtensionProductionOutputDemandInputV1 input = {};
			input.abi = production_dto_abi<GDExtensionProductionOutputDemandInputV1>();
			// Project only immutable sizing/finite recipe fields. Camera and
			// currentness remain the original prepared Acquire's runtime contract.
			input.particles = plan.acquire.input.particles;
			input.generation = plan.acquire.input.common.generation;
			input.acquire_action_ordinal = plan.acquire.action_ordinal;
			input.acquire_canonical_suboperation = plan.acquire.canonical_suboperation;
			input.acquire_source_recipe_substep = plan.acquire.source_recipe_substep;
			input.read_capacity = plan.acquire.read_capacity;
			input.read_graph_capacity = plan.acquire.read_graph_capacity;
			input.source_space = plan.source_space;
			input.finalize_action_ordinal = plan.finalize_action_ordinal;
			input.finalize_canonical_suboperation = request.actions[plan.finalize_action_ordinal].declaration.canonical_suboperation;
			input.finalize_source_recipe_substep = request.actions[plan.finalize_action_ordinal].declaration.source_recipe_substep;
			input.seal_action_ordinal = plan.seal_action_ordinal;
			input.seal_canonical_suboperation = request.actions[plan.seal_action_ordinal].declaration.canonical_suboperation;
			input.seal_source_recipe_substep = request.actions[plan.seal_action_ordinal].declaration.source_recipe_substep;
			GDExtensionProductionOutputGuardDemandV1 demand;
			error = production_output_demand_locked(*request.attachment, rd, input, demand);
			if (error != OK) {
				return error;
			}
			if (demand.guard_work > plan.guard_work_limit || demand.guard_dispatches > plan.guard_dispatch_limit || demand.guard_graphs > plan.guard_graph_limit || demand.guard_native_memberships > plan.guard_native_limit || demand.guard_owned_resources > plan.guard_owned_resource_limit || request.native_workers_bound != demand.native_workers) {
				return ERR_PARAMETER_RANGE_ERROR;
			}
			graphs = 0;
			if (index - 1 == plan.finalize_action_ordinal) {
				graphs = plan.guard_graph_limit;
			} else if (index - 1 == plan.seal_action_ordinal) {
				graphs = demand.read_graphs;
			} else if (index - 1 != acquire_index) {
				return ERR_INVALID_DATA;
			}
		}
		if (error != OK) {
			return error;
		}
		if (graphs > UINT32_MAX - suffix) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
		if (request.frozen && (action.graph_schedule_bound != graphs || action.graph_suffix_bound != suffix)) {
			return ERR_INVALID_DATA;
		}
		if (publish) {
			bounds.write[index - 1] = graphs;
		}
		suffix += graphs;
	}
	uint32_t native = 0;
	const Error error = production_membership_bound_locked(rd, suffix, request.native_workers_bound, native);
	if (error != OK) {
		return error;
	}
	if (suffix > request.capacity.graph_memberships || native > request.capacity.native_memberships) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	if (publish) {
		suffix = 0;
		for (uint32_t index = request.action_count; index > 0; --index) {
			auto &action = request.actions[index - 1];
			action.graph_schedule_bound = bounds[index - 1];
			action.graph_suffix_bound = suffix;
			suffix += bounds[index - 1];
		}
	}
	return OK;
}

namespace {
bool predecessor_ref_equal(const GDExtensionOutputConsumedProofRefV2 &a, const GDExtensionOutputConsumedProofRefV2 &b) {
	return production_dto_valid(a) && production_dto_valid(b) && a.registry_instance == b.registry_instance && a.request_serial == b.request_serial &&
			a.borrow_control_serial == b.borrow_control_serial && a.action_ordinal == b.action_ordinal && a.particles == b.particles &&
			a.generation == b.generation && a.lease_serial == b.lease_serial && a.view_cell_serial == b.view_cell_serial &&
			a.slot_index == b.slot_index && a.view_cell_index == b.view_cell_index && !memcmp(a.lease_token.bytes, b.lease_token.bytes, 32) &&
			!memcmp(a.original_color_token.bytes, b.original_color_token.bytes, 32);
}
bool predecessor_equal(const GDExtensionProductionOutputPredecessorV2 &a, const GDExtensionProductionOutputPredecessorV2 &b) {
	return production_dto_valid(a) && production_dto_valid(b) && a.resource_slot == b.resource_slot && a.use_index == b.use_index && predecessor_ref_equal(a.proof, b.proof);
}
GDExtensionDeviceSessionResultV1 predecessor_result(Error error) { return { int32_t(error), error == OK ? 0u : 16u, 0, 0 }; }
} // namespace

Error Registry::production_output_predecessor_service_locked(ProductionRequest &request, RenderingDevice &rd) {
	auto &cell = request.control;
	const auto &input = cell.input;
	const auto &binding = input.predecessor;
	const auto &ref = binding.proof;
	const auto &prepare = input.predecessor_prepare;
	if (!Lock::owns(*this) || !rd.native_immutable_is_render_thread() || !input.predecessor_bind || input.kind != ProductionControlKind::OBSERVE ||
			cell.state != ProductionControlState::EXECUTING || input.request_serial != request.serial || !request.attachment ||
			!input.budget.controls || !input.budget.receipts || !input.budget.passes || !input.budget.deadline_micros ||
			OS::get_singleton()->get_ticks_usec() >= input.budget.deadline_micros) {
		return ERR_BUSY;
	}
	if (!production_dto_valid(binding) || !production_dto_valid(ref) || ref.registry_instance != instance || !ref.borrow_control_serial ||
			!production_dto_valid(prepare) || binding.use_index < prepare.first_source_use ||
			binding.use_index - prepare.first_source_use >= prepare.source_use_count) {
		return ERR_INVALID_DATA;
	}
	// Resolve only the same accepted attachment's finite original request slab.
	// No request/owner is created by a locator or caller RID.
	ProductionRequest *original = nullptr;
	for (uint32_t i = 0; i < request.attachment->quote.capacity.requests; ++i) {
		auto &candidate = request.attachment->requests[i];
		if (candidate.serial == ref.request_serial) { original = &candidate; break; }
	}
	if (!original || original == &request || original->attachment != request.attachment || ref.action_ordinal >= original->action_count) {
		return ERR_INVALID_DATA;
	}
	auto &action = original->actions[ref.action_ordinal];
	if (action.request != original || !action.consumed_proof_requested || action.consumed_proof_returned ||
			!predecessor_ref_equal(action.consumed_read_ref, ref) || action.output_particles.get_id() != ref.particles ||
			action.output_generation != ref.generation || action.output_slot != ref.slot_index || action.output_lease != ref.lease_serial) {
		return ERR_UNAUTHORIZED;
	}
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	auto *particles = storage ? storage->particles_owner.get_or_null(action.output_particles) : nullptr;
	if (!particles || !particles->external || particles->external->device != &rd || particles->external->generation != ref.generation ||
			ref.slot_index >= particles->external->slot_count) { return ERR_UNAUTHORIZED; }
	auto &slot = particles->external->slots[ref.slot_index];
	auto &view = slot.consumed_proof_view;
	if (slot.original_registry != this || slot.original_request.handle_serial != original->serial || slot.original_action_ordinal != ref.action_ordinal ||
			slot.consumed_borrow_serial != ref.borrow_control_serial || view.is_empty() || view.serial != ref.view_cell_serial ||
			view.view_cell != ref.view_cell_index || view.slot != ref.slot_index || slot.lease_token.size() != 32 ||
			memcmp(slot.lease_token.ptr(), ref.lease_token.bytes, 32) ||
			!particles->external->native_color_consumption_matches(ref.slot_index, ref.original_color_token)) {
		return ERR_UNAUTHORIZED;
	}
	cell.output.service_usage = { 1, 1, 1 };
	const Error accepted = production_output_predecessor_accept_locked(request, prepare.action_ordinal, binding.resource_slot, binding.use_index, view, rd, &prepare.input, input.predecessor_staged);
	if (accepted == OK) {
		auto &resource = request.resources[binding.resource_slot];
		resource.output_predecessor_ref = ref;
		resource.output_predecessor_use = binding.use_index;
		const auto guard_slot = request.actions[prepare.action_ordinal].staged_history_guard;
		if (guard_slot != UINT32_MAX) { return production_output_history_guard_accept_locked(request, prepare.action_ordinal, binding.resource_slot, guard_slot, rd); }
	}
	return accepted;
}

Error Registry::production_output_predecessor_control(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOutputPrepareV1 &prepare,
		const GDExtensionProductionOutputPredecessorV2 &binding, const GDExtensionProductionServiceBudgetV1 &budget, bool staged) {
	uint64_t serial = 0;
	{
		Lock lock(*this, Lock::BLOCK_OR_BORROW);
		ProductionRequest *request = nullptr; RenderingDevice *rd = nullptr;
		const Error resolved = production_request_locked(ref, false, request, rd);
		if (resolved != OK) { return resolved; }
		if (request->contract_version != 2 || request->frozen || request->native_static_sealed || request->cancelled || request->release_started || request->capture_inflight ||
				request->continuation_inflight || prepare.action_ordinal >= request->action_count || binding.resource_slot >= request->resource_count ||
				binding.use_index >= request->use_count) { return ERR_INVALID_DATA; }
		auto &cell = request->control;
		if (cell.state != ProductionControlState::OWNER_CONSUMED) {
			if (!cell.input.predecessor_bind || cell.input.predecessor_staged != staged || cell.input.kind != ProductionControlKind::OBSERVE ||
					memcmp(&cell.input.predecessor_prepare, &prepare, sizeof(prepare)) || !predecessor_equal(cell.input.predecessor, binding)) { return ERR_BUSY; }
			if (cell.state == ProductionControlState::PENDING && (!cell.input.budget.controls || !cell.input.budget.receipts ||
					!cell.input.budget.passes || !cell.input.budget.deadline_micros || OS::get_singleton()->get_ticks_usec() >= cell.input.budget.deadline_micros)) {
				// No queue publication occurred. Return only this expired immutable
				// control; a later call may offer a separate original control turn.
				cell.output = {};
				cell.output.request_serial = request->serial;
				cell.output.control_serial = cell.serial;
				cell.output.result = predecessor_result(ERR_BUSY);
				production_report_locked(*request, cell.output.report);
				cell.state = ProductionControlState::RETURNED;
			}
			if (cell.state == ProductionControlState::RETURNED) {
				const Error error = Error(cell.output.result.native_error);
				cell.state = ProductionControlState::OWNER_CONSUMED;
				return error;
			}
			serial = cell.serial;
		} else {
			// Once bound, the same immutable source survives explicit proof return.
			// Original normal prepare still rechecks the entire READ span.
			const auto &resource = request->resources[binding.resource_slot];
			if (!resource.output_predecessor_view.is_empty()) {
				const auto guard_slot = request->actions[prepare.action_ordinal].staged_history_guard;
				const bool guard_ready = guard_slot == UINT32_MAX || (guard_slot < request->resource_count &&
						request->resources[guard_slot].output_guard_history && request->resources[guard_slot].imported);
				return guard_ready && resource.output_predecessor_action == prepare.action_ordinal &&
						resource.output_predecessor_use == binding.use_index && predecessor_ref_equal(resource.output_predecessor_ref, binding.proof) &&
						output_input_equal(resource.output_predecessor_input, prepare.input) ? OK : ERR_ALREADY_IN_USE;
			}
			if (request->actions[prepare.action_ordinal].prepared || !request->next_control_serial) { return ERR_INVALID_DATA; }
			if (!budget.controls || !budget.receipts || !budget.passes || !budget.deadline_micros || OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros) { return ERR_BUSY; }
			request->release_ready = {};
			cell = {};
			cell.serial = request->next_control_serial;
			request->next_control_serial = cell.serial == UINT64_MAX ? 0 : cell.serial + 1;
			cell.input = { request->serial, request->operation.record_generation, ProductionControlKind::OBSERVE, budget, UINT32_MAX, false };
			cell.input.predecessor_bind = true; cell.input.predecessor_staged = staged;
			cell.input.predecessor = binding;
			cell.input.predecessor_prepare = prepare;
			cell.state = ProductionControlState::PENDING;
			serial = cell.serial;
		}
	}
	const Error scheduled = production_control_schedule(ref, serial);
	return scheduled == OK ? ERR_BUSY : scheduled;
}

GDExtensionDeviceSessionResultV1 Registry::production_prepare_output_v2(GDExtensionProductionRequestRefV1 ref,
		const GDExtensionProductionOutputPrepareV2 *input, const GDExtensionProductionServiceBudgetV1 *budget) {
	const Error contract = production_contract_check(ref, 2);
	if (contract != OK) { return predecessor_result(contract); }
	if (!input || uintptr_t(input) % alignof(*input) || !budget || uintptr_t(budget) % alignof(*budget) || !production_dto_valid(*input) ||
			!production_dto_valid(input->prepare) || !production_dto_valid(input->predecessor) || !production_dto_valid(input->predecessor.proof) ||
			!production_dto_valid(*budget) || budget->reserved) { return predecessor_result(ERR_INVALID_PARAMETER); }
	const Error bound = production_output_predecessor_control(ref, input->prepare, input->predecessor, *budget);
	return bound == OK ? production_prepare_output(ref, &input->prepare) : predecessor_result(bound);
}

GDExtensionDeviceSessionResultV1 Registry::production_prepare_output_plan_v2(GDExtensionProductionRequestRefV1 ref,
		const GDExtensionProductionOutputPlanV2 *input, const GDExtensionProductionServiceBudgetV1 *budget) {
	const Error contract = production_contract_check(ref, 2);
	if (contract != OK) { return predecessor_result(contract); }
	if (!input || uintptr_t(input) % alignof(*input) || !budget || uintptr_t(budget) % alignof(*budget) || !production_dto_valid(*input) ||
			!production_dto_valid(input->plan) || !production_dto_valid(input->predecessor) || !production_dto_valid(input->predecessor.proof) ||
			!production_dto_valid(*budget) || budget->reserved) { return predecessor_result(ERR_INVALID_PARAMETER); }
	const Error bound = production_output_predecessor_control(ref, input->plan.acquire, input->predecessor, *budget);
	return bound == OK ? production_prepare_output_plan(ref, &input->plan) : predecessor_result(bound);
}

GDExtensionDeviceSessionResultV1 Registry::production_stage_output_plan_with_predecessor_v2(GDExtensionProductionRequestRefV1 ref,
		const GDExtensionProductionOutputPlanV2 *input, const GDExtensionProductionServiceBudgetV1 *stage_budget, const GDExtensionProductionServiceBudgetV1 *turn_budget) {
	return production_stage_output_plan_with_history_guard_v2(ref, input, UINT32_MAX, stage_budget, turn_budget);
}

GDExtensionDeviceSessionResultV1 Registry::production_stage_output_plan_with_history_guard_v2(GDExtensionProductionRequestRefV1 ref,
		const GDExtensionProductionOutputPlanV2 *input, uint32_t guard_slot, const GDExtensionProductionServiceBudgetV1 *stage_budget, const GDExtensionProductionServiceBudgetV1 *turn_budget) {
	const Error contract = production_contract_check(ref, 2); if (contract != OK) { return predecessor_result(contract); }
	if (!input || uintptr_t(input) % alignof(GDExtensionProductionOutputPlanV2) || !production_dto_valid(*input) ||
			!production_dto_valid(input->plan) || !production_dto_valid(input->predecessor) || !production_dto_valid(input->predecessor.proof) ||
			!stage_budget || uintptr_t(stage_budget) % alignof(GDExtensionProductionServiceBudgetV1) || !production_dto_valid(*stage_budget) || stage_budget->reserved ||
			!stage_budget->controls || !stage_budget->passes || !stage_budget->receipts || !stage_budget->deadline_micros ||
			!turn_budget || uintptr_t(turn_budget) % alignof(GDExtensionProductionServiceBudgetV1) || !production_dto_valid(*turn_budget) || turn_budget->reserved) { return predecessor_result(ERR_INVALID_PARAMETER); }
	RendererRD::ExternalOutputRequest parsed;
	if (output_decode(input->plan.acquire.input, parsed, true) != OK || !input->plan.acquire.input.common.has_previous_consumed_tuple || input->plan.acquire.input.common.history_reset) { return predecessor_result(ERR_INVALID_DATA); }
	{
		Lock lock(*this); ProductionRequest *request = nullptr; RenderingDevice *rd = nullptr;
		const Error resolved = production_request_locked(ref, false, request, rd); if (resolved != OK) { return predecessor_result(resolved); }
		const auto ordinal = input->plan.acquire.action_ordinal;
		if (request->native_static_sealed || request->frozen || ordinal >= request->action_count || request->cancelled || request->release_started) { return predecessor_result(ERR_BUSY); }
		auto &action = request->actions[ordinal];
		if (guard_slot != UINT32_MAX && (guard_slot >= request->resource_count || guard_slot == input->predecessor.resource_slot ||
				request->resources[guard_slot].declaration.physical_bytes != 32 || request->resources[guard_slot].declaration.ownership != GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1)) { return predecessor_result(ERR_INVALID_DATA); }
		if (!action.host_view_prepared || !action.preinput_host_prepared || action.preinput_camera_source.is_valid() || action.camera_ready_subscriber.subscribed ||
				memcmp(action.preinput_host_transform.host_token.bytes, input->plan.acquire.input.common.current_host_token.bytes, 32)) { return predecessor_result(ERR_UNCONFIGURED); }
		if (action.staged_predecessor_intent) {
			if (action.staged_history_guard != guard_slot || memcmp(&action.output_plan, &input->plan, sizeof(input->plan)) || memcmp(&request->native_stage_budget, stage_budget, sizeof(*stage_budget))) { return predecessor_result(ERR_ALREADY_IN_USE); }
		} else {
			if (action.prepared || request->native_staged_anchor != UINT64_MAX || OS::get_singleton()->get_ticks_usec() >= stage_budget->deadline_micros) { return predecessor_result(ERR_BUSY); }
			action.staged_predecessor_intent = true; action.output_plan = input->plan; action.staged_history_guard = guard_slot;
			request->native_staged_anchor = ordinal; request->native_stage_budget = *stage_budget;
		}
	}
	// An already accepted predecessor OBSERVE must remain consumable even
	// after the immutable native deadline expires. New intent was time-checked
	// above; normal stage still refuses expired execution after exact consume.
	// The special original OBSERVE binds only genuine prior proof custody; it
	// does not subscribe a camera or pretend the next capture already exists.
	const Error retained = production_output_predecessor_control(ref, input->plan.acquire, input->predecessor, *turn_budget, true);
	return retained == OK ? production_stage_output_plan_v2(ref, &input->plan, stage_budget) : predecessor_result(retained);
}

Error Registry::production_output_history_validate_locked(ProductionRequest &request, ProductionActionCell &action, RenderingDevice &rd, const GDExtensionOutputAcquireInputV1 &current) {
	if (!Lock::owns(*this) || !action.staged_predecessor_intent || !current.common.has_previous_consumed_tuple || !current.common.has_actual_previous_camera ||
			current.common.history_reset || !current.common.present_seq || !current.view_epoch) { return ERR_INVALID_DATA; }
	uint32_t found = 0;
	for (uint32_t i = 0; i < request.resource_count; ++i) {
		auto &resource = request.resources[i];
		if (resource.output_predecessor_view.is_empty() || resource.output_predecessor_action != action.declaration.action_ordinal) { continue; }
		if (!production_output_predecessor_matches_locked(request, resource, rd)) { return ERR_UNAUTHORIZED; }
		RendererRD::ExternalConsumedColorProofSnapshot proof;
		const Error observed = resource.output_predecessor_view.copy_native_color_consumption(proof); if (observed != OK) { return observed; }
		const auto &previous = proof.get_common().current_camera_token;
		if (proof.get_particles().get_id() != current.particles || proof.get_generation() != current.common.generation || proof.get_view_epoch() != current.view_epoch ||
				proof.get_common().present_seq >= current.common.present_seq || previous.view_id != current.common.actual_previous_camera.view_id ||
				previous.view_epoch != current.common.actual_previous_camera.view_epoch || previous.capture_ordinal != current.common.actual_previous_camera.capture_ordinal ||
				memcmp(proof.get_common().view_tag.ptr(), current.common.view_tag.bytes, 32) || memcmp(proof.get_common().output_tag.ptr(), current.common.output_tag.bytes, 32) ||
				memcmp(proof.get_token().bytes, current.common.previous_consumed_tuple_token.bytes, 32)) { return ERR_INVALID_DATA; }
		++found;
	}
	if (!found) { return ERR_UNCONFIGURED; }
	// Commit actual current association only after every retained source agrees.
	for (uint32_t i = 0; i < request.resource_count; ++i) {
		auto &resource = request.resources[i];
		if (!resource.output_predecessor_view.is_empty() && resource.output_predecessor_action == action.declaration.action_ordinal) { resource.output_predecessor_input = current; }
	}
	return OK;
}

Error Registry::production_output_history_borrow_locked(ProductionPhysicalContextRef context, ProductionRequest &request, uint64_t ordinal, RenderingDevice &rd, ProductionOutputHistoryBorrow &out) {
	ProductionRequest *original = nullptr; RenderingDevice *device = nullptr;
	const Error admitted = production_physical_request_locked(context, { instance, request.serial }, original, device, false);
	if (admitted != OK) { return admitted; }
	if (original != &request || device != &rd || !request.frozen || ordinal >= request.action_count || request.source_device_lost || request.effects_pending) { return ERR_UNAUTHORIZED; }
	const auto &action = request.actions[ordinal];
	if (!action.output_source_prepared || !action.preinput_host_prepared || action.preinput_host_returned || !action.output_input.common.has_previous_consumed_tuple) { return ERR_UNCONFIGURED; }
	for (uint32_t i = 0; i < request.resource_count; ++i) {
		const auto &resource = request.resources[i]; const auto &view = resource.output_predecessor_view;
		if (view.is_empty() || resource.output_predecessor_action != ordinal) { continue; }
		if (!production_output_predecessor_matches_locked(request, resource, rd) || !output_input_equal(resource.output_predecessor_input, action.output_input) ||
				!view.owner || view.view_cell >= view.owner->view_capacity) { return ERR_UNAUTHORIZED; }
		const auto &cell = view.owner->view_cells[view.view_cell]; const auto &slot = view.owner->slots[view.slot];
		if (cell.pin.is_empty() || slot.original_action_ordinal >= cell.pin.request->action_count) { return ERR_UNAUTHORIZED; }
		if (!slot.retained_host_snapshot || !production_dto_valid(resource.output_predecessor_host)) { return ERR_UNAVAILABLE; }
		ProductionOutputHistoryBorrow borrowed; borrowed.previous_view = &view; borrowed.current_host = &action.preinput_host_transform;
		borrowed.previous_host = &resource.output_predecessor_host; borrowed.previous_records_witness = &resource.output_predecessor_witness;
		borrowed.consumed_proof = &resource.output_predecessor_ref; borrowed.consumed_tuple = &resource.output_predecessor_token;
		borrowed.previous_no_records = resource.output_predecessor_no_records;
		if (!borrowed.previous_no_records) { borrowed.previous_records = resource.primary; } else { borrowed.previous_records_witness = nullptr; }
		ProductionOutputGuardSource guard;
		if (!production_output_guard_source_locked(*cell.pin.request, slot.original_action_ordinal, rd, guard)) { return ERR_UNAUTHORIZED; }
		borrowed.previous_guard_status = guard.buffer; borrowed.previous_guard_witness = guard.witness;
		borrowed.previous_guard_backing = guard.backing; borrowed.previous_guard_sequence = guard.sequence;
		borrowed.previous_guard_bytes = 32; borrowed.previous_guard_profile = 1;
		out = borrowed; return OK;
	}
	return ERR_UNCONFIGURED;
}

GDExtensionDeviceSessionResultV1 Registry::production_stage_output_history_fallback_v2(GDExtensionProductionRequestRefV1 ref,
		const GDExtensionProductionOutputPlanV1 *fallback, const GDExtensionProductionActionV2 *rows, uint32_t count) {
	const Error contract = production_contract_check(ref, 2); if (contract != OK) { return predecessor_result(contract); }
	if (!fallback || uintptr_t(fallback) % alignof(*fallback) || !production_dto_valid(*fallback) || !production_dto_valid(fallback->acquire) ||
			!rows || uintptr_t(rows) % alignof(*rows) || !count || count > SIZE_MAX / sizeof(*rows) ||
			!production_dto_valid(fallback->acquire.input) || fallback->acquire.input.common.has_previous_consumed_tuple || !fallback->acquire.input.common.history_reset) { return predecessor_result(ERR_INVALID_PARAMETER); }
	Lock lock(*this); ProductionRequest *request = nullptr; RenderingDevice *rd = nullptr;
	const Error resolved = production_request_locked(ref, true, request, rd); if (resolved != OK) { return predecessor_result(resolved); }
	if (request->frozen || request->native_static_sealed || request->next_action || request->graph_count || request->capture_inflight || request->cancelled || request->release_started ||
			count != request->action_count || count > request->capacity.suboperations / 2 || request->native_staged_anchor >= count) { return predecessor_result(ERR_INVALID_DATA); }
	const auto &anchor = request->actions[request->native_staged_anchor]; const auto &primary = anchor.output_plan;
	if (!anchor.output_plan_prepared || !anchor.staged_predecessor_intent || !primary.acquire.input.common.has_previous_consumed_tuple || primary.acquire.input.common.history_reset ||
			fallback->acquire.action_ordinal != primary.acquire.action_ordinal || fallback->finalize_action_ordinal != primary.finalize_action_ordinal ||
			fallback->seal_action_ordinal != primary.seal_action_ordinal || fallback->acquire.canonical_suboperation != primary.acquire.canonical_suboperation ||
			fallback->finalize_canonical_suboperation != primary.finalize_canonical_suboperation || fallback->seal_canonical_suboperation != primary.seal_canonical_suboperation ||
			!production_history_same_output_roles(primary.acquire.roles, fallback->acquire.roles) || fallback->source_space != primary.source_space) { return predecessor_result(ERR_INVALID_DATA); }
    auto fallback_identity = fallback->acquire.input;
    fallback_identity.common.history_reset = primary.acquire.input.common.history_reset;
    fallback_identity.common.has_previous_consumed_tuple = primary.acquire.input.common.has_previous_consumed_tuple;
    fallback_identity.common.previous_consumed_tuple_token = primary.acquire.input.common.previous_consumed_tuple_token;
    if (fallback->acquire.input.common.history_reset != 7 ||
            memcmp(&fallback_identity, &primary.acquire.input, sizeof(fallback_identity)) ||
            fallback->acquire.read_capacity != primary.acquire.read_capacity || fallback->acquire.read_graph_capacity != primary.acquire.read_graph_capacity ||
            fallback->acquire.source_recipe_substep != primary.acquire.source_recipe_substep || fallback->acquire.reserved || fallback->reserved ||
            fallback->finalize_source_recipe_substep != primary.finalize_source_recipe_substep || fallback->seal_source_recipe_substep != primary.seal_source_recipe_substep ||
            fallback->guard_work_limit != primary.guard_work_limit || fallback->guard_dispatch_limit != primary.guard_dispatch_limit ||
            fallback->guard_graph_limit != primary.guard_graph_limit || fallback->guard_native_limit != primary.guard_native_limit ||
            fallback->guard_owned_resource_limit != primary.guard_owned_resource_limit) { return predecessor_result(ERR_INVALID_DATA); }
    for (const auto &read : fallback->reads) {
        if (read.resource_slot >= request->resource_count || read.use_index >= request->use_count ||
                read.use_index < fallback->acquire.first_source_use || read.use_index - fallback->acquire.first_source_use >= fallback->acquire.source_use_count ||
                !read.generation || read.word_offset > UINT32_MAX) { return predecessor_result(ERR_INVALID_DATA); }
        const auto &resource = request->resources[read.resource_slot]; const auto &use = request->uses[read.use_index];
        const uint64_t offset = read.word_offset * 4;
        if (resource.request != request || resource.declaration.original_generation != read.generation || resource.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 ||
                use.request != request || use.declaration.resource_slot != read.resource_slot || use.declaration.access != GDEXTENSION_PRODUCTION_READ_V1 ||
                offset < use.declaration.byte_offset || offset - use.declaration.byte_offset > use.declaration.byte_size || use.declaration.byte_size - (offset - use.declaration.byte_offset) < 4 ||
                offset > resource.declaration.physical_bytes || resource.declaration.physical_bytes - offset < 4) { return predecessor_result(ERR_INVALID_DATA); }
    }
	for (uint32_t a = 0; a < count; ++a) {
		const auto &original = request->actions[a].declaration; const auto &candidate = rows[a];
		if (candidate.action_ordinal != a || candidate.canonical_suboperation != original.canonical_suboperation || candidate.source_recipe_substep != original.source_recipe_substep ||
				candidate.source_recipe_reserved || candidate.action_kind != original.action_kind || candidate.list_boundary != original.list_boundary || candidate.readback_slot != original.readback_slot ||
				candidate.first_use > request->use_count || candidate.use_count > request->use_count - candidate.first_use) { return predecessor_result(ERR_INVALID_DATA); }
		for (uint32_t u = 0; u < candidate.use_count; ++u) {
			const auto &use = request->uses[candidate.first_use + u].declaration;
			if (use.resource_slot >= request->resource_count) { return predecessor_result(ERR_INVALID_DATA); }
			const auto &resource = request->resources[use.resource_slot];
			if (!resource.output_predecessor_view.is_empty() || resource.output_guard_history) { return predecessor_result(ERR_UNAUTHORIZED); }
		}
        if (a > fallback->finalize_action_ordinal && candidate.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1) {
            for (uint32_t u = 0; u < candidate.use_count; ++u) {
                const auto &use = request->uses[candidate.first_use + u].declaration;
                if (use.access != GDEXTENSION_PRODUCTION_WRITE_V1 && use.access != GDEXTENSION_PRODUCTION_READ_WRITE_V1 && use.access != GDEXTENSION_PRODUCTION_ATOMIC_V1) { continue; }
                for (const auto &role : fallback->acquire.roles) {
                    if (role.bytes && role.resource_slot == use.resource_slot) { return predecessor_result(ERR_INVALID_DATA); }
                }
            }
        }
		if (request->native_history_fallback && memcmp(&request->actions[count + a].declaration, &candidate, sizeof(candidate))) { return predecessor_result(ERR_ALREADY_IN_USE); }
	}
	if (request->native_history_fallback) { return predecessor_result(memcmp(&request->native_history_fallback_plan, fallback, sizeof(*fallback)) ? ERR_ALREADY_IN_USE : OK); }
	for (uint32_t a = 0; a < count; ++a) {
		auto &cell = request->actions[count + a]; cell.declaration = rows[a];
		cell.prepared = rows[a].action_kind == GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || rows[a].action_kind == GDEXTENSION_PRODUCTION_READBACK_V1;
	}
	request->native_history_fallback_plan = *fallback; request->native_history_fallback = true;
	return predecessor_result(OK);
}

Error Registry::production_history_fallback_validate_locked(ProductionRequest &request, RenderingDevice &rd) {
	if (!request.native_history_fallback) { return OK; }
	if (!Lock::owns(*this) || request.action_count > request.capacity.suboperations / 2 || request.native_staged_anchor >= request.action_count) { return ERR_INVALID_DATA; }
	for (uint32_t a = 0; a < request.action_count; ++a) {
		const auto &primary = request.actions[a]; const auto &fallback = request.actions[request.action_count + a];
		if (fallback.request != &request || !fallback.prepared) { return ERR_UNCONFIGURED; }
		if (primary.declaration.action_kind == GDEXTENSION_PRODUCTION_DISPATCH_V1) {
			// Same actual dispatch cardinality/launch gives the same graph suffix;
			// both payloads and all distinct resources were retained and charged.
			if (primary.dispatch.groups_x != fallback.dispatch.groups_x || primary.dispatch.groups_y != fallback.dispatch.groups_y || primary.dispatch.groups_z != fallback.dispatch.groups_z) { return ERR_INVALID_DATA; }
			const Error closure = production_dispatch_closure_locked(request, rd, fallback, fallback.dispatch); if (closure != OK) { return closure; }
		} else if (primary.declaration.action_kind == GDEXTENSION_PRODUCTION_UPLOAD_V1) {
            uint32_t primary_graphs = 0, fallback_graphs = 0;
            const Error p = production_staging_graph_bound(primary.upload.bytes, rd.upload_staging_buffers.block_size, primary_graphs);
            const Error f = production_staging_graph_bound(fallback.upload.bytes, rd.upload_staging_buffers.block_size, fallback_graphs);
            if (p != OK || f != OK || primary_graphs != fallback_graphs) { return ERR_INVALID_DATA; }
        }
	}
	const auto &p = request.native_history_fallback_plan; ProductionActionCell check;
	check.declaration = request.actions[request.action_count + p.acquire.action_ordinal].declaration;
	check.output_input = p.acquire.input; check.output_source_prepared = true;
	if (p.acquire.source_resource_slot >= request.resource_count) { return ERR_INVALID_DATA; }
	check.output_source = &request.resources[p.acquire.source_resource_slot]; check.output_source_use = p.acquire.source_use_index;
	check.output_source_generation = p.acquire.source_generation; check.output_first_source_use = p.acquire.first_source_use; check.output_source_use_count = p.acquire.source_use_count;
    if (!production_history_acquire_use_coverage(p.acquire, check.declaration, request.resource_count, request.use_count,
            [&](uint32_t i) -> const auto & { return request.uses[i].declaration; },
            [&](uint32_t i) -> const auto & { return request.resources[i].declaration; })) { return ERR_INVALID_DATA; }
	return production_output_source_matches_locked(request, check, rd) ? OK : ERR_UNAUTHORIZED;
}

GDExtensionDeviceSessionResultV1 Registry::production_stage_output_history_static_v2(GDExtensionProductionRequestRefV1 ref, uint64_t ordinal,
 const GDExtensionProductionOutputHistoryStaticV2 *input, GDExtensionProductionOutputHistoryStateV2 *out) {
 if (!input || !out || uintptr_t(input) % alignof(*input) || uintptr_t(out) % alignof(*out) ||
     !production_dto_valid(*input) || input->kind < 1 || input->kind > 2 || (input->static_mask & ~0x17au) ||
     (input->kind == 1 && !input->static_mask) || (input->kind == 2 && input->static_mask)) { return predecessor_result(ERR_INVALID_PARAMETER); }
 const Error contract = production_contract_check(ref, 2); if (contract != OK) { return predecessor_result(contract); }
 Lock lock(*this); ProductionRequest *request = nullptr; RenderingDevice *rd = nullptr;
 const Error resolved = production_request_locked(ref, false, request, rd); if (resolved != OK) { return predecessor_result(resolved); }
 if (ordinal != request->native_staged_anchor || ordinal >= request->action_count) { return predecessor_result(ERR_INVALID_DATA); }
 if (request->native_history_static_prepared) {
  if (memcmp(input, &request->native_history_static, sizeof(*input))) { return predecessor_result(ERR_ALREADY_IN_USE); }
  if (request->native_history_state.finalized && (request->control.input.kind != ProductionControlKind::RECORD ||
      request->control.state != ProductionControlState::OWNER_CONSUMED || !request->frozen)) { return predecessor_result(ERR_BUSY); }
  *out = request->native_history_state; return predecessor_result(OK);
 }
 if (request->native_static_sealed || request->frozen || request->next_action || request->graph_count || request->cancelled || request->release_started ||
     (input->kind == 2) != request->native_history_fallback) { return predecessor_result(ERR_BUSY); }
 const auto &action = request->actions[ordinal];
 if (!action.output_plan_prepared || (input->kind == 1 && (action.staged_predecessor_intent || action.output_input.common.has_previous_consumed_tuple))) { return predecessor_result(ERR_INVALID_DATA); }
 bool token = false; for (auto byte : input->previous_color_token.bytes) { token |= byte != 0; }
 if (bool(input->static_mask & 2u) == token) { return predecessor_result(ERR_INVALID_DATA); }
 auto *storage = RendererRD::ParticlesStorage::get_singleton();
 auto *particles = storage ? storage->particles_owner.get_or_null(RID::from_uint64(action.output_input.particles)) : nullptr;
 auto *owner = particles ? particles->external : nullptr;
 if (!owner || owner->device != rd || owner->generation_registry != this || owner->generation != action.output_input.common.generation) { return predecessor_result(ERR_UNAUTHORIZED); }
 if (token) {
  bool found = false;
  for (uint32_t v = 0; v < owner->config.max_views; ++v) {
   const auto &view = owner->views[v];
   if (!view.occupied || view.closed || view.tag.size() != 32 ||
       memcmp(view.tag.ptr(), action.output_input.common.view_tag.bytes, 32)) { continue; }
   for (const auto &tuple : view.consumed) {
    if (!tuple.valid || memcmp(tuple.token.bytes, input->previous_color_token.bytes, 32)) { continue; }
    if (found) { return predecessor_result(ERR_INVALID_DATA); }
    request->native_history_previous_camera = tuple.camera;
    request->native_history_previous_present = tuple.present;
    request->native_history_previous_epoch = tuple.epoch; found = true;
   }
  }
  if (!found) { return predecessor_result(ERR_UNAVAILABLE); }
 }
 const uint64_t uploads[] = { input->reset_upload_action, input->fallback_reset_upload_action };
 for (uint32_t bank = 0; bank < (input->kind == 2 ? 2u : 1u); ++bank) {
  const auto index = uploads[bank];
  if (index >= ordinal || index >= request->action_count) { return predecessor_result(ERR_INVALID_DATA); }
  const auto &upload = request->actions[bank * request->action_count + index];
  if (upload.request != request || upload.declaration.action_kind != GDEXTENSION_PRODUCTION_UPLOAD_V1 || !upload.prepared ||
      (upload.upload.bytes != 4 && upload.upload.bytes != 12) || upload.payload_bytes != upload.upload.bytes || upload.recorded_bytes ||
      upload.payload_offset < request->payload_offset || upload.payload_offset - request->payload_offset > request->capacity.retained_payload_bytes ||
      upload.payload_bytes > request->capacity.retained_payload_bytes - (upload.payload_offset - request->payload_offset)) { return predecessor_result(ERR_INVALID_DATA); }
 }
 if (input->kind == 1 && input->fallback_reset_upload_action != UINT64_MAX) { return predecessor_result(ERR_INVALID_DATA); }
 request->native_history_static = *input; request->native_history_static_prepared = true; request->native_history_has_tuple = token;
 request->native_history_state = { production_dto_abi<GDExtensionProductionOutputHistoryStateV2>(), 0, input->static_mask, 0, 0 };
 *out = request->native_history_state; return predecessor_result(OK);
}

Error Registry::production_history_choose_locked(ProductionRequest &request, ProductionActionCell &action, RenderingDevice &rd, GDExtensionOutputAcquireInputV1 &actual) {
	if (!request.native_history_static_prepared) { return request.native_history_fallback ? ERR_UNCONFIGURED : OK; }
	if (request.native_history_selected || request.next_action || request.graph_count) { return ERR_UNAUTHORIZED; }
	const uint32_t mask = production_history_final_mask(request.native_history_static.static_mask, request.native_history_has_tuple,
	 request.native_history_previous_epoch, request.native_history_previous_present, request.native_history_previous_camera, actual);
	const uint32_t reset = production_history_reset_reason(mask);
	const bool dynamic = request.native_history_static.kind == 2;
	if (!dynamic) {
	 actual.common.history_reset = reset;
	 const auto ordinal = request.native_history_static.reset_upload_action;
	 memcpy(request.attachment->payload + request.actions[ordinal].payload_offset, &reset, 4);
	 request.native_history_state = { production_dto_abi<GDExtensionProductionOutputHistoryStateV2>(), 1, mask, reset, 0 };
	 request.native_history_selected = true; return OK;
	}
	if (!action.staged_predecessor_intent) { return ERR_UNAUTHORIZED; }
	bool found = false, matches = actual.common.has_actual_previous_camera;
	for (uint32_t r = 0; r < request.resource_count; ++r) {
		const auto &source = request.resources[r]; const auto &view = source.output_predecessor_view;
		if (view.is_empty() || source.output_predecessor_action != action.declaration.action_ordinal) { continue; }
		if (!production_output_predecessor_matches_locked(request, source, rd)) { return ERR_UNAUTHORIZED; }
		RendererRD::ExternalConsumedColorProofSnapshot proof; const Error copied = view.copy_native_color_consumption(proof); if (copied != OK) { return copied; }
		const auto &previous = proof.get_common().current_camera_token;
		matches &= proof.get_view_epoch() == actual.view_epoch && proof.get_common().present_seq < actual.common.present_seq &&
				previous.view_id == actual.common.actual_previous_camera.view_id && previous.view_epoch == actual.common.actual_previous_camera.view_epoch && previous.capture_ordinal == actual.common.actual_previous_camera.capture_ordinal;
		found = true;
	}
	if (!found) { return ERR_UNCONFIGURED; }
	if (matches != (mask == 0)) { return ERR_INVALID_DATA; }
	if (mask) {
		const auto &fallback = request.native_history_fallback_plan;
		// Install only immutable preparation values. The stable action objects,
		// camera subscriber, request, nonce and ordinal/fact clocks never move.
		for (uint32_t a = 0; a < request.action_count; ++a) {
			auto &target = request.actions[a]; const auto &prepared = request.actions[request.action_count + a];
			target.declaration = prepared.declaration; target.dispatch = prepared.dispatch; target.upload = prepared.upload;
			target.payload_offset = prepared.payload_offset; target.payload_bytes = prepared.payload_bytes;
		}
		action.output_plan = fallback; action.output_source = &request.resources[fallback.acquire.source_resource_slot];
        for (uint32_t i = 0; i < 3; ++i) {
            const auto &role = fallback.acquire.roles[i];
            action.output_roles[i] = { role.role, role.resource_slot, role.use_index, role.allocation_generation, role.bytes };
        }
		action.output_source_use = fallback.acquire.source_use_index; action.output_source_generation = fallback.acquire.source_generation;
		action.output_first_source_use = fallback.acquire.first_source_use; action.output_source_use_count = fallback.acquire.source_use_count;
		const auto camera = actual.common.current_camera; const auto previous = actual.common.actual_previous_camera;
		const auto present = actual.common.present_seq; const auto epoch = actual.view_epoch; const auto has_previous = actual.common.has_actual_previous_camera;
		actual = fallback.acquire.input; actual.common.current_camera = camera; actual.common.actual_previous_camera = previous;
		actual.common.present_seq = present; actual.view_epoch = epoch; actual.common.has_actual_previous_camera = has_previous; actual.common.history_reset = reset;
		action.staged_predecessor_intent = false; request.native_history_used_fallback = true;
	}
	const auto reset_ordinal = mask ? request.native_history_static.fallback_reset_upload_action : request.native_history_static.reset_upload_action;
	memcpy(request.attachment->payload + request.actions[reset_ordinal].payload_offset, &reset, 4);
	request.native_history_state = { production_dto_abi<GDExtensionProductionOutputHistoryStateV2>(), 1, mask, reset, 0 };
	request.native_history_selected = true; return OK;
}

GDExtensionDeviceSessionResultV1 Registry::production_read_staged_output_input_v2(GDExtensionProductionRequestRefV1 ref, uint64_t ordinal, GDExtensionProductionOutputPrepareV1 *out) {
	if (!out || uintptr_t(out) % alignof(GDExtensionProductionOutputPrepareV1)) { return predecessor_result(ERR_INVALID_PARAMETER); }
	const Error contract = production_contract_check(ref, 2);
	if (contract != OK) { return predecessor_result(contract); }
	Lock lock(*this);
	ProductionRequest *request = nullptr; RenderingDevice *rd = nullptr;
	const Error resolved = production_request_locked(ref, false, request, rd);
	if (resolved != OK) { return predecessor_result(resolved); }
	if (!request->native_static_sealed || !request->native_prepared_executor || ordinal != request->native_staged_anchor || ordinal >= request->action_count) { return predecessor_result(ERR_INVALID_DATA); }
	const auto &action = request->actions[ordinal];
	if (request->control.input.kind != ProductionControlKind::RECORD || request->control.state != ProductionControlState::OWNER_CONSUMED ||
			!request->frozen || action.output_camera_pending || action.preinput_camera_source.is_null() || !action.output_plan_prepared) { return predecessor_result(ERR_BUSY); }
	*out = action.output_plan.acquire;
	return predecessor_result(OK);
}

GDExtensionDeviceSessionResultV1 Registry::production_stage_camera_payload_v2(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionCameraPayloadV1 *input) {
	const Error contract = production_contract_check(ref, 2);
	if (contract != OK) { return predecessor_result(contract); }
	if (!input || uintptr_t(input) % alignof(GDExtensionProductionCameraPayloadV1) || !production_dto_valid(*input) || input->reserved || input->needs_inverse > 1) { return predecessor_result(ERR_INVALID_PARAMETER); }
	Lock lock(*this);
	ProductionRequest *request = nullptr; RenderingDevice *rd = nullptr;
	const Error resolved = production_request_locked(ref, true, request, rd);
	if (resolved != OK) { return predecessor_result(resolved); }
	if (request->native_staged_anchor != input->acquire_action_ordinal || input->acquire_action_ordinal >= request->action_count ||
			!request->actions[input->acquire_action_ordinal].output_camera_pending) { return predecessor_result(ERR_INVALID_DATA); }
	const uint64_t ordinals[] = { input->view_upload_action_ordinal, input->sort_upload_action_ordinal, input->control_upload_action_ordinal };
	const uint64_t bytes[] = { 96, 72, 12 };
	bool any = false;
	for (uint32_t i = 0; i < 3; ++i) {
		if (ordinals[i] == UINT64_MAX) { continue; }
		any = true;
		if (ordinals[i] >= input->acquire_action_ordinal) { return predecessor_result(ERR_INVALID_DATA); }
		const auto &action = request->actions[ordinals[i]];
		if (action.request != request || action.declaration.action_ordinal != ordinals[i] || action.declaration.action_kind != GDEXTENSION_PRODUCTION_UPLOAD_V1 ||
				!action.prepared || action.upload.bytes != bytes[i] || action.recorded_bytes ||
				(action.camera_payload_kind && action.camera_payload_kind != i + 1) || action.payload_offset < request->payload_offset ||
				action.payload_offset - request->payload_offset > request->capacity.retained_payload_bytes ||
				bytes[i] > request->capacity.retained_payload_bytes - (action.payload_offset - request->payload_offset)) { return predecessor_result(ERR_INVALID_DATA); }
		if (action.upload.destination_slot >= request->resource_count || action.declaration.first_use >= request->use_count ||
				action.declaration.use_count > request->use_count - action.declaration.first_use ||
				action.upload.destination_offset > UINT64_MAX - bytes[i]) { return predecessor_result(ERR_INVALID_DATA); }
		const auto &resource = request->resources[action.upload.destination_slot];
		if (resource.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || !resource.imported || resource.retiring ||
				action.upload.destination_offset + bytes[i] > resource.declaration.physical_bytes) { return predecessor_result(ERR_INVALID_DATA); }
		bool writable = false, consumed = false;
		for (uint32_t u = 0; u < action.declaration.use_count; ++u) {
			const auto &use = request->uses[action.declaration.first_use + u].declaration;
			writable |= use.resource_slot == action.upload.destination_slot && use.access == GDEXTENSION_PRODUCTION_WRITE_V1 &&
					use.byte_offset == action.upload.destination_offset && use.byte_size == bytes[i];
		}
		const auto finalizer = request->actions[input->acquire_action_ordinal].output_plan.finalize_action_ordinal;
		for (uint64_t a = input->acquire_action_ordinal + 1; a < finalizer && a < request->action_count; ++a) {
			const auto &row = request->actions[a].declaration;
			if (row.action_kind != GDEXTENSION_PRODUCTION_DISPATCH_V1 || row.first_use > request->use_count || row.use_count > request->use_count - row.first_use) { continue; }
			for (uint32_t u = 0; u < row.use_count; ++u) {
				const auto &use = request->uses[row.first_use + u].declaration;
				consumed |= use.resource_slot == action.upload.destination_slot && use.access == GDEXTENSION_PRODUCTION_READ_V1 &&
						use.byte_offset <= action.upload.destination_offset && action.upload.destination_offset - use.byte_offset <= use.byte_size &&
						bytes[i] <= use.byte_size - (action.upload.destination_offset - use.byte_offset);
			}
		}
		if (!writable || !consumed) { return predecessor_result(ERR_INVALID_DATA); }
		for (uint32_t j = 0; j < i; ++j) { if (ordinals[j] == ordinals[i]) { return predecessor_result(ERR_INVALID_DATA); } }
	}
	if (!any) { return predecessor_result(ERR_INVALID_PARAMETER); }
	for (uint32_t i = 0; i < 3; ++i) {
		if (request->native_camera_uploads[i] != UINT64_MAX && request->native_camera_uploads[i] != ordinals[i]) { return predecessor_result(ERR_ALREADY_IN_USE); }
	}
	if ((request->native_camera_uploads[0] != UINT64_MAX || request->native_camera_uploads[1] != UINT64_MAX || request->native_camera_uploads[2] != UINT64_MAX) &&
			request->native_camera_needs_inverse != input->needs_inverse) { return predecessor_result(ERR_ALREADY_IN_USE); }
	for (uint32_t i = 0; i < 3; ++i) {
		request->native_camera_uploads[i] = ordinals[i];
		if (ordinals[i] != UINT64_MAX) { request->actions[ordinals[i]].camera_payload_kind = i + 1; }
	}
	request->native_camera_needs_inverse = input->needs_inverse;
	return predecessor_result(OK);
}

GDExtensionDeviceSessionResultV1 Registry::production_stage_output_plan_v2(GDExtensionProductionRequestRefV1 ref,
		const GDExtensionProductionOutputPlanV1 *input, const GDExtensionProductionServiceBudgetV1 *budget) {
	const Error contract = production_contract_check(ref, 2);
	if (contract != OK) { return predecessor_result(contract); }
	if (!input || uintptr_t(input) % alignof(GDExtensionProductionOutputPlanV1) || !budget || uintptr_t(budget) % alignof(GDExtensionProductionServiceBudgetV1) ||
			!production_dto_valid(*budget) || budget->reserved || !budget->controls || !budget->passes || !budget->receipts ||
			!budget->deadline_micros || OS::get_singleton()->get_ticks_usec() >= budget->deadline_micros) { return predecessor_result(ERR_INVALID_PARAMETER); }
	Lock lock(*this, Lock::BLOCK_OR_BORROW);
	ProductionRequest *request = nullptr; RenderingDevice *rd = nullptr;
	const Error resolved = production_request_locked(ref, true, request, rd);
	if (resolved != OK) { return predecessor_result(resolved); }
	if (request->native_staged_anchor != UINT64_MAX && (request->native_staged_anchor != input->acquire.action_ordinal ||
			memcmp(&request->native_stage_budget, budget, sizeof(*budget)))) { return predecessor_result(ERR_ALREADY_IN_USE); }
	const auto prepared = production_prepare_output_plan_internal(ref, input, true);
	if (prepared.native_error == OK) {
		request->native_staged_anchor = input->acquire.action_ordinal;
		request->native_stage_budget = *budget;
	}
	return prepared;
}

GDExtensionDeviceSessionResultV1 Registry::production_prepare_output(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOutputPrepareV1 *input) {
	return production_prepare_output_internal(ref, input, false);
}

GDExtensionDeviceSessionResultV1 Registry::production_prepare_output_internal(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOutputPrepareV1 *input, bool staged) {
	auto result = [](Error error) -> GDExtensionDeviceSessionResultV1 { return { int32_t(error), error == OK ? 0u : 16u, 0, 0 }; };
	if (!input || uintptr_t(input) % alignof(GDExtensionProductionOutputPrepareV1) || !production_dto_valid(*input) || !production_dto_valid(input->input) || input->reserved || input->input.reserved || !input->source_use_count || !input->source_generation || ((!input->read_capacity) != (!input->read_graph_capacity))) {
		return result(ERR_INVALID_PARAMETER);
	}
	RendererRD::ExternalOutputRequest decoded;
	if (output_decode(input->input, decoded, staged) != OK) {
		return result(ERR_INVALID_PARAMETER);
	}
	Lock lock(*this, Lock::BLOCK_OR_BORROW);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_request_locked(ref, true, request, rd);
	if (error != OK) {
		return result(error);
	}
	ProductionActionCell *action = nullptr;
	if (request->contract_version != 2 || (error = production_action_locked(*request, input->action_ordinal, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, action)) != OK) {
		return result(error == OK ? ERR_INVALID_DATA : error);
	}
	if (action->declaration.canonical_suboperation != input->canonical_suboperation || action->declaration.source_recipe_substep != input->source_recipe_substep || input->source_resource_slot >= request->resource_count || input->source_use_index >= request->use_count || input->source_use_index < action->declaration.first_use || input->source_use_index - action->declaration.first_use >= action->declaration.use_count || (input->read_capacity && input->read_capacity > request->capacity.graph_memberships / input->read_graph_capacity)) {
		return result(ERR_INVALID_DATA);
	}
	if (staged && (!action->host_view_prepared ||
			action->camera_ready_subscriber.closed || action->camera_ready_subscriber.released || !action->preinput_host_prepared ||
			action->preinput_host_returned || action->preinput_camera_returned || action->preinput_camera_source.is_valid())) {
		return result(ERR_UNCONFIGURED);
	}
	if (action->preinput_host_prepared && memcmp(action->preinput_host_transform.host_token.bytes, input->input.common.current_host_token.bytes, 32) != 0) {
		return result(ERR_INVALID_DATA);
	}
	if (action->preinput_camera_source.is_valid()) {
		// A source-first ready delivery already fixed this action's camera.
		// Preparation cannot replace it, even if capture is never called again.
		const auto &token = action->preinput_camera_source->get_token();
		const auto &key = action->preinput_camera_source->get_presentation_key();
		const auto &frozen = input->input.common.current_camera;
		if (token.view_id != frozen.view_id || token.view_epoch != frozen.view_epoch || token.capture_ordinal != frozen.capture_ordinal || key.view_id != token.view_id || key.view_epoch != input->input.view_epoch || key.present_seq != input->input.common.present_seq) {
			return result(ERR_INVALID_DATA);
		}
	}
	auto &source = request->resources[input->source_resource_slot];
	const auto &source_use = request->uses[input->source_use_index];
	if (source.request != request || source.request_serial != request->serial || source.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || source.declaration.original_generation != input->source_generation || !source.imported || source.retiring || source.use_count != 1 || !source.uses[0].same_physical() || !source.uses[0].linked() || source.uses[0].physical->use_closed.load() || source_use.request != request || source_use.declaration.resource_slot != input->source_resource_slot || source_use.declaration.access != GDEXTENSION_PRODUCTION_READ_V1 || !source_use.declaration.byte_size) {
		return result(ERR_INVALID_DATA);
	}
	ProductionActionCell source_check;
	source_check.declaration = action->declaration;
	source_check.output_input = input->input;
	source_check.output_source_prepared = true;
	source_check.output_source = &source;
	source_check.output_source_use = input->source_use_index;
	source_check.output_first_source_use = input->first_source_use;
	source_check.output_source_use_count = input->source_use_count;
	source_check.output_source_generation = input->source_generation;
	if (!rd->_thread_safe_.try_lock()) {
		return result(ERR_BUSY);
	}
	const bool source_matches = production_output_source_matches_locked(*request, source_check, *rd);
	rd->_thread_safe_.unlock();
	if (!source_matches) {
		return result(ERR_INVALID_DATA);
	}

	if (staged && input->input.common.has_previous_consumed_tuple) {
		bool retained = false;
		for (uint32_t i = 0; i < request->resource_count; ++i) {
			const auto &previous = request->resources[i];
			retained |= !previous.output_predecessor_view.is_empty() && previous.output_predecessor_action == input->action_ordinal &&
					output_input_equal(previous.output_predecessor_input, input->input);
		}
		if (!action->staged_predecessor_intent || !retained) { return result(ERR_UNCONFIGURED); }
	}
	const uint32_t roles[3] = { 1, 2, 7 };
	for (uint32_t i = 0; i < 3; ++i) {
		const auto &role = input->roles[i];
		if (role.role != roles[i] || role.reserved || role.bytes > UINT32_MAX) {
			return result(ERR_INVALID_DATA);
		}
		if (!role.bytes) {
			if (role.resource_slot != UINT32_MAX || role.use_index != UINT32_MAX || role.allocation_generation) {
				return result(ERR_INVALID_DATA);
			}
			continue;
		}
		if (role.resource_slot >= request->resource_count || role.use_index >= request->use_count || role.use_index < action->declaration.first_use || role.use_index - action->declaration.first_use >= action->declaration.use_count || !role.allocation_generation) {
			return result(ERR_INVALID_DATA);
		}
		const auto &resource = request->resources[role.resource_slot];
		const auto &use = request->uses[role.use_index];
		if (resource.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || resource.declaration.ownership != GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1 || resource.declaration.original_generation != role.allocation_generation || resource.declaration.physical_bytes != role.bytes || resource.imported || resource.owned_serial || resource.use_count || resource.output_guard_readonly || use.request != request || use.declaration.resource_slot != role.resource_slot || use.declaration.byte_offset || use.declaration.byte_size != role.bytes || use.declaration.access != GDEXTENSION_PRODUCTION_WRITE_V1 || (resource.output_alias_prepared && resource.output_action_ordinal != input->action_ordinal)) {
			return result(ERR_INVALID_DATA);
		}
		for (uint32_t prior = 0; prior < i; ++prior) {
			if (input->roles[prior].bytes && input->roles[prior].resource_slot == role.resource_slot) {
				return result(ERR_INVALID_DATA);
			}
		}
	}
	for (uint32_t i = 0; i < action->declaration.use_count; ++i) {
		const uint32_t index = action->declaration.first_use + i;
		const auto &use = request->uses[index].declaration;
		if (request->resources[use.resource_slot].declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1) {
			return result(ERR_INVALID_DATA);
		}
		if (index >= input->first_source_use && index - input->first_source_use < input->source_use_count) {
			continue;
		}
		bool output_role = false;
		for (const auto &role : input->roles) {
			output_role |= role.bytes && role.use_index == index && role.resource_slot == use.resource_slot && use.access == GDEXTENSION_PRODUCTION_WRITE_V1;
		}
		if (!output_role) {
			return result(ERR_INVALID_DATA);
		}
	}
	if (action->prepared) {
		bool same = action->output_camera_pending == staged && action->output_source == &source && action->output_source_use == input->source_use_index && action->output_first_source_use == input->first_source_use && action->output_source_use_count == input->source_use_count && action->output_source_generation == input->source_generation && action->output_read_capacity == input->read_capacity && action->output_read_graph_capacity == input->read_graph_capacity && output_input_equal(action->output_input, input->input);
		for (uint32_t i = 0; i < 3; ++i) {
			const auto &a = action->output_roles[i];
			const auto &b = input->roles[i];
			same &= a.role == b.role && a.resource_slot == b.resource_slot && a.use_index == b.use_index && a.generation == b.allocation_generation && a.bytes == b.bytes;
		}
		return result(same ? OK : ERR_INVALID_DATA);
	}
	action->output_input = input->input;
	action->output_source = &source;
	action->output_source_use = input->source_use_index;
	action->output_first_source_use = input->first_source_use;
	action->output_source_use_count = input->source_use_count;
	action->output_source_generation = input->source_generation;
	action->output_read_capacity = input->read_capacity;
	action->output_read_graph_capacity = input->read_graph_capacity;
	for (uint32_t i = 0; i < 3; ++i) {
		const auto &role = input->roles[i];
		action->output_roles[i] = { role.role, role.resource_slot, role.use_index, role.allocation_generation, role.bytes };
		if (role.bytes) {
			auto &resource = request->resources[role.resource_slot];
			resource.output_alias_prepared = true;
			resource.output_action_ordinal = input->action_ordinal;
		}
	}
	// This is the original accepted physical source projection only. The
	// installed caller retains Pending/finalized selection and account owners.
	action->output_source_prepared = true;
	action->output_camera_pending = staged;
	action->prepared = true;
	return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::production_prepare_output_plan(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOutputPlanV1 *input) {
	return production_prepare_output_plan_internal(ref, input, false);
}

GDExtensionDeviceSessionResultV1 Registry::production_prepare_output_plan_internal(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOutputPlanV1 *input, bool staged) {
	auto result = [](Error error) -> GDExtensionDeviceSessionResultV1 { return { int32_t(error), error == OK ? 0u : 16u, 0, 0 }; };
	if (!input || uintptr_t(input) % alignof(GDExtensionProductionOutputPlanV1) || !production_dto_valid(*input) || input->reserved || (input->source_space != 1 && input->source_space != 2) || input->acquire.action_ordinal >= input->finalize_action_ordinal || input->finalize_action_ordinal >= input->seal_action_ordinal || !input->guard_dispatch_limit || !input->guard_graph_limit || !input->guard_native_limit || !input->guard_owned_resource_limit) {
		return result(ERR_INVALID_PARAMETER);
	}
	Lock lock(*this, Lock::BLOCK_OR_BORROW);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_request_locked(ref, true, request, rd);
	if (error != OK) {
		return result(error);
	}
	if (request->frozen || input->seal_action_ordinal >= request->action_count || input->guard_graph_limit > request->capacity.graph_memberships || input->guard_native_limit > request->capacity.native_memberships || input->guard_owned_resource_limit >= request->capacity.resource_uses) {
		return result(ERR_INVALID_DATA);
	}
	auto &acquire = request->actions[input->acquire.action_ordinal];
	auto &finalizer = request->actions[input->finalize_action_ordinal];
	auto &seal = request->actions[input->seal_action_ordinal];
	if (acquire.request != request || finalizer.request != request || seal.request != request || acquire.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || finalizer.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || seal.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || finalizer.declaration.action_ordinal != input->finalize_action_ordinal || seal.declaration.action_ordinal != input->seal_action_ordinal || finalizer.declaration.canonical_suboperation != input->finalize_canonical_suboperation || finalizer.declaration.source_recipe_substep != input->finalize_source_recipe_substep || seal.declaration.canonical_suboperation != input->seal_canonical_suboperation || seal.declaration.source_recipe_substep != input->seal_source_recipe_substep || (finalizer.output_acquire_action_ordinal != UINT64_MAX && finalizer.output_acquire_action_ordinal != input->acquire.action_ordinal) || (seal.output_acquire_action_ordinal != UINT64_MAX && seal.output_acquire_action_ordinal != input->acquire.action_ordinal)) {
		return result(ERR_INVALID_DATA);
	}
	if (!acquire.output_scene_prepared || acquire.output_scene.particles != input->acquire.input.particles || acquire.output_scene.generation != input->acquire.input.common.generation) { return result(ERR_UNCONFIGURED); }
	// Guard authenticates the last writer of these same original aliases.
	// Keep unrelated work/read-only rows legal, but no later ordinary writer
	// (including a post-Seal suffix) may invalidate that frozen validation.
	for (uint32_t a = uint32_t(input->finalize_action_ordinal) + 1; a < request->action_count; ++a) {
		const auto &later = request->actions[a].declaration;
		if (later.action_kind == GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1) {
			continue;
		}
		if (later.first_use > request->use_count || later.use_count > request->use_count - later.first_use) {
			return result(ERR_INVALID_DATA);
		}
		for (uint32_t u = 0; u < later.use_count; ++u) {
			const auto &use = request->uses[later.first_use + u].declaration;
			if (use.access != GDEXTENSION_PRODUCTION_WRITE_V1 && use.access != GDEXTENSION_PRODUCTION_READ_WRITE_V1 && use.access != GDEXTENSION_PRODUCTION_ATOMIC_V1) {
				continue;
			}
			for (const auto &role : input->acquire.roles) {
				if (role.bytes && use.resource_slot == role.resource_slot) {
					return result(ERR_INVALID_DATA);
				}
			}
		}
	}
	for (const auto &read : input->reads) {
		if (read.resource_slot >= request->resource_count || read.use_index >= request->use_count || read.use_index < input->acquire.first_source_use || read.use_index - input->acquire.first_source_use >= input->acquire.source_use_count || !read.generation || read.word_offset > UINT32_MAX) {
			return result(ERR_INVALID_DATA);
		}
		const auto &resource = request->resources[read.resource_slot];
		const auto &use = request->uses[read.use_index];
		const uint64_t offset = uint64_t(read.word_offset) * 4;
		if (resource.request != request || resource.declaration.original_generation != read.generation || resource.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || use.request != request || use.declaration.resource_slot != read.resource_slot || use.declaration.access != GDEXTENSION_PRODUCTION_READ_V1 || offset < use.declaration.byte_offset || offset - use.declaration.byte_offset > use.declaration.byte_size || use.declaration.byte_size - (offset - use.declaration.byte_offset) < 4 || offset > resource.declaration.physical_bytes || resource.declaration.physical_bytes - offset < 4) {
			return result(ERR_INVALID_DATA);
		}
	}
	if (acquire.output_plan_prepared) {
		// Frozen successor rows cannot be retargeted independently of Acquire.
		return result(acquire.output_camera_pending == staged && memcmp(&acquire.output_plan, input, sizeof(*input)) == 0 ? OK : ERR_INVALID_DATA);
	}
	const uint64_t serial = request->serial;
	// Borrow the original metadata lock: no arena lifetime gap between the
	// existing Acquire preparation and its two frozen successor rows.
	const auto prepared = production_prepare_output_internal(ref, &input->acquire, staged);
	if (prepared.native_error != OK) {
		return prepared;
	}
	if (request->serial != serial || request->frozen || !acquire.prepared || acquire.output_plan_prepared || finalizer.prepared || seal.prepared) {
		return result(ERR_BUSY);
	}
	acquire.output_plan = *input;
	acquire.output_plan_prepared = true;
	acquire.output_acquire_action_ordinal = input->acquire.action_ordinal;
	finalizer.output_acquire_action_ordinal = seal.output_acquire_action_ordinal = input->acquire.action_ordinal;
	finalizer.prepared = seal.prepared = true;
	return result(OK);
}

void Registry::production_output_acquire(ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, uint64_t ordinal, const GDExtensionOutputAcquireInputV1 *input, GDExtensionOutputAcquireResultV1 *out) {
	if (!out || !input || uintptr_t(out) % alignof(GDExtensionOutputAcquireResultV1) || uintptr_t(input) % alignof(GDExtensionOutputAcquireInputV1)) {
		return;
	}
	*out = {};
	out->abi = production_dto_abi<GDExtensionOutputAcquireResultV1>();
	out->action_ordinal = UINT64_MAX;
	out->status = output_status(ERR_INVALID_PARAMETER, "output_input");
	for (uint32_t i = 0; i < 3; ++i) {
		out->resource_slots[i] = out->use_indices[i] = UINT32_MAX;
	}
	RendererRD::ExternalOutputRequest parsed;
	if (output_decode(*input, parsed) != OK) {
		return;
	}
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_physical_request_locked(context, ref, request, rd, true);
	if (error != OK) {
		out->status = output_status(error, "original_record_context");
		return;
	}
	if (!request->frozen || ordinal >= request->action_count || request->capture_inflight || request->cancelled || request->release_started || request->control.state != ProductionControlState::EXECUTING || request->control.input.kind != ProductionControlKind::RECORD) {
		out->status = output_status(ERR_UNAUTHORIZED, "original_output_action");
		return;
	}
	auto &action = request->actions[ordinal];
	if (action.request != request || action.declaration.action_ordinal != ordinal || action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || !action.prepared || !output_input_equal(action.output_input, *input)) {
		out->status = output_status(ERR_INVALID_DATA, "output_action_mismatch");
		return;
	}
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	auto *particles = storage ? storage->particles_owner.get_or_null(RID::from_uint64(input->particles)) : nullptr;
	if (!particles || !particles->external || particles->external->device != rd || particles->external->generation != input->common.generation || particles->external->generation_registry != this) {
		out->status = output_status(ERR_UNAUTHORIZED, "original_output_owner");
		return;
	}
	auto &owner = *particles->external;
	using Owner = RendererRD::ExternalOutputOwner;
	using Storage = RendererRD::ExternalOutputStorage;
	auto project = [&](bool writable = true) {
		out->request_serial = request->serial;
		out->action_ordinal = ordinal;
		if (action.output_slot >= owner.slot_count) {
			return;
		}
		const auto &slot = owner.slots[action.output_slot];
		out->state = uint32_t(slot.state) + 1;
		out->custody = action.output_state == ProductionActionCell::OutputState::UNKNOWN ? GDEXTENSION_OUTPUT_CUSTODY_UNKNOWN_V1 : action.output_state == ProductionActionCell::OutputState::RETIRING ? GDEXTENSION_OUTPUT_CUSTODY_RETAINED_PREFIX_V1
				: action.output_state == ProductionActionCell::OutputState::RETURNED																												  ? GDEXTENSION_OUTPUT_NO_CUSTODY_V1
																																																	  : GDEXTENSION_OUTPUT_CUSTODY_ACCEPTED_V1;
		if (slot.lease_token.size() == 32) {
			memcpy(out->lease_token.bytes, slot.lease_token.ptr(), 32);
		}
		if (!writable || action.output_state != ProductionActionCell::OutputState::BOUND) {
			return;
		}
		for (uint32_t i = 0; i < 3; ++i) {
			const auto &role = action.output_roles[i];
			if (!role.bytes || role.resource_slot >= request->resource_count) {
				continue;
			}
			const auto &resource = request->resources[role.resource_slot];
			const uint32_t index = out->writable_count++;
			out->writables[index] = { role.role, GDEXTENSION_OUTPUT_ACCESS_WRITE_ONLY_V1, resource.primary.get_id(), role.generation, uint32_t(role.bytes), i < 2 ? 192u : 4u };
			out->resource_slots[index] = role.resource_slot;
			out->use_indices[index] = role.use_index;
		}
	};
	if (action.output_state != ProductionActionCell::OutputState::UNBOUND) {
		if (action.output_particles.get_id() != input->particles || action.output_generation != owner.generation || action.output_slot >= owner.slot_count || owner.slots[action.output_slot].original_action_ordinal != ordinal || owner.slots[action.output_slot].original_request.handle_serial != request->serial || owner.slots[action.output_slot].lease_serial != action.output_lease) {
			out->status = output_status(ERR_INVALID_DATA, "output_duplicate_mismatch");
			return;
		}
		const auto &slot = owner.slots[action.output_slot];
		if (slot.state == Owner::LeaseState::RETIRING && action.output_state != ProductionActionCell::OutputState::UNKNOWN) {
			action.output_state = ProductionActionCell::OutputState::RETIRING;
		}
		if (action.output_state == ProductionActionCell::OutputState::BOUND) {
			if (!owner.admits_work() || slot.publication_closed || slot.pass_uses_closed || (slot.state != Owner::LeaseState::RESERVED && slot.state != Owner::LeaseState::WRITING) || !production_output_pin_matches_locked(slot.original_pin, ref, *rd) || request->effects_pending || request->source_device_lost || !rd->_thread_safe_.try_lock()) {
				project(false);
				out->status = output_status(ERR_BUSY, "output_duplicate_not_writable");
				return;
			}
			bool valid = production_output_source_matches_locked(*request, action, *rd);
			for (const auto &role : action.output_roles) {
				if (role.bytes) {
					valid &= production_output_alias_matches_locked(*request, request->resources[role.resource_slot], *rd);
				}
			}
			rd->_thread_safe_.unlock();
			if (!valid) {
				project(false);
				out->status = output_status(ERR_INVALID_DATA, "output_duplicate_backing");
				out->custody = GDEXTENSION_OUTPUT_CUSTODY_RETAINED_PREFIX_V1;
				return;
			}
		}
		project();
		out->status = output_status(action.output_state == ProductionActionCell::OutputState::BOUND ? OK : ERR_BUSY, "output_existing_custody");
		return;
	}
	error = production_output_scene_validate_locked(action, *input);
	if (error != OK) {
		out->status = output_status(error, "output_original_committed_scene");
		return;
	}
	if (request->next_action != ordinal || owner.generation_pin.is_empty() || owner.generation_pin.request->attachment != request->attachment || !owner.metadata_directory_installed || owner.metadata_lease.is_empty() || owner.config.profile == RendererRD::ExternalOutputConfig::INDEXED_144) {
		out->status = output_status(ERR_UNAVAILABLE, "output_prepared_owner_required");
		return;
	}
	if (!rd->_thread_safe_.try_lock()) {
		out->status = output_status(ERR_BUSY, "output_device_busy");
		return;
	}
	const bool source_matches = production_output_source_matches_locked(*request, action, *rd);
	const bool lists_closed = !rd->draw_list.active && !rd->compute_list.active && !rd->raytracing_list.active && !request->compute_list_open && !request->draw_list_open;
	rd->_thread_safe_.unlock();
	if (!source_matches || !lists_closed || request->effects_pending || request->source_device_lost) {
		out->status = output_status(ERR_UNAUTHORIZED, "output_original_source");
		return;
	}
	uint32_t view = UINT32_MAX, slot_index = UINT32_MAX;
	error = owner.select_slot(parsed, view, slot_index);
	if (error != OK) {
		out->status = output_status(error, "output_slot_preflight");
		return;
	}
	if (owner.generation_config_inline && !owner.generation_selection_complete) {
		out->status = output_status(ERR_UNAVAILABLE, "native_generation_selection_required");
		return;
	}
	const uint32_t surfaces = owner.generation_config_inline ? owner.generation_material_count : uint32_t(owner.config.surface_materials.size());
	Storage::Instance192Layout layout;
	if (owner.config.profile == RendererRD::ExternalOutputConfig::INSTANCE_192) {
		error = Storage::plan_instance192(owner.config.capacity, surfaces, layout);
		if (error != OK || !surfaces || uint64_t(owner.slot_count) * surfaces > (owner.generation_config_inline ? owner.binding_capacity : owner.surface_capacity)) {
			out->status = output_status(error == OK ? ERR_OUT_OF_MEMORY : error, "output_surface_capacity");
			return;
		}
	}
	for (uint32_t i = 0; i < 3; ++i) {
		if (action.output_roles[i].bytes != (owner.config.capacity ? layout.bytes[i] : 0)) {
			out->status = output_status(ERR_INVALID_DATA, "output_role_extent");
			return;
		}
	}
	// Zero native storage has no preparation rows or manufactured buffers.
	// Nonzero native preparation pays the original action P/G envelope.
	const bool reserve_pipelines = layout.resource_count && action.output_read_capacity;
	if (action.consumed_proof_requested && (!reserve_pipelines || owner.view_capacity < 2)) {
		out->status = output_status(ERR_OUT_OF_MEMORY, "consumed_proof_quoted_view_capacity");
		return;
	}
	if (reserve_pipelines && (!action.output_read_graph_capacity || action.output_read_capacity < surfaces)) {
		out->status = output_status(ERR_OUT_OF_MEMORY, "output_preparation_capacity");
		return;
	}
	uint32_t spare = 0;
	for (uint32_t i = request->use_count + request->binding_used; i < request->capacity.resource_uses; ++i) {
		const auto &cell = request->uses[i];
		spare += cell.request == request && cell.output_role == ProductionUseCell::OutputRole::UNUSED && !cell.output_consumer && !cell.dependency.physical && !cell.output_pin_serial && !cell.disposal_group_serial;
	}
	const uint64_t borrower_headroom = reserve_pipelines ? uint64_t(owner.view_capacity) + owner.surface_capacity + owner.read_capacity : 0;
	const uint32_t guard_members = action.output_plan_prepared && layout.resource_count ? (owner.config.capacity ? 3u : 2u) : 0u;
	if (guard_members && action.output_plan.guard_owned_resource_limit < guard_members) {
		out->status = output_status(ERR_PARAMETER_RANGE_ERROR, "output_guard_owned_capacity");
		return;
	}
	const uint64_t per_row_uses = uint64_t(action.output_read_graph_capacity) * 2 + 6;
	const uint64_t fixed_uses = 1u + (layout.resource_count ? uint64_t(layout.resource_count) + 1 : 0) +
			(reserve_pipelines ? borrower_headroom : 0) + (guard_members ? uint64_t(guard_members) + 1 : 0);
	if (reserve_pipelines && uint64_t(action.output_read_capacity) > (UINT64_MAX - fixed_uses) / per_row_uses) {
		out->status = output_status(ERR_PARAMETER_RANGE_ERROR, "output_preparation_quote_overflow");
		return;
	}
	const uint64_t required_uses = fixed_uses + (reserve_pipelines ? uint64_t(action.output_read_capacity) * per_row_uses : 0);
	const uint32_t preparation_count = layout.resource_count ? (reserve_pipelines ? action.output_read_capacity : surfaces) : 0;
	if (preparation_count > owner.free_preparation_rows()) {
		// The original action is known even though this refusal admitted no lease.
		out->request_serial = request->serial;
		out->action_ordinal = ordinal;
		out->status = output_status(ERR_OUT_OF_MEMORY, "output_global_preparation_bank");
		return;
	}
	if (spare < required_uses || action.output_read_capacity > owner.read_capacity || request->graph_count > request->capacity.graph_memberships || uint64_t(action.output_read_capacity) * action.output_read_graph_capacity > request->capacity.graph_memberships - request->graph_count) {
		out->status = output_status(ERR_OUT_OF_MEMORY, "output_prequoted_capacity");
		return;
	}
	const uint64_t required_groups = (layout.resource_count ? 1u : 0u) + (reserve_pipelines ? uint64_t(action.output_read_capacity) * 4 : 0u) + (guard_members ? 1u : 0u);
	if (required_groups && (!request->next_disposal_group_serial || required_groups - 1 > UINT64_MAX - request->next_disposal_group_serial)) {
		out->status = output_status(ERR_PARAMETER_RANGE_ERROR, "output_group_serial_capacity");
		return;
	}
	Owner::MetadataLayout metadata;
	error = owner.generation_config_inline ? Owner::plan_metadata_layout(owner.view_capacity, owner.read_capacity, owner.surface_capacity, metadata, owner.generation_mesh_count, owner.generation_material_count, owner.binding_capacity, owner.storage_capacity, owner.preparation_capacity) : Owner::plan_metadata_layout(owner.view_capacity, owner.read_capacity, owner.surface_capacity, metadata);
	if (error != OK || metadata.total_bytes != owner.metadata_lease.get_size()) {
		out->status = output_status(ERR_INVALID_DATA, "output_metadata_layout");
		return;
	}
	error = production_output_native_append_enter_locked();
	if (error != OK) {
		out->status = output_status(error, "output_native_append_budget");
		return;
	}
	auto &slot = owner.slots[slot_index];
	error = production_output_action_pin_locked(*request, action, *rd, slot.original_pin);
	if (error != OK) {
		out->status = output_status(error, "output_action_root");
		return;
	}
	slot.original_registry = this;
	slot.original_request = ref;
	slot.original_revision = request->revision;
	slot.original_action_ordinal = ordinal;
	slot.request = parsed;
	// Preserve the real pre-cull proof with the accepted slot. Later versions
	// cannot revoke this original prefix or its physical retirement.
	slot.scene_cull_bound = action.scene_cull_prepared;
	slot.scene_cull_instance = slot.scene_cull_particles = slot.scene_cull_scenario = slot.scene_cull_viewport = RID();
	slot.scene_cull_version = 0;
	slot.scene_cull_local = slot.scene_cull_world = AABB();
	if (action.scene_cull_prepared) {
		slot.scene_cull_instance = RID::from_uint64(action.output_scene.instance);
		slot.scene_cull_particles = RID::from_uint64(input->particles);
		slot.scene_cull_scenario = RID::from_uint64(action.output_scene.scenario);
		slot.scene_cull_viewport = action.scene_cull_capture->get_viewport();
		slot.scene_cull_version = action.scene_cull_version;
		slot.scene_cull_local = action.scene_cull_local;
		slot.scene_cull_world = action.scene_cull_world;
	}
	slot.lease_serial = owner.next_lease_serial;
	owner.next_lease_serial = owner.next_lease_serial == UINT64_MAX ? 0 : owner.next_lease_serial + 1;
	slot.consumed_proof_requested = action.consumed_proof_requested;
	slot.consumed_borrow_serial = 0;
	slot.state = Owner::LeaseState::RESERVED;
	slot.publication_closed = slot.pass_uses_closed = false;
	slot.lease_token.resize(32);
	const uint64_t locator[] = { instance, request->serial, ordinal, slot.lease_serial };
	for (uint32_t word = 0; word < 4; ++word) {
		for (uint32_t byte = 0; byte < 8; ++byte) {
			slot.lease_token.ptrw()[word * 8 + byte] = uint8_t(locator[word] >> (byte * 8));
		}
	}
	error = production_output_action_owner_bind_locked(*request, action, slot.original_pin, *rd, RID::from_uint64(input->particles), owner.generation, slot_index, slot.lease_serial);
	if (error != OK) {
		const Error rollback = production_output_unpin_locked(slot.original_pin);
		CRASH_COND(rollback != OK);
		slot.state = Owner::LeaseState::EMPTY;
		slot.original_registry = nullptr;
		slot.original_request = {};
		action.output_state = ProductionActionCell::OutputState::UNBOUND;
		action.output_root = 0;
		out->status = output_status(error, "output_owner_bind");
		return;
	}
	action.output_preparation_borrower_headroom = borrower_headroom;
	if (layout.resource_count) {
		auto *base = static_cast<uint8_t *>(owner.metadata_lease.get_data());
		slot.storage = new (base + metadata.storage_offset + size_t(slot_index) * sizeof(Storage)) Storage(*rd);
		slot.storage_placement = true;
		slot.surface_count = surfaces;
		slot.surfaces = reinterpret_cast<Owner::SurfaceBinding *>(base + metadata.surface_binding_offset) + size_t(slot_index) * surfaces;
		slot.surfaces_placement = true;
		for (uint32_t i = 0; i < surfaces; ++i) {
			new (&slot.surfaces[i]) Owner::SurfaceBinding;
		}
		// The executing original RECORD and installed independent root retain
		// the action/slot while each factory reacquires the Registry lock.
		error = owner.bind_preparation_rows(slot_index, preparation_count);
		if (error == OK && guard_members) {
			// Reserve the exact original future guard prefix before native factories.
			// A reservation is neither a ready descriptor nor completed Records.
			unlock_mutex_for_call();
			error = rd->_production_output_owned_reserve(context, slot.original_pin, guard_members, slot.guard_owned_group);
			relock_mutex_after_call();
		}
		if (error == OK && reserve_pipelines) {
			// Prepay all row groups before the first storage/native factory.
			unlock_mutex_for_call();
			error = storage->_external_preparation_bank_reserve_owned_prefix(context, RID::from_uint64(input->particles), slot_index, action.output_read_graph_capacity);
			relock_mutex_after_call();
		}
		if (error == OK) {
			unlock_mutex_for_call();
			error = slot.storage->allocate_instance192_owned(context, slot.original_pin, slot.owned_group, owner.config.capacity, surfaces);
			relock_mutex_after_call();
		}
		if (error == OK) {
			uint32_t member = 0;
			for (uint32_t i = 0; i < Storage::RESOURCE_COUNT; ++i) {
				const auto &resource = slot.storage->resources[i];
				if (!resource.bytes) {
					continue;
				}
				if (i < 3 && action.output_roles[i].bytes) {
					error = production_output_alias_bind_locked(*request, action, i, slot.original_pin, slot.owned_group, member, resource.witness, *rd);
					if (error != OK) {
						break;
					}
				}
				++member;
			}
		}
	}
	if (error == OK) {
		for (uint32_t i = 0; i < request->resource_count; ++i) {
			auto &set = request->resources[i];
			if (!set.output_set_deferred || set.imported || set.primary.is_valid()) {
				continue;
			}
            if (request->native_history_selected) {
                bool selected_set = false;
                for (uint32_t a = 0; a < request->action_count; ++a) {
                    const auto &selected = request->actions[a];
                    selected_set |= selected.declaration.action_kind == GDEXTENSION_PRODUCTION_DISPATCH_V1 &&
                            i >= selected.dispatch.first_set_slot && i - selected.dispatch.first_set_slot < selected.dispatch.set_count;
                }
                // The unchosen immutable preparation retains no physical SET
                // prefix. Its paid finite slot remains until original release.
                if (!selected_set) { continue; }
            }
			bool participates = false, ready = true;
			for (uint32_t b = 0; b < set.binding_count; ++b) {
				const auto &resource = request->resources[request->uses[set.first_binding + b].binding.resource_slot];
				participates |= resource.output_alias_prepared && resource.output_action_ordinal == ordinal;
				ready &= resource.imported;
			}
			if (!participates || !ready) {
				continue;
			}
			set.creating = true;
			request->returns[i].pending = true;
			request->returns[i].serial = set.retire_serial;
			error = production_create_execute_locked(*request, *rd, ProductionControlKind::SET_CREATE, i);
			if (error != OK) {
				// This is a synchronous child of the original output action, not
				// the queued SET_CREATE control's retry loop. Retain every real
				// prefix, but do not leave an unissued child marked creating.
				set.creating = false;
				set.handoff_error = error;
				if (set.primary.is_null()) {
					request->returns[i].pending = false;
				}
				break;
			}
		}
	}
	if (error != OK) {
		bool prefix = slot.owned_group != 0 || slot.guard_owned_group != 0;
		for (uint32_t i = 0; i < slot.surface_count; ++i) {
			prefix |= slot.surfaces[i].pipeline_owned_group != 0 || slot.surfaces[i].created_pipeline.is_valid();
		}
		for (uint32_t i = 0; i < owner.preparation_capacity; ++i) {
			const auto &row = owner.preparation_rows[i];
			prefix |= row.owner == &owner && row.owner_slot == slot_index && (row.pipeline_owned_group || row.geometry_reserved_group || row.material_reserved_group || row.transforms_owned_group || row.transforms_uniform_set.is_valid() || row.owned_pipeline_root.is_valid());
		}
		if (slot.storage) {
			for (const auto &resource : slot.storage->resources) {
				prefix |= resource.rid.is_valid();
			}
		}
		if (!prefix) {
			if (slot.storage) {
				slot.storage->~ExternalOutputStorage();
				slot.storage = nullptr;
			}
			for (uint32_t i = 0; i < owner.preparation_capacity; ++i) {
				auto &row = owner.preparation_rows[i];
				if (row.owner != &owner || row.owner_slot != slot_index) {
					continue;
				}
				row.pipeline_pin = nullptr;
				row.pipeline_original_root = nullptr;
				if (row.owner_surface != UINT32_MAX) {
					slot.surfaces[row.owner_surface].preparation = nullptr;
				}
				CRASH_COND(owner.return_preparation_bank_row(slot_index, i) != OK);
			}
			for (uint32_t i = 0; i < slot.surface_count; ++i) {
				// Only unopened metadata is rolled back here. Native prefixes
				// keep the slot and its stable preparation row in retirement.
				CRASH_COND(owner.return_preparation_row(slot_index, i) != OK);
				slot.surfaces[i].~SurfaceBinding();
			}
			slot.surfaces = nullptr;
			slot.surface_count = 0;
			const Error rollback = production_output_unpin_locked(slot.original_pin);
			CRASH_COND(rollback != OK);
			slot.state = Owner::LeaseState::EMPTY;
			slot.original_registry = nullptr;
			slot.original_request = {};
			action.output_state = ProductionActionCell::OutputState::UNBOUND;
			action.output_root = action.output_lease = action.output_generation = 0;
			action.output_preparation_borrower_headroom = 0;
			action.output_slot = UINT32_MAX;
			action.output_particles = RID();
		} else {
			if (action.output_state != ProductionActionCell::OutputState::UNKNOWN) {
				action.output_state = ProductionActionCell::OutputState::RETIRING;
			}
			owner.cancel_slot(slot_index);
			project();
		}
		out->status = output_status(error, "output_factory_prefix");
		return;
	}
	action.output_state = ProductionActionCell::OutputState::BOUND;
	++request->next_action;
	++request->revision;
	owner.views[view].tag = parsed.common.view_tag;
	owner.views[view].epoch = parsed.view_epoch;
	owner.views[view].last_present = parsed.common.present_seq;
	owner.views[view].occupied = true;
	owner.state = Owner::GenerationState::ACTIVE;
	project();
	out->status = output_status(OK, "");
}

static Error output_scene_query_world(const RendererSceneCull::NativeParticleSceneFacts &facts,
		uint64_t generation, const AABB &initial_default, AABB &out) {
	if (!facts.query_generation) {
		out = initial_default;
		return OK;
	}
	// A default is only an initial domain. It cannot renew an invalidated
	// instance projection or replace a different generation's query source.
	if (facts.query_generation != generation) { return ERR_INVALID_DATA; }
	if (!facts.query_committed) { return ERR_UNCONFIGURED; }
	const Vector3 end = facts.query_world.get_end();
	if (!facts.query_world.is_finite() || !end.is_finite() || !facts.query_world.has_volume() ||
			end.x <= facts.query_world.position.x || end.y <= facts.query_world.position.y || end.z <= facts.query_world.position.z) {
		return ERR_INVALID_DATA;
	}
	out = facts.query_world;
	return OK;
}

Error Registry::production_output_cull_required_locked(const ProductionActionCell &action,
		const GDExtensionOutputAcquireInputV1 &input, bool include_candidate, AABB &out, AABB *out_local) const {
	if (!action.preinput_host_prepared || action.preinput_host_returned || !action.request || !action.request->attachment) {
		return ERR_UNCONFIGURED;
	}
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	const auto *particles = storage ? storage->particles_owner.get_or_null(RID::from_uint64(input.particles)) : nullptr;
	const auto *owner = particles ? particles->external : nullptr;
	if (!owner || owner->generation_registry != this || owner->generation != input.common.generation ||
			owner->device != action.request->attachment->held_device || owner->config.profile != RendererRD::ExternalOutputConfig::INSTANCE_192 ||
			(owner->config.space != RendererRD::ExternalOutputConfig::WORLD && owner->config.space != RendererRD::ExternalOutputConfig::LOCAL)) {
		return ERR_UNAUTHORIZED;
	}
	AABB declared;
	for (uint32_t axis = 0; axis < 3; ++axis) {
		const double position = input.common.bounds.position[axis];
		const double size = input.common.bounds.size[axis];
		declared.position[axis] = real_t(position);
		declared.size[axis] = real_t(size);
		if (!Math::is_finite(position) || !Math::is_finite(size) || size <= 0 ||
				double(declared.position[axis]) != position || double(declared.size[axis]) != size) {
			return ERR_INVALID_PARAMETER;
		}
	}
	AABB current_world = declared;
	if (owner->config.space == RendererRD::ExternalOutputConfig::LOCAL) {
		const Error transformed = NativeOutputCull::transform_bounds(declared, action.preinput_host_transform.affine, current_world);
		if (transformed != OK) { return transformed; }
	}
	AABB previous_world;
	bool has_previous = false;
	if (include_candidate && !input.common.history_reset && input.common.has_previous_consumed_tuple) {
		if (!action.staged_predecessor_intent) { return ERR_UNAUTHORIZED; }
		const auto &request = *action.request;
		for (uint32_t index = 0; index < request.resource_count; ++index) {
			const auto &resource = request.resources[index];
			const auto &view = resource.output_predecessor_view;
			if (view.is_empty() || resource.output_predecessor_action != action.declaration.action_ordinal) { continue; }
			if (!production_output_predecessor_matches_locked(request, resource, *owner->device) ||
					!output_input_equal(resource.output_predecessor_input, action.output_input) || view.owner != owner ||
					view.get_space() != owner->config.space || !production_dto_valid(resource.output_predecessor_host)) {
				return ERR_UNAUTHORIZED;
			}
			RendererRD::ExternalConsumedColorProofSnapshot proof;
			const Error copied = view.copy_native_color_consumption(proof);
			if (copied != OK) { return copied; }
			const auto &common = proof.get_common();
			if (proof.get_particles().get_id() != input.particles || proof.get_generation() != input.common.generation ||
					common.output_tag.size() != 32 || common.view_tag.size() != 32 || common.current_host_token.size() != 32 ||
					memcmp(common.output_tag.ptr(), input.common.output_tag.bytes, 32) ||
					memcmp(common.view_tag.ptr(), input.common.view_tag.bytes, 32) ||
					memcmp(proof.get_token().bytes, input.common.previous_consumed_tuple_token.bytes, 32) ||
					memcmp(common.current_host_token.ptr(), resource.output_predecessor_host.host_token.bytes, 32)) {
				return ERR_INVALID_DATA;
			}
			const auto &previous_host = proof.get_current_host();
			for (uint32_t row = 0; row < 3; ++row) {
				for (uint32_t column = 0; column < 3; ++column) {
					if (previous_host.basis[row][column] != real_t(resource.output_predecessor_host.affine[row * 4 + column])) { return ERR_INVALID_DATA; }
				}
				if (previous_host.origin[row] != real_t(resource.output_predecessor_host.affine[row * 4 + 3])) { return ERR_INVALID_DATA; }
			}
			AABB candidate = common.bounds;
			if (view.get_space() == RendererRD::ExternalOutputConfig::LOCAL) {
				const Error transformed = NativeOutputCull::transform_bounds(common.bounds, resource.output_predecessor_host.affine, candidate);
				if (transformed != OK) { return transformed; }
			}
			if (has_previous && candidate != previous_world) { return ERR_INVALID_DATA; }
			previous_world = candidate;
			has_previous = true;
		}
		if (!has_previous) { return ERR_UNCONFIGURED; }
	}
	AABB query_world;
	if (owner->config.query_enabled) {
		auto *scene = static_cast<RendererSceneCull *>(RSG::scene);
		if (!scene) { return ERR_UNAVAILABLE; }
		RendererSceneCull::NativeParticleSceneFacts facts;
		const Error observed = scene->instance_get_committed_particle_scene_facts(RID::from_uint64(action.output_scene.instance),
				RID::from_uint64(input.particles), facts);
		if (observed != OK) { return observed; }
		const Error queried = output_scene_query_world(facts, input.common.generation, owner->config.query_bounds, query_world);
		if (queried != OK) { return queried; }
		// A real query owner must commit a changed pose before this selected
		// view. Cull preparation has no authority to issue a query revision.
		if (facts.query_committed) {
			for (uint32_t row = 0; row < 3; ++row) {
				for (uint32_t column = 0; column < 3; ++column) {
					if (facts.transform.basis[row][column] != real_t(action.preinput_host_transform.affine[row * 4 + column])) { return ERR_UNCONFIGURED; }
				}
				if (facts.transform.origin[row] != real_t(action.preinput_host_transform.affine[row * 4 + 3])) { return ERR_UNCONFIGURED; }
			}
		}
	}
	NativeOutputCull::Envelope envelope;
	const Error composed = NativeOutputCull::compose(current_world, has_previous ? &previous_world : nullptr,
			owner->config.query_enabled ? &query_world : nullptr,
			action.preinput_host_transform.affine, action.preinput_host_transform.inverse, envelope);
	if (composed == OK) {
		out = envelope.world;
		if (out_local) { *out_local = envelope.local; }
	}
	return composed;
}

Error Registry::production_prepare_staged_cull_locked(ProductionRequest &request, ProductionActionCell &action,
		RenderingDevice &rd, const ProductionCameraInvocationBorrow &borrow) {
	const auto &invocation = borrow.get_scope();
	const auto &capture = invocation.get_capture();
	const auto &budget = request.native_stage_budget;
	if (!Lock::owns(*this) || !invocation.is_pre_cull() || !invocation.is_current(&action.camera_ready_subscriber, capture) ||
			request.attachment->held_device != &rd || !action.output_source_prepared || !action.output_plan_prepared ||
			!action.output_scene_prepared || !action.preinput_host_prepared || action.preinput_host_returned ||
			action.preinput_camera_returned || action.preinput_camera_source.is_valid() ||
			request.next_action > action.declaration.action_ordinal || !action.host_view_prepared || !action.host_view_received ||
			capture->get_viewport() != action.camera_ready_viewport || capture->get_invocation().camera != action.host_view_camera ||
			capture->get_presentation_key().view_id != action.host_view_key.view_id ||
			capture->get_presentation_key().view_epoch != action.host_view_key.view_epoch) {
		return ERR_UNAUTHORIZED;
	}
	if (!budget.deadline_micros || OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros ||
			request.native_stage_controls_used >= budget.controls || request.native_stage_passes_used >= budget.passes ||
			request.native_stage_receipts_used >= budget.receipts) {
		return ERR_BUSY;
	}
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	auto *particles = storage ? storage->particles_owner.get_or_null(RID::from_uint64(action.output_input.particles)) : nullptr;
	auto *owner = particles ? particles->external : nullptr;
	if (!owner || owner->device != &rd || owner->generation_registry != this || owner->generation != action.output_input.common.generation ||
			!owner->admits_work() || !owner->generation_root_matches(owner->generation_pin) ||
			owner->generation_pin.is_empty() || owner->generation_pin.request->attachment != request.attachment ||
			action.output_plan.source_space != uint32_t(owner->config.space) ||
			action.output_scene.particles != action.output_input.particles || action.output_scene.generation != owner->generation ||
			memcmp(action.output_scene.host.affine, action.preinput_host_transform.affine, sizeof(action.output_scene.host.affine)) ||
			memcmp(action.output_scene.host.host_token.bytes, action.preinput_host_transform.host_token.bytes, 32) ||
			memcmp(action.output_input.common.current_host_token.bytes, action.preinput_host_transform.host_token.bytes, 32)) {
		return ERR_UNAUTHORIZED;
	}
	const Error admission = production_output_pin_new_work_matches_locked(owner->generation_pin, rd);
	if (admission != OK) { return admission; }
	if (!production_output_source_matches_locked(request, action, rd)) { return ERR_UNAUTHORIZED; }
	AABB required, native_local;
	const bool candidate = !request.native_history_static_prepared || request.native_history_static.kind != 1;
	const Error gathered = production_output_cull_required_locked(action, action.output_input, candidate, required, &native_local);
	if (gathered != OK) { return gathered; }
	const auto &host = action.preinput_host_transform;
	Transform3D transform;
	for (uint32_t row = 0; row < 3; ++row) {
		for (uint32_t column = 0; column < 3; ++column) { transform.basis[row][column] = host.affine[row * 4 + column]; }
		transform.origin[row] = host.affine[row * 4 + 3];
	}
	auto *scene = static_cast<RendererSceneCull *>(RSG::scene);
	if (!scene) { return ERR_UNAVAILABLE; }
	if (OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros) { return ERR_BUSY; }
	RendererSceneCull::NativeParticleSceneFacts facts;
	const Error committed = scene->instance_commit_particle_output_cull(RID::from_uint64(action.output_scene.instance),
			RID::from_uint64(action.output_scene.particles), RID::from_uint64(action.output_scene.scenario), owner->generation,
			transform, native_local, required, facts);
	if (committed != OK) { return committed; }
	if (facts.instance.get_id() != action.output_scene.instance || facts.particles.get_id() != action.output_scene.particles ||
			facts.scenario.get_id() != action.output_scene.scenario || facts.transform != transform ||
			!facts.version || !facts.world_aabb.encloses(required)) {
		return ERR_INVALID_DATA;
	}
	action.scene_cull_capture = capture;
	action.scene_cull_version = facts.version;
	action.scene_cull_local = facts.local_aabb;
	action.scene_cull_world = facts.world_aabb;
	action.scene_cull_required_world = required;
	action.scene_cull_prepared = true;
	return OK;
}

// The physical RECORD supplies append authority. This observation binds its
// original source to the actual committed scene envelope and selected window.
Error Registry::production_output_scene_validate_locked(const ProductionActionCell &action, const GDExtensionOutputAcquireInputV1 &input) const {
	if (!action.output_scene_prepared || !action.preinput_host_prepared || action.preinput_host_returned) { return ERR_UNCONFIGURED; }
	const auto &source = action.output_scene;
	const auto &host = action.preinput_host_transform;
	if (source.particles != input.particles || source.generation != input.common.generation ||
			memcmp(&source.host.host_token, &host.host_token, sizeof(host.host_token)) ||
			memcmp(source.host.affine, host.affine, sizeof(host.affine)) ||
			memcmp(&host.host_token, &input.common.current_host_token, sizeof(host.host_token))) { return ERR_INVALID_DATA; }
	// RenderingServerDefault owns exactly this SceneCull (see its _init).
	auto *scene = static_cast<RendererSceneCull *>(RSG::scene);
	if (!scene) { return ERR_UNAVAILABLE; }
	RendererSceneCull::NativeParticleSceneFacts facts;
	const Error observed = scene->instance_get_committed_particle_scene_facts(RID::from_uint64(source.instance), RID::from_uint64(source.particles), facts);
	if (observed != OK) { return observed; }
	if (facts.instance.get_id() != source.instance || facts.particles.get_id() != source.particles || facts.scenario.get_id() != source.scenario) { return ERR_INVALID_DATA; }
	for (uint32_t row = 0; row < 3; ++row) {
		for (uint32_t column = 0; column < 3; ++column) {
			if (facts.transform.basis[row][column] != real_t(host.affine[row * 4 + column])) { return ERR_INVALID_DATA; }
		}
		if (facts.transform.origin[row] != real_t(host.affine[row * 4 + 3])) { return ERR_INVALID_DATA; }
	}
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	const auto *particles = storage ? storage->particles_owner.get_or_null(RID::from_uint64(input.particles)) : nullptr;
	const auto *owner = particles ? particles->external : nullptr;
	if (!owner || !action.request || !action.request->attachment || owner->generation_registry != this ||
			owner->generation != input.common.generation || owner->device != action.request->attachment->held_device) { return ERR_UNAUTHORIZED; }
	// SceneOnly has no authored geometry declaration or native geometry guard.
	// Keep its original query-only world envelope and ordinary Host association.
	if (owner->config.profile == RendererRD::ExternalOutputConfig::SCENE_ONLY) {
		if (action.scene_cull_prepared) { return ERR_UNAUTHORIZED; }
		if (!owner->config.query_enabled) { return OK; }
		AABB query_world;
		const Error queried = output_scene_query_world(facts, input.common.generation, owner->config.query_bounds, query_world);
		if (queried != OK) { return queried; }
		return facts.world_aabb.encloses(query_world) ? OK : ERR_INVALID_DATA;
	}
	AABB required;
	const Error gathered = production_output_cull_required_locked(action, input, false, required);
	if (gathered != OK) { return gathered; }
	if (!facts.world_aabb.encloses(required)) { return ERR_INVALID_DATA; }
	if (action.scene_cull_prepared) {
		if (action.scene_cull_error != OK || action.scene_cull_capture.is_null() || facts.version != action.scene_cull_version ||
				(action.preinput_camera_source.is_valid() && action.preinput_camera_source != action.scene_cull_capture) ||
				facts.local_aabb != action.scene_cull_local || facts.world_aabb != action.scene_cull_world ||
				!facts.world_aabb.encloses(action.scene_cull_required_world)) { return ERR_INVALID_DATA; }
		const auto &capture = action.scene_cull_capture;
		const auto &token = capture->get_token();
		if (!scene->instance_particle_output_cull_window_matches(facts.instance, capture->get_viewport(),
				token.view_id, token.view_epoch, token.capture_ordinal)) { return ERR_UNAUTHORIZED; }
	}
	return OK;
}

void Registry::production_finalize_output(ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, uint64_t ordinal, GDExtensionProductionOutputFinalizeResultV1 *out) {
	if (!out || uintptr_t(out) % alignof(GDExtensionProductionOutputFinalizeResultV1)) {
		return;
	}
	*out = {};
	out->abi = production_dto_abi<GDExtensionProductionOutputFinalizeResultV1>();
	out->status = output_status(ERR_UNAUTHORIZED, "original_finalize_context");
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_physical_request_locked(context, ref, request, rd, true);
	if (error != OK) {
		out->status = output_status(error, "original_finalize_context");
		return;
	}
	if (ordinal >= request->action_count || !request->frozen || request->capture_inflight || request->effects_pending || request->source_device_lost || request->revision > UINT64_MAX - uint64_t(request->capacity.graph_memberships) * 2 - uint64_t(request->capacity.native_memberships) * 2 - request->capacity.readbacks - request->capacity.resources - 8) {
		return;
	}
	auto &finalizer = request->actions[ordinal];
	if (finalizer.request != request || !finalizer.prepared || finalizer.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || finalizer.output_acquire_action_ordinal >= ordinal) {
		return;
	}
	auto &acquire = request->actions[finalizer.output_acquire_action_ordinal];
	const auto &plan = acquire.output_plan;
	if (acquire.request != request || !acquire.output_plan_prepared || plan.finalize_action_ordinal != ordinal || plan.finalize_canonical_suboperation != finalizer.declaration.canonical_suboperation || plan.finalize_source_recipe_substep != finalizer.declaration.source_recipe_substep || acquire.output_state != ProductionActionCell::OutputState::BOUND) {
		out->status = output_status(ERR_INVALID_DATA, "original_finalize_row");
		return;
	}
	if (!acquire.output_guard_entered && !acquire.output_guard_complete) {
		error = production_output_scene_validate_locked(acquire, acquire.output_input);
		if (error != OK) {
			out->custody = GDEXTENSION_OUTPUT_CUSTODY_RETAINED_PREFIX_V1;
			out->status = output_status(error, "output_finalize_committed_scene");
			return;
		}
	}
	if (acquire.output_guard_complete) {
		out->state = GDEXTENSION_PRODUCTION_APPENDED_V1;
		out->custody = GDEXTENSION_OUTPUT_CUSTODY_ACCEPTED_V1;
		out->record_token = acquire.output_record_token;
		out->status = output_status(OK, "");
		return;
	}
	if ((acquire.output_guard_entered && !production_native_batch_frame_locked(*request)) || request->next_action != ordinal || request->draw_list_open || request->compute_list_open || (request->segment.segment != request->acknowledged_segment && !production_native_batch_frame_locked(*request))) {
		out->state = acquire.output_guard_append.state == CheckedRecordAppend::State::Unknown ? GDEXTENSION_PRODUCTION_UNKNOWN_V1 : GDEXTENSION_PRODUCTION_KNOWN_PREFIX_V1;
		out->custody = acquire.output_guard_entered ? GDEXTENSION_OUTPUT_CUSTODY_RETAINED_PREFIX_V1 : GDEXTENSION_OUTPUT_NO_CUSTODY_V1;
		out->status = output_status(ERR_BUSY, "original_finalize_prefix");
		return;
	}
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	auto *particles = storage ? storage->particles_owner.get_or_null(acquire.output_particles) : nullptr;
	if (!particles || !particles->external || particles->external->device != rd || particles->external->generation_registry != this || particles->external->generation != acquire.output_generation || acquire.output_slot >= particles->external->slot_count) {
		return;
	}
	auto &owner = *particles->external;
	auto &slot = owner.slots[acquire.output_slot];
	if (slot.original_registry != this || slot.original_request.registry_instance != instance || slot.original_request.handle_serial != request->serial || slot.original_action_ordinal != finalizer.output_acquire_action_ordinal || slot.lease_serial != acquire.output_lease || !owner.admits_work() || slot.publication_closed || slot.pass_uses_closed || !slot.storage || !acquire.preinput_host_prepared || acquire.preinput_host_returned || acquire.preinput_camera_source.is_null() || memcmp(acquire.preinput_host_transform.host_token.bytes, acquire.output_input.common.current_host_token.bytes, 32) != 0) {
		out->status = output_status(ERR_UNAVAILABLE, "original_finalize_source");
		return;
	}
	using Guard = RendererRD::ParticlesExternalGuard;
	using Storage = RendererRD::ExternalOutputStorage;
	if (!rd->_thread_safe_.try_lock()) {
		out->status = output_status(ERR_BUSY, "output_device_busy");
		return;
	}
	bool valid = !rd->draw_list.active && !rd->compute_list.active && !rd->raytracing_list.active && rd->_recording_error() == OK && production_output_factory_pin_matches_locked(*request, slot.original_pin, *rd) && production_output_records_written_locked(*request, acquire, uint32_t(ordinal));
	for (uint32_t g = 0; g < request->graph_count; ++g) {
		uint32_t command = UINT32_MAX;
		valid &= rd->draw_graph._resolve_record_command_index(request->graphs[g].handle, command);
	}
	Vector<RD::ProductionOutputUniformSource> source_reads;
	source_reads.resize(5);
	uint64_t source_bytes[5]{};
	uint32_t source_words[5]{};
	for (uint32_t i = 0; i < 5 && valid; ++i) {
		const auto &mapping = plan.reads[i];
		if (mapping.resource_slot >= request->resource_count || mapping.use_index >= request->use_count || mapping.word_offset > UINT32_MAX) {
			valid = false;
			break;
		}
		const auto &resource = request->resources[mapping.resource_slot];
		const auto &use = request->uses[mapping.use_index].declaration;
		const auto *buffer = rd->storage_buffer_owner.get_or_null(resource.primary);
		const uint64_t word_byte = uint64_t(mapping.word_offset) * 4;
		valid &= buffer && resource.declaration.original_generation == mapping.generation && use.resource_slot == mapping.resource_slot && use.access == GDEXTENSION_PRODUCTION_READ_V1 && word_byte >= use.byte_offset && word_byte - use.byte_offset <= use.byte_size && use.byte_size - (word_byte - use.byte_offset) >= 4;
		if (valid) {
			source_reads.write[i].buffer = resource.primary;
			source_reads.write[i].resource_slot = mapping.resource_slot;
			source_reads.write[i].use_index = mapping.use_index;
			source_reads.write[i].byte_offset = use.byte_offset;
			source_reads.write[i].byte_size = use.byte_size;
			source_bytes[i] = buffer->size;
			source_words[i] = mapping.word_offset;
		}
	}
	const uint32_t max_groups = rd->limit_get(RD::LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X);
	const uint64_t chunk_records = uint64_t(max_groups) * 64;
	const uint64_t validations = owner.config.capacity && chunk_records ? (uint64_t(owner.config.capacity) + chunk_records - 1) / chunk_records : 0;
	const uint64_t dispatches = validations + 2;
	const uint64_t work = uint64_t(owner.config.capacity) * slot.surface_count;
	const uint64_t later = finalizer.graph_suffix_bound;
	const uint32_t guard_graphs_used = acquire.output_guard_entered ? request->graph_count - acquire.output_guard_graph_begin : 0;
	const uint32_t guard_remaining = guard_graphs_used <= plan.guard_graph_limit ? plan.guard_graph_limit - guard_graphs_used : 0;
	const uint64_t remaining_dispatches = acquire.output_guard_dispatch_cursor <= dispatches ? dispatches - acquire.output_guard_dispatch_cursor : dispatches;

	const Error schedule = production_schedule_locked(*request, *rd, false);
	uint32_t native_bound = 0, remaining_native = 0;
	const Error suffix_native = uint64_t(guard_remaining) + later > UINT32_MAX ? ERR_PARAMETER_RANGE_ERROR : production_membership_bound_locked(*rd, uint32_t(guard_remaining + later), request->native_workers_bound, remaining_native);
	if (suffix_native != OK || schedule != OK || !valid || !max_groups || plan.guard_owned_resource_limit < (owner.config.capacity ? 3u : 2u) || work > plan.guard_work_limit || dispatches > plan.guard_dispatch_limit || (remaining_dispatches > guard_remaining || (remaining_dispatches == guard_remaining && (!acquire.output_guard_entered || acquire.output_guard_upload_bytes < slot.guard_proof_bytes))) || request->graph_count > request->capacity.graph_memberships || uint64_t(guard_remaining) + later > request->capacity.graph_memberships - request->graph_count || production_membership_bound_locked(*rd, guard_remaining, request->native_workers_bound, native_bound) != OK || native_bound > plan.guard_native_limit || request->native_count > request->capacity.native_memberships || remaining_native > request->capacity.native_memberships - request->native_count) {
		rd->_thread_safe_.unlock();
		out->status = output_status(ERR_PARAMETER_RANGE_ERROR, "output_guard_original_budget");
		return;
	}
	RendererRD::ParticlesStorage::ExternalGuardHistorySnapshot history;
	const bool has_history = !slot.request.common.history_reset && slot.request.common.has_previous_consumed_tuple;
	if (has_history) {
		ProductionOutputHistoryBorrow borrowed;
		error = production_output_history_borrow_locked(context, *request, finalizer.output_acquire_action_ordinal, *rd, borrowed);
		RD::ProductionOutputUniformSource predecessor_read;
		bool found_read = false;
		for (uint32_t i = 0; i < request->resource_count && error == OK; ++i) {
			const auto &resource = request->resources[i];
			if (&resource.output_predecessor_view != borrowed.previous_view) {
				continue;
			}
			const uint32_t use_index = resource.output_predecessor_use;
			if ((!borrowed.previous_no_records && (resource.primary != borrowed.previous_records || &resource.output_predecessor_witness != borrowed.previous_records_witness)) ||
					(borrowed.previous_no_records && (!resource.output_predecessor_no_records || resource.primary != borrowed.previous_guard_status ||
							!borrowed.previous_guard_witness || resource.output_predecessor_witness.cell != borrowed.previous_guard_witness->cell)) ||
					use_index >= request->use_count || use_index < acquire.output_first_source_use ||
					use_index - acquire.output_first_source_use >= acquire.output_source_use_count ||
					!production_output_predecessor_matches_locked(*request, resource, *rd)) {
				error = ERR_UNAUTHORIZED;
				break;
			}
			const auto &use = request->uses[use_index];
			if (use.request != request || use.declaration.resource_slot != i || use.declaration.access != GDEXTENSION_PRODUCTION_READ_V1) {
				error = ERR_INVALID_DATA;
				break;
			}
			predecessor_read.buffer = resource.primary;
			predecessor_read.resource_slot = i;
			predecessor_read.use_index = use_index;
			predecessor_read.byte_offset = use.declaration.byte_offset;
			predecessor_read.byte_size = use.declaration.byte_size;
			found_read = true;
			break;
		}
		if (error == OK) {
			error = found_read ? storage->_external_guard_copy_history_locked(borrowed, predecessor_read, acquire.output_particles, acquire.output_slot, history) : ERR_UNAUTHORIZED;
		}
		if (error != OK) {
			rd->_thread_safe_.unlock();
			out->status = output_status(error, "output_guard_original_history");
			return;
		}
	}
	rd->_thread_safe_.unlock();
	const auto &host = acquire.preinput_host_transform;
	Transform3D current_host;
	for (uint32_t column = 0; column < 3; ++column) {
		current_host.basis.set_column(column, Vector3(host.affine[column], host.affine[4 + column], host.affine[8 + column]));
	}
	current_host.origin = Vector3(host.affine[3], host.affine[7], host.affine[11]);
	if (!current_host.is_finite()) {
		out->status = output_status(ERR_INVALID_DATA, "output_guard_original_host");
		return;
	}
	slot.current_host = current_host;
	slot.retained_host_snapshot = true;
	Guard::LayoutPlan guard_plan;
	unlock_mutex_for_call();
	error = storage->_external_guard_prepare_owned_prefix(context, acquire.output_particles, acquire.output_slot, source_reads, source_bytes, source_words, host, has_history ? &history : nullptr, plan.source_space, max_groups, guard_plan);
	relock_mutex_after_call();
	if (error != OK) {
		out->status = output_status(error, "output_guard_factory_prefix");
		out->custody = slot.guard_owned_group ? GDEXTENSION_OUTPUT_CUSTODY_RETAINED_PREFIX_V1 : GDEXTENSION_OUTPUT_CUSTODY_ACCEPTED_V1;
		return;
	}
	if (!slot.guard_resources_ready || !slot.guard_proof_payload || !slot.guard_proof_bytes || guard_plan.validation_chunks.size() != validations || !rd->_thread_safe_.try_lock()) {
		out->status = output_status(ERR_BUSY, "output_guard_ready");
		return;
	}
	Vector<RID> grant;
	for (uint32_t i = 0; i < Storage::RESOURCE_COUNT; ++i) {
		if (slot.storage->resources[i].rid.is_valid()) {
			grant.push_back(slot.storage->resources[i].rid);
		}
	}
	for (uint32_t i = 0; i < Guard::MODE_COUNT; ++i) {
		if (slot.guard_uniform_sets[i].is_valid()) {
			grant.push_back(slot.guard_uniform_sets[i]);
		}
	}
	error = rd->_production_output_guard_begin_locked(*request, slot.original_pin, slot.owned_group, slot.guard_owned_group, grant);
	if (error != OK) {
		rd->_thread_safe_.unlock();
		out->status = output_status(error, "output_guard_exact_scope");
		return;
	}
	uint64_t record_serial = acquire.output_guard_record_serial;
	if ((!record_serial && !serials.take(record_serial)) || production_output_native_append_enter_locked() != OK) {
		rd->_production_output_guard_end_locked();
		rd->_thread_safe_.unlock();
		out->status = output_status(ERR_PARAMETER_RANGE_ERROR, "output_record_serial");
		return;
	}
	OriginalOperationIdentity identity{ request->operation.owner_request_nonce, request->attachment->library_generation, device_records[request->attachment->device].session, request->operation.record_generation, request->operation.canonical_begin, request->operation.canonical_end };
	CheckedRecordAppend observed;
	CRASH_COND(bind_checked_record_append(observed, identity) != OK);
	const uint32_t first_graph = request->graph_count;
	BufferUploadSourceCapture upload;
	upload.segments = &request->graphs[first_graph].captured;
	upload.segment_stride = sizeof(ProductionGraphCell);
	upload.capacity = guard_remaining - uint32_t(remaining_dispatches);
	upload.consumed = acquire.output_guard_upload_bytes;
	if (!acquire.output_guard_entered) { acquire.output_guard_graph_begin = request->graph_count; }
	acquire.output_guard_entered = true; acquire.output_guard_record_serial = record_serial;
	slot.state = RendererRD::ExternalOutputOwner::LeaseState::WRITING;
	++request->capture_inflight;
	rd->production_recording_request = request;
	production_append_effect_enter_locked(*request);
	unlock_mutex_for_call();
	if (upload.consumed < slot.guard_proof_bytes) {
		observed = rd->_record_source_buffer_upload(identity, slot.storage->resources[Storage::GEOMETRY_PROOF].rid, 0, slot.guard_proof_bytes, slot.guard_proof_payload, upload);
	} else { observed.state = CheckedRecordAppend::State::Appended; }
	relock_mutex_after_call();
	for (uint32_t i = 0; i < upload.count; ++i) {
		CheckedRecordAppend segment = observed;
		segment.handle = upload.segment(i).handle;
		if (production_capture_graph_locked(*request, *rd, ordinal, segment) != OK) {
			observed.state = CheckedRecordAppend::State::Unknown;
			observed.error = ERR_INVALID_DATA;
		}
	}
	const auto *guard = storage->get_external_guard();
	acquire.output_guard_upload_bytes = upload.consumed;
	for (uint32_t stage = acquire.output_guard_dispatch_cursor; stage < dispatches && observed.error == OK && observed.state == CheckedRecordAppend::State::Appended; ++stage) {
		const auto mode = stage == 0 ? guard_plan.initialize_mode : stage + 1 == dispatches ? guard_plan.finalize_mode : Guard::VALIDATE;
		const auto &push = stage == 0 ? guard_plan.initialize : stage + 1 == dispatches ? guard_plan.finalize : guard_plan.validation_chunks[stage - 1];
		const uint32_t groups = stage == 0 || stage + 1 == dispatches ? 1 : guard_plan.validation_groups_x[stage - 1];
		const RID pipeline = guard->get_pipeline(mode), set = slot.guard_uniform_sets[mode];
		unlock_mutex_for_call();
		const auto list = rd->compute_list_begin();
		RDG::RecordHandle dispatch, closed;
		if (list != RD::INVALID_ID) {
			rd->compute_list_bind_compute_pipeline(list, pipeline);
			rd->compute_list_bind_uniform_set(list, set, 0);
			rd->compute_list_set_push_constant(list, &push, sizeof(push));
			dispatch = rd->_compute_list_dispatch_recorded(list, groups, 1, 1);
			closed = rd->_compute_list_end_recorded();
		}
		relock_mutex_after_call();
		if (list == RD::INVALID_ID || !dispatch.is_valid() || !closed.is_valid() || rd->compute_list.active || rd->_recording_error() != OK) {
			observed.error = ERR_CANT_CREATE;
			observed.state = closed.is_valid() ? CheckedRecordAppend::State::KnownPrefix : CheckedRecordAppend::State::Unknown;
			observed.handle = closed;
		} else {
			observed.handle = closed;
		}
		if (closed.is_valid() && production_capture_graph_locked(*request, *rd, ordinal, observed) != OK) {
			observed.state = CheckedRecordAppend::State::Unknown;
			observed.error = ERR_INVALID_DATA;
		}
		if (observed.error == OK && observed.state == CheckedRecordAppend::State::Appended) { acquire.output_guard_dispatch_cursor = stage + 1; }
	}
	rd->_production_output_guard_end_locked();
	rd->production_recording_request = nullptr;
	--request->capture_inflight;
	request->compute_list_open = rd->compute_list.active;
	rd->_thread_safe_.unlock();
	if (request->source_device_lost || request->compute_list_open || observed.state == CheckedRecordAppend::State::Unknown) {
		request->effects_pending = true;
		observed.state = CheckedRecordAppend::State::Unknown;
	} else if (observed.error == OK && observed.state == CheckedRecordAppend::State::Appended) {
		acquire.output_guard_complete = true;
		const uint64_t words[] = { instance, request->serial, ordinal, record_serial };
		for (uint32_t word = 0; word < 4; ++word) {
			for (uint32_t byte = 0; byte < 8; ++byte) {
				acquire.output_record_token.bytes[word * 8 + byte] = uint8_t(words[word] >> (byte * 8));
			}
		}
		++request->next_action;
		out->record_token = acquire.output_record_token;
	} else if (!(request->native_batch_enabled && production_native_batch_frame_locked(*request) &&
			observed.state == CheckedRecordAppend::State::KnownPrefix && (observed.error == OK || observed.error == ERR_BUSY))) {
		request->cancelled = true;
	}
	acquire.output_guard_append = finalizer.original_append = observed;
	++request->revision;
	out->state = observed.state == CheckedRecordAppend::State::Appended ? GDEXTENSION_PRODUCTION_APPENDED_V1 : observed.state == CheckedRecordAppend::State::Unknown ? GDEXTENSION_PRODUCTION_UNKNOWN_V1 : GDEXTENSION_PRODUCTION_KNOWN_PREFIX_V1;
	out->custody = acquire.output_guard_complete ? GDEXTENSION_OUTPUT_CUSTODY_ACCEPTED_V1 : GDEXTENSION_OUTPUT_CUSTODY_RETAINED_PREFIX_V1;
	out->status = output_status(observed.error, "output_finalize_guard");
}

bool Registry::production_output_records_written_locked(const ProductionRequest &request, const ProductionActionCell &action, uint32_t cutoff) const {
	if (cutoff > request.next_action || cutoff > request.action_count || action.request != &request || action.declaration.action_ordinal >= cutoff) {
		return false;
	}
	// A declaration is not a Records/Meta write receipt. Cover each complete
	// writable with successful checked effects of this original operation after
	// Acquire. The interval scan accepts split writes without allocating a map.
	for (const auto &role : action.output_roles) {
		uint64_t covered = 0;
		while (covered < role.bytes) {
			uint64_t end = covered;
			for (uint32_t a = uint32_t(action.declaration.action_ordinal) + 1; a < cutoff; ++a) {
				const auto &writer = request.actions[a];
				const auto &append = writer.original_append;
				if (writer.request != &request || !writer.prepared || (writer.declaration.action_kind != GDEXTENSION_PRODUCTION_DISPATCH_V1 && writer.declaration.action_kind != GDEXTENSION_PRODUCTION_UPLOAD_V1 && writer.declaration.action_kind != GDEXTENSION_PRODUCTION_COPY_V1) || !append.handle.is_valid() || append.state != CheckedRecordAppend::State::Appended || !append.operation_bound || append.operation.owner_request_nonce != request.operation.owner_request_nonce || append.operation.library_generation != request.attachment->library_generation || append.operation.qualification_session_serial != device_records[request.attachment->device].session || append.operation.record_generation != request.operation.record_generation || append.operation.record_range_begin != request.operation.canonical_begin || append.operation.record_range_end != request.operation.canonical_end) {
					continue;
				}
				if (writer.declaration.first_use > request.use_count || writer.declaration.use_count > request.use_count - writer.declaration.first_use) {
					return false;
				}
				for (uint32_t u = 0; u < writer.declaration.use_count; ++u) {
					const auto &use = request.uses[writer.declaration.first_use + u].declaration;
					uint64_t written_offset = use.byte_offset, written_bytes = use.byte_size;
					if (writer.declaration.action_kind == GDEXTENSION_PRODUCTION_UPLOAD_V1) {
						if (writer.upload.destination_slot != role.resource_slot) {
							continue;
						}
						written_offset = writer.upload.destination_offset;
						written_bytes = writer.upload.bytes;
					} else if (writer.declaration.action_kind == GDEXTENSION_PRODUCTION_COPY_V1) {
						if (writer.copy.destination_slot != role.resource_slot) {
							continue;
						}
						written_offset = writer.copy.destination_offset;
						written_bytes = writer.copy.bytes;
					}
					if (use.resource_slot == role.resource_slot && (use.access == GDEXTENSION_PRODUCTION_WRITE_V1 || use.access == GDEXTENSION_PRODUCTION_READ_WRITE_V1 || use.access == GDEXTENSION_PRODUCTION_ATOMIC_V1) && written_offset <= covered && written_bytes <= role.bytes && written_offset <= role.bytes - written_bytes) {
						end = MAX(end, written_offset + written_bytes);
					}
				}
			}
			if (end == covered) {
				return false;
			}
			covered = end;
		}
	}
	return true;
}

void Registry::production_output_seal(ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const GDExtensionOutputSealInputV1 *input, GDExtensionOutputSealResultV1 *out) {
	if (!out || !input || uintptr_t(out) % alignof(GDExtensionOutputSealResultV1) || uintptr_t(input) % alignof(GDExtensionOutputSealInputV1)) {
		return;
	}
	*out = {};
	out->abi = production_dto_abi<GDExtensionOutputSealResultV1>();
	out->status = output_status(ERR_INVALID_PARAMETER, "output_seal_input");
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_physical_request_locked(context, ref, request, rd, true);
	if (error != OK) {
		out->status = output_status(error, "original_record_context");
		return;
	}
	if (!production_dto_valid(*input) || !input->particles) {
		return;
	}
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	auto *particles = storage ? storage->particles_owner.get_or_null(RID::from_uint64(input->particles)) : nullptr;
	if (!particles || !particles->external || particles->external->device != rd || particles->external->generation_registry != this || particles->external->generation != input->common.generation) {
		out->status = output_status(ERR_UNAUTHORIZED, "original_output_owner");
		return;
	}
	auto &owner = *particles->external;
	using Owner = RendererRD::ExternalOutputOwner;
	uint32_t slot_index = UINT32_MAX;
	for (uint32_t i = 0; i < owner.slot_count; ++i) {
		if (owner.slots[i].lease_token.size() == sizeof(input->lease_token.bytes) && memcmp(owner.slots[i].lease_token.ptr(), input->lease_token.bytes, sizeof(input->lease_token.bytes)) == 0) {
			slot_index = i;
			break;
		}
	}
	if (slot_index == UINT32_MAX) {
		out->status = output_status(ERR_INVALID_DATA, "original_output_lease");
		return;
	}
	auto &slot = owner.slots[slot_index];
	if (slot.original_registry != this || slot.original_request.registry_instance != instance || slot.original_request.handle_serial != request->serial || slot.original_action_ordinal >= request->action_count || !production_output_pin_matches_locked(slot.original_pin, ref, *rd)) {
		out->status = output_status(ERR_UNAUTHORIZED, "original_output_slot");
		return;
	}
	auto &action = request->actions[slot.original_action_ordinal];
	GDExtensionOutputAcquireInputV1 supplied = action.output_input;
	supplied.particles = input->particles;
	supplied.common = input->common;
	RendererRD::ExternalOutputRequest parsed;
	if (action.request != request || action.declaration.action_ordinal != slot.original_action_ordinal || action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || !action.prepared || action.output_state != ProductionActionCell::OutputState::BOUND || action.output_particles.get_id() != input->particles || action.output_generation != owner.generation || action.output_slot != slot_index || action.output_lease != slot.lease_serial || action.output_root != slot.original_pin.serial || output_decode(supplied, parsed) != OK || !output_input_equal(action.output_input, supplied)) {
		out->status = output_status(ERR_INVALID_DATA, "output_seal_action_mismatch");
		return;
	}
	// A successor may carry a previous consumed-tuple token only when this
	// view's immediately retained predecessor has a genuine native color
	// adoption receipt. The token is correlation data; the predecessor slot
	// remains the authority and must still be held by the original owner.
	if (parsed.common.has_previous_consumed_tuple) {
		GDExtensionOutputTokenV1 previous_token{};
		if (parsed.common.previous_consumed_tuple_token.size() != sizeof(previous_token.bytes)) {
			out->status = output_status(ERR_INVALID_DATA, "previous_consumed_tuple_token");
			return;
		}
		memcpy(previous_token.bytes, parsed.common.previous_consumed_tuple_token.ptr(), sizeof(previous_token.bytes));
		const uint32_t view_index = owner.config.max_inflight ? slot_index / owner.config.max_inflight : UINT32_MAX;
		if (view_index >= owner.config.max_views || !owner.views[view_index].occupied || owner.views[view_index].closed ||
				owner.views[view_index].tag != parsed.common.view_tag || owner.views[view_index].epoch != parsed.view_epoch ||
				owner.views[view_index].history_slot == UINT32_MAX ||
				!owner.native_color_consumption_matches(owner.views[view_index].history_slot, previous_token)) {
			out->status = output_status(ERR_UNAVAILABLE, "previous_consumed_tuple_required");
			return;
		}
	}
	const OriginalOperationIdentity expected_operation{ request->operation.owner_request_nonce, request->attachment->library_generation, device_records[request->attachment->device].session, request->operation.record_generation, request->operation.canonical_begin, request->operation.canonical_end };
	if (!action.output_plan_prepared || action.output_plan.seal_action_ordinal >= request->action_count || action.output_plan.finalize_action_ordinal >= action.output_plan.seal_action_ordinal || memcmp(action.output_record_token.bytes, input->record_token.bytes, 32) != 0 || !action.output_guard_complete || action.output_guard_append.state != CheckedRecordAppend::State::Appended || !action.output_guard_append.operation_bound || !same_original_operation_identity(action.output_guard_append.operation, expected_operation)) {
		out->status = output_status(ERR_UNAUTHORIZED, "output_finalize_receipt_required");
		return;
	}
	auto &seal = request->actions[action.output_plan.seal_action_ordinal];
	if (seal.request != request || !seal.prepared || seal.output_acquire_action_ordinal != slot.original_action_ordinal || seal.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || seal.declaration.canonical_suboperation != action.output_plan.seal_canonical_suboperation || seal.declaration.source_recipe_substep != action.output_plan.seal_source_recipe_substep) {
		out->status = output_status(ERR_INVALID_DATA, "output_seal_frozen_row");
		return;
	}
	if (slot.state == Owner::LeaseState::SEALED_RECORDED || slot.state == Owner::LeaseState::ADOPTED) {
		if (seal.original_append.state != CheckedRecordAppend::State::Appended || slot.record_token.size() != 32 || slot.packet_token.size() != 32 || memcmp(slot.record_token.ptr(), input->record_token.bytes, 32) != 0 || memcmp(slot.packet_token.ptr(), action.output_packet_token.bytes, 32) != 0) {
			out->status = output_status(ERR_INVALID_DATA, "output_seal_duplicate_mismatch");
			return;
		}
		out->state = uint32_t(slot.state) + 1;
		out->packet_token = action.output_packet_token;
		out->status = output_status(OK, "");
		return;
	}
	error = production_output_scene_validate_locked(action, action.output_input);
	if (error != OK) {
		out->status = output_status(error, "output_seal_committed_scene");
		return;
	}
	if (!owner.admits_work() || slot.publication_closed || slot.pass_uses_closed || (slot.state != Owner::LeaseState::RESERVED && slot.state != Owner::LeaseState::WRITING)) {
		out->status = output_status(ERR_BUSY, "output_seal_closed");
		return;
	}
	if (!request->frozen || request->capture_inflight || request->cancelled || request->release_started || request->effects_pending || request->source_device_lost || request->next_action != action.output_plan.seal_action_ordinal || request->draw_list_open || request->compute_list_open || !rd->_thread_safe_.try_lock()) {
		out->status = output_status(ERR_BUSY, "output_seal_original_order");
		return;
	}
	bool source_matches = !rd->draw_list.active && !rd->compute_list.active && !rd->raytracing_list.active && rd->_recording_error() == OK && production_output_source_matches_locked(*request, action, *rd);
	for (uint32_t g = 0; g < request->graph_count; ++g) {
		uint32_t command = UINT32_MAX;
		source_matches &= rd->draw_graph._resolve_record_command_index(request->graphs[g].handle, command);
	}
	for (const auto &role : action.output_roles) {
		if (role.bytes) {
			source_matches &= role.resource_slot < request->resource_count && production_output_alias_matches_locked(*request, request->resources[role.resource_slot], *rd);
		}
	}
	rd->_thread_safe_.unlock();
	if (!source_matches || !production_output_records_written_locked(*request, action, uint32_t(action.output_plan.finalize_action_ordinal))) {
		out->status = output_status(ERR_UNAUTHORIZED, "output_seal_original_source");
		return;
	}
	if (action.preinput_camera_source.is_null() || !slot.retained_host_snapshot || !action.preinput_host_prepared || action.preinput_host_returned || memcmp(action.preinput_host_transform.host_token.bytes, input->common.current_host_token.bytes, 32) != 0) {
		out->status = output_status(ERR_UNAVAILABLE, "output_original_capture_required");
		return;
	}
	const GDExtensionOutputTokenV1 absent{};
	if (memcmp(action.output_packet_token.bytes, absent.bytes, 32) == 0) {
		uint64_t receipt = 0;
		if (!serials.take(receipt)) {
			out->status = output_status(ERR_PARAMETER_RANGE_ERROR, "output_packet_serial");
			return;
		}
		const uint64_t words[] = { instance, request->serial, action.output_plan.seal_action_ordinal, receipt };
		for (uint32_t word = 0; word < 4; ++word) {
			for (uint32_t byte = 0; byte < 8; ++byte) {
				action.output_packet_token.bytes[word * 8 + byte] = uint8_t(words[word] >> (byte * 8));
			}
		}
	}
	error = owner.seal_recorded_slot(slot_index, action.preinput_camera_source, action.output_record_token, action.output_packet_token);
	if (error != OK) {
		// Finalize already owns real graph prefixes. Commit retry must neither
		// reissue those graphs nor replace its immutable registered record token.
		out->status = output_status(error, "output_packet_commit");
		return;
	}
	OriginalOperationIdentity identity{ request->operation.owner_request_nonce, request->attachment->library_generation, device_records[request->attachment->device].session, request->operation.record_generation, request->operation.canonical_begin, request->operation.canonical_end };
	CRASH_COND(bind_checked_record_append(seal.original_append, identity) != OK);
	seal.original_append.state = CheckedRecordAppend::State::Appended;
	seal.original_append.error = OK;
	++request->next_action;
	++request->revision;
	CRASH_COND(production_output_native_append_enter_locked() != OK);
	out->state = uint32_t(slot.state) + 1;
	out->packet_token = action.output_packet_token;
	out->status = output_status(OK, "");
}
} // namespace GDExtensionDeviceExit

namespace GDExtensionDeviceExit {
Error Registry::production_output_read_begin_locked(RendererRD::ExternalOutputOwner &owner, uint32_t index, uint32_t graph_limit) {
	using Owner = RendererRD::ExternalOutputOwner;
	if (!owner.device || !owner.device->native_immutable_is_render_thread() || index >= owner.read_capacity || !owner.admits_work()) {
		return ERR_UNAUTHORIZED;
	}
	auto &read = owner.read_cells[index];
	if (read.slot >= owner.slot_count || read.state != Owner::ReadCell::State::RESERVED || !read.consumer || read.admitted ||
			(read.consumption_kind == Owner::ReadCell::ConsumptionKind::RASTER ?
					(read.preparation >= owner.preparation_capacity || graph_limit < 2) :
					(read.consumption_kind != Owner::ReadCell::ConsumptionKind::EMPTY_COLOR || read.preparation != UINT32_MAX || graph_limit != 1)) ||
			graph_limit > Owner::ReadCell::GRAPH_CAPACITY) {
		return ERR_INVALID_PARAMETER;
	}
	auto &slot = owner.slots[read.slot];
	if (slot.original_registry != this || slot.publication_closed || slot.pass_uses_closed ||
			(slot.state != Owner::LeaseState::SEALED_RECORDED && slot.state != Owner::LeaseState::ADOPTED) ||
			!production_output_pin_matches_locked(read.pin, slot.original_request, *owner.device) ||
			!production_output_pin_matches_locked(slot.original_pin, slot.original_request, *owner.device) ||
			!read.camera_retained || read.camera_source.is_null() || read.camera_source != slot.native_camera_source) {
		return ERR_UNAUTHORIZED;
	}
	const Error admitted = production_output_pin_new_work_matches_locked(read.pin, *owner.device);
	if (admitted != OK) {
		return admitted;
	}
	auto &request = *read.pin.request;
	if (slot.original_action_ordinal >= request.action_count || request.next_action != request.action_count ||
			request.capture_inflight || request.compute_list_open || request.draw_list_open || request.effects_pending || !request.graph_count ||
			request.revision > UINT64_MAX - uint64_t(request.capacity.graph_memberships) * 2 - uint64_t(request.capacity.native_memberships) * 2 - request.capacity.readbacks - request.capacity.resources - 8) {
		return ERR_BUSY;
	}
	// New raster reads may share the still-open native list. Require every
	// original producer graph ordered; later read graphs carry no new recipe.
	for (uint32_t i = 0; i < request.graph_count; ++i) {
		uint32_t command = UINT32_MAX;
		if (!request.graphs[i].output_read && !owner.device->draw_graph._resolve_record_command_index(request.graphs[i].handle, command)) {
			return ERR_BUSY;
		}
	}
	auto &action = request.actions[slot.original_action_ordinal];
	if (!production_output_source_matches_locked(request, action, *owner.device)) {
		return ERR_UNAUTHORIZED;
	}
	const bool empty = read.consumption_kind == Owner::ReadCell::ConsumptionKind::EMPTY_COLOR;
	bool color = false;
	if (action.request != &request || action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 ||
			action.output_root != slot.original_pin.serial || action.output_slot != read.slot || action.output_lease != slot.lease_serial ||
			read.surface >= slot.surface_count) { return ERR_UNAUTHORIZED; }
	if (empty) {
		const OriginalOperationIdentity expected{ request.operation.owner_request_nonce, request.attachment->library_generation,
			device_records[request.attachment->device].session, request.operation.record_generation, request.operation.canonical_begin, request.operation.canonical_end };
		if (owner.config.profile != RendererRD::ExternalOutputConfig::INSTANCE_192 || owner.config.capacity || !slot.storage ||
				!slot.retained_host_snapshot || !slot.guard_resources_ready || !action.output_guard_complete ||
				action.output_guard_append.state != CheckedRecordAppend::State::Appended || action.output_guard_append.error != OK ||
				!action.output_guard_append.operation_bound || !same_original_operation_identity(action.output_guard_append.operation, expected) ||
				read.raster_bound || read.draw_accepted || read.transfer_count || read.graph_count ||
				!owner.generation_surfaces_complete || read.surface >= owner.generation_material_count) { return ERR_UNAUTHORIZED; }
		for (uint32_t role : { uint32_t(RendererRD::ExternalOutputStorage::CURRENT_RECORDS), uint32_t(RendererRD::ExternalOutputStorage::PREVIOUS_RECORDS) }) {
			const auto &records = slot.storage->resources[role];
			if (records.bytes || records.rid.is_valid() || records.witness.cell.is_valid()) { return ERR_UNAUTHORIZED; }
		}
		const auto &material = owner.generation_materials[read.surface];
		const uint32_t mask = read.empty_color_pass == uint32_t(RendererRD::ExternalPacketPass::OPAQUE) ? RendererRD::ExternalOutputConfig::PASS_OPAQUE :
				read.empty_color_pass == uint32_t(RendererRD::ExternalPacketPass::TRANSPARENT) ? RendererRD::ExternalOutputConfig::PASS_TRANSPARENT : 0;
		if (!mask || !(slot.request.pass_mask & mask) || !(owner.config.pass_mask & mask) ||
				(material.material.is_null() || !material.generation)) { return ERR_UNAUTHORIZED; }
		color = true;
	} else {
		const auto &row = owner.preparation_rows[read.preparation];
		if (action.request != &request || action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 ||
				action.output_root != slot.original_pin.serial || action.output_slot != read.slot || action.output_lease != slot.lease_serial ||
				row.owner != &owner || row.owner_slot != read.slot || row.tuple_surface != read.surface || read.surface >= slot.surface_count ||
				row.tuple_state != RendererRD::ExternalPipelinePreparation::TupleState::READY || !row.finish_complete ||
				row.finish_error != OK || row.backend_error != OK || !row.pipeline_sealed || !row.geometry_adopted || !row.material_adopted ||
				!row.material_transferred || row.selected_geometry >= 2 || !row.geometry_transferred[row.selected_geometry] ||
				row.created_pipeline.is_null() || row.transforms_uniform_set.is_null()) {
			return ERR_UNAUTHORIZED;
		}
		const auto *pipeline = owner.device->render_pipeline_owner.get_or_null(row.created_pipeline);
		// Uniform set binds are deferred CPU state. The draw emits at most one
		// prepare + one descriptor bind per original set (PREFIX_CAPACITY=32).
		// Outside that prefix: pipeline, VA, IA, push, draw and close = six.
		const uint64_t required_graphs = pipeline ? uint64_t(pipeline->set_formats.size()) * 2 + 6 : UINT64_MAX;
		if (required_graphs > graph_limit) {
			return ERR_OUT_OF_MEMORY; // Before spending the read or touching RD.
		}
		color = row.selection.pass == RendererRD::ExternalPacketPass::OPAQUE || row.selection.pass == RendererRD::ExternalPacketPass::TRANSPARENT;
	}
	if (action.output_read_cursor >= action.output_read_capacity || graph_limit > action.output_read_graph_capacity ||
			request.graph_count > request.capacity.graph_memberships || graph_limit > request.capacity.graph_memberships - request.graph_count) {
		return ERR_OUT_OF_MEMORY;
	}
	uint32_t native_bound = 0;
	uint32_t uncaptured_graphs = request.capacity.graph_memberships - request.graph_count;
	for (uint32_t i = 0; i < request.graph_count; ++i) {
		const auto &graph = request.graphs[i];
		bool pending = graph.pending;
		for (uint32_t t = 0; t < graph.output_read_transfer_count; ++t) {
			pending |= graph.output_read_transfers[t].pending;
		}
		// A submitted draw can still owe a later original transfer alias.
		// Reserve that graph's worst-case native join until both are captured.
		uncaptured_graphs += pending;
	}
	const Error bounded = production_membership_bound_locked(*owner.device, uncaptured_graphs, request.native_workers_bound, native_bound);
	if (bounded != OK || request.native_count > request.capacity.native_memberships || native_bound > request.capacity.native_memberships - request.native_count) {
		return bounded != OK ? bounded : ERR_OUT_OF_MEMORY;
	}
	// Reserve the original correlation before any native effect. An exhausted
	// issuer refuses this read rather than allocating a receipt after drawing.
	if (color && !slot.native_color_consumption_serial && !serials.take(read.color_consumption_serial)) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	// Consume once, before the first effect. No action/range/shader progress.
	read.read_ordinal = action.output_read_cursor++;
	read.graph_limit = graph_limit;
	read.admitted = true;
	read.capture_complete = true;
	++request.revision;
	return OK;
}

Error Registry::production_output_read_capture_locked(RendererRD::ExternalOutputOwner &owner, uint32_t index, const CheckedRecordAppend &append, bool draw) {
	using Owner = RendererRD::ExternalOutputOwner;
	if (!owner.device || index >= owner.read_capacity || !owner.device->native_immutable_is_render_thread()) {
		return ERR_UNAUTHORIZED;
	}
	auto &read = owner.read_cells[index];
	if (read.consumption_kind != Owner::ReadCell::ConsumptionKind::RASTER || !read.admitted || read.slot >= owner.slot_count || read.close_graph != UINT32_MAX || read.pin.is_empty()) {
		return ERR_INVALID_DATA;
	}
	auto &slot = owner.slots[read.slot];
	if (slot.original_registry != this || !production_output_pin_matches_locked(read.pin, slot.original_request, *owner.device)) {
		return ERR_UNAUTHORIZED;
	}
	auto &request = *read.pin.request;
	// This is capture of an already executed native operation, including its
	// failure prefix. Cancellation cannot turn missing capture into no-use.
	if (append.state == CheckedRecordAppend::State::Rejected && !append.handle.is_valid() && !append.transfer_count && !append.became_mutable) {
		return append.error;
	}
	if (append.state == CheckedRecordAppend::State::Unknown || append.transfer_count || !append.handle.is_valid() ||
			read.graph_count >= read.graph_limit - 1 || append.handle.kind != 3) {
		// Transfers use the paid ReadCell ledger at the actual RD bind site.
		// An unjoined raw receipt must never silently drop a transfer member.
		read.capture_complete = false;
		read.uncaptured_effect = true;
		request.effects_pending = true;
		read.state = Owner::ReadCell::State::UNKNOWN;
		return ERR_UNAVAILABLE;
	}
	const OriginalOperationIdentity expected_operation{ request.operation.owner_request_nonce, request.attachment->library_generation,
		device_records[request.attachment->device].session, request.operation.record_generation, request.operation.canonical_begin, request.operation.canonical_end };
	if (!append.operation_bound || !same_original_operation_identity(append.operation, expected_operation)) {
		read.capture_complete = false;
		read.uncaptured_effect = true;
		request.effects_pending = true;
		read.state = Owner::ReadCell::State::UNKNOWN;
		return ERR_UNAUTHORIZED;
	}
	const uint32_t before = request.graph_count;
	const Error captured = production_capture_graph_locked(request, *owner.device, slot.original_action_ordinal, append);
	if (captured != OK) {
		read.capture_complete = false;
		read.uncaptured_effect = true;
		request.effects_pending = true;
		read.state = Owner::ReadCell::State::UNKNOWN;
		return captured;
	}
	uint32_t graph = UINT32_MAX;
	for (uint32_t i = 0; i < request.graph_count; ++i) {
		const auto &handle = request.graphs[i].handle;
		if (handle.graph_instance == append.handle.graph_instance && handle.graph_frame == append.handle.graph_frame &&
				handle.kind == append.handle.kind && handle.index == append.handle.index && handle.offset == append.handle.offset) {
			graph = i;
			break;
		}
	}
	CRASH_COND(graph == UINT32_MAX || request.graph_count > before + 1);
	request.graphs[graph].output_read |= read.raster_bound;
	bool existing = false;
	for (uint32_t i = 0; i < read.graph_count; ++i) {
		existing |= read.graph_indices[i] == graph;
	}
	if (!existing) {
		read.graph_indices[read.graph_count++] = graph;
	}
	if (draw && append.state == CheckedRecordAppend::State::Appended && append.error == OK) {
		read.draw_graph = graph;
		read.draw_accepted = true;
	}
	read.state = read.capture_complete ? Owner::ReadCell::State::RECORDED : Owner::ReadCell::State::UNKNOWN;
	return append.error;
}

Error Registry::production_output_read_close_locked(RendererRD::ExternalOutputOwner &owner, uint32_t index, const CheckedRecordAppend &append) {
	if (!owner.device || !owner.device->native_immutable_is_render_thread() || index >= owner.read_capacity) {
		return ERR_UNAUTHORIZED;
	}
	auto &read = owner.read_cells[index];
	if (!read.admitted || read.slot >= owner.slot_count || read.pin.is_empty() || read.close_graph != UINT32_MAX) {
		return ERR_INVALID_DATA;
	}
	const auto &slot = owner.slots[read.slot];
	if (slot.original_registry != this || !production_output_pin_matches_locked(read.pin, slot.original_request, *owner.device)) {
		return ERR_UNAUTHORIZED;
	}
	auto &request = *read.pin.request;
	const OriginalOperationIdentity expected_operation{ request.operation.owner_request_nonce, request.attachment->library_generation,
		device_records[request.attachment->device].session, request.operation.record_generation, request.operation.canonical_begin, request.operation.canonical_end };
	const bool original = append.operation_bound && same_original_operation_identity(append.operation, expected_operation);
	bool containing = owner.device->draw_graph.is_draw_list_closed_by(read.list_begin, append.handle);
	for (uint32_t i = 0; i < read.graph_count; ++i) {
		const uint32_t graph = read.graph_indices[i];
		containing &= graph < request.graph_count && owner.device->draw_graph.is_instruction_in_closed_list(request.graphs[graph].handle, append.handle);
	}
	if ((read.consumption_kind == RendererRD::ExternalOutputOwner::ReadCell::ConsumptionKind::EMPTY_COLOR &&
				(read.graph_count || read.graph_limit != 1 || read.raster_bound || read.draw_accepted || read.transfer_count)) ||
			!original || append.state != CheckedRecordAppend::State::Appended || append.error != OK || append.transfer_count ||
			append.handle.kind != 7 || !containing || read.graph_count >= read.graph_limit) {
		read.capture_complete = false;
		read.uncaptured_effect = true;
		request.effects_pending = true;
		return ERR_UNAVAILABLE;
	}
	// A real list close shared by reads of the same request is one original
	// graph fact. Each read references it; do not recapture under another parent.
	uint32_t graph = UINT32_MAX;
	for (uint32_t i = 0; i < request.graph_count; ++i) {
		const auto &handle = request.graphs[i].handle;
		if (handle.graph_instance == append.handle.graph_instance && handle.graph_frame == append.handle.graph_frame &&
				handle.kind == append.handle.kind && handle.index == append.handle.index && handle.offset == append.handle.offset) {
			graph = i;
			break;
		}
	}
	if (graph == UINT32_MAX) {
		graph = request.graph_count;
		const Error captured = production_capture_graph_locked(request, *owner.device, slot.original_action_ordinal, append);
		if (captured != OK) {
			read.capture_complete = false;
			read.uncaptured_effect = true;
			request.effects_pending = true;
			return captured;
		}
	}
	if (read.draw_accepted && (read.draw_graph >= request.graph_count ||
			!owner.device->draw_graph.is_draw_in_closed_list(request.graphs[read.draw_graph].handle, append.handle))) {
		read.capture_complete = false;
		return ERR_INVALID_DATA;
	}
	request.graphs[graph].output_read |= read.raster_bound || read.consumption_kind == RendererRD::ExternalOutputOwner::ReadCell::ConsumptionKind::EMPTY_COLOR;
	read.graph_indices[read.graph_count++] = graph;
	read.close_graph = graph;
	if (read.consumption_kind == RendererRD::ExternalOutputOwner::ReadCell::ConsumptionKind::EMPTY_COLOR) {
		read.state = RendererRD::ExternalOutputOwner::ReadCell::State::RECORDED;
	}
	return OK;
}

void Registry::production_output_read_adopt_locked(RendererRD::ExternalOutputOwner &owner, uint32_t index) {
	if (index >= owner.read_capacity || !owner.device) { return; }
	auto &read = owner.read_cells[index];
	if (read.slot >= owner.slot_count || read.pin.is_empty() || read.close_graph == UINT32_MAX || !read.list_linked) { return; }
	auto &slot = owner.slots[read.slot];
	if (slot.original_registry != this || !production_output_pin_matches_locked(read.pin, slot.original_request, *owner.device)) { return; }
	auto &request = *read.pin.request;
	const uint32_t graph = read.close_graph;
	using Kind = RendererRD::ExternalOutputOwner::ReadCell::ConsumptionKind;
	const bool empty = read.consumption_kind == Kind::EMPTY_COLOR;
	const bool raster = read.consumption_kind == Kind::RASTER && read.raster_bound && read.draw_accepted && read.preparation < owner.preparation_capacity;
	if ((empty || raster) && read.capture_complete && !read.uncaptured_effect &&
			read.surface < slot.surface_count && !slot.publication_closed && !slot.pass_uses_closed && !request.effects_pending &&
			production_output_pin_new_work_matches_locked(read.pin, *owner.device) == OK) {
		bool color = false;
		if (empty) {
			color = !owner.config.capacity && read.preparation == UINT32_MAX && !read.raster_bound && !read.draw_accepted && !read.transfer_count &&
					(read.empty_color_pass == uint32_t(RendererRD::ExternalPacketPass::OPAQUE) || read.empty_color_pass == uint32_t(RendererRD::ExternalPacketPass::TRANSPARENT));
		} else {
			const auto &row = owner.preparation_rows[read.preparation];
			color = (row.selection.pass == RendererRD::ExternalPacketPass::OPAQUE || row.selection.pass == RendererRD::ExternalPacketPass::TRANSPARENT) &&
					row.owner == &owner && row.owner_slot == read.slot && row.tuple_surface == read.surface;
		}
		if (color) {
			auto &surface = owner.slots[read.slot].surfaces[read.surface];
			if (empty) {
				if (surface.color_empty_close_graph == UINT32_MAX) { surface.color_empty_close_graph = graph; }
			} else if (surface.color_close_graph == UINT32_MAX) {
				surface.color_draw_graph = read.draw_graph;
				surface.color_close_graph = graph;
			}
			bool covered = slot.surface_count != 0;
			for (uint32_t i = 0; i < slot.surface_count; ++i) {
				covered &= empty ? slot.surfaces[i].color_empty_close_graph != UINT32_MAX :
						(slot.surfaces[i].color_draw_graph != UINT32_MAX && slot.surfaces[i].color_close_graph != UINT32_MAX);
			}
			auto &mutable_slot = owner.slots[read.slot];
			if (covered && mutable_slot.state == RendererRD::ExternalOutputOwner::LeaseState::SEALED_RECORDED) {
				const uint32_t view_index = read.slot / owner.config.max_inflight;
				auto &view = owner.views[view_index];
				if (view.occupied && !view.closed && view.tag == slot.request.common.view_tag && view.epoch == slot.request.view_epoch) {
					// A late close never replaces a more recent consumed tuple.
					const bool newer_history = view.history_slot != UINT32_MAX && view.history_slot < owner.slot_count &&
							owner.slots[view.history_slot].request.common.present_seq > slot.request.common.present_seq;
					if (!mutable_slot.native_color_consumption_serial) {
						// Only this actual final covering color read can publish its
						// prepaid native receipt. No public token or bool issues it.
						if (!read.color_consumption_serial) {
							return;
						}
						if (slot.original_action_ordinal >= request.action_count) { return; }
						const auto &action = request.actions[slot.original_action_ordinal];
						if (action.request != &request || action.output_particles.is_null() || action.output_generation != owner.generation ||
								action.output_slot != read.slot || action.output_lease != slot.lease_serial) { return; }
						mutable_slot.native_color_consumption_particles = action.output_particles;
						const uint64_t words[] = { instance, request.serial, slot.original_action_ordinal, read.color_consumption_serial };
						for (uint32_t word = 0; word < 4; ++word) {
							for (uint32_t byte = 0; byte < 8; ++byte) {
								mutable_slot.native_color_consumption_token.bytes[word * 8 + byte] = uint8_t(words[word] >> (byte * 8));
							}
						}
						mutable_slot.native_color_consumption_serial = read.color_consumption_serial;
						mutable_slot.native_color_empty = empty;
						mutable_slot.native_color_read_serial = read.serial;
						mutable_slot.native_color_read_cell = index;
						mutable_slot.native_color_close_graph = graph;
					}
					mutable_slot.state = RendererRD::ExternalOutputOwner::LeaseState::ADOPTED;
					if (!newer_history) {
						view.history_slot = read.slot;
						auto &tuple = view.consumed[read.slot % owner.config.max_inflight];
						tuple.token = mutable_slot.native_color_consumption_token;
						const auto &camera = slot.request.common.current_camera_token;
						tuple.camera = { camera.view_id, camera.view_epoch, camera.capture_ordinal };
						tuple.present = slot.request.common.present_seq;
						tuple.epoch = slot.request.view_epoch; tuple.valid = true;
					}
				}
			}
		}
	}
}

Error Registry::production_output_reads_settle(RendererRD::ExternalOutputOwner &owner, uint32_t slot_index,
		const GDExtensionProductionServiceBudgetV1 &budget, ProductionServiceUsage &usage) {
	using Owner = RendererRD::ExternalOutputOwner;
	Lock lock(*this);
	if (!owner.device || !owner.device->native_immutable_is_render_thread() || slot_index >= owner.slot_count) {
		return ERR_UNAUTHORIZED;
	}
	auto &slot = owner.slots[slot_index];
	if (slot.original_registry != this || !production_output_pin_matches_locked(slot.original_pin, slot.original_request, *owner.device)) {
		return ERR_UNAUTHORIZED;
	}
	auto &request = *slot.original_pin.request;
	auto &control = request.control;
	if (control.state != ProductionControlState::EXECUTING || control.input.kind != ProductionControlKind::OBSERVE ||
			control.input.request_serial != request.serial || &budget != &control.input.budget || &usage != &control.output.service_usage ||
			request.capture_inflight != 1 || request.continuation_inflight) {
		return ERR_UNAUTHORIZED;
	}
	if (request.source_device_lost || request.effects_pending) {
		return ERR_BUSY;
	}
	// Fixed metadata scan; persist the cursor so sparse original cells cannot
	// starve behind earlier UNKNOWN rows. Each slot has its own cursor: other
	// slots cannot consume its scan window. No native polling occurs here.
	for (uint32_t visit = 0; visit < MIN(owner.read_capacity, 64u); ++visit) {
		if (!budget.controls || usage.passes >= MIN(budget.passes, 64u) || usage.receipts >= MIN(budget.receipts, 128u) ||
				!budget.deadline_micros || OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros) {
			return ERR_BUSY;
		}
		const uint32_t index = slot.read_settle_cursor;
		slot.read_settle_cursor = (index + 1) % owner.read_capacity;
		auto &read = owner.read_cells[index];
		if (read.slot != slot_index || read.pin.is_empty() || read.consumer) {
			continue;
		}
		++usage.passes;
		usage.controls = 1;
		if (!production_output_pin_matches_locked(read.pin, slot.original_request, *owner.device)) {
			return ERR_INVALID_DATA;
		}
		if (!read.admitted || !read.capture_complete || read.uncaptured_effect || !read.graph_count ||
				read.close_graph == UINT32_MAX || (read.state != Owner::ReadCell::State::RECORDED && read.state != Owner::ReadCell::State::UNKNOWN)) {
			continue;
		}
		// One paid maintenance pass examines at most64 metadata entries and
		// checks the original deadline between every O(1) graph/native visit.
		// Cursors belong to this exact paid read, not a rescanned G*N product.
		uint32_t metadata_visits = 0;
		while (read.settle_graph_cursor < read.graph_count && metadata_visits < 64) {
			if (OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros) {
				return ERR_BUSY;
			}
			++metadata_visits;
			const uint32_t graph_index = read.graph_indices[read.settle_graph_cursor];
			if (graph_index >= request.graph_count || request.graphs[graph_index].request != &request) {
				return ERR_INVALID_DATA;
			}
			if (request.graphs[graph_index].pending) {
				return ERR_BUSY;
			}
			for (uint32_t t = 0; t < request.graphs[graph_index].output_read_transfer_count; ++t) {
				if (OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros) { return ERR_BUSY; }
				auto &dependency = request.graphs[graph_index].output_read_transfers[t];
				if (!dependency.pending) { continue; }
				if (dependency.worker >= owner.device->transfer_worker_pool.size() || !owner.device->transfer_worker_pool[dependency.worker]) { return ERR_INVALID_DATA; }
				auto *worker = owner.device->transfer_worker_pool[dependency.worker];
				if (!worker->thread_mutex.try_lock()) { return ERR_BUSY; }
				if (!worker->operations_mutex.try_lock()) { worker->thread_mutex.unlock(); return ERR_BUSY; }
				// This monotonic original owner frontier advances only after its
				// real source/fence return. A caller bool/phase never writes it.
				if (worker->operations_processed >= dependency.operation) { dependency.pending = false; }

				worker->operations_mutex.unlock();
				worker->thread_mutex.unlock();
				if (dependency.pending) { return ERR_BUSY; }
			}
			while (read.settle_native_cursor < request.native_count && metadata_visits < 64) {
				if (OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros) {
					return ERR_BUSY;
				}
				++metadata_visits;
				const uint32_t native_index = read.settle_native_cursor;
				if (request.natives[native_index].fact.membership_parent != graph_index) {
					++read.settle_native_cursor;
					continue;
				}
				read.settle_member_found = true;
				const ProductionNativeCell *native = nullptr;
				const Error resolved = production_native_owner_locked(request, native_index, native);
				if (resolved != OK) {
					return resolved;
				}
				const bool complete = native->member.source.issue == RDD::IssueFact::NotIssued
						? !native->member.retained && !native->member.owner_released
						: native->member.retained && !native->member.owner_released && native->observation.state == NativeSourceObservation::State::Complete;
				if (!complete) {
					return ERR_BUSY; // Revisit this original owner on next OBSERVE.
				}
				++read.settle_native_cursor;
			}
			if (read.settle_native_cursor == request.native_count) {
				if (!read.settle_member_found) {
					return ERR_BUSY;
				}
				++read.settle_graph_cursor;
				read.settle_native_cursor = 0;
				read.settle_member_found = false;
			}
		}
		if (read.settle_graph_cursor != read.graph_count) {
			return ERR_BUSY;
		}
		// The exact predecessor camera/source pin was borrowed with this read.
		// Release it only after all original read graphs/native owners completed,
		// and before clearing the metadata cell. A failed return retains custody.
		if (!read.previous_pin.is_empty()) {
			const Error previous = production_output_unpin_locked(read.previous_pin);
			if (previous != OK) {
				return previous;
			}
		}
		const Error returned = production_output_unpin_locked(read.pin);
		if (returned != OK) {
			return returned;
		}
		owner.read_clear_returned(read);
		return OK; // One original read return per bounded service pass.
	}
	return OK;
}
// Additive V2 transport on the original request/control/output owner only.
namespace {
template <class A, class B>
bool consumed_disjoint(const A *a, const B *b) {
	const uintptr_t x = uintptr_t(a), y = uintptr_t(b);
	return x <= UINTPTR_MAX - sizeof(A) && y <= UINTPTR_MAX - sizeof(B) &&
			(x + sizeof(A) <= y || y + sizeof(B) <= x);
}
template <class T>
bool consumed_aligned(const T *p) { return p && uintptr_t(p) % alignof(T) == 0 && uintptr_t(p) <= UINTPTR_MAX - sizeof(T); }
bool consumed_ref_matches(const GDExtensionOutputConsumedProofRefV2 &a, const GDExtensionOutputConsumedProofRefV2 &b) {
	return production_dto_valid(a) && production_dto_valid(b) && a.registry_instance == b.registry_instance &&
			a.request_serial == b.request_serial && a.borrow_control_serial == b.borrow_control_serial && a.action_ordinal == b.action_ordinal &&
			a.particles == b.particles && a.generation == b.generation && a.lease_serial == b.lease_serial &&
			a.view_cell_serial == b.view_cell_serial && a.slot_index == b.slot_index && a.view_cell_index == b.view_cell_index &&
			!memcmp(a.lease_token.bytes, b.lease_token.bytes, sizeof(a.lease_token.bytes)) &&
			!memcmp(a.original_color_token.bytes, b.original_color_token.bytes, sizeof(a.original_color_token.bytes));
}
GDExtensionDeviceSessionResultV1 consumed_result(Error error) { return { int32_t(error), 0, 0, 0 }; }
GDExtensionProductionHostTransformV1 consumed_host(const Transform3D &host) {
	GDExtensionProductionHostTransformV1 out{};
	out.abi = production_dto_abi<GDExtensionProductionHostTransformV1>();
	const Transform3D inverse = host.affine_inverse();
	for (uint32_t row = 0; row < 3; ++row) {
		for (uint32_t col = 0; col < 3; ++col) {
			out.affine[row * 4 + col] = host.basis[row][col];
			out.inverse[row * 4 + col] = inverse.basis[row][col];
		}
		out.affine[row * 4 + 3] = host.origin[row];
		out.inverse[row * 4 + 3] = inverse.origin[row];
	}
	return out;
}
} // namespace

GDExtensionDeviceSessionResultV1 Registry::output_consumed_prepare_v2(GDExtensionProductionRequestRefV1 ref, const GDExtensionOutputConsumedSelectorV2 *selector) {
	const Error contract = production_contract_check(ref, 2);
	if (contract != OK) { return consumed_result(contract); }
	if (!consumed_aligned(selector) || !production_dto_valid(*selector)) { return consumed_result(ERR_INVALID_PARAMETER); }
	Lock lock(*this);
	ProductionRequest *request = nullptr; RenderingDevice *rd = nullptr;
	const Error resolved = production_request_locked(ref, true, request, rd);
	if (resolved != OK) { return consumed_result(resolved); }
	if (request->frozen || request->cancelled || request->control.state != ProductionControlState::OWNER_CONSUMED || request->continuation_inflight) { return consumed_result(ERR_BUSY); }
	if (selector->action_ordinal >= request->action_count) { return consumed_result(ERR_INVALID_PARAMETER); }
	auto &action = request->actions[selector->action_ordinal];
	if (action.request != request || action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || !action.output_source_prepared || (action.output_acquire_action_ordinal != UINT64_MAX && (!action.output_plan_prepared || action.output_acquire_action_ordinal != selector->action_ordinal)) || action.output_state != ProductionActionCell::OutputState::UNBOUND) { return consumed_result(ERR_UNAUTHORIZED); }
	action.consumed_proof_requested = true;
	return consumed_result(OK);
}

// Registry+RD held, original paid OBSERVE, executing on the native thread.
Error Registry::output_consumed_service_locked(ProductionRequest &request, RenderingDevice &rd) {
	auto &control = request.control;
	const auto &input = control.input;
	const auto &budget = input.budget;
	if (!rd.native_immutable_is_render_thread() || control.state != ProductionControlState::EXECUTING ||
			input.kind != ProductionControlKind::OBSERVE || input.request_serial != request.serial ||
			(input.consumed_operation != 1 && input.consumed_operation != 2)) { return ERR_UNAUTHORIZED; }
	if (!budget.controls || !budget.passes || !budget.receipts || !budget.deadline_micros ||
			OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros || request.capture_inflight || request.continuation_inflight) { return ERR_BUSY; }
	if (input.consumed_action_ordinal >= request.action_count) { return ERR_INVALID_PARAMETER; }
	auto &action = request.actions[input.consumed_action_ordinal];
	if (action.request != &request || action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 ||
			!action.consumed_proof_requested || !action.output_generation || !action.output_lease || action.output_particles.is_null()) { return ERR_UNAUTHORIZED; }
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	auto *particles = storage ? storage->particles_owner.get_or_null(action.output_particles) : nullptr;
	if (!particles || !particles->external) { return ERR_DOES_NOT_EXIST; }
	auto &owner = *particles->external;
	if (owner.device != &rd || owner.generation != action.output_generation || action.output_slot >= owner.slot_count) { return ERR_INVALID_DATA; }
	auto &slot = owner.slots[action.output_slot];
	auto &view = slot.consumed_proof_view;
	if (slot.original_registry != this || slot.original_request.registry_instance != instance ||
			slot.original_request.handle_serial != request.serial || slot.original_action_ordinal != input.consumed_action_ordinal ||
			slot.lease_serial != action.output_lease || !production_output_pin_matches_locked(slot.original_pin, slot.original_request, rd) ||
			view.is_empty() || !owner.preparation_view_matches(view, action.output_slot, true)) { return ERR_UNAUTHORIZED; }
	auto &cell = owner.view_cells[view.view_cell];
	if (cell.camera_source.is_null() || cell.camera_source != slot.native_camera_source ||
			(input.consumed_operation == 1 && cell.camera_source != action.preinput_camera_source) || !slot.retained_host_snapshot ||
			!owner.native_color_consumption_matches(action.output_slot, slot.native_color_consumption_token) ||
			slot.native_color_consumption_particles != action.output_particles || slot.lease_token.size() != 32) { return ERR_UNAVAILABLE; }
	control.output.service_usage = { 1, 1, 1 };
	GDExtensionOutputConsumedProofV2 proof{};
	proof.abi = production_dto_abi<GDExtensionOutputConsumedProofV2>();
	auto &ref = proof.ref;
	ref.abi = production_dto_abi<GDExtensionOutputConsumedProofRefV2>();
	ref.registry_instance = instance;
	ref.request_serial = request.serial;
	ref.borrow_control_serial = input.consumed_operation == 1 ? control.serial : slot.consumed_borrow_serial;
	ref.action_ordinal = input.consumed_action_ordinal;
	ref.particles = action.output_particles.get_id();
	ref.generation = owner.generation;
	ref.lease_serial = slot.lease_serial;
	ref.view_cell_serial = view.serial;
	ref.slot_index = action.output_slot;
	ref.view_cell_index = view.view_cell;
	memcpy(ref.lease_token.bytes, slot.lease_token.ptr(), 32);
	ref.original_color_token = slot.native_color_consumption_token;
	if (input.consumed_operation == 1) {
		if (slot.consumed_borrow_serial || action.consumed_proof_returned) { return ERR_ALREADY_IN_USE; }
		const Error admission = production_output_pin_new_work_matches_locked(cell.pin, rd);
		if (admission != OK) { return admission; }
		proof.original_acquire = action.output_input;
		const Error camera = production_camera_sample(**cell.camera_source, proof.original_camera);
		if (camera != OK) { return camera; }
		proof.current_host = consumed_host(slot.current_host);
		proof.current_host.host_token = action.preinput_host_transform.host_token;
		proof.previous_host = consumed_host(slot.previous_host);
		// This assignment is the sole export edge. Its exact original ViewCell
		// and strong pin were reserved before native append, not allocated here.
		slot.consumed_borrow_serial = control.serial;
		control.output.consumed_proof = proof;
		control.output.consumed_result = true;
		return OK;
	}
	if (!slot.consumed_borrow_serial || action.consumed_proof_returned ||
			!consumed_ref_matches(ref, input.consumed_expected) ||
			!consumed_ref_matches(ref, action.consumed_read_ref)) { return ERR_INVALID_DATA; }
	// Same explicit view_return operation under the already-held Registry lock.
	// This returns one metadata pin, never the slot root or any native use.
	const Error returned = production_output_unpin_locked(cell.pin);
	if (returned != OK) { return returned; }
	cell.camera_source.unref(); cell.consumer = nullptr; cell.serial = 0; cell.slot = UINT32_MAX;
	view.owner = nullptr; view.serial = 0; view.slot = view.view_cell = UINT32_MAX; view.retained_host_snapshot = false;
	action.consumed_proof_returned = true;
	control.output.consumed_result = true;
	return OK;
}

// Admission/peek into the ORIGINAL fixed control. Never executes inline.
Error Registry::output_consumed_control_v2(GDExtensionProductionRequestRefV1 ref, uint32_t operation, uint64_t ordinal,
		const GDExtensionOutputConsumedProofRefV2 *expected, const GDExtensionProductionServiceBudgetV1 &budget, uint64_t &serial) {
	Lock lock(*this);
	ProductionRequest *request = nullptr; RenderingDevice *rd = nullptr;
	const Error resolved = production_request_locked(ref, false, request, rd);
	if (resolved != OK) { return resolved; }
	if (request->continuation_inflight || ordinal >= request->action_count) { return ERR_BUSY; }
	auto &cell = request->control;
	if (cell.state != ProductionControlState::OWNER_CONSUMED) {
		if (cell.input.kind != ProductionControlKind::OBSERVE || cell.input.consumed_operation != operation ||
				cell.input.consumed_action_ordinal != ordinal || (expected && !consumed_ref_matches(cell.input.consumed_expected, *expected))) { return ERR_BUSY; }
		if (cell.state == ProductionControlState::PENDING &&
				(!cell.input.budget.controls || !cell.input.budget.receipts || !cell.input.budget.passes ||
				 !cell.input.budget.deadline_micros || OS::get_singleton()->get_ticks_usec() >= cell.input.budget.deadline_micros)) {
			// This exact immutable turn never published. Reconcile only its
			// no-effect result; a retry cannot renew its retained authorization.
			cell.output = {};
			cell.output.request_serial = request->serial;
			cell.output.control_serial = cell.serial;
			cell.output.result = consumed_result(ERR_BUSY);
			production_report_locked(*request, cell.output.report);
			cell.state = ProductionControlState::RETURNED;
		}
		serial = cell.serial;
		return OK;
	}
	if (!budget.controls || !budget.receipts || !budget.passes || !budget.deadline_micros || OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros) { return ERR_BUSY; }
	auto &action = request->actions[ordinal];
	if (action.request != request || action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || !action.output_lease) { return ERR_UNAUTHORIZED; }
	if (operation == 1) {
		if (request->cancelled || request->release_started || request->source_device_lost || request->attachment->closing || action.consumed_read_ref.borrow_control_serial || action.consumed_proof_returned) { return ERR_UNAVAILABLE; }
	} else if (!expected || action.consumed_proof_returned || !consumed_ref_matches(action.consumed_read_ref, *expected)) { return ERR_INVALID_DATA; }
	if (!request->next_control_serial) { return ERR_PARAMETER_RANGE_ERROR; }
	request->release_ready = {};
	cell = {};
	cell.serial = request->next_control_serial;
	request->next_control_serial = cell.serial == UINT64_MAX ? 0 : cell.serial + 1;
	cell.input = { request->serial, request->operation.record_generation, ProductionControlKind::OBSERVE, budget, UINT32_MAX, false };
	cell.input.consumed_operation = operation;
	cell.input.consumed_action_ordinal = ordinal;
	if (expected) { cell.input.consumed_expected = *expected; }
	cell.state = ProductionControlState::PENDING;
	serial = cell.serial;
	return OK;
}

GDExtensionDeviceSessionResultV1 Registry::output_consumed_borrow_v2(GDExtensionProductionRequestRefV1 request, const GDExtensionOutputConsumedSelectorV2 *selector,
		const GDExtensionProductionServiceBudgetV1 *budget, GDExtensionOutputConsumedBorrowResultV2 *out) {
	const Error contract = production_contract_check(request, 2);
	if (contract != OK) { return consumed_result(contract); }
	if (!consumed_aligned(selector) || !consumed_aligned(budget) || !consumed_aligned(out) || !production_dto_valid(*selector) ||
			!production_dto_valid(*budget) || budget->reserved || !consumed_disjoint(selector, out) || !consumed_disjoint(budget, out)) { return consumed_result(ERR_INVALID_PARAMETER); }
	uint64_t serial = 0;
	const Error accepted = output_consumed_control_v2(request, 1, selector->action_ordinal, nullptr, *budget, serial);
	if (accepted != OK) { return consumed_result(accepted); }
	{
		Lock lock(*this);
		ProductionRequest *original = nullptr; RenderingDevice *rd = nullptr;
		const Error resolved = production_request_locked(request, false, original, rd);
		if (resolved != OK) { return consumed_result(resolved); }
		auto &cell = original->control;
		if (cell.serial != serial) { return consumed_result(ERR_INVALID_DATA); }
		if (cell.state == ProductionControlState::RETURNED) {
			GDExtensionOutputConsumedBorrowResultV2 projected{};
			projected.abi = production_dto_abi<GDExtensionOutputConsumedBorrowResultV2>();
			projected.result = cell.output.result;
			projected.report = cell.output.report;
			projected.proof = cell.output.consumed_proof;
			if (!cell.output.consumed_result) { cell.state = ProductionControlState::OWNER_CONSUMED; }
			*out = projected;
			return projected.result;
		}
	}
	if (!budget->controls || !budget->receipts || !budget->passes || !budget->deadline_micros || OS::get_singleton()->get_ticks_usec() >= budget->deadline_micros) { return consumed_result(ERR_BUSY); }
	const Error scheduled = production_control_schedule(request, serial);
	return consumed_result(scheduled == OK ? ERR_BUSY : scheduled);
}

GDExtensionDeviceSessionResultV1 Registry::output_consumed_read_v2(GDExtensionProductionRequestRefV1 ref,
		const GDExtensionOutputConsumedProofRefV2 *expected, GDExtensionOutputConsumedProofV2 *out) {
	const Error contract = production_contract_check(ref, 2);
	if (contract != OK) { return consumed_result(contract); }
	if (!consumed_aligned(expected) || !consumed_aligned(out) || !production_dto_valid(*expected) || !consumed_disjoint(expected, out)) { return consumed_result(ERR_INVALID_PARAMETER); }
	Lock lock(*this);
	ProductionRequest *request = nullptr; RenderingDevice *rd = nullptr;
	const Error resolved = production_request_locked(ref, false, request, rd);
	if (resolved != OK) { return consumed_result(resolved); }
	auto &cell = request->control;
	if (!expected->borrow_control_serial || cell.serial != expected->borrow_control_serial || cell.state == ProductionControlState::OWNER_CONSUMED) { return consumed_result(ERR_DOES_NOT_EXIST); }
	if (cell.state != ProductionControlState::RETURNED) { return consumed_result(ERR_BUSY); }
	if (cell.input.kind != ProductionControlKind::OBSERVE || cell.input.consumed_operation != 1 ||
			cell.input.request_serial != request->serial || cell.output.request_serial != request->serial || cell.output.control_serial != cell.serial ||
			cell.output.result.native_error != OK || !cell.output.consumed_result || !consumed_ref_matches(cell.output.consumed_proof.ref, *expected) ||
			expected->registry_instance != instance || expected->request_serial != request->serial || expected->action_ordinal >= request->action_count) { return consumed_result(ERR_INVALID_DATA); }
	auto &action = request->actions[expected->action_ordinal];
	if (action.request != request || action.consumed_read_ref.borrow_control_serial || action.consumed_proof_returned) { return consumed_result(ERR_INVALID_DATA); }
	action.consumed_read_ref = *expected;
	*out = cell.output.consumed_proof;
	cell.state = ProductionControlState::OWNER_CONSUMED;
	return consumed_result(OK);
}

GDExtensionDeviceSessionResultV1 Registry::output_consumed_return_v2(GDExtensionProductionRequestRefV1 request,
		const GDExtensionOutputConsumedProofRefV2 *expected, const GDExtensionProductionServiceBudgetV1 *budget, GDExtensionProductionReportV2 *out) {
	const Error contract = production_contract_check(request, 2);
	if (contract != OK) { return consumed_result(contract); }
	if (!consumed_aligned(expected) || !consumed_aligned(budget) || !consumed_aligned(out) || !production_dto_valid(*expected) ||
			!production_dto_valid(*budget) || budget->reserved || !consumed_disjoint(expected, out) || !consumed_disjoint(budget, out)) { return consumed_result(ERR_INVALID_PARAMETER); }
	uint64_t serial = 0;
	const Error accepted = output_consumed_control_v2(request, 2, expected->action_ordinal, expected, *budget, serial);
	if (accepted != OK) { return consumed_result(accepted); }
	{
		Lock lock(*this);
		ProductionRequest *original = nullptr; RenderingDevice *rd = nullptr;
		const Error resolved = production_request_locked(request, false, original, rd);
		if (resolved != OK) { return consumed_result(resolved); }
		auto &cell = original->control;
		if (cell.serial != serial) { return consumed_result(ERR_INVALID_DATA); }
		if (cell.state == ProductionControlState::RETURNED) {
			const auto result = cell.output.result;
			*out = cell.output.report;
			cell.state = ProductionControlState::OWNER_CONSUMED;
			return result;
		}
	}
	if (!budget->controls || !budget->receipts || !budget->passes || !budget->deadline_micros || OS::get_singleton()->get_ticks_usec() >= budget->deadline_micros) { return consumed_result(ERR_BUSY); }
	const Error scheduled = production_control_schedule(request, serial);
	return consumed_result(scheduled == OK ? ERR_BUSY : scheduled);
}

} // namespace GDExtensionDeviceExit
