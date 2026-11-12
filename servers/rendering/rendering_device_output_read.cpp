/**************************************************************************/
/*  rendering_device_output_read.cpp                                      */
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
#include "servers/rendering/rendering_device.h"

#ifdef TESTS_ENABLED
#include "core/os/os.h"

#include <cstdio>
#endif

namespace {
void trace_staged_raster([[maybe_unused]] const char *stage, [[maybe_unused]] Error error,
		[[maybe_unused]] uint32_t first = 0, [[maybe_unused]] uint32_t second = 0) {
#ifdef TESTS_ENABLED
	const auto *os = OS::get_singleton();
	if (os && os->get_environment("ANGE_TEST_STAGED_MOBILE_PIPELINE_PHASE") == "1") {
		std::fprintf(stdout, "STAGED_MOBILE_RASTER_RECEIPT stage=%s error=%d first=%u second=%u\n", stage, int(error), first, second);
		std::fflush(stdout);
	}
#endif
}
bool same_read_list(const RDG::RecordHandle &a, const RDG::RecordHandle &b) {
	return a.is_valid() && b.is_valid() && a.graph_instance == b.graph_instance && a.graph_frame == b.graph_frame &&
			a.kind == b.kind && a.index == b.index && a.offset == b.offset;
}
} //namespace

bool RenderingDevice::_production_output_raster_permitted(RID rid) {
	using namespace GDExtensionDeviceExit;
	const auto &scope = production_output_raster_recording;
	if (!scope.owner || !scope.request || !draw_list.active || !same_read_list(scope.begin, draw_list.begin_handle) ||
			scope.read >= scope.owner->read_capacity) {
		return false;
	}
	const auto &read = scope.owner->read_cells[scope.read];
	if (!read.admitted || !read.list_linked || read.pin.is_empty() || read.pin.request != scope.request ||
			!same_read_list(read.list_begin, scope.begin) || read.slot >= scope.owner->slot_count) {
		return false;
	}
	const auto &slot = scope.owner->slots[read.slot];
	if (read.camera_source.is_null() || read.camera_source != slot.native_camera_source || !scope.request->capture_inflight) {
		return false;
	}
	uint32_t kind = UINT32_MAX;
	uint64_t incarnation = 0, driver_id = 0;
	const auto *head = _production_disposal_head(rid, kind, incarnation, driver_id);
	// Kind 10 is an RD vertex array. Its live owner supplies the RID as
	// the incarnation because the array has no standalone driver object.
	if (!head || !incarnation || (!driver_id && (kind != 10 || incarnation != rid.get_id()))) {
		return false;
	}
	for (const auto *use = *head; use; use = use->next) {
		if (use->request != scope.request || use->request_serial != scope.request->serial || use->rid != rid.get_id() ||
				use->kind != kind || use->serial != incarnation || use->native_driver_id != driver_id || use->state.load(std::memory_order_acquire) != 1) {
			continue;
		}
		bool group = false;
		for (uint32_t i = 0; i < 5; ++i) {
			group |= scope.groups[i] && use->group == scope.groups[i];
		}
		if (!group) {
			continue;
		}
		if (use->group == scope.groups[4] && rid != scope.storage_reads[0] && rid != scope.storage_reads[1] && rid != scope.storage_reads[2]) {
			return false;
		}
		// The exact original slot pin owns this group, including borrowed
		// immutable material/geometry incarnations. A numeric group alone fails.
		for (uint32_t i = scope.request->use_count + scope.request->binding_used; i < scope.request->capacity.resource_uses; ++i) {
			const auto &row = scope.request->uses[i];
			if (row.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP && row.disposal_group_serial == use->group &&
					row.disposal_group_pin_serial == slot.original_pin.serial && !row.disposal_group_release && !row.disposal_group_dropped) {
				return true;
			}
		}
	}
	return false;
}

