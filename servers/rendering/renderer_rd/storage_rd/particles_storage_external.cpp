/**************************************************************************/
/*  particles_storage_external.cpp                                        */
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
#include "servers/rendering/renderer_rd/storage_rd/external_output_owner.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/rendering_server.h"

#include <limits>

namespace RendererRD {
Error ParticlesStorage::_external_guard_copy_host_inverse(const GDExtensionProductionHostTransformV1 &p_host,
		const PackedByteArray &p_original_token, const Transform3D &p_current_host, ParticlesExternalGuard::GeometryProofHeader &r_proof) {
	if (!GDExtensionDeviceExit::production_dto_valid(p_host) || p_original_token.size() != 32 ||
			memcmp(p_host.host_token.bytes, p_original_token.ptr(), 32) != 0 || !p_current_host.is_finite()) {
		return ERR_INVALID_DATA;
	}
	// Validate all lanes before publishing any. This preserves the widened/exact
	// Host result instead of repeating real_t determinant or affine_inverse.
	for (uint32_t row = 0; row < 3; ++row) {
		for (uint32_t column = 0; column < 4; ++column) {
			const uint32_t lane = row * 4 + column;
			const real_t original = column == 3 ? p_current_host.origin[row] : p_current_host.basis.rows[row][column];
			if (!Math::is_finite(p_host.affine[lane]) || !Math::is_finite(p_host.inverse[lane]) || original != real_t(p_host.affine[lane])) {
				return ERR_INVALID_DATA;
			}
		}
	}
	memcpy(r_proof.current_host_inverse_rows, p_host.inverse, sizeof(p_host.inverse));
	return OK;
}
Error ParticlesStorage::_external_guard_copy_history_locked(const GDExtensionDeviceExit::ProductionOutputHistoryBorrow &p_borrow,
		const RD::ProductionOutputUniformSource &p_predecessor_read, RID p_particles, uint32_t p_slot, ExternalGuardHistorySnapshot &r_snapshot) {
	using namespace GDExtensionDeviceExit;
	Particles *particles = particles_owner.get_or_null(p_particles);
	if (!particles || !particles->external || p_slot >= particles->external->slot_count) {
		return ERR_INVALID_PARAMETER;
	}
	auto &owner = *particles->external;
	auto &slot = owner.slots[p_slot];
	auto *registry = slot.original_registry;
	auto *rd = owner.device;
	if (!registry || !rd || !Registry::Lock::owns(*registry) || !rd->native_immutable_is_render_thread() ||
			slot.request.common.history_reset || !slot.request.common.has_previous_consumed_tuple || !slot.storage) {
		return ERR_UNAUTHORIZED;
	}
	// Registry authenticated this stack-only original borrow and READ while
	// holding its lock and the RD mutex. Consume every pointer before return.
	const auto &borrow = p_borrow;
	if (!borrow.previous_view || !borrow.current_host || !borrow.previous_host ||
			!borrow.consumed_proof || !borrow.consumed_tuple ||
			(borrow.previous_no_records ?
					(owner.config.capacity || borrow.previous_records.is_valid() || borrow.previous_records_witness || !borrow.previous_guard_witness ||
							borrow.previous_guard_status.is_null() || p_predecessor_read.buffer != borrow.previous_guard_status ||
							borrow.previous_guard_bytes != 32 || borrow.previous_guard_profile != 1 || p_predecessor_read.byte_offset || p_predecessor_read.byte_size != 32) :
					(!borrow.previous_records_witness || borrow.previous_records.is_null() || p_predecessor_read.buffer != borrow.previous_records)) || p_predecessor_read.owned_group ||
			p_predecessor_read.resource_slot == UINT32_MAX || p_predecessor_read.use_index == UINT32_MAX) {
		return ERR_INVALID_DATA;
	}
	const auto &view = *borrow.previous_view;
	if (view.owner != &owner || view.slot >= owner.slot_count || view.slot == p_slot ||
			!view.has_retained_host_snapshot() || view.get_profile() != ExternalOutputConfig::INSTANCE_192 ||
			view.get_space() != ExternalOutputConfig::WORLD) {
		return ERR_INVALID_DATA;
	}
	const auto &previous = owner.slots[view.slot];
	const uint64_t proof_bytes = sizeof(ParticlesExternalGuard::GeometryProofHeader) +
			uint64_t(slot.surface_count) * sizeof(ParticlesExternalGuard::SurfaceProof);
	if (!previous.storage || !previous.guard_resources_ready || !previous.guard_proof_payload ||
			previous.guard_proof_bytes != proof_bytes || previous.surface_count != slot.surface_count ||
			!slot.surface_count || slot.surface_count > ExternalOutputConfig::MAX_FLATTENED_SURFACES ||
			previous.request.common.current_host_token.size() != 32 ||
			slot.request.common.previous_consumed_tuple_token.size() != 32 ||
			memcmp(borrow.consumed_tuple->bytes, slot.request.common.previous_consumed_tuple_token.ptr(), 32) != 0) {
		return ERR_INVALID_DATA;
	}
	const auto &records = previous.storage->resources[borrow.previous_no_records ? ExternalOutputStorage::GUARD_STATUS : ExternalOutputStorage::CURRENT_RECORDS];
	if (borrow.previous_no_records) {
		for (uint32_t r : { uint32_t(ExternalOutputStorage::CURRENT_RECORDS), uint32_t(ExternalOutputStorage::PREVIOUS_RECORDS) }) {
			const auto &absent = previous.storage->resources[r];
			if (absent.bytes || absent.rid.is_valid()) { return ERR_INVALID_DATA; }
		}
		if (borrow.previous_guard_sequence != previous.request.common.present_seq) { return ERR_INVALID_DATA; }
	}
	const RID retained_buffer = borrow.previous_no_records ? borrow.previous_guard_status : borrow.previous_records;
	const auto *buffer = rd->storage_buffer_owner.get_or_null(retained_buffer);
	NativeDisposalWitness::Report witness;
	Error error = rd->_native_disposal_observe(borrow.previous_no_records ? *borrow.previous_guard_witness : *borrow.previous_records_witness, witness);
	if (error != OK || !buffer || records.rid != retained_buffer || !records.bytes ||
			records.bytes != buffer->size || records.bytes != (borrow.previous_no_records ? 32 : uint64_t(owner.config.capacity) * 192) ||
			(borrow.previous_no_records && (buffer->backing_serial != borrow.previous_guard_backing)) ||
			witness.state != NativeDisposalWitness::State::LIVE || witness.rid != records.rid.get_id() ||
			witness.serial != buffer->backing_serial || !p_predecessor_read.byte_size ||
			p_predecessor_read.byte_offset > buffer->size || p_predecessor_read.byte_size > buffer->size - p_predecessor_read.byte_offset) {
		return error != OK ? error : ERR_INVALID_DATA;
	}
	// Reconstruct only the retained immutable Host POD. The prior action's
	// Host edge may already have returned; the old view is not a Host source.
	Transform3D previous_host;
	for (uint32_t row = 0; row < 3; ++row) {
		for (uint32_t column = 0; column < 3; ++column) {
			previous_host.basis.rows[row][column] = real_t(borrow.previous_host->affine[row * 4 + column]);
		}
		previous_host.origin[row] = real_t(borrow.previous_host->affine[row * 4 + 3]);
	}
	ParticlesExternalGuard::GeometryProofHeader host_proof;
	error = _external_guard_copy_host_inverse(*borrow.previous_host, previous.request.common.current_host_token, previous_host, host_proof);
	if (error != OK) {
		return error;
	}
	ExternalGuardHistorySnapshot snapshot;
	snapshot.predecessor_read = p_predecessor_read;
	ParticlesExternalGuard::GeometryProofHeader previous_proof;
	memcpy(&previous_proof, previous.guard_proof_payload, sizeof(previous_proof));
	memcpy(snapshot.proof.previous_decl_min, previous_proof.current_decl_min, sizeof(snapshot.proof.previous_decl_min));
	memcpy(snapshot.proof.previous_decl_max, previous_proof.current_decl_max, sizeof(snapshot.proof.previous_decl_max));
	memcpy(snapshot.proof.previous_host_inverse_rows, host_proof.current_host_inverse_rows, sizeof(snapshot.proof.previous_host_inverse_rows));
	snapshot.proof.options[0] = 1;
	error = snapshot.surfaces.resize(slot.surface_count);
	if (error != OK) {
		return error;
	}
	for (uint32_t i = 0; i < slot.surface_count; ++i) {
		ParticlesExternalGuard::SurfaceProof original;
		memcpy(&original, previous.guard_proof_payload + sizeof(previous_proof) + uint64_t(i) * sizeof(original), sizeof(original));
		auto &surface = snapshot.surfaces.write[i];
		memcpy(surface.previous_mesh_min, original.current_mesh_min, sizeof(surface.previous_mesh_min));
		memcpy(surface.previous_mesh_max, original.current_mesh_max, sizeof(surface.previous_mesh_max));
		memcpy(surface.previous_displacement_min, original.current_displacement_min, sizeof(surface.previous_displacement_min));
		memcpy(surface.previous_displacement_max, original.current_displacement_max, sizeof(surface.previous_displacement_max));
	}
	snapshot.current_host = *borrow.current_host;
	snapshot.previous_host = previous_host;
	snapshot.request_serial = slot.original_request.handle_serial;
	snapshot.lease_serial = slot.lease_serial;
	r_snapshot = snapshot;
	return OK;
}

Error ParticlesStorage::_external_guard_prepare_owned_prefix(GDExtensionProductionPhysicalContextRefV1 p_context, RID p_particles, uint32_t p_slot,
		const Vector<RD::ProductionOutputUniformSource> &p_source_reads, const uint64_t *p_full_buffer_bytes, const uint32_t *p_source_words,
		const GDExtensionProductionHostTransformV1 &p_host, const ExternalGuardHistorySnapshot *p_history,
		uint32_t p_source_space, uint32_t p_max_groups_x, ParticlesExternalGuard::LayoutPlan &r_plan) {
	using Guard = ParticlesExternalGuard;
	Particles *particles = particles_owner.get_or_null(p_particles);
	if (!particles || !particles->external || p_slot >= ExternalOutputOwner::MAX_SLOTS || p_source_reads.size() != 5 ||
			!p_full_buffer_bytes || !p_source_words || (p_source_space != 1 && p_source_space != 2)) {
		return ERR_INVALID_PARAMETER;
	}
	ExternalOutputOwner &owner = *particles->external;
	ExternalOutputOwner::Slot &slot = owner.slots[p_slot];
	if (!owner.device || (!owner.device->is_main_instance || owner.device->render_thread_id != Thread::get_caller_id()) || !owner.admits_work() || !slot.storage ||
			slot.original_pin.is_empty() || !slot.original_registry || !slot.owned_group || slot.publication_closed ||
			(slot.state != ExternalOutputOwner::LeaseState::RESERVED && slot.state != ExternalOutputOwner::LeaseState::WRITING) ||
			!external_guard || !external_guard->is_prepared() || owner.config.profile != ExternalOutputConfig::INSTANCE_192) {
		return ERR_UNAVAILABLE;
	}
	const bool history = !slot.request.common.history_reset && slot.request.common.has_previous_consumed_tuple;
	if (history != bool(p_history) || (p_history &&
			(p_history->request_serial != slot.original_request.handle_serial || p_history->lease_serial != slot.lease_serial ||
					p_history->surfaces.size() != slot.surface_count || p_history->predecessor_read.buffer.is_null() ||
					p_history->predecessor_read.resource_slot == UINT32_MAX || p_history->predecessor_read.use_index == UINT32_MAX ||
					memcmp(&p_history->current_host, &p_host, sizeof(p_host)) != 0))) {
		return ERR_INVALID_DATA;
	}
	if ((owner.config.motion == ExternalOutputConfig::REQUIRED && !slot.request.common.history_reset && !history) ||
			owner.config.space != ExternalOutputConfig::WORLD || !slot.retained_host_snapshot || !slot.current_host.is_finite() ||
			p_source_space != uint32_t(owner.config.space) || !slot.surface_count || !slot.surfaces ||
			!owner.generation_surfaces_complete || owner.generation_material_count != slot.surface_count) {
		return ERR_UNAVAILABLE;
	}
	ExternalOutputOwner::MetadataLayout layout;
	// CreateV3 has moved the parsed selections into the installed directory.
	Error error = ExternalOutputOwner::plan_metadata_layout(owner.view_capacity, owner.read_capacity, owner.surface_capacity, layout,
			owner.generation_mesh_count, owner.generation_material_count, owner.binding_capacity, owner.storage_capacity, owner.preparation_capacity);
	const uint64_t proof_size = sizeof(Guard::GeometryProofHeader) + uint64_t(slot.surface_count) * sizeof(Guard::SurfaceProof);
	const uint64_t proof_relative = uint64_t(p_slot) * proof_size;
	if (error != OK || owner.metadata_lease.get_data() != &owner || p_slot >= layout.storage_count ||
			layout.total_bytes > owner.metadata_lease.get_size() || proof_relative > layout.guard_proof_bytes ||
			proof_size > layout.guard_proof_bytes - proof_relative || proof_size > UINT32_MAX) {
		return error != OK ? error : ERR_INVALID_DATA;
	}
	auto *payload = static_cast<uint8_t *>(owner.metadata_lease.get_data()) + layout.guard_proof_offset + proof_relative;
	if (slot.guard_proof_payload && (slot.guard_proof_payload != payload || slot.guard_proof_bytes != proof_size)) {
		return ERR_INVALID_DATA;
	}
	Guard::GeometryProofHeader proof = p_history ? p_history->proof : Guard::GeometryProofHeader();
	auto bounds = [](const AABB &p_bounds, float *r_min, float *r_max, bool p_strict) {
		if (!p_bounds.is_finite() || !p_bounds.get_end().is_finite()) {
			return false;
		}
		const Vector3 end = p_bounds.get_end();
		for (uint32_t axis = 0; axis < 3; ++axis) {
			r_min[axis] = float(p_bounds.position[axis]);
			r_max[axis] = float(end[axis]);
			if (!Math::is_finite(r_min[axis]) || !Math::is_finite(r_max[axis]) || (p_strict ? r_min[axis] >= r_max[axis] : r_min[axis] > r_max[axis])) {
				return false;
			}
		}
		return true;
	};
	// The native particles AABB is the configured envelope, never an enlarged
	// declaration made up at Finalize. Local space requires a retained native
	// scene-commit envelope and remains unavailable until that source is bound.
	const AABB cull = owner.config.bounds;
	if (!bounds(slot.request.common.bounds, proof.current_decl_min, proof.current_decl_max, true) ||
			!bounds(cull, proof.cull_min, proof.cull_max, true)) {
		return ERR_INVALID_DATA;
	}
	// Consume the same immutable sample accepted by the original paid action.
	error = _external_guard_copy_host_inverse(p_host, slot.request.common.current_host_token, slot.current_host, proof);
	if (error != OK) {
		return error;
	}
	proof.source_accessor[0] = p_source_words[0];
	proof.source_accessor[3] = p_source_space;
	Guard::Bindings bindings;
	Vector<RD::ProductionOutputUniformSource> sources;
	uint32_t owned_ordinal = 0;
	for (uint32_t binding = 0; binding < Guard::BINDING_COUNT; ++binding) {
		const bool imported = binding == Guard::SOURCE_COUNT || binding >= Guard::SOURCE_COUNT_VALIDITY;
		RD::ProductionOutputUniformSource source;
		if (imported) {
			const uint32_t index = binding == Guard::SOURCE_COUNT ? 0 : binding - Guard::SOURCE_COUNT_VALIDITY + 1;
			source = p_source_reads[index];
			const uint64_t word_byte = uint64_t(p_source_words[index]) * 4;
			if (source.buffer.is_null() || source.owned_group || source.resource_slot == UINT32_MAX || source.use_index == UINT32_MAX ||
					source.byte_size < 4 || source.byte_offset > p_full_buffer_bytes[index] || source.byte_size > p_full_buffer_bytes[index] - source.byte_offset ||
					word_byte < source.byte_offset || word_byte - source.byte_offset > source.byte_size - 4) {
				return ERR_INVALID_DATA;
			}
			bindings.buffers[binding] = { source.buffer, p_full_buffer_bytes[index] };
			if (index) {
				proof.source_gate_words[index - 1] = p_source_words[index];
			}
		} else {
			const uint32_t role = binding <= Guard::OUTPUT_META ? binding : binding - 1;
			const ExternalOutputStorage::Resource &resource = slot.storage->resources[role];
			bindings.buffers[binding] = { resource.rid, resource.bytes };
			if (!resource.bytes) {
				continue;
			}
			source.buffer = resource.rid;
			source.owned_group = slot.owned_group;
			source.owned_ordinal = owned_ordinal++;
			source.byte_size = resource.bytes;
		}
		sources.push_back(source);
	}
	Guard::PushConstant push;
	push.capacity = owner.config.capacity;
	push.sequence_low = uint32_t(slot.request.common.present_seq);
	push.sequence_high = uint32_t(slot.request.common.present_seq >> 32);
	push.surface_count = slot.surface_count;
	push.record_count = push.capacity;
	Guard::LayoutPlan plan;
	error = Guard::plan_layout(push, bindings, proof, p_max_groups_x, plan);
	if (error != OK || sources.size() != plan.uniforms.size()) {
		return error != OK ? error : ERR_INVALID_DATA;
	}
	if (slot.guard_proof_payload && memcmp(payload, &proof, sizeof(proof))) {
		return ERR_INVALID_DATA;
	}
	if (!slot.guard_proof_payload) {
		memcpy(payload, &proof, sizeof(proof));
	}
	for (uint32_t surface = 0; surface < slot.surface_count; ++surface) {
		ExternalOutputOwner::BoundMaterialContact contact;
		error = owner.read_bound_material_contact(surface, contact);
		if (error != OK || !contact.material.zero_displacement) {
			return error != OK ? error : ERR_UNAVAILABLE;
		}
		Guard::SurfaceProof surface_proof = p_history ? p_history->surfaces[surface] : Guard::SurfaceProof();
		if (!bounds(contact.mesh_bounds, surface_proof.current_mesh_min, surface_proof.current_mesh_max, false)) {
			return ERR_INVALID_DATA;
		}
		const uint32_t indices = contact.index_count;
		surface_proof.command[0] = indices != 0;
		surface_proof.command[1] = indices ? indices : contact.vertex_count;
		if (!surface_proof.command[1]) {
			return ERR_INVALID_DATA;
		}
		auto *surface_payload = payload + sizeof(proof) + surface * sizeof(surface_proof);
		if (slot.guard_proof_payload && memcmp(surface_payload, &surface_proof, sizeof(surface_proof))) {
			return ERR_INVALID_DATA;
		}
		if (!slot.guard_proof_payload) {
			memcpy(surface_payload, &surface_proof, sizeof(surface_proof));
		}
		slot.surfaces[surface].command_offset = uint64_t(surface) * 20;
		slot.surfaces[surface].command_extent = 20;
	}
	const uint32_t mode_count = push.capacity ? 3 : 2;
	if (!slot.guard_owned_group) {
		error = owner.device->_production_output_owned_reserve(p_context, slot.original_pin, mode_count, slot.guard_owned_group);
		if (error != OK) {
			return error;
		}
	}
	// Only an admitted original group owns retained proof custody. A failed
	// reservation leaves the paid slab bytes inactive and the slot returnable.
	slot.guard_proof_payload = payload;
	slot.guard_proof_bytes = uint32_t(proof_size);
	const Guard::Mode modes[3] = { plan.initialize_mode, push.capacity ? Guard::VALIDATE : plan.finalize_mode, plan.finalize_mode };
	for (uint32_t ordinal = 0; ordinal < mode_count; ++ordinal) {
		const Guard::Mode mode = modes[ordinal];
		if (slot.guard_uniform_sets[mode].is_valid()) {
			continue; // Preserve the exact original native factory prefix on retry.
		}
		error = owner.device->_production_output_owned_uniform_set_create(p_context, slot.original_pin, slot.guard_owned_group, ordinal,
				VectorView<RD::Uniform>(plan.uniforms.ptr(), plan.uniforms.size()), external_guard->get_shader(mode), 0, slot.guard_uniform_sets[mode], slot.guard_uniform_witnesses[mode],
				VectorView<RD::ProductionOutputUniformSource>(sources.ptr(), sources.size()));
		if (error != OK) {
			return error;
		}
	}
	// The transform set and guard keep the E09-written aligned previous
	// destination. The raw prior records remain the original writer READ source.
	slot.previous_host = p_history ? p_history->previous_host : slot.current_host;
	slot.guard_resources_ready = true;
	r_plan = plan;
	return OK;
}

Error ParticlesStorage::particles_scene_validate_receiver(RID p_particles, uint64_t p_generation, const RenderingDevice *p_device) const {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	if (!particles || !particles->external || !p_generation || !p_device) {
		return ERR_INVALID_PARAMETER;
	}
	const ExternalOutputOwner &owner = *particles->external;
	if (owner.generation != p_generation || owner.device != p_device) {
		return ERR_INVALID_DATA;
	}
	if (!owner.admits_work() || !owner.generation_registry || !owner.generation_root_matches(owner.generation_pin) ||
			!owner.generation_registry->production_output_pin_matches(owner.generation_pin, owner.generation_request, *owner.device)) {
		return ERR_UNAVAILABLE;
	}
	if (!owner.config.query_enabled || !owner.config.query_bounds.is_finite() ||
			!owner.config.query_bounds.get_end().is_finite() || !owner.config.query_bounds.has_volume()) {
		return ERR_INVALID_DATA;
	}
	return OK;
}

namespace {
Dictionary output_error(Error p_error, const StringName &p_detail) {
	Dictionary result;
	result["error"] = p_error;
	result["detail"] = p_detail;
	return result;
}
bool generation_matches(const PackedByteArray &p_bytes, uint64_t p_generation) {
	if (p_bytes.size() != 8) {
		return false;
	}
	uint64_t value = 0;
	for (uint32_t i = 0; i < 8; ++i) {
		value |= uint64_t(p_bytes[i]) << (8 * i);
	}
	return value == p_generation && value != 0;
}

template <size_t N>
GDExtensionOutputStatusV1 output_status(Error p_error, const char (&p_detail)[N]) {
	return { int32_t(p_error), uint32_t(N - 1), p_detail };
}
bool token_matches(const PackedByteArray &p_token, const uint8_t *p_bytes) {
	return p_bytes && p_token.size() == 32 && memcmp(p_token.ptr(), p_bytes, 32) == 0;
}
GDExtensionOutputStatusV1 create_body() {
	// Same actual admission refusal for both callers; no owner/RID is minted.
	return output_status(ERR_UNAVAILABLE, "original_output_admission_required");
}
bool valid_abi(const GDExtensionDeviceExitAbi &p_abi, uint32_t p_size) {
	return p_abi.abi_version == 1 && p_abi.struct_size == p_size && !p_abi.reserved;
}
bool decode_bounds(const GDExtensionOutputBoundsV1 &p_input, AABB &r_bounds, bool p_nonempty) {
	const double real_max = double(std::numeric_limits<real_t>::max());
	for (uint32_t i = 0; i < 3; ++i) {
		if (!Math::is_finite(p_input.position[i]) || !Math::is_finite(p_input.size[i]) ||
				p_input.position[i] < -real_max || p_input.position[i] > real_max || p_input.size[i] < -real_max || p_input.size[i] > real_max) {
			return false;
		}
		const real_t position = real_t(p_input.position[i]);
		const real_t size = real_t(p_input.size[i]);
		if (!Math::is_finite(position) || !Math::is_finite(size) || double(position) != p_input.position[i] || double(size) != p_input.size[i]) {
			return false;
		}
		r_bounds.position[i] = position;
		r_bounds.size[i] = size;
	}
	const Vector3 end = r_bounds.get_end();
	return end.is_finite() && (!p_nonempty || (r_bounds.size.x > 0 && r_bounds.size.y > 0 && r_bounds.size.z > 0 && end.x > r_bounds.position.x && end.y > r_bounds.position.y && end.z > r_bounds.position.z));
}
GDExtensionOutputCommonInputV1 project_common(const ExternalOutputRequestCommon &p_common) {
	GDExtensionOutputCommonInputV1 result{};
	result.generation = p_common.generation;
	result.present_seq = p_common.present_seq;
	memcpy(result.output_tag.bytes, p_common.output_tag.ptr(), 32);
	memcpy(result.view_tag.bytes, p_common.view_tag.ptr(), 32);
	memcpy(result.current_host_token.bytes, p_common.current_host_token.ptr(), 32);
	for (uint32_t i = 0; i < 3; ++i) {
		result.bounds.position[i] = p_common.bounds.position[i];
		result.bounds.size[i] = p_common.bounds.size[i];
	}
	result.history_reset = p_common.history_reset;
	result.current_camera = { p_common.current_camera_token.view_id, p_common.current_camera_token.view_epoch, p_common.current_camera_token.capture_ordinal };
	result.has_actual_previous_camera = p_common.has_actual_previous_camera;
	if (result.has_actual_previous_camera) {
		result.actual_previous_camera = { p_common.actual_previous_camera_token.view_id, p_common.actual_previous_camera_token.view_epoch, p_common.actual_previous_camera_token.capture_ordinal };
	}
	result.has_previous_consumed_tuple = p_common.has_previous_consumed_tuple;
	if (result.has_previous_consumed_tuple) {
		memcpy(result.previous_consumed_tuple_token.bytes, p_common.previous_consumed_tuple_token.ptr(), 32);
	}
	return result;
}
bool camera_equal(const GDExtensionOutputCameraV1 &p_a, const GDExtensionOutputCameraV1 &p_b) {
	return p_a.view_id == p_b.view_id && p_a.view_epoch == p_b.view_epoch && p_a.capture_ordinal == p_b.capture_ordinal;
}
bool common_matches(const ExternalOutputRequestCommon &p_original, const GDExtensionOutputCommonInputV1 &p_input) {
	if (p_original.output_tag.size() != 32 || p_original.view_tag.size() != 32 || p_original.current_host_token.size() != 32 || (p_original.has_previous_consumed_tuple && p_original.previous_consumed_tuple_token.size() != 32)) {
		return false;
	}
	const auto actual = project_common(p_original);
	AABB bounds;
	return decode_bounds(p_input.bounds, bounds, true) && p_original.bounds == bounds &&
			actual.generation == p_input.generation && actual.present_seq == p_input.present_seq && actual.history_reset == p_input.history_reset &&
			memcmp(actual.output_tag.bytes, p_input.output_tag.bytes, 32) == 0 && memcmp(actual.view_tag.bytes, p_input.view_tag.bytes, 32) == 0 &&
			memcmp(actual.current_host_token.bytes, p_input.current_host_token.bytes, 32) == 0 && camera_equal(actual.current_camera, p_input.current_camera) &&
			actual.has_actual_previous_camera == p_input.has_actual_previous_camera && (!actual.has_actual_previous_camera || camera_equal(actual.actual_previous_camera, p_input.actual_previous_camera)) &&
			actual.has_previous_consumed_tuple == p_input.has_previous_consumed_tuple && (!actual.has_previous_consumed_tuple || memcmp(actual.previous_consumed_tuple_token.bytes, p_input.previous_consumed_tuple_token.bytes, 32) == 0);
}
} //namespace
Dictionary ParticlesStorage::particles_external_create(const Dictionary &p_config) {
	ExternalOutputConfig config;
	StringName detail;
	const Error error = ExternalOutputConfig::parse(p_config, config, detail);
	if (error != OK) {
		return output_error(error, detail);
	}
	// No original parent charge/source/material/qualification issuer yet.
	// Parsing cannot authorize allocating an owner or even an empty RID.
	const auto result = create_body();
	return output_error(Error(result.error), result.detail);
}
Dictionary ParticlesStorage::particles_external_acquire(RID p_particles, const Dictionary &p_request) {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	if (!particles || !particles->external) {
		return output_error(ERR_INVALID_PARAMETER, "external_owner_required");
	}
	ExternalOutputRequest request;
	StringName detail;
	const Error error = ExternalOutputRequest::parse(p_request, request, detail);
	if (error != OK) {
		return output_error(error, detail);
	}
	uint32_t view = UINT32_MAX, slot = UINT32_MAX;
	const Error preflight = particles->external->select_slot(request, view, slot);
	if (preflight != OK) {
		return output_error(preflight, "output_preflight");
	}
	return output_error(ERR_UNAVAILABLE, "original_source_mapping_host_required");
}
Dictionary ParticlesStorage::particles_external_seal(RID p_particles, const PackedByteArray &p_lease_token, const Dictionary &p_seal) {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	if (!particles || !particles->external || p_lease_token.size() != 32) {
		return output_error(ERR_INVALID_PARAMETER, "external_lease_required");
	}
	ExternalOutputSeal seal;
	StringName detail;
	const Error error = ExternalOutputSeal::parse(p_seal, seal, detail);
	if (error != OK) {
		return output_error(error, detail);
	}
	const auto result = _external_seal_body(p_particles, p_lease_token.ptr(), project_common(seal.common), seal.common.phase_token.ptr());
	return output_error(Error(result.error), result.detail);
}
GDExtensionOutputStatusV1 ParticlesStorage::_external_seal_body(RID p_particles, const uint8_t *p_lease, const GDExtensionOutputCommonInputV1 &p_common, const uint8_t *p_facade_phase) {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	if (!particles || !particles->external || !p_lease) {
		return output_status(ERR_INVALID_PARAMETER, "external_lease_required");
	}
	const auto &owner = *particles->external;
	for (uint32_t i = 0; i < owner.slot_count; ++i) {
		const auto &slot = owner.slots[i];
		if (!token_matches(slot.lease_token, p_lease)) {
			continue;
		}
		if (!common_matches(slot.request.common, p_common) || (p_facade_phase && !token_matches(slot.request.common.phase_token, p_facade_phase))) {
			return output_status(ERR_INVALID_DATA, "seal_request_mismatch");
		}
		if (slot.publication_closed || !owner.admits_work()) {
			return output_status(ERR_UNAVAILABLE, "output_closed");
		}
		// Typed physical context does not invent the facade phase token or the
		// missing original record/phase/source association. No publication occurs.
		return output_status(ERR_UNAVAILABLE, "original_record_bridge_required");
	}
	return output_status(ERR_INVALID_PARAMETER, "stale_lease");
}
Error ParticlesStorage::particles_external_cancel(RID p_particles, const PackedByteArray &p_lease_token) {
	return p_lease_token.size() == 32 ? _external_cancel_body(p_particles, p_lease_token.ptr()) : ERR_INVALID_PARAMETER;
}
Error ParticlesStorage::_external_cancel_body(RID p_particles, const uint8_t *p_lease) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	if (!particles || !particles->external || !p_lease) {
		return ERR_INVALID_PARAMETER;
	}
	auto &owner = *particles->external;
	for (uint32_t i = 0; i < owner.slot_count; ++i) {
		if (token_matches(owner.slots[i].lease_token, p_lease)) {
			return owner.cancel_slot(i);
		}
	}
	return ERR_INVALID_PARAMETER;
}
Error ParticlesStorage::particles_external_withdraw(RID p_particles, const PackedByteArray &p_generation) {
	if (p_generation.size() != 8) {
		return ERR_INVALID_PARAMETER;
	}
	uint64_t generation = 0;
	for (uint32_t i = 0; i < 8; ++i) {
		generation |= uint64_t(p_generation[i]) << (8 * i);
	}
	return _external_withdraw_body(p_particles, generation);
}
Error ParticlesStorage::_external_withdraw_body(RID p_particles, uint64_t p_generation) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	if (!particles || !particles->external || !p_generation || p_generation != particles->external->generation) {
		return ERR_INVALID_PARAMETER;
	}
	particles->external->withdraw();
	// Explicit withdrawal runs outside Registry service locks. Return renderer
	// cache source pins synchronously, even when no later draw or free occurs.
	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
	return OK;
}

