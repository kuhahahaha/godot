/**************************************************************************/
/*  external_output_owner.cpp                                             */
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

#include "external_output_owner.h"

#include "core/extension/gdextension_library_exit_guard.h"
#include "servers/rendering/renderer_rd/effects/particles_external_guard.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_packet.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_storage.h"

#include <cstddef>

namespace RendererRD {
Error ExternalOutputOwner::validate_native_selection(const ExternalOutputConfig &config, uint32_t &surface_count) {
	if (config.profile == ExternalOutputConfig::SCENE_ONLY) {
		if (!config.surfaces.is_empty() || !config.surface_materials.is_empty()) {
			return ERR_INVALID_PARAMETER;
		}
		surface_count = 0;
		return OK;
	}
	if (config.surfaces.size() > ExternalOutputConfig::MAX_MESH_RIDS ||
			config.surface_materials.is_empty() || config.surface_materials.size() > ExternalOutputConfig::MAX_FLATTENED_SURFACES) {
		return ERR_INVALID_PARAMETER;
	}
	auto *meshes = MeshStorage::get_singleton();
	auto *materials = MaterialStorage::get_singleton();
	if (!meshes || !materials) {
		return ERR_UNAVAILABLE;
	}
	uint32_t flattened = 0;
	if (config.profile == ExternalOutputConfig::INSTANCE_192) {
		if (config.surfaces.is_empty()) {
			return ERR_INVALID_PARAMETER;
		}
		for (RID mesh : config.surfaces) {
			if (!meshes->owns_mesh(mesh)) {
				return ERR_INVALID_PARAMETER;
			}
			const int count = meshes->mesh_get_surface_count(mesh);
			if (count <= 0 || uint32_t(count) > ExternalOutputConfig::MAX_FLATTENED_SURFACES - flattened) {
				return ERR_INVALID_PARAMETER;
			}
			flattened += uint32_t(count);
		}
		if (flattened != uint32_t(config.surface_materials.size())) {
			return ERR_INVALID_PARAMETER;
		}
	} else if (config.profile == ExternalOutputConfig::INDEXED_144 && config.surfaces.is_empty()) {
		flattened = config.surface_materials.size();
	} else {
		return ERR_INVALID_PARAMETER;
	}
	for (RID material : config.surface_materials) {
		if (!materials->owns_material(material)) {
			return ERR_INVALID_PARAMETER;
		}
		MaterialStorage::MaterialData *data = nullptr;
		const Error error = materials->material_get_native_preparation_data(material, materials->material_get_generation(material), data);
		if (error != OK) {
			return error;
		}
	}
	surface_count = flattened;
	return OK;
}

void ExternalOutputOwner::invalidate_generation_demand() {
	// Same original RT lifecycle as withdraw/unpin. A nonempty original root
	// keeps this request arena alive. No Registry lock is taken: withdraw can
	// already be inside that lock during original retirement service.
	if (!generation_pin.is_empty() && generation_pin.request && generation_pin.cell_index < generation_pin.request->capacity.resource_uses) {
		generation_pin.request->uses[generation_pin.cell_index].output_demand_live.store(false, std::memory_order_release);
	}
}

bool ExternalOutputOwner::generation_root_matches(const GDExtensionDeviceExit::ProductionOutputPin &pin) const {
	return &pin == &generation_pin && !pin.is_empty() && generation_registry == pin.registry &&
			generation_request.registry_instance && generation_request.handle_serial == pin.request_serial &&
			generation_root_serial && generation_root_serial == pin.serial;
}

bool ExternalOutputOwner::has_inflight_preparation() const {
	if (generation_pin.request && generation_pin.request->continuation_inflight) {
		return true;
	}
	for (uint32_t i = 0; i < slot_count; ++i) {
		const auto *request = slots[i].original_pin.request;
		if (request && request->continuation_inflight) {
			return true;
		}
	}
	return false;
}

Error ExternalOutputOwner::generation_returnable(uint32_t &unused, uint32_t &returned) const {
	unused = returned = 0;
	if (has_inflight_preparation() || (state != GenerationState::WITHDRAWING && state != GenerationState::RETIRING && state != GenerationState::FAULTED_RETIRING) || has_live_slots() || generation_surface_has_custody()) {
		return ERR_BUSY;
	}
	for (uint32_t i = 0; i < slot_count; ++i) {
		const auto &slot = slots[i];
		if (!slot.publication_closed || !slot.pass_uses_closed || slot.storage || slot.surfaces || slot.surface_count || slot.preparation_reserved_count || slot.preparation_selected_count || !slot.original_pin.is_empty() || slot.guard_owned_group || slot.guard_resources_ready || slot.guard_proof_payload || slot.owned_group || slot.output_membership_group || slot_has_custody(i)) {
			return ERR_BUSY;
		}
		unused += slot.state == LeaseState::EMPTY;
		returned += slot.state == LeaseState::RELEASED;
	}
	for (uint32_t i = 0; i < config.max_views; ++i) {
		if (!views[i].closed || views[i].history_slot != UINT32_MAX) {
			return ERR_BUSY;
		}
	}
	return OK;
}

Error ExternalOutputOwner::install_generation_root(GDExtensionDeviceExit::ProductionPhysicalContextRef context,
		GDExtensionProductionRequestRefV1 request, GDExtensionDeviceExit::Registry &registry, RenderingDevice &rd, RID particles,
		GDExtensionDeviceExit::ProductionOutputMetadataLease *metadata) {
	if (!generation_pin.is_empty() || generation_registry || generation_request.registry_instance || generation_request.handle_serial || generation_root_serial) {
		return ERR_ALREADY_IN_USE;
	}
	if (device != &rd || state != GenerationState::CONFIGURED || has_live_slots()) {
		return ERR_INVALID_PARAMETER;
	}
	// Real current protected RECORD and original prequoted use capacity only.
	// This does not install any source/Host/phase or U08 permission.
	Error error = registry.production_output_pin(context, request, rd, generation_pin);
	if (error != OK) {
		return error;
	}
	generation_registry = &registry;
	generation_request = request;
	generation_root_serial = generation_pin.serial;
	if (metadata) {
		error = registry.production_output_metadata_bind(*metadata, generation_pin);
	}
	if (error == OK) {
		error = registry.production_output_owner_bind(generation_pin, rd, particles, generation, UINT32_MAX, 0, GDExtensionDeviceExit::ProductionOutputOwnerKind::GENERATION);
	}
	if (error != OK) {
		// No native effect and no linked root was installed. Return only this
		// unused custody row, keeping its consumed serial non-reusable in Core.
		const Error returned = registry.production_output_unpin(generation_pin);
		CRASH_COND(returned != OK);
		generation_registry = nullptr;
		generation_request = {};
		generation_root_serial = 0;
	}
	return error;
}

Error ExternalOutputOwner::select_slot(const ExternalOutputRequest &request, uint32_t &out_view, uint32_t &out_slot) const {
	if (!admits_work()) {
		return ERR_UNAVAILABLE;
	}
	if (request.common.generation != generation) {
		return ERR_INVALID_PARAMETER;
	}
	const bool digests_match = config_tags_inline ? (request.layout_digest.size() == 32 && request.mapping_digest.size() == 32 &&
															memcmp(request.layout_digest.ptr(), frozen_config_tags[2], 32) == 0 && memcmp(request.mapping_digest.ptr(), frozen_config_tags[3], 32) == 0)
												  : (request.layout_digest == config.layout_digest && request.mapping_digest == config.mapping_digest);
	if (!digests_match || (request.pass_mask & ~config.pass_mask)) {
		return ERR_INVALID_DATA;
	}
	if (!next_lease_serial) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	uint32_t selected = UINT32_MAX, free_view = UINT32_MAX;
	for (uint32_t view = 0; view < config.max_views; ++view) {
		if (!views[view].occupied) {
			if (free_view == UINT32_MAX) {
				free_view = view;
			}
			continue;
		}
		if (views[view].tag == request.common.view_tag) {
			selected = view;
			if (views[view].closed) {
				return ERR_UNAVAILABLE;
			}
			if (views[view].epoch == request.view_epoch && request.common.present_seq <= views[view].last_present) {
				return ERR_INVALID_PARAMETER;
			}
			if (views[view].epoch != request.view_epoch) {
				// A lawful provider epoch transition is admitted only after all
				// old per-view native/source/history obligations really returned.
				// This preflight does not authenticate that provider transition.
				if (views[view].history_slot != UINT32_MAX) {
					return ERR_BUSY;
				}
				const uint32_t begin = view * config.max_inflight;
				for (uint32_t old = begin; old < begin + config.max_inflight; ++old) {
					if ((slots[old].state != LeaseState::EMPTY && slots[old].state != LeaseState::RELEASED) || slots[old].storage || slots[old].surfaces || !slots[old].original_pin.is_empty()) {
						return ERR_BUSY;
					}
				}
			}

			break;
		}
	}
	if (selected == UINT32_MAX) {
		selected = free_view;
	}
	if (selected == UINT32_MAX) {
		return ERR_BUSY;
	}
	uint32_t available = UINT32_MAX;
	const uint32_t begin = selected * config.max_inflight;
	for (uint32_t slot = begin; slot < begin + config.max_inflight; ++slot) {
		const auto &cell = slots[slot];
		if (cell.state == LeaseState::EMPTY || cell.state == LeaseState::RELEASED) {
			if (available == UINT32_MAX && views[selected].history_slot != slot) {
				available = slot;
			}
		} else if (cell.request.common.present_seq == request.common.present_seq || cell.request.common.output_tag == request.common.output_tag) {
			return ERR_ALREADY_IN_USE;
		}
	}
	if (available == UINT32_MAX) {
		return ERR_BUSY;
	}
	if (slots[available].storage || slots[available].surfaces || !slots[available].original_pin.is_empty()) {
		return ERR_INVALID_DATA;
	}
	out_view = selected;
	out_slot = available;
	return OK;
}

Error ExternalOutputOwner::plan_metadata_layout(uint32_t p_views, uint32_t p_reads, uint32_t p_surfaces, MetadataLayout &r_layout, uint32_t p_mesh_selections, uint32_t p_material_selections, uint32_t p_bindings, uint32_t p_storage, uint32_t p_preparations) {
	MetadataLayout layout;
	layout.view_count = p_views;
	layout.read_count = p_reads;
	layout.surface_count = p_surfaces;
	layout.mesh_selection_count = p_mesh_selections;
	layout.material_selection_count = p_material_selections;
	layout.binding_count = p_bindings == UINT32_MAX ? p_surfaces : p_bindings;
	layout.storage_count = p_storage == UINT32_MAX ? (p_surfaces ? MAX_SLOTS : 0) : p_storage;
	// Legacy component layouts retain one row per binding. Production quotes
	// the generation-wide finite bank independently from surface bindings.
	layout.preparation_count = p_preparations == UINT32_MAX ? layout.binding_count : p_preparations;
	// Retained upload bytes belong to the same paid metadata slab as each slot.
	// Layouts without storage slots have no guard producer and need no payload.
	const uint64_t proof_bytes = layout.storage_count ? uint64_t(layout.storage_count) * sizeof(ParticlesExternalGuard::GeometryProofHeader) + uint64_t(layout.binding_count) * sizeof(ParticlesExternalGuard::SurfaceProof) : 0;
	if (proof_bytes > UINT32_MAX) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	layout.guard_proof_bytes = uint32_t(proof_bytes);
	uint64_t cursor = sizeof(ExternalOutputOwner);
	uint32_t alignment = alignof(ExternalOutputOwner);
	auto append = [&](uint32_t count, size_t stride, size_t align, uint32_t &offset) {
		alignment = MAX(alignment, uint32_t(align));
		if (!count) {
			return true;
		}
		const uint64_t aligned = (cursor + align - 1) & ~(uint64_t(align) - 1);
		if (aligned > UINT32_MAX || count > (UINT32_MAX - aligned) / stride) {
			return false;
		}
		const uint64_t bytes = uint64_t(count) * stride;
		offset = uint32_t(aligned);
		cursor = aligned + bytes;
		return true;
	};
	if (cursor > UINT32_MAX ||
			!append(p_views, sizeof(ViewCell), alignof(ViewCell), layout.view_offset) ||
			!append(p_reads, sizeof(ReadCell), alignof(ReadCell), layout.read_offset) ||
			!append(layout.storage_count, sizeof(ExternalOutputStorage), alignof(ExternalOutputStorage), layout.storage_offset) ||
			!append(layout.binding_count, sizeof(SurfaceBinding), alignof(SurfaceBinding), layout.surface_binding_offset) ||
			!append(layout.preparation_count, sizeof(ExternalPipelinePreparation), alignof(ExternalPipelinePreparation), layout.preparation_offset) ||
			!append(p_mesh_selections, sizeof(GenerationMeshSelection), alignof(GenerationMeshSelection), layout.mesh_selection_offset) ||
			!append(p_material_selections, sizeof(GenerationMaterialSelection), alignof(GenerationMaterialSelection), layout.material_selection_offset) ||
			!append(p_surfaces, sizeof(SurfaceCell), alignof(SurfaceCell), layout.surface_offset) ||
			!append(layout.guard_proof_bytes, 1, alignof(ParticlesExternalGuard::GeometryProofHeader), layout.guard_proof_offset) ||
			alignment > alignof(std::max_align_t)) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	layout.total_bytes = uint32_t(cursor);
	layout.alignment = alignment;
	r_layout = layout;
	return OK;
}

Error ExternalOutputOwner::plan_generation_selection_layout(const ExternalOutputConfig &config, uint32_t views, uint32_t reads, uint32_t surface_consumers, MetadataLayout &out) {
	if (config.profile != ExternalOutputConfig::INSTANCE_192) {
		return ERR_UNAVAILABLE;
	}
	uint32_t flattened = 0;
	const Error error = validate_native_selection(config, flattened);
	if (error != OK) {
		return error;
	}
	if (!config.max_views || config.max_views > MAX_VIEWS || !config.max_inflight || config.max_inflight > MAX_INFLIGHT) {
		return ERR_INVALID_PARAMETER;
	}
	const uint64_t slots = uint64_t(config.max_views) * config.max_inflight;
	const uint64_t bindings = slots * flattened;
	if (bindings > UINT32_MAX) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	return plan_metadata_layout(views, reads, surface_consumers, out, config.surfaces.size(), flattened, uint32_t(bindings), uint32_t(slots), reads);
}

Error ExternalOutputOwner::install_metadata_directory(GDExtensionDeviceExit::ProductionPhysicalContextRef context,
		GDExtensionProductionRequestRefV1 request, GDExtensionDeviceExit::Registry &registry,
		const GDExtensionDeviceExit::ProductionOutputMetadataLease &lease,
		uint32_t p_views, uint32_t p_reads, uint32_t p_surfaces, bool p_generation_selection) {
	if (metadata_directory_installed || view_cells || read_cells || surface_cells || preparation_rows || view_capacity || read_capacity || surface_capacity || preparation_capacity) {
		return ERR_ALREADY_IN_USE;
	}
	if (state != GenerationState::CONFIGURED || has_live_slots()) {
		return ERR_BUSY;
	}
	if (!device || lease.is_empty() || lease.get_data() != this ||
			(!metadata_lease.is_empty() && (metadata_lease.get_data() != lease.get_data() || metadata_lease.get_size() != lease.get_size()))) {
		return ERR_UNAUTHORIZED;
	}
	if (generation_pin.is_empty()) {
		if (generation_registry || generation_request.registry_instance || generation_request.handle_serial || generation_root_serial) {
			return ERR_UNAUTHORIZED;
		}
	} else if (generation_registry != &registry || generation_request.registry_instance != request.registry_instance ||
			generation_request.handle_serial != request.handle_serial || !generation_root_matches(generation_pin)) {
		return ERR_UNAUTHORIZED;
	}
	uint32_t mesh_selections = 0, material_selections = 0;
	if (p_generation_selection) {
		if (config.profile != ExternalOutputConfig::INSTANCE_192 || !config_tags_inline) {
			return ERR_UNAVAILABLE;
		}
		const Error selection = validate_native_selection(config, material_selections);
		if (selection != OK) {
			return selection;
		}
		mesh_selections = config.surfaces.size();
	}
	MetadataLayout layout;
	Error error = p_generation_selection ? plan_generation_selection_layout(config, p_views, p_reads, p_surfaces, layout) : plan_metadata_layout(p_views, p_reads, p_surfaces, layout);
	if (error != OK) {
		return error;
	}
	error = registry.production_output_metadata_validate(context, request, lease, *device, layout.total_bytes, layout.alignment);
	if (error != OK) {
		return error;
	}
	auto *base = static_cast<uint8_t *>(lease.get_data());
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
	for (uint32_t i = 0; i < mesh_selections; ++i) {
		auto *row = new (base + layout.mesh_selection_offset + size_t(i) * sizeof(GenerationMeshSelection)) GenerationMeshSelection;
		row->mesh = config.surfaces[i];
	}
	for (uint32_t i = 0; i < material_selections; ++i) {
		auto *row = new (base + layout.material_selection_offset + size_t(i) * sizeof(GenerationMaterialSelection)) GenerationMaterialSelection;
		row->material = config.surface_materials[i];
		row->generation = MaterialStorage::get_singleton()->material_get_generation(row->material);
	}
	generation_meshes = mesh_selections ? reinterpret_cast<GenerationMeshSelection *>(base + layout.mesh_selection_offset) : nullptr;
	generation_materials = material_selections ? reinterpret_cast<GenerationMaterialSelection *>(base + layout.material_selection_offset) : nullptr;
	generation_mesh_count = mesh_selections;
	generation_material_count = material_selections;
	binding_capacity = layout.binding_count;
	preparation_rows = layout.preparation_count ? reinterpret_cast<ExternalPipelinePreparation *>(base + layout.preparation_offset) : nullptr;
	preparation_capacity = layout.preparation_count;
	storage_capacity = layout.storage_count;
	if (p_generation_selection) {
		// Identity resides in quoted rows; do not retain parser Vector storage.
		config.surfaces.clear();
		config.surface_materials.clear();
		generation_config_inline = true;
	}
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

bool ExternalOutputOwner::preparation_source_matches(const ExternalPipelinePreparation &row) const {
	const bool selected = row.tuple_state != ExternalPipelinePreparation::TupleState::UNSELECTED;
	const uint32_t source_index = selected ? row.tuple_surface : row.owner_surface;
	if (source_index == UINT32_MAX) {
		return !row.source_geometry && row.source_material.is_null() && !row.source_material_generation;
	}
	if (selected) {
		if (!generation_selection_complete || source_index >= generation_material_count) {
			return false;
		}
		const auto &source = generation_materials[source_index];
		return row.tuple_mesh == source.mesh && row.tuple_native_surface == source.native_surface && row.source_material == source.material && row.source_material_generation == source.generation &&
				row.source_geometry == (generation_surfaces_complete ? &source.geometry_source : nullptr);
	}
	if (generation_surfaces_complete) {
		if (source_index >= retained_surface_count || retained_surface_count != generation_material_count) {
			return false;
		}
		const auto &source = generation_materials[source_index];
		return row.source_geometry == &source.geometry_source && row.source_material == source.material && row.source_material_generation == source.generation;
	}
	return !row.source_geometry && row.source_material.is_null() && !row.source_material_generation;
}

Error ExternalOutputOwner::material_future_pipeline_plan(const ExternalPipelinePreparation &row, RID p_shader, RID p_uniform_set, MaterialStorage::NativeMaterialFuturePipelinePlan &r_plan) const {
	if (!device || device != RenderingDevice::get_singleton() || !device->native_immutable_is_render_thread() || row.owner != this || row.owner_slot >= slot_count) {
		return ERR_UNAUTHORIZED;
	}
	const auto &slot = slots[row.owner_slot];
	bool bank_row = false;
	for (uint32_t i = 0; i < preparation_capacity; ++i) {
		if (preparation_rows && &preparation_rows[i] == &row) {
			bank_row = preparation_bank_row_matches(row.owner_slot, i, row);
			break;
		}
	}
	if (!bank_row || !slot.original_registry || !row.pipeline_scope || row.source_sealed || row.pipeline_pin != &slot.original_pin ||
			row.pipeline_original_root != &row.owned_pipeline_root || !row.max_resources || row.finish_complete || row.pipeline_sealed ||
			row.geometry_adopted || row.material_adopted || row.native_cache_queried || row.created_pipeline.is_valid() || row.owned_pipeline_root.is_valid() ||
			row.geometry[0].is_active() || row.geometry[1].is_active() || row.material.is_active()) {
		return ERR_UNAUTHORIZED;
	}
	// Never hold Registry across the material/RD observation. The synchronous
	// original scope retains the row; NewWork is rechecked before publication.
	auto authenticate = [&]() -> Error {
		GDExtensionDeviceExit::Registry::Lock lock(*slot.original_registry);
		GDExtensionDeviceExit::ProductionRequest *request = nullptr;
		const Error error = slot.original_registry->production_output_preparation_matches_locked(*row.pipeline_scope, *device, request);
		return error != OK ? error : (request == slot.original_pin.request ? OK : ERR_UNAUTHORIZED);
	};
	const Error admitted = authenticate();
	if (admitted != OK) {
		return admitted;
	}
	if (config.profile != ExternalOutputConfig::INSTANCE_192 || !generation_selection_complete || !generation_surfaces_complete ||
			retained_mesh_count != generation_mesh_count || retained_material_count != generation_material_count || retained_surface_count != generation_material_count || !generation_materials ||
			row.tuple_state != ExternalPipelinePreparation::TupleState::SELECTED || row.tuple_surface >= generation_material_count || !preparation_source_matches(row)) {
		return ERR_UNAVAILABLE;
	}
	const auto &source = generation_materials[row.tuple_surface];
	if (!source.lease.is_active() || !source.geometry_source.is_valid() || row.source_geometry != &source.geometry_source) {
		return ERR_UNAVAILABLE;
	}
	auto *materials = MaterialStorage::get_singleton();
	if (!materials) {
		return ERR_UNAVAILABLE;
	}
	MaterialStorage::NativeMaterialFuturePipelinePlan plan;
	const Error planned = materials->material_selection_get_future_pipeline_hold_plan(source.lease, p_shader, p_uniform_set, plan, row.max_resources);
	if (planned != OK) {
		return planned;
	}
	const Error current = authenticate();
	if (current != OK) {
		return current;
	}
	if (!preparation_source_matches(row) || row.source_geometry != &source.geometry_source || !source.lease.is_active() || !source.geometry_source.is_valid()) {
		return ERR_UNAUTHORIZED;
	}
	r_plan = std::move(plan);
	return OK;
}

bool ExternalOutputOwner::preparation_view_matches(const ExternalPacketViewLease &view, uint32_t p_slot, bool cleanup) const {
	if (view.owner != this || !view.serial || view.view_cell >= view_capacity || view.slot != p_slot || p_slot >= slot_count || !view_cells) {
		return false;
	}
	const auto &cell = view_cells[view.view_cell];
	const auto &slot = slots[p_slot];
	if (cell.consumer != &view || cell.serial != view.serial || cell.slot != p_slot || cell.pin.is_empty() || !slot.original_registry || !device) {
		return false;
	}
	GDExtensionDeviceExit::Registry::Lock lock(*slot.original_registry, GDExtensionDeviceExit::Registry::Lock::BLOCK_OR_BORROW);
	if (!slot.original_registry->production_output_pin_matches_locked(cell.pin, slot.original_request, *device)) {
		return false;
	}
	if (cell.camera_source.is_valid() && (slot.native_camera_source != cell.camera_source ||
			cell.camera_source->get_token() != slot.request.common.current_camera_token)) {
		return false;
	}
	return cleanup || (admits_work() && !slot.pass_uses_closed && (slot.state == LeaseState::SEALED_RECORDED || slot.state == LeaseState::ADOPTED));
}

Error ExternalOutputOwner::select_preparation_tuple(uint32_t p_slot, uint32_t p_surface, const ExternalPacketPipelineRequest &selection, uint32_t &out_index, bool &out_reused) {
	if (has_inflight_preparation()) {
		return ERR_BUSY;
	}
	if (!admits_work() || p_slot >= slot_count || !generation_selection_complete || p_surface >= generation_material_count) {
		return ERR_UNAVAILABLE;
	}
	auto &slot = slots[p_slot];
	if (slot.original_pin.is_empty() || slot.pass_uses_closed || slot.state == LeaseState::RETIRING || !slot.preparation_reserved_count) {
		return ERR_UNAUTHORIZED;
	}
	if (config.profile == ExternalOutputConfig::INSTANCE_192) {
		// Renderer categories and wire bits are distinct. An unrequested pass
		// cannot occupy this exact slot's finite preparation bank.
		uint32_t pass = 0;
		switch (selection.pass) {
			case ExternalPacketPass::OPAQUE:
				pass = ExternalOutputConfig::PASS_OPAQUE;
				break;
			case ExternalPacketPass::TRANSPARENT:
				pass = ExternalOutputConfig::PASS_TRANSPARENT;
				break;
			case ExternalPacketPass::DEPTH:
				pass = ExternalOutputConfig::PASS_DEPTH;
				break;
			case ExternalPacketPass::SHADOW:
				pass = ExternalOutputConfig::PASS_SHADOW;
				break;
			case ExternalPacketPass::PROBE:
				pass = ExternalOutputConfig::PASS_PROBE;
				break;
			case ExternalPacketPass::MOTION:
				break;
		}
		if (!(config.pass_mask & slot.request.pass_mask & pass)) {
			return ERR_UNAVAILABLE;
		}
	}
	const auto &source = generation_materials[p_surface];
	uint32_t available = UINT32_MAX;
	for (uint32_t i = 0; i < preparation_capacity; ++i) {
		auto &row = preparation_rows[i];
		if (!preparation_bank_row_matches(p_slot, i, row)) {
			continue;
		}
		if (row.tuple_state != ExternalPipelinePreparation::TupleState::UNSELECTED) {
			if (row.tuple_surface == p_surface && row.tuple_mesh == source.mesh && row.tuple_native_surface == source.native_surface &&
					row.source_material == source.material && row.source_material_generation == source.generation && row.selection == selection && preparation_source_matches(row)) {
				out_index = i;
				out_reused = true;
				return OK;
			}
		} else if (available == UINT32_MAX && !row.finish_complete && !row.geometry_adopted && !row.material_adopted && !row.pipeline_sealed && row.bank_groups_reserved && !row.pipeline_scope && row.source_sealed && row.owned_pipeline_root.is_null() &&
				!row.native_cache_queried && !row.geometry[0].is_active() && !row.geometry[1].is_active() && !row.material.is_active()) {
			available = i;
		}
	}
	if (available == UINT32_MAX || slot.preparation_selected_count >= slot.preparation_reserved_count) {
		return ERR_OUT_OF_MEMORY;
	}
	auto &row = preparation_rows[available];
	row.selection = selection;
	row.tuple_surface = p_surface;
	row.tuple_native_surface = source.native_surface;
	row.tuple_mesh = source.mesh;
	row.source_geometry = generation_surfaces_complete ? &source.geometry_source : nullptr;
	row.source_material = source.material;
	row.source_material_generation = source.generation;
	row.tuple_state = ExternalPipelinePreparation::TupleState::SELECTED;
	++slot.preparation_selected_count; // One prospective original P credit, including culled/failed prefixes.
	out_index = available;
	out_reused = false;
	return OK;
}

bool ExternalOutputOwner::preparation_bank_row_matches(uint32_t p_slot, uint32_t p_index, const ExternalPipelinePreparation &p_row) const {
	return p_slot < slot_count && preparation_rows && p_index < preparation_capacity && &p_row == &preparation_rows[p_index] &&
			p_row.owner == this && p_row.owner_slot == p_slot && p_row.generation == generation && slots[p_slot].lease_serial && p_row.serial == slots[p_slot].lease_serial;
}

uint32_t ExternalOutputOwner::free_preparation_rows() const {
	uint32_t count = 0;
	for (uint32_t i = 0; preparation_rows && i < preparation_capacity; ++i) {
		const auto &row = preparation_rows[i];
		count += !row.owner && row.owner_slot == UINT32_MAX && row.owner_surface == UINT32_MAX && !row.serial && !row.generation && row.source_sealed && !row.max_resources &&
				!row.source_geometry && row.source_material.is_null() && !row.source_material_generation &&
				row.tuple_state == ExternalPipelinePreparation::TupleState::UNSELECTED && row.tuple_surface == UINT32_MAX && row.tuple_native_surface == UINT32_MAX && row.tuple_mesh.is_null() &&
				!row.finish_complete && !row.geometry_adopted && !row.material_adopted && !row.pipeline_sealed && row.backend_error == ERR_UNCONFIGURED && row.finish_error == ERR_UNCONFIGURED && row.selection == ExternalPacketPipelineRequest() &&
				!row.geometry_transferred[0] && !row.geometry_transferred[1] && !row.material_transferred && !row.geometry_metadata_returned[0] && !row.geometry_metadata_returned[1] && !row.material_metadata_returned &&
				!row.pipeline_pin && !row.pipeline_original_root && !row.pipeline_owned_group && !row.geometry_reserved_group && !row.material_reserved_group && !row.bank_groups_reserved && row.owned_pipeline_root.is_null() && !row.pipeline_scope &&
				!row.pipeline_context.registry_instance && !row.pipeline_context.frame_serial && !row.native_cache_queried &&
				row.created_pipeline.is_null() && !row.geometry[0].is_active() && !row.geometry[1].is_active() && !row.material.is_active();
	}
	return count;
}

bool ExternalOutputOwner::preparation_row_matches(uint32_t p_slot, uint32_t p_surface, const ExternalPipelinePreparation &p_row) const {
	if (p_slot >= slot_count || !slots[p_slot].surfaces || p_surface >= slots[p_slot].surface_count || !preparation_rows) {
		return false;
	}
	const auto &slot = slots[p_slot];
	const auto &surface = slot.surfaces[p_surface];
	if (surface.preparation_index >= preparation_capacity || surface.preparation_row != &preparation_rows[surface.preparation_index] ||
			&p_row != surface.preparation_row || p_row.owner != this || p_row.owner_slot != p_slot || p_row.owner_surface != p_surface ||
			p_row.generation != generation || !slot.lease_serial || p_row.serial != slot.lease_serial) {
		return false;
	}
	return preparation_source_matches(p_row);
}

Error ExternalOutputOwner::bind_preparation_rows(uint32_t p_slot, uint32_t p_count) {
	if (has_inflight_preparation()) {
		return ERR_BUSY;
	}
	if (p_slot >= slot_count || !metadata_directories_placement || !preparation_rows) {
		return ERR_INVALID_PARAMETER;
	}
	auto &slot = slots[p_slot];
	const uint32_t count = p_count ? p_count : slot.surface_count;
	if (count < slot.surface_count) {
		return ERR_OUT_OF_MEMORY;
	}
	if (slot.preparation_reserved_count) {
		return ERR_ALREADY_IN_USE;
	}
	if (slot.state != LeaseState::RESERVED || slot.original_pin.is_empty() || !slot.lease_serial ||
			!slot.surfaces || !slot.surfaces_placement || !slot.surface_count || slot.publication_closed) {
		return ERR_UNAUTHORIZED;
	}
	for (uint32_t i = 0; i < slot.surface_count; ++i) {
		const auto &surface = slot.surfaces[i];
		if (surface.preparation_row || surface.preparation || surface.preparation_index != UINT32_MAX) {
			return ERR_ALREADY_IN_USE;
		}
	}
	// Count the complete free set first. Fragmentation is legal; insufficient
	// capacity must leave every surface locator and bank row unchanged.
	auto free_row = [](const ExternalPipelinePreparation &row) {
		return !row.owner && row.owner_slot == UINT32_MAX && row.owner_surface == UINT32_MAX && !row.serial && !row.generation && row.source_sealed && !row.max_resources &&
				!row.source_geometry && row.source_material.is_null() && !row.source_material_generation &&
				row.tuple_state == ExternalPipelinePreparation::TupleState::UNSELECTED && row.tuple_surface == UINT32_MAX && row.tuple_native_surface == UINT32_MAX && row.tuple_mesh.is_null() &&
				!row.finish_complete && !row.geometry_adopted && !row.material_adopted && !row.pipeline_sealed && row.backend_error == ERR_UNCONFIGURED && row.finish_error == ERR_UNCONFIGURED && row.selection == ExternalPacketPipelineRequest() &&
				!row.geometry_transferred[0] && !row.geometry_transferred[1] && !row.material_transferred && !row.geometry_metadata_returned[0] && !row.geometry_metadata_returned[1] && !row.material_metadata_returned &&
				!row.pipeline_pin && !row.pipeline_original_root && !row.pipeline_owned_group && !row.geometry_reserved_group && !row.material_reserved_group && !row.bank_groups_reserved && row.owned_pipeline_root.is_null() && !row.pipeline_scope &&
				!row.pipeline_context.registry_instance && !row.pipeline_context.frame_serial && !row.native_cache_queried &&
				row.created_pipeline.is_null() && !row.geometry[0].is_active() && !row.geometry[1].is_active() && !row.material.is_active();
	};
	uint32_t available = 0;
	for (uint32_t i = 0; i < preparation_capacity; ++i) {
		available += free_row(preparation_rows[i]);
	}
	if (available < count) {
		return ERR_OUT_OF_MEMORY;
	}
	if (generation_surfaces_complete && (retained_surface_count != generation_material_count || slot.surface_count > retained_surface_count)) {
		return ERR_INVALID_DATA;
	}
	// No callbacks, native effects or allocation occur between preflight and
	// association. Existing owner serialization protects this complete claim.
	slot.retire_preparation_index = 0;
	slot.preparation_selected_count = 0;
	uint32_t next = 0;
	for (uint32_t i = 0; i < count; ++i) {
		while (next < preparation_capacity && !free_row(preparation_rows[next])) {
			++next;
		}
		CRASH_COND(next == preparation_capacity);
		auto &row = preparation_rows[next];
		row.owner = this;
		row.owner_slot = p_slot;
		row.owner_surface = i < slot.surface_count ? i : UINT32_MAX;
		row.generation = generation;
		row.serial = slot.lease_serial;
		if (generation_surfaces_complete && i < slot.surface_count) {
			const auto &source = generation_materials[i];
			row.source_geometry = &source.geometry_source;
			row.source_material = source.material;
			row.source_material_generation = source.generation;
		}
		if (i < slot.surface_count) {
			slot.surfaces[i].preparation_index = next;
			slot.surfaces[i].preparation_row = &row;
		}
		++next;
		++slot.preparation_reserved_count;
	}
	return OK;
}

Error ExternalOutputOwner::return_preparation_bank_row(uint32_t p_slot, uint32_t p_index) {
	if (has_inflight_preparation()) {
		return ERR_BUSY;
	}
	if (p_slot >= slot_count || !preparation_rows || p_index >= preparation_capacity) {
		return ERR_INVALID_PARAMETER;
	}
	auto &slot = slots[p_slot];
	auto &row = preparation_rows[p_index];
	if (!preparation_bank_row_matches(p_slot, p_index, row) || !slot.preparation_reserved_count) {
		return ERR_INVALID_DATA;
	}
	SurfaceBinding *surface = nullptr;
	if (row.owner_surface != UINT32_MAX) {
		if (!preparation_row_matches(p_slot, row.owner_surface, row)) {
			return ERR_INVALID_DATA;
		}
		surface = &slot.surfaces[row.owner_surface];
	}
	if ((surface && surface->preparation) || !row.source_sealed || row.pipeline_scope || row.pipeline_pin || row.pipeline_original_root || row.pipeline_owned_group ||
			row.geometry_reserved_group || row.material_reserved_group || row.transforms_owned_group || row.transforms_uniform_set.is_valid() || row.owned_pipeline_root.is_valid() ||
			row.created_pipeline.is_valid() || row.geometry[0].is_active() || row.geometry[1].is_active() || row.material.is_active()) {
		return ERR_BUSY;
	}
	if (row.tuple_state != ExternalPipelinePreparation::TupleState::UNSELECTED) {
		if (!slot.preparation_selected_count) {
			return ERR_INVALID_DATA;
		}
		--slot.preparation_selected_count;
	}
	row.~ExternalPipelinePreparation();
	new (&row) ExternalPipelinePreparation;
	if (surface) {
		surface->preparation_row = nullptr;
		surface->preparation_index = UINT32_MAX;
	}
	--slot.preparation_reserved_count;
	return OK;
}

Error ExternalOutputOwner::return_preparation_row(uint32_t p_slot, uint32_t p_surface) {
	if (has_inflight_preparation()) {
		return ERR_BUSY;
	}
	if (p_slot >= slot_count || !slots[p_slot].surfaces || p_surface >= slots[p_slot].surface_count) {
		return ERR_INVALID_PARAMETER;
	}
	auto &surface = slots[p_slot].surfaces[p_surface];
	if (!surface.preparation_row) {
		return surface.preparation || surface.preparation_index != UINT32_MAX ? ERR_INVALID_DATA : OK;
	}
	if (!preparation_row_matches(p_slot, p_surface, *surface.preparation_row)) {
		return ERR_INVALID_DATA;
	}
	return return_preparation_bank_row(p_slot, surface.preparation_index);
}

Error ExternalOutputOwner::install_reserved_cells(uint32_t p_views, uint32_t p_reads) {
	if (metadata_directory_installed || !metadata_lease.is_empty()) {
		return ERR_ALREADY_IN_USE;
	}
	if (view_cells || read_cells || view_capacity || read_capacity || has_live_slots()) {
		return ERR_ALREADY_IN_USE;
	}
	if (!p_views || !p_reads || state != GenerationState::CONFIGURED) {
		return ERR_INVALID_PARAMETER;
	}
	// Counts are the original caller's already admitted metadata capacities.
	// This allocation does not charge or enlarge that original parent account.
	view_cells = memnew_arr(ViewCell, p_views);
	read_cells = memnew_arr(ReadCell, p_reads);
	view_capacity = p_views;
	read_capacity = p_reads;
	return OK;
}

Error ExternalOutputOwner::install_reserved_surface_cells(uint32_t p_surfaces) {
	if (metadata_directory_installed || !metadata_lease.is_empty()) {
		return ERR_ALREADY_IN_USE;
	}
	if (surface_cells || surface_capacity || has_live_slots()) {
		return ERR_ALREADY_IN_USE;
	}
	if (!p_surfaces || state != GenerationState::CONFIGURED) {
		return ERR_INVALID_PARAMETER;
	}
	// Original caller has already charged this exact finite metadata table.
	surface_cells = memnew_arr(SurfaceCell, p_surfaces);
	surface_capacity = p_surfaces;
	return OK;
}

Error ExternalOutputOwner::return_reserved_cells() {
	if (has_inflight_preparation() || has_live_slots() || has_generation_selection_custody()) {
		return ERR_BUSY;
	}
	for (uint32_t i = 0; i < view_capacity; ++i) {
		if (view_cells[i].consumer || !view_cells[i].pin.is_empty() || view_cells[i].camera_source.is_valid()) {
			return ERR_BUSY;
		}
	}
	for (uint32_t i = 0; i < read_capacity; ++i) {
		const auto &cell = read_cells[i];
		if (cell.consumer || !cell.pin.is_empty() || cell.camera_source.is_valid() || cell.camera_retained || cell.list_linked ||
				(cell.state != ReadCell::State::UNUSED && cell.state != ReadCell::State::RETURNED)) {
			return ERR_BUSY;
		}
	}
	for (uint32_t i = 0; i < surface_capacity; ++i) {
		if (surface_cells[i].consumer || !surface_cells[i].pin.is_empty()) {
			return ERR_BUSY;
		}
	}
	for (uint32_t i = 0; i < preparation_capacity; ++i) {
		const auto &row = preparation_rows[i];
		if (row.owner || row.owner_slot != UINT32_MAX || row.owner_surface != UINT32_MAX || row.serial || row.generation || !row.source_sealed || row.pipeline_scope || row.pipeline_pin || row.pipeline_original_root || row.pipeline_owned_group || row.geometry_reserved_group || row.material_reserved_group || row.transforms_owned_group || row.transforms_uniform_set.is_valid() || row.owned_pipeline_root.is_valid() ||
				row.created_pipeline.is_valid() || row.geometry[0].is_active() || row.geometry[1].is_active() || row.material.is_active()) {
			return ERR_BUSY;
		}
	}
	if (metadata_directories_placement) {
		for (uint32_t i = 0; i < preparation_capacity; ++i) {
			preparation_rows[i].~ExternalPipelinePreparation();
		}
		preparation_rows = nullptr;
		preparation_capacity = 0;
		for (uint32_t i = 0; i < generation_material_count; ++i) {
			generation_materials[i].~GenerationMaterialSelection();
		}
		for (uint32_t i = 0; i < generation_mesh_count; ++i) {
			generation_meshes[i].~GenerationMeshSelection();
		}
		generation_meshes = nullptr;
		generation_materials = nullptr;
		generation_mesh_count = generation_material_count = 0;
		for (uint32_t i = 0; i < surface_capacity; ++i) {
			surface_cells[i].~SurfaceCell();
		}
		for (uint32_t i = 0; i < view_capacity; ++i) {
			view_cells[i].~ViewCell();
		}
		for (uint32_t i = 0; i < read_capacity; ++i) {
			read_cells[i].~ReadCell();
		}
	} else if (surface_cells) {
		memdelete_arr(surface_cells);
	}
	surface_cells = nullptr;
	surface_capacity = 0;
	if (!metadata_directories_placement && view_cells) {
		memdelete_arr(view_cells);
	}
	if (!metadata_directories_placement && read_cells) {
		memdelete_arr(read_cells);
	}
	view_cells = nullptr;
	read_cells = nullptr;
	view_capacity = read_capacity = 0;
	metadata_directories_placement = false;
	return OK;
}

// Private commit only. Registry reaches this after authenticating the original
// Records/Meta writes and actual guard append; tokens and pins alone cannot
// call this from the public/facade path or stand in for that record proof.
Error ExternalOutputOwner::seal_recorded_slot(uint32_t p_slot, const Ref<RendererViewport::Viewport::CameraCapture> &p_source,
		const GDExtensionOutputTokenV1 &p_record_token, const GDExtensionOutputTokenV1 &p_packet_token) {
	if (!device || device != RenderingDevice::get_singleton() || !device->native_immutable_is_render_thread()) {
		return ERR_UNAUTHORIZED;
	}
	if (p_slot >= slot_count || p_source.is_null()) {
		return ERR_INVALID_PARAMETER;
	}
	auto &slot = slots[p_slot];
	if (!admits_work() || slot.publication_closed || slot.pass_uses_closed) {
		return ERR_UNAVAILABLE;
	}
	if (!slot.original_registry || !slot.original_request.registry_instance || !slot.original_request.handle_serial ||
			slot.original_pin.is_empty() || !slot.lease_serial || slot.original_action_ordinal == UINT64_MAX || slot.lease_token.size() != 32 ||
			slot.request.common.generation != generation || !slot.retained_host_snapshot ||
			!slot.current_host.is_finite() || !slot.previous_host.is_finite()) {
		return ERR_INVALID_DATA;
	}
	const auto &token = p_source->get_token();
	const auto &key = p_source->get_presentation_key();
	const auto &invocation = p_source->get_invocation();
	const auto &camera = p_source->get_camera_data();
	if (p_source->get_viewport().is_null() || token != slot.request.common.current_camera_token ||
			!token.view_id || token.view_id != key.view_id || token.view_epoch != key.view_epoch ||
			key.view_epoch != slot.request.view_epoch || key.present_seq != slot.request.common.present_seq ||
			camera.source_viewport != p_source->get_viewport() || camera.source_view_id != token.view_id ||
			camera.source_view_epoch != token.view_epoch || camera.source_capture_ordinal != token.capture_ordinal ||
			invocation.camera.is_null() || invocation.render_buffers.is_null() ||
			(slot.native_camera_source.is_valid() && slot.native_camera_source != p_source)) {
		return ERR_INVALID_DATA;
	}
	if (invocation.use_xr || invocation.view_count != 1) {
		return ERR_UNAVAILABLE;
	}
	bool has_record = false, has_packet = false;
	for (uint32_t i = 0; i < 32; ++i) {
		has_record |= p_record_token.bytes[i] != 0;
		has_packet |= p_packet_token.bytes[i] != 0;
	}
	if (!has_record || !has_packet) {
		return ERR_INVALID_DATA;
	}
	if (slot.state == LeaseState::SEALED_RECORDED || slot.state == LeaseState::ADOPTED) {
		return slot.native_camera_source == p_source && slot.record_token.size() == 32 && slot.packet_token.size() == 32 &&
					memcmp(slot.record_token.ptr(), p_record_token.bytes, 32) == 0 && memcmp(slot.packet_token.ptr(), p_packet_token.bytes, 32) == 0 ? OK : ERR_INVALID_DATA;
	}
	if (slot.state != LeaseState::RESERVED && slot.state != LeaseState::WRITING) {
		return ERR_INVALID_DATA;
	}
	// Allocate both bounded opaque correlations before changing observable state.
	// Their issuer is the original Registry record proof, never this commit.
	PackedByteArray record, packet;
	Error error = record.resize(32);
	if (error == OK) {
		error = packet.resize(32);
	}
	if (error != OK) {
		return error;
	}
	memcpy(record.ptrw(), p_record_token.bytes, 32);
	memcpy(packet.ptrw(), p_packet_token.bytes, 32);
	slot.record_token = record;
	slot.packet_token = packet;
	slot.native_camera_source = p_source;
	slot.state = LeaseState::SEALED_RECORDED;
	return OK;
}

Error ExternalOutputOwner::reserve_slot(const ExternalOutputRequest &,
		GDExtensionDeviceExit::Registry &, GDExtensionProductionRequestRefV1,
		const GDExtensionDeviceExit::ProductionOutputPin &, RenderingDevice &, uint32_t &) {
	// The original typed source/mapping/Host issuer join has not been provided.
	// A metadata pin is not that join; do not borrow it or commit any slot here.
	return ERR_UNAVAILABLE;
}

Error ExternalOutputOwner::cancel_slot(uint32_t p_slot) {
	if (p_slot >= slot_count) {
		return ERR_INVALID_PARAMETER;
	}
	auto &cell = slots[p_slot];
	if (cell.state == LeaseState::EMPTY || cell.state == LeaseState::RELEASED) {
		return ERR_DOES_NOT_EXIST;
	}
	cell.publication_closed = true;
	cell.pass_uses_closed = true;
	cell.state = LeaseState::RETIRING;
	return OK;
}

bool ExternalOutputOwner::slot_has_custody(uint32_t p_slot) const {
	if (p_slot >= slot_count) {
		return true;
	}
	for (uint32_t i = 0; i < surface_capacity; ++i) {
		const auto &cell = surface_cells[i];
		if (cell.slot == p_slot && (cell.consumer || !cell.pin.is_empty())) {
			return true;
		}
	}
	for (uint32_t i = 0; i < view_capacity; ++i) {
		const auto &cell = view_cells[i];
		if (cell.slot == p_slot && (cell.consumer || !cell.pin.is_empty())) {
			return true;
		}
	}
	for (uint32_t i = 0; i < read_capacity; ++i) {
		const auto &cell = read_cells[i];
		if ((cell.slot == p_slot || cell.previous_slot == p_slot) && (cell.consumer || !cell.pin.is_empty() || !cell.previous_pin.is_empty() || cell.camera_retained || cell.previous_camera_source.is_valid() || (cell.state != ReadCell::State::UNUSED && cell.state != ReadCell::State::RETURNED))) {
			return true;
		}
	}
	return false;
}

bool ExternalOutputOwner::native_color_consumption_matches(uint32_t p_slot, const GDExtensionOutputTokenV1 &p_token) const {
	if (p_slot >= slot_count) {
		return false;
	}
	const auto &slot = slots[p_slot];
	return slot.native_color_consumption_serial && slot.native_color_read_serial &&
			slot.native_color_empty == (config.profile == ExternalOutputConfig::INSTANCE_192 && config.capacity == 0) &&
			slot.native_color_read_cell < read_capacity && slot.native_color_close_graph != UINT32_MAX &&
			(slot.state == LeaseState::ADOPTED || slot.state == LeaseState::RETIRING) &&
			!slot.original_pin.is_empty() && slot.original_registry && slot.original_request.registry_instance &&
			slot.original_request.handle_serial && slot.native_camera_source.is_valid() && slot.retained_host_snapshot &&
			memcmp(slot.native_color_consumption_token.bytes, p_token.bytes, 32) == 0;
}

Error ExternalOutputOwner::return_slot_pin(uint32_t p_slot) {
	if (p_slot >= slot_count) {
		return ERR_INVALID_PARAMETER;
	}
	auto &cell = slots[p_slot];
	if (cell.state != LeaseState::RETIRING || !cell.publication_closed || !cell.pass_uses_closed ||
			cell.guard_owned_group || cell.storage || cell.surfaces || cell.surface_count || cell.preparation_reserved_count || cell.preparation_selected_count || cell.owned_group || slot_has_custody(p_slot)) {
		return ERR_BUSY;
	}
	for (uint32_t i = 0; i < config.max_views; ++i) {
		if (views[i].history_slot == p_slot) {
			return ERR_BUSY;
		}
	}
	if (!cell.original_registry || cell.original_pin.is_empty()) {
		return ERR_INVALID_DATA;
	}
	// Registry refuses return while any associated disposal group is pending.
	// No output/source/native completion is synthesized by this metadata return.
	const Error error = cell.original_registry->production_output_unpin(cell.original_pin);
	if (error != OK) {
		return error;
	}
	cell.original_registry = nullptr;
	// Keep exact cold incarnation identity for Poll after root return. These
	// scalars confer no authority; a reused slot installs a new original tuple.
	cell.original_revision = 0;
	cell.output_group_returned = false;
	cell.output_membership_group = 0;
	cell.retire_surface = cell.retire_resource = 0;
	cell.retained_host_snapshot = false;
	cell.consumed_proof_requested = false;
	cell.consumed_borrow_serial = 0;
	cell.native_camera_source.unref();
	cell.guard_proof_payload = nullptr;
	cell.guard_proof_bytes = 0;
	cell.guard_resources_ready = false;
	cell.request = {};
	cell.lease_token = {};
	cell.record_token = {};
	cell.packet_token = {};
	// Actual original retirement returned the pin after history and native uses.
	// Dropping a short read wrapper or cancellation never clears this receipt.
	cell.native_color_consumption_token = {};
	cell.native_color_consumption_particles = RID();
	cell.native_color_consumption_serial = cell.native_color_read_serial = 0;
	cell.native_color_empty = false;
	cell.native_color_read_cell = cell.native_color_close_graph = UINT32_MAX;
	cell.state = LeaseState::RELEASED;
	return OK;
}

Error ExternalOutputOwner::view_acquire(uint32_t, ExternalPacketViewLease &) {
	// Actual camera -> sealed slot/consumed tuple issuer join is still absent.
	// Never select a packet from a slot number or its copied tokens alone.
	return ERR_UNAVAILABLE;
}

Error ExternalOutputOwner::view_acquire(uint32_t p_slot, RendererViewport &p_viewports, const Ref<RendererViewport::Viewport::CameraCapture> &p_source, ExternalPacketViewLease &r_view) {
	if (!device || device != RenderingDevice::get_singleton() || !device->native_immutable_is_render_thread()) {
		return ERR_UNAUTHORIZED;
	}
	if (!r_view.is_empty()) {
		return ERR_ALREADY_IN_USE;
	}
	if (!admits_work() || p_slot >= slot_count || !view_cells || p_source.is_null()) {
		return ERR_UNAVAILABLE;
	}
	auto &slot = slots[p_slot];
	// This Ref was retained by the original prepared action/seal. Matching
	// projection bytes or a caller-selected slot cannot issue a view lease.
	if (slot.native_camera_source.is_null() || slot.native_camera_source != p_source || !slot.retained_host_snapshot ||
			slot.publication_closed || slot.pass_uses_closed || slot.original_pin.is_empty() || !slot.original_registry ||
			(slot.state != LeaseState::SEALED_RECORDED && slot.state != LeaseState::ADOPTED)) {
		return ERR_UNAUTHORIZED;
	}
	const auto &key = p_source->get_presentation_key();
	const auto &invocation = p_source->get_invocation();
	const auto *native = p_viewports.viewport_owner.get_or_null(p_source->get_viewport());
	if (!native || !native->presentation_registered || native->captured_camera != p_source ||
			!native->captured_camera_data_valid || native->current_camera_token != p_source->get_token() ||
			p_source->get_token() != slot.request.common.current_camera_token || key.view_epoch != slot.request.view_epoch ||
			key.present_seq != slot.request.common.present_seq || native->presentation_key.view_id != key.view_id ||
			native->presentation_key.view_epoch != key.view_epoch || native->presentation_key.present_seq != key.present_seq ||
			invocation.camera.is_null() || invocation.camera != native->camera || invocation.render_buffers.is_null() ||
			invocation.render_buffers != native->render_buffers || invocation.render_target != native->render_target ||
			invocation.target_size != native->size || invocation.internal_size != native->internal_size) {
		return ERR_INVALID_DATA;
	}
	if (invocation.use_xr || native->use_xr || invocation.view_count != 1 || native->view_count != 1) {
		return ERR_UNAVAILABLE;
	}
	if (!next_view_serial) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	uint32_t selected = UINT32_MAX;
	for (uint32_t i = 0; i < view_capacity; i++) {
		const auto &cell = view_cells[i];
		if (!cell.consumer && cell.pin.is_empty() && cell.camera_source.is_null()) {
			selected = i;
			break;
		}
	}
	if (selected == UINT32_MAX) {
		return ERR_OUT_OF_MEMORY;
	}
	if (slot.consumed_proof_requested && slot.consumed_proof_view.is_empty() && !slot.consumed_borrow_serial) {
		// Preflight the second quoted cell and serial before admitting any view.
		// The persistent original proof pin is prepaid before the first append.
		if (next_view_serial == UINT64_MAX) { return ERR_PARAMETER_RANGE_ERROR; }
		uint32_t proof_index = UINT32_MAX;
		for (uint32_t i = 0; i < view_capacity; ++i) {
			const auto &candidate = view_cells[i];
			if (i != selected && !candidate.consumer && candidate.pin.is_empty() && candidate.camera_source.is_null()) { proof_index = i; break; }
		}
		if (proof_index == UINT32_MAX) { return ERR_OUT_OF_MEMORY; }
		auto &proof_cell = view_cells[proof_index];
		const Error proof_borrowed = slot.original_registry->production_output_pin_borrow(slot.original_pin, *device, proof_cell.pin);
		if (proof_borrowed != OK) { return proof_borrowed; }
		auto &proof = slot.consumed_proof_view;
		proof_cell.camera_source = p_source;
		proof_cell.consumer = &proof;
		proof_cell.serial = next_view_serial++;
		proof_cell.slot = p_slot;
		proof.owner = this;
		proof.serial = proof_cell.serial;
		proof.slot = p_slot;
		proof.view_cell = proof_index;
		proof.profile = config.profile;
		proof.space = config.space;
		proof.current_host = slot.current_host;
		proof.previous_host = slot.previous_host;
		proof.retained_host_snapshot = true;
	}
	auto &cell = view_cells[selected];
	const Error borrowed = slot.original_registry->production_output_pin_borrow(slot.original_pin, *device, cell.pin);
	if (borrowed != OK) {
		return borrowed;
	}
	const uint64_t serial = next_view_serial;
	next_view_serial = serial == UINT64_MAX ? 0 : serial + 1;
	cell.camera_source = p_source;
	cell.consumer = &r_view;
	cell.serial = serial;
	cell.slot = p_slot;
	r_view.owner = this;
	r_view.serial = serial;
	r_view.slot = p_slot;
	r_view.view_cell = selected;
	r_view.profile = config.profile;
	r_view.space = config.space;
	r_view.current_host = slot.current_host;
	r_view.previous_host = slot.previous_host;
	r_view.retained_host_snapshot = true;
	return OK;
}

Error ExternalOutputOwner::view_copy_native_color_consumption(const ExternalPacketViewLease &p_view, ExternalConsumedColorProofSnapshot &r_proof) const {
	r_proof = {};
	if (!device || device != RenderingDevice::get_singleton() || !device->native_immutable_is_render_thread()) {
		return ERR_UNAUTHORIZED;
	}
	if (!preparation_view_matches(p_view, p_view.slot, true)) {
		return ERR_UNAUTHORIZED;
	}
	const auto &slot = slots[p_view.slot];
	const auto &cell = view_cells[p_view.view_cell];
	if (!native_color_consumption_matches(p_view.slot, slot.native_color_consumption_token)) {
		return ERR_UNAVAILABLE;
	}
	if (!p_view.retained_host_snapshot || cell.camera_source.is_null() || cell.camera_source != slot.native_camera_source) {
		return ERR_INVALID_DATA;
	}
	if (slot.native_color_consumption_particles.is_null() || slot.lease_token.size() != sizeof(r_proof.lease_token.bytes)) {
		return ERR_INVALID_DATA;
	}
	r_proof.request = slot.original_request;
	r_proof.action_ordinal = slot.original_action_ordinal;
	r_proof.particles = slot.native_color_consumption_particles;
	r_proof.generation = generation;
	r_proof.lease = slot.lease_serial;
	memcpy(r_proof.lease_token.bytes, slot.lease_token.ptr(), sizeof(r_proof.lease_token.bytes));
	r_proof.view_epoch = slot.request.view_epoch;
	r_proof.common = slot.request.common;
	r_proof.token = slot.native_color_consumption_token; // Original issuer only.
	r_proof.camera_source = cell.camera_source;
	r_proof.current_host = slot.current_host;
	r_proof.previous_host = slot.previous_host;
	return OK;
}

Error ExternalOutputOwner::view_retain_native_color_consumption(const ExternalPacketViewLease &p_view, ExternalPacketViewLease &r_retained) {
	if (!r_retained.is_empty()) {
		return ERR_ALREADY_IN_USE;
	}
	ExternalConsumedColorProofSnapshot proof;
	const Error observed = view_copy_native_color_consumption(p_view, proof);
	if (observed != OK) {
		return observed;
	}
	if (!next_view_serial) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	uint32_t selected = UINT32_MAX;
	for (uint32_t i = 0; i < view_capacity; ++i) {
		const auto &cell = view_cells[i];
		if (!cell.consumer && cell.pin.is_empty() && cell.camera_source.is_null()) {
			selected = i;
			break;
		}
	}
	if (selected == UINT32_MAX) {
		return ERR_OUT_OF_MEMORY;
	}
	auto &slot = slots[p_view.slot];
	auto &cell = view_cells[selected];
	const auto &source = view_cells[p_view.view_cell];
	// Retain an existing original view. Never reacquire a historical capture
	// through current viewport state and never issue another consumption token.
	const Error borrowed = slot.original_registry->production_output_pin_borrow(source.pin, *device, cell.pin);
	if (borrowed != OK) {
		return borrowed;
	}
	const uint64_t serial = next_view_serial;
	next_view_serial = serial == UINT64_MAX ? 0 : serial + 1;
	cell.camera_source = source.camera_source;
	cell.consumer = &r_retained;
	cell.serial = serial;
	cell.slot = p_view.slot;
	r_retained.owner = this;
	r_retained.serial = serial;
	r_retained.slot = p_view.slot;
	r_retained.view_cell = selected;
	r_retained.flattened_surface = p_view.flattened_surface;
	r_retained.profile = p_view.profile;
	r_retained.space = p_view.space;
	r_retained.current_host = p_view.current_host;
	r_retained.previous_host = p_view.previous_host;
	r_retained.retained_host_snapshot = p_view.retained_host_snapshot;
	return OK;
}

Error ExternalOutputOwner::view_return(ExternalPacketViewLease &p_view) {
	if (p_view.owner != this || p_view.view_cell >= view_capacity) {
		return ERR_INVALID_PARAMETER;
	}
	auto &cell = view_cells[p_view.view_cell];
	if (cell.consumer != &p_view || !p_view.serial || cell.serial != p_view.serial ||
			cell.slot != p_view.slot || cell.slot >= slot_count) {
		return ERR_INVALID_DATA;
	}
	auto &slot = slots[cell.slot];
	if (!slot.original_registry || cell.pin.is_empty()) {
		return ERR_INVALID_DATA;
	}
	const Error error = slot.original_registry->production_output_unpin(cell.pin);
	if (error != OK) {
		return error;
	}
	cell.camera_source.unref();
	cell.consumer = nullptr;
	cell.serial = 0;
	cell.slot = UINT32_MAX;
	p_view.owner = nullptr;
	p_view.serial = 0;
	p_view.slot = p_view.view_cell = UINT32_MAX;
	p_view.retained_host_snapshot = false;
	return OK;
}

void ExternalOutputOwner::view_move(ExternalPacketViewLease &p_to, ExternalPacketViewLease &p_from) {
	CRASH_COND(p_to.owner || p_from.owner != this || p_from.view_cell >= view_capacity);
	auto &cell = view_cells[p_from.view_cell];
	CRASH_COND(cell.consumer != &p_from || cell.serial != p_from.serial || cell.slot != p_from.slot);
	p_to.owner = this;
	p_to.serial = p_from.serial;
	p_to.slot = p_from.slot;
	p_to.view_cell = p_from.view_cell;
	p_to.flattened_surface = p_from.flattened_surface;
	p_to.profile = p_from.profile;
	p_to.space = p_from.space;
	p_to.current_host = p_from.current_host;
	p_to.previous_host = p_from.previous_host;
	p_to.retained_host_snapshot = p_from.retained_host_snapshot;
	cell.consumer = &p_to;
	p_from.owner = nullptr;
	p_from.serial = 0;
	p_from.slot = p_from.view_cell = UINT32_MAX;
	p_from.retained_host_snapshot = false;
}

Error ExternalOutputOwner::read_reserve_from_view(const ExternalPacketViewLease &p_view, ExternalPacketBinding &r_read) {
	if (!device || device != RenderingDevice::get_singleton() || !device->native_immutable_is_render_thread()) {
		return ERR_UNAUTHORIZED;
	}
	if (!r_read.is_empty()) {
		return ERR_ALREADY_IN_USE;
	}
	if (!preparation_view_matches(p_view, p_view.slot, false)) {
		return ERR_UNAUTHORIZED;
	}
	if (!next_read_serial) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	uint32_t selected = UINT32_MAX;
	for (uint32_t i = 0; i < read_capacity; ++i) {
		const auto &cell = read_cells[i];
		if (!cell.consumer && cell.pin.is_empty() && cell.camera_source.is_null() && !cell.camera_retained && (cell.state == ReadCell::State::UNUSED || cell.state == ReadCell::State::RETURNED)) {
			selected = i;
			break;
		}
	}
	if (selected == UINT32_MAX) {
		return ERR_OUT_OF_MEMORY;
	}
	auto &cell = read_cells[selected];
	const auto &view_cell = view_cells[p_view.view_cell];
	auto &slot = slots[p_view.slot];
	// Borrow only the original view pin. Registry independently rechecks live
	// original request/code/device and quoted unused cells before publishing.
	const Error borrowed = slot.original_registry->production_output_pin_borrow(view_cell.pin, *device, cell.pin);
	if (borrowed != OK) {
		return borrowed;
	}
	const uint64_t serial = next_read_serial;
	next_read_serial = serial == UINT64_MAX ? 0 : serial + 1;
	cell.consumer = &r_read;
	cell.serial = serial;
	cell.slot = p_view.slot;
	cell.previous_pin = {};
	cell.previous_slot = UINT32_MAX;
	cell.previous_camera_source.unref();
	cell.state = ReadCell::State::RESERVED;
	cell.consumption_kind = ReadCell::ConsumptionKind::RASTER;
	cell.empty_color_pass = uint32_t(ExternalPacketPass::OPAQUE);
	cell.color_consumption_serial = 0;
	cell.graph_count = cell.graph_limit = 0;
	cell.preparation = cell.surface = cell.draw_graph = cell.close_graph = cell.read_ordinal = UINT32_MAX;
	cell.admitted = cell.capture_complete = cell.uncaptured_effect = cell.draw_accepted = cell.raster_bound = false;
	cell.settle_graph_cursor = cell.settle_native_cursor = 0;
	cell.settle_member_found = false;
	cell.list_next_owner = nullptr;
	cell.list_next_read = UINT32_MAX;
	cell.list_begin = RDG::RecordHandle();
	cell.list_linked = false;
	cell.transfer_count = 0;
	r_read.owner = this;
	r_read.serial = serial;
	r_read.slot = p_view.slot;
	r_read.read_cell = selected;
	r_read.empty_packet = false;
	r_read.space = p_view.space;
	r_read.current_host = p_view.current_host;
	r_read.previous_host = p_view.previous_host;
	r_read.retained_host_snapshot = p_view.retained_host_snapshot;
	// A real selected view already retained its exact source before list begin.
	// Propagate that immutable Ref, never resolve a newer viewport capture.
	if (view_cell.camera_source.is_valid()) {
		cell.camera_source = view_cell.camera_source;
		cell.captured_viewport = cell.camera_source->get_viewport();
		cell.camera_key = cell.camera_source->get_presentation_key();
		cell.camera_token = cell.camera_source->get_token();
		cell.camera_invocation = cell.camera_source->get_invocation();
		cell.camera_data = cell.camera_source->get_camera_data();
		const Vector3 origin = cell.camera_data.main_transform.origin;
		const Vector3 forward = -cell.camera_data.main_transform.basis.get_column(2).normalized();
		for (uint32_t i = 0; i < 3; i++) {
			cell.camera_origin[i] = float(origin[i]);
			cell.camera_forward[i] = float(forward[i]);
		}
		cell.camera_retained = true;
	}
	if (slot.request.common.has_previous_consumed_tuple) {
		auto refuse_previous = [&](Error p_error) {
			const Error returned = slot.original_registry->production_output_unpin(cell.pin);
			CRASH_COND(returned != OK);
			cell.pin = {};
			read_clear_returned(cell);
			cell.consumer = nullptr;
			r_read.owner = nullptr;
			r_read.serial = 0;
			r_read.slot = r_read.read_cell = UINT32_MAX;
			r_read.retained_host_snapshot = false;
			r_read.previous_camera_source.unref();
			return p_error;
		};
		const uint32_t view_index = config.max_inflight ? p_view.slot / config.max_inflight : UINT32_MAX;
		if (view_index >= config.max_views || !views[view_index].occupied || views[view_index].closed ||
				views[view_index].tag != slot.request.common.view_tag || views[view_index].epoch != slot.request.view_epoch) {
			return refuse_previous(ERR_UNAVAILABLE);
		}
		GDExtensionOutputTokenV1 previous_token{};
		if (slot.request.common.previous_consumed_tuple_token.size() != sizeof(previous_token.bytes)) {
			return refuse_previous(ERR_INVALID_DATA);
		}
		memcpy(previous_token.bytes, slot.request.common.previous_consumed_tuple_token.ptr(), sizeof(previous_token.bytes));
		// The request pins one exact consumed tuple. The view's current history
		// may already have advanced after an earlier color pass of this packet.
		// Resolve only that retained native receipt within this view's slot bank.
		uint32_t previous_slot = UINT32_MAX;
		const uint32_t first_slot = view_index * config.max_inflight;
		if (first_slot >= slot_count || config.max_inflight > slot_count - first_slot) {
			return refuse_previous(ERR_INVALID_DATA);
		}
		for (uint32_t i = first_slot; i < first_slot + config.max_inflight; ++i) {
			if (i == p_view.slot || !native_color_consumption_matches(i, previous_token)) {
				continue;
			}
			if (previous_slot != UINT32_MAX || slots[i].request.common.view_tag != slot.request.common.view_tag ||
					slots[i].request.view_epoch != slot.request.view_epoch) {
				return refuse_previous(ERR_INVALID_DATA);
			}
			previous_slot = i;
		}
		if (previous_slot == UINT32_MAX) {
			return refuse_previous(ERR_UNAVAILABLE);
		}
		auto &predecessor = slots[previous_slot];
		if (!predecessor.original_registry || predecessor.original_pin.is_empty() || predecessor.native_camera_source.is_null()) {
			return refuse_previous(ERR_UNAVAILABLE);
		}
		const Error borrowed = predecessor.original_registry->production_output_pin_borrow(predecessor.original_pin, *device, cell.previous_pin);
		if (borrowed != OK) {
			return refuse_previous(borrowed);
		}
		cell.previous_slot = previous_slot;
		cell.previous_camera_source = predecessor.native_camera_source;
		r_read.previous_camera_source = cell.previous_camera_source;
	}
	return OK;
}

Error ExternalOutputOwner::read_retain_native_camera(ExternalPacketBinding &p_read, RendererViewport &p_viewports, RID p_viewport) {
	if (!device || device != RenderingDevice::get_singleton() || !device->native_immutable_is_render_thread()) {
		return ERR_UNAUTHORIZED;
	}
	if (!admits_work() || p_read.owner != this || p_read.read_cell >= read_capacity) {
		return ERR_UNAUTHORIZED;
	}
	auto &cell = read_cells[p_read.read_cell];
	if (cell.consumer != &p_read || !p_read.serial || cell.serial != p_read.serial || cell.slot != p_read.slot ||
			cell.slot >= slot_count || cell.pin.is_empty() || cell.state != ReadCell::State::RESERVED) {
		return ERR_INVALID_DATA;
	}
	if (cell.camera_retained) {
		return ERR_ALREADY_IN_USE; // No recapture, even for another native pass.
	}
	const auto &slot = slots[cell.slot];
	if (slot.publication_closed || slot.pass_uses_closed) {
		return ERR_UNAVAILABLE;
	}
	if (!slot.original_registry) {
		return ERR_UNAUTHORIZED;
	}
	// Same lock-or-borrow discipline as the original native preparation arm.
	// Keep original cancellation/close serialized through capture commit.
	GDExtensionDeviceExit::Registry::Lock lock(*slot.original_registry, GDExtensionDeviceExit::Registry::Lock::BLOCK_OR_BORROW);
	if (!slot.original_registry->production_output_pin_matches_locked(cell.pin, slot.original_request, *device)) {
		return ERR_UNAUTHORIZED;
	}
	const Error admitted = slot.original_registry->production_output_pin_new_work_matches_locked(cell.pin, *device);
	if (admitted != OK) {
		return admitted;
	}
	const auto *native = p_viewports.viewport_owner.get_or_null(p_viewport);
	if (!native || !native->presentation_registered || !native->captured_camera_data_valid) {
		return ERR_UNAVAILABLE;
	}
	const auto &token = slot.request.common.current_camera_token;
	const auto &key = native->captured_presentation_key;
	const auto &invocation = native->captured_invocation;
	// Only the real invocation overload supplies these original owner facts.
	// The token-only/payload fixture overloads cannot supply native custody.
	if (!token.is_valid() || token.view_epoch == 0 || token != native->current_camera_token ||
			key.view_id != token.view_id || key.view_epoch != token.view_epoch || key.view_epoch != slot.request.view_epoch ||
			key.present_seq != slot.request.common.present_seq || native->captured_camera_data.source_viewport != p_viewport ||
			native->captured_camera_data.source_view_id != token.view_id || native->captured_camera_data.source_view_epoch != token.view_epoch ||
			native->captured_camera_data.source_capture_ordinal != token.capture_ordinal || native->presentation_key.view_id != key.view_id ||
			native->presentation_key.view_epoch != key.view_epoch || native->presentation_key.present_seq != key.present_seq ||
			invocation.camera.is_null() || invocation.camera != native->camera || invocation.render_buffers.is_null() ||
			invocation.render_buffers != native->render_buffers || invocation.render_target != native->render_target ||
			invocation.target_size != native->size || invocation.internal_size != native->internal_size) {
		return ERR_INVALID_DATA;
	}
	// No selected-eye borrow exists yet. Do not infer an eye for XR/stereo.
	if (invocation.use_xr || native->use_xr || invocation.view_count != 1 || native->view_count != 1) {
		return ERR_UNAVAILABLE;
	}
	Ref<RendererViewport::Viewport::CameraCapture> source;
	if (!p_viewports.viewport_get_camera_capture_ref(p_viewport, token, source) || source.is_null() ||
			(slot.native_camera_source.is_valid() && slot.native_camera_source != source)) {
		return ERR_INVALID_DATA;
	}
	cell.camera_source = source;
	cell.captured_viewport = p_viewport;
	cell.camera_key = key;
	cell.camera_token = token;
	cell.camera_invocation = invocation;
	cell.camera_data = native->captured_camera_data;
	for (uint32_t i = 0; i < 3; ++i) {
		cell.camera_origin[i] = native->captured_camera_origin[i];
		cell.camera_forward[i] = native->captured_camera_forward[i];
	}
	cell.camera_retained = true;
	return OK;
}

void ExternalOutputOwner::read_clear_returned(ReadCell &cell) {
	CRASH_COND(!cell.pin.is_empty() || !cell.previous_pin.is_empty() || cell.list_linked);
	cell.camera_source.unref();
	cell.previous_camera_source.unref();
	cell.previous_slot = UINT32_MAX;
	cell.camera_invocation = RendererViewport::Viewport::CameraInvocation();
	cell.camera_data = RendererSceneRender::CameraData();
	cell.captured_viewport = RID();
	cell.camera_key = RendererViewport::Viewport::ViewPresentationKey();
	cell.camera_token = RendererViewport::Viewport::CameraSourceToken();
	for (uint32_t i = 0; i < 3; ++i) {
		cell.camera_origin[i] = cell.camera_forward[i] = 0;
	}
	cell.camera_retained = false;
	cell.color_consumption_serial = 0;
	cell.state = ReadCell::State::RETURNED;
	cell.consumption_kind = ReadCell::ConsumptionKind::RASTER;
	cell.empty_color_pass = uint32_t(ExternalPacketPass::OPAQUE);
}

Error ExternalOutputOwner::read_return(ExternalPacketBinding &p_read, bool p_may_have_recorded_prefix) {
	if (p_read.owner != this || p_read.read_cell >= read_capacity) {
		return ERR_INVALID_PARAMETER;
	}
	auto &cell = read_cells[p_read.read_cell];
	if (cell.consumer != &p_read || !p_read.serial || cell.serial != p_read.serial ||
			cell.slot != p_read.slot || cell.slot >= slot_count || cell.pin.is_empty()) {
		return ERR_INVALID_DATA;
	}
	auto &slot = slots[cell.slot];
	if (!slot.original_registry) {
		return ERR_INVALID_DATA;
	}
	if (cell.state != ReadCell::State::RESERVED && cell.state != ReadCell::State::RECORDED && cell.state != ReadCell::State::UNKNOWN) {
		return ERR_INVALID_DATA;
	}
	if (p_may_have_recorded_prefix) {
		// A bool is not a recorded/completed receipt. Keep the original pin and
		// finite cell until actual native settlement can resolve the prefix.
		cell.state = ReadCell::State::UNKNOWN;
	} else {
		if (cell.state != ReadCell::State::RESERVED || cell.graph_count || cell.uncaptured_effect) {
			return ERR_BUSY;
		}
		if (!cell.previous_pin.is_empty()) {
			const Error previous_error = slot.original_registry->production_output_unpin(cell.previous_pin);
			if (previous_error != OK) {
				return previous_error;
			}
		}
		const Error error = slot.original_registry->production_output_unpin(cell.pin);
		if (error != OK) {
			return error;
		}
		read_clear_returned(cell);
	}
	cell.consumer = nullptr;
	p_read.owner = nullptr;
	p_read.serial = 0;
	p_read.slot = p_read.read_cell = UINT32_MAX;
	p_read.empty_packet = false;
	p_read.retained_host_snapshot = false;
	p_read.previous_camera_source.unref();
	return OK;
}

void ExternalOutputOwner::read_move(ExternalPacketBinding &p_to, ExternalPacketBinding &p_from) {
	CRASH_COND(p_to.owner || p_from.owner != this || p_from.read_cell >= read_capacity);
	auto &cell = read_cells[p_from.read_cell];
	CRASH_COND(cell.consumer != &p_from || cell.serial != p_from.serial || cell.slot != p_from.slot);
	p_to.owner = this;
	p_to.serial = p_from.serial;
	p_to.slot = p_from.slot;
	p_to.read_cell = p_from.read_cell;
	p_to.selection = p_from.selection;
	p_to.empty_packet = p_from.empty_packet;
	p_to.pipeline = p_from.pipeline;
	p_to.vertex_array = p_from.vertex_array;
	p_to.index_array = p_from.index_array;
	p_to.transforms_uniform_set = p_from.transforms_uniform_set;
	p_to.material = p_from.material;
	p_to.current_host = p_from.current_host;
	p_to.previous_host = p_from.previous_host;
	p_to.previous_camera_source = p_from.previous_camera_source;
	p_to.space = p_from.space;
	p_to.retained_host_snapshot = p_from.retained_host_snapshot;
	p_to.vertex_format = p_from.vertex_format;
	p_to.commands = p_from.commands;
	p_to.command_bytes = p_from.command_bytes;
	p_to.command_offset = p_from.command_offset;
	p_to.ubershader = p_from.ubershader;
	p_to.cull_mode = p_from.cull_mode;
	cell.consumer = &p_to;
	p_from.owner = nullptr;
	p_from.serial = 0;
	p_from.slot = p_from.read_cell = UINT32_MAX;
	p_from.empty_packet = false;
	p_from.retained_host_snapshot = false;
}

bool ExternalOutputOwner::generation_surface_has_custody() const {
	for (uint32_t i = 0; i < surface_capacity; ++i) {
		const auto &cell = surface_cells[i];
		if (cell.slot == UINT32_MAX && (cell.consumer || !cell.pin.is_empty())) {
			return true;
		}
	}
	return false;
}

Error ExternalOutputOwner::surface_acquire(uint32_t p_surface, ExternalSurfaceSelection &r_surface) {
	auto *rd = RenderingDevice::get_singleton();
	if (!rd || !rd->native_immutable_is_render_thread()) {
		return ERR_UNAUTHORIZED;
	}
	if (!r_surface.is_empty()) {
		return ERR_ALREADY_IN_USE;
	}
	if (!admits_work() || config.profile != ExternalOutputConfig::INSTANCE_192 || !generation_surfaces_complete ||
			!generation_selection_complete || !generation_materials || retained_surface_count != generation_material_count) {
		return ERR_UNAVAILABLE;
	}
	if (p_surface >= retained_surface_count) {
		return ERR_INVALID_PARAMETER;
	}
	if (!generation_registry || !device || !generation_root_matches(generation_pin)) {
		return ERR_UNAUTHORIZED;
	}
	const auto &source = generation_materials[p_surface];
	if (!source.geometry_source.is_valid() || source.mesh.is_null() || source.native_surface == UINT32_MAX || source.material.is_null() || !source.generation) {
		return ERR_INVALID_DATA;
	}
	if (!next_surface_serial) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	uint32_t selected = UINT32_MAX;
	for (uint32_t i = 0; i < surface_capacity; ++i) {
		if (!surface_cells[i].consumer && surface_cells[i].pin.is_empty()) {
			selected = i;
			break;
		}
	}
	if (selected == UINT32_MAX) {
		return ERR_OUT_OF_MEMORY;
	}
	auto &cell = surface_cells[selected];
	const Error error = generation_registry->production_output_pin_borrow(generation_pin, *device, cell.pin);
	if (error != OK) {
		return error;
	}
	const uint64_t serial = next_surface_serial;
	next_surface_serial = serial == UINT64_MAX ? 0 : serial + 1;
	cell.consumer = &r_surface;
	cell.serial = serial;
	cell.slot = UINT32_MAX; // Generation-source custody; no slot or pass was selected.
	cell.surface = p_surface;
	r_surface.owner = this;
	r_surface.serial = serial;
	r_surface.generation = generation;
	r_surface.mesh = source.mesh;
	r_surface.material_selection = source.material;
	r_surface.geometry_source = &source.geometry_source;
	r_surface.cell = selected;
	r_surface.native_surface = source.native_surface;
	r_surface.flattened_surface = p_surface;
	r_surface.profile = config.profile;
	r_surface.primitive = source.geometry_source.get_primitive();
	r_surface.pass_mask = config.pass_mask;
	r_surface.surface_format = source.geometry_source.get_format();
	r_surface.surface_aabb = source.geometry_source.get_aabb();
	r_surface.surface_uv_scale = source.geometry_source.get_uv_scale();
	r_surface.classification = {};
	r_surface.material_generation = source.generation;
	r_surface.priority = 0;
	return OK;
}

Error ExternalOutputOwner::surface_return(ExternalSurfaceSelection &p_surface) {
	if (device && !device->native_immutable_is_render_thread()) {
		return ERR_UNAUTHORIZED;
	}
	if (p_surface.owner != this || p_surface.cell >= surface_capacity) {
		return ERR_INVALID_PARAMETER;
	}
	auto &cell = surface_cells[p_surface.cell];
	if (cell.consumer != &p_surface || !p_surface.serial || cell.serial != p_surface.serial || p_surface.generation != generation || cell.pin.is_empty()) {
		return ERR_INVALID_DATA;
	}
	GDExtensionDeviceExit::Registry *registry = nullptr;
	if (cell.slot == UINT32_MAX) {
		if (config.profile != ExternalOutputConfig::INSTANCE_192 || !generation_surfaces_complete || !generation_materials ||
				retained_surface_count != generation_material_count || cell.surface >= retained_surface_count || !generation_registry) {
			return ERR_INVALID_DATA;
		}
		const auto &source = generation_materials[cell.surface];
		if (p_surface.geometry_source != &source.geometry_source || !source.geometry_source.is_valid() ||
				p_surface.mesh != source.mesh || p_surface.native_surface != source.native_surface || p_surface.flattened_surface != cell.surface ||
				p_surface.material_selection != source.material || p_surface.material_generation != source.generation) {
			return ERR_INVALID_DATA;
		}
		registry = generation_registry;
	} else {
		if (cell.slot >= slot_count || cell.surface >= slots[cell.slot].surface_count) {
			return ERR_INVALID_DATA;
		}
		auto &slot = slots[cell.slot];
		const MeshStorage::NativeSurfaceHold *source = slot.surfaces ? &slot.surfaces[cell.surface].geometry : nullptr;
		if (config.profile == ExternalOutputConfig::INSTANCE_192 && generation_surfaces_complete) {
			if (!generation_materials || cell.surface >= retained_surface_count || retained_surface_count != generation_material_count) {
				return ERR_INVALID_DATA;
			}
			source = &generation_materials[cell.surface].geometry_source;
		}
		if (p_surface.generation != generation || !slot.surfaces || p_surface.geometry_source != source ||
				!slot.original_registry || cell.pin.is_empty()) {
			return ERR_INVALID_DATA;
		}
		registry = slot.original_registry;
	}
	const Error error = registry->production_output_unpin(cell.pin);
	if (error != OK) {
		return error;
	}
	cell.consumer = nullptr;
	cell.serial = 0;
	cell.slot = cell.surface = UINT32_MAX;
	p_surface.owner = nullptr;
	p_surface.serial = p_surface.generation = 0;
	p_surface.cell = UINT32_MAX;
	p_surface.geometry_source = nullptr;
	return OK;
}

void ExternalOutputOwner::surface_move(ExternalSurfaceSelection &p_to, ExternalSurfaceSelection &p_from) {
	CRASH_COND(p_to.owner || p_from.owner != this || p_from.cell >= surface_capacity);
	auto &cell = surface_cells[p_from.cell];
	CRASH_COND(cell.consumer != &p_from || cell.serial != p_from.serial);
	p_to.owner = this;
	p_to.serial = p_from.serial;
	p_to.generation = p_from.generation;
	p_to.mesh = p_from.mesh;
	p_to.material_selection = p_from.material_selection;
	p_to.geometry_source = p_from.geometry_source;
	p_to.cell = p_from.cell;
	p_to.native_surface = p_from.native_surface;
	p_to.flattened_surface = p_from.flattened_surface;
	p_to.profile = p_from.profile;
	p_to.primitive = p_from.primitive;
	p_to.pass_mask = p_from.pass_mask;
	p_to.surface_format = p_from.surface_format;
	p_to.surface_aabb = p_from.surface_aabb;
	p_to.surface_uv_scale = p_from.surface_uv_scale;
	p_to.classification = p_from.classification;
	p_to.material_generation = p_from.material_generation;
	p_to.priority = p_from.priority;
	cell.consumer = &p_to;
	p_from.owner = nullptr;
	p_from.serial = p_from.generation = 0;
	p_from.cell = UINT32_MAX;
	p_from.geometry_source = nullptr;
}
} // namespace RendererRD

namespace RendererRD {
Error ExternalOutputOwner::acquire_generation_selection(GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 request) {
	if (generation_selection_attempted || state != GenerationState::CONFIGURED || has_live_slots()) {
		return ERR_ALREADY_IN_USE;
	}
	if (!generation_registry || !device || !generation_root_matches(generation_pin) ||
			request.registry_instance != generation_request.registry_instance || request.handle_serial != generation_request.handle_serial ||
			!generation_registry->production_output_pin_matches(generation_pin, request, *device)) {
		return ERR_UNAUTHORIZED;
	}
	if (config.profile != ExternalOutputConfig::INSTANCE_192 || !metadata_directories_placement || !generation_config_inline || !generation_meshes || !generation_materials || !generation_mesh_count || !generation_material_count) {
		return ERR_INVALID_DATA;
	}
	MetadataLayout layout;
	Error error = plan_metadata_layout(view_capacity, read_capacity, surface_capacity, layout, generation_mesh_count, generation_material_count, binding_capacity, storage_capacity, preparation_capacity);
	if (error != OK) {
		return error;
	}
	error = generation_registry->production_output_metadata_validate(context, request, metadata_lease, *device, layout.total_bytes, layout.alignment);
	if (error != OK) {
		return error;
	}
	auto *meshes = MeshStorage::get_singleton();
	auto *materials = MaterialStorage::get_singleton();
	if (!meshes || !materials) {
		return ERR_UNAVAILABLE;
	}
	generation_selection_attempted = true;
	auto fail = [&](Error reason) {
		withdraw(); // Close every view/slot before setting the terminal failure state.
		generation_selection_unpublished = true;
		state = GenerationState::FAULTED_RETIRING;
		return reason;
	};
	for (uint32_t i = 0; i < generation_mesh_count; ++i) {
		auto &row = generation_meshes[i];
		error = meshes->mesh_selection_acquire(row.mesh, row.lease);
		if (error != OK) {
			return fail(error);
		}
		++retained_mesh_count;
	}
	uint32_t flattened = 0;
	for (uint32_t i = 0; i < generation_mesh_count; ++i) {
		const int count = meshes->mesh_get_surface_count(generation_meshes[i].mesh);
		if (count <= 0 || uint32_t(count) > generation_material_count - flattened) {
			return fail(ERR_INVALID_DATA);
		}
		flattened += uint32_t(count);
	}
	if (flattened != generation_material_count) {
		return fail(ERR_INVALID_DATA);
	}
	for (uint32_t i = 0; i < generation_material_count; ++i) {
		auto &row = generation_materials[i];
		error = materials->material_selection_acquire(row.material, row.lease, row.generation);
		if (error != OK) {
			return fail(error);
		}
		++retained_material_count;
	}
	generation_selection_complete = true;
	return OK;
}
Error ExternalOutputOwner::read_bound_material_contact(uint32_t surface, BoundMaterialContact &out) const {
	if (!admits_work() || !generation_registry || !device || !generation_root_matches(generation_pin) ||
			!generation_registry->production_output_pin_matches(generation_pin, generation_request, *device)) {
		return ERR_UNAUTHORIZED;
	}
	if (!generation_selection_complete || !generation_surfaces_complete ||
			retained_mesh_count != generation_mesh_count || retained_material_count != generation_material_count ||
			retained_surface_count != generation_material_count || surface >= retained_surface_count) {
		return ERR_INVALID_DATA;
	}
	const auto &selection = generation_materials[surface];
	if (!selection.lease.is_active() || !selection.geometry_source.is_active() || !selection.geometry_source.is_valid() ||
			selection.mesh.is_null() || selection.native_surface == UINT32_MAX) {
		return ERR_UNAVAILABLE;
	}
	uint64_t mesh_revision = 0;
	for (uint32_t i = 0; i < retained_mesh_count; ++i) {
		if (generation_meshes[i].mesh == selection.mesh && generation_meshes[i].lease.is_active()) {
			mesh_revision = generation_meshes[i].lease.get_revision();
			break;
		}
	}
	if (!mesh_revision) {
		return ERR_INVALID_DATA;
	}
	auto *materials = MaterialStorage::get_singleton();
	if (!materials) {
		return ERR_UNAVAILABLE;
	}
	BoundMaterialContact contact;
	const Error observed = materials->material_selection_get_shader_observation(selection.lease, contact.material);
	if (observed != OK) {
		return observed;
	}
	if (contact.material.material != selection.material || contact.material.material_generation != selection.generation) {
		return ERR_INVALID_DATA;
	}
	contact.output_generation = generation;
	contact.mesh = selection.mesh;
	contact.mesh_revision = mesh_revision;
	contact.native_surface = selection.native_surface;
	contact.mesh_bounds = selection.geometry_source.get_aabb();
	contact.vertex_count = selection.geometry_source.get_vertex_count();
	contact.index_count = selection.geometry_source.get_index_count();
	if (!contact.mesh_bounds.is_finite() || !contact.mesh_bounds.get_end().is_finite()) {
		return ERR_INVALID_DATA;
	}
	out = contact;
	return OK;
}
Error ExternalOutputOwner::acquire_generation_surface_sources(GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 request) {
	if (generation_surfaces_attempted || state != GenerationState::CONFIGURED || has_live_slots()) {
		return ERR_ALREADY_IN_USE;
	}
	if (!generation_selection_complete || retained_mesh_count != generation_mesh_count || retained_material_count != generation_material_count ||
			!generation_registry || !device || !generation_root_matches(generation_pin) ||
			request.registry_instance != generation_request.registry_instance || request.handle_serial != generation_request.handle_serial ||
			!generation_registry->production_output_pin_matches(generation_pin, request, *device)) {
		return ERR_UNAUTHORIZED;
	}
	MetadataLayout layout;
	Error error = plan_metadata_layout(view_capacity, read_capacity, surface_capacity, layout, generation_mesh_count, generation_material_count, binding_capacity, storage_capacity, preparation_capacity);
	if (error != OK) {
		return error;
	}
	error = generation_registry->production_output_metadata_validate(context, request, metadata_lease, *device, layout.total_bytes, layout.alignment);
	if (error != OK) {
		return error;
	}
	auto *meshes = MeshStorage::get_singleton();
	if (!meshes) {
		return ERR_UNAVAILABLE;
	}
	generation_surfaces_attempted = true;
	auto fail = [&](Error reason) {
		withdraw();
		generation_selection_unpublished = true;
		state = GenerationState::FAULTED_RETIRING;
		return reason;
	};
	for (uint32_t mesh = 0; mesh < generation_mesh_count; ++mesh) {
		const RID rid = generation_meshes[mesh].mesh;
		const int count = meshes->mesh_get_surface_count(rid);
		if (count <= 0 || uint32_t(count) > generation_material_count - retained_surface_count) {
			return fail(ERR_INVALID_DATA);
		}
		for (uint32_t surface = 0; surface < uint32_t(count); ++surface) {
			auto &row = generation_materials[retained_surface_count];
			error = meshes->mesh_surface_source_hold_acquire(rid, surface, row.geometry_source);
			if (error != OK) {
				return fail(error);
			}
			row.mesh = rid;
			row.native_surface = surface;
			++retained_surface_count;
		}
	}
	if (retained_surface_count != generation_material_count) {
		return fail(ERR_INVALID_DATA);
	}
	generation_surfaces_complete = true;
	const Error published = generation_registry->production_output_demand_publish(context, request, *this, *device);
	return published == OK ? OK : fail(published);
}
} // namespace RendererRD