Error RenderingDevice::_production_output_read_record(RendererRD::ExternalPacketBinding &binding, DrawListID list, const void *push, uint32_t push_bytes) {
	using namespace GDExtensionDeviceExit;
	using Owner = RendererRD::ExternalOutputOwner;
	if (!native_immutable_is_render_thread() || !binding.owner || binding.owner->device != this || !binding.owner->admits_work() ||
			!draw_list.active || list != (int64_t(ID_TYPE_DRAW_LIST) << ID_BASE_SHIFT) || _recording_error() != OK ||
			production_output_raster_recording.owner || production_output_guard_recording.request || production_recording_request ||
			native_height_active_checked_range.is_valid() || binding.read_cell >= binding.owner->read_capacity ||
			production_output_list_reads == UINT32_MAX) {
		return ERR_UNAVAILABLE;
	}
	if (!_thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	struct RdUnlock {
		Mutex &mutex;
		~RdUnlock() { mutex.unlock(); }
	} rd_unlock{ _thread_safe_ };
	auto &owner = *binding.owner;
	auto &read = owner.read_cells[binding.read_cell];
	if (read.consumer != &binding || read.serial != binding.serial || read.slot != binding.slot || read.slot >= owner.slot_count ||
			read.consumption_kind != Owner::ReadCell::ConsumptionKind::RASTER || binding.empty_packet ||
			read.preparation >= owner.preparation_capacity || read.list_linked || !draw_graph.contains_record(draw_list.begin_handle)) {
		return ERR_UNAUTHORIZED;
	}
	auto &slot = owner.slots[read.slot];
	auto &row = owner.preparation_rows[read.preparation];
	if (!slot.original_registry || !slot.storage || row.selected_geometry >= 2) {
		return ERR_UNAUTHORIZED;
	}
	const auto &geometry = row.geometry[row.selected_geometry];
	const auto &material = row.material.get_snapshot();
	const auto *pipeline = render_pipeline_owner.get_or_null(binding.pipeline);
	if (!pipeline || !pipeline->driver_id || binding.pipeline != row.created_pipeline || binding.vertex_array != geometry.get_retained_vertex_array() ||
			binding.index_array != geometry.get_retained_index_array() || binding.vertex_format != geometry.get_retained_vertex_format() ||
			binding.material != &material || binding.transforms_uniform_set != row.transforms_uniform_set || !(binding.selection == row.selection) ||
			binding.commands != slot.storage->resources[RendererRD::ExternalOutputStorage::PROTECTED_COMMANDS].rid ||
			binding.command_bytes != 20 || binding.command_offset != uint64_t(read.surface) * 20 || binding.command_offset > UINT32_MAX ||
			pipeline->shader != material.shader || pipeline->validation.framebuffer_format != draw_list_framebuffer_format ||
			pipeline->validation.render_pass != draw_list_current_subpass || pipeline->validation.vertex_format != binding.vertex_format || pipeline->set_formats.size() > MAX_UNIFORM_SETS || draw_list.state.set_count > uint32_t(pipeline->set_formats.size()) ||
			(push_bytes && !push) || (push && push_bytes != pipeline->push_constant_size) ||
			(!push && pipeline->push_constant_size && (draw_list.state.pipeline != binding.pipeline || !draw_list.state.checked_push_constant_supplied))) {
		return ERR_UNAUTHORIZED;
	}
	if (material.uniform_set_index >= MAX_UNIFORM_SETS || material.uniform_set_index == 2 || !binding.index_array.is_valid()) {
		return ERR_UNAVAILABLE; // This sealed command ABI is indexed DrawIndexedIndirect20.
	}
	// A material with no uniforms or textures may have no reflected set.
	if (!_native_immutable_uniform_set_matches_formats_locked(material.uniform_set, pipeline->set_formats, material.uniform_set_index)) {
		return material.uniform_set_index < uint32_t(pipeline->set_formats.size()) ? ERR_INVALID_DATA : ERR_UNAVAILABLE;
	}
	const auto *vertices = vertex_array_owner.get_or_null(binding.vertex_array);
	const auto *indices = index_array_owner.get_or_null(binding.index_array);
	const auto *commands = storage_buffer_owner.get_or_null(binding.commands);
	if (!vertices || !indices || !commands || binding.command_offset > commands->size || 20 > commands->size - binding.command_offset) {
		return ERR_INVALID_DATA;
	}
	const auto *shader = shader_owner.get_or_null(pipeline->shader);
	if (!shader || shader->driver_id != pipeline->shader_driver_id || shader->set_formats != pipeline->set_formats) {
		return ERR_INVALID_DATA;
	}
	uint64_t transfer_bound = vertices->transfer_worker_indices.size() + uint64_t(indices->transfer_worker_index >= 0) + uint64_t(commands->transfer_worker_index >= 0);
	for (uint32_t i = 0; i < pipeline->set_formats.size(); ++i) {
		if (!pipeline->set_formats[i]) {
			continue;
		}
		const RID set_rid = i == 2 ? binding.transforms_uniform_set : i == material.uniform_set_index ? material.uniform_set
																									  : draw_list.state.sets[i].uniform_set;
		const auto *set = uniform_set_owner.get_or_null(set_rid);
		if (!set || set->format != pipeline->set_formats[i] || _native_immutable_uniform_set_writes(*set)) {
			return ERR_INVALID_DATA;
		}
		for (const auto &entry : set->untracked_usage) {
			if (entry.value == RDG::RESOURCE_USAGE_TEXTURE_SAMPLE) {
				const auto *texture = texture_owner.get_or_null(entry.key);
				if (!texture) {
					return ERR_INVALID_DATA;
				}
				transfer_bound += texture->transfer_worker_index >= 0;
			}
		}
	}
	if (transfer_bound > Owner::ReadCell::TRANSFER_CAPACITY) {
		return ERR_OUT_OF_MEMORY;
	}
	if (production_output_list_head_owner && (production_output_list_head_read >= production_output_list_head_owner->read_capacity || !same_read_list(production_output_list_head_owner->read_cells[production_output_list_head_read].list_begin, draw_list.begin_handle))) {
		return ERR_BUSY; // Never splice different native list incarnations.
	}
	Registry &registry = *slot.original_registry;
	Registry::Lock lock(registry);
	const uint32_t graph_limit = uint32_t(pipeline->set_formats.size()) * 2 + 6;
	const Error admitted = registry.production_output_read_begin_locked(owner, binding.read_cell, graph_limit);
	trace_staged_raster("read_begin", admitted, graph_limit);
	if (admitted != OK) {
		return admitted;
	}
	auto &request = *read.pin.request;
	// The one shared close needs at most one still-live batch per worker.
	// Check the union across already linked reads of this original request
	// before this read can issue any bind/transfer effect.
	uint32_t worker_ids[32] = {}, worker_count = 0;
	auto add_worker = [&](int32_t worker) {
		if (worker < 0) {
			return true;
		}
		for (uint32_t i = 0; i < worker_count; ++i) {
			if (worker_ids[i] == uint32_t(worker)) {
				return true;
			}
		}
		if (worker_count >= MIN(32u, request.native_workers_bound)) {
			return false;
		}
		worker_ids[worker_count++] = uint32_t(worker);
		return true;
	};
	for (int32_t worker : vertices->transfer_worker_indices) {
		if (!add_worker(worker)) {
			return ERR_OUT_OF_MEMORY;
		}
	}
	if (!add_worker(indices->transfer_worker_index) || !add_worker(commands->transfer_worker_index)) {
		return ERR_OUT_OF_MEMORY;
	}
	for (uint32_t i = 0; i < pipeline->set_formats.size(); ++i) {
		if (!pipeline->set_formats[i]) {
			continue;
		}
		const RID set_rid = i == 2 ? binding.transforms_uniform_set : i == material.uniform_set_index ? material.uniform_set
																									  : draw_list.state.sets[i].uniform_set;
		const auto *set = uniform_set_owner.get_or_null(set_rid);
		for (const auto &entry : set->untracked_usage) {
			if (entry.value == RDG::RESOURCE_USAGE_TEXTURE_SAMPLE && !add_worker(texture_owner.get_or_null(entry.key)->transfer_worker_index)) {
				return ERR_OUT_OF_MEMORY;
			}
		}
	}
	auto *linked_owner = production_output_list_head_owner;
	uint32_t linked_index = production_output_list_head_read;
	for (uint32_t n = 0; n < production_output_list_reads && linked_owner; ++n) {
		if (linked_index >= linked_owner->read_capacity) {
			return ERR_INVALID_DATA;
		}
		const auto &linked = linked_owner->read_cells[linked_index];
		if (linked.pin.request == &request) {
			for (uint32_t t = 0; t < linked.transfer_count; ++t) {
				if (!add_worker(linked.transfers[t].worker)) {
					return ERR_OUT_OF_MEMORY;
				}
			}
		}
		linked_owner = linked.list_next_owner;
		linked_index = linked.list_next_read;
	}
	const OriginalOperationIdentity identity{ request.operation.owner_request_nonce, request.attachment->library_generation,
		registry.device_records[request.attachment->device].session, request.operation.record_generation,
		request.operation.canonical_begin, request.operation.canonical_end };
	// The finite original read cell is itself the active-list link. No new
	// directory/registry/allocation; exact begin handle disambiguates ID reuse.
	read.list_begin = draw_list.begin_handle;
	read.list_next_owner = production_output_list_head_owner;
	read.list_next_read = production_output_list_head_read;
	read.list_linked = true;
	production_output_list_head_owner = &owner;
	production_output_list_head_read = binding.read_cell;
	++production_output_list_reads;
	auto &scope = production_output_raster_recording;
	scope.owner = &owner;
	scope.read = binding.read_cell;
	scope.request = &request;
	scope.begin = read.list_begin;
	scope.groups[0] = row.pipeline_owned_group;
	scope.groups[1] = row.geometry_reserved_group;
	scope.groups[2] = row.material_reserved_group;
	scope.groups[3] = row.transforms_owned_group;
	scope.groups[4] = slot.owned_group;
	scope.storage_reads[0] = binding.commands;
	scope.storage_reads[1] = slot.storage->resources[RendererRD::ExternalOutputStorage::CURRENT_RECORDS].rid;
	scope.storage_reads[2] = slot.storage->resources[RendererRD::ExternalOutputStorage::PREVIOUS_RECORDS].rid;
	++request.capture_inflight;
	struct ScopeReturn {
		RenderingDevice &device;
		ProductionRequest &request;
		~ScopeReturn() {
			device.production_output_raster_recording = ProductionOutputRasterRecording();
			--request.capture_inflight;
		}
	} scope_return{ *this, request };
	auto fail_unknown = [&](Error error) {
		trace_staged_raster("read_unresolved", error, read.graph_count, read.raster_bound);
		read.capture_complete = false;
		read.uncaptured_effect = true;
		read.state = Owner::ReadCell::State::UNKNOWN;
		request.effects_pending = true;
		return error;
	};
	// Preflight every selected resource against its exact retained group before
	// the first bind. No guard or qualification recording flag is enabled.
	for (RID rid : { binding.pipeline, binding.vertex_array, binding.index_array, binding.transforms_uniform_set, material.shader, material.uniform_set, binding.commands }) {
		if (rid.is_valid() && !_production_output_raster_permitted(rid)) {
#ifdef TESTS_ENABLED
			uint32_t kind = UINT32_MAX;
			uint64_t incarnation = 0, driver_id = 0;
			_production_disposal_head(rid, kind, incarnation, driver_id);
			trace_staged_raster("selected_resource", ERR_UNAUTHORIZED, kind);
#endif
			const Error rejected = fail_unknown(ERR_UNAUTHORIZED);
			owner.read_return(binding, true);
			return rejected;
		}
	}
	auto capture_handle = [&](const RDG::RecordHandle &handle, bool draw, Error error = OK) {
		if (!handle.is_valid()) {
			return error;
		}
		CheckedRecordAppend append;
		bind_checked_record_append(append, identity);
		append.handle = handle;
		append.state = CheckedRecordAppend::State::Appended;
		append.error = error;
		const Error captured = registry.production_output_read_capture_locked(owner, binding.read_cell, append, draw);
		trace_staged_raster(draw ? "checked_draw" : "checked_prefix", captured, handle.kind, read.graph_count);
		return captured;
	};
	auto transfers = [&](const NativeTransferDependency *values, uint32_t count) {
		if (count > Owner::ReadCell::TRANSFER_CAPACITY - read.transfer_count) {
			return fail_unknown(ERR_OUT_OF_MEMORY);
		}
		for (uint32_t i = 0; i < count; ++i) {
			bool found = false;
			for (uint32_t j = 0; j < read.transfer_count; ++j) {
				found |= read.transfers[j].worker == values[i].worker && read.transfers[j].operation == values[i].operation;
			}
			if (!found) {
				read.transfers[read.transfer_count++] = values[i];
			}
		}
		return OK;
	};
	auto capture_draw = [&](const DrawAppendResult &native, bool draw) {
		for (uint32_t i = 0; i < native.prefix_count; ++i) {
			const Error error = capture_handle(native.prefix_handles[i], false);
			if (error != OK) {
				return error;
			}
		}
		if (native.has_transfer) {
			const Error error = transfers(&native.transfer, 1);
			if (error != OK) {
				return error;
			}
		}
		const Error captured = capture_handle(native.draw_handle, draw && native.error == OK, native.error);
		if (!native.capture_complete && native.effect != NativeAppendEffect::NoNewEffect) {
			return fail_unknown(native.error != OK ? native.error : ERR_INVALID_DATA);
		}
		return captured != OK ? captured : native.error;
	};
	NativeTransferDependency transfer_buffer[Owner::ReadCell::TRANSFER_CAPACITY];
	auto bind_geometry = [&](bool indices) {
		const BindAppendResult native = indices
				? _draw_list_bind_index_array_recorded(list, binding.index_array, transfer_buffer, Owner::ReadCell::TRANSFER_CAPACITY, true)
				: _draw_list_bind_vertex_array_recorded(list, binding.vertex_array, transfer_buffer, Owner::ReadCell::TRANSFER_CAPACITY, true);
		const Error enrolled = transfers(transfer_buffer, native.transfer_count);
		const Error captured = capture_handle(native.bind_handle, false, native.error);
		if (enrolled != OK || captured != OK || (!native.capture_complete && native.effect != NativeAppendEffect::NoNewEffect)) {
			return fail_unknown(enrolled != OK ? enrolled : captured != OK ? captured
																		   : ERR_INVALID_DATA);
		}
		return native.error;
	};
	Error error = capture_draw(_draw_list_bind_render_pipeline_recorded(list, binding.pipeline, true), false);
	if (error == OK) {
		error = bind_geometry(false);
	}
	if (error == OK && binding.index_array.is_valid()) {
		error = bind_geometry(true);
	}
	// Re-enroll existing renderer sets too: each deferred bind may expose real
	// sampled texture upload dependencies; none are hidden behind draw prefix.
	for (uint32_t i = 0; error == OK && i < pipeline->set_formats.size(); ++i) {
		if (!pipeline->set_formats[i]) {
			continue;
		}
		RID set = i == 2 ? binding.transforms_uniform_set : i == material.uniform_set_index ? material.uniform_set
																							: draw_list.state.sets[i].uniform_set;
		const BindAppendResult native = _draw_list_bind_uniform_set_recorded(list, set, i, true, transfer_buffer, Owner::ReadCell::TRANSFER_CAPACITY - read.transfer_count);
		error = transfers(transfer_buffer, native.transfer_count);
		if (error == OK) {
			error = native.error;
		}
	}
	if (error == OK && push) {
		error = capture_draw(_draw_list_set_push_constant_recorded(list, push, push_bytes, true), false);
	}
	if (error == OK) {
		// Only this exact scope, after actual checked bindings, may establish
		// raster provenance. An append handle/bool alone can never do so.
		read.raster_bound = draw_list.state.pipeline == binding.pipeline && draw_list.state.vertex_array == binding.vertex_array &&
				(binding.index_array.is_null() || draw_list.state.index_array == binding.index_array) &&
				draw_list.state.sets[2].uniform_set == binding.transforms_uniform_set &&
				(material.uniform_set.is_null() || draw_list.state.sets[material.uniform_set_index].uniform_set == material.uniform_set) &&
				(!pipeline->push_constant_size || draw_list.state.checked_push_constant_supplied);
		if (!read.raster_bound) {
			error = fail_unknown(ERR_INVALID_DATA);
		}
	}
	if (error == OK) {
		const auto native = _record_source_draw_indirect(identity, list, binding.index_array.is_valid(), binding.commands, uint32_t(binding.command_offset), 1, 20);
		error = capture_draw(native.native, true);
	}
	if (read.raster_bound) {
		for (uint32_t i = 0; i < read.graph_count; ++i) {
			request.graphs[read.graph_indices[i]].output_read = true;
		}
	}
	// Consume the short binding, preserving the original paid cell and pin for
	// actual close and original OBSERVE. Bool does not certify last use.
	trace_staged_raster("record_return", error, read.raster_bound, read.draw_accepted);
	const Error detached = owner.read_return(binding, true);
	return error != OK ? error : detached;
}

Error RenderingDevice::_production_output_empty_read_record(RendererRD::ExternalPacketBinding &binding, DrawListID list) {
	using namespace GDExtensionDeviceExit;
	using Owner = RendererRD::ExternalOutputOwner;
	using Storage = RendererRD::ExternalOutputStorage;
	using Config = RendererRD::ExternalOutputConfig;
	if (!native_immutable_is_render_thread() || !binding.owner || binding.owner->device != this || !binding.owner->admits_work() ||
			!draw_list.active || list != (int64_t(ID_TYPE_DRAW_LIST) << ID_BASE_SHIFT) || _recording_error() != OK ||
			production_output_raster_recording.owner || production_output_guard_recording.request || production_recording_request ||
			native_height_active_checked_range.is_valid() || binding.read_cell >= binding.owner->read_capacity ||
			production_output_list_reads == UINT32_MAX) {
		return ERR_UNAVAILABLE;
	}
	if (!_thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	struct RdUnlock {
		Mutex &mutex;
		~RdUnlock() { mutex.unlock(); }
	} rd_unlock{ _thread_safe_ };
	auto &owner = *binding.owner;
	auto &read = owner.read_cells[binding.read_cell];
	if (read.consumer != &binding || read.serial != binding.serial || read.slot != binding.slot || read.slot >= owner.slot_count ||
			read.consumption_kind != Owner::ReadCell::ConsumptionKind::EMPTY_COLOR || !binding.empty_packet ||
			read.preparation != UINT32_MAX || read.list_linked || !draw_graph.contains_record(draw_list.begin_handle)) {
		return ERR_UNAUTHORIZED;
	}
	auto &slot = owner.slots[read.slot];
	const uint32_t pass = read.empty_color_pass == uint32_t(RendererRD::ExternalPacketPass::OPAQUE) ? Config::PASS_OPAQUE :
			read.empty_color_pass == uint32_t(RendererRD::ExternalPacketPass::TRANSPARENT) ? Config::PASS_TRANSPARENT : 0;
	if (owner.config.profile != Config::INSTANCE_192 || owner.config.capacity != 0 || !slot.original_registry || !slot.storage ||
			!slot.guard_resources_ready || slot.publication_closed || slot.pass_uses_closed || read.surface >= slot.surface_count ||
			(slot.state != Owner::LeaseState::SEALED_RECORDED && slot.state != Owner::LeaseState::ADOPTED) ||
			!pass || !(owner.config.pass_mask & pass) || !(slot.request.pass_mask & pass) ||
			uint32_t(binding.selection.pass) != read.empty_color_pass || binding.selection.framebuffer_format != draw_list_framebuffer_format ||
			binding.selection.subpass != draw_list_current_subpass || !read.camera_retained || read.camera_source.is_null() ||
			read.camera_source != slot.native_camera_source || binding.pipeline.is_valid() || binding.vertex_array.is_valid() ||
			binding.index_array.is_valid() || binding.transforms_uniform_set.is_valid() || binding.material || binding.commands.is_valid() ||
			binding.command_bytes || binding.command_offset || read.raster_bound || read.draw_accepted || read.transfer_count) {
		return ERR_UNAUTHORIZED;
	}
	const auto *framebuffer = framebuffer_formats.getptr(draw_list_framebuffer_format);
	if (!framebuffer || !framebuffer->E || framebuffer->view_count != binding.selection.view_count || binding.selection.view_count != 1 ||
			draw_list_current_subpass >= uint32_t(framebuffer->E->key().passes.size())) {
		return ERR_UNAUTHORIZED;
	}
	bool color_attachment = false;
	for (int32_t attachment : framebuffer->E->key().passes[draw_list_current_subpass].color_attachments) {
		color_attachment |= attachment != ATTACHMENT_UNUSED;
	}
	if (!color_attachment) {
		return ERR_UNAUTHORIZED;
	}
	const auto &current = slot.storage->resources[Storage::CURRENT_RECORDS];
	const auto &previous = slot.storage->resources[Storage::PREVIOUS_RECORDS];
	const auto &commands = slot.storage->resources[Storage::PROTECTED_COMMANDS];
	const auto &surface = slot.surfaces[read.surface];
	if (current.rid.is_valid() || current.bytes || current.witness.cell.is_valid() || previous.rid.is_valid() || previous.bytes || previous.witness.cell.is_valid() || commands.rid.is_null() ||
			surface.command_extent != 20 || surface.command_offset != uint64_t(read.surface) * 20 ||
			surface.command_offset > commands.bytes || surface.command_extent > commands.bytes - surface.command_offset) {
		return ERR_INVALID_DATA;
	}
	if (production_output_list_head_owner && (production_output_list_head_read >= production_output_list_head_owner->read_capacity ||
			!same_read_list(production_output_list_head_owner->read_cells[production_output_list_head_read].list_begin, draw_list.begin_handle))) {
		return ERR_BUSY;
	}
	if (!owner.generation_materials || read.surface >= owner.generation_material_count) {
		return ERR_UNAUTHORIZED;
	}
	const auto &source = owner.generation_materials[read.surface];
	RendererRD::MaterialStorage::NativeSelectionShaderObservation classification;
	auto *materials = RendererRD::MaterialStorage::get_singleton();
	const Error classified = materials ? materials->material_selection_get_shader_observation(source.lease, classification) : ERR_UNAVAILABLE;
	if (classified != OK) {
		return classified;
	}
	if (classification.material != source.material || classification.material_generation != source.generation ||
			classification.shader_version.is_null() || !(classification.output_pass_mask & pass)) {
		return ERR_UNAUTHORIZED;
	}
	Registry &registry = *slot.original_registry;
	Registry::Lock lock(registry);
	// Core authenticates the original action, complete guard, slot/root/lease,
	// source, capacity-zero configuration and quoted one-close graph budget.
	const Error admitted = registry.production_output_read_begin_locked(owner, binding.read_cell, 1);
	if (admitted != OK) {
		return admitted;
	}
	// Participate in the actual original color list. No raster bindings, push
	// constants, draw instruction, transfer dependency or synthetic handle.
	read.list_begin = draw_list.begin_handle;
	read.list_next_owner = production_output_list_head_owner;
	read.list_next_read = production_output_list_head_read;
	read.list_linked = true;
	production_output_list_head_owner = &owner;
	production_output_list_head_read = binding.read_cell;
	++production_output_list_reads;
	// Preserve the original cell/pin until real kind-7 close and OBSERVE.
	return owner.read_return(binding, true);
}

Error RenderingDevice::_production_output_reads_close() {
	using namespace GDExtensionDeviceExit;
	if (!native_immutable_is_render_thread() || !draw_list.active || production_output_raster_recording.owner || !_thread_safe_.try_lock()) {
		return ERR_UNAVAILABLE;
	}
	struct RdUnlock {
		Mutex &mutex;
		~RdUnlock() { mutex.unlock(); }
	} rd_unlock{ _thread_safe_ };
	const auto begin = draw_list.begin_handle;
	// The sole native end. Every read below receives this same actual receipt.
	const DrawAppendResult native = _draw_list_end_recorded(true);
	trace_staged_raster("native_list_end", native.error, native.draw_handle.kind, native.capture_complete);
	Error result = native.error;
	if (draw_list.active && native.effect == NativeAppendEffect::NoNewEffect) {
		return result; // No end happened. Preserve the exact list links for retry.
	}
	auto *owner = production_output_list_head_owner;
	uint32_t index = production_output_list_head_read;
	const uint32_t count = production_output_list_reads;
	production_output_list_head_owner = nullptr;
	production_output_list_head_read = UINT32_MAX;
	production_output_list_reads = 0;
	for (uint32_t visited = 0; visited < count && owner; ++visited) {
		if (index >= owner->read_capacity) {
			return ERR_INVALID_DATA;
		}
		auto &read = owner->read_cells[index];
		auto *next_owner = read.list_next_owner;
		const uint32_t next_read = read.list_next_read;
		if (!read.list_linked || read.slot >= owner->slot_count || !same_read_list(begin, read.list_begin)) {
			trace_staged_raster("list_begin_mismatch", ERR_INVALID_DATA, begin.kind, read.list_begin.kind);
			return ERR_INVALID_DATA;
		}
		auto &slot = owner->slots[read.slot];
		Registry &registry = *slot.original_registry;
		Registry::Lock lock(registry);
		auto &request = *read.pin.request;
		CheckedRecordAppend close;
		bind_checked_record_append(close, { request.operation.owner_request_nonce, request.attachment->library_generation, registry.device_records[request.attachment->device].session, request.operation.record_generation, request.operation.canonical_begin, request.operation.canonical_end });
		close.handle = native.draw_handle;
		close.error = native.error;
		close.state = native.capture_complete && native.effect == NativeAppendEffect::Recorded ? CheckedRecordAppend::State::Appended : CheckedRecordAppend::State::Unknown;
		const Error captured = registry.production_output_read_close_locked(*owner, index, close);
		trace_staged_raster("checked_list_close", captured, read.graph_count, read.draw_accepted);
		if (captured == OK && read.close_graph < request.graph_count) {
			auto &graph = request.graphs[read.close_graph];
			for (uint32_t t = 0; t < read.transfer_count; ++t) {
				const auto &dependency = read.transfers[t];
				if (dependency.worker >= transfer_worker_pool.size() || !transfer_worker_pool[dependency.worker]) {
					result = ERR_INVALID_DATA;
					break;
				}
				uint32_t entry = UINT32_MAX;
				for (uint32_t k = 0; k < graph.output_read_transfer_count; ++k) {
					if (graph.output_read_transfers[k].worker == dependency.worker) {
						entry = k;
						break;
					}
				}
				if (entry == UINT32_MAX) {
					if (graph.output_read_transfer_count == 32) {
						request.effects_pending = true;
						result = ERR_OUT_OF_MEMORY;
						break;
					}
					entry = graph.output_read_transfer_count++;
					graph.output_read_transfers[entry] = { dependency.worker, dependency.operation, true };
				}
				if (dependency.operation > graph.output_read_transfers[entry].operation) {
					graph.output_read_transfers[entry].operation = dependency.operation;
					graph.output_read_transfers[entry].pending = true;
				}
				auto *worker = transfer_worker_pool[dependency.worker];
				// Enroll the raw dependency before trying original worker locks.
				// Contention leaves a retryable paid fact, never a lost prefix.
				if (!worker->thread_mutex.try_lock()) {
					continue;
				}
				if (!worker->operations_mutex.try_lock()) {
					worker->thread_mutex.unlock();
					continue;
				}
				if (worker->operations_processed >= graph.output_read_transfers[entry].operation) {
					graph.output_read_transfers[entry].pending = false; // Original fence-owner physical retirement, before this read.
				} else if (worker->submitted && graph.output_read_transfers[entry].operation <= worker->operations_submitted) {
					registry.production_capture_issue_locked(*this, UINT32_MAX, { worker->last_submission }, worker->last_submission.error,
							worker->index, worker->operations_processed + 1, worker->operations_submitted);
					worker->production_observer_pending = true;
				}
				worker->operations_mutex.unlock();
				worker->thread_mutex.unlock();
			}
		}
		if (captured == OK && !request.effects_pending) {
			registry.production_output_read_adopt_locked(*owner, index);
		}
		if (captured != OK && result == OK) {
			result = captured;
		}
		read.list_linked = false;
		read.list_next_owner = nullptr;
		read.list_next_read = UINT32_MAX;
		owner = next_owner;
		index = next_read;
	}
	return owner ? ERR_INVALID_DATA : result;
}