Dictionary ParticlesStorage::particles_external_poll(RID p_particles, const PackedByteArray &p_generation) {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	if (!particles || !particles->external || !generation_matches(p_generation, particles->external->generation)) {
		return output_error(ERR_INVALID_PARAMETER, "stale_generation");
	}
	// No scheduler/fence/source-observation join. Never label Released from
	// missing next draw, elapsed time or empty metadata alone.
	return output_error(ERR_UNAVAILABLE, "original_retirement_observation_required");
}
Error ParticlesStorage::_external_typed_context(GDExtensionProductionPhysicalContextRefV1 p_context, GDExtensionProductionRequestRefV1 p_request, bool p_new_work) {
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	GDExtensionDeviceExit::Registry::Lock lock(registry);
	GDExtensionDeviceExit::ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	const Error error = registry.production_physical_request_locked(p_context, p_request, request, device, p_new_work);
	if (error != OK) {
		return error;
	}
	return device == RenderingDevice::get_singleton() ? OK : ERR_INVALID_DATA;
}
void ParticlesStorage::particles_external_create_typed(GDExtensionProductionPhysicalContextRefV1 p_context, GDExtensionProductionRequestRefV1 p_request, const GDExtensionOutputCreateInputV1 &p_input, GDExtensionOutputCreateResultV1 &r_result) {
	_external_create_typed(p_context, p_request, p_input, UINT32_MAX, 0, 0, 0, r_result);
}
uint64_t ParticlesStorage::particles_external_scene_metadata_bytes() {
	ExternalOutputOwner::MetadataLayout layout;
	const Error error = ExternalOutputOwner::plan_metadata_layout(0, 0, 0, layout);
	CRASH_COND(error != OK);
	return layout.total_bytes;
}
void ParticlesStorage::particles_external_create_typed_v2(GDExtensionProductionPhysicalContextRefV1 p_context, GDExtensionProductionRequestRefV1 p_request, const GDExtensionOutputCreateInputV1 &p_input, uint32_t p_metadata_slot, GDExtensionOutputCreateResultV1 &r_result) {
	_external_create_typed(p_context, p_request, p_input, p_metadata_slot, 0, 0, 0, r_result);
}
bool ParticlesStorage::_external_decode_create(const GDExtensionOutputCreateInputV1 &p_input, ExternalOutputConfig &config, uint32_t &surface_count, GDExtensionOutputCreateResultV1 &r_result) {
	r_result.status = output_status(ERR_INVALID_PARAMETER, "CONTRACT");
	if (!valid_abi(p_input.abi, sizeof(p_input)) || p_input.profile < 1 || p_input.profile > 3 || p_input.space < 1 || p_input.space > 2 || p_input.motion > 1 || p_input.query_enabled > 1 || (p_input.pass_mask & ~uint32_t(0x1f)) || p_input.surfaces.reserved || p_input.surface_materials.reserved ||
			(p_input.surfaces.count && !p_input.surfaces.data) || (p_input.surface_materials.count && !p_input.surface_materials.data)) {
		return false;
	}
	if (p_input.capacity > (1u << 20) || !p_input.max_views || p_input.max_views > 8 || !p_input.max_inflight || p_input.max_inflight > 4) {
		r_result.status = output_status(ERR_INVALID_PARAMETER, "LIMIT");
		return false;
	}
	if (p_input.surface_materials.count > ExternalOutputConfig::MAX_FLATTENED_SURFACES) {
		r_result.status = output_status(ERR_INVALID_PARAMETER, "LIMIT");
		return false;
	}
	AABB bounds, geometry_bounds, query_bounds;
	if (!decode_bounds(p_input.bounds, bounds, true) || !decode_bounds(p_input.query_bounds, query_bounds, p_input.query_enabled != 0)) {
		return false;
	}
	if (p_input.query_enabled) {
		const AABB cull_bounds = bounds.merge(query_bounds);
		if (!cull_bounds.is_finite() || !cull_bounds.get_end().is_finite()) {
			return false;
		}
	}
	if (p_input.profile == 1 && (!p_input.surfaces.count || p_input.surfaces.count > ExternalOutputConfig::MAX_MESH_RIDS || !p_input.surface_materials.count)) {
		return false;
	}
	if (p_input.profile == 2 && (p_input.surfaces.count || !p_input.surface_materials.count || !decode_bounds(p_input.geometry_bounds, geometry_bounds, true))) {
		return false;
	}
	if (p_input.profile != 2) {
		const GDExtensionOutputBoundsV1 absent{};
		if (memcmp(&p_input.geometry_bounds, &absent, sizeof(absent)) != 0) {
			return false;
		}
	}
	if (p_input.profile == 3 && (p_input.capacity || p_input.surfaces.count || p_input.surface_materials.count || p_input.pass_mask || p_input.motion || !p_input.query_enabled)) {
		return false;
	}
	config.profile = ExternalOutputConfig::Profile(p_input.profile);
	config.capacity = p_input.capacity;
	config.max_views = p_input.max_views;
	config.max_inflight = p_input.max_inflight;
	config.space = ExternalOutputConfig::Space(p_input.space);
	config.pass_mask = p_input.pass_mask;
	config.motion = ExternalOutputConfig::Motion(p_input.motion);
	config.query_enabled = p_input.query_enabled;
	config.bounds = bounds;
	config.geometry_bounds = geometry_bounds;
	config.query_bounds = query_bounds;
	for (uint32_t i = 0; i < p_input.surfaces.count; ++i) {
		config.surfaces.push_back(RID::from_uint64(p_input.surfaces.data[i]));
	}
	for (uint32_t i = 0; i < p_input.surface_materials.count; ++i) {
		config.surface_materials.push_back(RID::from_uint64(p_input.surface_materials.data[i]));
	}
	const auto copy_tag = [](const GDExtensionOutputTokenV1 &input, PackedByteArray &output) {
		output.resize(32);
		memcpy(output.ptrw(), input.bytes, 32);
	};
	copy_tag(p_input.producer_tag, config.producer_tag);
	copy_tag(p_input.device_tag, config.device_tag);
	copy_tag(p_input.layout_digest, config.layout_digest);
	copy_tag(p_input.mapping_digest, config.mapping_digest);
	copy_tag(p_input.numeric_profile_digest, config.numeric_profile_digest);
	const Error selection_error = ExternalOutputOwner::validate_native_selection(config, surface_count);
	if (selection_error != OK) {
		r_result.status = output_status(selection_error, "native_output_selection");
		return false;
	}
	return true;
}

