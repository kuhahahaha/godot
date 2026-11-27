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
#include "core/math/affine_f32.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_owner.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_collision_scene_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_decal_bridge_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_mesh_light_sort_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/render_data_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"
#include "servers/rendering/renderer_scene_cull.h"
#include "servers/rendering/rendering_server.h"

#include <cstdio>
#include <cstdlib>
#include <limits>

namespace RendererRD {

ParticlesDecalBridgeRD *ParticlesStorage::get_decal_bridge_rd() {
	TextureStorage *ts = TextureStorage::get_singleton();
	return ts ? ts->get_decal_bridge_rd() : nullptr;
}

const ParticlesDecalBridgeRD *ParticlesStorage::get_decal_bridge_rd() const {
	const TextureStorage *ts = TextureStorage::get_singleton();
	return ts ? ts->get_decal_bridge_rd() : nullptr;
}

// Built inline inside ParticlesStorage member functions so the lambdas retain
// ParticlesStorage's friend access to ExternalOutputOwner and ExternalOutputStorage.
#define BUILD_EXTERNAL_DECAL_SLOT_REGISTRATION(p_owner_key, p_owner, p_slot_index) \
	([&]() -> ParticlesDecalBridgeRD::ExternalDecalSlotRegistration { \
		using _BdrStorage = ExternalOutputStorage; \
		const uint64_t _bdr_owner_key = (p_owner_key); \
		const ExternalOutputOwner &_bdr_owner = (p_owner); \
		const uint32_t _bdr_slot_index = (p_slot_index); \
		const ExternalOutputOwner::Slot &_bdr_slot = _bdr_owner.slots[_bdr_slot_index]; \
		ParticlesDecalBridgeRD::ExternalDecalSlotRegistration _bdr_reg; \
		_bdr_reg.owner_key = _bdr_owner_key; \
		_bdr_reg.owner_ptr = &_bdr_owner; \
		_bdr_reg.is_slot_live_fn = [](const ExternalOutputOwner *_bdr_o, uint64_t _bdr_gen, uint32_t _bdr_s, uint64_t _bdr_lease) -> bool { \
			if (!_bdr_o || !_bdr_o->admits_work() || _bdr_o->generation != _bdr_gen || _bdr_s >= _bdr_o->slot_count) { \
				return false; \
			} \
			const auto &_bdr_live_slot = _bdr_o->slots[_bdr_s]; \
			if (_bdr_live_slot.publication_closed || _bdr_live_slot.pass_uses_closed || _bdr_live_slot.lease_serial != _bdr_lease || \
					_bdr_live_slot.storage == nullptr || \
					(_bdr_live_slot.state != ExternalOutputOwner::LeaseState::ADOPTED && \
							_bdr_live_slot.state != ExternalOutputOwner::LeaseState::SEALED_RECORDED)) { \
				return false; \
			} \
			if (_bdr_live_slot.scene_cull_bound) { \
				RendererSceneCull *_bdr_scene = static_cast<RendererSceneCull *>(RSG::scene); \
				RendererSceneCull::NativeParticleSceneFacts _bdr_actual; \
				const Error _bdr_facts_err = _bdr_scene ? _bdr_scene->instance_get_committed_particle_scene_facts(_bdr_live_slot.scene_cull_instance, _bdr_live_slot.scene_cull_particles, _bdr_actual) : ERR_UNAVAILABLE; \
				if (_bdr_facts_err == OK) { \
					if (_bdr_actual.scenario != _bdr_live_slot.scene_cull_scenario) { \
						return false; \
					} \
				} else if (_bdr_facts_err != ERR_BUSY) { \
					return false; \
				} \
				const RendererViewport::Viewport *_bdr_vp = (RSG::viewport && _bdr_live_slot.scene_cull_viewport.is_valid()) \
						? RSG::viewport->viewport_owner.get_or_null(_bdr_live_slot.scene_cull_viewport) \
						: nullptr; \
				if (!_bdr_vp || _bdr_vp->scenario != _bdr_live_slot.scene_cull_scenario) { \
					return false; \
				} \
			} \
			return true; \
		}; \
		_bdr_reg.is_slot_visible_fn = [](const ExternalOutputOwner *_bdr_o, uint64_t _bdr_gen, uint32_t _bdr_s, uint64_t _bdr_lease) -> bool { \
			if (!_bdr_o || _bdr_o->generation != _bdr_gen || _bdr_s >= _bdr_o->slot_count) { \
				return false; \
			} \
			const auto &_bdr_live_slot = _bdr_o->slots[_bdr_s]; \
			if (_bdr_live_slot.lease_serial != _bdr_lease) { \
				return false; \
			} \
			if (_bdr_live_slot.scene_cull_bound) { \
				RendererSceneCull *_bdr_scene = static_cast<RendererSceneCull *>(RSG::scene); \
				if (!_bdr_scene || !_bdr_scene->instance_is_visible(_bdr_live_slot.scene_cull_instance)) { \
					return false; \
				} \
			} \
			return true; \
		}; \
		_bdr_reg.generation = _bdr_owner.generation; \
		_bdr_reg.slot_index = _bdr_slot_index; \
		_bdr_reg.lease_serial = _bdr_slot.lease_serial; \
		_bdr_reg.scenario_rid = _bdr_slot.scene_cull_bound ? _bdr_slot.scene_cull_scenario : RID(); \
		_bdr_reg.viewport_rid = _bdr_slot.scene_cull_bound ? _bdr_slot.scene_cull_viewport : RID(); \
		const bool _bdr_valid_cull_bounds = _bdr_slot.scene_cull_bound && _bdr_slot.scene_cull_world.is_finite() && \
				_bdr_slot.scene_cull_world.get_end().is_finite() && _bdr_slot.scene_cull_world.has_volume(); \
		_bdr_reg.world_bounds = _bdr_valid_cull_bounds ? _bdr_slot.scene_cull_world : _bdr_owner.config.bounds; \
		_bdr_reg.records_rid = _bdr_slot.storage ? _bdr_slot.storage->resources[_BdrStorage::CURRENT_RECORDS].rid : RID(); \
		_bdr_reg.meta_rid = _bdr_slot.storage ? _bdr_slot.storage->resources[_BdrStorage::OUTPUT_META].rid : RID(); \
		_bdr_reg.harvest_uniform_set = _bdr_slot.guard_uniform_sets[0]; \
		for (uint32_t _bdr_ti = 0; _bdr_ti < ParticlesDecalBridgeRD::TEXTURE_SLOT_COUNT; ++_bdr_ti) { \
			_bdr_reg.textures[_bdr_ti] = _bdr_owner.decal_textures[_bdr_ti]; \
		} \
		_bdr_reg.capacity = _bdr_owner.config.capacity; \
		_bdr_reg.present_seq = _bdr_slot.request.common.present_seq; \
		return _bdr_reg; \
	}())

Error ExternalOutputOwner::validate_decal128_selection(const ExternalOutputConfig &p_config, uint32_t &r_surface_count) {
	if (p_config.profile != ExternalOutputConfig::PARTICLE_DECAL_128 ||
			p_config.capacity > ParticlesDecalBridgeRD::MAX_PARTICLE_DECALS ||
			!p_config.surfaces.is_empty() || p_config.surface_materials.size() != 4 ||
			p_config.pass_mask != 0 || p_config.motion != ExternalOutputConfig::DISABLED) {
		return ERR_INVALID_PARAMETER;
	}
	TextureStorage *textures = TextureStorage::get_singleton();
	if (!textures) {
		return ERR_UNAVAILABLE;
	}
	bool has_albedo_or_emission = false;
	for (uint32_t i = 0; i < 4; ++i) {
		const RID tex = p_config.surface_materials[i];
		if (tex.is_null()) {
			continue;
		}
		if (!textures->owns_texture(tex)) {
			return ERR_INVALID_PARAMETER;
		}
		if (i == 0 || i == 3) {
			has_albedo_or_emission = true;
		}
	}
	if (!has_albedo_or_emission) {
		return ERR_INVALID_PARAMETER;
	}
	r_surface_count = 0;
	return OK;
}

Error ExternalOutputOwner::plan_decal128_selection_layout(const ExternalOutputConfig &p_config, uint32_t p_views, uint32_t p_reads, uint32_t p_surfaces, MetadataLayout &r_layout) {
	uint32_t flattened = 0;
	const Error error = validate_decal128_selection(p_config, flattened);
	if (error != OK) {
		return error;
	}
	if (!p_config.max_views || p_config.max_views > MAX_VIEWS || !p_config.max_inflight || p_config.max_inflight > MAX_INFLIGHT) {
		return ERR_INVALID_PARAMETER;
	}
	const uint64_t slots = uint64_t(p_config.max_views) * p_config.max_inflight;
	return plan_metadata_layout(p_views, p_reads, p_surfaces, r_layout, 0, 0, 0, uint32_t(slots), p_reads);
}

Error ExternalOutputOwner::install_decal128_metadata_directory(GDExtensionDeviceExit::ProductionPhysicalContextRef p_context,
		GDExtensionProductionRequestRefV1 p_request, GDExtensionDeviceExit::Registry &p_registry,
		const GDExtensionDeviceExit::ProductionOutputMetadataLease &p_lease,
		uint32_t p_views, uint32_t p_reads, uint32_t p_surfaces) {
	if (metadata_directory_installed || view_cells || read_cells || surface_cells || preparation_rows || view_capacity || read_capacity || surface_capacity || preparation_capacity) {
		return ERR_ALREADY_IN_USE;
	}
	if (state != GenerationState::CONFIGURED || has_live_slots()) {
		return ERR_BUSY;
	}
	if (!device || p_lease.is_empty() || p_lease.get_data() != this ||
			(!metadata_lease.is_empty() && (metadata_lease.get_data() != p_lease.get_data() || metadata_lease.get_size() != p_lease.get_size()))) {
		return ERR_UNAUTHORIZED;
	}
	if (generation_pin.is_empty()) {
		if (generation_registry || generation_request.registry_instance || generation_request.handle_serial || generation_root_serial) {
			return ERR_UNAUTHORIZED;
		}
	} else if (generation_registry != &p_registry || generation_request.registry_instance != p_request.registry_instance ||
			generation_request.handle_serial != p_request.handle_serial || !generation_root_matches(generation_pin)) {
		return ERR_UNAUTHORIZED;
	}
	if (config.profile != ExternalOutputConfig::PARTICLE_DECAL_128 || !config_tags_inline) {
		return ERR_UNAVAILABLE;
	}
	uint32_t material_selections = 0;
	Error error = validate_decal128_selection(config, material_selections);
	if (error != OK) {
		return error;
	}
	MetadataLayout layout;
	error = plan_decal128_selection_layout(config, p_views, p_reads, p_surfaces, layout);
	if (error != OK) {
		return error;
	}
	error = p_registry.production_output_metadata_validate(p_context, p_request, p_lease, *device, layout.total_bytes, layout.alignment);
	if (error != OK) {
		return error;
	}
	auto *base = static_cast<uint8_t *>(p_lease.get_data());
	for (uint32_t i = 0; i < p_views; ++i) {
		new (base + layout.view_offset + size_t(i) * sizeof(ViewCell)) ViewCell;
	}
	for (uint32_t i = 0; i < p_reads; ++i) {
		new (base + layout.read_offset + size_t(i) * sizeof(ReadCell)) ReadCell;
	}
	for (uint32_t i = 0; i < p_surfaces; ++i) {
		new (base + layout.surface_offset + size_t(i) * sizeof(SurfaceCell)) SurfaceCell;
	}
	for (uint32_t i = 0; i < layout.preparation_count; ++i) {
		new (base + layout.preparation_offset + size_t(i) * sizeof(ExternalPipelinePreparation)) ExternalPipelinePreparation;
	}
	for (uint32_t i = 0; i < 4; ++i) {
		decal_textures[i] = config.surface_materials[i];
	}
	generation_meshes = nullptr;
	generation_materials = nullptr;
	generation_mesh_count = 0;
	generation_material_count = 0;
	binding_capacity = layout.binding_count;
	preparation_rows = layout.preparation_count ? reinterpret_cast<ExternalPipelinePreparation *>(base + layout.preparation_offset) : nullptr;
	preparation_capacity = layout.preparation_count;
	storage_capacity = layout.storage_count;
	config.surfaces.clear();
	config.surface_materials.clear();
	generation_config_inline = true;
	view_cells = p_views ? reinterpret_cast<ViewCell *>(base + layout.view_offset) : nullptr;
	read_cells = p_reads ? reinterpret_cast<ReadCell *>(base + layout.read_offset) : nullptr;
	surface_cells = p_surfaces ? reinterpret_cast<SurfaceCell *>(base + layout.surface_offset) : nullptr;
	view_capacity = p_views;
	read_capacity = p_reads;
	surface_capacity = p_surfaces;
	metadata_directory_installed = true;
	metadata_directories_placement = true;
	return OK;
}

Error ExternalOutputOwner::acquire_decal128_selection(GDExtensionDeviceExit::ProductionPhysicalContextRef p_context, GDExtensionProductionRequestRefV1 p_request) {
	if (generation_selection_attempted || state != GenerationState::CONFIGURED || has_live_slots()) {
		return ERR_ALREADY_IN_USE;
	}
	if (!generation_registry || !device || !generation_root_matches(generation_pin) ||
			p_request.registry_instance != generation_request.registry_instance || p_request.handle_serial != generation_request.handle_serial ||
			!generation_registry->production_output_pin_matches(generation_pin, p_request, *device)) {
		return ERR_UNAUTHORIZED;
	}
	if (!metadata_directories_placement || !generation_config_inline ||
			config.profile != ExternalOutputConfig::PARTICLE_DECAL_128 ||
			generation_meshes || generation_materials || generation_mesh_count || generation_material_count) {
		return ERR_INVALID_DATA;
	}
	MetadataLayout layout;
	Error error = plan_metadata_layout(view_capacity, read_capacity, surface_capacity, layout, 0, 0, binding_capacity, storage_capacity, preparation_capacity);
	if (error != OK) {
		return error;
	}
	error = generation_registry->production_output_metadata_validate(p_context, p_request, metadata_lease, *device, layout.total_bytes, layout.alignment);
	if (error != OK) {
		return error;
	}
	TextureStorage *textures = TextureStorage::get_singleton();
	if (!textures) {
		return ERR_UNAVAILABLE;
	}
	generation_selection_attempted = true;
	auto fail = [&](Error p_reason) {
		withdraw();
		generation_selection_unpublished = true;
		state = GenerationState::FAULTED_RETIRING;
		return p_reason;
	};
	bool has_albedo_or_emission = false;
	for (uint32_t i = 0; i < 4; ++i) {
		if (decal_textures[i].is_null()) {
			continue;
		}
		if (!textures->owns_texture(decal_textures[i])) {
			return fail(ERR_INVALID_PARAMETER);
		}
		if (i == 0 || i == 3) {
			has_albedo_or_emission = true;
		}
	}
	if (!has_albedo_or_emission) {
		return fail(ERR_INVALID_PARAMETER);
	}
	for (uint32_t i = 0; i < 4; ++i) {
		if (decal_textures[i].is_valid()) {
			textures->texture_add_to_decal_atlas(decal_textures[i]);
		}
	}
	decal_textures_registered = true;
	generation_selection_complete = true;
	return OK;
}

void ExternalOutputOwner::release_decal_atlas_registrations() {
	if (!decal_textures_registered) {
		return;
	}
	decal_textures_registered = false;
	TextureStorage *textures = TextureStorage::get_singleton();
	for (uint32_t i = 0; i < 4; ++i) {
		if (decal_textures[i].is_valid()) {
			if (textures && textures->owns_texture(decal_textures[i])) {
				textures->texture_remove_from_decal_atlas(decal_textures[i]);
			}
			decal_textures[i] = RID();
		}
	}
}

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
	const bool is_indexed144 = owner.config.profile == ExternalOutputConfig::INDEXED_144;
	const uint32_t expected_guard_profile = is_indexed144 ? 2u : 1u;
	// Registry authenticated this stack-only original borrow and READ while
	// holding its lock and the RD mutex. Consume every pointer before return.
	const auto &borrow = p_borrow;
	if (!borrow.previous_view || !borrow.current_host || !borrow.previous_host ||
			!borrow.consumed_proof || !borrow.consumed_tuple ||
			(borrow.previous_no_records ?
					(owner.config.capacity || borrow.previous_records.is_valid() || borrow.previous_records_witness || !borrow.previous_guard_witness ||
							borrow.previous_guard_status.is_null() || p_predecessor_read.buffer != borrow.previous_guard_status ||
							borrow.previous_guard_bytes != 32 || borrow.previous_guard_profile != expected_guard_profile || p_predecessor_read.byte_offset || p_predecessor_read.byte_size != 32) :
					(!borrow.previous_records_witness || borrow.previous_records.is_null() || p_predecessor_read.buffer != borrow.previous_records)) || p_predecessor_read.owned_group ||
			p_predecessor_read.resource_slot == UINT32_MAX || p_predecessor_read.use_index == UINT32_MAX) {
		return ERR_INVALID_DATA;
	}
	const auto &view = *borrow.previous_view;
	if (view.owner != &owner || view.slot >= owner.slot_count || view.slot == p_slot ||
			!view.has_retained_host_snapshot() ||
			(view.get_profile() != ExternalOutputConfig::INSTANCE_192 && view.get_profile() != ExternalOutputConfig::INDEXED_144) ||
			view.get_profile() != owner.config.profile ||
			(view.get_space() != ExternalOutputConfig::WORLD && view.get_space() != ExternalOutputConfig::LOCAL)) {
		return ERR_INVALID_DATA;
	}
	const auto &previous = owner.slots[view.slot];
	const uint64_t proof_bytes = is_indexed144
			? sizeof(ParticlesExternalGuardIndexed144::GeometryProofHeader) + uint64_t(slot.surface_count) * sizeof(ParticlesExternalGuardIndexed144::RibbonSurfaceProof)
			: sizeof(ParticlesExternalGuard::GeometryProofHeader) + uint64_t(slot.surface_count) * sizeof(ParticlesExternalGuard::SurfaceProof);
	if (!previous.scene_cull_bound || !previous.storage || !previous.guard_resources_ready || !previous.guard_proof_payload ||
			previous.guard_proof_bytes != proof_bytes || previous.surface_count != slot.surface_count ||
			!slot.surface_count || slot.surface_count > ExternalOutputConfig::MAX_FLATTENED_SURFACES ||
			previous.request.common.current_host_token.size() != 32 ||
			slot.request.common.previous_consumed_tuple_token.size() != 32 ||
			memcmp(borrow.consumed_tuple->bytes, slot.request.common.previous_consumed_tuple_token.ptr(), 32) != 0) {
		return ERR_INVALID_DATA;
	}
	const auto &records = previous.storage->resources[borrow.previous_no_records ? ExternalOutputStorage::GUARD_STATUS : ExternalOutputStorage::CURRENT_RECORDS];
	if (borrow.previous_no_records) {
		for (uint32_t r : { uint32_t(ExternalOutputStorage::CURRENT_RECORDS), uint32_t(ExternalOutputStorage::PREVIOUS_RECORDS), uint32_t(ExternalOutputStorage::INSTANCE_AUX_PARAMS) }) {
			const auto &absent = previous.storage->resources[r];
			if (absent.bytes || absent.rid.is_valid()) { return ERR_INVALID_DATA; }
		}
		if (is_indexed144) {
			for (uint32_t r : { uint32_t(ExternalOutputStorage::INDEXED_VERTEX_HISTORY_VALIDITY), uint32_t(ExternalOutputStorage::INDEXED_STAGING) }) {
				const auto &absent = previous.storage->resources[r];
				if (absent.bytes || absent.rid.is_valid()) { return ERR_INVALID_DATA; }
			}
		}
		if (borrow.previous_guard_sequence != previous.request.common.present_seq) { return ERR_INVALID_DATA; }
	}
	const RID retained_buffer = borrow.previous_no_records ? borrow.previous_guard_status : borrow.previous_records;
	const auto *buffer = rd->storage_buffer_owner.get_or_null(retained_buffer);
	NativeDisposalWitness::Report witness;
	Error error = rd->_native_disposal_observe(borrow.previous_no_records ? *borrow.previous_guard_witness : *borrow.previous_records_witness, witness);
	const uint64_t expected_record_bytes = borrow.previous_no_records ? 32ull : uint64_t(owner.config.capacity) * (is_indexed144 ? 576ull : 192ull);
	if (error != OK || !buffer || records.rid != retained_buffer || !records.bytes ||
			records.bytes != buffer->size || records.bytes != expected_record_bytes ||
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
	if (is_indexed144) {
		ParticlesExternalGuardIndexed144::GeometryProofHeader previous_proof;
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
			ParticlesExternalGuardIndexed144::RibbonSurfaceProof original;
			memcpy(&original, previous.guard_proof_payload + sizeof(previous_proof) + uint64_t(i) * sizeof(original), sizeof(original));
			auto &surface = snapshot.surfaces.write[i];
			memcpy(surface.previous_displacement_min, original.current_displacement_min, sizeof(surface.previous_displacement_min));
			memcpy(surface.previous_displacement_max, original.current_displacement_max, sizeof(surface.previous_displacement_max));
		}
	} else {
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
			(owner.config.space != ExternalOutputConfig::WORLD && owner.config.space != ExternalOutputConfig::LOCAL) ||
			!slot.scene_cull_bound || !slot.retained_host_snapshot || !slot.current_host.is_finite() ||
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
	// Geometry declarations retain their original spaces. The cull proof is
	// the actual pre-cull world commit, never the immutable config fallback.
	const Error scene_commit = owner.validate_scene_cull(p_slot);
	if (scene_commit != OK) {
		return scene_commit;
	}
	const AABB cull = slot.scene_cull_world;
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
	proof.options[1] = slot.storage->resources[ExternalOutputStorage::INSTANCE_AUX_PARAMS].bytes != 0 ? 1u : 0u;
	error = Guard::configure_multi_mesh_proof(proof, owner.generation_mesh_count);
	if (error != OK) {
		return error;
	}
	Guard::Bindings bindings;
	Vector<RD::ProductionOutputUniformSource> sources;
	uint32_t owned_ordinal = 0;
	for (uint32_t binding = 0; binding < Guard::BINDING_COUNT; ++binding) {
		const bool imported = binding == Guard::SOURCE_COUNT || (binding >= Guard::SOURCE_COUNT_VALIDITY && binding <= Guard::SOURCE_INSTANCE_GATE);
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
			const uint32_t role = binding == Guard::INSTANCE_AUX_PARAMS
					? uint32_t(ExternalOutputStorage::INSTANCE_AUX_PARAMS)
					: (binding <= Guard::OUTPUT_META ? binding : binding - 1);
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
	uint32_t owner_mesh_index = 0;
	for (uint32_t surface = 0; surface < slot.surface_count; ++surface) {
		ExternalOutputOwner::BoundMaterialContact contact;
		error = owner.read_bound_material_contact(surface, contact);
		if (error != OK || !contact.material.zero_displacement) {
			return error != OK ? error : ERR_UNAVAILABLE;
		}
		if (surface > 0 && contact.native_surface == 0) {
			++owner_mesh_index;
		}
		if (owner_mesh_index >= owner.generation_mesh_count || !owner.generation_meshes || owner.generation_meshes[owner_mesh_index].mesh != contact.mesh) {
			return ERR_INVALID_DATA;
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
		error = Guard::configure_surface_mesh_index(surface_proof, proof, owner.generation_mesh_count > 1u ? owner_mesh_index : 0u);
		if (error != OK) {
			return error;
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
	const Guard::Mode modes[3] = { plan.initialize_mode, push.capacity ? plan.validate_mode : plan.finalize_mode, plan.finalize_mode };
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

Error ParticlesStorage::_external_guard_prepare_owned_prefix_indexed144(GDExtensionProductionPhysicalContextRefV1 p_context, RID p_particles, uint32_t p_slot,
		const Vector<RD::ProductionOutputUniformSource> &p_source_reads, const uint64_t *p_full_buffer_bytes, const uint32_t *p_source_words,
		const GDExtensionProductionHostTransformV1 &p_host, const ExternalGuardHistorySnapshot *p_history,
		uint32_t p_source_space, uint32_t p_max_groups_x, ParticlesExternalGuardIndexed144::LayoutPlan &r_plan) {
	using Guard = ParticlesExternalGuardIndexed144;
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
			!external_guard_indexed144 || !external_guard_indexed144->is_prepared() || owner.config.profile != ExternalOutputConfig::INDEXED_144) {
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
			(owner.config.space != ExternalOutputConfig::WORLD && owner.config.space != ExternalOutputConfig::LOCAL) ||
			!slot.scene_cull_bound || !slot.retained_host_snapshot || !slot.current_host.is_finite() ||
			p_source_space != uint32_t(owner.config.space) || !slot.surface_count || !slot.surfaces ||
			!owner.generation_surfaces_complete || owner.generation_material_count != slot.surface_count) {
		return ERR_UNAVAILABLE;
	}
	ExternalOutputOwner::MetadataLayout layout;
	Error error = ExternalOutputOwner::plan_metadata_layout(owner.view_capacity, owner.read_capacity, owner.surface_capacity, layout,
			owner.generation_mesh_count, owner.generation_material_count, owner.binding_capacity, owner.storage_capacity, owner.preparation_capacity);
	const uint64_t proof_size = sizeof(Guard::GeometryProofHeader) + uint64_t(slot.surface_count) * sizeof(Guard::RibbonSurfaceProof);
	const uint64_t proof_stride = sizeof(ParticlesExternalGuard::GeometryProofHeader) + uint64_t(slot.surface_count) * sizeof(ParticlesExternalGuard::SurfaceProof);
	const uint64_t proof_relative = uint64_t(p_slot) * proof_stride;
	if (error != OK || owner.metadata_lease.get_data() != &owner || p_slot >= layout.storage_count ||
			layout.total_bytes > owner.metadata_lease.get_size() || proof_relative > layout.guard_proof_bytes ||
			proof_size > layout.guard_proof_bytes - proof_relative || proof_size > UINT32_MAX) {
		return error != OK ? error : ERR_INVALID_DATA;
	}
	auto *payload = static_cast<uint8_t *>(owner.metadata_lease.get_data()) + layout.guard_proof_offset + proof_relative;
	if (slot.guard_proof_payload && (slot.guard_proof_payload != payload || slot.guard_proof_bytes != proof_size)) {
		return ERR_INVALID_DATA;
	}
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
	const Error scene_commit = owner.validate_scene_cull(p_slot);
	if (scene_commit != OK) {
		return scene_commit;
	}
	ParticlesExternalGuard::GeometryProofHeader host_proof = p_history ? p_history->proof : ParticlesExternalGuard::GeometryProofHeader();
	error = _external_guard_copy_host_inverse(p_host, slot.request.common.current_host_token, slot.current_host, host_proof);
	if (error != OK) {
		return error;
	}
	Guard::GeometryProofHeader proof = {};
	const AABB decl_bounds = owner.config.geometry_bounds.has_volume() ? owner.config.geometry_bounds : slot.request.common.bounds;
	if (!bounds(decl_bounds, proof.current_decl_min, proof.current_decl_max, true) ||
			!bounds(slot.scene_cull_world, proof.cull_min, proof.cull_max, true)) {
		return ERR_INVALID_DATA;
	}
	if (owner.config.motion == ExternalOutputConfig::REQUIRED) {
		for (uint32_t axis = 0; axis < 4; ++axis) {
			proof.previous_decl_min[axis] = p_history ? host_proof.previous_decl_min[axis] : proof.current_decl_min[axis];
			proof.previous_decl_max[axis] = p_history ? host_proof.previous_decl_max[axis] : proof.current_decl_max[axis];
		}
		memcpy(proof.previous_host_inverse_rows, p_history ? host_proof.previous_host_inverse_rows : host_proof.current_host_inverse_rows, sizeof(proof.previous_host_inverse_rows));
	}
	memcpy(proof.current_host_inverse_rows, host_proof.current_host_inverse_rows, sizeof(proof.current_host_inverse_rows));
	proof.source_accessor[0] = p_source_words[0];
	proof.source_accessor[3] = p_source_space;
	proof.options[0] = owner.config.motion == ExternalOutputConfig::REQUIRED ? 1u : 0u;

	auto role_for_binding = [](uint32_t p_binding) -> uint32_t {
		switch (p_binding) {
			case Guard::CURRENT_VERTICES:
				return ExternalOutputStorage::CURRENT_RECORDS;
			case Guard::ALIGNED_PREVIOUS_VERTICES:
				return ExternalOutputStorage::PREVIOUS_RECORDS;
			case Guard::VERTEX_HISTORY_VALIDITY:
				return ExternalOutputStorage::INDEXED_VERTEX_HISTORY_VALIDITY;
			case Guard::INDEX_STAGING:
				return ExternalOutputStorage::INDEXED_STAGING;
			case Guard::OUTPUT_META:
				return ExternalOutputStorage::OUTPUT_META;
			case Guard::GUARD_SCRATCH:
				return ExternalOutputStorage::GUARD_SCRATCH;
			case Guard::GUARD_STATUS:
				return ExternalOutputStorage::GUARD_STATUS;
			case Guard::PROTECTED_COMMANDS:
				return ExternalOutputStorage::PROTECTED_COMMANDS;
			case Guard::GEOMETRY_PROOF:
				return ExternalOutputStorage::GEOMETRY_PROOF;
			default:
				return UINT32_MAX;
		}
	};
	auto ordinal_for_role = [&](uint32_t p_role) -> uint32_t {
		uint32_t ord = 0;
		for (uint32_t r = 0; r < p_role; ++r) {
			ord += slot.storage->resources[r].bytes != 0;
		}
		return ord;
	};

	Guard::Bindings bindings;
	Vector<RD::ProductionOutputUniformSource> sources;
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
			const uint32_t role = role_for_binding(binding);
			if (role == UINT32_MAX) {
				return ERR_INVALID_DATA;
			}
			const ExternalOutputStorage::Resource &resource = slot.storage->resources[role];
			bindings.buffers[binding] = { resource.rid, resource.bytes };
			if (!resource.bytes) {
				continue;
			}
			source.buffer = resource.rid;
			source.owned_group = slot.owned_group;
			source.owned_ordinal = ordinal_for_role(role);
			source.byte_size = resource.bytes;
		}
		sources.push_back(source);
	}

	Guard::PushConstant push;
	push.capacity = owner.config.capacity;
	push.sequence_low = uint32_t(slot.request.common.present_seq);
	push.sequence_high = uint32_t(slot.request.common.present_seq >> 32);
	push.surface_count = slot.surface_count;
	push.segment_base = 0;
	push.segment_count = push.capacity;
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
	auto *materials = MaterialStorage::get_singleton();
	if (!materials) {
		return ERR_UNAVAILABLE;
	}
	for (uint32_t surface = 0; surface < slot.surface_count; ++surface) {
		const auto &selection = owner.generation_materials[surface];
		if (!selection.lease.is_active()) {
			return ERR_UNAVAILABLE;
		}
		MaterialStorage::NativeSelectionShaderObservation material_obs;
		error = materials->material_selection_get_shader_observation(selection.lease, material_obs);
		if (error != OK || !material_obs.zero_displacement) {
			return error != OK ? error : ERR_UNAVAILABLE;
		}
		Guard::RibbonSurfaceProof surface_proof = {};
		if (p_history) {
			memcpy(surface_proof.previous_displacement_min, p_history->surfaces[surface].previous_displacement_min, sizeof(surface_proof.previous_displacement_min));
			memcpy(surface_proof.previous_displacement_max, p_history->surfaces[surface].previous_displacement_max, sizeof(surface_proof.previous_displacement_max));
		}
		surface_proof.command[0] = 1;
		surface_proof.command[1] = 6;
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
	slot.guard_proof_payload = payload;
	slot.guard_proof_bytes = uint32_t(proof_size);
	const Guard::Mode modes[3] = { plan.initialize_mode, push.capacity ? plan.validate_mode : plan.finalize_mode, plan.finalize_mode };
	for (uint32_t ordinal = 0; ordinal < mode_count; ++ordinal) {
		const Guard::Mode mode = modes[ordinal];
		if (slot.guard_uniform_sets[mode].is_valid()) {
			continue;
		}
		error = owner.device->_production_output_owned_uniform_set_create(p_context, slot.original_pin, slot.guard_owned_group, ordinal,
				VectorView<RD::Uniform>(plan.uniforms.ptr(), plan.uniforms.size()), external_guard_indexed144->get_shader(mode), 0, slot.guard_uniform_sets[mode], slot.guard_uniform_witnesses[mode],
				VectorView<RD::ProductionOutputUniformSource>(sources.ptr(), sources.size()));
		if (error != OK) {
			return error;
		}
	}
	slot.previous_host = p_history ? p_history->previous_host : slot.current_host;
	slot.guard_resources_ready = true;
	r_plan = plan;
	return OK;
}

Error ParticlesStorage::particles_external_validate_scene_generation(RID p_particles, uint64_t p_generation, const RenderingDevice *p_device) const {
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
	return OK;
}

Error ParticlesStorage::particles_scene_validate_receiver(RID p_particles, uint64_t p_generation, const RenderingDevice *p_device) const {
	const Error validated = particles_external_validate_scene_generation(p_particles, p_generation, p_device);
	if (validated != OK) {
		return validated;
	}
	const ExternalOutputOwner &owner = *particles_owner.get_or_null(p_particles)->external;
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
			const Error err = owner.cancel_slot(i);
			if (err == OK && mesh_light_sort_rd) {
				mesh_light_sort_rd->unregister_external_light_slot(p_particles.get_id(), owner.generation, i);
			}
			if (err == OK) {
				if (ParticlesDecalBridgeRD *decal_bridge = get_decal_bridge_rd()) {
					decal_bridge->unregister_external_decal_slot(p_particles.get_id(), owner.generation, i);
					if (owner.config.profile == ExternalOutputConfig::PARTICLE_DECAL_128 &&
							owner.state == ExternalOutputOwner::GenerationState::ACTIVE) {
						uint32_t fallback_slot = UINT32_MAX;
						uint64_t fallback_seq = 0;
						uint64_t fallback_lease = 0;
						for (uint32_t s = 0; s < owner.slot_count; ++s) {
							const ExternalOutputOwner::Slot &cand = owner.slots[s];
							if ((cand.state != ExternalOutputOwner::LeaseState::ADOPTED &&
										cand.state != ExternalOutputOwner::LeaseState::SEALED_RECORDED) ||
									cand.publication_closed || cand.pass_uses_closed || !cand.storage) {
								continue;
							}
							if (fallback_slot == UINT32_MAX || cand.request.common.present_seq > fallback_seq ||
									(cand.request.common.present_seq == fallback_seq && cand.lease_serial > fallback_lease)) {
								fallback_slot = s;
								fallback_seq = cand.request.common.present_seq;
								fallback_lease = cand.lease_serial;
							}
						}
						if (fallback_slot != UINT32_MAX) {
							decal_bridge->register_sealed_external_decal_slot(
									BUILD_EXTERNAL_DECAL_SLOT_REGISTRATION(p_particles.get_id(), owner, fallback_slot));
						}
					}
				}
			}
			return err;
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
	if (mesh_light_sort_rd) {
		mesh_light_sort_rd->unregister_external_light_owner(p_particles.get_id());
	}
	if (ParticlesDecalBridgeRD *decal_bridge = get_decal_bridge_rd()) {
		decal_bridge->unregister_external_decal_owner(p_particles.get_id());
	}
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
	if (!valid_abi(p_input.abi, sizeof(p_input)) || p_input.profile < 1 || p_input.profile > 5 || p_input.space < 1 || p_input.space > 2 || p_input.motion > 1 || p_input.query_enabled > 1 || (p_input.pass_mask & ~uint32_t(0x1f)) || p_input.surfaces.reserved || p_input.surface_materials.reserved ||
			(p_input.surfaces.count && !p_input.surfaces.data) || (p_input.surface_materials.count && !p_input.surface_materials.data)) {
		return false;
	}
	if (p_input.capacity > (1u << 20) || (p_input.profile == 2 && p_input.capacity > (1u << 18)) || ((p_input.profile == 4 || p_input.profile == 5) && p_input.capacity > 256) || !p_input.max_views || p_input.max_views > 8 || !p_input.max_inflight || p_input.max_inflight > 4) {
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
	if (p_input.profile == 4 && (p_input.surfaces.count || p_input.surface_materials.count || p_input.pass_mask || p_input.motion)) {
		return false;
	}
	if (p_input.profile == 5 && (p_input.surfaces.count || p_input.surface_materials.count != 4 || p_input.pass_mask || p_input.motion)) {
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
	const Error selection_error = (config.profile == ExternalOutputConfig::PARTICLE_DECAL_128)
			? ExternalOutputOwner::validate_decal128_selection(config, surface_count)
			: ExternalOutputOwner::validate_native_selection(config, surface_count);
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

Error ParticlesStorage::_external_indexed144_preparation_error() const {
	const Error prepared = get_external_guard_indexed144_preparation_error();
	if (prepared != OK) {
		return prepared;
	}
	return is_external_guard_indexed144_prepared() ? OK : ERR_UNAVAILABLE;
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
	} else if (config.profile == ExternalOutputConfig::INDEXED_144) {
		const Error prepared = _external_indexed144_preparation_error();
		if (prepared != OK) {
			return prepared;
		}
		planned = ExternalOutputOwner::plan_generation_selection_layout(config, limits.view_lease_capacity, limits.read_lease_capacity, limits.surface_lease_capacity, layout);
	} else if (config.profile == ExternalOutputConfig::PARTICLE_LIGHT_64) {
		planned = ExternalOutputOwner::plan_generation_selection_layout(config, limits.view_lease_capacity, limits.read_lease_capacity, limits.surface_lease_capacity, layout);
	} else if (config.profile == ExternalOutputConfig::PARTICLE_DECAL_128) {
		planned = ExternalOutputOwner::plan_decal128_selection_layout(config, limits.view_lease_capacity, limits.read_lease_capacity, limits.surface_lease_capacity, layout);
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
	const bool render_selection = p_generation_selection &&
			(p_config.profile == ExternalOutputConfig::INSTANCE_192 ||
					p_config.profile == ExternalOutputConfig::INDEXED_144 ||
					p_config.profile == ExternalOutputConfig::PARTICLE_LIGHT_64 ||
					p_config.profile == ExternalOutputConfig::PARTICLE_DECAL_128);
	if (!rd || (!render_selection && p_config.profile != ExternalOutputConfig::SCENE_ONLY) || !next_external_generation) {
		r_result.status = output_status(ERR_UNAVAILABLE, "native_generation_unavailable");
		return;
	}
	if (render_selection) {
		Error prepared = OK;
		if (p_config.profile == ExternalOutputConfig::INSTANCE_192) {
			prepared = _external_instance_preparation_error();
		} else if (p_config.profile == ExternalOutputConfig::INDEXED_144) {
			prepared = _external_indexed144_preparation_error();
		}
		if (prepared != OK) {
			r_result.status = output_status(prepared, "native_profile_preparation_required");
			return;
		}
	}
	ExternalOutputOwner::MetadataLayout layout;
	const Error planned = render_selection
			? ((p_config.profile == ExternalOutputConfig::PARTICLE_DECAL_128)
							  ? ExternalOutputOwner::plan_decal128_selection_layout(p_config, p_view_capacity, p_read_capacity, p_surface_capacity, layout)
							  : ExternalOutputOwner::plan_generation_selection_layout(p_config, p_view_capacity, p_read_capacity, p_surface_capacity, layout))
			: ExternalOutputOwner::plan_metadata_layout(p_view_capacity, p_read_capacity, p_surface_capacity, layout);
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
	auto *owner = new (p_metadata.get_data()) ExternalOutputOwner(p_config, generation, true, render_selection);
	owner->device = rd;
	const Error directory = (render_selection && p_config.profile == ExternalOutputConfig::PARTICLE_DECAL_128)
			? owner->install_decal128_metadata_directory(p_context, p_request, registry, p_metadata, p_view_capacity, p_read_capacity, p_surface_capacity)
			: owner->install_metadata_directory(p_context, p_request, registry, p_metadata, p_view_capacity, p_read_capacity, p_surface_capacity, render_selection);
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
	if (render_selection) {
		Error selected = (p_config.profile == ExternalOutputConfig::PARTICLE_DECAL_128)
				? owner->acquire_decal128_selection(p_context, p_request)
				: owner->acquire_generation_selection(p_context, p_request);
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
	if (owner.config.profile == ExternalOutputConfig::SCENE_ONLY ||
			owner.config.profile == ExternalOutputConfig::PARTICLE_LIGHT_64 ||
			owner.config.profile == ExternalOutputConfig::PARTICLE_DECAL_128) {
		if ((owner.config.profile == ExternalOutputConfig::PARTICLE_LIGHT_64 ||
					owner.config.profile == ExternalOutputConfig::PARTICLE_DECAL_128) &&
				(owner.generation_material_count || owner.generation_mesh_count || owner.retained_material_count || owner.retained_mesh_count || owner.retained_surface_count)) {
			return ERR_INVALID_DATA;
		}
		return OK;
	}
	const bool is_instance192 = owner.config.profile == ExternalOutputConfig::INSTANCE_192;
	const bool is_indexed144 = owner.config.profile == ExternalOutputConfig::INDEXED_144;
	if ((!is_instance192 && !is_indexed144) || !owner.generation_selection_complete || !owner.generation_surfaces_complete ||
			!owner.generation_materials || !owner.generation_material_count ||
			owner.generation_material_count > ExternalOutputConfig::MAX_FLATTENED_SURFACES ||
			owner.retained_material_count != owner.generation_material_count ||
			(is_instance192
					? (!owner.generation_meshes || !owner.generation_mesh_count ||
							owner.generation_mesh_count > ExternalOutputConfig::MAX_MESH_RIDS ||
							owner.retained_mesh_count != owner.generation_mesh_count ||
							owner.retained_surface_count != owner.generation_material_count)
					: (owner.generation_meshes || owner.generation_mesh_count || owner.retained_mesh_count || owner.retained_surface_count))) {
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
		if (source.material.is_null() || !source.generation || !source.lease.is_active() ||
				(is_instance192
						? (source.mesh.is_null() || source.native_surface == UINT32_MAX || !source.geometry_source.is_valid())
						: (!source.mesh.is_null() || source.native_surface != UINT32_MAX || source.geometry_source.is_valid()))) {
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

void ParticlesStorage::particles_external_query_owner_framebuffer_reads(RID p_particles, bool &r_uses_screen_texture, bool &r_uses_depth_texture) const {
	auto *rd = RenderingDevice::get_singleton();
	auto *materials = MaterialStorage::get_singleton();
	if (!rd || !rd->native_immutable_is_render_thread() || !materials) {
		return;
	}
	const Particles *particles = particles_owner.get_or_null(p_particles);
	if (!particles || !particles->external) {
		return;
	}
	const ExternalOutputOwner &owner = *particles->external;
	if (!owner.admits_work() || !owner.generation_selection_complete || !owner.generation_materials ||
			(owner.config.profile != ExternalOutputConfig::INSTANCE_192 && owner.config.profile != ExternalOutputConfig::INDEXED_144)) {
		return;
	}
	for (uint32_t i = 0; i < owner.generation_material_count; ++i) {
		const RID mat_rid = owner.generation_materials[i].material;
		if (mat_rid.is_null()) {
			continue;
		}
		const MaterialStorage::ShaderData *shader_data = materials->material_get_shader_data(mat_rid);
		if (shader_data) {
			r_uses_screen_texture |= shader_data->uses_screen_texture_backbuffer();
			r_uses_depth_texture |= shader_data->uses_depth_texture_backbuffer();
		}
	}
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
Error ParticlesStorage::particles_external_generated_surfaces_for_view(RID p_particles, const ::RenderDataRD &p_render_data,
		uint32_t &r_slot, uint32_t &r_count, bool p_allow_adopted_replay, bool p_defer_transparent_replay) const {
	auto *rd = RenderingDevice::get_singleton();
	if (!rd || !rd->native_immutable_is_render_thread() || p_render_data.reflection_probe.is_valid() ||
			!::RenderDataRD::admission_succeeded(p_render_data.admit_camera_status())) {
		return ERR_UNAUTHORIZED;
	}
	const auto *particles = particles_owner.get_or_null(p_particles);
	if (!particles || !particles->external || particles->external->device != rd) {
		return ERR_UNAVAILABLE;
	}
	const auto &owner = *particles->external;
	const Ref<RendererViewport::Viewport::CameraCapture> capture = p_render_data.get_native_camera_source();
	uint32_t slot = UINT32_MAX;
	Error selected = owner.find_generated_slot_for_capture(capture, slot);
	bool replayed_across_capture = false;
	const bool can_replay_adopted = p_allow_adopted_replay &&
			!(p_defer_transparent_replay && (owner.config.pass_mask & ExternalOutputConfig::PASS_TRANSPARENT) != 0);
	if (selected == ERR_UNAVAILABLE && can_replay_adopted && capture.is_valid() &&
			owner.admits_work() && owner.config.profile == ExternalOutputConfig::INDEXED_144 &&
			owner.config.motion == ExternalOutputConfig::DISABLED && owner.config.capacity > 0) {
		const auto &camera_token = capture->get_token();
		uint32_t best_ready_slot = UINT32_MAX;
		uint64_t best_ready_present_seq = 0;
		for (uint32_t s = 0; s < owner.slot_count; ++s) {
			const auto &cand_slot = owner.slots[s];
			if ((cand_slot.state != ExternalOutputOwner::LeaseState::SEALED_RECORDED &&
						cand_slot.state != ExternalOutputOwner::LeaseState::ADOPTED) ||
					cand_slot.publication_closed || cand_slot.pass_uses_closed ||
					cand_slot.generated_indexed_mesh.is_null() || !cand_slot.storage ||
					!cand_slot.scene_cull_bound || !cand_slot.guard_resources_ready) {
				continue;
			}
			const uint32_t view_idx = s / owner.config.max_inflight;
			if (view_idx >= owner.config.max_views || !owner.views[view_idx].occupied || owner.views[view_idx].closed) {
				continue;
			}
			if (cand_slot.request.common.generation != owner.generation ||
					owner.views[view_idx].epoch != cand_slot.request.view_epoch ||
					cand_slot.request.view_epoch != camera_token.view_epoch ||
					cand_slot.request.common.current_camera_token.view_id != camera_token.view_id ||
					cand_slot.request.common.current_camera_token.view_epoch != camera_token.view_epoch) {
				continue;
			}
			if (!cand_slot.original_registry || cand_slot.original_pin.is_empty() ||
					!cand_slot.original_registry->production_output_pin_matches(cand_slot.original_pin, cand_slot.original_request, *rd)) {
				continue;
			}
			bool has_ready_row = false;
			for (uint32_t r = 0; r < owner.preparation_capacity; ++r) {
				const auto &row = owner.preparation_rows[r];
				if (row.owner == &owner && row.owner_slot == s &&
						row.tuple_state == ExternalPipelinePreparation::TupleState::READY &&
						row.finish_complete && row.finish_error == OK && row.backend_error == OK && row.pipeline_sealed) {
					has_ready_row = true;
					break;
				}
			}
			if (cand_slot.state == ExternalOutputOwner::LeaseState::ADOPTED) {
				if (!has_ready_row) {
					continue;
				}
			} else if (!has_ready_row &&
					!cand_slot.original_registry->production_output_slot_cross_capture_preparable(
							cand_slot.original_pin, cand_slot.original_request, *rd)) {
				continue;
			}
			if (best_ready_slot == UINT32_MAX || cand_slot.request.common.present_seq >= best_ready_present_seq) {
				best_ready_slot = s;
				best_ready_present_seq = cand_slot.request.common.present_seq;
			}
		}
		if (best_ready_slot != UINT32_MAX) {
			slot = best_ready_slot;
			selected = OK;
			replayed_across_capture = true;
		}
	}
	if (selected != OK) {
		return selected;
	}
	const Error scene = owner.validate_scene_cull(slot, replayed_across_capture);
	if (scene != OK) {
		return scene;
	}
	r_slot = slot;
	r_count = owner.slots[slot].surface_count;
	return OK;
}

Error ParticlesStorage::particles_external_slot_surface_acquire(RID p_particles, uint32_t p_slot, uint32_t p_surface, ExternalSurfaceSelection &r_selection) {
	auto *rd = RenderingDevice::get_singleton();
	if (!rd || !rd->native_immutable_is_render_thread()) {
		return ERR_UNAUTHORIZED;
	}
	Particles *particles = particles_owner.get_or_null(p_particles);
	return particles && particles->external ? particles->external->slot_surface_acquire(p_slot, p_surface, r_selection) : ERR_DOES_NOT_EXIST;
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
	if (owner.device != device ||
			(owner.config.profile != ExternalOutputConfig::INSTANCE_192 && owner.config.profile != ExternalOutputConfig::INDEXED_144) ||
			!owner.config.capacity || view.slot >= owner.slot_count) {
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
			!owner.generation_registry || !owner.generation_materials || !owner.surface_cells || !owner.generation_surfaces_complete ||
			!surface.geometry_source || !surface.geometry_source->is_valid() || surface.flattened_surface >= owner.generation_material_count) {
		return ERR_UNAUTHORIZED;
	}
	auto &slot = owner.slots[view.slot];
	const auto &cell = owner.surface_cells[surface.cell];
	const auto &original_source = owner.generation_materials[surface.flattened_surface];
	if (owner.config.profile == ExternalOutputConfig::INDEXED_144) {
		if (owner.retained_surface_count != 0 || !slot.surfaces || surface.flattened_surface >= slot.surface_count ||
				cell.consumer != &surface || cell.serial != surface.serial || cell.slot != view.slot || cell.surface != surface.flattened_surface || cell.pin.is_empty() ||
				surface.geometry_source != &slot.surfaces[surface.flattened_surface].geometry || !surface.geometry_source->is_external() ||
				surface.mesh.is_valid() || surface.native_surface != UINT32_MAX ||
				surface.material_selection != original_source.material || surface.material_generation != original_source.generation ||
				!slot.original_registry || !slot.original_registry->production_output_pin_matches(cell.pin, slot.original_request, *device)) {
			return ERR_UNAUTHORIZED;
		}
	} else {
		if (surface.flattened_surface >= owner.retained_surface_count ||
				cell.consumer != &surface || cell.serial != surface.serial || cell.surface != surface.flattened_surface || cell.pin.is_empty() ||
				surface.geometry_source != &original_source.geometry_source || surface.mesh != original_source.mesh || surface.native_surface != original_source.native_surface ||
				surface.material_selection != original_source.material || surface.material_generation != original_source.generation ||
				!owner.generation_registry->production_output_pin_matches(cell.pin, owner.generation_request, *device)) {
			return ERR_UNAUTHORIZED;
		}
	}
	// The pre-list pass has no factory work for an empty color packet.
	if (p_pre_list && (owner.config.profile == ExternalOutputConfig::INSTANCE_192 || owner.config.profile == ExternalOutputConfig::INDEXED_144) && owner.config.capacity == 0) {
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
		// Pre-scope refusal touched no RD/pipeline/disposal state: release the freshly
		// selected bank row back to UNSELECTED so transient refusals (e.g. ERR_BUSY while
		// awaiting OWNER_CONSUMED or cross-capture staged_return mismatch) never permanently
		// poison this (slot, surface, selection) tuple to FAILED on future frames.
		{
			GDExtensionDeviceExit::Registry::Lock lock(*slot.original_registry);
			owner.unselect_preparation_tuple(view.slot, index);
		}
		out = nullptr;
		return error;
	}
	// Synchronous real backend body; it cannot retain this stack scope. Every
	// actual successful/failed hold stays in the original stable row.
	row.backend_error = body(userdata, surface, row);
	if (row.backend_error == OK && row.owned_pipeline_root.is_null()) {
		row.backend_error = ERR_CANT_CREATE;
	}
	if (row.backend_error == OK) {
		for (int attempt = 0; attempt < 32; ++attempt) {
			row.backend_error = _external_transforms_prepare_owned_prefix(row);
			if (row.backend_error != ERR_BUSY) {
				break;
			}
		}
	}
	row.finish_error = slot.original_registry->production_output_preparation_finish(scope);
	if (row.finish_error == OK && row.backend_error == OK && row.transforms_uniform_set.is_valid() &&
			!row.transforms_source_group && row.transforms_owned_group && row.transforms_set_format &&
			row.transforms_source_count > 0 && row.transforms_source_count <= 3 &&
			slot.cached_transforms_count < ExternalOutputOwner::Slot::CACHED_TRANSFORMS_CAPACITY) {
		auto &entry = slot.cached_transforms_sets[slot.cached_transforms_count++];
		entry.uniform_set = row.transforms_uniform_set;
		entry.owned_group = row.transforms_owned_group;
		entry.set_format = row.transforms_set_format;
		entry.buffer_count = row.transforms_source_count;
		for (uint32_t i = 0; i < row.transforms_source_count; ++i) {
			entry.buffers[i] = row.transforms_source_buffers[i];
		}
	}
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
	if (view.owner && (view.owner->config.profile == ExternalOutputConfig::INSTANCE_192 || view.owner->config.profile == ExternalOutputConfig::INDEXED_144) && view.owner->config.capacity == 0) {
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
	if (view.owner && view.slot < view.owner->slot_count) {
		const auto &slot = view.owner->slots[view.slot];
		if (slot.state == ExternalOutputOwner::LeaseState::ADOPTED &&
				(slot.fast_refresh_active || slot.native_camera_source != p_render_data->get_native_camera_source()) &&
				slot.storage && !slot.publication_closed && !slot.pass_uses_closed) {
			// Already-adopted immutable slot replayed on a subsequent frame while the
			// producer computes the next step: draw directly from the prepared pipeline
			// & ProtectedCommands without allocating a new ReadCell or blocking OBSERVE.
			const ExternalPipelinePreparation *ready_row = nullptr;
			for (uint32_t i = 0; i < view.owner->preparation_capacity; ++i) {
				const auto &cand = view.owner->preparation_rows[i];
				if (cand.owner == view.owner && cand.owner_slot == view.slot && cand.tuple_surface == surface.flattened_surface &&
						cand.tuple_state == ExternalPipelinePreparation::TupleState::READY && cand.finish_complete &&
						cand.finish_error == OK && cand.backend_error == OK && cand.pipeline_sealed &&
						cand.selection == p_pipeline) {
					ready_row = &cand;
					break;
				}
			}
			if (ready_row && ready_row->selected_geometry < 2 && ready_row->created_pipeline.is_valid() && ready_row->transforms_uniform_set.is_valid()) {
				auto *rd = RD::get_singleton();
				const RID commands = slot.storage->resources[ExternalOutputStorage::PROTECTED_COMMANDS].rid;
				ExternalPacketBinding dummy_binding;
				dummy_binding.selection = ready_row->selection;
				dummy_binding.ubershader = ready_row->ubershader;
				const void *push = nullptr;
				uint32_t push_bytes = 0;
				if (rd && commands.is_valid() && p_prepare_push(p_push_context, dummy_binding, push, push_bytes) == OK) {
					const Error replay_err = rd->_production_output_replay_adopted_record(*view.owner, view.slot, *ready_row, surface.flattened_surface, p_list, push, push_bytes);
					const Error view_error = view.release_checked();
					return replay_err != OK ? replay_err : view_error;
				}
			}
		}
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

bool ParticlesStorage::fast_refresh_invert_affine_f32(const float p_affine[12], float r_inverse[12]) {
	for (uint32_t i = 0; i < 12; ++i) {
		if (!Math::is_finite(p_affine[i])) {
			return false;
		}
	}
	auto write_fallback_inverse = [&]() {
		// Singular or near-singular host_affine (e.g. temporary scale == 0 during
		// spawn-in animation, hide, or pause): preserve a finite normal-or-zero
		// fallback inverse with identity rotation/scale and negated translation
		// so Tier 2 fast-refresh stays resident without shader NUMERIC_FAULT.
		for (uint32_t row = 0; row < 3; ++row) {
			for (uint32_t column = 0; column < 3; ++column) {
				r_inverse[row * 4 + column] = (row == column) ? 1.0f : 0.0f;
			}
			const float neg_t = -p_affine[row * 4 + 3];
			r_inverse[row * 4 + 3] = (!Math::is_finite(neg_t) || Math::abs(neg_t) < std::numeric_limits<float>::min()) ? 0.0f : neg_t;
		}
	};
	const double determinant = AffineF32::exact_basis_determinant(p_affine);
	if (!Math::is_finite(determinant) || Math::abs(determinant) < 1e-12) {
		write_fallback_inverse();
		return true;
	}
	double basis[3][3];
	for (uint32_t row = 0; row < 3; ++row) {
		for (uint32_t column = 0; column < 3; ++column) {
			basis[row][column] = double(p_affine[row * 4 + column]);
		}
	}
	double inverse[3][4] = {};
	for (uint32_t row = 0; row < 3; ++row) {
		for (uint32_t column = 0; column < 3; ++column) {
			inverse[column][row] = basis[(row + 1) % 3][(column + 1) % 3] * basis[(row + 2) % 3][(column + 2) % 3] -
					basis[(row + 1) % 3][(column + 2) % 3] * basis[(row + 2) % 3][(column + 1) % 3];
		}
	}
	for (uint32_t row = 0; row < 3; ++row) {
		for (uint32_t column = 0; column < 3; ++column) {
			inverse[row][column] /= determinant;
		}
		float numerator_affine[12];
		for (uint32_t i = 0; i < 12; ++i) {
			numerator_affine[i] = p_affine[i];
		}
		for (uint32_t column = 0; column < 3; ++column) {
			numerator_affine[column * 4 + row] = p_affine[column * 4 + 3];
		}
		inverse[row][3] = -AffineF32::exact_basis_determinant(numerator_affine) / determinant;
	}
	float candidate[12] = {};
	for (uint32_t row = 0; row < 3; ++row) {
		for (uint32_t column = 0; column < 4; ++column) {
			if (!Math::is_finite(inverse[row][column]) || Math::abs(inverse[row][column]) > double(std::numeric_limits<float>::max())) {
				write_fallback_inverse();
				return true;
			}
			const float val = float(inverse[row][column]);
			if (!Math::is_finite(val)) {
				write_fallback_inverse();
				return true;
			}
			// Flush individual subnormal components to 0.0f rather than discarding
			// the entire valid inverse matrix.
			candidate[row * 4 + column] = (Math::abs(val) < std::numeric_limits<float>::min()) ? 0.0f : val;
		}
	}
	memcpy(r_inverse, candidate, sizeof(candidate));
	return true;
}

void ParticlesStorage::fast_refresh_encode_light64_guard_status(uint64_t p_present_seq, uint32_t r_words[8]) {
	// Canonical GUARD_STATUS ABI layout matching gdextension_production_output.cpp
	// and ANGE validate_output_guard_status32:
	// [0..3] = fault/status/reserved zeros, [4..5] = present_seq (lo, hi), [6] = finalized (1), [7] = 0.
	r_words[0] = 0u;
	r_words[1] = 0u;
	r_words[2] = 0u;
	r_words[3] = 0u;
	r_words[4] = uint32_t(p_present_seq);
	r_words[5] = uint32_t(p_present_seq >> 32);
	r_words[6] = 1u;
	r_words[7] = 0u;
}

namespace {

Transform3D fast_refresh_decode_transform(const float p_affine[12]) {
	Transform3D xf;
	for (uint32_t row = 0; row < 3; ++row) {
		for (uint32_t col = 0; col < 3; ++col) {
			xf.basis.rows[row][col] = real_t(p_affine[row * 4 + col]);
		}
		xf.origin[row] = real_t(p_affine[row * 4 + 3]);
	}
	return xf;
}

} // namespace

int32_t ParticlesStorage::particles_external_begin_fast_refresh_v1(const GDExtensionFastRefreshBeginInputV1 *p_input, GDExtensionFastRefreshTargetsV1 *p_out_targets) {
	if (p_out_targets) {
		*p_out_targets = {};
		p_out_targets->abi_version = 1;
		p_out_targets->struct_size = sizeof(GDExtensionFastRefreshTargetsV1);
		p_out_targets->native_error = int32_t(ERR_INVALID_PARAMETER);
		p_out_targets->slot_index = UINT32_MAX;
	}
	auto fail = [&](Error p_err) -> int32_t {
		if (p_out_targets) {
			p_out_targets->native_error = int32_t(p_err);
		}
		return int32_t(p_err);
	};
	if (!p_input || !p_out_targets || p_input->abi_version != 1 || p_input->struct_size != sizeof(GDExtensionFastRefreshBeginInputV1) ||
			p_input->reserved != 0 || p_input->reserved2 != 0 || p_input->has_host_affine > 1 ||
			p_input->particles_rid == 0 || p_input->next_present_seq == 0) {
		return fail(ERR_INVALID_PARAMETER);
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!rs || !rs->is_on_render_thread() || !rd || rs->get_rendering_device() != rd) {
		return fail(ERR_UNAUTHORIZED);
	}
	if (rd->draw_list.active || rd->compute_list.active || rd->raytracing_list.active) {
		return fail(ERR_BUSY);
	}
	const RID particles_rid = RID::from_uint64(p_input->particles_rid);
	Particles *particles = particles_owner.get_or_null(particles_rid);
	if (!particles || !particles->external) {
		return fail(ERR_DOES_NOT_EXIST);
	}
	ExternalOutputOwner &owner = *particles->external;
	if (owner.device != rd || owner.state != ExternalOutputOwner::GenerationState::ACTIVE) {
		return fail(ERR_UNAVAILABLE);
	}
	if (owner.config.profile != ExternalOutputConfig::INSTANCE_192 &&
			owner.config.profile != ExternalOutputConfig::INDEXED_144 &&
			owner.config.profile != ExternalOutputConfig::PARTICLE_LIGHT_64 &&
			owner.config.profile != ExternalOutputConfig::PARTICLE_DECAL_128) {
		return fail(ERR_UNAVAILABLE);
	}
	uint32_t best_slot = UINT32_MAX;
	bool best_is_adopted = false;
	uint64_t best_present_seq = 0;
	uint64_t best_lease_serial = 0;
	for (uint32_t i = 0; i < owner.slot_count; ++i) {
		const ExternalOutputOwner::Slot &cand = owner.slots[i];
		const bool is_adopted = cand.state == ExternalOutputOwner::LeaseState::ADOPTED;
		const bool is_sealed = cand.state == ExternalOutputOwner::LeaseState::SEALED_RECORDED;
		if ((!is_adopted && !is_sealed) || cand.storage == nullptr ||
				cand.publication_closed || cand.pass_uses_closed ||
				cand.original_pin.is_empty() || owner.preparation_request(i) == nullptr) {
			continue;
		}
		if (best_slot == UINT32_MAX || (is_adopted && !best_is_adopted) ||
				(is_adopted == best_is_adopted && (cand.request.common.present_seq > best_present_seq ||
				(cand.request.common.present_seq == best_present_seq && cand.lease_serial > best_lease_serial)))) {
			best_slot = i;
			best_is_adopted = is_adopted;
			best_present_seq = cand.request.common.present_seq;
			best_lease_serial = cand.lease_serial;
		}
	}
	if (best_slot == UINT32_MAX) {
		return fail(ERR_UNAVAILABLE);
	}
	ExternalOutputOwner::Slot &slot = owner.slots[best_slot];
	if (p_input->has_host_affine) {
		float cur_inv[12] = {};
		float prev_inv[12] = {};
		if (!fast_refresh_invert_affine_f32(p_input->current_host_affine, cur_inv) ||
				!fast_refresh_invert_affine_f32(p_input->previous_host_affine, prev_inv)) {
			return fail(ERR_INVALID_DATA);
		}
		slot.current_host = fast_refresh_decode_transform(p_input->current_host_affine);
		slot.previous_host = fast_refresh_decode_transform(p_input->previous_host_affine);
		// Refresh committed scene cull bounds for ALL profiles (including PARTICLE_LIGHT_64,
		// which has no GEOMETRY_PROOF buffer) so moving light emitters keep fresh world_bounds.
		bool cull_updated = false;
		if (slot.scene_cull_bound) {
			if (auto *scene = static_cast<RendererSceneCull *>(RSG::scene)) {
				RendererSceneCull::NativeParticleSceneFacts actual;
				if (scene->instance_get_committed_particle_scene_facts(slot.scene_cull_instance, slot.scene_cull_particles, actual) == OK &&
						actual.scenario == slot.scene_cull_scenario &&
						actual.world_aabb.is_finite() && actual.world_aabb.get_end().is_finite() && actual.world_aabb.has_volume()) {
					slot.scene_cull_version = actual.version;
					slot.scene_cull_local = actual.local_aabb;
					slot.scene_cull_world = actual.world_aabb;
					cull_updated = true;
				}
			}
		}
		const RID proof_rid = slot.storage->resources[ExternalOutputStorage::GEOMETRY_PROOF].rid;
		if (slot.guard_proof_payload != nullptr &&
				slot.guard_proof_bytes >= sizeof(ParticlesExternalGuard::GeometryProofHeader) &&
				proof_rid.is_valid()) {
			ParticlesExternalGuard::GeometryProofHeader header;
			memcpy(&header, slot.guard_proof_payload, sizeof(header));
			memcpy(header.current_host_inverse_rows, cur_inv, sizeof(cur_inv));
			memcpy(header.previous_host_inverse_rows, prev_inv, sizeof(prev_inv));
			if (cull_updated) {
				const Vector3 end = slot.scene_cull_world.get_end();
				for (uint32_t axis = 0; axis < 3; ++axis) {
					header.cull_min[axis] = float(slot.scene_cull_world.position[axis]);
					header.cull_max[axis] = float(end[axis]);
				}
			}
			memcpy(slot.guard_proof_payload, &header, sizeof(header));
			const Error upd = rd->buffer_update(proof_rid, 0, sizeof(header), slot.guard_proof_payload);
			if (upd != OK) {
				return fail(upd);
			}
		}
	}
	slot.fast_refresh_active = true;
	RenderingDevice::ProductionOutputFastRefreshEntry scope_entry;
	scope_entry.owner = &owner;
	scope_entry.slot_index = best_slot;
	scope_entry.lease_serial = slot.lease_serial;
	scope_entry.request = owner.preparation_request(best_slot);
	scope_entry.request_serial = owner.preparation_request_serial(best_slot);
	scope_entry.pin_serial = owner.preparation_pin_serial(best_slot);
	scope_entry.owned_group = slot.owned_group;
	rd->production_output_fast_refresh.set_single(scope_entry);

	p_out_targets->native_error = int32_t(OK);
	p_out_targets->slot_index = best_slot;
	p_out_targets->lease_serial = slot.lease_serial;
	p_out_targets->current_present_seq = slot.request.common.present_seq;
	p_out_targets->profile = uint32_t(owner.config.profile);
	p_out_targets->capacity = owner.config.capacity;
	p_out_targets->surface_count = slot.surface_count;
	p_out_targets->aux_params_enabled = slot.storage->resources[ExternalOutputStorage::INSTANCE_AUX_PARAMS].bytes != 0 ? 1u : 0u;
	p_out_targets->current_records_rid = slot.storage->resources[ExternalOutputStorage::CURRENT_RECORDS].rid.get_id();
	p_out_targets->previous_records_rid = slot.storage->resources[ExternalOutputStorage::PREVIOUS_RECORDS].rid.get_id();
	p_out_targets->output_meta_rid = slot.storage->resources[ExternalOutputStorage::OUTPUT_META].rid.get_id();
	p_out_targets->aux_params_rid = slot.storage->resources[ExternalOutputStorage::INSTANCE_AUX_PARAMS].rid.get_id();
	p_out_targets->indexed_staging_rid = slot.storage->resources[ExternalOutputStorage::INDEXED_STAGING].rid.get_id();
	p_out_targets->indexed_history_rid = slot.storage->resources[ExternalOutputStorage::INDEXED_VERTEX_HISTORY_VALIDITY].rid.get_id();
	p_out_targets->guard_status_rid = slot.storage->resources[ExternalOutputStorage::GUARD_STATUS].rid.get_id();
	p_out_targets->protected_commands_rid = slot.storage->resources[ExternalOutputStorage::PROTECTED_COMMANDS].rid.get_id();
	p_out_targets->motion_required = owner.config.motion == ExternalOutputConfig::REQUIRED ? 1u : 0u;
	p_out_targets->reserved = 0;
	return int32_t(OK);
}

int32_t ParticlesStorage::particles_external_end_fast_refresh_v1(const GDExtensionFastRefreshEndInputV1 *p_input) {
	if (!p_input || p_input->abi_version != 1 || p_input->struct_size != sizeof(GDExtensionFastRefreshEndInputV1) ||
			p_input->reserved != 0 || p_input->parity_bank > 1 ||
			p_input->particles_rid == 0 || p_input->lease_serial == 0 || p_input->next_present_seq == 0) {
		return int32_t(ERR_INVALID_PARAMETER);
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!rs || !rs->is_on_render_thread() || !rd || rs->get_rendering_device() != rd) {
		return int32_t(ERR_UNAUTHORIZED);
	}
	if (rd->draw_list.active || rd->compute_list.active || rd->raytracing_list.active) {
		return int32_t(ERR_BUSY);
	}
	const RID particles_rid = RID::from_uint64(p_input->particles_rid);
	Particles *particles = particles_owner.get_or_null(particles_rid);
	if (!particles || !particles->external) {
		return int32_t(ERR_DOES_NOT_EXIST);
	}
	ExternalOutputOwner &owner = *particles->external;
	if (owner.device != rd || owner.state != ExternalOutputOwner::GenerationState::ACTIVE ||
			p_input->slot_index >= owner.slot_count) {
		return int32_t(ERR_UNAVAILABLE);
	}
	ExternalOutputOwner::Slot &slot = owner.slots[p_input->slot_index];
	if ((slot.state != ExternalOutputOwner::LeaseState::ADOPTED &&
				slot.state != ExternalOutputOwner::LeaseState::SEALED_RECORDED) ||
			slot.lease_serial != p_input->lease_serial ||
			slot.publication_closed || slot.pass_uses_closed || !slot.storage ||
			slot.original_pin.is_empty() || !owner.preparation_request(p_input->slot_index)) {
		return int32_t(ERR_UNAVAILABLE);
	}

	RenderingDevice::ProductionOutputFastRefreshEntry scope_entry;
	scope_entry.owner = &owner;
	scope_entry.slot_index = p_input->slot_index;
	scope_entry.lease_serial = slot.lease_serial;
	scope_entry.request = owner.preparation_request(p_input->slot_index);
	scope_entry.request_serial = owner.preparation_request_serial(p_input->slot_index);
	scope_entry.pin_serial = owner.preparation_pin_serial(p_input->slot_index);
	scope_entry.owned_group = slot.owned_group;
	rd->production_output_fast_refresh.set_single(scope_entry);
	struct FastRefreshScopeExit {
		RenderingDevice *device = nullptr;
		~FastRefreshScopeExit() {
			if (device) {
				device->production_output_fast_refresh.clear();
			}
		}
	} scope_exit{ rd };

	const uint32_t view_idx = p_input->slot_index / owner.config.max_inflight;
	const uint32_t bank = p_input->parity_bank;

	if (owner.config.profile == ExternalOutputConfig::PARTICLE_LIGHT_64 ||
			owner.config.profile == ExternalOutputConfig::PARTICLE_DECAL_128) {
		const RID status_rid = slot.storage->resources[ExternalOutputStorage::GUARD_STATUS].rid;
		if (status_rid.is_valid()) {
			uint32_t status_words[8] = {};
			fast_refresh_encode_light64_guard_status(p_input->next_present_seq, status_words);
			const Error upd = rd->buffer_update(status_rid, 0, sizeof(status_words), status_words);
			if (upd != OK) {
				return int32_t(upd);
			}
		}
		slot.request.common.present_seq = p_input->next_present_seq;
		if (view_idx < owner.config.max_views && owner.views[view_idx].occupied) {
			owner.views[view_idx].last_present = MAX(owner.views[view_idx].last_present, p_input->next_present_seq);
		}
		if (slot.scene_cull_bound) {
			if (auto *scene = static_cast<RendererSceneCull *>(RSG::scene)) {
				RendererSceneCull::NativeParticleSceneFacts actual;
				if (scene->instance_get_committed_particle_scene_facts(slot.scene_cull_instance, slot.scene_cull_particles, actual) == OK &&
						actual.scenario == slot.scene_cull_scenario &&
						actual.world_aabb.is_finite() && actual.world_aabb.get_end().is_finite() && actual.world_aabb.has_volume()) {
					slot.scene_cull_version = actual.version;
					slot.scene_cull_local = actual.local_aabb;
					slot.scene_cull_world = actual.world_aabb;
				}
			}
		}
		const AABB *updated_bounds = (slot.scene_cull_bound && slot.scene_cull_world.is_finite() &&
												 slot.scene_cull_world.get_end().is_finite() && slot.scene_cull_world.has_volume())
				? &slot.scene_cull_world
				: nullptr;
		if (owner.config.profile == ExternalOutputConfig::PARTICLE_LIGHT_64) {
			if (mesh_light_sort_rd) {
				mesh_light_sort_rd->update_external_light_slot_present_seq(
						p_input->particles_rid, owner.generation, p_input->slot_index, slot.lease_serial, p_input->next_present_seq, updated_bounds);
			}
		} else if (ParticlesDecalBridgeRD *decal_bridge = get_decal_bridge_rd()) {
			decal_bridge->upsert_external_decal_slot(
					BUILD_EXTERNAL_DECAL_SLOT_REGISTRATION(p_input->particles_rid, owner, p_input->slot_index));
		}
		slot.fast_refresh_active = true;
		return int32_t(OK);
	}

	const uint32_t max_groups_x = uint32_t(MIN(uint64_t(UINT32_MAX), rd->limit_get(RD::LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X)));
	if (max_groups_x == 0) {
		return int32_t(ERR_UNAVAILABLE);
	}

	if (owner.config.profile == ExternalOutputConfig::INSTANCE_192) {
		using Guard = ParticlesExternalGuard;
		if (!external_guard || !external_guard->is_prepared() || !slot.guard_resources_ready ||
				!slot.guard_proof_payload || slot.guard_proof_bytes < sizeof(Guard::GeometryProofHeader)) {
			return int32_t(ERR_UNAVAILABLE);
		}
		Guard::GeometryProofHeader proof;
		memcpy(&proof, slot.guard_proof_payload, sizeof(proof));
		bool proof_dirty = false;
		if (p_input->source_count_word_offset != UINT32_MAX && proof.source_accessor[0] != p_input->source_count_word_offset) {
			proof.source_accessor[0] = p_input->source_count_word_offset;
			proof_dirty = true;
		}
		for (uint32_t g = 0; g < 4; ++g) {
			if (p_input->source_gate_word_offsets[g] != UINT32_MAX && proof.source_gate_words[g] != p_input->source_gate_word_offsets[g]) {
				proof.source_gate_words[g] = p_input->source_gate_word_offsets[g];
				proof_dirty = true;
			}
		}

		Guard::PushConstant push;
		push.capacity = owner.config.capacity;
		push.sequence_low = uint32_t(p_input->next_present_seq);
		push.sequence_high = uint32_t(p_input->next_present_seq >> 32);
		push.surface_count = slot.surface_count;
		push.record_base = 0;
		push.record_count = push.capacity;

		const bool no_previous_records = push.capacity != 0 && proof.options[0] == 0 &&
				slot.storage->resources[ExternalOutputStorage::PREVIOUS_RECORDS].rid.is_null() &&
				slot.storage->resources[ExternalOutputStorage::PREVIOUS_RECORDS].bytes == 0;
		const bool aux_enabled = push.capacity != 0 && proof.options[1] == 1;
		Guard::Mode init_mode = Guard::INIT;
		Guard::Mode validate_mode = Guard::VALIDATE;
		Guard::Mode finalize_mode = Guard::FINALIZE;
		if (push.capacity == 0) {
			init_mode = Guard::INIT_EMPTY;
			finalize_mode = Guard::FINALIZE_EMPTY;
		} else if (no_previous_records) {
			init_mode = aux_enabled ? Guard::INIT_NO_MOTION_AUX : Guard::INIT_NO_MOTION;
			validate_mode = aux_enabled ? Guard::VALIDATE_NO_MOTION_AUX : Guard::VALIDATE_NO_MOTION;
			finalize_mode = aux_enabled ? Guard::FINALIZE_NO_MOTION_AUX : Guard::FINALIZE_NO_MOTION;
		} else if (aux_enabled) {
			init_mode = Guard::INIT_AUX;
			validate_mode = Guard::VALIDATE_AUX;
			finalize_mode = Guard::FINALIZE_AUX;
		}

		const bool need_validate = fast_refresh_item_needs_validate(false, push.capacity, proof.options[2]);
		const uint32_t mode_count = push.capacity ? 3u : 2u;
		const Guard::Mode modes[3] = { init_mode, need_validate ? validate_mode : finalize_mode, finalize_mode };

		bool cache_hit = (slot.fast_refresh_source_count_rid[bank] == p_input->source_count_rid) && p_input->source_count_rid != 0;
		for (uint32_t g = 0; g < 4 && cache_hit; ++g) {
			cache_hit = (slot.fast_refresh_source_gate_rids[bank][g] == p_input->source_gate_rids[g]) && p_input->source_gate_rids[g] != 0;
		}
		for (uint32_t i = 0; i < mode_count && cache_hit; ++i) {
			const RID set = slot.fast_refresh_guard_sets[bank][modes[i]];
			cache_hit = set.is_valid() && rd->uniform_set_is_valid(set);
		}

		if (!cache_hit || proof_dirty) {
			Guard::Bindings bindings;
			for (uint32_t binding = 0; binding < Guard::BINDING_COUNT; ++binding) {
				if (binding == Guard::SOURCE_COUNT) {
					const RID rid = RID::from_uint64(p_input->source_count_rid);
					const RenderingDevice::Buffer *buf = rd->_production_output_storage_buffer(rid);
					if (!buf) {
						return int32_t(ERR_INVALID_PARAMETER);
					}
					bindings.buffers[binding] = { rid, buf->size };
				} else if (binding >= Guard::SOURCE_COUNT_VALIDITY && binding <= Guard::SOURCE_INSTANCE_GATE) {
					const uint32_t g = binding - Guard::SOURCE_COUNT_VALIDITY;
					const RID rid = RID::from_uint64(p_input->source_gate_rids[g]);
					const RenderingDevice::Buffer *buf = rd->_production_output_storage_buffer(rid);
					if (!buf) {
						return int32_t(ERR_INVALID_PARAMETER);
					}
					bindings.buffers[binding] = { rid, buf->size };
				} else {
					const uint32_t role = binding == Guard::INSTANCE_AUX_PARAMS
							? uint32_t(ExternalOutputStorage::INSTANCE_AUX_PARAMS)
							: (binding <= Guard::OUTPUT_META ? binding : binding - 1);
					const ExternalOutputStorage::Resource &res = slot.storage->resources[role];
					bindings.buffers[binding] = { res.rid, res.bytes };
				}
			}
			Guard::LayoutPlan plan;
			const Error planned = Guard::plan_layout(push, bindings, proof, max_groups_x, plan);
			if (planned != OK) {
				return int32_t(planned);
			}
			init_mode = plan.initialize_mode;
			validate_mode = plan.validate_mode;
			finalize_mode = plan.finalize_mode;
			if (proof_dirty) {
				const RID proof_rid = slot.storage->resources[ExternalOutputStorage::GEOMETRY_PROOF].rid;
				if (proof_rid.is_null()) {
					return int32_t(ERR_UNAVAILABLE);
				}
				memcpy(slot.guard_proof_payload, &proof, sizeof(proof));
				const Error upd = rd->buffer_update(proof_rid, 0, sizeof(proof), slot.guard_proof_payload);
				if (upd != OK) {
					return int32_t(upd);
				}
			}
			if (!cache_hit) {
				for (uint32_t m = 0; m < ExternalOutputOwner::GUARD_MODE_CAPACITY; ++m) {
					if (slot.fast_refresh_guard_sets[bank][m].is_valid()) {
						if (rd->uniform_set_is_valid(slot.fast_refresh_guard_sets[bank][m])) {
							rd->free_rid(slot.fast_refresh_guard_sets[bank][m]);
						}
						slot.fast_refresh_guard_sets[bank][m] = RID();
					}
				}
				slot.fast_refresh_source_count_rid[bank] = 0;
				for (uint32_t g = 0; g < 4; ++g) {
					slot.fast_refresh_source_gate_rids[bank][g] = 0;
				}
				const Guard::Mode active_modes[3] = { init_mode, push.capacity ? validate_mode : finalize_mode, finalize_mode };
				for (uint32_t i = 0; i < mode_count; ++i) {
					const Guard::Mode mode = active_modes[i];
					const RID shader = external_guard->get_shader(mode);
					if (shader.is_null()) {
						return int32_t(ERR_UNAVAILABLE);
					}
					const RID created = rd->uniform_set_create(plan.uniforms, shader, 0);
					if (created.is_null()) {
						const Error create_err = rd->get_last_uniform_set_create_error() == ERR_BUSY ? ERR_BUSY : ERR_CANT_CREATE;
						for (uint32_t j = 0; j < i; ++j) {
							const Guard::Mode prev_mode = active_modes[j];
							if (slot.fast_refresh_guard_sets[bank][prev_mode].is_valid()) {
								rd->free_rid(slot.fast_refresh_guard_sets[bank][prev_mode]);
								slot.fast_refresh_guard_sets[bank][prev_mode] = RID();
							}
						}
						return int32_t(create_err);
					}
					slot.fast_refresh_guard_sets[bank][mode] = created;
				}
				slot.fast_refresh_source_count_rid[bank] = p_input->source_count_rid;
				for (uint32_t g = 0; g < 4; ++g) {
					slot.fast_refresh_source_gate_rids[bank][g] = p_input->source_gate_rids[g];
				}
			}
		}

		const RID init_pipe = external_guard->get_pipeline(init_mode);
		const RID finalize_pipe = external_guard->get_pipeline(finalize_mode);
		const RID validate_pipe = need_validate ? external_guard->get_pipeline(validate_mode) : RID();
		const RID init_set = slot.fast_refresh_guard_sets[bank][init_mode];
		const RID finalize_set = slot.fast_refresh_guard_sets[bank][finalize_mode];
		const RID validate_set = need_validate ? slot.fast_refresh_guard_sets[bank][validate_mode] : RID();
		if (init_pipe.is_null() || finalize_pipe.is_null() || init_set.is_null() || finalize_set.is_null() ||
				(need_validate && (validate_pipe.is_null() || validate_set.is_null()))) {
			return int32_t(ERR_UNAVAILABLE);
		}

		const RD::ComputeListID cl = rd->compute_list_begin();
		if (cl == RD::INVALID_ID) {
			return int32_t(ERR_CANT_CREATE);
		}
		Guard::PushConstant stage_pc = push;
		stage_pc.record_base = 0;
		stage_pc.record_count = 0;
		rd->compute_list_bind_compute_pipeline(cl, init_pipe);
		rd->compute_list_bind_uniform_set(cl, init_set, 0);
		rd->compute_list_set_push_constant(cl, &stage_pc, sizeof(stage_pc));
		rd->compute_list_dispatch(cl, 1, 1, 1);
		rd->compute_list_add_barrier(cl);

		if (need_validate) {
			const uint64_t chunk_capacity = MIN(uint64_t(max_groups_x) * 64ull, uint64_t(push.capacity));
			for (uint32_t base = 0; base < push.capacity;) {
				Guard::PushConstant chunk = push;
				chunk.record_base = base;
				chunk.record_count = uint32_t(MIN(chunk_capacity, uint64_t(push.capacity - base)));
				const uint32_t groups_x = (chunk.record_count + 63u) / 64u;
				rd->compute_list_bind_compute_pipeline(cl, validate_pipe);
				rd->compute_list_bind_uniform_set(cl, validate_set, 0);
				rd->compute_list_set_push_constant(cl, &chunk, sizeof(chunk));
				rd->compute_list_dispatch(cl, groups_x, 1, 1);
				base += chunk.record_count;
			}
			rd->compute_list_add_barrier(cl);
		}

		rd->compute_list_bind_compute_pipeline(cl, finalize_pipe);
		rd->compute_list_bind_uniform_set(cl, finalize_set, 0);
		rd->compute_list_set_push_constant(cl, &stage_pc, sizeof(stage_pc));
		rd->compute_list_dispatch(cl, 1, 1, 1);
		rd->compute_list_end();

		slot.request.common.present_seq = p_input->next_present_seq;
		if (view_idx < owner.config.max_views && owner.views[view_idx].occupied) {
			owner.views[view_idx].last_present = MAX(owner.views[view_idx].last_present, p_input->next_present_seq);
		}
		slot.fast_refresh_active = true;
		return int32_t(OK);
	}

	if (owner.config.profile == ExternalOutputConfig::INDEXED_144) {
		using Guard = ParticlesExternalGuardIndexed144;
		if (!external_guard_indexed144 || !external_guard_indexed144->is_prepared() || !slot.guard_resources_ready ||
				!slot.guard_proof_payload || slot.guard_proof_bytes < sizeof(Guard::GeometryProofHeader)) {
			return int32_t(ERR_UNAVAILABLE);
		}
		Guard::GeometryProofHeader proof;
		memcpy(&proof, slot.guard_proof_payload, sizeof(proof));
		bool proof_dirty = false;
		if (p_input->source_count_word_offset != UINT32_MAX && proof.source_accessor[0] != p_input->source_count_word_offset) {
			proof.source_accessor[0] = p_input->source_count_word_offset;
			proof_dirty = true;
		}
		for (uint32_t g = 0; g < 4; ++g) {
			if (p_input->source_gate_word_offsets[g] != UINT32_MAX && proof.source_gate_words[g] != p_input->source_gate_word_offsets[g]) {
				proof.source_gate_words[g] = p_input->source_gate_word_offsets[g];
				proof_dirty = true;
			}
		}

		Guard::PushConstant push;
		push.capacity = owner.config.capacity;
		push.sequence_low = uint32_t(p_input->next_present_seq);
		push.sequence_high = uint32_t(p_input->next_present_seq >> 32);
		push.surface_count = slot.surface_count;
		push.segment_base = 0;
		push.segment_count = push.capacity;

		const bool motion_required = (proof.options[0] == 1u);
		const bool motion_buffers_omitted = !motion_required && push.capacity > 0 &&
				slot.storage->resources[ExternalOutputStorage::PREVIOUS_RECORDS].rid.is_null() &&
				slot.storage->resources[ExternalOutputStorage::PREVIOUS_RECORDS].bytes == 0 &&
				slot.storage->resources[ExternalOutputStorage::INDEXED_VERTEX_HISTORY_VALIDITY].rid.is_null() &&
				slot.storage->resources[ExternalOutputStorage::INDEXED_VERTEX_HISTORY_VALIDITY].bytes == 0;
		Guard::Mode init_mode = Guard::INIT;
		Guard::Mode validate_mode = Guard::VALIDATE;
		Guard::Mode finalize_mode = Guard::FINALIZE;
		if (push.capacity == 0) {
			init_mode = Guard::INIT_EMPTY;
			finalize_mode = Guard::FINALIZE_EMPTY;
		} else if (motion_buffers_omitted) {
			init_mode = Guard::INIT_NO_MOTION;
			validate_mode = Guard::VALIDATE_NO_MOTION;
			finalize_mode = Guard::FINALIZE_NO_MOTION;
		}

		const bool need_validate = fast_refresh_item_needs_validate(false, push.capacity, proof.options[2]);
		const uint32_t mode_count = push.capacity ? 3u : 2u;
		const Guard::Mode modes[3] = { init_mode, need_validate ? validate_mode : finalize_mode, finalize_mode };

		bool cache_hit = (slot.fast_refresh_source_count_rid[bank] == p_input->source_count_rid) && p_input->source_count_rid != 0;
		for (uint32_t g = 0; g < 4 && cache_hit; ++g) {
			cache_hit = (slot.fast_refresh_source_gate_rids[bank][g] == p_input->source_gate_rids[g]) && p_input->source_gate_rids[g] != 0;
		}
		for (uint32_t i = 0; i < mode_count && cache_hit; ++i) {
			const RID set = slot.fast_refresh_guard_sets[bank][modes[i]];
			cache_hit = set.is_valid() && rd->uniform_set_is_valid(set);
		}

		if (!cache_hit || proof_dirty) {
			auto role_for_binding = [](uint32_t p_binding) -> uint32_t {
				switch (p_binding) {
					case Guard::CURRENT_VERTICES:
						return ExternalOutputStorage::CURRENT_RECORDS;
					case Guard::ALIGNED_PREVIOUS_VERTICES:
						return ExternalOutputStorage::PREVIOUS_RECORDS;
					case Guard::VERTEX_HISTORY_VALIDITY:
						return ExternalOutputStorage::INDEXED_VERTEX_HISTORY_VALIDITY;
					case Guard::INDEX_STAGING:
						return ExternalOutputStorage::INDEXED_STAGING;
					case Guard::OUTPUT_META:
						return ExternalOutputStorage::OUTPUT_META;
					case Guard::GUARD_SCRATCH:
						return ExternalOutputStorage::GUARD_SCRATCH;
					case Guard::GUARD_STATUS:
						return ExternalOutputStorage::GUARD_STATUS;
					case Guard::PROTECTED_COMMANDS:
						return ExternalOutputStorage::PROTECTED_COMMANDS;
					case Guard::GEOMETRY_PROOF:
						return ExternalOutputStorage::GEOMETRY_PROOF;
					default:
						return UINT32_MAX;
				}
			};
			Guard::Bindings bindings;
			for (uint32_t binding = 0; binding < Guard::BINDING_COUNT; ++binding) {
				if (binding == Guard::SOURCE_COUNT) {
					const RID rid = RID::from_uint64(p_input->source_count_rid);
					const RenderingDevice::Buffer *buf = rd->_production_output_storage_buffer(rid);
					if (!buf) {
						return int32_t(ERR_INVALID_PARAMETER);
					}
					bindings.buffers[binding] = { rid, buf->size };
				} else if (binding >= Guard::SOURCE_COUNT_VALIDITY && binding <= Guard::SOURCE_INSTANCE_GATE) {
					const uint32_t g = binding - Guard::SOURCE_COUNT_VALIDITY;
					const RID rid = RID::from_uint64(p_input->source_gate_rids[g]);
					const RenderingDevice::Buffer *buf = rd->_production_output_storage_buffer(rid);
					if (!buf) {
						return int32_t(ERR_INVALID_PARAMETER);
					}
					bindings.buffers[binding] = { rid, buf->size };
				} else {
					const uint32_t role = role_for_binding(binding);
					if (role == UINT32_MAX) {
						return int32_t(ERR_INVALID_DATA);
					}
					const ExternalOutputStorage::Resource &res = slot.storage->resources[role];
					bindings.buffers[binding] = { res.rid, res.bytes };
				}
			}
			Guard::LayoutPlan plan;
			const Error planned = Guard::plan_layout(push, bindings, proof, max_groups_x, plan);
			if (planned != OK) {
				return int32_t(planned);
			}
			init_mode = plan.initialize_mode;
			validate_mode = plan.validate_mode;
			finalize_mode = plan.finalize_mode;
			if (proof_dirty) {
				const RID proof_rid = slot.storage->resources[ExternalOutputStorage::GEOMETRY_PROOF].rid;
				if (proof_rid.is_null()) {
					return int32_t(ERR_UNAVAILABLE);
				}
				memcpy(slot.guard_proof_payload, &proof, sizeof(proof));
				const Error upd = rd->buffer_update(proof_rid, 0, sizeof(proof), slot.guard_proof_payload);
				if (upd != OK) {
					return int32_t(upd);
				}
			}
			if (!cache_hit) {
				for (uint32_t m = 0; m < ExternalOutputOwner::GUARD_MODE_CAPACITY; ++m) {
					if (slot.fast_refresh_guard_sets[bank][m].is_valid()) {
						if (rd->uniform_set_is_valid(slot.fast_refresh_guard_sets[bank][m])) {
							rd->free_rid(slot.fast_refresh_guard_sets[bank][m]);
						}
						slot.fast_refresh_guard_sets[bank][m] = RID();
					}
				}
				slot.fast_refresh_source_count_rid[bank] = 0;
				for (uint32_t g = 0; g < 4; ++g) {
					slot.fast_refresh_source_gate_rids[bank][g] = 0;
				}
				const Guard::Mode active_modes[3] = { init_mode, push.capacity ? validate_mode : finalize_mode, finalize_mode };
				for (uint32_t i = 0; i < mode_count; ++i) {
					const Guard::Mode mode = active_modes[i];
					const RID shader = external_guard_indexed144->get_shader(mode);
					if (shader.is_null()) {
						return int32_t(ERR_UNAVAILABLE);
					}
					const RID created = rd->uniform_set_create(plan.uniforms, shader, 0);
					if (created.is_null()) {
						const Error create_err = rd->get_last_uniform_set_create_error() == ERR_BUSY ? ERR_BUSY : ERR_CANT_CREATE;
						for (uint32_t j = 0; j < i; ++j) {
							const Guard::Mode prev_mode = active_modes[j];
							if (slot.fast_refresh_guard_sets[bank][prev_mode].is_valid()) {
								rd->free_rid(slot.fast_refresh_guard_sets[bank][prev_mode]);
								slot.fast_refresh_guard_sets[bank][prev_mode] = RID();
							}
						}
						return int32_t(create_err);
					}
					slot.fast_refresh_guard_sets[bank][mode] = created;
				}
				slot.fast_refresh_source_count_rid[bank] = p_input->source_count_rid;
				for (uint32_t g = 0; g < 4; ++g) {
					slot.fast_refresh_source_gate_rids[bank][g] = p_input->source_gate_rids[g];
				}
			}
		}

		const RID init_pipe = external_guard_indexed144->get_pipeline(init_mode);
		const RID finalize_pipe = external_guard_indexed144->get_pipeline(finalize_mode);
		const RID validate_pipe = need_validate ? external_guard_indexed144->get_pipeline(validate_mode) : RID();
		const RID init_set = slot.fast_refresh_guard_sets[bank][init_mode];
		const RID finalize_set = slot.fast_refresh_guard_sets[bank][finalize_mode];
		const RID validate_set = need_validate ? slot.fast_refresh_guard_sets[bank][validate_mode] : RID();
		if (init_pipe.is_null() || finalize_pipe.is_null() || init_set.is_null() || finalize_set.is_null() ||
				(need_validate && (validate_pipe.is_null() || validate_set.is_null()))) {
			return int32_t(ERR_UNAVAILABLE);
		}

		const RD::ComputeListID cl = rd->compute_list_begin();
		if (cl == RD::INVALID_ID) {
			return int32_t(ERR_CANT_CREATE);
		}
		Guard::PushConstant stage_pc = push;
		stage_pc.segment_base = 0;
		stage_pc.segment_count = 0;
		rd->compute_list_bind_compute_pipeline(cl, init_pipe);
		rd->compute_list_bind_uniform_set(cl, init_set, 0);
		rd->compute_list_set_push_constant(cl, &stage_pc, sizeof(stage_pc));
		rd->compute_list_dispatch(cl, 1, 1, 1);
		rd->compute_list_add_barrier(cl);

		if (need_validate) {
			const uint64_t chunk_capacity = MIN(uint64_t(max_groups_x) * 64ull, uint64_t(push.capacity));
			for (uint32_t base = 0; base < push.capacity;) {
				Guard::PushConstant chunk = push;
				chunk.segment_base = base;
				chunk.segment_count = uint32_t(MIN(chunk_capacity, uint64_t(push.capacity - base)));
				const uint32_t groups_x = (chunk.segment_count + 63u) / 64u;
				rd->compute_list_bind_compute_pipeline(cl, validate_pipe);
				rd->compute_list_bind_uniform_set(cl, validate_set, 0);
				rd->compute_list_set_push_constant(cl, &chunk, sizeof(chunk));
				rd->compute_list_dispatch(cl, groups_x, 1, 1);
				base += chunk.segment_count;
			}
			rd->compute_list_add_barrier(cl);
		}

		rd->compute_list_bind_compute_pipeline(cl, finalize_pipe);
		rd->compute_list_bind_uniform_set(cl, finalize_set, 0);
		rd->compute_list_set_push_constant(cl, &stage_pc, sizeof(stage_pc));
		rd->compute_list_dispatch(cl, 1, 1, 1);
		rd->compute_list_end();

		slot.request.common.present_seq = p_input->next_present_seq;
		if (view_idx < owner.config.max_views && owner.views[view_idx].occupied) {
			owner.views[view_idx].last_present = MAX(owner.views[view_idx].last_present, p_input->next_present_seq);
		}
		slot.fast_refresh_active = true;
		return int32_t(OK);
	}

	return int32_t(ERR_UNAVAILABLE);
}

int32_t ParticlesStorage::particles_external_release_fast_refresh_v1(uint64_t p_particles_rid, uint32_t p_slot_index) {
	if (p_particles_rid == 0) {
		return int32_t(ERR_INVALID_PARAMETER);
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!rs || !rs->is_on_render_thread() || !rd || rs->get_rendering_device() != rd) {
		return int32_t(ERR_UNAUTHORIZED);
	}
	Particles *particles = particles_owner.get_or_null(RID::from_uint64(p_particles_rid));
	if (!particles || !particles->external) {
		return int32_t(ERR_DOES_NOT_EXIST);
	}
	ExternalOutputOwner &owner = *particles->external;
	if (owner.device != rd) {
		return int32_t(ERR_UNAVAILABLE);
	}
	rd->production_output_fast_refresh.remove_owner(&owner, p_slot_index);
	if (p_slot_index == UINT32_MAX) {
		for (uint32_t i = 0; i < owner.slot_count; ++i) {
			owner.slots[i].release_fast_refresh_resources(rd);
		}
		return int32_t(OK);
	}
	if (p_slot_index >= owner.slot_count) {
		return int32_t(ERR_INVALID_PARAMETER);
	}
	owner.slots[p_slot_index].release_fast_refresh_resources(rd);
	return int32_t(OK);
}

int32_t ParticlesStorage::particles_external_begin_fast_refresh_batch_v1(
		uint32_t p_count,
		const GDExtensionFastRefreshBeginInputV1 *p_inputs,
		GDExtensionFastRefreshTargetsV1 *p_out_targets) {
	if (p_out_targets && p_count > 0) {
		for (uint32_t i = 0; i < p_count; ++i) {
			p_out_targets[i] = {};
			p_out_targets[i].abi_version = 1;
			p_out_targets[i].struct_size = sizeof(GDExtensionFastRefreshTargetsV1);
			p_out_targets[i].native_error = int32_t(ERR_INVALID_PARAMETER);
			p_out_targets[i].slot_index = UINT32_MAX;
		}
	}
	auto fail_all = [&](Error p_err) -> int32_t {
		if (p_out_targets && p_count > 0) {
			for (uint32_t i = 0; i < p_count; ++i) {
				p_out_targets[i].native_error = int32_t(p_err);
			}
		}
		return int32_t(p_err);
	};
	if (p_count == 0 || !p_inputs || !p_out_targets) {
		return fail_all(ERR_INVALID_PARAMETER);
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!rs || !rs->is_on_render_thread() || !rd || rs->get_rendering_device() != rd) {
		return fail_all(ERR_UNAUTHORIZED);
	}
	if (rd->draw_list.active || rd->compute_list.active || rd->raytracing_list.active) {
		return fail_all(ERR_BUSY);
	}

	rd->production_output_fast_refresh.clear();
	uint32_t succeeded_count = 0;
	Error first_error = OK;
	auto record_item_error = [&](uint32_t p_idx, Error p_err) {
		p_out_targets[p_idx].native_error = int32_t(p_err);
		if (first_error == OK) {
			first_error = p_err;
		}
	};

	for (uint32_t idx = 0; idx < p_count; ++idx) {
		const GDExtensionFastRefreshBeginInputV1 &in = p_inputs[idx];
		GDExtensionFastRefreshTargetsV1 &out = p_out_targets[idx];
		if (in.abi_version != 1 || in.struct_size != sizeof(GDExtensionFastRefreshBeginInputV1) ||
				in.reserved != 0 || in.reserved2 != 0 || in.has_host_affine > 1 ||
				in.particles_rid == 0 || in.next_present_seq == 0) {
			record_item_error(idx, ERR_INVALID_PARAMETER);
			continue;
		}
		const RID particles_rid = RID::from_uint64(in.particles_rid);
		Particles *particles = particles_owner.get_or_null(particles_rid);
		if (!particles || !particles->external) {
			record_item_error(idx, ERR_DOES_NOT_EXIST);
			continue;
		}
		ExternalOutputOwner &owner = *particles->external;
		if (owner.device != rd || owner.state != ExternalOutputOwner::GenerationState::ACTIVE) {
			record_item_error(idx, ERR_UNAVAILABLE);
			continue;
		}
		if (owner.config.profile != ExternalOutputConfig::INSTANCE_192 &&
				owner.config.profile != ExternalOutputConfig::INDEXED_144 &&
				owner.config.profile != ExternalOutputConfig::PARTICLE_LIGHT_64 &&
				owner.config.profile != ExternalOutputConfig::PARTICLE_DECAL_128) {
			record_item_error(idx, ERR_UNAVAILABLE);
			continue;
		}
		uint32_t best_slot = UINT32_MAX;
		bool best_is_adopted = false;
		uint64_t best_present_seq = 0;
		uint64_t best_lease_serial = 0;
		for (uint32_t i = 0; i < owner.slot_count; ++i) {
			const ExternalOutputOwner::Slot &cand = owner.slots[i];
			const bool is_adopted = cand.state == ExternalOutputOwner::LeaseState::ADOPTED;
			const bool is_sealed = cand.state == ExternalOutputOwner::LeaseState::SEALED_RECORDED;
			if ((!is_adopted && !is_sealed) || cand.storage == nullptr ||
					cand.publication_closed || cand.pass_uses_closed ||
					cand.original_pin.is_empty() || owner.preparation_request(i) == nullptr) {
				continue;
			}
			if (best_slot == UINT32_MAX || (is_adopted && !best_is_adopted) ||
					(is_adopted == best_is_adopted && (cand.request.common.present_seq > best_present_seq ||
					(cand.request.common.present_seq == best_present_seq && cand.lease_serial > best_lease_serial)))) {
				best_slot = i;
				best_is_adopted = is_adopted;
				best_present_seq = cand.request.common.present_seq;
				best_lease_serial = cand.lease_serial;
			}
		}
		if (best_slot == UINT32_MAX) {
			record_item_error(idx, ERR_UNAVAILABLE);
			continue;
		}
		ExternalOutputOwner::Slot &slot = owner.slots[best_slot];

		float cur_inv[12] = {};
		float prev_inv[12] = {};
		if (in.has_host_affine) {
			if (!fast_refresh_invert_affine_f32(in.current_host_affine, cur_inv) ||
					!fast_refresh_invert_affine_f32(in.previous_host_affine, prev_inv)) {
				record_item_error(idx, ERR_INVALID_DATA);
				continue;
			}
		}

		bool already_in_scope = false;
		for (uint32_t s = 0; s < rd->production_output_fast_refresh.count; ++s) {
			const auto &existing = rd->production_output_fast_refresh.entry_at(s);
			if (existing.owner == &owner && existing.slot_index == best_slot) {
				already_in_scope = true;
				break;
			}
		}
		RenderingDevice::ProductionOutputFastRefreshEntry scope_entry;
		scope_entry.owner = &owner;
		scope_entry.slot_index = best_slot;
		scope_entry.lease_serial = slot.lease_serial;
		scope_entry.request = owner.preparation_request(best_slot);
		scope_entry.request_serial = owner.preparation_request_serial(best_slot);
		scope_entry.pin_serial = owner.preparation_pin_serial(best_slot);
		scope_entry.owned_group = slot.owned_group;
		rd->production_output_fast_refresh.push(scope_entry);

		if (in.has_host_affine) {
			slot.current_host = fast_refresh_decode_transform(in.current_host_affine);
			slot.previous_host = fast_refresh_decode_transform(in.previous_host_affine);
			// Refresh committed scene cull bounds for ALL profiles (including PARTICLE_LIGHT_64,
			// which has no GEOMETRY_PROOF buffer) so moving light emitters keep fresh world_bounds.
			bool cull_updated = false;
			if (slot.scene_cull_bound) {
				if (auto *scene = static_cast<RendererSceneCull *>(RSG::scene)) {
					RendererSceneCull::NativeParticleSceneFacts actual;
					if (scene->instance_get_committed_particle_scene_facts(slot.scene_cull_instance, slot.scene_cull_particles, actual) == OK &&
							actual.scenario == slot.scene_cull_scenario &&
							actual.world_aabb.is_finite() && actual.world_aabb.get_end().is_finite() && actual.world_aabb.has_volume()) {
						slot.scene_cull_version = actual.version;
						slot.scene_cull_local = actual.local_aabb;
						slot.scene_cull_world = actual.world_aabb;
						cull_updated = true;
					}
				}
			}
			const RID proof_rid = slot.storage->resources[ExternalOutputStorage::GEOMETRY_PROOF].rid;
			if (slot.guard_proof_payload != nullptr &&
					slot.guard_proof_bytes >= sizeof(ParticlesExternalGuard::GeometryProofHeader) &&
					proof_rid.is_valid()) {
				ParticlesExternalGuard::GeometryProofHeader header;
				memcpy(&header, slot.guard_proof_payload, sizeof(header));
				memcpy(header.current_host_inverse_rows, cur_inv, sizeof(cur_inv));
				memcpy(header.previous_host_inverse_rows, prev_inv, sizeof(prev_inv));
				if (cull_updated) {
					const Vector3 end = slot.scene_cull_world.get_end();
					for (uint32_t axis = 0; axis < 3; ++axis) {
						header.cull_min[axis] = float(slot.scene_cull_world.position[axis]);
						header.cull_max[axis] = float(end[axis]);
					}
				}
				memcpy(slot.guard_proof_payload, &header, sizeof(header));
				const Error upd = rd->buffer_update(proof_rid, 0, sizeof(header), slot.guard_proof_payload);
				if (upd != OK) {
					if (!already_in_scope) {
						rd->production_output_fast_refresh.remove_owner(&owner, best_slot);
					}
					record_item_error(idx, upd);
					continue;
				}
			}
		}

		slot.fast_refresh_active = true;
		out.native_error = int32_t(OK);
		out.slot_index = best_slot;
		out.lease_serial = slot.lease_serial;
		out.current_present_seq = slot.request.common.present_seq;
		out.profile = uint32_t(owner.config.profile);
		out.capacity = owner.config.capacity;
		out.surface_count = slot.surface_count;
		out.aux_params_enabled = slot.storage->resources[ExternalOutputStorage::INSTANCE_AUX_PARAMS].bytes != 0 ? 1u : 0u;
		out.current_records_rid = slot.storage->resources[ExternalOutputStorage::CURRENT_RECORDS].rid.get_id();
		out.previous_records_rid = slot.storage->resources[ExternalOutputStorage::PREVIOUS_RECORDS].rid.get_id();
		out.output_meta_rid = slot.storage->resources[ExternalOutputStorage::OUTPUT_META].rid.get_id();
		out.aux_params_rid = slot.storage->resources[ExternalOutputStorage::INSTANCE_AUX_PARAMS].rid.get_id();
		out.indexed_staging_rid = slot.storage->resources[ExternalOutputStorage::INDEXED_STAGING].rid.get_id();
		out.indexed_history_rid = slot.storage->resources[ExternalOutputStorage::INDEXED_VERTEX_HISTORY_VALIDITY].rid.get_id();
		out.guard_status_rid = slot.storage->resources[ExternalOutputStorage::GUARD_STATUS].rid.get_id();
		out.protected_commands_rid = slot.storage->resources[ExternalOutputStorage::PROTECTED_COMMANDS].rid.get_id();
		out.motion_required = owner.config.motion == ExternalOutputConfig::REQUIRED ? 1u : 0u;
		out.reserved = 0;
		++succeeded_count;
	}

	if (succeeded_count == 0) {
		rd->production_output_fast_refresh.clear();
		return int32_t(first_error != OK ? first_error : ERR_UNAVAILABLE);
	}
	return int32_t(OK);
}

int32_t ParticlesStorage::particles_external_end_fast_refresh_batch_v1(
		uint32_t p_count,
		const GDExtensionFastRefreshEndInputV1 *p_inputs,
		uint32_t p_flags,
		int32_t *p_out_errors) {
	auto fail_all = [&](Error p_err) -> int32_t {
		if (p_out_errors && p_count > 0) {
			for (uint32_t i = 0; i < p_count; ++i) {
				p_out_errors[i] = int32_t(p_err);
			}
		}
		return int32_t(p_err);
	};
	if (p_count == 0 || !p_inputs || (p_flags & ~GDEXTENSION_FAST_REFRESH_END_FLAG_TRUSTED_TIER2) != 0) {
		return fail_all(ERR_INVALID_PARAMETER);
	}
	if (p_out_errors) {
		for (uint32_t i = 0; i < p_count; ++i) {
			p_out_errors[i] = int32_t(ERR_INVALID_PARAMETER);
		}
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!rs || !rs->is_on_render_thread() || !rd || rs->get_rendering_device() != rd) {
		return fail_all(ERR_UNAUTHORIZED);
	}
	if (rd->draw_list.active || rd->compute_list.active || rd->raytracing_list.active) {
		return fail_all(ERR_BUSY);
	}

	struct FastRefreshScopeExit {
		RenderingDevice *device = nullptr;
		~FastRefreshScopeExit() {
			if (device) {
				device->production_output_fast_refresh.clear();
			}
		}
	} scope_exit{ rd };
	rd->production_output_fast_refresh.clear();

	const uint32_t max_groups_x = uint32_t(MIN(uint64_t(UINT32_MAX), rd->limit_get(RD::LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X)));
	const bool skip_validate = (p_flags & GDEXTENSION_FAST_REFRESH_END_FLAG_TRUSTED_TIER2) != 0;

	struct BatchedGuardItem {
		uint32_t input_index = 0;
		ExternalOutputOwner *owner = nullptr;
		ExternalOutputOwner::Slot *slot = nullptr;
		uint32_t slot_index = UINT32_MAX;
		uint32_t view_idx = UINT32_MAX;
		uint32_t capacity = 0;
		uint32_t surface_count = 0;
		uint64_t next_present_seq = 0;
		RID init_pipe;
		RID validate_pipe;
		RID finalize_pipe;
		RID init_set;
		RID validate_set;
		RID finalize_set;
		bool need_validate = false;
	};
	static constexpr uint32_t INLINE_BATCH_CAPACITY = 128;
	BatchedGuardItem inline_gpu_items[INLINE_BATCH_CAPACITY];
	LocalVector<BatchedGuardItem> overflow_gpu_items;
	uint32_t gpu_item_count = 0;
	bool any_need_validate = false;
	auto get_gpu_item = [&](uint32_t p_idx) -> BatchedGuardItem & {
		return p_idx < INLINE_BATCH_CAPACITY ? inline_gpu_items[p_idx] : overflow_gpu_items[p_idx - INLINE_BATCH_CAPACITY];
	};
	auto push_gpu_item = [&](const BatchedGuardItem &p_item) {
		if (gpu_item_count < INLINE_BATCH_CAPACITY) {
			inline_gpu_items[gpu_item_count++] = p_item;
		} else {
			overflow_gpu_items.push_back(p_item);
			++gpu_item_count;
		}
	};

	ParticlesMeshLightSortRD::ExternalLightSlotPresentSeqUpdate inline_light_updates[INLINE_BATCH_CAPACITY];
	LocalVector<ParticlesMeshLightSortRD::ExternalLightSlotPresentSeqUpdate> overflow_light_updates;
	uint32_t light_update_count = 0;
	auto push_light_update = [&](const ParticlesMeshLightSortRD::ExternalLightSlotPresentSeqUpdate &p_upd) {
		if (light_update_count < INLINE_BATCH_CAPACITY) {
			inline_light_updates[light_update_count++] = p_upd;
		} else {
			overflow_light_updates.push_back(p_upd);
			++light_update_count;
		}
	};

	ParticlesDecalBridgeRD::ExternalDecalSlotRegistration inline_decal_updates[INLINE_BATCH_CAPACITY];
	LocalVector<ParticlesDecalBridgeRD::ExternalDecalSlotRegistration> overflow_decal_updates;
	uint32_t decal_update_count = 0;
	auto push_decal_update = [&](const ParticlesDecalBridgeRD::ExternalDecalSlotRegistration &p_upd) {
		if (decal_update_count < INLINE_BATCH_CAPACITY) {
			inline_decal_updates[decal_update_count++] = p_upd;
		} else {
			overflow_decal_updates.push_back(p_upd);
			++decal_update_count;
		}
	};

	uint32_t succeeded_count = 0;
	Error first_error = OK;
	auto set_item_result = [&](uint32_t p_idx, Error p_err) {
		if (p_out_errors) {
			p_out_errors[p_idx] = int32_t(p_err);
		}
		if (p_err != OK && first_error == OK) {
			first_error = p_err;
		}
	};

	// Phase 1: Validate all inputs, update GEOMETRY_PROOF & PARTICLE_LIGHT_64 status,
	// and lazily create any missing fast_refresh_guard_sets[bank] BEFORE compute_list_begin().
	for (uint32_t idx = 0; idx < p_count; ++idx) {
		const GDExtensionFastRefreshEndInputV1 &in = p_inputs[idx];
		if (in.abi_version != 1 || in.struct_size != sizeof(GDExtensionFastRefreshEndInputV1) ||
				in.reserved != 0 || in.parity_bank > 1 ||
				in.particles_rid == 0 || in.lease_serial == 0 || in.next_present_seq == 0) {
			set_item_result(idx, ERR_INVALID_PARAMETER);
			continue;
		}
		const RID particles_rid = RID::from_uint64(in.particles_rid);
		Particles *particles = particles_owner.get_or_null(particles_rid);
		if (!particles || !particles->external) {
			set_item_result(idx, ERR_DOES_NOT_EXIST);
			continue;
		}
		ExternalOutputOwner &owner = *particles->external;
		if (owner.device != rd || owner.state != ExternalOutputOwner::GenerationState::ACTIVE ||
				in.slot_index >= owner.slot_count) {
			set_item_result(idx, ERR_UNAVAILABLE);
			continue;
		}
		ExternalOutputOwner::Slot &slot = owner.slots[in.slot_index];
		if ((slot.state != ExternalOutputOwner::LeaseState::ADOPTED &&
					slot.state != ExternalOutputOwner::LeaseState::SEALED_RECORDED) ||
				slot.lease_serial != in.lease_serial ||
				slot.publication_closed || slot.pass_uses_closed || !slot.storage ||
				slot.original_pin.is_empty() || !owner.preparation_request(in.slot_index)) {
			set_item_result(idx, ERR_UNAVAILABLE);
			continue;
		}

		RenderingDevice::ProductionOutputFastRefreshEntry scope_entry;
		scope_entry.owner = &owner;
		scope_entry.slot_index = in.slot_index;
		scope_entry.lease_serial = slot.lease_serial;
		scope_entry.request = owner.preparation_request(in.slot_index);
		scope_entry.request_serial = owner.preparation_request_serial(in.slot_index);
		scope_entry.pin_serial = owner.preparation_pin_serial(in.slot_index);
		scope_entry.owned_group = slot.owned_group;
		rd->production_output_fast_refresh.push(scope_entry);

		const uint32_t view_idx = in.slot_index / owner.config.max_inflight;
		const uint32_t bank = in.parity_bank;

		if (owner.config.profile == ExternalOutputConfig::PARTICLE_LIGHT_64 ||
				owner.config.profile == ExternalOutputConfig::PARTICLE_DECAL_128) {
			const RID status_rid = slot.storage->resources[ExternalOutputStorage::GUARD_STATUS].rid;
			if (status_rid.is_valid()) {
				uint32_t status_words[8] = {};
				fast_refresh_encode_light64_guard_status(in.next_present_seq, status_words);
				const Error upd = rd->buffer_update(status_rid, 0, sizeof(status_words), status_words);
				if (upd != OK) {
					set_item_result(idx, upd);
					continue;
				}
			}
			slot.request.common.present_seq = in.next_present_seq;
			if (view_idx < owner.config.max_views && owner.views[view_idx].occupied) {
				owner.views[view_idx].last_present = MAX(owner.views[view_idx].last_present, in.next_present_seq);
			}
			if (slot.scene_cull_bound) {
				if (auto *scene = static_cast<RendererSceneCull *>(RSG::scene)) {
					RendererSceneCull::NativeParticleSceneFacts actual;
					if (scene->instance_get_committed_particle_scene_facts(slot.scene_cull_instance, slot.scene_cull_particles, actual) == OK &&
							actual.scenario == slot.scene_cull_scenario &&
							actual.world_aabb.is_finite() && actual.world_aabb.get_end().is_finite() && actual.world_aabb.has_volume()) {
						slot.scene_cull_version = actual.version;
						slot.scene_cull_local = actual.local_aabb;
						slot.scene_cull_world = actual.world_aabb;
					}
				}
			}
			const bool valid_bounds = slot.scene_cull_bound && slot.scene_cull_world.is_finite() &&
					slot.scene_cull_world.get_end().is_finite() && slot.scene_cull_world.has_volume();
			if (owner.config.profile == ExternalOutputConfig::PARTICLE_LIGHT_64) {
				if (mesh_light_sort_rd) {
					ParticlesMeshLightSortRD::ExternalLightSlotPresentSeqUpdate upd;
					upd.owner_key = in.particles_rid;
					upd.generation = owner.generation;
					upd.slot_index = in.slot_index;
					upd.lease_serial = slot.lease_serial;
					upd.next_present_seq = in.next_present_seq;
					if (valid_bounds) {
						upd.world_bounds = slot.scene_cull_world;
						upd.has_world_bounds = true;
					}
					push_light_update(upd);
				}
			} else {
				push_decal_update(BUILD_EXTERNAL_DECAL_SLOT_REGISTRATION(in.particles_rid, owner, in.slot_index));
			}
			slot.fast_refresh_active = true;
			set_item_result(idx, OK);
			++succeeded_count;
			continue;
		}

		if (max_groups_x == 0) {
			set_item_result(idx, ERR_UNAVAILABLE);
			continue;
		}

		// Guard against duplicate (owner, slot) within the same batch racing on GUARD_SCRATCH.
		bool duplicate_slot = false;
		for (uint32_t k = 0; k < gpu_item_count; ++k) {
			const BatchedGuardItem &prev = get_gpu_item(k);
			if (prev.owner == &owner && prev.slot_index == in.slot_index) {
				duplicate_slot = true;
				break;
			}
		}
		if (duplicate_slot) {
			set_item_result(idx, ERR_ALREADY_IN_USE);
			continue;
		}

		if (owner.config.profile == ExternalOutputConfig::INSTANCE_192) {
			using Guard = ParticlesExternalGuard;
			if (!external_guard || !external_guard->is_prepared() || !slot.guard_resources_ready ||
					!slot.guard_proof_payload || slot.guard_proof_bytes < sizeof(Guard::GeometryProofHeader)) {
				set_item_result(idx, ERR_UNAVAILABLE);
				continue;
			}
			Guard::GeometryProofHeader proof;
			memcpy(&proof, slot.guard_proof_payload, sizeof(proof));
			bool proof_dirty = false;
			if (in.source_count_word_offset != UINT32_MAX && proof.source_accessor[0] != in.source_count_word_offset) {
				proof.source_accessor[0] = in.source_count_word_offset;
				proof_dirty = true;
			}
			for (uint32_t g = 0; g < 4; ++g) {
				if (in.source_gate_word_offsets[g] != UINT32_MAX && proof.source_gate_words[g] != in.source_gate_word_offsets[g]) {
					proof.source_gate_words[g] = in.source_gate_word_offsets[g];
					proof_dirty = true;
				}
			}

			Guard::PushConstant push;
			push.capacity = owner.config.capacity;
			push.sequence_low = uint32_t(in.next_present_seq);
			push.sequence_high = uint32_t(in.next_present_seq >> 32);
			push.surface_count = slot.surface_count;
			push.record_base = 0;
			push.record_count = push.capacity;

			const bool no_previous_records = push.capacity != 0 && proof.options[0] == 0 &&
					slot.storage->resources[ExternalOutputStorage::PREVIOUS_RECORDS].rid.is_null() &&
					slot.storage->resources[ExternalOutputStorage::PREVIOUS_RECORDS].bytes == 0;
			const bool aux_enabled = push.capacity != 0 && proof.options[1] == 1;
			Guard::Mode init_mode = Guard::INIT;
			Guard::Mode validate_mode = Guard::VALIDATE;
			Guard::Mode finalize_mode = Guard::FINALIZE;
			if (push.capacity == 0) {
				init_mode = Guard::INIT_EMPTY;
				finalize_mode = Guard::FINALIZE_EMPTY;
			} else if (no_previous_records) {
				init_mode = aux_enabled ? Guard::INIT_NO_MOTION_AUX : Guard::INIT_NO_MOTION;
				validate_mode = aux_enabled ? Guard::VALIDATE_NO_MOTION_AUX : Guard::VALIDATE_NO_MOTION;
				finalize_mode = aux_enabled ? Guard::FINALIZE_NO_MOTION_AUX : Guard::FINALIZE_NO_MOTION;
			} else if (aux_enabled) {
				init_mode = Guard::INIT_AUX;
				validate_mode = Guard::VALIDATE_AUX;
				finalize_mode = Guard::FINALIZE_AUX;
			}

			const bool need_validate = fast_refresh_item_needs_validate(skip_validate, push.capacity, proof.options[2]);
			const uint32_t mode_count = push.capacity ? 3u : 2u;
			const uint32_t check_count = need_validate ? 3u : 2u;
			const Guard::Mode check_modes[3] = { init_mode, need_validate ? validate_mode : finalize_mode, finalize_mode };

			bool cache_hit = (slot.fast_refresh_source_count_rid[bank] == in.source_count_rid) && in.source_count_rid != 0;
			for (uint32_t g = 0; g < 4 && cache_hit; ++g) {
				cache_hit = (slot.fast_refresh_source_gate_rids[bank][g] == in.source_gate_rids[g]) && in.source_gate_rids[g] != 0;
			}
			for (uint32_t i = 0; i < check_count && cache_hit; ++i) {
				const RID set = slot.fast_refresh_guard_sets[bank][check_modes[i]];
				cache_hit = set.is_valid() && rd->uniform_set_is_valid(set);
			}

			if (!cache_hit || proof_dirty) {
				Guard::Bindings bindings;
				bool binding_ok = true;
				for (uint32_t binding = 0; binding < Guard::BINDING_COUNT; ++binding) {
					if (binding == Guard::SOURCE_COUNT) {
						const RID rid = RID::from_uint64(in.source_count_rid);
						const RenderingDevice::Buffer *buf = rd->_production_output_storage_buffer(rid);
						if (!buf) {
							binding_ok = false;
							break;
						}
						bindings.buffers[binding] = { rid, buf->size };
					} else if (binding >= Guard::SOURCE_COUNT_VALIDITY && binding <= Guard::SOURCE_INSTANCE_GATE) {
						const uint32_t g = binding - Guard::SOURCE_COUNT_VALIDITY;
						const RID rid = RID::from_uint64(in.source_gate_rids[g]);
						const RenderingDevice::Buffer *buf = rd->_production_output_storage_buffer(rid);
						if (!buf) {
							binding_ok = false;
							break;
						}
						bindings.buffers[binding] = { rid, buf->size };
					} else {
						const uint32_t role = binding == Guard::INSTANCE_AUX_PARAMS
								? uint32_t(ExternalOutputStorage::INSTANCE_AUX_PARAMS)
								: (binding <= Guard::OUTPUT_META ? binding : binding - 1);
						const ExternalOutputStorage::Resource &res = slot.storage->resources[role];
						bindings.buffers[binding] = { res.rid, res.bytes };
					}
				}
				if (!binding_ok) {
					set_item_result(idx, ERR_INVALID_PARAMETER);
					continue;
				}
				Guard::LayoutPlan plan;
				const Error planned = Guard::plan_layout(push, bindings, proof, max_groups_x, plan);
				if (planned != OK) {
					set_item_result(idx, planned);
					continue;
				}
				init_mode = plan.initialize_mode;
				validate_mode = plan.validate_mode;
				finalize_mode = plan.finalize_mode;
				if (proof_dirty) {
					const RID proof_rid = slot.storage->resources[ExternalOutputStorage::GEOMETRY_PROOF].rid;
					if (proof_rid.is_null()) {
						set_item_result(idx, ERR_UNAVAILABLE);
						continue;
					}
					memcpy(slot.guard_proof_payload, &proof, sizeof(proof));
					const Error upd = rd->buffer_update(proof_rid, 0, sizeof(proof), slot.guard_proof_payload);
					if (upd != OK) {
						set_item_result(idx, upd);
						continue;
					}
				}
				if (!cache_hit) {
					for (uint32_t m = 0; m < ExternalOutputOwner::GUARD_MODE_CAPACITY; ++m) {
						if (slot.fast_refresh_guard_sets[bank][m].is_valid()) {
							if (rd->uniform_set_is_valid(slot.fast_refresh_guard_sets[bank][m])) {
								rd->free_rid(slot.fast_refresh_guard_sets[bank][m]);
							}
							slot.fast_refresh_guard_sets[bank][m] = RID();
						}
					}
					slot.fast_refresh_source_count_rid[bank] = 0;
					for (uint32_t g = 0; g < 4; ++g) {
						slot.fast_refresh_source_gate_rids[bank][g] = 0;
					}
					const Guard::Mode active_modes[3] = { init_mode, push.capacity ? validate_mode : finalize_mode, finalize_mode };
					Error create_err = OK;
					for (uint32_t i = 0; i < mode_count; ++i) {
						const Guard::Mode mode = active_modes[i];
						const RID shader = external_guard->get_shader(mode);
						if (shader.is_null()) {
							create_err = ERR_UNAVAILABLE;
						} else {
							const RID created = rd->uniform_set_create(plan.uniforms, shader, 0);
							if (created.is_null()) {
								create_err = rd->get_last_uniform_set_create_error() == ERR_BUSY ? ERR_BUSY : ERR_CANT_CREATE;
							} else {
								slot.fast_refresh_guard_sets[bank][mode] = created;
							}
						}
						if (create_err != OK) {
							for (uint32_t j = 0; j < i; ++j) {
								const Guard::Mode prev_mode = active_modes[j];
								if (slot.fast_refresh_guard_sets[bank][prev_mode].is_valid()) {
									rd->free_rid(slot.fast_refresh_guard_sets[bank][prev_mode]);
									slot.fast_refresh_guard_sets[bank][prev_mode] = RID();
								}
							}
							break;
						}
					}
					if (create_err != OK) {
						set_item_result(idx, create_err);
						continue;
					}
					slot.fast_refresh_source_count_rid[bank] = in.source_count_rid;
					for (uint32_t g = 0; g < 4; ++g) {
						slot.fast_refresh_source_gate_rids[bank][g] = in.source_gate_rids[g];
					}
				}
			}

			const RID init_pipe = external_guard->get_pipeline(init_mode);
			const RID finalize_pipe = external_guard->get_pipeline(finalize_mode);
			const RID validate_pipe = need_validate ? external_guard->get_pipeline(validate_mode) : RID();
			const RID init_set = slot.fast_refresh_guard_sets[bank][init_mode];
			const RID finalize_set = slot.fast_refresh_guard_sets[bank][finalize_mode];
			const RID validate_set = need_validate ? slot.fast_refresh_guard_sets[bank][validate_mode] : RID();
			if (init_pipe.is_null() || finalize_pipe.is_null() || init_set.is_null() || finalize_set.is_null() ||
					(need_validate && (validate_pipe.is_null() || validate_set.is_null()))) {
				set_item_result(idx, ERR_UNAVAILABLE);
				continue;
			}

			BatchedGuardItem item;
			item.input_index = idx;
			item.owner = &owner;
			item.slot = &slot;
			item.slot_index = in.slot_index;
			item.view_idx = view_idx;
			item.capacity = push.capacity;
			item.surface_count = push.surface_count;
			item.next_present_seq = in.next_present_seq;
			item.init_pipe = init_pipe;
			item.validate_pipe = validate_pipe;
			item.finalize_pipe = finalize_pipe;
			item.init_set = init_set;
			item.validate_set = validate_set;
			item.finalize_set = finalize_set;
			item.need_validate = need_validate;
			if (need_validate) {
				any_need_validate = true;
			}
			push_gpu_item(item);
			continue;
		}

		if (owner.config.profile == ExternalOutputConfig::INDEXED_144) {
			using Guard = ParticlesExternalGuardIndexed144;
			if (!external_guard_indexed144 || !external_guard_indexed144->is_prepared() || !slot.guard_resources_ready ||
					!slot.guard_proof_payload || slot.guard_proof_bytes < sizeof(Guard::GeometryProofHeader)) {
				set_item_result(idx, ERR_UNAVAILABLE);
				continue;
			}
			Guard::GeometryProofHeader proof;
			memcpy(&proof, slot.guard_proof_payload, sizeof(proof));
			bool proof_dirty = false;
			if (in.source_count_word_offset != UINT32_MAX && proof.source_accessor[0] != in.source_count_word_offset) {
				proof.source_accessor[0] = in.source_count_word_offset;
				proof_dirty = true;
			}
			for (uint32_t g = 0; g < 4; ++g) {
				if (in.source_gate_word_offsets[g] != UINT32_MAX && proof.source_gate_words[g] != in.source_gate_word_offsets[g]) {
					proof.source_gate_words[g] = in.source_gate_word_offsets[g];
					proof_dirty = true;
				}
			}

			Guard::PushConstant push;
			push.capacity = owner.config.capacity;
			push.sequence_low = uint32_t(in.next_present_seq);
			push.sequence_high = uint32_t(in.next_present_seq >> 32);
			push.surface_count = slot.surface_count;
			push.segment_base = 0;
			push.segment_count = push.capacity;

			const bool motion_required = (proof.options[0] == 1u);
			const bool motion_buffers_omitted = !motion_required && push.capacity > 0 &&
					slot.storage->resources[ExternalOutputStorage::PREVIOUS_RECORDS].rid.is_null() &&
					slot.storage->resources[ExternalOutputStorage::PREVIOUS_RECORDS].bytes == 0 &&
					slot.storage->resources[ExternalOutputStorage::INDEXED_VERTEX_HISTORY_VALIDITY].rid.is_null() &&
					slot.storage->resources[ExternalOutputStorage::INDEXED_VERTEX_HISTORY_VALIDITY].bytes == 0;
			Guard::Mode init_mode = Guard::INIT;
			Guard::Mode validate_mode = Guard::VALIDATE;
			Guard::Mode finalize_mode = Guard::FINALIZE;
			if (push.capacity == 0) {
				init_mode = Guard::INIT_EMPTY;
				finalize_mode = Guard::FINALIZE_EMPTY;
			} else if (motion_buffers_omitted) {
				init_mode = Guard::INIT_NO_MOTION;
				validate_mode = Guard::VALIDATE_NO_MOTION;
				finalize_mode = Guard::FINALIZE_NO_MOTION;
			}

			const bool need_validate = fast_refresh_item_needs_validate(skip_validate, push.capacity, proof.options[2]);
			const uint32_t mode_count = push.capacity ? 3u : 2u;
			const uint32_t check_count = need_validate ? 3u : 2u;
			const Guard::Mode check_modes[3] = { init_mode, need_validate ? validate_mode : finalize_mode, finalize_mode };

			bool cache_hit = (slot.fast_refresh_source_count_rid[bank] == in.source_count_rid) && in.source_count_rid != 0;
			for (uint32_t g = 0; g < 4 && cache_hit; ++g) {
				cache_hit = (slot.fast_refresh_source_gate_rids[bank][g] == in.source_gate_rids[g]) && in.source_gate_rids[g] != 0;
			}
			for (uint32_t i = 0; i < check_count && cache_hit; ++i) {
				const RID set = slot.fast_refresh_guard_sets[bank][check_modes[i]];
				cache_hit = set.is_valid() && rd->uniform_set_is_valid(set);
			}

			if (!cache_hit || proof_dirty) {
				auto role_for_binding = [](uint32_t p_binding) -> uint32_t {
					switch (p_binding) {
						case Guard::CURRENT_VERTICES:
							return ExternalOutputStorage::CURRENT_RECORDS;
						case Guard::ALIGNED_PREVIOUS_VERTICES:
							return ExternalOutputStorage::PREVIOUS_RECORDS;
						case Guard::VERTEX_HISTORY_VALIDITY:
							return ExternalOutputStorage::INDEXED_VERTEX_HISTORY_VALIDITY;
						case Guard::INDEX_STAGING:
							return ExternalOutputStorage::INDEXED_STAGING;
						case Guard::OUTPUT_META:
							return ExternalOutputStorage::OUTPUT_META;
						case Guard::GUARD_SCRATCH:
							return ExternalOutputStorage::GUARD_SCRATCH;
						case Guard::GUARD_STATUS:
							return ExternalOutputStorage::GUARD_STATUS;
						case Guard::PROTECTED_COMMANDS:
							return ExternalOutputStorage::PROTECTED_COMMANDS;
						case Guard::GEOMETRY_PROOF:
							return ExternalOutputStorage::GEOMETRY_PROOF;
						default:
							return UINT32_MAX;
					}
				};
				Guard::Bindings bindings;
				Error binding_err = OK;
				for (uint32_t binding = 0; binding < Guard::BINDING_COUNT; ++binding) {
					if (binding == Guard::SOURCE_COUNT) {
						const RID rid = RID::from_uint64(in.source_count_rid);
						const RenderingDevice::Buffer *buf = rd->_production_output_storage_buffer(rid);
						if (!buf) {
							binding_err = ERR_INVALID_PARAMETER;
							break;
						}
						bindings.buffers[binding] = { rid, buf->size };
					} else if (binding >= Guard::SOURCE_COUNT_VALIDITY && binding <= Guard::SOURCE_INSTANCE_GATE) {
						const uint32_t g = binding - Guard::SOURCE_COUNT_VALIDITY;
						const RID rid = RID::from_uint64(in.source_gate_rids[g]);
						const RenderingDevice::Buffer *buf = rd->_production_output_storage_buffer(rid);
						if (!buf) {
							binding_err = ERR_INVALID_PARAMETER;
							break;
						}
						bindings.buffers[binding] = { rid, buf->size };
					} else {
						const uint32_t role = role_for_binding(binding);
						if (role == UINT32_MAX) {
							binding_err = ERR_INVALID_DATA;
							break;
						}
						const ExternalOutputStorage::Resource &res = slot.storage->resources[role];
						bindings.buffers[binding] = { res.rid, res.bytes };
					}
				}
				if (binding_err != OK) {
					set_item_result(idx, binding_err);
					continue;
				}
				Guard::LayoutPlan plan;
				const Error planned = Guard::plan_layout(push, bindings, proof, max_groups_x, plan);
				if (planned != OK) {
					set_item_result(idx, planned);
					continue;
				}
				init_mode = plan.initialize_mode;
				validate_mode = plan.validate_mode;
				finalize_mode = plan.finalize_mode;
				if (proof_dirty) {
					const RID proof_rid = slot.storage->resources[ExternalOutputStorage::GEOMETRY_PROOF].rid;
					if (proof_rid.is_null()) {
						set_item_result(idx, ERR_UNAVAILABLE);
						continue;
					}
					memcpy(slot.guard_proof_payload, &proof, sizeof(proof));
					const Error upd = rd->buffer_update(proof_rid, 0, sizeof(proof), slot.guard_proof_payload);
					if (upd != OK) {
						set_item_result(idx, upd);
						continue;
					}
				}
				if (!cache_hit) {
					for (uint32_t m = 0; m < ExternalOutputOwner::GUARD_MODE_CAPACITY; ++m) {
						if (slot.fast_refresh_guard_sets[bank][m].is_valid()) {
							if (rd->uniform_set_is_valid(slot.fast_refresh_guard_sets[bank][m])) {
								rd->free_rid(slot.fast_refresh_guard_sets[bank][m]);
							}
							slot.fast_refresh_guard_sets[bank][m] = RID();
						}
					}
					slot.fast_refresh_source_count_rid[bank] = 0;
					for (uint32_t g = 0; g < 4; ++g) {
						slot.fast_refresh_source_gate_rids[bank][g] = 0;
					}
					const Guard::Mode active_modes[3] = { init_mode, push.capacity ? validate_mode : finalize_mode, finalize_mode };
					Error create_err = OK;
					for (uint32_t i = 0; i < mode_count; ++i) {
						const Guard::Mode mode = active_modes[i];
						const RID shader = external_guard_indexed144->get_shader(mode);
						if (shader.is_null()) {
							create_err = ERR_UNAVAILABLE;
						} else {
							const RID created = rd->uniform_set_create(plan.uniforms, shader, 0);
							if (created.is_null()) {
								create_err = rd->get_last_uniform_set_create_error() == ERR_BUSY ? ERR_BUSY : ERR_CANT_CREATE;
							} else {
								slot.fast_refresh_guard_sets[bank][mode] = created;
							}
						}
						if (create_err != OK) {
							for (uint32_t j = 0; j < i; ++j) {
								const Guard::Mode prev_mode = active_modes[j];
								if (slot.fast_refresh_guard_sets[bank][prev_mode].is_valid()) {
									rd->free_rid(slot.fast_refresh_guard_sets[bank][prev_mode]);
									slot.fast_refresh_guard_sets[bank][prev_mode] = RID();
								}
							}
							break;
						}
					}
					if (create_err != OK) {
						set_item_result(idx, create_err);
						continue;
					}
					slot.fast_refresh_source_count_rid[bank] = in.source_count_rid;
					for (uint32_t g = 0; g < 4; ++g) {
						slot.fast_refresh_source_gate_rids[bank][g] = in.source_gate_rids[g];
					}
				}
			}

			const RID init_pipe = external_guard_indexed144->get_pipeline(init_mode);
			const RID finalize_pipe = external_guard_indexed144->get_pipeline(finalize_mode);
			const RID validate_pipe = need_validate ? external_guard_indexed144->get_pipeline(validate_mode) : RID();
			const RID init_set = slot.fast_refresh_guard_sets[bank][init_mode];
			const RID finalize_set = slot.fast_refresh_guard_sets[bank][finalize_mode];
			const RID validate_set = need_validate ? slot.fast_refresh_guard_sets[bank][validate_mode] : RID();
			if (init_pipe.is_null() || finalize_pipe.is_null() || init_set.is_null() || finalize_set.is_null() ||
					(need_validate && (validate_pipe.is_null() || validate_set.is_null()))) {
				set_item_result(idx, ERR_UNAVAILABLE);
				continue;
			}

			BatchedGuardItem item;
			item.input_index = idx;
			item.owner = &owner;
			item.slot = &slot;
			item.slot_index = in.slot_index;
			item.view_idx = view_idx;
			item.capacity = push.capacity;
			item.surface_count = push.surface_count;
			item.next_present_seq = in.next_present_seq;
			item.init_pipe = init_pipe;
			item.validate_pipe = validate_pipe;
			item.finalize_pipe = finalize_pipe;
			item.init_set = init_set;
			item.validate_set = validate_set;
			item.finalize_set = finalize_set;
			item.need_validate = need_validate;
			if (need_validate) {
				any_need_validate = true;
			}
			push_gpu_item(item);
			continue;
		}

		set_item_result(idx, ERR_UNAVAILABLE);
	}

	// Phase 2: Single compute_list executing all INSTANCE_192 and INDEXED_144 guards:
	//   Full mode:          Pass 1 (all INITs) -> 1 barrier -> Pass 2 (all VALIDATEs) -> 1 barrier -> Pass 3 (all FINALIZEs)
	//   Trusted Tier 2:     Pass 1 (all INITs) -> 1 barrier -> Pass 3 (all FINALIZEs)  [skips VALIDATE & 2nd barrier!]
	if (gpu_item_count > 0) {
		const RD::ComputeListID cl = rd->compute_list_begin();
		if (cl == RD::INVALID_ID) {
			for (uint32_t k = 0; k < gpu_item_count; ++k) {
				set_item_result(get_gpu_item(k).input_index, ERR_CANT_CREATE);
			}
		} else {
			struct GuardPushConstant32 {
				uint32_t capacity = 0;
				uint32_t sequence_low = 0;
				uint32_t sequence_high = 0;
				uint32_t surface_count = 0;
				uint32_t base = 0;
				uint32_t count = 0;
				uint32_t reserved0 = 0;
				uint32_t reserved1 = 0;
			};
			static_assert(sizeof(GuardPushConstant32) == sizeof(ParticlesExternalGuard::PushConstant));
			static_assert(sizeof(GuardPushConstant32) == sizeof(ParticlesExternalGuardIndexed144::PushConstant));

			// Pass 1: All INITs (0 barriers between independent outputs).
			RID bound_pipe;
			for (uint32_t k = 0; k < gpu_item_count; ++k) {
				const BatchedGuardItem &item = get_gpu_item(k);
				GuardPushConstant32 stage_pc;
				stage_pc.capacity = item.capacity;
				stage_pc.sequence_low = uint32_t(item.next_present_seq);
				stage_pc.sequence_high = uint32_t(item.next_present_seq >> 32);
				stage_pc.surface_count = item.surface_count;
				if (item.init_pipe != bound_pipe) {
					rd->compute_list_bind_compute_pipeline(cl, item.init_pipe);
					bound_pipe = item.init_pipe;
				}
				rd->compute_list_bind_uniform_set(cl, item.init_set, 0);
				rd->compute_list_set_push_constant(cl, &stage_pc, sizeof(stage_pc));
				rd->compute_list_dispatch(cl, 1, 1, 1);
			}
			rd->compute_list_add_barrier(cl);

			// Pass 2: All VALIDATEs required by this batch (skipped completely when every item has need_validate == false,
			// e.g. single-mesh TRUSTED_TIER2 batches; multi-mesh INSTANCE_192 items always need VALIDATE for mesh_first/mesh_count).
			if (any_need_validate) {
				bool any_validated = false;
				bound_pipe = RID();
				for (uint32_t k = 0; k < gpu_item_count; ++k) {
					const BatchedGuardItem &item = get_gpu_item(k);
					if (!item.need_validate || item.capacity == 0) {
						continue;
					}
					any_validated = true;
					if (item.validate_pipe != bound_pipe) {
						rd->compute_list_bind_compute_pipeline(cl, item.validate_pipe);
						bound_pipe = item.validate_pipe;
					}
					rd->compute_list_bind_uniform_set(cl, item.validate_set, 0);
					const uint64_t chunk_capacity = MIN(uint64_t(max_groups_x) * 64ull, uint64_t(item.capacity));
					for (uint32_t base = 0; base < item.capacity;) {
						GuardPushConstant32 chunk;
						chunk.capacity = item.capacity;
						chunk.sequence_low = uint32_t(item.next_present_seq);
						chunk.sequence_high = uint32_t(item.next_present_seq >> 32);
						chunk.surface_count = item.surface_count;
						chunk.base = base;
						chunk.count = uint32_t(MIN(chunk_capacity, uint64_t(item.capacity - base)));
						const uint32_t groups_x = (chunk.count + 63u) / 64u;
						rd->compute_list_set_push_constant(cl, &chunk, sizeof(chunk));
						rd->compute_list_dispatch(cl, groups_x, 1, 1);
						base += chunk.count;
					}
				}
				if (any_validated) {
					rd->compute_list_add_barrier(cl);
				}
			}

			// Pass 3: All FINALIZEs (0 barriers between independent outputs).
			bound_pipe = RID();
			for (uint32_t k = 0; k < gpu_item_count; ++k) {
				const BatchedGuardItem &item = get_gpu_item(k);
				GuardPushConstant32 stage_pc;
				stage_pc.capacity = item.capacity;
				stage_pc.sequence_low = uint32_t(item.next_present_seq);
				stage_pc.sequence_high = uint32_t(item.next_present_seq >> 32);
				stage_pc.surface_count = item.surface_count;
				if (item.finalize_pipe != bound_pipe) {
					rd->compute_list_bind_compute_pipeline(cl, item.finalize_pipe);
					bound_pipe = item.finalize_pipe;
				}
				rd->compute_list_bind_uniform_set(cl, item.finalize_set, 0);
				rd->compute_list_set_push_constant(cl, &stage_pc, sizeof(stage_pc));
				rd->compute_list_dispatch(cl, 1, 1, 1);
			}
			rd->compute_list_end();

			// Phase 3: Commit present_seq metadata for all succeeded GPU outputs.
			for (uint32_t k = 0; k < gpu_item_count; ++k) {
				const BatchedGuardItem &item = get_gpu_item(k);
				item.slot->request.common.present_seq = item.next_present_seq;
				if (item.view_idx < item.owner->config.max_views && item.owner->views[item.view_idx].occupied) {
					item.owner->views[item.view_idx].last_present = MAX(item.owner->views[item.view_idx].last_present, item.next_present_seq);
				}
				item.slot->fast_refresh_active = true;
				set_item_result(item.input_index, OK);
				++succeeded_count;
			}
		}
	}

	// Phase 4: Commit all PARTICLE_LIGHT_64 & PARTICLE_DECAL_128 present_seq updates in a single lock acquisition each.
	if (light_update_count > 0 && mesh_light_sort_rd) {
		if (light_update_count <= INLINE_BATCH_CAPACITY) {
			mesh_light_sort_rd->update_external_light_slots_present_seq_batch(inline_light_updates, light_update_count);
		} else {
			LocalVector<ParticlesMeshLightSortRD::ExternalLightSlotPresentSeqUpdate> combined;
			combined.reserve(light_update_count);
			for (uint32_t i = 0; i < INLINE_BATCH_CAPACITY; ++i) {
				combined.push_back(inline_light_updates[i]);
			}
			for (uint32_t i = 0; i < overflow_light_updates.size(); ++i) {
				combined.push_back(overflow_light_updates[i]);
			}
			mesh_light_sort_rd->update_external_light_slots_present_seq_batch(combined.ptr(), combined.size());
		}
	}
	if (decal_update_count > 0) {
		if (ParticlesDecalBridgeRD *decal_bridge = get_decal_bridge_rd()) {
			if (decal_update_count <= INLINE_BATCH_CAPACITY) {
				decal_bridge->upsert_external_decal_slots_batch(inline_decal_updates, decal_update_count);
			} else {
				LocalVector<ParticlesDecalBridgeRD::ExternalDecalSlotRegistration> combined;
				combined.reserve(decal_update_count);
				for (uint32_t i = 0; i < INLINE_BATCH_CAPACITY; ++i) {
					combined.push_back(inline_decal_updates[i]);
				}
				for (uint32_t i = 0; i < overflow_decal_updates.size(); ++i) {
					combined.push_back(overflow_decal_updates[i]);
				}
				decal_bridge->upsert_external_decal_slots_batch(combined.ptr(), combined.size());
			}
		}
	}

	if (succeeded_count == p_count) {
		return int32_t(OK);
	}
	if (p_out_errors != nullptr && succeeded_count > 0) {
		return int32_t(OK);
	}
	return int32_t(first_error != OK ? first_error : ERR_UNAVAILABLE);
}

} // namespace RendererRD

namespace GDExtensionDeviceExit {

int32_t ange_external_output_begin_fast_refresh_v1(const GDExtensionFastRefreshBeginInputV1 *p_input, GDExtensionFastRefreshTargetsV1 *p_out_targets) {
	RendererRD::ParticlesStorage *storage = RendererRD::ParticlesStorage::get_singleton();
	if (!storage) {
		if (p_out_targets) {
			*p_out_targets = {};
			p_out_targets->abi_version = 1;
			p_out_targets->struct_size = sizeof(GDExtensionFastRefreshTargetsV1);
			p_out_targets->native_error = int32_t(ERR_UNAVAILABLE);
			p_out_targets->slot_index = UINT32_MAX;
		}
		return int32_t(ERR_UNAVAILABLE);
	}
	return storage->particles_external_begin_fast_refresh_v1(p_input, p_out_targets);
}

int32_t ange_external_output_end_fast_refresh_v1(const GDExtensionFastRefreshEndInputV1 *p_input) {
	RendererRD::ParticlesStorage *storage = RendererRD::ParticlesStorage::get_singleton();
	if (!storage) {
		return int32_t(ERR_UNAVAILABLE);
	}
	return storage->particles_external_end_fast_refresh_v1(p_input);
}

int32_t ange_external_output_release_fast_refresh_v1(uint64_t p_particles_rid, uint32_t p_slot_index) {
	RendererRD::ParticlesStorage *storage = RendererRD::ParticlesStorage::get_singleton();
	if (!storage) {
		return int32_t(ERR_UNAVAILABLE);
	}
	return storage->particles_external_release_fast_refresh_v1(p_particles_rid, p_slot_index);
}

int32_t ange_external_output_begin_fast_refresh_batch_v1(uint32_t p_count, const GDExtensionFastRefreshBeginInputV1 *p_inputs, GDExtensionFastRefreshTargetsV1 *p_out_targets) {
	RendererRD::ParticlesStorage *storage = RendererRD::ParticlesStorage::get_singleton();
	if (!storage) {
		if (p_out_targets && p_count > 0) {
			for (uint32_t i = 0; i < p_count; ++i) {
				p_out_targets[i] = {};
				p_out_targets[i].abi_version = 1;
				p_out_targets[i].struct_size = sizeof(GDExtensionFastRefreshTargetsV1);
				p_out_targets[i].native_error = int32_t(ERR_UNAVAILABLE);
				p_out_targets[i].slot_index = UINT32_MAX;
			}
		}
		return int32_t(ERR_UNAVAILABLE);
	}
	return storage->particles_external_begin_fast_refresh_batch_v1(p_count, p_inputs, p_out_targets);
}

int32_t ange_external_output_end_fast_refresh_batch_v1(uint32_t p_count, const GDExtensionFastRefreshEndInputV1 *p_inputs, uint32_t p_flags, int32_t *p_out_errors) {
	RendererRD::ParticlesStorage *storage = RendererRD::ParticlesStorage::get_singleton();
	if (!storage) {
		if (p_out_errors && p_count > 0) {
			for (uint32_t i = 0; i < p_count; ++i) {
				p_out_errors[i] = int32_t(ERR_UNAVAILABLE);
			}
		}
		return int32_t(ERR_UNAVAILABLE);
	}
	return storage->particles_external_end_fast_refresh_batch_v1(p_count, p_inputs, p_flags, p_out_errors);
}

int32_t ange_external_output_query_viewport_depth_pair_v1(const GDExtensionViewportDepthPairQueryInputV1 *p_input, GDExtensionViewportDepthPairResultV1 *p_out_result) {
	if (p_out_result) {
		*p_out_result = {};
		p_out_result->abi_version = 1;
		p_out_result->struct_size = sizeof(GDExtensionViewportDepthPairResultV1);
		p_out_result->native_error = int32_t(ERR_INVALID_PARAMETER);
	}
	auto fail = [&](Error p_err) -> int32_t {
		if (p_out_result) {
			p_out_result->native_error = int32_t(p_err);
		}
		return int32_t(p_err);
	};
	if (!p_input || !p_out_result ||
			p_input->abi_version != 1 ||
			p_input->struct_size != sizeof(GDExtensionViewportDepthPairQueryInputV1) ||
			p_input->arm_capture > 1 ||
			p_input->reserved != 0 ||
			p_input->viewport_rid == 0) {
		return fail(ERR_INVALID_PARAMETER);
	}
	RendererRD::ParticlesStorage *storage = RendererRD::ParticlesStorage::get_singleton();
	RendererRD::ParticlesCollisionSceneRD *collision_scene = storage ? storage->get_collision_scene_rd() : nullptr;
	if (!collision_scene) {
		return fail(ERR_UNAVAILABLE);
	}
	const Error err = collision_scene->query_and_arm_viewport_depth_pair(
			RID::from_uint64(p_input->viewport_rid),
			p_input->arm_capture != 0,
			p_input->slab_thickness_m,
			p_input->silhouette_jump_m,
			*p_out_result);
	p_out_result->native_error = int32_t(err);
	return int32_t(err);
}

} // namespace GDExtensionDeviceExit
