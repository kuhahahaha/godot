/**************************************************************************/
/*  gdextension_production_action.cpp                                     */
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
#include "core/os/thread.h"
#include "servers/rendering/rendering_device.h"

namespace GDExtensionDeviceExit {
Error Registry::production_dispatch_closure_locked(const ProductionRequest &request, RenderingDevice &rd, const ProductionActionCell &action, const GDExtensionProductionDispatchV2 &dispatch) const {
	if (dispatch.unit_slot >= request.resource_count || dispatch.first_set_slot > request.resource_count || dispatch.set_count > request.resource_count - dispatch.first_set_slot || dispatch.set_count > RenderingDevice::MAX_UNIFORM_SETS) {
		return ERR_INVALID_DATA;
	}
	const auto &unit = request.resources[dispatch.unit_slot];
	const auto *pipeline = rd.compute_pipeline_owner.get_or_null(unit.primary);
	const auto *shader = rd.shader_owner.get_or_null(unit.secondary);
	if (!unit.imported || !pipeline || !shader || !pipeline->driver_id || !pipeline->shader_driver_id || pipeline->shader != unit.secondary || pipeline->push_constant_size != dispatch.push_bytes || pipeline->set_formats.size() != dispatch.set_count || shader->uniform_sets.size() != dispatch.set_count) {
		return ERR_INVALID_DATA;
	}
	const auto &declaration = action.declaration;
	if (declaration.first_use > request.use_count || declaration.use_count > request.use_count - declaration.first_use) {
		return ERR_INVALID_DATA;
	}
	auto compatible = [&](const GDExtensionProductionSetBindingV1 &binding, const RenderingDevice::ShaderUniform &uniform, const GDExtensionProductionUseV1 &use) {
		// Binding spans are physical windows; sealed uses are logical accesses.
		// A writable alias's use must not be imposed on a readonly alias.
		return uniform.binding == binding.binding && uniform.type == RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER &&
				use.resource_slot == binding.resource_slot && use.byte_size && use.byte_offset >= binding.offset &&
				use.byte_offset - binding.offset <= binding.bytes && use.byte_size <= binding.bytes - (use.byte_offset - binding.offset) &&
				(uniform.writable || use.access == GDEXTENSION_PRODUCTION_READ_V1);
	};
	for (uint32_t i = 0; i < dispatch.set_count; ++i) {
		const auto &slot = request.resources[dispatch.first_set_slot + i];
		const auto *set = rd.uniform_set_owner.get_or_null(slot.primary);
		const bool deferred = !request.frozen && slot.output_set_deferred && !slot.imported && !set && slot.owned_serial && !slot.creating && !slot.retiring;
		if ((!deferred && (!slot.imported || !set || !set->driver_id || set->shader_id != pipeline->shader || set->format != pipeline->set_formats[i] || !set->shared_textures_to_update.is_empty() || !set->pending_clear_textures.is_empty())) || slot.imported_unit_slot != dispatch.unit_slot || slot.shader_set != i || slot.binding_count != shader->uniform_sets[i].size() || slot.first_binding > request.capacity.resource_uses || slot.binding_count > request.capacity.resource_uses - slot.first_binding) {
			return ERR_INVALID_DATA;
		}
		for (uint32_t b = 0; b < slot.binding_count; ++b) {
			const auto &binding = request.uses[slot.first_binding + b].binding;
			if (binding.resource_slot >= request.resource_count || binding.set != i || binding.array_element) {
				return ERR_INVALID_DATA;
			}
			const auto &storage = request.resources[binding.resource_slot];
			bool physical_dependency = storage.use_count == 1 && storage.uses[0].linked() && storage.uses[0].same_physical() && (deferred || set->qualification_dependencies.has(storage.uses[0].physical));
			if (storage.output_alias_prepared) {
				physical_dependency = deferred && !storage.imported && storage.output_action_ordinal < action.declaration.action_ordinal && storage.output_action_ordinal < request.action_count && request.actions[storage.output_action_ordinal].prepared;
				if (!deferred && production_output_alias_matches_locked(request, storage, rd)) {
					for (const auto &dependency : set->production_output_dependencies) {
						physical_dependency |= dependency.cell == storage.output_witness.cell;
					}
				}
			}
			if ((!storage.imported && !(deferred && storage.output_alias_prepared)) || storage.retiring || storage.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || !physical_dependency) {
				return ERR_INVALID_DATA;
			}
			bool covered = false;
			for (const auto &uniform : shader->uniform_sets[i]) {
				for (uint32_t u = 0; u < declaration.use_count; ++u) {
					covered |= compatible(binding, uniform, request.uses[declaration.first_use + u].declaration);
				}
			}
			if (!covered) {
				return ERR_INVALID_DATA;
			}
		}
	}
	// Existence for each binding alone could silently drop an incompatible
	// write or out-of-window use. Every sealed storage use needs a match too.
	// UNIT/SET metadata associations retain the original reservation checks;
	// their actual owners/identities are validated above, not as SSBO spans.
	for (uint32_t u = 0; u < declaration.use_count; ++u) {
		const auto &use = request.uses[declaration.first_use + u].declaration;
		if (use.resource_slot >= request.resource_count) {
			return ERR_INVALID_DATA;
		}
		if (request.resources[use.resource_slot].declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1) {
			continue;
		}
		bool covered = false;
		for (uint32_t i = 0; i < dispatch.set_count; ++i) {
			const auto &slot = request.resources[dispatch.first_set_slot + i];
			for (uint32_t b = 0; b < slot.binding_count; ++b) {
				const auto &binding = request.uses[slot.first_binding + b].binding;
				for (const auto &uniform : shader->uniform_sets[i]) {
					covered |= compatible(binding, uniform, use);
				}
			}
		}
		if (!covered) {
			return ERR_INVALID_DATA;
		}
	}
	return OK;
}

Error Registry::production_capture_graph_locked(ProductionRequest &request, RenderingDevice &rd, uint64_t ordinal, const CheckedRecordAppend &append) {
	const auto &handle = append.handle;
	if (!append.operation_bound || append.operation.owner_request_nonce != request.operation.owner_request_nonce || append.operation.library_generation != request.attachment->library_generation || append.operation.record_generation != request.operation.record_generation || append.operation.record_range_begin != request.operation.canonical_begin || append.operation.record_range_end != request.operation.canonical_end) {
		return ERR_INVALID_DATA;
	}
	if (!request.frozen || !handle.is_valid() || request.graph_count >= request.capacity.graph_memberships || uint32_t(rd.frame) >= rd.frames.size() || !rd.frames[rd.frame].qualification_epoch || !rd.draw_graph.contains_record(handle)) {
		return ERR_INVALID_DATA;
	}
	if (ordinal >= request.action_count || request.actions[ordinal].request != &request || request.actions[ordinal].declaration.action_ordinal != ordinal) {
		return ERR_INVALID_DATA;
	}
	// A validated real action graph is append evidence. Failed preflight or
	// merely creating a resource must not be reclassified as business append.
	production_append_effect_enter_locked(request);
	const auto &declaration = request.actions[ordinal].declaration;
	for (uint32_t i = 0; i < request.graph_count; ++i) {
		const auto &old = request.graphs[i];
		if (old.handle.graph_instance == handle.graph_instance && old.handle.graph_frame == handle.graph_frame && old.handle.kind == handle.kind && old.handle.index == handle.index && old.handle.offset == handle.offset) {
			return old.fact.action_ordinal == ordinal && old.fact.canonical_suboperation == declaration.canonical_suboperation && old.fact.source_recipe_substep == declaration.source_recipe_substep ? OK : ERR_INVALID_DATA;
		}
	}
	auto &cell = request.graphs[request.graph_count++];
	cell.handle = handle;
	cell.device_frame = uint32_t(rd.frame);
	cell.device_epoch = rd.frames[rd.frame].qualification_epoch;
	cell.pending = true;
	cell.fact.action_ordinal = ordinal;
	cell.fact.canonical_suboperation = declaration.canonical_suboperation;
	cell.fact.source_recipe_substep = declaration.source_recipe_substep;
	cell.fact.graph_instance = handle.graph_instance;
	cell.fact.graph_frame = handle.graph_frame;
	cell.fact.graph_kind = handle.kind;
	cell.fact.graph_index = handle.index;
	cell.fact.graph_offset = handle.offset;
	cell.fact.command_slot = UINT32_MAX;
	cell.fact.membership_parent = UINT32_MAX;
	cell.fact.phase = 1;
	++request.revision;
	return OK;
}

void Registry::production_seal_graphs_locked(RenderingDevice &rd) {
	for (auto *attachment : production_attachments) {
		if (!attachment || attachment->held_device != &rd) {
			continue;
		}
		for (uint32_t r = 0; r < attachment->quote.capacity.requests; ++r) {
			auto &request = attachment->requests[r];
			if (!request.serial) {
				continue;
			}
			for (uint32_t g = 0; g < request.graph_count; ++g) {
				auto &cell = request.graphs[g];
				if (!cell.pending || cell.fact.phase >= 2 || cell.device_frame != uint32_t(rd.frame) || cell.device_epoch != rd.frames[rd.frame].qualification_epoch) {
					continue;
				}
				uint32_t slot = UINT32_MAX;
				if (rd.draw_graph.record_mapping_state(cell.handle) != RDG::RecordMappingState::Recorded || !rd.draw_graph.resolve_record_command_buffer_slot(cell.handle, slot)) {
					cell.fact.issue_error = FAILED;
					request.effects_pending = true;
				} else {
					cell.fact.command_slot = slot;
					cell.fact.phase = 2;
					// Source issue has not occurred; mapping alone is no native fact.
				}
				++request.revision;
			}
		}
	}
}

void Registry::production_capture_issue_locked(RenderingDevice &rd, uint32_t slot, const NativeSourceIssueView &view, Error error, uint32_t worker, uint64_t first_operation, uint64_t last_operation) {
	const auto &report = view.report;
	// A supported preparation phase may perform no native issue (for example
	// Vulkan). Its empty successful report is no new membership or prefix.
	if (slot == UINT32_MAX && worker == UINT32_MAX && report.calls.is_empty() && !report.issued && (error == OK || error == ERR_BUSY) && (report.error == OK || report.error == ERR_BUSY)) {
		return;
	}
	for (auto *attachment : production_attachments) {
		if (!attachment || attachment->held_device != &rd) {
			continue;
		}
		for (uint32_t r = 0; r < attachment->quote.capacity.requests; ++r) {
			auto &request = attachment->requests[r];
			if (!request.serial) {
				continue;
			}
			for (uint32_t g = 0; g < request.graph_count; ++g) {
				auto &graph = request.graphs[g];
				bool transfer = false;
				if (worker != UINT32_MAX) {
					for (uint32_t u = 0; u < request.resource_count; ++u) {
						const auto &resource = request.resources[u];
						transfer |= resource.transfer_pending && resource.transfer_worker == worker && resource.transfer_operation >= first_operation && resource.transfer_operation <= last_operation && resource.transfer_action_ordinal == graph.fact.action_ordinal;
					}
				}
				const bool hidden = worker == UINT32_MAX && slot == UINT32_MAX;
				if (graph.device_frame != uint32_t(rd.frame) || graph.device_epoch != rd.frames[rd.frame].qualification_epoch || (!transfer && !hidden && (graph.fact.phase < 2 || graph.fact.command_slot != slot))) {
					continue;
				}
				const uint32_t native_before = request.native_count;
				const bool pending_before = graph.pending;
				const int32_t error_before = graph.fact.issue_error;
				bool captured = !report.calls.is_empty();
				for (const auto &source : report.calls) {
					// A source fact comes only from this real command-slot issue.
					// Aliases preserve every edge; only one row owns retain/release.
					if (source.presentation_only || !source.serial || !source.queue_index || !source.completion_serial || source.completion == RDD::CompletionCapability::Disabled) {
						captured = false;
						continue;
					}
					uint32_t owner = UINT32_MAX;
					bool duplicate = false;
					for (uint32_t n = 0; n < request.native_count; ++n) {
						const auto &old = request.natives[n];
						if (old.member.source.serial == source.serial && old.member.source.queue_index == source.queue_index && old.member.source.completion_serial == source.completion_serial) {
							const auto &prior = old.member.source;
							if (prior.issue != source.issue || prior.error != source.error || prior.native_code != source.native_code || prior.completion != source.completion || prior.present != source.present || prior.presentation_only != source.presentation_only || prior.source_owner_returned != source.source_owner_returned || prior.frame_observer_owned != source.frame_observer_owned) {
								captured = false;
								duplicate = true;
								break;
							}
							owner = old.owner_index;
							duplicate |= old.fact.membership_parent == g;
						}
					}
					if (duplicate) {
						continue;
					}
					if (request.native_count == request.capacity.native_memberships) {
						captured = false;
						continue;
					}
					const uint32_t index = request.native_count++;
					auto &native = request.natives[index];
					native.owner_index = owner == UINT32_MAX ? index : owner;
					native.member.source = source;
					native.member.frame = uint32_t(rd.frame);
					native.member.epoch = graph.device_epoch;
					native.member.slot = slot;
					native.member.fact = index;
					native.fact = graph.fact;
					native.fact.membership_parent = g;
					native.fact.backend_queue = source.queue_index;
					native.fact.issue = uint32_t(source.issue);
					native.fact.phase = 3;
					native.fact.native_serial = source.serial;
					native.fact.completion_serial = source.completion_serial;
					native.fact.issue_native_code = source.native_code;
					native.fact.issue_error = source.error;
				}
				if (!transfer && !hidden) {
					graph.pending = !captured;
				} else if (transfer && captured) {
					for (uint32_t u = 0; u < request.resource_count; ++u) {
						auto &resource = request.resources[u];
						if (resource.transfer_worker == worker && resource.transfer_operation >= first_operation && resource.transfer_operation <= last_operation && resource.transfer_action_ordinal == graph.fact.action_ordinal) {
							resource.transfer_pending = false;
						}
					}
				}
				if (!captured || (error != OK && error != ERR_BUSY) || report.error != OK) {
					request.effects_pending |= !captured;
					graph.fact.issue_error = error != OK ? error : report.error != OK ? report.error
																					  : FAILED;
				}
				if (native_before != request.native_count || pending_before != graph.pending || error_before != graph.fact.issue_error) {
					CRASH_COND(request.revision == UINT64_MAX);
					++request.revision;
				}
			}
		}
	}
}

Error Registry::production_retain_sources_locked(RenderingDevice &rd, uint32_t frame, uint64_t epoch, const NativeSourceIssueView *view) {
	for (auto *attachment : production_attachments) {
		if (!attachment || attachment->held_device != &rd) {
			continue;
		}
		for (uint32_t r = 0; r < attachment->quote.capacity.requests; ++r) {
			auto &request = attachment->requests[r];
			if (!request.serial) {
				continue;
			}
			// An uncaptured issue prefix must keep its original frame owner alive;
			// absence of a metadata row is never proof that no source exists.
			if (request.effects_pending) {
				for (uint32_t g = 0; g < request.graph_count; ++g) {
					if (request.graphs[g].device_frame == frame && request.graphs[g].device_epoch == epoch) {
						return FAILED;
					}
				}
			}
			for (uint32_t n = 0; n < request.native_count; ++n) {
				auto &native = request.natives[n];
				if (native.owner_index != n || native.member.retained || native.member.owner_released || native.member.source.issue == RDD::IssueFact::NotIssued) {
					continue;
				}
				bool selected = !view && native.member.frame == frame && native.member.epoch == epoch;
				if (view) {
					for (const auto &source : view->report.calls) {
						selected |= source.serial == native.member.source.serial && source.queue_index == native.member.source.queue_index && source.completion_serial == native.member.source.completion_serial;
					}
				}
				if (!selected) {
					continue;
				}
				if (!rd._native_disposal_admit(RenderingDevice::NativeDisposalWork::RETAIN)) {
					return ERR_BUSY;
				}
				const uint64_t serial = request.serial;
				auto member = native.member;
				++request.capture_inflight;
				mutex.unlock();
				const Error error = retain_native_source(*rd.driver, member);
				mutex.lock();
				CRASH_COND(request.serial != serial || !request.capture_inflight || native.request != &request);
				native.member = member;
				--request.capture_inflight;
				native.observation.retention_error = error;
				if (error != OK) {
					native.observation.state = NativeSourceObservation::State::Unknown;
					return error;
				}
				CRASH_COND(request.revision == UINT64_MAX);
				++request.revision;
			}
		}
	}
	return OK;
}

// Only the original authenticated record-window receiver may invoke this
// private mechanical tail in its checked physical scope. Original B/Runtime
// logical operation and window authority remain separate.
CheckedRecordAppend Registry::production_record_effect_locked(ProductionRequest &request, RenderingDevice &rd, uint32_t index, const OriginalOperationIdentity &identity) {
	CheckedRecordAppend observed;
	observed.error = bind_checked_record_append(observed, identity);
	if (observed.error != OK) {
		return observed;
	}
	if (identity.owner_request_nonce != request.operation.owner_request_nonce || identity.library_generation != request.attachment->library_generation || identity.record_generation != request.operation.record_generation || identity.record_range_begin != request.operation.canonical_begin || identity.record_range_end != request.operation.canonical_end || request.attachment->held_device != &rd || index != request.next_action || index >= request.action_count || request.capture_inflight || request.effects_pending || request.source_device_lost || rd.production_recording_request || rd.render_thread_id != Thread::get_caller_id()) {
		observed.error = ERR_UNAUTHORIZED;
		return observed;
	}
	observed.error = production_freeze_locked(request);
	if (observed.error != OK) {
		return observed;
	}
	if (request.segment.segment != request.acknowledged_segment) {
		observed.error = ERR_BUSY;
		return observed;
	}
	const uint32_t later_actions = request.action_count - index - 1;
	if (request.graph_count >= request.capacity.graph_memberships || later_actions >= request.capacity.graph_memberships - request.graph_count || request.revision > UINT64_MAX - uint64_t(request.capacity.graph_memberships) * 2 - uint64_t(request.capacity.native_memberships) * 2 - request.capacity.readbacks - request.capacity.resources - 8 || rd.draw_list.active || rd.raytracing_list.active || rd.compute_list.active != request.compute_list_open || rd._recording_error() != OK) {
		observed.error = ERR_BUSY;
		return observed;
	}
	// Pending inherited wrapper sources can change after reservation. Recheck
	// all still-unissued graph edges before entering any actual append effect.
	uint32_t uncaptured_graphs = request.capacity.graph_memberships - request.graph_count;
	for (uint32_t g = 0; g < request.graph_count; ++g) {
		uncaptured_graphs += request.graphs[g].pending;
	}
	uint32_t memberships = 0;
	observed.error = production_membership_bound_locked(rd, uncaptured_graphs, request.native_workers_bound, memberships);
	if (observed.error != OK) {
		return observed;
	}
	if (request.native_count > request.capacity.native_memberships || memberships > request.capacity.native_memberships - request.native_count) {
		observed.error = ERR_PARAMETER_RANGE_ERROR;
		return observed;
	}
	auto &action = request.actions[index];
	const uint32_t first_graph = request.graph_count;
	const uint32_t room = request.capacity.graph_memberships - first_graph - later_actions;
	BufferUploadSourceCapture upload;
	upload.segments = &request.graphs[first_graph].captured;
	upload.segment_stride = sizeof(ProductionGraphCell);
	upload.capacity = room;
	upload.consumed = action.recorded_bytes;
	BufferReadbackSourceCapture *readback = nullptr;
	RID pipeline;
	if (action.declaration.action_kind == GDEXTENSION_PRODUCTION_DISPATCH_V1) {
		const auto &dispatch = action.dispatch;
		pipeline = request.resources[dispatch.unit_slot].primary;
		observed.error = production_dispatch_closure_locked(request, rd, action, dispatch);
		if (observed.error != OK) {
			return observed;
		}
	} else if (action.declaration.action_kind == GDEXTENSION_PRODUCTION_READBACK_V1) {
		if (action.readback_slot >= request.readback_count) {
			observed.error = ERR_INVALID_DATA;
			return observed;
		}
		auto &cell = request.readbacks[action.readback_slot];
		const auto &declaration = cell.declaration;
		if (cell.request != &request || declaration.copy_action_ordinal != index || declaration.resource_slot >= request.resource_count ||
				!production_use_matches_locked(request, action, declaration.resource_slot, declaration.byte_offset, declaration.byte_size, GDEXTENSION_PRODUCTION_READ_V1)) {
			observed.error = ERR_INVALID_DATA;
			return observed;
		}
		const auto &resource = request.resources[declaration.resource_slot];
		if (resource.output_alias_prepared) {
			// Only this exact original output action/root/source and factory
			// witness permit the alias. No qualification return cell is invented.
			if (declaration.producer_action_ordinal >= index || resource.output_action_ordinal != declaration.producer_action_ordinal ||
					!production_output_alias_matches_locked(request, resource, rd)) {
				observed.error = ERR_UNAUTHORIZED;
				return observed;
			}
			const auto &writer = request.actions[declaration.producer_action_ordinal];
			if (writer.output_state != ProductionActionCell::OutputState::BOUND || !production_output_source_matches_locked(request, writer, rd) ||
					!declaration.byte_size || declaration.byte_offset > resource.declaration.physical_bytes ||
					declaration.byte_size > resource.declaration.physical_bytes - declaration.byte_offset) {
				observed.error = ERR_UNAUTHORIZED;
				return observed;
			}
		}
		if (cell.declaration.byte_offset > UINT32_MAX || cell.declaration.byte_size > UINT32_MAX) {
			observed.error = ERR_PARAMETER_RANGE_ERROR;
			return observed;
		}
		readback = &cell.capture;
		readback->segments = &request.graphs[first_graph].captured;
		readback->segment_stride = sizeof(ProductionGraphCell);
		readback->capacity = room;
		readback->count = 0;
	}
	bool transfer_effect_entered = false;
	auto finish_transfer_staging = [&]() {
		for (uint32_t i = 0; i < request.resource_count; ++i) {
			auto &resource = request.resources[i];
			if (resource.transfer_staged) {
				if (!transfer_effect_entered || observed.state == CheckedRecordAppend::State::Rejected) {
					resource.transfer_pending = false;
				}
				resource.transfer_staged = false;
			}
		}
	};
	struct TransferStagingReturn {
		decltype(finish_transfer_staging) &finish;
		~TransferStagingReturn() { finish(); }
	} transfer_staging_return{ finish_transfer_staging };
	for (uint32_t i = 0; i < request.resource_count; ++i) {
		bool used = false;
		if (action.declaration.action_kind == GDEXTENSION_PRODUCTION_DISPATCH_V1) {
			// Driver preparations follow actual set bindings, including aliases.
			const auto &dispatch = action.dispatch;
			for (uint32_t set_index = 0; set_index < dispatch.set_count; ++set_index) {
				const auto &set = request.resources[dispatch.first_set_slot + set_index];
				for (uint32_t b = 0; b < set.binding_count; ++b) {
					used |= request.uses[set.first_binding + b].binding.resource_slot == i;
				}
			}
		} else {
			for (uint32_t u = 0; u < action.declaration.use_count; ++u) {
				used |= request.uses[action.declaration.first_use + u].declaration.resource_slot == i;
			}
		}
		auto &resource = request.resources[i];
		if (!used || resource.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1) {
			continue;
		}
		const auto *buffer = rd._get_buffer_from_owner(resource.primary);
		if (buffer && buffer->transfer_worker_index >= 0) {
			const uint32_t worker_index = uint32_t(buffer->transfer_worker_index);
			if (worker_index >= rd.transfer_worker_pool_size || !rd.transfer_worker_pool[worker_index]->operations_mutex.try_lock()) {
				observed.error = ERR_BUSY;
				return observed;
			}
			const bool already_returned = rd.transfer_worker_pool[worker_index]->operations_processed >= buffer->transfer_worker_operation;
			rd.transfer_worker_pool[worker_index]->operations_mutex.unlock();
			if (already_returned) {
				continue;
			}
			resource.transfer_staged = !resource.transfer_pending;
			resource.transfer_worker = uint32_t(buffer->transfer_worker_index);
			resource.transfer_operation = buffer->transfer_worker_operation;
			resource.transfer_action_ordinal = action.declaration.action_ordinal;
			resource.transfer_pending = true;
		}
	}
	// Fixed request and entity/RD ownership survive synchronous driver loss.
	// Release/close observe capture_inflight before reclaiming this arena.
	const uint64_t serial = request.serial;
	++request.capture_inflight;
	rd.production_recording_request = &request;
	for (uint32_t i = 0; i < request.resource_count; ++i) {
		const auto &resource = request.resources[i];
		if (!rd._production_resource_permitted(resource.primary) || (resource.secondary.is_valid() && !rd._production_resource_permitted(resource.secondary))) {
			rd.production_recording_request = nullptr;
			--request.capture_inflight;
			observed.error = ERR_UNAUTHORIZED;
			return observed;
		}
	}
	production_append_effect_enter_locked(request);
	transfer_effect_entered = true;
	bool list_open = request.compute_list_open;
	mutex.unlock();
	if (list_open && action.declaration.action_kind != GDEXTENSION_PRODUCTION_DISPATCH_V1) {
		rd.compute_list_end();
		list_open = rd.compute_list.active;
	}
	switch (action.declaration.action_kind) {
		case GDEXTENSION_PRODUCTION_DISPATCH_V1: {
			const auto &dispatch = action.dispatch;
			const auto list = list_open ? RenderingDevice::ComputeListID(RenderingDevice::ID_TYPE_COMPUTE_LIST) : rd.compute_list_begin();
			if (list == RenderingDevice::INVALID_ID) {
				observed.error = ERR_CANT_CREATE;
				break;
			}
			list_open = true;
			rd.compute_list_bind_compute_pipeline(list, pipeline);
			for (uint32_t i = 0; i < dispatch.set_count; ++i) {
				rd.compute_list_bind_uniform_set(list, request.resources[dispatch.first_set_slot + i].primary, i);
			}
			observed = rd._record_source_dispatch(identity, list, dispatch.groups_x, dispatch.groups_y, dispatch.groups_z, request.attachment->payload + action.payload_offset, dispatch.push_bytes);
			if (observed.state == CheckedRecordAppend::State::Rejected) {
				// The original list/bind prefix exists even if dispatch refused.
				observed.state = CheckedRecordAppend::State::Unknown;
			}
		} break;
		case GDEXTENSION_PRODUCTION_COPY_V1: {
			const auto &copy = action.copy;
			observed = rd._record_source_buffer_copy(identity, request.resources[copy.source_slot].primary, request.resources[copy.destination_slot].primary, uint32_t(copy.source_offset), uint32_t(copy.destination_offset), uint32_t(copy.bytes));
		} break;
		case GDEXTENSION_PRODUCTION_UPLOAD_V1: {
			const auto &value = action.upload;
			observed = rd._record_source_buffer_upload(identity, request.resources[value.destination_slot].primary, uint32_t(value.destination_offset), uint32_t(value.bytes), request.attachment->payload + action.payload_offset, upload);
		} break;
		case GDEXTENSION_PRODUCTION_READBACK_V1: {
			const auto &declaration = request.readbacks[action.readback_slot].declaration;
			observed.error = rd._buffer_get_data_async(request.resources[declaration.resource_slot].primary, Callable(), uint32_t(declaration.byte_offset), uint32_t(declaration.byte_size), {}, readback);
			if (readback->count) {
				observed.handle = readback->segment(readback->count - 1).handle;
				observed.state = readback->state->issued == declaration.byte_size ? CheckedRecordAppend::State::Appended : CheckedRecordAppend::State::KnownPrefix;
			}
		} break;
	}
	if (list_open && (action.declaration.list_boundary == GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1 || observed.state == CheckedRecordAppend::State::Unknown)) {
		const auto closed = rd._compute_list_end_recorded();
		list_open = rd.compute_list.active;
		if (!observed.handle.is_valid() && closed.is_valid()) {
			observed.handle = closed;
			observed.state = CheckedRecordAppend::State::KnownPrefix;
		}
	}
	rd.production_recording_request = nullptr;
	mutex.lock();
	CRASH_COND(request.serial != serial || !request.capture_inflight);
	--request.capture_inflight;
	request.compute_list_open = list_open;
	uint32_t captures = readback ? readback->count : upload.count;
	for (uint32_t i = 0; i < captures; ++i) {
		CheckedRecordAppend segment = observed;
		segment.handle = readback ? readback->segment(i).handle : upload.segment(i).handle;
		if (production_capture_graph_locked(request, rd, action.declaration.action_ordinal, segment) != OK) {
			observed.state = CheckedRecordAppend::State::Unknown;
		}
	}
	if (!captures && observed.handle.is_valid() && production_capture_graph_locked(request, rd, action.declaration.action_ordinal, observed) != OK) {
		observed.state = CheckedRecordAppend::State::Unknown;
	}
	if (action.declaration.action_kind == GDEXTENSION_PRODUCTION_UPLOAD_V1) {
		action.recorded_bytes = upload.consumed;
	}
	if (observed.state == CheckedRecordAppend::State::KnownPrefix && action.declaration.action_kind != GDEXTENSION_PRODUCTION_UPLOAD_V1) {
		// Only the registered partial-upload parent has continuation permission.
		request.cancelled = true;
	}
	if (request.source_device_lost || observed.state == CheckedRecordAppend::State::Unknown) {
		request.effects_pending = true;
		observed.state = CheckedRecordAppend::State::Unknown;
	} else if (observed.state == CheckedRecordAppend::State::Appended) {
		++request.next_action;
	}
	++request.revision;
	return observed;
}

Error Registry::production_close_segment_locked(ProductionRequest &request, GDExtensionProductionSegmentV2 &out, bool physical_scope_exit) {
	if (request.capture_inflight || (request.compute_list_open || request.draw_list_open) || request.effects_pending || request.source_device_lost || request.cancelled || request.segment.segment != request.acknowledged_segment) {
		return ERR_BUSY;
	}
	if (request.graph_count == request.segment_graph_begin || request.segment.segment == UINT64_MAX || request.revision == UINT64_MAX) {
		return ERR_UNAVAILABLE;
	}
	const bool incomplete = request.next_action < request.action_count && request.actions[request.next_action].recorded_bytes != 0;
	const uint32_t cutoff = incomplete ? request.next_action : request.next_action ? request.next_action - 1
																				   : UINT32_MAX;
	if (cutoff == UINT32_MAX || (!physical_scope_exit && request.actions[cutoff].declaration.list_boundary != GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1)) {
		return ERR_INVALID_DATA;
	}
	request.segment.abi = production_dto_abi<GDExtensionProductionSegmentV2>();
	++request.segment.segment;
	request.segment.segment_revision = ++request.revision;
	request.segment.first_action_ordinal = request.segment_action_begin;
	request.segment.end_action_ordinal = request.next_action;
	request.segment.incomplete_upload_action_ordinal = incomplete ? cutoff : UINT64_MAX;
	request.segment_graph_begin = request.graph_count;
	request.segment_action_begin = request.next_action;
	out = request.segment;
	return OK;
}
// Authentication belongs to the original ACK call frame, before this private
// immutable-segment comparison. A numeric public RequestRef never reaches it.
Error Registry::production_ack_segment_locked(ProductionRequest &request, uint64_t segment, uint64_t revision) {
	if (!segment || segment != request.segment.segment || revision != request.segment.segment_revision) {
		return ERR_DOES_NOT_EXIST;
	}
	if (request.capture_inflight || request.effects_pending || request.source_device_lost) {
		return ERR_BUSY;
	}
	if (request.acknowledged_segment == segment) {
		return OK;
	}
	if (request.revision == UINT64_MAX) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	request.acknowledged_segment = segment;
	++request.revision;
	return OK;
}

Error Registry::production_readback_delivery(BufferReadbackSourceCapture &capture, bool begin) {
	MutexLock lock(mutex);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	if (capture.registry != this || !capture.request_serial) {
		return ERR_INVALID_DATA;
	}
	// This is the original Frame's internal RT return, not a public owner
	// request. Its fixed accepted arena and held device survive owner close.
	for (auto *attachment : production_attachments) {
		if (!attachment) {
			continue;
		}
		for (uint32_t i = 0; i < attachment->quote.capacity.requests; ++i) {
			auto &candidate = attachment->requests[i];
			if (candidate.serial == capture.request_serial && candidate.attachment == attachment) {
				request = &candidate;
				rd = attachment->held_device;
				break;
			}
		}
		if (request) {
			break;
		}
	}
	if (!request || !rd || rd->render_thread_id != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	bool original = false;
	for (uint32_t i = 0; i < request->readback_count; ++i) {
		original |= &request->readbacks[i].capture == &capture && capture.state == &request->readbacks[i].source;
	}
	if (!original) {
		return ERR_INVALID_DATA;
	}
	if (begin) {
		if (request->capture_inflight == UINT32_MAX || request->revision == UINT64_MAX || (request->capture_inflight && (!rd || rd->render_thread_id != Thread::get_caller_id() || rd->production_recording_request))) {
			return ERR_BUSY;
		}
		++request->capture_inflight;
	} else {
		CRASH_COND(!request->capture_inflight);
		--request->capture_inflight;
		++request->revision;
	}
	return OK;
}

Error Registry::production_close_lists_locked(ProductionRequest &request, RenderingDevice &rd) {
	if (request.draw_list_open) {
		return production_raster_close_locked(request, rd);
	}
	if (!request.compute_list_open) {
		return OK;
	}
	if (request.capture_inflight || rd.render_thread_id != Thread::get_caller_id() || !rd.compute_list.active || rd.draw_list.active || rd.raytracing_list.active || request.revision == UINT64_MAX) {
		return ERR_BUSY;
	}
	++request.capture_inflight;
	production_append_effect_enter_locked(request);
	mutex.unlock();
	const auto closed = rd._compute_list_end_recorded();
	mutex.lock();
	CRASH_COND(!request.capture_inflight);
	--request.capture_inflight;
	request.compute_list_open = rd.compute_list.active;
	if (!closed.is_valid() || request.compute_list_open || request.draw_list_open) {
		request.effects_pending = true;
	}
	++request.revision;
	return request.effects_pending ? ERR_BUSY : OK;
}
} // namespace GDExtensionDeviceExit