void ParticlesStorage::_external_create_typed(GDExtensionProductionPhysicalContextRefV1 p_context, GDExtensionProductionRequestRefV1 p_request, const GDExtensionOutputCreateInputV1 &p_input, uint32_t p_metadata_slot, uint32_t p_view_capacity, uint32_t p_read_capacity, uint32_t p_surface_capacity, GDExtensionOutputCreateResultV1 &r_result) {
	r_result = {};
	r_result.abi = { 1, sizeof(r_result), 0 };
	const Error context_error = _external_typed_context(p_context, p_request, true);
	if (context_error != OK) {
		r_result.status = output_status(context_error, "original_record_context_required");
		return;
	}
	ExternalOutputConfig config;
	uint32_t surface_count = 0;
	if (!_external_decode_create(p_input, config, surface_count, r_result)) {
		return;
	}
	if (p_metadata_slot == UINT32_MAX) {
		r_result.status = create_body();
		return;
	}
	// Render profiles still require their actual fixed-profile shader readiness
	// and retained selection directories. A SceneOnly generation issues neither
	// a raster capability nor a published final particle source.
	if (config.profile != ExternalOutputConfig::SCENE_ONLY) {
		r_result.status = output_status(ERR_UNAVAILABLE, "native_profile_preparation_required");
		return;
	}
	if (!next_external_generation) {
		r_result.status = output_status(ERR_PARAMETER_RANGE_ERROR, "generation_exhausted");
		return;
	}
	ExternalOutputOwner::MetadataLayout layout;
	const Error planned = ExternalOutputOwner::plan_metadata_layout(p_view_capacity, p_read_capacity, p_surface_capacity, layout);
	if (planned != OK) {
		r_result.status = output_status(planned, "native_metadata_layout");
		return;
	}
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	GDExtensionDeviceExit::ProductionOutputMetadataLease metadata;
	const Error allocated = registry.production_output_metadata_create(p_context, p_request, p_metadata_slot, *RenderingDevice::get_singleton(), layout.total_bytes, layout.alignment, metadata);
	if (allocated != OK) {
		r_result.status = output_status(allocated, "original_output_metadata_required");
		return;
	}
	_external_create_received(p_context, p_request, config, metadata, p_view_capacity, p_read_capacity, p_surface_capacity, r_result);
	if (!metadata.is_empty()) {
		// Refusal before native ownership committed: the exact resource row is
		// returned once. A committed owner moved this lease out of the caller.
		const Error returned = registry.production_output_metadata_return(metadata);
		CRASH_COND(returned != OK);
	}
}

