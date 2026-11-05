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

#include "servers/rendering/renderer_rd/storage_rd/external_output_owner.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"

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
	return output_error(ERR_UNAVAILABLE, "original_output_admission_required");
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
	const auto &owner = *particles->external;
	for (uint32_t i = 0; i < owner.slot_count; ++i) {
		const auto &slot = owner.slots[i];
		if (slot.lease_token != p_lease_token || slot.lease_token.is_empty()) {
			continue;
		}
		if (!slot.request.common.matches(seal.common)) {
			return output_error(ERR_INVALID_DATA, "seal_request_mismatch");
		}
		if (slot.publication_closed || !owner.admits_work()) {
			return output_error(ERR_UNAVAILABLE, "output_closed");
		}
		return output_error(ERR_UNAVAILABLE, "original_record_bridge_required");
	}
	return output_error(ERR_INVALID_PARAMETER, "stale_lease");
}
Error ParticlesStorage::particles_external_cancel(RID p_particles, const PackedByteArray &p_lease_token) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	if (!particles || !particles->external || p_lease_token.size() != 32) {
		return ERR_INVALID_PARAMETER;
	}
	auto &owner = *particles->external;
	for (uint32_t i = 0; i < owner.slot_count; ++i) {
		const auto &slot = owner.slots[i];
		if (slot.lease_token == p_lease_token && !slot.lease_token.is_empty()) {
			return owner.cancel_slot(i);
		}
	}
	return ERR_INVALID_PARAMETER;
}
Error ParticlesStorage::particles_external_withdraw(RID p_particles, const PackedByteArray &p_generation) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	if (!particles || !particles->external || !generation_matches(p_generation, particles->external->generation)) {
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
