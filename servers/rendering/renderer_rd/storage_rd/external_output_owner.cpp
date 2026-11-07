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
#include "servers/rendering/renderer_rd/storage_rd/external_output_packet.h"

#include <cstddef>
#include "external_output_storage.h"

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

bool ExternalOutputOwner::generation_root_matches(const GDExtensionDeviceExit::ProductionOutputPin &pin) const {
	return &pin == &generation_pin && !pin.is_empty() && generation_registry == pin.registry &&
			generation_request.registry_instance && generation_request.handle_serial == pin.request_serial &&
			generation_root_serial && generation_root_serial == pin.serial;
}

Error ExternalOutputOwner::generation_returnable(uint32_t &unused, uint32_t &returned) const {
	unused = returned = 0;
	if ((state != GenerationState::WITHDRAWING && state != GenerationState::RETIRING && state != GenerationState::FAULTED_RETIRING) || has_live_slots()) {
		return ERR_BUSY;
	}
	for (uint32_t i = 0; i < slot_count; ++i) {
		const auto &slot = slots[i];
		if (!slot.publication_closed || !slot.pass_uses_closed || slot.storage || slot.surfaces || slot.surface_count || !slot.original_pin.is_empty() || slot.owned_group || slot.output_membership_group || slot_has_custody(i)) {
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
	const bool digests_match = config_tags_inline ?
			(request.layout_digest.size() == 32 && request.mapping_digest.size() == 32 &&
					memcmp(request.layout_digest.ptr(), frozen_config_tags[2], 32) == 0 && memcmp(request.mapping_digest.ptr(), frozen_config_tags[3], 32) == 0) :
			(request.layout_digest == config.layout_digest && request.mapping_digest == config.mapping_digest);
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

Error ExternalOutputOwner::plan_metadata_layout(uint32_t p_views, uint32_t p_reads, uint32_t p_surfaces, MetadataLayout &r_layout) {
	MetadataLayout layout;
	layout.view_count = p_views;
	layout.read_count = p_reads;
	layout.surface_count = p_surfaces;
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
			!append(p_surfaces ? MAX_SLOTS : 0, sizeof(ExternalOutputStorage), alignof(ExternalOutputStorage), layout.storage_offset) ||
			!append(p_surfaces, sizeof(SurfaceBinding), alignof(SurfaceBinding), layout.surface_binding_offset) ||
			!append(p_surfaces, sizeof(SurfaceCell), alignof(SurfaceCell), layout.surface_offset) ||
			alignment > alignof(std::max_align_t)) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	layout.total_bytes = uint32_t(cursor);
	layout.alignment = alignment;
	r_layout = layout;
	return OK;
}

Error ExternalOutputOwner::install_metadata_directory(GDExtensionDeviceExit::ProductionPhysicalContextRef context,
		GDExtensionProductionRequestRefV1 request, GDExtensionDeviceExit::Registry &registry,
		const GDExtensionDeviceExit::ProductionOutputMetadataLease &lease,
		uint32_t p_views, uint32_t p_reads, uint32_t p_surfaces) {
	if (metadata_directory_installed || view_cells || read_cells || surface_cells || view_capacity || read_capacity || surface_capacity) {
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
	MetadataLayout layout;
	Error error = plan_metadata_layout(p_views, p_reads, p_surfaces, layout);
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
	if (has_live_slots()) {
		return ERR_BUSY;
	}
	for (uint32_t i = 0; i < view_capacity; ++i) {
		if (view_cells[i].consumer || !view_cells[i].pin.is_empty()) {
			return ERR_BUSY;
		}
	}
	for (uint32_t i = 0; i < read_capacity; ++i) {
		const auto &cell = read_cells[i];
		if (cell.consumer || !cell.pin.is_empty() ||
				(cell.state != ReadCell::State::UNUSED && cell.state != ReadCell::State::RETURNED)) {
			return ERR_BUSY;
		}
	}
	for (uint32_t i = 0; i < surface_capacity; ++i) {
		if (surface_cells[i].consumer || !surface_cells[i].pin.is_empty()) {
			return ERR_BUSY;
		}
	}
	if (metadata_directories_placement) {
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
		if (cell.slot == p_slot && (cell.consumer || !cell.pin.is_empty() || (cell.state != ReadCell::State::UNUSED && cell.state != ReadCell::State::RETURNED))) {
			return true;
		}
	}
	return false;
}

Error ExternalOutputOwner::return_slot_pin(uint32_t p_slot) {
	if (p_slot >= slot_count) {
		return ERR_INVALID_PARAMETER;
	}
	auto &cell = slots[p_slot];
	if (cell.state != LeaseState::RETIRING || !cell.publication_closed || !cell.pass_uses_closed ||
			cell.storage || cell.surfaces || cell.surface_count || cell.owned_group || slot_has_custody(p_slot)) {
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
	cell.request = {};
	cell.lease_token = {};
	cell.record_token = {};
	cell.packet_token = {};
	cell.state = LeaseState::RELEASED;
	return OK;
}

Error ExternalOutputOwner::view_acquire(uint32_t, ExternalPacketViewLease &) {
	// Actual camera -> sealed slot/consumed tuple issuer join is still absent.
	// Never select a packet from a slot number or its copied tokens alone.
	return ERR_UNAVAILABLE;
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
		if (cell.state != ReadCell::State::RESERVED) {
			return ERR_BUSY;
		}
		const Error error = slot.original_registry->production_output_unpin(cell.pin);
		if (error != OK) {
			return error;
		}
		cell.state = ReadCell::State::RETURNED;
	}
	cell.consumer = nullptr;
	p_read.owner = nullptr;
	p_read.serial = 0;
	p_read.slot = p_read.read_cell = UINT32_MAX;
	p_read.retained_host_snapshot = false;
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
	p_to.pipeline = p_from.pipeline;
	p_to.vertex_array = p_from.vertex_array;
	p_to.index_array = p_from.index_array;
	p_to.transforms_uniform_set = p_from.transforms_uniform_set;
	p_to.material = p_from.material;
	p_to.current_host = p_from.current_host;
	p_to.previous_host = p_from.previous_host;
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
	p_from.retained_host_snapshot = false;
}

Error ExternalOutputOwner::surface_return(ExternalSurfaceSelection &p_surface) {
	if (p_surface.owner != this || p_surface.cell >= surface_capacity) {
		return ERR_INVALID_PARAMETER;
	}
	auto &cell = surface_cells[p_surface.cell];
	if (cell.consumer != &p_surface || !p_surface.serial || cell.serial != p_surface.serial ||
			cell.slot >= slot_count || cell.surface >= slots[cell.slot].surface_count) {
		return ERR_INVALID_DATA;
	}
	auto &slot = slots[cell.slot];
	if (p_surface.generation != generation || !slot.surfaces ||
			p_surface.geometry_source != &slot.surfaces[cell.surface].geometry ||
			!slot.original_registry || cell.pin.is_empty()) {
		return ERR_INVALID_DATA;
	}
	const Error error = slot.original_registry->production_output_unpin(cell.pin);
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