Error ParticlesStorage::_external_instance_preparation_error() const {
	// Engine-owned state only: a component/test guard cannot grant readiness.
	const Error prepared = get_external_guard_preparation_error();
	if (prepared != OK) {
		return prepared;
	}
	return is_external_guard_prepared() ? OK : ERR_UNAVAILABLE;
}

Error ParticlesStorage::_external_metadata_requirements(const ExternalOutputConfig &config, const GDExtensionOutputMetadataLimitsV1 &limits, GDExtensionOutputMetadataRequirementsV1 &out) const {
	if (!valid_abi(limits.abi, sizeof(limits)) || limits.reserved) {
		return ERR_INVALID_PARAMETER;
	}
	auto *rd = RenderingDevice::get_singleton();
	auto *server = RenderingServer::get_singleton();
	if (!rd || !server || !server->is_on_render_thread() || server->get_rendering_device() != rd) {
		return ERR_UNAUTHORIZED;
	}
	ExternalOutputOwner::MetadataLayout layout;
	Error planned;
	if (config.profile == ExternalOutputConfig::INSTANCE_192) {
		const Error prepared = _external_instance_preparation_error();
		if (prepared != OK) {
			return prepared;
		}
		planned = ExternalOutputOwner::plan_generation_selection_layout(config, limits.view_lease_capacity, limits.read_lease_capacity, limits.surface_lease_capacity, layout);
	} else if (config.profile == ExternalOutputConfig::SCENE_ONLY) {
		if (limits.view_lease_capacity || limits.read_lease_capacity || limits.surface_lease_capacity) {
			return ERR_INVALID_PARAMETER;
		}
		planned = ExternalOutputOwner::plan_metadata_layout(0, 0, 0, layout);
	} else {
		return ERR_UNAVAILABLE;
	}
	if (planned != OK) {
		return planned;
	}
	GDExtensionOutputMetadataRequirementsV1 result{};
	result.abi = { 1, sizeof(result), 0 };
	result.layout_version = 1;
	result.required_metadata_bytes = layout.total_bytes;
	result.required_alignment = layout.alignment;
	result.slot_count = config.max_views * config.max_inflight;
	result.flattened_surface_count = layout.material_selection_count;
	result.binding_count = layout.binding_count;
	result.limits = limits;
	out = result;
	return OK;
}

