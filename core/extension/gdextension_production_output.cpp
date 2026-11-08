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
#include "servers/rendering/renderer_rd/storage_rd/external_output_owner.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/rendering_device.h"

namespace GDExtensionDeviceExit {
thread_local ProductionOutputPreparationScope *Registry::production_output_preparation_scope = nullptr;

ProductionOutputPreparationScope::~ProductionOutputPreparationScope() {
	if (registry) {
		registry->production_output_preparation_end(*this);
	}
}

Error Registry::production_output_preparation_validate_locked(ProductionOutputPin &pin, RenderingDevice &rd, uint32_t action_ordinal, uint32_t surface_ordinal, uint64_t group, RID *&root, RendererRD::ExternalPipelinePreparation *&preparation, const ProductionOutputPreparationScope *scope) {
	if (!rd.is_main_instance || RenderingDevice::get_singleton() != &rd || rd.render_thread_id != Thread::get_caller_id() ||
			!production_output_pin_matches_locked(pin, { instance, pin.request_serial }, rd)) {
		return ERR_UNAUTHORIZED;
	}
	auto &request = *pin.request;
	auto &attachment = *request.attachment;
	const auto &record = records[attachment.library];
	const auto &native = device_records[attachment.device];
	if (phase != Phase::RUNNING || !request.frozen || request.cancelled || request.release_started || request.source_device_lost || request.effects_pending || request.capture_inflight ||
			attachment.closing || !attachment.opened || !attachment.holder || attachment.holder->pending ||
			attachment.holder->generation != attachment.library_generation || attachment.holder->handle_serial != record.producer_code_holder ||
			!record.occupied || !record.attached || record.quiescing || record.close_started || record.close_error != OK ||
			record.guard_enrollment != GuardEnrollment::QUALIFIED || record.snapshot.library_generation != attachment.library_generation ||
			record.producer_registration_serial != attachment.producer_serial || !record.producer_attachment_identity || !native.live || rd.production_finalization_started.load(std::memory_order_acquire) ||
			request.control.state != ProductionControlState::OWNER_CONSUMED ||
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
				action.output_lease, surface_ordinal, group, pin, rd, original_root, preparation, scope) ||
			!original_root || preparation->owner->slots[action.output_slot].original_registry != this || preparation->owner->slots[action.output_slot].original_action_ordinal != action_ordinal || !RendererRD::ParticlesStorage::_production_output_owner_matches(ProductionOutputOwnerKind::SLOT, action.output_particles, action.output_generation, action.output_slot, action.output_lease, pin, rd, { instance, request.serial })) {
		return ERR_UNAUTHORIZED;
	}
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
		} else {
			return ERR_ALREADY_IN_USE;
		}
	}
	if (headers != 1 || members != 1 || original_root->is_valid()) {
		return ERR_ALREADY_IN_USE;
	}
	root = original_root;
	return OK;
}

Error Registry::production_output_preparation_begin(ProductionOutputPin &pin, RenderingDevice &rd, uint32_t action_ordinal, uint32_t surface_ordinal, uint64_t group, ProductionOutputPreparationScope &scope) {
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
	const Error error = production_output_preparation_validate_locked(pin, rd, action_ordinal, surface_ordinal, group, root, row);
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
			action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || action.output_state != ProductionActionCell::OutputState::BOUND ||
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
				scope.original_lease, scope.surface_ordinal, scope.group, *scope.pin, rd, root, row, &scope, true) ||
			!RendererRD::ParticlesStorage::_production_output_owner_matches(ProductionOutputOwnerKind::SLOT, scope.original_particles, scope.original_generation, scope.original_slot, scope.original_lease, *scope.pin, rd, { instance, scope.request_serial }) ||
			root != scope.original_root || row != scope.row || row->owner->slots[scope.original_slot].original_registry != this ||
			row->owner->slots[scope.original_slot].original_action_ordinal != scope.action_ordinal || row->source_geometry != scope.original_geometry ||
			row->source_material != scope.original_material || row->source_material_generation != scope.original_material_generation) {
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
	const Error error = production_output_preparation_validate_locked(*scope.pin, rd, scope.action_ordinal, scope.surface_ordinal, scope.group, root, row, &scope);
	if (error != OK) {
		return error;
	}
	if (root != scope.original_root || row != scope.row) {
		return ERR_UNAUTHORIZED;
	}
	request = retained;
	return OK;
}

