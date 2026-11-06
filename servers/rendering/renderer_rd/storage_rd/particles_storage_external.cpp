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
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"

#include <limits>

namespace RendererRD {
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
	MutexLock lock(registry.mutex);
	GDExtensionDeviceExit::ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	const Error error = registry.production_physical_request_locked(p_context, p_request, request, device, p_new_work);
	if (error != OK) {
		return error;
	}
	return device == RenderingDevice::get_singleton() ? OK : ERR_INVALID_DATA;
}
void ParticlesStorage::particles_external_create_typed(GDExtensionProductionPhysicalContextRefV1 p_context, GDExtensionProductionRequestRefV1 p_request, const GDExtensionOutputCreateInputV1 &p_input, GDExtensionOutputCreateResultV1 &r_result) {
	r_result = {};
	r_result.abi = { 1, sizeof(r_result), 0 };
	const Error context_error = _external_typed_context(p_context, p_request, true);
	if (context_error != OK) {
		r_result.status = output_status(context_error, "original_record_context_required");
		return;
	}
	r_result.status = output_status(ERR_INVALID_PARAMETER, "CONTRACT");
	if (!valid_abi(p_input.abi, sizeof(p_input)) || p_input.profile < 1 || p_input.profile > 3 || p_input.space < 1 || p_input.space > 2 || p_input.motion > 1 || p_input.query_enabled > 1 || (p_input.pass_mask & ~uint32_t(0x1f)) || p_input.surfaces.reserved || p_input.surface_materials.reserved ||
			(p_input.surfaces.count && !p_input.surfaces.data) || (p_input.surface_materials.count && !p_input.surface_materials.data)) {
		return;
	}
	if (p_input.capacity > (1u << 20) || !p_input.max_views || p_input.max_views > 8 || !p_input.max_inflight || p_input.max_inflight > 4) {
		r_result.status = output_status(ERR_INVALID_PARAMETER, "LIMIT");
		return;
	}
	AABB bounds, geometry_bounds, query_bounds;
	if (!decode_bounds(p_input.bounds, bounds, true) || !decode_bounds(p_input.query_bounds, query_bounds, p_input.query_enabled != 0)) {
		return;
	}
	if (p_input.profile == 1 && (!p_input.surfaces.count || p_input.surfaces.count > ExternalOutputConfig::MAX_MESH_RIDS || !p_input.surface_materials.count)) {
		return;
	}
	if (p_input.profile == 2 && (p_input.surfaces.count || !p_input.surface_materials.count || !decode_bounds(p_input.geometry_bounds, geometry_bounds, true))) {
		return;
	}
	if (p_input.profile != 2) {
		const GDExtensionOutputBoundsV1 absent{};
		if (memcmp(&p_input.geometry_bounds, &absent, sizeof(absent)) != 0) {
			return;
		}
	}
	if (p_input.profile == 3 && (p_input.capacity || p_input.surfaces.count || p_input.surface_materials.count || p_input.pass_mask || p_input.motion || !p_input.query_enabled)) {
		return;
	}
	r_result.status = create_body();
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
	r_result.status = _external_seal_body(RID::from_uint64(p_input.particles), p_input.lease_token.bytes, common, nullptr);
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
uint32_t ParticlesStorage::particles_external_surface_count(RID p_particles) const {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	if (!particles || !particles->external) {
		return 0;
	}
	// config.surfaces counts mesh RIDs, not authenticated flattened surfaces.
	// No generation-wide retained surface directory is installed yet.
	return 0;
}
Error ParticlesStorage::particles_external_surface_acquire(RID, uint32_t, ExternalSurfaceSelection &) {
	return ERR_UNAVAILABLE;
}
Error ParticlesStorage::particles_external_surface_release(ExternalSurfaceSelection &p_selection) {
	return p_selection.owner ? p_selection.owner->surface_return(p_selection) : ERR_DOES_NOT_EXIST;
}
Error ParticlesStorage::particles_external_view_acquire(const ExternalSurfaceSelection &, const ::RenderDataRD *, ExternalPacketViewLease &) {
	return ERR_UNAVAILABLE;
}
Error ParticlesStorage::particles_external_resolve(const ExternalPacketViewLease &, const ExternalPacketPipelineRequest &, RD::DrawListID, ExternalPacketBinding &) {
	return ERR_UNAVAILABLE;
}
Error ParticlesStorage::particles_external_end_draw_list() {
	// Cannot attest native list closure without original recorded read cells.
	return ERR_UNAVAILABLE;
}
} // namespace RendererRD