void ParticlesStorage::_external_create_received(GDExtensionProductionPhysicalContextRefV1 p_context, GDExtensionProductionRequestRefV1 p_request, const ExternalOutputConfig &p_config,
		GDExtensionDeviceExit::ProductionOutputMetadataLease &p_metadata, uint32_t p_view_capacity, uint32_t p_read_capacity, uint32_t p_surface_capacity, GDExtensionOutputCreateResultV1 &r_result, bool p_generation_selection) {
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	auto *rd = RenderingDevice::get_singleton();
	// Internal opt-in only. Public V2/V3 keep their SceneOnly profile gates.
	const bool instance_selection = p_generation_selection && p_config.profile == ExternalOutputConfig::INSTANCE_192;
	if (!rd || (!instance_selection && p_config.profile != ExternalOutputConfig::SCENE_ONLY) || !next_external_generation) {
		r_result.status = output_status(ERR_UNAVAILABLE, "native_generation_unavailable");
		return;
	}
	if (instance_selection) {
		const Error prepared = _external_instance_preparation_error();
		if (prepared != OK) {
			r_result.status = output_status(prepared, "native_profile_preparation_required");
			return;
		}
	}
	ExternalOutputOwner::MetadataLayout layout;
	const Error planned = instance_selection ? ExternalOutputOwner::plan_generation_selection_layout(p_config, p_view_capacity, p_read_capacity, p_surface_capacity, layout) : ExternalOutputOwner::plan_metadata_layout(p_view_capacity, p_read_capacity, p_surface_capacity, layout);
	if (planned != OK) {
		r_result.status = output_status(planned, "native_metadata_layout");
		return;
	}
	const Error authenticated = registry.production_output_metadata_validate(p_context, p_request, p_metadata, *rd, layout.total_bytes, layout.alignment);
	if (authenticated != OK) {
		r_result.status = output_status(authenticated, "original_output_metadata_required");
		return;
	}
	const uint64_t generation = next_external_generation;
	next_external_generation = generation == UINT64_MAX ? 0 : generation + 1;
	auto *owner = new (p_metadata.get_data()) ExternalOutputOwner(p_config, generation, true, instance_selection);
	owner->device = rd;
	const Error directory = owner->install_metadata_directory(p_context, p_request, registry, p_metadata, p_view_capacity, p_read_capacity, p_surface_capacity, instance_selection);
	if (directory != OK) {
		owner->state = ExternalOutputOwner::GenerationState::RELEASED;
		owner->~ExternalOutputOwner();
		r_result.status = output_status(directory, "native_metadata_directory");
		return;
	}
	const RID rid = particles_owner.allocate_rid();
	if (rid.is_null()) {
		CRASH_COND(owner->return_reserved_cells() != OK);
		owner->state = ExternalOutputOwner::GenerationState::RELEASED;
		owner->~ExternalOutputOwner();
		r_result.status = output_status(ERR_OUT_OF_MEMORY, "particles_rid_capacity");
		return;
	}
	particles_owner.initialize_rid(rid);
	auto *particles = particles_owner.get_or_null(rid);
	particles->external = owner;
	const Error installed = owner->install_generation_root(p_context, p_request, registry, *rd, rid, &p_metadata);
	if (installed != OK) {
		// install_generation_root returns its own unlinked pin on failure.
		// No source/native append occurred and no public RID escaped.
		particles->external = nullptr;
		CRASH_COND(owner->return_reserved_cells() != OK);
		owner->state = ExternalOutputOwner::GenerationState::RELEASED;
		owner->~ExternalOutputOwner();
		particles_owner.free(rid);
		r_result.status = output_status(installed, "original_generation_root_required");
		return;
	}
	owner->metadata_lease = static_cast<GDExtensionDeviceExit::ProductionOutputMetadataLease &&>(p_metadata);
	if (instance_selection) {
		Error selected = owner->acquire_generation_selection(p_context, p_request);
		if (selected == OK) {
			selected = owner->acquire_generation_surface_sources(p_context, p_request);
		}
		if (selected != OK) {
			// All post-root refusals, including a pre-lease refusal, retain the
			// linked owner/metadata and any acquired original selection prefix.
			// Original OBSERVE returns leases/rows/root and frees unpublished RID.
			owner->withdraw();
			owner->generation_selection_unpublished = true;
			owner->state = ExternalOutputOwner::GenerationState::FAULTED_RETIRING;
			r_result.status = output_status(selected, "native_generation_selection_required");
			return;
		}
	}
	r_result.particles = rid.get_id();
	r_result.generation = generation;
	// No canonical named Create capability bits exist. Root must pin public
	// capability semantics before enabling Instance192; no raster/seal/consumer
	// completeness is advertised by this internal transaction candidate.
	r_result.capabilities = 0;
	r_result.status = output_status(OK, "");
}
void ParticlesStorage::particles_external_seal_typed(GDExtensionProductionPhysicalContextRefV1 p_context, GDExtensionProductionRequestRefV1 p_request, const GDExtensionOutputSealInputV1 &p_input, GDExtensionOutputSealResultV1 &r_result) {
	r_result = {};
	r_result.abi = { 1, sizeof(r_result), 0 };
	const Error context_error = _external_typed_context(p_context, p_request, true);
	if (context_error != OK) {
		r_result.status = output_status(context_error, "original_record_context_required");
		return;
	}
	r_result.status = output_status(ERR_INVALID_PARAMETER, "CONTRACT");
	const auto &common = p_input.common;
	AABB bounds;
	if (!valid_abi(p_input.abi, sizeof(p_input)) || common.reserved || common.history_reset > 10 || common.has_actual_previous_camera > 1 || common.has_previous_consumed_tuple > 1 ||
			!common.current_camera.view_id || !common.current_camera.view_epoch || !common.current_camera.capture_ordinal || !decode_bounds(common.bounds, bounds, true)) {
		return;
	}
	if (common.has_actual_previous_camera) {
		if (!common.actual_previous_camera.view_id || !common.actual_previous_camera.view_epoch || !common.actual_previous_camera.capture_ordinal) {
			return;
		}
	} else if (common.actual_previous_camera.view_id || common.actual_previous_camera.view_epoch || common.actual_previous_camera.capture_ordinal) {
		return;
	}
	if (!common.has_previous_consumed_tuple) {
		const GDExtensionOutputTokenV1 absent{};
		if (memcmp(common.previous_consumed_tuple_token.bytes, absent.bytes, 32) != 0) {
			return;
		}
	}
	// Only Registry can join this same slot's original action, checked Records
	// writes and actual guard append. A general typed context is insufficient.
	GDExtensionDeviceExit::Registry::get_singleton().production_output_seal(p_context, p_request, &p_input, &r_result);
}
GDExtensionOutputStatusV1 ParticlesStorage::particles_external_cancel_typed(GDExtensionProductionPhysicalContextRefV1 p_context, GDExtensionProductionRequestRefV1 p_request, const GDExtensionOutputCancelInputV1 &p_input) {
	const Error context_error = _external_typed_context(p_context, p_request, false);
	if (context_error != OK) {
		return output_status(context_error, "original_record_context_required");
	}
	if (!valid_abi(p_input.abi, sizeof(p_input))) {
		return output_status(ERR_INVALID_PARAMETER, "CONTRACT");
	}
	const RID particles_rid = RID::from_uint64(p_input.particles);
	Particles *particles = particles_owner.get_or_null(particles_rid);
	if (!particles || !particles->external) {
		return output_status(ERR_INVALID_PARAMETER, "output_cancel");
	}
	auto &owner = *particles->external;
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	for (uint32_t i = 0; i < owner.slot_count; ++i) {
		auto &slot = owner.slots[i];
		if (!token_matches(slot.lease_token, p_input.lease_token.bytes)) {
			continue;
		}
		if (slot.original_registry != &registry || owner.device != RenderingDevice::get_singleton() || !owner.device) {
			return output_status(ERR_UNAUTHORIZED, "original_output_owner_required");
		}
		const Error owner_error = registry.production_output_cleanup_context(p_context, p_request, slot.original_pin, *owner.device, particles_rid, owner.generation, i, slot.lease_serial);
		if (owner_error != OK) {
			return output_status(owner_error, "original_output_owner_required");
		}
		// The original body still owns the state transition. Nothing above
		// changes publication, custody, reservations or retirement counters.
		const Error result = _external_cancel_body(particles_rid, p_input.lease_token.bytes);
		if (result == OK) {
			particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
		}
		return result == OK ? output_status(OK, "") : output_status(result, "output_cancel");
	}
	return output_status(ERR_INVALID_PARAMETER, "output_cancel");
}
GDExtensionOutputStatusV1 ParticlesStorage::particles_external_withdraw_typed(GDExtensionProductionPhysicalContextRefV1 p_context, GDExtensionProductionRequestRefV1 p_request, const GDExtensionOutputGenerationInputV1 &p_input) {
	const Error context_error = _external_typed_context(p_context, p_request, false);
	if (context_error != OK) {
		return output_status(context_error, "original_record_context_required");
	}
	if (!valid_abi(p_input.abi, sizeof(p_input))) {
		return output_status(ERR_INVALID_PARAMETER, "CONTRACT");
	}
	const RID particles_rid = RID::from_uint64(p_input.particles);
	const Particles *particles = particles_owner.get_or_null(particles_rid);
	if (!particles || !particles->external || !p_input.generation || p_input.generation != particles->external->generation) {
		return output_status(ERR_INVALID_PARAMETER, "stale_generation");
	}
	const auto &owner = *particles->external;
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	if (owner.generation_pin.is_empty()) {
		// A terminal root is returned by original OBSERVE, which retains only
		// its correlation. RELEASED plus that original serial identifies the
		// same closed generation for an idempotent cleanup retry.
		if (owner.state == ExternalOutputOwner::GenerationState::RELEASED && owner.generation_root_serial &&
				owner.generation_registry == &registry && owner.device == RenderingDevice::get_singleton() &&
				owner.generation_request.registry_instance == p_request.registry_instance &&
				owner.generation_request.handle_serial == p_request.handle_serial) {
			return output_status(OK, "");
		}
		if (owner.generation_root_serial || owner.generation_registry || owner.generation_request.registry_instance || owner.generation_request.handle_serial) {
			return output_status(ERR_UNAUTHORIZED, "original_output_generation_owner_required");
		}
		return output_status(ERR_UNAVAILABLE, "original_output_generation_owner_required");
	}
	if (owner.generation_registry != &registry || !owner.device || owner.device != RenderingDevice::get_singleton() ||
			owner.generation_request.registry_instance != p_request.registry_instance || owner.generation_request.handle_serial != p_request.handle_serial ||
			!owner.generation_root_matches(owner.generation_pin)) {
		return output_status(ERR_UNAUTHORIZED, "original_output_generation_owner_required");
	}
	const Error owner_error = registry.production_output_generation_cleanup_context(p_context, p_request, owner.generation_pin, *owner.device, particles_rid, owner.generation);
	if (owner_error != OK) {
		return output_status(owner_error, "original_output_generation_owner_required");
	}
	const Error result = _external_withdraw_body(particles_rid, p_input.generation);
	return result == OK ? output_status(OK, "") : output_status(result, "output_withdraw");
}
bool ParticlesStorage::particles_is_external(RID p_particles) const {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	return particles && particles->external;
}
bool ParticlesStorage::particles_is_external_scene_only(RID p_particles) const {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	return particles && particles->external && particles->external->config.profile == ExternalOutputConfig::SCENE_ONLY;
}
Error ParticlesStorage::particles_external_scene_material_facts(RID p_particles, ExternalSceneMaterialFacts &r_facts, ExternalSceneMaterialVisit p_visit, void *p_context) const {
	r_facts = {};
	auto *rd = RenderingDevice::get_singleton();
	if (!rd || !rd->native_immutable_is_render_thread() || !p_visit || !p_context) {
		return ERR_UNAVAILABLE;
	}
	const Particles *particles = particles_owner.get_or_null(p_particles);
	if (!particles || !particles->external) {
		return ERR_INVALID_PARAMETER;
	}
	const auto &owner = *particles->external;
	if (owner.device != rd || !owner.admits_work() || !owner.generation_registry || !owner.generation_root_matches(owner.generation_pin) ||
			!owner.generation_registry->production_output_pin_matches(owner.generation_pin, owner.generation_request, *rd)) {
		return ERR_UNAUTHORIZED;
	}
	if (owner.config.profile == ExternalOutputConfig::SCENE_ONLY) {
		return OK;
	}
	if (owner.config.profile != ExternalOutputConfig::INSTANCE_192 || !owner.generation_selection_complete || !owner.generation_surfaces_complete ||
			!owner.generation_materials || !owner.generation_meshes || !owner.generation_material_count || !owner.generation_mesh_count ||
			owner.generation_mesh_count > ExternalOutputConfig::MAX_MESH_RIDS ||
			owner.generation_material_count > ExternalOutputConfig::MAX_FLATTENED_SURFACES || owner.retained_mesh_count != owner.generation_mesh_count ||
			owner.retained_material_count != owner.generation_material_count || owner.retained_surface_count != owner.generation_material_count) {
		return ERR_UNAVAILABLE;
	}
	auto *materials = MaterialStorage::get_singleton();
	if (!materials) {
		return ERR_UNAVAILABLE;
	}
	for (uint32_t i = 0; i < owner.generation_mesh_count; ++i) {
		if (owner.generation_meshes[i].mesh.is_null() || !owner.generation_meshes[i].lease.is_active()) {
			return ERR_INVALID_DATA;
		}
	}
	ExternalSceneMaterialFacts facts;
	// Validate the entire immutable generation before the first callback. All
	// original selection leases stay in the owner; this creates no new borrow.
	for (uint32_t i = 0; i < owner.generation_material_count; ++i) {
		const auto &source = owner.generation_materials[i];
		if (source.mesh.is_null() || source.material.is_null() || !source.generation || source.native_surface == UINT32_MAX ||
				!source.geometry_source.is_valid() || !source.lease.is_active()) {
			return ERR_INVALID_DATA;
		}
		MaterialStorage::NativeSelectionShaderObservation classification;
		const Error classified = materials->material_selection_get_shader_observation(source.lease, classification);
		if (classified != OK) {
			return classified;
		}
		if (classification.material != source.material || classification.material_generation != source.generation || classification.shader_version.is_null()) {
			return ERR_INVALID_DATA;
		}
		facts.casts_shadows |= bool(classification.output_pass_mask & owner.config.pass_mask & ExternalOutputConfig::PASS_SHADOW);
		// The observation validated this exact held material/shader generation;
		// reuse its original native animation classifier, never instance overrides.
		facts.animated |= materials->material_is_animated(source.material);
	}
	for (uint32_t i = 0; i < owner.generation_material_count; ++i) {
		const auto &source = owner.generation_materials[i];
		p_visit(p_context, source.mesh, source.material);
	}
	r_facts = facts;
	return OK;
}

