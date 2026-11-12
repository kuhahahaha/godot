/**************************************************************************/
/*  gdextension_production_metadata_selection.cpp                                    */
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

// Native CPU selection transport in the original OUTPUT_METADATA resource.
// No physical surface/pipeline custody or material GPU qualification is issued.
#include "core/extension/gdextension_external_output_interface.h"
#include "core/extension/gdextension_output_config_facts.h"
#include "core/extension/gdextension_production_request.h"
#include "core/os/os.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_config.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_owner.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/rendering_server.h"
#include "servers/rendering/renderer_scene_cull.h"

#include <cstddef>
#include <new>

namespace GDExtensionDeviceExit {
namespace {
using Meshes = RendererRD::MeshStorage;
using Materials = RendererRD::MaterialStorage;
static_assert(uint32_t(RendererRD::ExternalOutputConfig::PASS_OPAQUE) == uint32_t(GDEXTENSION_OUTPUT_PASS_OPAQUE_V2));
static_assert(uint32_t(RendererRD::ExternalOutputConfig::PASS_DEPTH) == uint32_t(GDEXTENSION_OUTPUT_PASS_DEPTH_V2));
static_assert(uint32_t(RendererRD::ExternalOutputConfig::PASS_SHADOW) == uint32_t(GDEXTENSION_OUTPUT_PASS_SHADOW_V2));
static_assert(uint32_t(RendererRD::ExternalOutputConfig::PASS_TRANSPARENT) == uint32_t(GDEXTENSION_OUTPUT_PASS_TRANSPARENT_V2));
static_assert(uint32_t(RendererRD::ExternalOutputConfig::PASS_PROBE) == uint32_t(GDEXTENSION_OUTPUT_PASS_PROBE_V2));
struct SelectionMeshRow {
	RID mesh;
	uint32_t surfaces = 0;
	Meshes::NativeSelectionLease lease;
};
struct SelectionMaterialRow {
	RID material;
	uint64_t generation = 0;
	Materials::NativeSelectionLease lease;
};
struct SelectionOwner {
	ProductionOutputMetadataLease lease;
	RenderingDevice *device = nullptr;
	Meshes *meshes = nullptr;
	Materials *materials = nullptr;
	SelectionMeshRow *mesh_rows = nullptr;
	SelectionMaterialRow *material_rows = nullptr;
	uint32_t mesh_count = 0, material_count = 0, held_meshes = 0, held_materials = 0;
	bool complete = false, closing = false;
	~SelectionOwner() { CRASH_COND(held_meshes || held_materials || !lease.is_empty()); }
};
struct SelectionLayout {
	uint64_t meshes = 0, materials = 0, bytes = 0;
};
Error selection_layout(uint32_t meshes, uint32_t materials, SelectionLayout &out) {
	if (!meshes || meshes > RendererRD::ExternalOutputConfig::MAX_MESH_RIDS || !materials || materials > RendererRD::ExternalOutputConfig::MAX_FLATTENED_SURFACES) {
		return ERR_INVALID_PARAMETER;
	}
	auto align = [](uint64_t bytes, size_t alignment) { return (bytes + alignment - 1) & ~(uint64_t(alignment) - 1); };
	SelectionLayout layout;
	layout.meshes = align(sizeof(SelectionOwner), alignof(SelectionMeshRow));
	layout.materials = align(layout.meshes + uint64_t(meshes) * sizeof(SelectionMeshRow), alignof(SelectionMaterialRow));
	layout.bytes = layout.materials + uint64_t(materials) * sizeof(SelectionMaterialRow);
	if (layout.bytes > UINT32_MAX) { return ERR_PARAMETER_RANGE_ERROR; }
	out = layout;
	return OK;
}
template <class T> bool abi2(const T &value) { return value.abi.abi_version == 2 && value.abi.struct_size == sizeof(T) && !value.abi.reserved; }
GDExtensionDeviceSessionResultV1 status(Error error) { return { int32_t(error), 0, 0, 0 }; }
struct Range { uintptr_t begin = 0, end = 0; };
bool range(const void *data, uint64_t bytes, size_t alignment, Range &out) {
	const uintptr_t begin = uintptr_t(data);
	if (!begin || begin % alignment || bytes > UINTPTR_MAX - begin) { return false; }
	out = { begin, begin + uintptr_t(bytes) }; return true;
}
template <class T> bool range(const T *data, Range &out) { return range(data, sizeof(T), alignof(T), out); }
bool overlaps(const Range &a, const Range &b) { return a.begin < b.end && b.begin < a.end; }
bool rid_span(const GDExtensionOutputRidSpanV1 &span, const Range &input, const Range &output) {
	Range values;
	return !span.reserved && span.count && range(span.data, uint64_t(span.count) * sizeof(uint64_t), alignof(uint64_t), values) && !overlaps(values, input) && !overlaps(values, output);
}
void bounds_dto(const AABB &bounds, GDExtensionOutputBoundsV1 &out) {
	for (uint32_t i = 0; i < 3; ++i) { out.position[i] = bounds.position[i]; out.size[i] = bounds.size[i]; }
}
} // namespace

Error Registry::output_metadata_selection_resolve_locked(GDExtensionProductionAttachmentRefV1 attachment_ref,
		GDExtensionOutputMetadataSelectionRefV2 ref, bool new_work, ProductionRequest *&request, ProductionResourceSlot *&resource, RenderingDevice *&rd) {
	ProductionAttachment *attachment = nullptr;
	Error error = OK;
	if (new_work) {
		// Existing read/facts/CreateV4 callbacks run on the original protected RT,
		// not the installation's owner thread. The referenced selection may live
		// in an older request of the same retained original attachment.
		error = production_output_selection_request_locked(attachment_ref, ref.request, request, rd);
		if (error != OK) { return error; }
		attachment = request->attachment;
	} else {
		// Main return only seals CPU reads; retain the existing strict owner gate.
		error = production_attachment_locked(attachment_ref, false, attachment, rd);
		if (error != OK) { return error; }
		RenderingDevice *original = nullptr;
		error = production_request_locked(ref.request, false, request, original);
		if (error != OK) { return error; }
		if (original != rd || request->attachment != attachment) { return ERR_INVALID_DATA; }
	}
	if (ref.reserved || !ref.resource_generation || request->attachment != attachment || ref.resource_slot >= request->resource_count) { return ERR_INVALID_DATA; }
	resource = &request->resources[ref.resource_slot];
	if (resource->request != request || resource->request_serial != request->serial ||
			resource->declaration.resource_kind != GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1 ||
			resource->declaration.original_generation != ref.resource_generation || resource->metadata_kind != 2 || !resource->metadata_created) { return ERR_INVALID_DATA; }
	return OK;
}

Error Registry::output_metadata_selection_prepare(ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref,
		const GDExtensionOutputMetadataSelectionPrepareV2 &input, GDExtensionOutputMetadataSelectionResultV2 &out) {
	SelectionLayout layout;
	Error error = selection_layout(input.meshes.count, input.materials.count, layout);
	if (!abi2(input) || input.reserved || input.meshes.reserved || input.materials.reserved || error != OK) { return ERR_INVALID_PARAMETER; }
	Lock lock(*this, Lock::BLOCK_OR_BORROW);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	error = production_physical_request_locked(context, ref, request, rd, true);
	if (error != OK) { return error; }
	if (input.metadata_resource_slot >= request->resource_count || !rd || rd != RenderingDevice::get_singleton() || !rd->native_immutable_is_render_thread()) { return ERR_UNAUTHORIZED; }
	auto *meshes = Meshes::get_singleton();
	auto *materials = Materials::get_singleton();
	if (!meshes || !materials) { return ERR_UNAVAILABLE; }
	auto &resource = request->resources[input.metadata_resource_slot];
	if (!resource.declaration.original_generation) { return ERR_INVALID_DATA; }
	ProductionOutputMetadataLease allocation;
	error = production_output_metadata_create(context, ref, input.metadata_resource_slot, *rd, layout.bytes, alignof(std::max_align_t), allocation, true);
	if (error != OK) { return error; }
	auto *owner = new (allocation.get_data()) SelectionOwner;
	owner->device = rd; owner->meshes = meshes; owner->materials = materials;
	owner->mesh_count = input.meshes.count; owner->material_count = input.materials.count;
	owner->mesh_rows = reinterpret_cast<SelectionMeshRow *>(static_cast<uint8_t *>(allocation.get_data()) + layout.meshes);
	owner->material_rows = reinterpret_cast<SelectionMaterialRow *>(static_cast<uint8_t *>(allocation.get_data()) + layout.materials);
	for (uint32_t i = 0; i < owner->mesh_count; ++i) { new (&owner->mesh_rows[i]) SelectionMeshRow; }
	for (uint32_t i = 0; i < owner->material_count; ++i) { new (&owner->material_rows[i]) SelectionMaterialRow; }
	production_output_metadata_move(owner->lease, allocation);
	resource.metadata_kind = 2;
	out.selection = { ref, input.metadata_resource_slot, 0, resource.declaration.original_generation };
	auto finish = [&](Error result) {
		out.retained_meshes = owner->held_meshes; out.retained_materials = owner->held_materials;
		return result;
	};
	uint32_t flattened = 0;
	for (uint32_t i = 0; i < owner->mesh_count; ++i) {
		auto &row = owner->mesh_rows[i]; row.mesh = RID::from_uint64(input.meshes.data[i]);
		error = meshes->mesh_selection_acquire(row.mesh, row.lease);
		if (error != OK) { return finish(error); }
		++owner->held_meshes;
		const int count = meshes->mesh_get_surface_count(row.mesh);
		if (count <= 0 || uint32_t(count) > owner->material_count - flattened) { return finish(ERR_INVALID_DATA); }
		row.surfaces = uint32_t(count); flattened += row.surfaces;
	}
	if (flattened != owner->material_count) { return finish(ERR_INVALID_DATA); }
	for (uint32_t i = 0; i < owner->material_count; ++i) {
		auto &row = owner->material_rows[i]; row.material = RID::from_uint64(input.materials.data[i]);
		if (row.material.is_null() || !materials->owns_material(row.material)) { return finish(ERR_INVALID_DATA); }
		row.generation = materials->material_get_generation(row.material);
		if (!row.generation) { return finish(ERR_INVALID_DATA); }
		error = materials->material_selection_acquire(row.material, row.lease, row.generation);
		if (error != OK) { return finish(error); }
		++owner->held_materials;
	}
	owner->complete = true;
	return finish(OK);
}

Error Registry::output_metadata_selection_read(GDExtensionProductionAttachmentRefV1 attachment,
		GDExtensionOutputMetadataSelectionRefV2 ref, uint32_t surface, GDExtensionOutputMetadataSelectionObservationV2 &out) {
	Lock lock(*this, Lock::BLOCK_OR_BORROW);
	ProductionRequest *request = nullptr; ProductionResourceSlot *resource = nullptr; RenderingDevice *rd = nullptr;
	Error error = output_metadata_selection_resolve_locked(attachment, ref, true, request, resource, rd);
	if (error != OK) { return error; }
	if (!rd || rd != RenderingDevice::get_singleton() || !rd->native_immutable_is_render_thread()) { return ERR_UNAUTHORIZED; }
	if (request->cancelled || request->release_started || resource->metadata_returned || !resource->metadata_data) { return ERR_UNAVAILABLE; }
	const auto &owner = *static_cast<const SelectionOwner *>(resource->metadata_data);
	if (!owner.complete || owner.closing || owner.device != rd || owner.meshes != Meshes::get_singleton() || owner.materials != Materials::get_singleton() ||
			owner.held_meshes != owner.mesh_count || owner.held_materials != owner.material_count || surface >= owner.material_count || resource->metadata_consumer != &owner.lease) { return ERR_UNAVAILABLE; }
	uint32_t local_surface = surface, mesh_index = 0;
	while (mesh_index < owner.mesh_count && local_surface >= owner.mesh_rows[mesh_index].surfaces) { local_surface -= owner.mesh_rows[mesh_index++].surfaces; }
	if (mesh_index == owner.mesh_count) { return ERR_INVALID_DATA; }
	Meshes::NativeSelectionSurfaceObservation mesh;
	error = owner.meshes->mesh_selection_get_surface_observation(owner.mesh_rows[mesh_index].lease, local_surface, mesh);
	if (error != OK) { return error; }
	const auto &material = owner.material_rows[surface];
	if (!material.lease.is_active() || owner.materials->material_get_generation(material.material) != material.generation) { return ERR_INVALID_DATA; }
	GDExtensionOutputMetadataSelectionObservationV2 observed{};
	observed.abi = { 2, sizeof(observed), 0 };
	observed.mesh = mesh.mesh.get_id(); observed.mesh_revision = mesh.mesh_revision;
	observed.native_surface = mesh.native_surface; observed.vertex_count = mesh.vertex_count; observed.index_count = mesh.index_count;
	observed.primitive = mesh.primitive; observed.format = mesh.format; bounds_dto(mesh.bounds, observed.mesh_bounds);
	observed.material = material.material.get_id(); observed.material_generation = material.generation;
	Materials::NativeSelectionShaderObservation compiled;
	const Error compiled_error = owner.materials->material_selection_get_shader_observation(material.lease, compiled);
	observed.shader_observation_error = compiled_error;
	observed.material_recipe_rejections = UINT32_MAX;
	if (compiled_error == OK) {
		if (compiled.material != material.material || compiled.material_generation != material.generation) { return ERR_INVALID_DATA; }
		observed.shader_version = compiled.shader_version.get_id(); observed.needs_inverse = compiled.needs_inverse;
		observed.zero_displacement = compiled.zero_displacement; observed.material_recipe_rejections = compiled.material_recipe_rejections;
	}
	out = observed;
	return OK;
}

Error Registry::output_selection_facts_v2(GDExtensionProductionAttachmentRefV1 attachment,
		GDExtensionOutputMetadataSelectionRefV2 ref, const GDExtensionOutputSelectionFactsInputV2 &input, GDExtensionOutputSelectionFactsV2 &out) {
	if (!abi2(input) || input.reserved || input.profile < 1 || input.profile > 2 || (input.requested_pass_mask & ~uint32_t(0x1f))) { return ERR_INVALID_PARAMETER; }
	// There is no original generated Ribbon geometry-envelope owner in this
	// transport. Never relabel source bounds or an unrelated mesh as that proof.
	if (input.profile == 2) { return ERR_UNAVAILABLE; }
	Lock lock(*this, Lock::BLOCK_OR_BORROW);
	GDExtensionOutputMetadataSelectionObservationV2 observed{};
	Error error = output_metadata_selection_read(attachment, ref, input.surface, observed);
	if (error != OK) { return error; }
	if (!output_mesh_bounds_valid_v2(observed.mesh_bounds)) { return ERR_INVALID_DATA; }
	ProductionRequest *request = nullptr; ProductionResourceSlot *resource = nullptr; RenderingDevice *rd = nullptr;
	error = output_metadata_selection_resolve_locked(attachment, ref, true, request, resource, rd);
	if (error != OK) { return error; }
	const auto &owner = *static_cast<const SelectionOwner *>(resource->metadata_data);
	GDExtensionOutputSelectionFactsV2 facts{};
	facts.abi = { 2, sizeof(facts), 0 }; facts.selection = ref; facts.surface = input.surface; facts.profile = input.profile;
	const auto &device = device_records[request->attachment->device];
	facts.device.abi = { 2, sizeof(facts.device), 0 };
	facts.device.registry_instance = instance; facts.device.device_epoch = device.epoch; facts.device.session = device.session;
	if (!output_device_tag_v2(instance, device.epoch, device.session, facts.device.device_tag)) { return ERR_INVALID_DATA; }
	facts.observation = observed;
	facts.pass_status = status(Error(observed.shader_observation_error));
	// geometry_bounds is exact zero for Instance192. Native mesh bounds remain
	// an independent observation, even when the current shader is not ready.
	if (observed.shader_observation_error == OK) {
		Materials::NativeSelectionShaderObservation compiled;
		error = owner.materials->material_selection_get_shader_observation(owner.material_rows[input.surface].lease, compiled);
		if (error != OK) { return error; }
		if (compiled.material.get_id() != observed.material || compiled.material_generation != observed.material_generation || compiled.shader_version.get_id() != observed.shader_version) { return ERR_INVALID_DATA; }
		facts.eligible_pass_mask = compiled.output_pass_mask;
		facts.pass_status = status(output_pass_intent_v2(input.requested_pass_mask, facts.eligible_pass_mask, facts.selected_pass_mask) ? OK : ERR_UNAVAILABLE);
	}
	out = facts;
	return OK;
}

Error Registry::output_metadata_selection_return(GDExtensionProductionAttachmentRefV1 attachment, GDExtensionOutputMetadataSelectionRefV2 ref) {
	Lock lock(*this, Lock::BLOCK_OR_BORROW);
	ProductionRequest *request = nullptr; ProductionResourceSlot *resource = nullptr; RenderingDevice *rd = nullptr;
	Error error = output_metadata_selection_resolve_locked(attachment, ref, false, request, resource, rd);
	if (error != OK) { return error; }
	if (resource->metadata_returned) { return OK; }
	if (!resource->metadata_data) { return ERR_INVALID_DATA; }
	auto &owner = *static_cast<SelectionOwner *>(resource->metadata_data);
	if (resource->metadata_consumer != &owner.lease) { return ERR_INVALID_DATA; }
	// No native owner is read or returned on main. This only seals the original
	// slab under Registry; the actual RT RELEASE owns every checked return.
	owner.closing = true;
	return ERR_BUSY;
}

Error Registry::output_metadata_selection_release_locked(ProductionRequest &request, ProductionResourceSlot &resource, RenderingDevice *rd,
		bool reclaim, const GDExtensionProductionServiceBudgetV1 *budget, ProductionServiceUsage *usage) {
	if (reclaim || !rd || !resource.metadata_data || resource.metadata_returned || resource.metadata_kind != 2) { return ERR_BUSY; }
	if (!budget && !usage) { return OK; } // Readiness only; no native return.
	if (!budget || !usage || !budget->controls || !budget->deadline_micros || OS::get_singleton()->get_ticks_usec() >= budget->deadline_micros ||
			usage->passes >= MIN(budget->passes, 64u) || usage->receipts >= MIN(budget->receipts, 128u) || !rd->native_immutable_is_render_thread() ||
			device_records[request.attachment->device].execution_thread != Thread::get_caller_id()) { return ERR_BUSY; }
	if (request.control.state != ProductionControlState::EXECUTING || request.control.input.kind != ProductionControlKind::RELEASE || request.revision == UINT64_MAX) { return ERR_UNAUTHORIZED; }
	auto &owner = *static_cast<SelectionOwner *>(resource.metadata_data);
	if (owner.device != rd || owner.meshes != Meshes::get_singleton() || owner.materials != Materials::get_singleton() || resource.metadata_consumer != &owner.lease) { return ERR_INVALID_DATA; }
	owner.closing = true;
	++usage->passes; ++usage->receipts;
	if (owner.held_materials) {
		const Error returned = owner.materials->material_selection_release(owner.material_rows[owner.held_materials - 1].lease);
		if (returned != OK) { return returned; }
		--owner.held_materials; ++request.revision; return ERR_BUSY;
	}
	if (owner.held_meshes) {
		const Error returned = owner.meshes->mesh_selection_release(owner.mesh_rows[owner.held_meshes - 1].lease);
		if (returned != OK) { return returned; }
		--owner.held_meshes; ++request.revision; return ERR_BUSY;
	}
	for (uint32_t i = 0; i < owner.material_count; ++i) { owner.material_rows[i].~SelectionMaterialRow(); }
	for (uint32_t i = 0; i < owner.mesh_count; ++i) { owner.mesh_rows[i].~SelectionMeshRow(); }
	ProductionOutputMetadataLease allocation;
	production_output_metadata_move(allocation, owner.lease);
	owner.~SelectionOwner();
	const Error returned = production_output_metadata_return(allocation);
	CRASH_COND_MSG(returned != OK, "Original selection slab remained owned after checked prefix return and placement destruction.");
	return ERR_BUSY; // Fresh original RELEASE finishes its remaining owner work.
}

Error Registry::output_metadata_selection_matches(GDExtensionProductionAttachmentRefV1 attachment, GDExtensionOutputMetadataSelectionRefV2 ref,
		const RendererRD::ExternalOutputConfig &config, const RendererRD::ExternalOutputOwner *late) {
	Lock lock(*this, Lock::BLOCK_OR_BORROW);
	ProductionRequest *request = nullptr; ProductionResourceSlot *resource = nullptr; RenderingDevice *rd = nullptr;
	Error error = output_metadata_selection_resolve_locked(attachment, ref, true, request, resource, rd);
	if (error != OK) { return error; }
	if (!rd || !rd->native_immutable_is_render_thread() || request->cancelled || request->release_started || !resource->metadata_data || resource->metadata_returned) { return ERR_UNAVAILABLE; }
	const auto &owner = *static_cast<const SelectionOwner *>(resource->metadata_data);
	if (!owner.complete || owner.closing || resource->metadata_consumer != &owner.lease || owner.device != rd ||
			uint32_t(config.surfaces.size()) != owner.mesh_count || uint32_t(config.surface_materials.size()) != owner.material_count) { return ERR_INVALID_DATA; }
	if (late && (late->device != rd || late->generation_registry != this || late->generation_mesh_count != owner.mesh_count ||
			late->generation_material_count != owner.material_count || !late->generation_selection_complete || !late->generation_surfaces_complete)) { return ERR_INVALID_DATA; }
	for (uint32_t i = 0; i < owner.mesh_count; ++i) {
		const auto &mesh = owner.mesh_rows[i];
		if (config.surfaces[i] != mesh.mesh || !mesh.lease.is_active() || (late && (late->generation_meshes[i].mesh != mesh.mesh ||
				late->generation_meshes[i].lease.get_revision() != mesh.lease.get_revision()))) { return ERR_INVALID_DATA; }
	}
	for (uint32_t i = 0; i < owner.material_count; ++i) {
		GDExtensionOutputMetadataSelectionObservationV2 observed{};
		error = output_metadata_selection_read(attachment, ref, i, observed);
		if (error != OK) { return error; }
		if (config.surface_materials[i].get_id() != observed.material || (late && (late->generation_materials[i].material.get_id() != observed.material ||
				late->generation_materials[i].generation != observed.material_generation || late->generation_materials[i].mesh.get_id() != observed.mesh ||
				late->generation_materials[i].native_surface != observed.native_surface))) { return ERR_INVALID_DATA; }
	}
	return OK;
}

GDExtensionDeviceSessionResultV1 device_session_output_selection_facts_v2(GDExtensionProductionAttachmentRefV1 attachment,
		GDExtensionOutputMetadataSelectionRefV2 ref, const GDExtensionOutputSelectionFactsInputV2 *input, GDExtensionOutputSelectionFactsV2 *out) {
	Range a, b; if (!range(input, a) || !range(out, b) || overlaps(a, b)) { return status(ERR_INVALID_PARAMETER); }
	return status(Registry::get_singleton().output_selection_facts_v2(attachment, ref, *input, *out));
}

GDExtensionDeviceSessionResultV1 device_session_output_metadata_selection_query_v2(const GDExtensionOutputMetadataSelectionQueryV2 *input, GDExtensionOutputMetadataSelectionRequirementsV2 *out) {
	Range a, b; if (!range(input, a) || !range(out, b) || overlaps(a, b) || !abi2(*input)) { return status(ERR_INVALID_PARAMETER); }
	SelectionLayout layout; const Error error = selection_layout(input->mesh_count, input->material_count, layout);
	if (error != OK) { return status(error); }
	*out = { { 2, sizeof(*out), 0 }, layout.bytes, alignof(std::max_align_t), input->mesh_count, input->material_count, 0 };
	return status(OK);
}
GDExtensionDeviceSessionResultV1 device_session_output_metadata_selection_prepare_v2(GDExtensionProductionPhysicalContextRefV1 context, GDExtensionProductionRequestRefV1 request,
		const GDExtensionOutputMetadataSelectionPrepareV2 *input, GDExtensionOutputMetadataSelectionResultV2 *out) {
	Range a, b; if (!range(input, a) || !range(out, b) || overlaps(a, b) || !abi2(*input) ||
			!rid_span(input->meshes, a, b) || !rid_span(input->materials, a, b)) { return status(ERR_INVALID_PARAMETER); }
	*out = {}; out->abi = { 2, sizeof(*out), 0 };
	const Error error = Registry::get_singleton().output_metadata_selection_prepare(context, request, *input, *out);
	out->status = status(error); return status(error);
}
GDExtensionDeviceSessionResultV1 device_session_output_metadata_selection_read_v2(GDExtensionProductionAttachmentRefV1 attachment, GDExtensionOutputMetadataSelectionRefV2 selection,
		uint32_t surface, GDExtensionOutputMetadataSelectionObservationV2 *out) {
	Range output; if (!range(out, output)) { return status(ERR_INVALID_PARAMETER); }
	return status(Registry::get_singleton().output_metadata_selection_read(attachment, selection, surface, *out));
}
GDExtensionDeviceSessionResultV1 device_session_output_metadata_selection_return_v2(GDExtensionProductionAttachmentRefV1 attachment, GDExtensionOutputMetadataSelectionRefV2 selection) {
	return status(Registry::get_singleton().output_metadata_selection_return(attachment, selection));
}

Error Registry::output_create_v4(ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref,
		const GDExtensionOutputCreateInputV4 &input, GDExtensionOutputCreateResultV1 &out) {
	if (input.abi.abi_version != 4 || input.abi.struct_size != sizeof(input) || input.abi.reserved) { return ERR_INVALID_PARAMETER; }
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	if (!storage) { return ERR_UNAVAILABLE; }
	GDExtensionProductionAttachmentRefV1 attachment{};
	RendererRD::ExternalOutputConfig config;
	{
		Lock lock(*this, Lock::BLOCK_OR_BORROW);
		ProductionRequest *request = nullptr; RenderingDevice *rd = nullptr;
		const Error authenticated = production_physical_request_locked(context, ref, request, rd, true);
		if (authenticated != OK) { return authenticated; }
		attachment = { instance, request->attachment->serial };
		uint32_t surfaces = 0;
		GDExtensionOutputCreateResultV1 decoded{};
		if (!storage->_external_decode_create(input.create.config, config, surfaces, decoded)) { return Error(decoded.status.error); }
		if (config.profile != RendererRD::ExternalOutputConfig::INSTANCE_192) { return ERR_INVALID_PARAMETER; }
	}
	Error matched = output_metadata_selection_matches(attachment, input.selection, config);
	if (matched != OK) { return matched; }
	// No RT task can interleave the original native selection return with this
	// synchronous Create. Main can only seal reads; it cannot return the holds.
	storage->particles_external_create_typed_v3(context, ref, input.create, out);
	if (out.status.error != OK) { return Error(out.status.error); }
	Lock lock(*this, Lock::BLOCK_OR_BORROW);
	auto *particles = storage->particles_owner.get_or_null(RID::from_uint64(out.particles));
	if (!particles || !particles->external) { return ERR_INVALID_DATA; }
	auto &owner = *particles->external;
	matched = output_metadata_selection_matches(attachment, input.selection, config, &owner);
	if (matched != OK) {
		// Preserve the returned real generation/RID and its original close debt.
		// No failed post-check may drop an actually created native owner.
		owner.withdraw(); owner.generation_selection_unpublished = true;
		owner.state = RendererRD::ExternalOutputOwner::GenerationState::FAULTED_RETIRING;
		out.status = { matched, 31, "early_native_selection_mismatch" };
	}
	return matched;
}

Error Registry::output_scene_query_commit(ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref,
		const GDExtensionOutputSceneQueryCommitV2 &input, GDExtensionOutputSceneQueryResultV2 &out) {
	if (!abi2(input)) { return ERR_INVALID_PARAMETER; }
	Lock lock(*this, Lock::BLOCK_OR_BORROW);
	ProductionRequest *request = nullptr; RenderingDevice *rd = nullptr;
	Error error = production_physical_request_locked(context, ref, request, rd, true);
	if (error != OK) { return error; }
	Transform3D transform;
	for (uint32_t i = 0; i < 3; ++i) {
		transform.basis.set_column(i, Vector3(input.transform[i * 3], input.transform[i * 3 + 1], input.transform[i * 3 + 2]));
	}
	transform.origin = Vector3(input.transform[9], input.transform[10], input.transform[11]);
	auto bounds = [](const GDExtensionOutputBoundsV1 &dto) { return AABB(Vector3(dto.position[0], dto.position[1], dto.position[2]), Vector3(dto.size[0], dto.size[1], dto.size[2])); };
	SceneConnectionQueryObservation committed;
	error = scene_connection_commit_query({ instance, request->attachment->serial }, input.connection, transform, bounds(input.local_envelope), bounds(input.world_query), input.bounds_revision, committed);
	if (error != OK) { return error; }
	GDExtensionOutputSceneQueryResultV2 result{};
	result.abi = { 2, sizeof(result), 0 }; result.status = status(OK);
	result.receiver = committed.receiver; result.base = committed.base; result.generation = committed.generation;
	result.bounds_revision = committed.bounds_revision; result.pose_revision = committed.pose_revision; result.pair_revision = committed.pair_revision;
	for (uint32_t i = 0; i < 3; ++i) {
		const Vector3 column = committed.transform.basis.get_column(i);
		for (uint32_t j = 0; j < 3; ++j) { result.transform[i * 3 + j] = column[j]; }
		result.transform[9 + i] = committed.transform.origin[i];
	}
	bounds_dto(committed.local_envelope, result.local_envelope); bounds_dto(committed.world_query, result.world_query);
	out = result;
	return OK;
}

GDExtensionDeviceSessionResultV1 device_session_output_create_v4(GDExtensionProductionPhysicalContextRefV1 context, GDExtensionProductionRequestRefV1 request,
		const GDExtensionOutputCreateInputV4 *input, GDExtensionOutputCreateResultV1 *out) {
	Range a, b;
	if (!range(input, a) || !range(out, b) || overlaps(a, b) ||
			!rid_span(input->create.config.surfaces, a, b) || !rid_span(input->create.config.surface_materials, a, b)) { return status(ERR_INVALID_PARAMETER); }
	GDExtensionOutputCreateResultV1 created{};
	created.abi = { 1, sizeof(created), 0 };
	const Error error = Registry::get_singleton().output_create_v4(context, request, *input, created);
	if (!created.status.error && error != OK) { created.status = { error, 0, "" }; }
	*out = created;
	return status(error);
}
GDExtensionDeviceSessionResultV1 device_session_output_scene_query_commit_v2(GDExtensionProductionPhysicalContextRefV1 context, GDExtensionProductionRequestRefV1 request,
		const GDExtensionOutputSceneQueryCommitV2 *input, GDExtensionOutputSceneQueryResultV2 *out) {
	Range a, b; if (!range(input, a) || !range(out, b) || overlaps(a, b)) { return status(ERR_INVALID_PARAMETER); }
	return status(Registry::get_singleton().output_scene_query_commit(context, request, *input, *out));
}
} // namespace GDExtensionDeviceExit