void Registry::production_output_preparation_end(ProductionOutputPreparationScope &scope) {
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	CRASH_COND(!scope.device || production_output_preparation_cleanup_matches_locked(scope, *scope.device, request) != OK);
	// This ends only the synchronous extent. The stable row, original pin,
	// group, root and failed/successful native prefixes stay in their owner.
	// Mandatory batch capture/adopt/seal is a separate, still-required step.
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
Error output_decode(const GDExtensionOutputAcquireInputV1 &input, RendererRD::ExternalOutputRequest &out) {
	const auto &common = input.common;
	if (!production_dto_valid(input) || input.reserved || common.reserved || !input.particles || !common.generation || common.history_reset > 10 || common.has_actual_previous_camera > 1 || common.has_previous_consumed_tuple > 1 || (input.pass_mask & ~RendererRD::ExternalOutputConfig::PASS_KNOWN_MASK) || !common.current_camera.view_id || !common.current_camera.view_epoch || !common.current_camera.capture_ordinal) {
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
		if (resource.request != &request || resource.request_serial != request.serial || resource.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || !resource.declaration.original_generation || resource.output_alias_prepared || !resource.imported || resource.retiring || resource.returned || resource.use_count != 1 || !resource.uses[0].same_physical() || !resource.uses[0].linked() || resource.uses[0].physical->use_closed.load() || !buffer || buffer->qualification_return != resource.uses[0].physical || buffer->backing_serial != resource.backing_serial || buffer->size != resource.declaration.physical_bytes || use.declaration.byte_offset > buffer->size || use.declaration.byte_size > buffer->size - use.declaration.byte_offset) {
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

GDExtensionDeviceSessionResultV1 Registry::production_prepare_output(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOutputPrepareV1 *input) {
	auto result = [](Error error) -> GDExtensionDeviceSessionResultV1 { return { int32_t(error), error == OK ? 0u : 16u, 0, 0 }; };
	if (!input || uintptr_t(input) % alignof(GDExtensionProductionOutputPrepareV1) || !production_dto_valid(*input) || !production_dto_valid(input->input) || input->reserved || input->input.reserved || !input->source_use_count || !input->source_generation || ((!input->read_capacity) != (!input->read_graph_capacity))) {
		return result(ERR_INVALID_PARAMETER);
	}
	RendererRD::ExternalOutputRequest decoded;
	if (output_decode(input->input, decoded) != OK) {
		return result(ERR_INVALID_PARAMETER);
	}
	Lock lock(*this);
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
	auto &source = request->resources[input->source_resource_slot];
	const auto &source_use = request->uses[input->source_use_index];
	if (source.request != request || source.request_serial != request->serial || source.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || source.declaration.original_generation != input->source_generation || !source.imported || source.retiring || source.use_count != 1 || !source.uses[0].same_physical() || !source.uses[0].linked() || source.uses[0].physical->use_closed.load() || source_use.request != request || source_use.declaration.resource_slot != input->source_resource_slot || source_use.declaration.access != GDEXTENSION_PRODUCTION_READ_V1 || !source_use.declaration.byte_size) {
		return result(ERR_INVALID_DATA);
	}
	ProductionActionCell source_check;
	source_check.declaration = action->declaration;
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
		if (resource.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || resource.declaration.ownership != GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1 || resource.declaration.original_generation != role.allocation_generation || resource.declaration.physical_bytes != role.bytes || resource.imported || resource.owned_serial || resource.use_count || use.request != request || use.declaration.resource_slot != role.resource_slot || use.declaration.byte_offset || use.declaration.byte_size != role.bytes || use.declaration.access != GDEXTENSION_PRODUCTION_WRITE_V1 || (resource.output_alias_prepared && resource.output_action_ordinal != input->action_ordinal)) {
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
		bool same = action->output_source == &source && action->output_source_use == input->source_use_index && action->output_first_source_use == input->first_source_use && action->output_source_use_count == input->source_use_count && action->output_source_generation == input->source_generation && action->output_read_capacity == input->read_capacity && action->output_read_graph_capacity == input->read_graph_capacity && output_input_equal(action->output_input, input->input);
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
	action->prepared = true;
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
	const uint64_t per_row_uses = uint64_t(action.output_read_graph_capacity) * 2 + 4;
	if (reserve_pipelines && uint64_t(action.output_read_capacity) > (UINT64_MAX - borrower_headroom - 1u - (uint64_t(layout.resource_count) + 1)) / per_row_uses) {
		out->status = output_status(ERR_PARAMETER_RANGE_ERROR, "output_preparation_quote_overflow");
		return;
	}
	const uint64_t required_uses = 1u + (layout.resource_count ? uint64_t(layout.resource_count) + 1 : 0) + (reserve_pipelines ? uint64_t(action.output_read_capacity) * per_row_uses + borrower_headroom : 0);
	const uint32_t preparation_count = layout.resource_count ? (reserve_pipelines ? action.output_read_capacity : surfaces) : 0;
	if (preparation_count > owner.free_preparation_rows()) {
		out->status = output_status(ERR_OUT_OF_MEMORY, "output_global_preparation_bank");
		return;
	}
	if (spare < required_uses || action.output_read_capacity > owner.read_capacity || request->graph_count > request->capacity.graph_memberships || uint64_t(action.output_read_capacity) * action.output_read_graph_capacity > request->capacity.graph_memberships - request->graph_count) {
		out->status = output_status(ERR_OUT_OF_MEMORY, "output_prequoted_capacity");
		return;
	}
	const uint64_t required_groups = (layout.resource_count ? 1u : 0u) + (reserve_pipelines ? uint64_t(action.output_read_capacity) * 3 : 0u);
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
	slot.lease_serial = owner.next_lease_serial;
	owner.next_lease_serial = owner.next_lease_serial == UINT64_MAX ? 0 : owner.next_lease_serial + 1;
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
		bool prefix = slot.owned_group != 0;
		for (uint32_t i = 0; i < slot.surface_count; ++i) {
			prefix |= slot.surfaces[i].pipeline_owned_group != 0 || slot.surfaces[i].created_pipeline.is_valid();
		}
		for (uint32_t i = 0; i < owner.preparation_capacity; ++i) {
			const auto &row = owner.preparation_rows[i];
			prefix |= row.owner == &owner && row.owner_slot == slot_index && (row.pipeline_owned_group || row.geometry_reserved_group || row.material_reserved_group || row.owned_pipeline_root.is_valid());
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
} // namespace GDExtensionDeviceExit