uint32_t ParticlesStorage::particles_external_surface_count(RID p_particles) const {
	auto *rd = RenderingDevice::get_singleton();
	if (!rd || !rd->native_immutable_is_render_thread()) {
		return 0;
	}
	const Particles *particles = particles_owner.get_or_null(p_particles);
	if (!particles || !particles->external) {
		return 0;
	}
	const auto &owner = *particles->external;
	return owner.admits_work() && owner.config.profile == ExternalOutputConfig::INSTANCE_192 && owner.generation_surfaces_complete &&
					owner.generation_selection_complete && owner.retained_surface_count == owner.generation_material_count
			? owner.retained_surface_count
			: 0;
}
Error ParticlesStorage::particles_external_surface_acquire(RID p_particles, uint32_t p_surface, ExternalSurfaceSelection &r_selection) {
	auto *rd = RenderingDevice::get_singleton();
	if (!rd || !rd->native_immutable_is_render_thread()) {
		return ERR_UNAUTHORIZED;
	}
	Particles *particles = particles_owner.get_or_null(p_particles);
	return particles && particles->external ? particles->external->surface_acquire(p_surface, r_selection) : ERR_DOES_NOT_EXIST;
}
Error ParticlesStorage::particles_external_surface_release(ExternalSurfaceSelection &p_selection) {
	return p_selection.owner ? p_selection.owner->surface_return(p_selection) : ERR_DOES_NOT_EXIST;
}
bool ParticlesStorage::particles_external_view_can_prepare_shader(const ExternalPacketViewLease &view) const {
	using GDExtensionDeviceExit::Registry;
	auto *device = RenderingDevice::get_singleton();
	if (!device || !device->is_main_instance || !device->native_renderer_recording_window_available() ||
			Registry::production_physical_frame || !view.owner) {
		return false;
	}
	const auto &owner = *view.owner;
	if (owner.device != device || owner.config.profile != ExternalOutputConfig::INSTANCE_192 || !owner.config.capacity ||
			view.slot >= owner.slot_count) {
		return false;
	}
	const auto &slot = owner.slots[view.slot];
	return slot.original_registry && !Registry::Lock::owns(*slot.original_registry) &&
			!slot.publication_closed && slot.storage && slot.guard_resources_ready &&
			owner.preparation_view_matches(view, view.slot);
}

