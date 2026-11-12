/**************************************************************************/
/*  external_output_packet.cpp                                            */
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

#include "external_output_packet.h"

#include "core/extension/gdextension_production_request.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_owner.h"
#include "servers/rendering/renderer_rd/storage_rd/render_data_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/rendering_server_globals.h"

#ifdef TESTS_ENABLED
#include "core/os/os.h"

#include <cstdio>
#endif

namespace RendererRD {
Error ParticlesStorage::_external_transforms_prepare_owned_prefix(ExternalPipelinePreparation &row) {
	if (!row.owner || !row.pipeline_scope || !row.pipeline_pin || !row.transforms_owned_group || row.backend_error != OK ||
			row.finish_complete || row.transforms_uniform_set.is_valid() || !row.material.is_valid() || row.owner_slot >= row.owner->slot_count) {
		return ERR_UNAUTHORIZED;
	}
	auto &owner = *row.owner;
	auto &slot = owner.slots[row.owner_slot];
	if (!owner.device || owner.device != RD::get_singleton() || !owner.device->native_immutable_is_render_thread() ||
			!slot.storage || !slot.owned_group || row.pipeline_pin != &slot.original_pin || slot.publication_closed || slot.pass_uses_closed) {
		return ERR_UNAUTHORIZED;
	}
	const auto &current = slot.storage->resources[ExternalOutputStorage::CURRENT_RECORDS];
	const auto &previous = slot.storage->resources[ExternalOutputStorage::PREVIOUS_RECORDS];
	// No ordinary-particle buffer/set can substitute for a missing slot record.
	if (current.rid.is_null() || previous.rid.is_null() || !current.bytes || current.bytes != previous.bytes) {
		return ERR_UNAVAILABLE;
	}
	RD::Uniform uniforms[2] = {
		RD::Uniform(RD::UNIFORM_TYPE_STORAGE_BUFFER, 0, current.rid),
		RD::Uniform(RD::UNIFORM_TYPE_STORAGE_BUFFER, 1, previous.rid),
	};
	RD::ProductionOutputUniformSource sources[2];
	for (uint32_t i = 0; i < 2; ++i) {
		sources[i].buffer = i ? previous.rid : current.rid;
		sources[i].owned_group = slot.owned_group;
		sources[i].owned_ordinal = i;
		sources[i].byte_size = current.bytes;
	}
	return owner.device->_production_output_owned_uniform_set_create(*row.pipeline_scope, row.transforms_owned_group, 0,
			VectorView<RD::Uniform>(uniforms, 2), row.material.get_snapshot().shader, 2, row.transforms_uniform_set,
			row.transforms_uniform_witness, VectorView<RD::ProductionOutputUniformSource>(sources, 2));
}

Error ParticlesStorage::particles_external_view_acquire(const ExternalSurfaceSelection &surface, const ::RenderDataRD *render_data, ExternalPacketViewLease &view) {
	if (!surface.owner || !render_data || !RSG::viewport || !view.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	auto &owner = *surface.owner;
	if (!owner.device || owner.device != RD::get_singleton() || !owner.device->native_immutable_is_render_thread() ||
			!owner.admits_work() || surface.generation != owner.generation || surface.cell >= owner.surface_capacity) {
		return ERR_UNAUTHORIZED;
	}
	const auto &surface_cell = owner.surface_cells[surface.cell];
	if (surface_cell.consumer != &surface || surface_cell.serial != surface.serial || surface_cell.pin.is_empty()) {
		return ERR_UNAUTHORIZED;
	}
	const auto &source = render_data->get_native_camera_source();
	if (source.is_null()) {
		return ERR_UNAVAILABLE;
	}
	uint32_t selected = UINT32_MAX;
	for (uint32_t i = 0; i < owner.slot_count; ++i) {
		const auto &slot = owner.slots[i];
		if ((slot.state == ExternalOutputOwner::LeaseState::SEALED_RECORDED || slot.state == ExternalOutputOwner::LeaseState::ADOPTED) &&
				!slot.publication_closed && !slot.pass_uses_closed && slot.native_camera_source == source) {
			if (selected != UINT32_MAX) {
				return ERR_INVALID_DATA;
			}
			selected = i;
		}
	}
	if (selected == UINT32_MAX || surface.flattened_surface >= owner.slots[selected].surface_count) {
		return ERR_UNAVAILABLE;
	}
	const Error error = owner.view_acquire(selected, *RSG::viewport, source, view);
	if (error == OK) {
		view.flattened_surface = surface.flattened_surface;
	}
	return error;
}

Error ParticlesStorage::particles_external_resolve(const ExternalPacketViewLease &view, const ExternalPacketPipelineRequest &pipeline_request, RD::DrawListID, ExternalPacketBinding &binding) {
	if (!view.owner || !binding.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	auto &owner = *view.owner;
	if (!owner.device || owner.device != RD::get_singleton() || !owner.device->native_immutable_is_render_thread() ||
			!owner.preparation_view_matches(view, view.slot, false) || view.slot >= owner.slot_count) {
		return ERR_UNAUTHORIZED;
	}
	auto &slot = owner.slots[view.slot];
	if ((slot.state != ExternalOutputOwner::LeaseState::SEALED_RECORDED && slot.state != ExternalOutputOwner::LeaseState::ADOPTED) ||
			!slot.storage || !slot.guard_resources_ready || view.flattened_surface >= slot.surface_count) {
		return ERR_UNAVAILABLE;
	}
	if (owner.config.profile == ExternalOutputConfig::INSTANCE_192 && owner.config.capacity == 0) {
		// A supported renderer pass with no empty-packet implementation skips
		// this item. Malformed pass values remain an authority failure.
		switch (pipeline_request.pass) {
			case ExternalPacketPass::OPAQUE:
			case ExternalPacketPass::TRANSPARENT:
				break;
			case ExternalPacketPass::DEPTH:
			case ExternalPacketPass::SHADOW:
			case ExternalPacketPass::PROBE:
			case ExternalPacketPass::MOTION:
				return ERR_UNAVAILABLE;
			default:
				return ERR_UNAUTHORIZED;
		}
		const uint32_t pass = pipeline_request.pass == ExternalPacketPass::OPAQUE ? ExternalOutputConfig::PASS_OPAQUE :
				pipeline_request.pass == ExternalPacketPass::TRANSPARENT ? ExternalOutputConfig::PASS_TRANSPARENT : 0;
		const auto &current = slot.storage->resources[ExternalOutputStorage::CURRENT_RECORDS];
		const auto &previous = slot.storage->resources[ExternalOutputStorage::PREVIOUS_RECORDS];
		const auto &surface = slot.surfaces[view.flattened_surface];
		const auto &command = slot.storage->resources[ExternalOutputStorage::PROTECTED_COMMANDS];
		if (!pass || !(owner.config.pass_mask & pass) || !(slot.request.pass_mask & pass) ||
				current.rid.is_valid() || current.bytes || previous.rid.is_valid() || previous.bytes ||
				command.rid.is_null() || surface.command_extent != 20 || surface.command_offset != uint64_t(view.flattened_surface) * 20 ||
				surface.command_offset > command.bytes || surface.command_extent > command.bytes - surface.command_offset) {
			return ERR_UNAUTHORIZED;
		}
		if (!owner.generation_materials || view.flattened_surface >= owner.generation_material_count) {
			return ERR_UNAUTHORIZED;
		}
		const auto &source = owner.generation_materials[view.flattened_surface];
		MaterialStorage::NativeSelectionShaderObservation classification;
		auto *materials = MaterialStorage::get_singleton();
		const Error classified = materials ? materials->material_selection_get_shader_observation(source.lease, classification) : ERR_UNAVAILABLE;
		if (classified != OK) {
			return classified;
		}
		if (classification.material != source.material || classification.material_generation != source.generation ||
				classification.shader_version.is_null() || !(classification.output_pass_mask & pass)) {
			return ERR_UNAUTHORIZED;
		}
		const Error error = owner.read_reserve_from_view(view, binding);
		if (error != OK) {
			return error;
		}
		auto &read = owner.read_cells[binding.read_cell];
		read.consumption_kind = ExternalOutputOwner::ReadCell::ConsumptionKind::EMPTY_COLOR;
		read.empty_color_pass = uint32_t(pipeline_request.pass);
		read.surface = view.flattened_surface;
		binding.selection = pipeline_request;
		binding.empty_packet = true;
		binding.pipeline = binding.vertex_array = binding.index_array = binding.transforms_uniform_set = RID();
		binding.material = nullptr;
		binding.commands = RID();
		binding.command_bytes = binding.command_offset = 0;
		return OK;
	}
	const ExternalPipelinePreparation *selected = nullptr;
	for (uint32_t i = 0; i < owner.preparation_capacity; ++i) {
		const auto &row = owner.preparation_rows[i];
		if (row.owner == &owner && row.owner_slot == view.slot && row.tuple_surface == view.flattened_surface && row.selection == pipeline_request &&
				row.tuple_state == ExternalPipelinePreparation::TupleState::READY) {
			if (selected) {
				return ERR_INVALID_DATA;
			}
			selected = &row;
		}
	}
	if (!selected || !selected->finish_complete || selected->finish_error != OK || selected->backend_error != OK ||
			!selected->pipeline_sealed || !selected->geometry_adopted || !selected->material_adopted || !selected->material_transferred || !selected->material.is_active() ||
			selected->selected_geometry >= 2 || !selected->geometry[selected->selected_geometry].has_retained_snapshot() ||
			selected->created_pipeline.is_null() || selected->transforms_uniform_set.is_null() || !selected->transforms_owned_group) {
		return ERR_UNAVAILABLE;
	}
	const auto &surface = slot.surfaces[view.flattened_surface];
	const auto &command = slot.storage->resources[ExternalOutputStorage::PROTECTED_COMMANDS];
	if (command.rid.is_null() || surface.command_extent != 20 || surface.command_offset != uint64_t(view.flattened_surface) * 20 ||
			surface.command_offset > command.bytes || surface.command_extent > command.bytes - surface.command_offset) {
		return ERR_INVALID_DATA;
	}
	const Error error = owner.read_reserve_from_view(view, binding);
	if (error != OK) {
		return error;
	}
	// Retain the exact selected row, never re-resolve a newer native tuple.
	owner.read_cells[binding.read_cell].preparation = uint32_t(selected - owner.preparation_rows);
	owner.read_cells[binding.read_cell].surface = view.flattened_surface;
	const auto &geometry = selected->geometry[selected->selected_geometry];
	binding.selection = pipeline_request;
	binding.pipeline = selected->created_pipeline;
	binding.vertex_array = geometry.get_retained_vertex_array();
	binding.index_array = geometry.get_retained_index_array();
	binding.vertex_format = geometry.get_retained_vertex_format();
	binding.transforms_uniform_set = selected->transforms_uniform_set;
	// The original read pin keeps this stable prepared snapshot alive. Do not
	// allocate/copy material vectors on a native per-pass resolution.
	binding.material = &selected->material.get_snapshot();
	binding.commands = command.rid;
	binding.command_bytes = surface.command_extent;
	binding.command_offset = surface.command_offset;
	binding.ubershader = selected->ubershader;
	binding.cull_mode = selected->cull_mode;
	return OK;
}

ExternalSurfaceSelection::ExternalSurfaceSelection(ExternalSurfaceSelection &&p_other) noexcept {
	if (p_other.owner) {
		p_other.owner->surface_move(*this, p_other);
	}
}
ExternalSurfaceSelection &ExternalSurfaceSelection::operator=(ExternalSurfaceSelection &&p_other) noexcept {
	if (this != &p_other) {
		CRASH_COND(owner);
		if (p_other.owner) {
			p_other.owner->surface_move(*this, p_other);
		}
	}
	return *this;
}
ExternalSurfaceSelection::~ExternalSurfaceSelection() {
	CRASH_COND(owner);
}

ExternalPacketViewLease::ExternalPacketViewLease(ExternalPacketViewLease &&p_other) noexcept {
	if (p_other.owner) {
		p_other.owner->view_move(*this, p_other);
	}
}
ExternalPacketViewLease &ExternalPacketViewLease::operator=(ExternalPacketViewLease &&p_other) noexcept {
	if (this != &p_other) {
		CRASH_COND(owner);
		if (p_other.owner) {
			p_other.owner->view_move(*this, p_other);
		}
	}
	return *this;
}
ExternalPacketViewLease::~ExternalPacketViewLease() {
	CRASH_COND(owner);
}
bool ExternalPacketViewLease::matches_native_camera_source(const ::RenderDataRD &p_render_data) const {
	if (!owner || !owner->device || !owner->device->native_immutable_is_render_thread() || !owner->preparation_view_matches(*this, slot, false)) {
		return false;
	}
	const auto &cell = owner->view_cells[view_cell];
	return cell.camera_source.is_valid() && cell.camera_source == p_render_data.get_native_camera_source();
}
Error ExternalPacketViewLease::release_checked() {
	return owner ? owner->view_return(*this) : ERR_DOES_NOT_EXIST;
}
Error ExternalPacketViewLease::copy_native_color_consumption(ExternalConsumedColorProofSnapshot &r_proof) const {
	r_proof = {};
	return owner ? owner->view_copy_native_color_consumption(*this, r_proof) : ERR_DOES_NOT_EXIST;
}
Error ExternalPacketViewLease::retain_native_color_consumption(ExternalPacketViewLease &r_retained) const {
	return owner ? owner->view_retain_native_color_consumption(*this, r_retained) : ERR_DOES_NOT_EXIST;
}
ExternalPacketBinding::ExternalPacketBinding(ExternalPacketBinding &&p_other) noexcept {
	if (p_other.owner) {
		p_other.owner->read_move(*this, p_other);
	}
}
ExternalPacketBinding &ExternalPacketBinding::operator=(ExternalPacketBinding &&p_other) noexcept {
	if (this != &p_other) {
		CRASH_COND(owner);
		if (p_other.owner) {
			p_other.owner->read_move(*this, p_other);
		}
	}
	return *this;
}
ExternalPacketBinding::~ExternalPacketBinding() {
	CRASH_COND(owner);
}
bool ExternalPacketBinding::matches_native_camera_source(const ::RenderDataRD &p_render_data) const {
	if (!owner || !owner->device || !owner->device->native_immutable_is_render_thread() || !owner->admits_work() ||
			read_cell >= owner->read_capacity || slot >= owner->slot_count || owner->slots[slot].publication_closed || owner->slots[slot].pass_uses_closed) {
		return false;
	}
	const auto &cell = owner->read_cells[read_cell];
	return cell.consumer == this && serial && cell.serial == serial && cell.slot == slot && !cell.pin.is_empty() &&
			cell.state == ExternalOutputOwner::ReadCell::State::RESERVED && cell.camera_retained &&
			cell.camera_source.is_valid() && cell.camera_source == owner->slots[slot].native_camera_source &&
			cell.camera_source == p_render_data.get_native_camera_source();
}
Error ExternalPacketBinding::cancel_before_draw() {
	return owner ? owner->read_return(*this, false) : ERR_DOES_NOT_EXIST;
}

ExternalOutputStorage *ExternalPacketBinding::get_previous_storage() const {
	if (!owner || read_cell >= owner->read_capacity) {
		return nullptr;
	}
	const auto &cell = owner->read_cells[read_cell];
	if (cell.consumer != this || cell.serial != serial || cell.previous_slot >= owner->slot_count || cell.previous_pin.is_empty()) {
		return nullptr;
	}
	return owner->slots[cell.previous_slot].storage;
}
Error ExternalPacketBinding::record_checked(RenderingDevice::DrawListID list) {
	return record_checked(list, nullptr, 0);
}
Error ExternalPacketBinding::record_checked(RenderingDevice::DrawListID list, const void *push, uint32_t push_bytes) {
	if (!owner || !owner->device) {
		return ERR_UNAVAILABLE;
	}
	if (empty_packet) {
		return push || push_bytes ? ERR_INVALID_PARAMETER : owner->device->_production_output_empty_read_record(*this, list);
	}
	return owner->device->_production_output_read_record(*this, list, push, push_bytes);
}

} // namespace RendererRD

namespace RendererRD {
ExternalPipelinePreparation::~ExternalPipelinePreparation() {
	CRASH_COND(pipeline_scope || pipeline_pin || pipeline_original_root || pipeline_owned_group || geometry_reserved_group || material_reserved_group || transforms_owned_group || transforms_uniform_set.is_valid() || owned_pipeline_root.is_valid() || geometry[0].is_active() || geometry[1].is_active() || material.is_active());
}
Error ExternalPipelinePreparation::_material_future_pipeline_plan(RID p_shader, RID p_uniform_set, MaterialStorage::NativeMaterialFuturePipelinePlan &r_plan) {
	return owner ? owner->material_future_pipeline_plan(*this, p_shader, p_uniform_set, r_plan) : ERR_UNAUTHORIZED;
}
Error ExternalPipelinePreparation::_prospective_admission(RID p_shader, uint64_t p_input_mask, const MeshStorage::NativeVertexArrayPlan &p_geometry, RID p_uniform_set, RD::FramebufferFormatID p_framebuffer, RD::RenderPrimitive p_primitive, const RD::PipelineRasterizationState &p_raster, const RD::PipelineMultisampleState &p_multisample, const RD::PipelineDepthStencilState &p_depth, const RD::PipelineColorBlendState &p_blend, uint32_t p_subpass, const Vector<RD::PipelineSpecializationConstant> &p_specialization, MaterialStorage::NativeMaterialFuturePipelinePlan &r_material_plan) {
#ifdef TESTS_ENABLED
	const bool trace_enabled = selection.backend == ExternalPacketPipelineRequest::Backend::MOBILE &&
			OS::get_singleton()->get_environment("ANGE_TEST_STAGED_MOBILE_PIPELINE_PHASE") == "1";
#endif
	auto traced = [&]([[maybe_unused]] const char *p_stage, Error p_error) {
#ifdef TESTS_ENABLED
		if (trace_enabled) {
			std::fprintf(stdout, "STAGED_MOBILE_PROSPECTIVE_PHASE stage=%s error=%d\n", p_stage, int(p_error));
			std::fflush(stdout);
		}
#endif
		return p_error;
	};
	// The lease is observed only by its actual owner; no caller root list or
	// copied material/generation origin is accepted as authority.
	MaterialStorage::NativeMaterialFuturePipelinePlan material_plan;
	Error error = _material_future_pipeline_plan(p_shader, p_uniform_set, material_plan);
	if (error != OK) {
		return traced("material_future_hold_plan", error);
	}
	if (!source_geometry || !owner || owner_slot >= owner->slot_count || !max_resources || !bank_groups_reserved ||
			material_plan.material.snapshot.shader != p_shader || material_plan.material.snapshot.uniform_set != p_uniform_set ||
			material_plan.material.snapshot.material_generation != source_material_generation || material_plan.pipeline.dependencies.size() != 1 ||
			material_plan.pipeline.dependencies[0] != p_shader || material_plan.pipeline.kind != RD::NativeProspectiveSymbol::FUTURE_RENDER_PIPELINE) {
		return traced("material_plan_identity", ERR_INVALID_DATA);
	}
	MeshStorage::NativeProspectiveGeometryPlan geometry_plan;
	error = MeshStorage::_native_hold_prospective_geometry(*source_geometry, p_input_mask, p_geometry, max_resources, geometry_plan);
	if (error != OK) {
		return traced("geometry_prospective", error);
	}
	// Consume the actual factory/cache identities returned by the retained
	// source observation. There is one derive in this Instance192 invocation;
	// directory and selected roots share ordinal1 for that exact Version.
	RD::VertexDescriptionKey raw_key{ geometry_plan.raw_vertex_description };
	RD::VertexDescriptionKey observed_key{ p_geometry.attributes };
	if (!geometry_plan.surface || geometry_plan.version_key.input_mask != p_input_mask ||
			geometry_plan.version_key.input_motion_vectors || geometry_plan.version_key.point_size_emulated ||
			geometry_plan.version_key.current_allocation_generation || geometry_plan.version_key.previous_allocation_generation ||
			!(raw_key == observed_key)) {
		return traced("geometry_plan_identity", ERR_INVALID_DATA);
	}
	RD::NativeProspectiveSymbol symbols[2];
	symbols[0] = material_plan.pipeline; // This row's one actual owned pipeline destination.
	uint32_t symbol_count = 1;
	if (geometry_plan.future_vertex_array) {
		symbols[1] = geometry_plan.vertex_array; // This retained Surface+Version factory, not descriptor equality.
		symbol_count = 2;
	}
	RD::NativeProspectiveHoldRoots holds[3];
	holds[0] = geometry_plan.initial;
	holds[1].roots = material_plan.roots;
	holds[1].limit = max_resources;
	uint32_t hold_count = 2;
	if (geometry_plan.detached) {
		// Finish acquires complete directory BEFORE releasing derived shares,
		// while material is already held. Prove the real transient three-hold peak.
		holds[2] = geometry_plan.directory;
		hold_count = 3;
	}
	Vector<RD::NativeProspectiveKey> geometry_roots = geometry_plan.initial.roots;
	if (geometry_plan.detached) {
		for (const auto &key : geometry_plan.directory.roots) {
			if (!geometry_roots.has(key) && geometry_roots.push_back(key) != OK) {
				return traced("geometry_roots_copy", ERR_OUT_OF_MEMORY);
			}
		}
	}
	auto *rd = RenderingDevice::get_singleton();
	auto *original_registry = owner->preparation_registry(*this);
	if (!rd || !pipeline_scope || !pipeline_pin || !original_registry || pipeline_original_root != &owned_pipeline_root ||
			!pipeline_owned_group || !geometry_reserved_group || !material_reserved_group || !rd->native_immutable_is_render_thread()) {
		return traced("original_preparation_scope", ERR_UNAUTHORIZED);
	}
	auto &registry = *original_registry;
	GDExtensionDeviceExit::Registry::Lock registry_lock(registry);
	GDExtensionDeviceExit::ProductionRequest *request = nullptr;
	error = registry.production_output_preparation_matches_locked(*pipeline_scope, *rd, request);
	if (error != OK || request != owner->preparation_request(owner_slot) || pipeline_pin != &owner->slots[owner_slot].original_pin ||
			!owner->preparation_source_matches(*this) || source_sealed || finish_complete || pipeline_sealed ||
			created_pipeline.is_valid() || owned_pipeline_root.is_valid() || geometry[0].is_active() || geometry[1].is_active() || material.is_active()) {
		return traced("original_preparation_validate", error != OK ? error : ERR_UNAUTHORIZED);
	}
	if (!rd->_thread_safe_.try_lock()) {
		return traced("rd_lock", ERR_BUSY);
	}
	struct Unlock {
		Mutex &mutex;
		~Unlock() { mutex.unlock(); }
	} unlock{ rd->_thread_safe_ };
	error = rd->_native_disposal_preflight();
	if (error != OK) {
		return traced("rd_preflight", error);
	}
	if (rd->native_immutable_device_escaped || rd->draw_graph.has_driver_callbacks() || rd->production_finalization_started.load(std::memory_order_acquire)) {
		return traced("recording_source_state", ERR_BUSY);
	}
	if (pipeline_owned_group == geometry_reserved_group || pipeline_owned_group == material_reserved_group || geometry_reserved_group == material_reserved_group) {
		return traced("owned_group_identity", ERR_INVALID_DATA);
	}
	// No writes to request.uses are used as scratch. Every original group and
	// its actual pin must remain entirely prepaid before the first factory.
	uint32_t group_index = UINT32_MAX;
	for (uint64_t group : { pipeline_owned_group, geometry_reserved_group, material_reserved_group }) {
		error = rd->_production_output_hold_reservation_locked(*request, owner->preparation_pin_serial(owner_slot), group,
				group == pipeline_owned_group ? 1 : max_resources, group_index);
		if (error != OK) {
			return traced("owned_hold_reservation", error);
		}
	}
	GDExtensionDeviceExit::ProductionUseCell *pipeline_destination = nullptr;
	error = rd->_production_output_owned_destination_locked(*request, pipeline_owned_group, 0, pipeline_destination);
	if (error != OK) {
		return traced("pipeline_destination", error);
	}
	// The original factory uses these SAME predicates. Prospective attributes
	// are real VF-key values; INVALID_ID is never replaced with an invented ID.
	RD::Shader *shader = nullptr;
	RD::FramebufferFormat framebuffer;
	RDD::VertexFormatID driver_format;
	// A missing Surface Version needs a VA even when its VF is already cached.
	// Select the validator mode from VF availability, independently of VA identity.
	error = rd->_native_render_pipeline_validate_values(p_shader, p_framebuffer, p_geometry.existing_vertex_format ? p_geometry.existing_vertex_format : RD::INVALID_ID, p_primitive,
			p_raster, p_multisample, p_depth, p_blend, 0, p_subpass, p_specialization, nullptr, RID(), nullptr, false,
			p_geometry.existing_vertex_format ? nullptr : &p_geometry.attributes, shader, framebuffer, driver_format);
	if (error != OK) {
		return traced("pipeline_value_validation", error);
	}
	// Both backends derive geometry[0] exactly once. Detached expansion
	// acquires a replacement before releasing that selected hold (peak 3),
	// then adoption transfers geometry/material without returning their counts.
	// Transforms and pipeline owned-seal each retain one more group: peak 4.
	// Owned-seal pins only its new member RID, not its dependency closure; its
	// fresh UniformSet/pipeline counters start at zero. Existing-resource shares
	// are therefore exactly the initial/material/directory closures below.
	error = rd->_native_immutable_hold_capacity_locked(4);
	if (error != OK) {
		return traced("hold_capacity", error);
	}
	error = rd->_native_prospective_hold_increments_locked({ holds, hold_count }, { symbols, symbol_count });
	if (error != OK) {
		return traced("hold_increments", error);
	}
#ifdef TESTS_ENABLED
	auto trace_closure_size = [&](const char *p_scope, Error p_original, const Vector<RD::NativeProspectiveKey> &p_roots, RD::NativeClosureMode p_mode) {
		if (!trace_enabled || p_original != ERR_OUT_OF_MEMORY) {
			return;
		}
		// The original registry and RD locks still cover this read-only retry.
		// It measures the same closure without adding any prepaid custody.
		constexpr uint32_t measurement_limit = 1024;
		Vector<RD::NativeProspectiveKey> measured;
		const Error measurement = rd->_native_prospective_closure_keys_locked(p_roots, { symbols, symbol_count }, measurement_limit, p_mode, measured);
		const int64_t required = measurement == OK ? measured.size() : -1;
		std::fprintf(stdout, "STAGED_MOBILE_CLOSURE_SIZE scope=%s error=%d original_limit=%u measurement_limit=%u required=%lld measurement_error=%d\n",
				p_scope, int(p_original), max_resources, measurement_limit, (long long)required, int(measurement));
		std::fflush(stdout);
	};
#endif
	Vector<RD::NativeProspectiveKey> geometry_nodes, material_nodes;
	error = rd->_native_prospective_closure_keys_locked(geometry_roots, { symbols, symbol_count }, max_resources, true, geometry_nodes);
	if (error != OK) {
#ifdef TESTS_ENABLED
		trace_closure_size("geometry", error, geometry_roots, RD::NativeClosureMode::BIDIRECTIONAL);
#endif
		return traced("closure_keys", error);
	}
	error = rd->_native_prospective_closure_keys_locked(material_plan.roots, { symbols, symbol_count }, max_resources, RD::NativeClosureMode::MATERIAL_DISPOSAL, material_nodes);
	if (error != OK) {
#ifdef TESTS_ENABLED
		trace_closure_size("material", error, material_plan.roots, RD::NativeClosureMode::MATERIAL_DISPOSAL);
#endif
		return traced("closure_keys", error);
	}
	if (geometry_plan.detached) {
		// Actual expansion requires the new complete hold to contain every old
		// selected share. Its own G bound is separate from initial derive256.
		Vector<RD::NativeProspectiveKey> initial_nodes, directory_nodes;
		error = rd->_native_prospective_closure_keys_locked(geometry_plan.initial.roots, { symbols, symbol_count }, 256, false, initial_nodes);
		if (error == OK) {
			error = rd->_native_prospective_closure_keys_locked(geometry_plan.directory.roots, { symbols, symbol_count }, max_resources, false, directory_nodes);
		}
		if (error != OK) {
			return traced("detached_closure_keys", error);
		}
		for (const auto &key : initial_nodes) {
			if (!directory_nodes.has(key)) {
				return traced("detached_geometry_identity", ERR_INVALID_DATA);
			}
		}
	}
	RD::NativeProspectiveMaterialOrigin origin{ source_material, source_material_generation };
	for (uint32_t group = 0; group < 2; ++group) {
		const auto &nodes = group ? material_nodes : geometry_nodes;
		for (const auto &key : nodes) {
			if (key.actual.is_null()) {
				continue; // Future nodes have no disposal head/incarnation yet.
			}
			RD::NativeProspectiveDisposalMetadata metadata;
			error = rd->_production_output_resource_preflight_locked(*request, key.actual, 0,
					{ group ? &origin : nullptr, group ? 1u : 0u }, metadata);
			if (error != OK) {
				return traced("actual_resource_preflight", error);
			}
		}
	}
	// Registry remains held through the final NewWork check. This proof is
	// observation only; actual derive/factory/acquire/finish retain custody.
	GDExtensionDeviceExit::ProductionRequest *current = nullptr;
	error = registry.production_output_preparation_matches_locked(*pipeline_scope, *rd, current);
	if (error != OK || current != request || !owner->preparation_source_matches(*this)) {
		return traced("final_original_preparation_validate", error != OK ? error : ERR_UNAUTHORIZED);
	}
	r_material_plan = std::move(material_plan);
	return traced("complete", OK);
}

Error ExternalPipelinePreparation::_prospective_execution_geometry_matches(const MeshStorage::NativeVertexArrayPlan &plan) {
	const Error current = _pipeline_preflight();
	if (current != OK) {
		return current;
	}
	auto *rd = RenderingDevice::get_singleton();
	if (!rd || !geometry[0].is_valid() || !rd->_thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	struct Unlock {
		Mutex &mutex;
		~Unlock() { mutex.unlock(); }
	} unlock{ rd->_thread_safe_ };
	const auto *array = rd->vertex_array_owner.get_or_null(geometry[0].get_vertex_array());
	RD::VertexDescriptionKey raw_key{ plan.attributes };
	const auto *format = rd->vertex_format_cache.getptr(raw_key);
	if (!array || !format || *format != geometry[0].get_vertex_format() || array->description != *format || array->vertex_count != plan.vertex_count ||
			(plan.existing_vertex_array.is_valid() && plan.existing_vertex_array != geometry[0].get_vertex_array()) ||
			(plan.existing_vertex_format && plan.existing_vertex_format != *format) || array->offsets.size() != plan.buffers.size() || (!plan.offsets.is_empty() && plan.offsets.size() != plan.buffers.size())) {
		return ERR_INVALID_DATA;
	}
	const auto *description = rd->vertex_formats.getptr(*format);
	if (!description || !description->driver_id) {
		return ERR_INVALID_DATA;
	}
	RD::VertexArray expected;
	HashSet<RID> used;
	const Error rebuilt = rd->_vertex_array_prepare_locked(plan.vertex_count, description->vertex_formats, plan.buffers, plan.offsets, expected, used);
	if (rebuilt != OK) {
		return rebuilt;
	}
	return array->buffers == expected.buffers && array->offsets == expected.offsets ? OK : ERR_INVALID_DATA;
}

Error ExternalPipelinePreparation::_pipeline_preflight() {
	if (finish_complete) {
		return ERR_ALREADY_IN_USE;
	}
	auto *rd = RenderingDevice::get_singleton();
	if (!rd || !pipeline_pin || !pipeline_owned_group || !pipeline_original_root) {
		return ERR_UNCONFIGURED;
	}
	// A row with a scope never falls back to an ambient or expired context.
	if (pipeline_scope && (pipeline_scope->row != this || source_sealed || pipeline_scope->pin != pipeline_pin || pipeline_scope->original_root != pipeline_original_root || pipeline_scope->group != pipeline_owned_group)) {
		return ERR_UNAUTHORIZED;
	}
	const Error error = pipeline_scope ? rd->_production_output_owned_pipeline_preflight(*pipeline_scope) : rd->_production_output_owned_pipeline_preflight(pipeline_context, *pipeline_pin, pipeline_owned_group);
	if (error != OK) {
		return error;
	}
	return pipeline_original_root->is_valid() ? ERR_ALREADY_IN_USE : OK;
}
Error ExternalPipelinePreparation::_cleanup_preflight() {
	auto *rd = RenderingDevice::get_singleton();
	if (!rd || !pipeline_scope || pipeline_scope->row != this || !pipeline_scope->registry) {
		return ERR_UNAUTHORIZED;
	}
	auto &registry = *pipeline_scope->registry;
	GDExtensionDeviceExit::Registry::Lock lock(registry);
	GDExtensionDeviceExit::ProductionRequest *request = nullptr;
	return registry.production_output_preparation_cleanup_matches_locked(*pipeline_scope, *rd, request);
}
RID ExternalPipelinePreparation::_create_pipeline(RID p_shader, RenderingDevice::FramebufferFormatID p_framebuffer_format, RenderingDevice::VertexFormatID p_vertex_format, RenderingDevice::RenderPrimitive p_render_primitive, const RenderingDevice::PipelineRasterizationState &p_rasterization_state, const RenderingDevice::PipelineMultisampleState &p_multisample_state, const RenderingDevice::PipelineDepthStencilState &p_depth_stencil_state, const RenderingDevice::PipelineColorBlendState &p_blend_state, BitField<RenderingDevice::PipelineDynamicStateFlags> p_dynamic_state_flags, uint32_t p_for_render_pass, const Vector<RenderingDevice::PipelineSpecializationConstant> &p_specialization_constants) {
	if (finish_complete || !pipeline_pin || !pipeline_owned_group || !pipeline_original_root || pipeline_original_root->is_valid()) {
		last_error = ERR_UNCONFIGURED;
		return RID();
	}
	auto *rd = RenderingDevice::get_singleton();
	if (!rd) {
		last_error = ERR_UNCONFIGURED;
		return RID();
	}
	if (pipeline_scope && (pipeline_scope->row != this || source_sealed || pipeline_scope->pin != pipeline_pin || pipeline_scope->original_root != pipeline_original_root || pipeline_scope->group != pipeline_owned_group)) {
		last_error = ERR_UNAUTHORIZED;
		return RID();
	}
	last_error = pipeline_scope ? rd->_production_output_owned_render_pipeline_create(*pipeline_scope, p_shader, p_framebuffer_format, p_vertex_format, p_render_primitive, p_rasterization_state, p_multisample_state, p_depth_stencil_state, p_blend_state, p_dynamic_state_flags, p_for_render_pass, p_specialization_constants, *pipeline_original_root) : rd->_production_output_owned_render_pipeline_create(pipeline_context, *pipeline_pin, pipeline_owned_group, 0, p_shader, p_framebuffer_format, p_vertex_format, p_render_primitive, p_rasterization_state, p_multisample_state, p_depth_stencil_state, p_blend_state, p_dynamic_state_flags, p_for_render_pass, p_specialization_constants, *pipeline_original_root);
	created_pipeline = *pipeline_original_root; // Original surface retains it even on a later leaf failure.
	return created_pipeline;
}
} // namespace RendererRD