Error ParticlesStorage::particles_external_prepare_pipeline(const ExternalSurfaceSelection &surface, const ExternalPacketViewLease &view, const ExternalPacketPipelineRequest &selection, ExternalPreparationBody body, void *userdata, const ExternalPipelinePreparation *&out, bool p_pre_list) {
	auto *device = RenderingDevice::get_singleton();
	if (!device || !device->native_immutable_is_render_thread() || !body || !surface.owner || view.owner != surface.owner) {
		return ERR_UNAUTHORIZED;
	}
	auto &owner = *surface.owner;
	if (owner.device != device || !owner.preparation_view_matches(view, view.slot) || surface.generation != owner.generation || surface.cell >= owner.surface_capacity ||
			!owner.generation_registry || !owner.generation_materials || !owner.surface_cells || !owner.generation_surfaces_complete || !surface.geometry_source || !surface.geometry_source->is_valid() || surface.flattened_surface >= owner.retained_surface_count) {
		return ERR_UNAUTHORIZED;
	}
	const auto &cell = owner.surface_cells[surface.cell];
	const auto &original_source = owner.generation_materials[surface.flattened_surface];
	if (cell.consumer != &surface || cell.serial != surface.serial || cell.surface != surface.flattened_surface || cell.pin.is_empty() ||
			surface.geometry_source != &original_source.geometry_source || surface.mesh != original_source.mesh || surface.native_surface != original_source.native_surface ||
			surface.material_selection != original_source.material || surface.material_generation != original_source.generation ||
			!owner.generation_registry->production_output_pin_matches(cell.pin, owner.generation_request, *device)) {
		return ERR_UNAUTHORIZED;
	}
	auto &slot = owner.slots[view.slot];
	// The pre-list pass has no factory work for an empty color packet.
	if (p_pre_list && owner.config.profile == ExternalOutputConfig::INSTANCE_192 && owner.config.capacity == 0) {
		return ERR_UNAVAILABLE;
	}
	uint32_t index = UINT32_MAX;
	bool reused = false;
	{
		GDExtensionDeviceExit::Registry::Lock lock(*slot.original_registry);
		if (owner.has_inflight_preparation()) {
			return ERR_BUSY;
		}
		const Error selected = owner.select_preparation_tuple(view.slot, surface.flattened_surface, selection, index, reused);
		if (selected != OK) {
			return selected;
		}
		if (reused) {
			RID *root = nullptr;
			ExternalPipelinePreparation *authenticated = nullptr;
			const Error authority = slot.original_registry->production_output_preparation_validate_locked(slot.original_pin, *device, slot.original_action_ordinal, surface.flattened_surface,
					owner.preparation_rows[index].pipeline_owned_group, root, authenticated, nullptr, index, &view, true);
			if (authority != OK) {
				return authority;
			}
		}
	}
	auto &row = owner.preparation_rows[index];
	if (reused) {
		out = &row;
		return row.finish_complete ? row.backend_error : (row.finish_error == ERR_UNCONFIGURED ? ERR_BUSY : row.finish_error);
	}
	GDExtensionDeviceExit::ProductionOutputPreparationScope scope;
	Error error = slot.original_registry->production_output_preparation_begin_row(slot.original_pin, *device, slot.original_action_ordinal, surface.flattened_surface, index, row.pipeline_owned_group, scope, &view);
	if (error != OK) {
		row.backend_error = error;
		row.tuple_state = ExternalPipelinePreparation::TupleState::FAILED;
		out = &row;
		return error;
	}
	// Synchronous real backend body; it cannot retain this stack scope. Every
	// actual successful/failed hold stays in the original stable row.
	row.backend_error = body(userdata, surface, row);
	if (row.backend_error == OK && row.owned_pipeline_root.is_null()) {
		row.backend_error = ERR_CANT_CREATE;
	}
	if (row.backend_error == OK) {
		row.backend_error = _external_transforms_prepare_owned_prefix(row);
	}
	row.finish_error = slot.original_registry->production_output_preparation_finish(scope);
	out = &row;
	return row.finish_error != OK ? row.finish_error : row.backend_error;
}

Error ParticlesStorage::particles_external_end_draw_list() {
	auto *device = RD::get_singleton();
	return device ? device->_production_output_reads_close() : ERR_UNAVAILABLE;
}

Error ParticlesStorage::particles_external_draw(const ExternalSurfaceSelection &surface, const ::RenderDataRD *p_render_data, RD::DrawListID p_list, const ExternalPacketPipelineRequest &p_pipeline, ExternalPipelinePrepareForView p_prepare, void *p_renderer, ExternalPushPrepareForBinding p_prepare_push, void *p_push_context, ExternalDrawPreparation p_preparation) {
	if (surface.is_empty() || !p_render_data || !p_prepare_push || !p_push_context ||
			(p_preparation == ExternalDrawPreparation::PREPARE_FOR_DRAW && (!p_prepare || !p_renderer))) {
		return ERR_INVALID_PARAMETER;
	}
	// Borrow the cache's original generation source. Acquiring a second surface
	// here would consume an unquoted cell for every draw of a retained cache.
	ExternalPacketViewLease view;
	Error error = particles_external_view_acquire(surface, p_render_data, view);
	if (error != OK) {
		return error;
	}
	if (view.owner && view.owner->config.profile == ExternalOutputConfig::INSTANCE_192 && view.owner->config.capacity == 0) {
		ExternalPacketBinding binding;
		error = particles_external_resolve(view, p_pipeline, p_list, binding);
		if (error == OK) {
			error = binding.record_checked(p_list);
		}
		if (error != OK && !binding.is_empty()) {
			const Error returned = binding.cancel_before_draw();
			if (returned != OK) {
				error = returned;
			}
		}
		const Error view_error = view.release_checked();
		return error != OK ? error : view_error;
	}
	if (p_preparation == ExternalDrawPreparation::PREPARE_FOR_DRAW) {
		const ExternalPipelinePreparation *preparation = nullptr;
		error = p_prepare(p_renderer, surface, view, p_pipeline, preparation);
		if (error == OK && !preparation) {
			error = ERR_UNAVAILABLE;
		}
	}
	if (error == OK) {
		ExternalPacketBinding binding;
		error = particles_external_resolve(view, p_pipeline, p_list, binding);
		if (error == OK) {
			const void *push = nullptr;
			uint32_t push_bytes = 0;
			error = p_prepare_push(p_push_context, binding, push, push_bytes);
			if (error == OK) {
				error = binding.record_checked(p_list, push, push_bytes);
			}
		}
		if (error != OK && !binding.is_empty()) {
			binding.cancel_before_draw();
		}
	}
	const Error view_error = view.release_checked();
	return error != OK ? error : view_error;
}
} // namespace RendererRD

namespace RendererRD {
void ParticlesStorage::particles_external_metadata_query(const GDExtensionOutputMetadataQueryInputV1 &input, GDExtensionOutputMetadataQueryResultV1 &out) {
	out = {};
	out.abi = { 1, sizeof(out), 0 };
	out.status = output_status(ERR_INVALID_PARAMETER, "CONTRACT");
	if (!valid_abi(input.abi, sizeof(input)) || !valid_abi(input.limits.abi, sizeof(input.limits)) || input.limits.reserved) {
		return;
	}
	auto *rd = RenderingDevice::get_singleton();
	auto *server = RenderingServer::get_singleton();
	if (!rd || !server || !server->is_on_render_thread() || server->get_rendering_device() != rd) {
		out.status = output_status(ERR_UNAUTHORIZED, "main_render_thread_required");
		return;
	}
	ExternalOutputConfig config;
	uint32_t surfaces = 0;
	GDExtensionOutputCreateResultV1 decoded{};
	if (!_external_decode_create(input.config, config, surfaces, decoded)) {
		out.status = decoded.status;
		return;
	}
	// SceneOnly has no native packet leases. Instance192 is planned through the
	// same original preparation/source owner and must retain the caller's paid
	// capacities all the way into Create.
	if (config.profile == ExternalOutputConfig::SCENE_ONLY &&
			(input.limits.view_lease_capacity || input.limits.read_lease_capacity || input.limits.surface_lease_capacity)) {
		return;
	}
	const Error planned = _external_metadata_requirements(config, input.limits, out.requirements);
	if (planned != OK) {
		out.status = output_status(planned == ERR_UNCONFIGURED ? ERR_UNAVAILABLE : planned, "native_metadata_layout");
		return;
	}
	out.status = output_status(OK, "");
}

void ParticlesStorage::particles_external_create_typed_v3(GDExtensionProductionPhysicalContextRefV1 context, GDExtensionProductionRequestRefV1 request, const GDExtensionOutputCreateInputV3 &input, GDExtensionOutputCreateResultV1 &out) {
	out = {};
	out.abi = { 1, sizeof(out), 0 };
	out.status = output_status(ERR_INVALID_PARAMETER, "CONTRACT");
	if (input.abi.abi_version != 3 || input.abi.struct_size != sizeof(input) || input.abi.reserved || input.reserved || input.metadata_resource_slot == UINT32_MAX || !valid_abi(input.requirements.abi, sizeof(input.requirements)) || input.requirements.reserved) {
		return;
	}
	const Error authenticated = _external_typed_context(context, request, true);
	if (authenticated != OK) {
		out.status = output_status(authenticated, "original_record_context_required");
		return;
	}
	GDExtensionOutputMetadataQueryInputV1 query{};
	query.abi = { 1, sizeof(query), 0 };
	query.config = input.config;
	query.limits = input.requirements.limits;
	GDExtensionOutputMetadataQueryResultV1 quote{};
	particles_external_metadata_query(query, quote);
	if (quote.status.error != OK) {
		out.status = quote.status;
		return;
	}
	const auto &a = input.requirements;
	const auto &b = quote.requirements;
	if (a.layout_version != b.layout_version || a.required_metadata_bytes != b.required_metadata_bytes || a.required_alignment != b.required_alignment || a.slot_count != b.slot_count || a.flattened_surface_count != b.flattened_surface_count || a.binding_count != b.binding_count) {
		return;
	}
	ExternalOutputConfig config;
	uint32_t surfaces = 0;
	if (!_external_decode_create(input.config, config, surfaces, out)) {
		return;
	}
	if (!next_external_generation) {
		out.status = output_status(ERR_PARAMETER_RANGE_ERROR, "generation_exhausted");
		return;
	}
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	GDExtensionDeviceExit::ProductionOutputMetadataLease metadata;
	const Error allocated = registry.production_output_metadata_create(context, request, input.metadata_resource_slot, *RenderingDevice::get_singleton(), b.required_metadata_bytes, b.required_alignment, metadata, true);
	if (allocated != OK) {
		out.status = output_status(allocated, "original_output_metadata_required");
		return;
	}
	_external_create_received(context, request, config, metadata,
			input.requirements.limits.view_lease_capacity,
			input.requirements.limits.read_lease_capacity,
			input.requirements.limits.surface_lease_capacity, out, true);
	if (!metadata.is_empty()) {
		CRASH_COND(registry.production_output_metadata_return(metadata) != OK);
	}
}

} // namespace RendererRD
