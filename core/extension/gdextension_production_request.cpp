/**************************************************************************/
/*  gdextension_production_request.cpp                                    */
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
#include "core/extension/gdextension_camera_ready.h"

#include "core/extension/gdextension_production_legacy_projection.h"
#include "core/io/native_drain_diagnostic.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_owner.h"
#include "servers/rendering/rendering_server_default.h"

#include <cstddef>
#include <cstring>
#include <new>
#include <optional>
#include <type_traits>

namespace GDExtensionDeviceExit {
namespace {
GDExtensionDeviceSessionResultV1 result(Error e, uint32_t reason = 0) {
	return { int32_t(e), reason, 0, 0 };
}
template <class T>
bool aligned(const T *p) {
	return p && uintptr_t(p) % alignof(T) == 0;
}
bool abi(const GDExtensionDeviceExitAbi &p) {
	return p.abi_version == 1 && p.struct_size == sizeof(p) && !p.reserved;
}
bool separate(const void *a, size_t an, const void *b, size_t bn) {
	auto x = uintptr_t(a), y = uintptr_t(b);
	return x <= y ? an <= y - x : bn <= x - y;
}
bool add_layout(uint64_t &offset, uint64_t count, uint64_t bytes, uint64_t alignment) {
	const uint64_t padding = (alignment - offset % alignment) % alignment;
	if (padding > UINT64_MAX - offset) {
		return false;
	}
	offset += padding;
	if (count > (UINT64_MAX - offset) / bytes) {
		return false;
	}
	offset += count * bytes;
	return offset <= SIZE_MAX && offset <= INT64_MAX;
}
bool capacity_fits(const GDExtensionProductionCapacityV1 &a, const GDExtensionProductionCapacityV1 &b) {
	return a.requests <= b.requests && a.suboperations <= b.suboperations && a.resources <= b.resources && a.resource_uses <= b.resource_uses && a.graph_memberships <= b.graph_memberships && a.native_memberships <= b.native_memberships && a.readbacks <= b.readbacks && a.return_cells <= b.return_cells && a.retained_payload_bytes <= b.retained_payload_bytes && a.readback_bytes <= b.readback_bytes;
}
struct RdUnlock {
	Mutex &mutex;
	~RdUnlock() { mutex.unlock(); }
};
bool range(uint64_t offset, uint64_t bytes, uint64_t extent) {
	return offset <= extent && bytes <= extent - offset;
}
bool before_service_deadline(const GDExtensionProductionServiceBudgetV1 &budget) {
	return budget.deadline_micros && OS::get_singleton() && OS::get_singleton()->get_ticks_usec() < budget.deadline_micros;
}
bool service_budget_active(const GDExtensionProductionServiceBudgetV1 &budget) {
	return budget.controls && budget.receipts && budget.passes && before_service_deadline(budget);
}
bool same_service_budget(const GDExtensionProductionServiceBudgetV1 &a, const GDExtensionProductionServiceBudgetV1 &b) {
	return a.deadline_micros == b.deadline_micros && a.controls == b.controls && a.receipts == b.receipts && a.passes == b.passes;
}
bool retainable_native_source(const RDD::NativeCallFact &s) {
	return s.serial && s.queue_index && s.completion_serial && s.completion != RDD::CompletionCapability::Disabled && !s.presentation_only;
}
bool same_native_source(const RDD::NativeCallFact &a, const RDD::NativeCallFact &b) {
	return a.serial == b.serial && a.issue == b.issue && a.error == b.error && a.native_code == b.native_code && a.completion == b.completion && a.completion_serial == b.completion_serial && a.queue_index == b.queue_index && a.present == b.present && a.presentation_only == b.presentation_only && a.source_owner_returned == b.source_owner_returned && a.frame_observer_owned == b.frame_observer_owned;
}
bool same_native_observation(const NativeSourceObservation &a, const NativeSourceObservation &b) {
	return a.state == b.state && a.error == b.error && a.native_code == b.native_code && a.completion_native_code == b.completion_native_code && a.resolved_issue == b.resolved_issue && a.retention_error == b.retention_error && a.completion_error == b.completion_error;
}
} //namespace
Error Registry::production_layout(const GDExtensionProductionCapacityV1 &c, GDExtensionProductionMetadataQuoteV1 &out, uint32_t current_commands, uint32_t workers) {
	if (!production_dto_valid(c) || !c.requests || (c.suboperations && c.requests > c.suboperations) || (c.resources && c.requests > c.resources) || c.resource_uses < c.resources || c.graph_memberships < c.suboperations || c.native_memberships < c.suboperations || c.return_cells < c.resources || c.readbacks > c.suboperations) {
		return ERR_INVALID_PARAMETER;
	}
	// Report counters and the paged graph/native fact index use uint32_t.
	if (uint64_t(c.graph_memberships) + c.native_memberships + c.readbacks + c.return_cells + 2 > UINT32_MAX) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	// Each reserved request owns its command slice. Its pre-existing RD prefix
	// cannot be shared with another request, even when both observe the same
	// current frame. Keep that prefix once per request in the attachment pool.
	const uint64_t prefixes = uint64_t(current_commands) * c.requests;
	if (prefixes > UINT32_MAX) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	uint32_t commands = 0, frames = 0;
	Error e = c.graph_memberships ? record_source_slot_bound(c.requests, c.graph_memberships, uint32_t(prefixes), workers, commands, frames) : OK;
	if (e != OK) {
		return e;
	}
	uint64_t bytes = 0;
#define LAYOUT(type, count) \
	if (!add_layout(bytes, count, sizeof(type), alignof(type))) \
	return ERR_PARAMETER_RANGE_ERROR
	LAYOUT(ProductionAttachment, 1);
	// Includes the fixed OBSERVE generation result; use the actual new sizeof.
	LAYOUT(ProductionRequest, c.requests);
	// Includes the full embedded native subscriber, immutable camera sample
	// and existing-source Ref; no request observer heap allocation.
	LAYOUT(ProductionActionCell, c.suboperations);
	LAYOUT(ProductionResourceSlot, c.resources);
	LAYOUT(ProductionUseCell, c.resource_uses);
	LAYOUT(ProductionReadbackCell, c.readbacks);
	LAYOUT(ProductionGraphCell, c.graph_memberships);
	LAYOUT(ProductionNativeCell, c.native_memberships);
	LAYOUT(ProductionReturnCell, c.return_cells);
	LAYOUT(ProductionCaptureCell, commands);
	LAYOUT(ProductionCaptureCell, frames);
	LAYOUT(uint8_t, c.retained_payload_bytes);
	LAYOUT(uint8_t, c.readback_bytes);
#undef LAYOUT
	out = { production_dto_abi<GDExtensionProductionMetadataQuoteV1>(), c, bytes, alignof(std::max_align_t), commands, frames, uint64_t(c.return_cells) + 1 }; // One accepted metadata entry credit.
	return OK;
}
Error Registry::production_membership_bound_locked(RenderingDevice &rd, uint32_t graphs, uint32_t workers, uint32_t &out) const {
	out = 0;
	if (!graphs) {
		return OK;
	}
	if (!rd.driver) {
		return ERR_UNAVAILABLE;
	}
	// A graph belongs to exactly one main slot, plus only the workers used by
	// its canonical action. The profile includes already committed wrapper
	// records and the pre-graph hidden source. Reserve the second post-graph
	// prepare source explicitly, preserving the original qualification profile.
	const uint32_t per_graph = rd.driver->qualification_native_call_bound_for_profile(1, workers, 1);
	uint32_t post_graph = 0;
	if (!per_graph || !rd.driver->native_issue_bound(RDD::NativeIssueKind::PREPARE_SEGMENT, 0, post_graph)) {
		return ERR_UNAVAILABLE;
	}
	const uint64_t per_edge = uint64_t(per_graph) + post_graph;
	if (graphs && per_edge > UINT32_MAX / graphs) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	out = uint32_t(uint64_t(graphs) * per_edge);
	return OK;
}
Error Registry::production_device_quote_locked(RenderingDevice &rd, const GDExtensionProductionCapacityV1 &capacity, GDExtensionProductionMetadataQuoteV1 &out, uint32_t worker_bound) {
	if (!rd._thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	RdUnlock unlock{ rd._thread_safe_ };
	if (!rd.driver || rd.frames.is_empty()) {
		return ERR_UNAVAILABLE;
	}
	const uint32_t current = rd.draw_graph.qualification_command_count();
	GDExtensionProductionMetadataQuoteV1 quote = {};
	Error e = production_layout(capacity, quote, current, rd.transfer_worker_pool_max_size);
	if (e != OK) {
		return e;
	}
	uint32_t memberships = 0;
	e = production_membership_bound_locked(rd, capacity.graph_memberships, MIN(worker_bound == UINT32_MAX ? capacity.resources : worker_bound, rd.transfer_worker_pool_max_size), memberships);
	if (e != OK) {
		return e;
	}
	if (memberships > capacity.native_memberships) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	out = quote;
	return OK;
}
// Read-only sizing, never a reservation or source/operation grant. Inputs remain
// synchronous; actual reserve/prepare/append validate the original frozen closure.
GDExtensionDeviceSessionResultV1 Registry::production_request_requirements(GDExtensionProductionAttachmentRefV1 ref, const GDExtensionProductionActionV2 *actions, uint32_t action_count, const GDExtensionProductionResourceV1 *resources, uint32_t resource_count, const GDExtensionProductionUseV1 *uses, uint32_t use_count, const GDExtensionProductionUploadV2 *uploads, uint32_t upload_count, const GDExtensionProductionReadbackSourceV1 *readbacks, uint32_t readback_count, uint32_t binding_count, uint64_t payload_bytes, GDExtensionProductionMetadataQuoteV1 *out) {
	return production_request_requirements_internal(ref, actions, action_count, resources, resource_count, uses, use_count, uploads, upload_count, readbacks, readback_count, binding_count, payload_bytes, nullptr, 0, nullptr, 0, out);
}
GDExtensionDeviceSessionResultV1 Registry::production_request_requirements_v2(GDExtensionProductionAttachmentRefV1 ref, const GDExtensionProductionActionV2 *actions, uint32_t action_count, const GDExtensionProductionResourceV1 *resources, uint32_t resource_count, const GDExtensionProductionUseV1 *uses, uint32_t use_count, const GDExtensionProductionUploadV2 *uploads, uint32_t upload_count, const GDExtensionProductionReadbackSourceV1 *readbacks, uint32_t readback_count, uint32_t binding_count, uint64_t payload_bytes, const GDExtensionProductionOutputDemandInputV1 *outputs, uint32_t output_count, GDExtensionProductionOutputGuardDemandV1 *demands, uint32_t demand_capacity, GDExtensionProductionMetadataQuoteV1 *out) {
	return production_request_requirements_internal(ref, actions, action_count, resources, resource_count, uses, use_count, uploads, upload_count, readbacks, readback_count, binding_count, payload_bytes, outputs, output_count, demands, demand_capacity, out);
}
GDExtensionDeviceSessionResultV1 Registry::production_request_requirements_internal(GDExtensionProductionAttachmentRefV1 ref, const GDExtensionProductionActionV2 *actions, uint32_t action_count, const GDExtensionProductionResourceV1 *resources, uint32_t resource_count, const GDExtensionProductionUseV1 *uses, uint32_t use_count, const GDExtensionProductionUploadV2 *uploads, uint32_t upload_count, const GDExtensionProductionReadbackSourceV1 *readbacks, uint32_t readback_count, uint32_t binding_count, uint64_t payload_bytes, const GDExtensionProductionOutputDemandInputV1 *outputs, uint32_t output_count, GDExtensionProductionOutputGuardDemandV1 *demands, uint32_t demand_capacity, GDExtensionProductionMetadataQuoteV1 *out) {
	if (!aligned(out)) {
		return result(ERR_INVALID_PARAMETER);
	}
	// Validate every address/extent before reading rows. Zero-length spans do
	// not dereference their pointer. Output may not overwrite an input span.
	auto span = [&](auto *p, uint32_t count) {
		using T = std::remove_cv_t<std::remove_pointer_t<decltype(p)>>;
		return !count || (aligned(p) && count <= SIZE_MAX / sizeof(T) &&
				uint64_t(count) * sizeof(T) <= UINTPTR_MAX - uintptr_t(p) &&
				separate(p, size_t(count) * sizeof(T), out, sizeof(*out)));
	};
	if (!span(actions, action_count) || !span(resources, resource_count) || !span(uses, use_count) || !span(uploads, upload_count) || !span(readbacks, readback_count) || !span(outputs, output_count) || !span(demands, output_count) || output_count > demand_capacity || sizeof(*out) > UINTPTR_MAX - uintptr_t(out)) {
		return result(ERR_INVALID_PARAMETER);
	}
	if (output_count) {
		auto demand_separate = [&](auto *p, uint32_t count) {
			using T = std::remove_cv_t<std::remove_pointer_t<decltype(p)>>;
			return !count || separate(p, size_t(count) * sizeof(T), demands, size_t(output_count) * sizeof(*demands));
		};
		if (!demand_separate(actions, action_count) || !demand_separate(resources, resource_count) || !demand_separate(uses, use_count) || !demand_separate(uploads, upload_count) || !demand_separate(readbacks, readback_count) || !demand_separate(outputs, output_count)) {
			return result(ERR_INVALID_PARAMETER);
		}
	}
	Lock lock(*this);
	ProductionAttachment *attachment = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_attachment_locked(ref, true, attachment, rd);
	if (error != OK) {
		return result(error);
	}
	const auto &pool = attachment->quote.capacity;
	// Bound both scanning and nested closure work by the accepted finite pool.
	if (output_count > action_count / 3 || action_count > pool.suboperations || resource_count > pool.resources || use_count > pool.resource_uses || readback_count > pool.readbacks || upload_count > action_count || binding_count > pool.resource_uses || payload_bytes > pool.retained_payload_bytes - attachment->scene_prefix_bytes || binding_count > UINT32_MAX - use_count) {
		return result(ERR_PARAMETER_RANGE_ERROR);
	}
	GDExtensionProductionCapacityV1 capacity = {};
	capacity.abi = production_dto_abi<GDExtensionProductionCapacityV1>();
	capacity.requests = 1;
	capacity.suboperations = action_count;
	capacity.resources = resource_count;
	capacity.resource_uses = MAX(resource_count, use_count + binding_count);
	capacity.readbacks = readback_count;
	// Original returns[] is resource-slot indexed, including retained slots.
	capacity.return_cells = resource_count;
	capacity.retained_payload_bytes = payload_bytes;
	for (uint32_t i = 0; i < resource_count; ++i) {
		if (resources[i].resource_kind < GDEXTENSION_PRODUCTION_STORAGE_V1 || resources[i].resource_kind > GDEXTENSION_PRODUCTION_SET_V1 || (!output_count && resources[i].ownership == GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1)) {
			return result(ERR_UNAVAILABLE);
		}
	}
	for (uint32_t i = 0; i < use_count; ++i) {
		if (uses[i].resource_slot >= resource_count || !range(uses[i].byte_offset, uses[i].byte_size, resources[uses[i].resource_slot].physical_bytes)) {
			return result(ERR_INVALID_DATA);
		}
	}
	uint32_t worker_bound = 0;
	for (uint32_t i = 0; i < action_count; ++i) {
		const auto &action = actions[i];
		if (action.action_ordinal != i || action.source_recipe_reserved || action.first_use > use_count || action.use_count > use_count - action.first_use) {
			return result(ERR_INVALID_DATA);
		}
		if (action.action_kind < GDEXTENSION_PRODUCTION_DISPATCH_V1 || action.action_kind > (output_count ? GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 : GDEXTENSION_PRODUCTION_READBACK_V1)) {
			return result(ERR_UNAVAILABLE);
		}
		uint32_t storages = 0;
		for (uint32_t u = 0; u < action.use_count; ++u) {
			const uint32_t slot = uses[action.first_use + u].resource_slot;
			bool duplicate = false;
			for (uint32_t before = 0; before < u; ++before) {
				duplicate |= uses[action.first_use + before].resource_slot == slot;
			}
			storages += resources[slot].resource_kind == GDEXTENSION_PRODUCTION_STORAGE_V1 && !duplicate;
		}
		worker_bound = MAX(worker_bound, storages);
	}
	if (!rd || !rd->_thread_safe_.try_lock()) {
		return result(ERR_BUSY);
	}
	RdUnlock unlock{ rd->_thread_safe_ };
	if (!rd->driver || rd->frames.is_empty()) {
		return result(ERR_UNAVAILABLE);
	}
	Vector<GDExtensionProductionOutputGuardDemandV1> projected;
	projected.resize(output_count);
	for (uint32_t i = 0; i < output_count; ++i) {
		const auto &input = outputs[i];
		if (input.generation_request.registry_instance != instance || !input.generation_request.handle_serial) {
			return result(ERR_UNAUTHORIZED);
		}
		if (input.acquire_action_ordinal >= input.finalize_action_ordinal || input.finalize_action_ordinal >= input.seal_action_ordinal || input.seal_action_ordinal >= action_count) {
			return result(ERR_INVALID_DATA);
		}
		const uint64_t ordinals[] = { input.acquire_action_ordinal, input.finalize_action_ordinal, input.seal_action_ordinal };
		const uint64_t canonicals[] = { input.acquire_canonical_suboperation, input.finalize_canonical_suboperation, input.seal_canonical_suboperation };
		const uint32_t substeps[] = { input.acquire_source_recipe_substep, input.finalize_source_recipe_substep, input.seal_source_recipe_substep };
		for (uint32_t k = 0; k < 3; ++k) {
			const auto &action = actions[ordinals[k]];
			if (action.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || action.canonical_suboperation != canonicals[k] || action.source_recipe_substep != substeps[k]) {
				return result(ERR_INVALID_DATA);
			}
		}
		error = production_output_demand_locked(*attachment, *rd, input, projected.write[i]);
		if (error != OK) {
			return result(error);
		}
		for (uint32_t j = 0; j < i; ++j) {
			const uint64_t a[] = { outputs[i].acquire_action_ordinal, outputs[i].finalize_action_ordinal, outputs[i].seal_action_ordinal };
			const uint64_t b[] = { outputs[j].acquire_action_ordinal, outputs[j].finalize_action_ordinal, outputs[j].seal_action_ordinal };
			for (auto x : a) {
				for (auto y : b) {
					if (x == y) {
						return result(ERR_INVALID_DATA);
					}
				}
			}
		}
		if (projected[i].required_output_use_cells > UINT32_MAX - capacity.resource_uses) {
			return result(ERR_PARAMETER_RANGE_ERROR);
		}
		capacity.resource_uses += projected[i].required_output_use_cells;
	}
	uint64_t upload_bytes = 0;
	for (uint32_t i = 0; i < action_count; ++i) {
		const auto &action = actions[i];
		uint32_t graphs = 1;
		if (action.action_kind == GDEXTENSION_PRODUCTION_UPLOAD_V1) {
			const GDExtensionProductionUploadV2 *upload = nullptr;
			for (uint32_t j = 0; j < upload_count; ++j) {
				if (uploads[j].action_ordinal == i) {
					if (upload) {
						return result(ERR_INVALID_DATA);
					}
					upload = &uploads[j];
				}
			}
			if (!upload || !production_dto_valid(*upload) || upload->reserved || upload->source_recipe_reserved || upload->canonical_suboperation != action.canonical_suboperation || upload->source_recipe_substep != action.source_recipe_substep || upload->destination_slot >= resource_count || resources[upload->destination_slot].resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || !range(upload->destination_offset, upload->bytes, resources[upload->destination_slot].physical_bytes) || upload->bytes > payload_bytes - upload_bytes) {
				return result(ERR_INVALID_DATA);
			}
			upload_bytes += upload->bytes;
			error = production_staging_graph_bound(upload->bytes, rd->upload_staging_buffers.block_size, graphs);
		} else if (action.action_kind == GDEXTENSION_PRODUCTION_READBACK_V1) {
			if (action.readback_slot >= readback_count) {
				return result(ERR_INVALID_DATA);
			}
			const auto &readback = readbacks[action.readback_slot];
			if (readback.copy_action_ordinal != i || readback.resource_slot >= resource_count || resources[readback.resource_slot].resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || !range(readback.byte_offset, readback.byte_size, resources[readback.resource_slot].physical_bytes) || readback.byte_size > UINT64_MAX - capacity.readback_bytes) {
				return result(ERR_INVALID_DATA);
			}
			capacity.readback_bytes += readback.byte_size;
			error = production_staging_graph_bound(readback.byte_size, rd->download_staging_buffers.block_size, graphs);
		}
		if (action.action_kind == GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1) {
			bool found = false;
			for (uint32_t j = 0; j < output_count; ++j) {
				if (outputs[j].acquire_action_ordinal == i) {
					graphs = 0;
					found = true;
				}
				if (outputs[j].finalize_action_ordinal == i) {
					projected.write[j].prefix_graphs = capacity.graph_memberships;
					graphs = projected[j].guard_graphs;
					found = true;
				}
				if (outputs[j].seal_action_ordinal == i) {
					graphs = projected[j].read_graphs;
					found = true;
				}
			}
			if (!found) {
				return result(ERR_INVALID_DATA);
			}
		}
		if (error != OK || graphs > UINT32_MAX - capacity.graph_memberships) {
			return result(error == OK ? ERR_PARAMETER_RANGE_ERROR : error);
		}
		capacity.graph_memberships += graphs;
	}
	// Reject orphan upload/readback rows, including repeated readback slots.
	for (uint32_t i = 0; i < upload_count; ++i) {
		if (uploads[i].action_ordinal >= action_count || actions[uint32_t(uploads[i].action_ordinal)].action_kind != GDEXTENSION_PRODUCTION_UPLOAD_V1) {
			return result(ERR_INVALID_DATA);
		}
	}
	for (uint32_t i = 0; i < readback_count; ++i) {
		if (readbacks[i].copy_action_ordinal >= action_count || actions[uint32_t(readbacks[i].copy_action_ordinal)].action_kind != GDEXTENSION_PRODUCTION_READBACK_V1 || actions[uint32_t(readbacks[i].copy_action_ordinal)].readback_slot != i) {
			return result(ERR_INVALID_DATA);
		}
	}
	worker_bound = output_count ? rd->transfer_worker_pool_max_size : MIN(worker_bound, rd->transfer_worker_pool_max_size);
	error = production_membership_bound_locked(*rd, capacity.graph_memberships, worker_bound, capacity.native_memberships);
	if (error != OK) {
		return result(error);
	}
	GDExtensionProductionMetadataQuoteV1 quote = {};
	// Use the existing command arena layout (max workers), matching reserve.
	error = production_layout(capacity, quote, rd->draw_graph.qualification_command_count(), rd->transfer_worker_pool_max_size);
	if (error != OK || !capacity_fits(capacity, pool)) {
		return result(error == OK ? ERR_PARAMETER_RANGE_ERROR : error);
	}
	for (uint32_t i = 0; i < output_count; ++i) {
		auto &demand = projected.write[i];
		demand.total_graphs = capacity.graph_memberships;
		demand.suffix_graphs = capacity.graph_memberships - demand.prefix_graphs - demand.guard_graphs;
		demand.total_native_memberships = capacity.native_memberships;
		demands[i] = demand;
	}
	*out = quote;
	return result(OK);
}
Error Registry::production_installation_locked(GDExtensionDeviceExitSceneReadyContextRef frame, GDExtensionProducerRegistrationRefV1 producer, GDExtensionDeviceRegistryContextRef context, GDExtensionOriginalDeviceSessionRef session, uint32_t &library, uint32_t &device, HolderRetirement *&holder) {
	View *view = nullptr;
	Error e = resolve_borrow_locked(frame.registry_instance, frame.handle_serial, ViewDomain::SCENE_READY, view);
	if (e != OK) {
		return e;
	}
	library = view->record;
	auto &record = records[library];
	const auto *current = RegistrationContext::current;
	if (!current || !current->reference || current->reference->registry != this || current->reference->slot != library || !current->admitted || current->ack || current->level_phase != LevelPhase::SCENE_READY || record.scene_ready_state != Record::SceneReadyState::IN_FRAME || record.owner_thread != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	if (!record.attached || record.quiescing || record.close_started || record.close_error != OK || record.guard_enrollment != GuardEnrollment::QUALIFIED) {
		return ERR_UNAVAILABLE;
	}
	if (record.scene_ready_flavor == Record::SceneReadyFlavor::V2 && (!(record.scene_install_plan.selected_features & GDEXTENSION_SCENE_INSTALL_RD_PAIR_V2) || !scene_install_plan_matches_locked(record))) {
		return ERR_UNAVAILABLE;
	}
	uint32_t producer_view = INVALID_SLOT;
	e = producer_view_locked(producer, producer_view);
	if (e != OK) {
		return e;
	}
	if (views[producer_view].record != library || !record.producer_attachment_identity || !record.producer_registration_serial) {
		return ERR_UNAUTHORIZED;
	}
	DeviceRef *c = nullptr, *s = nullptr;
	e = device_ref_locked(context.registry_instance, context.handle_serial, false, c);
	if (e != OK) {
		return e;
	}
	e = device_ref_locked(session.registry_instance, session.handle_serial, true, s);
	if (e != OK) {
		return e;
	}
	if (c->library != library || s->library != library || c->device != s->device) {
		return ERR_UNAUTHORIZED;
	}
	device = c->device;
	auto &d = device_records[device];
	auto *rd = Object::cast_to<RenderingDevice>(d.object);
	// Physical main-device comparison is independent of qualification_main.
	if (!d.live || !d.epoch || !d.session || !rd || rd != RenderingDevice::get_singleton() || !rd->is_main_instance || d.execution_thread != rd->render_thread_id || rd->production_finalization_started.load(std::memory_order_acquire) || rd->native_finalization_returned) {
		return ERR_UNAVAILABLE;
	}
	if (!(live_services & (1u << 5))) {
		return ERR_UNAVAILABLE;
	}
	Registry *entity_owner = rd->production_entity_registry.load(std::memory_order_acquire);
	if (entity_owner && entity_owner != this) {
		return ERR_UNAUTHORIZED;
	}
	holder = nullptr;
	for (auto *h = record.holders; h; h = h->record_next) {
		if (!h->typed_code_hold || h->pending || h->handle_serial != record.producer_code_holder || h->generation != record.snapshot.library_generation || h->acquired_thread != record.owner_thread) {
			continue;
		}
		const auto &scene = record.levels[GDEXTENSION_INITIALIZATION_SCENE];
		if (!scene.initialized || scene.finalized) {
			continue;
		}
		// The original Host currently acquires its SCENE USER terminal hold.
		// Either original action is valid only with its own declared RD service.
		for (const auto *action : { &scene.user_action, &scene.binding_action }) {
			if (action->serial && h->terminal_action == action->serial && !action->returned &&
					action->generation == h->generation && action->owner_thread == record.owner_thread &&
					action->level == GDEXTENSION_INITIALIZATION_SCENE &&
					action->kind == (action == &scene.user_action ? TerminalActionKind::LEVEL_USER : TerminalActionKind::LEVEL_BINDING) &&
					(action->services & (1u << 5)) &&
					action->wrapper && action->wrapper == record.original_initialization.deinitialize &&
					action->userdata == record.original_initialization.userdata) {
				holder = h;
			}
		}
	}
	return holder ? OK : ERR_UNAUTHORIZED;
}
GDExtensionDeviceSessionResultV1 Registry::production_quote(const GDExtensionDeviceExitAbi *a, GDExtensionDeviceExitSceneReadyContextRef frame, GDExtensionProducerRegistrationRefV1 producer, GDExtensionDeviceRegistryContextRef context, GDExtensionOriginalDeviceSessionRef session, const GDExtensionProductionCapacityV1 *capacity, GDExtensionProductionMetadataQuoteV1 *out) {
	if (!aligned(a) || !abi(*a) || !aligned(capacity) || !aligned(out)) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	// SceneReady's synchronous main-owner Engine stack keeps the original
	// main RD Object alive. Take RD before Registry so a concurrent RD owner
	// can finish its Registry work; never wait for RD while holding Registry.
	// This TLS filter is only a lifetime/lock-order preflight. The original
	// installation check below remains the authority for every input.
	RenderingDevice *locked_main_rd = nullptr;
	std::optional<MutexLock<Mutex>> main_rd_lock;
	const auto *current = RegistrationContext::current;
	if (Thread::is_main_thread() && current && current->reference &&
			current->reference->registry == this && current->admitted && !current->ack &&
			current->level_phase == LevelPhase::SCENE_READY) {
		locked_main_rd = RenderingDevice::get_singleton();
		if (locked_main_rd) {
			main_rd_lock.emplace(locked_main_rd->_thread_safe_);
		}
	}
	// Other original owner-thread paths retain their nonblocking contract.
	Lock lock(*this);
	uint32_t library = 0, device = 0;
	HolderRetirement *holder = nullptr;
	Error e = production_installation_locked(frame, producer, context, session, library, device, holder);
	if (e != OK) {
		return result(e, 16);
	}
	if (locked_main_rd && device_records[device].object != locked_main_rd) {
		return result(ERR_UNAVAILABLE, 16);
	}
	GDExtensionProductionMetadataQuoteV1 quote = {};
	e = production_device_quote_locked(*Object::cast_to<RenderingDevice>(device_records[device].object), *capacity, quote);
	if (e != OK) {
		return result(e, 19);
	}
	*out = quote;
	return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::production_accept(const GDExtensionDeviceExitAbi *a, GDExtensionDeviceExitSceneReadyContextRef frame, GDExtensionProducerRegistrationRefV1 producer, GDExtensionDeviceRegistryContextRef context, GDExtensionOriginalDeviceSessionRef session, GDExtensionDeviceExitLibraryCodeHoldRef code, const GDExtensionProductionMetadataQuoteV1 *input, GDExtensionProductionAttachmentRefV1 *out, GDExtensionProductionInstallationReportV1 *report) {
	if (!aligned(a) || !abi(*a) || !aligned(input) || !production_dto_valid(*input) || !aligned(out) || !aligned(report) || !separate(out, sizeof(*out), report, sizeof(*report)) || out->registry_instance || out->handle_serial) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	// SceneReady's synchronous main-owner Engine stack keeps the original
	// main RD Object alive. Take RD before Registry so a concurrent RD owner
	// can finish its Registry work; never wait for RD while holding Registry.
	// This TLS filter is only a lifetime/lock-order preflight. The original
	// installation check below remains the authority for every input.
	RenderingDevice *locked_main_rd = nullptr;
	std::optional<MutexLock<Mutex>> main_rd_lock;
	const auto *current = RegistrationContext::current;
	if (Thread::is_main_thread() && current && current->reference &&
			current->reference->registry == this && current->admitted && !current->ack &&
			current->level_phase == LevelPhase::SCENE_READY) {
		locked_main_rd = RenderingDevice::get_singleton();
		if (locked_main_rd) {
			main_rd_lock.emplace(locked_main_rd->_thread_safe_);
		}
	}
	// Other original owner-thread paths retain their nonblocking contract.
	Lock lock(*this);
	uint32_t library = 0, device = 0;
	HolderRetirement *holder = nullptr;
	Error e = production_installation_locked(frame, producer, context, session, library, device, holder);
	if (e != OK) {
		return result(e, 16);
	}
	if (locked_main_rd && device_records[device].object != locked_main_rd) {
		return result(ERR_UNAVAILABLE, 16);
	}
	if (code.registry_instance != instance || code.handle_serial != holder->handle_serial) {
		return result(ERR_UNAUTHORIZED, 16);
	}
	GDExtensionProductionMetadataQuoteV1 quote = {};
	e = production_device_quote_locked(*Object::cast_to<RenderingDevice>(device_records[device].object), input->capacity, quote);
	if (e != OK) {
		return result(e, 19);
	}
	if (input->allocation_bytes != quote.allocation_bytes || input->allocation_alignment != quote.allocation_alignment || input->graph_slots != quote.graph_slots || input->graph_frames != quote.graph_frames || input->cleanup_slots != quote.cleanup_slots) {
		return result(ERR_INVALID_DATA, 19);
	}
	uint32_t slot = INVALID_SLOT;
	for (uint32_t i = 0; i < RECORD_CAPACITY; ++i) {
		if (production_attachments[i] && production_attachments[i]->library == library && production_attachments[i]->device == device) {
			return result(ERR_ALREADY_IN_USE, 16);
		}
		if (!production_attachments[i]) {
			slot = i;
		}
	}
	auto &record = records[library];
	auto &d = device_records[device];
	if (slot == INVALID_SLOT || production_serials.exhausted || frame_serials.exhausted || record.private_refs >= private_limit || d.refs == UINT32_MAX || holder->production_children == UINT32_MAX || quote.cleanup_slots > UINT32_MAX - record.cleanup_reservations || uint64_t(record.active_callbacks) + record.cleanup_reservations + quote.cleanup_slots > UINT32_MAX || quote.cleanup_slots > UINT32_MAX - record.cleanup_by_phase[uint8_t(CleanupPhase::BINDING)]) {
		return result(ERR_PARAMETER_RANGE_ERROR, 19);
	}
	void *arena = Memory::alloc_static(quote.allocation_bytes);
	if (!arena) {
		return result(ERR_OUT_OF_MEMORY, 19);
	}
	uint64_t offset = 0;
	auto allocate = [&](auto *type, uint64_t count) {
		using T = std::remove_pointer_t<decltype(type)>;
		offset += (alignof(T) - offset % alignof(T)) % alignof(T);
		T *p = reinterpret_cast<T *>(static_cast<uint8_t *>(arena) + offset);
		for (uint64_t i = 0; i < count; ++i) {
			new (&p[i]) T();
		}
		offset += count * sizeof(T);
		return p;
	};
	auto *attachment = allocate(static_cast<ProductionAttachment *>(nullptr), 1);
	attachment->requests = allocate(static_cast<ProductionRequest *>(nullptr), quote.capacity.requests);
	attachment->actions = allocate(static_cast<ProductionActionCell *>(nullptr), quote.capacity.suboperations);
	attachment->resources = allocate(static_cast<ProductionResourceSlot *>(nullptr), quote.capacity.resources);
	attachment->uses = allocate(static_cast<ProductionUseCell *>(nullptr), quote.capacity.resource_uses);
	attachment->readbacks = allocate(static_cast<ProductionReadbackCell *>(nullptr), quote.capacity.readbacks);
	attachment->graphs = allocate(static_cast<ProductionGraphCell *>(nullptr), quote.capacity.graph_memberships);
	attachment->natives = allocate(static_cast<ProductionNativeCell *>(nullptr), quote.capacity.native_memberships);
	attachment->returns = allocate(static_cast<ProductionReturnCell *>(nullptr), quote.capacity.return_cells);
	attachment->commands = allocate(static_cast<ProductionCaptureCell *>(nullptr), quote.graph_slots);
	attachment->frames = allocate(static_cast<ProductionCaptureCell *>(nullptr), quote.graph_frames);
	attachment->payload = allocate(static_cast<uint8_t *>(nullptr), quote.capacity.retained_payload_bytes);
	attachment->readback_bytes = allocate(static_cast<uint8_t *>(nullptr), quote.capacity.readback_bytes);
	CRASH_COND(offset != quote.allocation_bytes);
	production_serials.take(attachment->serial);
	attachment->library = library;
	attachment->device = device;
	attachment->epoch = d.epoch;
	attachment->producer_serial = record.producer_registration_serial;
	attachment->library_generation = record.snapshot.library_generation;
	attachment->holder = holder;
	attachment->metadata_reference = RecordRef(this, library);
	CRASH_COND(!frame_serials.take(attachment->metadata_frame));
	attachment->quote = quote;
	attachment->arena = arena;
	attachment->held_device = Object::cast_to<RenderingDevice>(d.object);
	attachment->terminal_action = holder->terminal_action;
	attachment->entity_next = attachment->held_device->production_entity_holds;
	if (attachment->entity_next) {
		attachment->entity_next->entity_previous = attachment;
	}
	attachment->held_device->production_entity_holds = attachment;
	attachment->held_device->production_entity_registry.store(this, std::memory_order_release);
	++record.private_refs;
	++d.refs;
	++holder->production_children;
	record.cleanup_reservations += uint32_t(quote.cleanup_slots);
	record.cleanup_by_phase[uint8_t(CleanupPhase::BINDING)] += uint32_t(quote.cleanup_slots);
	production_attachments[slot] = attachment;
	*out = { instance, attachment->serial };
	*report = { production_dto_abi<GDExtensionProductionInstallationReportV1>(), GDEXTENSION_PRODUCTION_INSTALL_ACCEPTED_V1, 0, quote.allocation_bytes, 0, result(OK) };
	return result(OK);
}
Error Registry::production_attachment_locked(GDExtensionProductionAttachmentRefV1 ref, bool admit, ProductionAttachment *&out, RenderingDevice *&rd, StringName *detail) {
	if (detail) *detail = StringName();
	auto fail = [&](Error error, const char *cause) {
		if (detail) *detail = StringName(cause);
		return error;
	};
	out = nullptr;
	rd = nullptr;
	if (ref.registry_instance != instance || !ref.handle_serial) {
		return fail(ERR_INVALID_DATA, "WrongOwner");
	}
	for (auto *a : production_attachments) {
		if (!a || a->serial != ref.handle_serial) {
			continue;
		}
		auto &r = records[a->library];
		auto &d = device_records[a->device];
		if (!r.occupied || r.snapshot.library_generation != a->library_generation || r.producer_registration_serial != a->producer_serial || !a->holder || a->holder->pending || a->holder->generation != a->library_generation || a->holder->handle_serial != r.producer_code_holder || !r.producer_attachment_identity) {
			return fail(ERR_INVALID_DATA, "WrongOwner");
		}
		if (r.owner_thread != Thread::get_caller_id()) {
			return fail(ERR_UNAUTHORIZED, "WrongOwner");
		}
		if (admit && is_production_metadata_entry()) {
			return fail(ERR_UNAUTHORIZED, "WrongOwner");
		}
		if (admit && (a->closing || !r.attached || r.quiescing || r.close_started || r.close_error != OK || r.guard_enrollment != GuardEnrollment::QUALIFIED)) {
			return fail(ERR_BUSY, "Revoked");
		}
		if (d.epoch != a->epoch) {
			return fail(ERR_INVALID_DATA, "WrongDevice");
		}
		rd = Object::cast_to<RenderingDevice>(d.object);
		if (admit && (!d.live || !rd || rd != RenderingDevice::get_singleton() || !rd->is_main_instance)) {
			return fail(ERR_UNAVAILABLE, "WrongDevice");
		}
		if (admit && rd->production_finalization_started.load(std::memory_order_acquire)) {
			return fail(ERR_BUSY, "Revoked");
		}
		out = a;
		return OK;
	}
	return fail(ERR_DOES_NOT_EXIST, "WrongOwner");
}
Error Registry::production_request_locked(GDExtensionProductionRequestRefV1 ref, bool admit, ProductionRequest *&out, RenderingDevice *&rd, bool p_allow_static_sealed, StringName *detail) {
	if (detail) *detail = StringName();
	auto fail = [&](Error error, const char *cause) {
		if (detail) *detail = StringName(cause);
		return error;
	};
	out = nullptr;
	rd = nullptr;
	if (ref.registry_instance != instance || !ref.handle_serial) {
		return fail(ERR_INVALID_DATA, "WrongOwner");
	}
	for (auto *a : production_attachments) {
		if (!a) {
			continue;
		}
		for (uint32_t i = 0; i < a->quote.capacity.requests; ++i) {
			auto &r = a->requests[i];
			if (r.serial != ref.handle_serial) {
				continue;
			}
			ProductionAttachment *checked = nullptr;
			Error e = production_attachment_locked({ instance, a->serial }, admit, checked, rd, detail);
			if (e != OK) {
				return e;
			}
			if (r.attachment != a || (admit && (r.cancelled || r.release_started || (r.native_static_sealed && !r.frozen && !p_allow_static_sealed) || r.continuation_inflight || r.control.state != ProductionControlState::OWNER_CONSUMED))) {
				return fail(ERR_BUSY, r.attachment != a ? "WrongOwner" : "Revoked");
			}
			out = &r;
			return OK;
		}
	}
	return fail(ERR_DOES_NOT_EXIST, "WrongOwner");
}
Error Registry::production_resource_slot_locked(ProductionRequest &r, uint32_t index, ProductionResourceSlot *&out) {
	out = nullptr;
	if (index >= r.resource_count || !r.serial || !r.attachment) {
		return ERR_INVALID_PARAMETER;
	}
	auto &cell = r.resources[index];
	if (cell.request != &r || cell.request_serial != r.serial) {
		return ERR_INVALID_DATA;
	}
	out = &cell;
	return OK;
}
GDExtensionDeviceSessionResultV1 Registry::production_open(GDExtensionDeviceExitSceneReadyContextRef frame, GDExtensionProductionAttachmentRefV1 ref) {
	Lock lock(*this);
	ProductionAttachment *a = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_attachment_locked(ref, true, a, rd);
	if (e != OK) {
		return result(e, 16);
	}
	View *view = nullptr;
	e = resolve_borrow_locked(frame.registry_instance, frame.handle_serial, ViewDomain::SCENE_READY, view);
	if (e != OK) {
		return result(e, 14);
	}
	if (view->record != a->library || !RegistrationContext::current || RegistrationContext::current->level_phase != LevelPhase::SCENE_READY || records[a->library].scene_ready_state != Record::SceneReadyState::IN_FRAME) {
		return result(ERR_UNAUTHORIZED, 14);
	}
	if (!a->record_receiver) {
		return result(ERR_UNAVAILABLE, GDEXTENSION_DEVICE_SESSION_REASON_NATIVE_UNSUPPORTED_V1);
	}
	// This opens only the installed native transport. The original logical
	// operation and Runtime record window remain the receiver owner's duty.
	a->opened = true;
	a->metadata_pending = a->metadata_receiver != nullptr;
	return result(OK);
}
Error Registry::production_reserve_metadata_locked(ProductionAttachment &a, const GDExtensionProductionOperationV1 &op, const GDExtensionProductionCapacityV1 &c, const GDExtensionProductionActionV1 *actions, uint32_t action_count, const GDExtensionProductionResourceV1 *resources, uint32_t resource_count, const GDExtensionProductionUseV1 *uses, uint32_t use_count, const GDExtensionProductionReadbackV1 *readbacks, uint32_t readback_count, ProductionRequest *&out) {
	return production_reserve_metadata_rows_locked(a, op, c, { actions, nullptr }, action_count, resources, resource_count, uses, use_count, { readbacks, nullptr, op.canonical_begin }, readback_count, out, 1);
}
Error Registry::production_reserve_metadata_rows_locked(ProductionAttachment &a, const GDExtensionProductionOperationV1 &op, const GDExtensionProductionCapacityV1 &c, const ProductionActionRows &actions, uint32_t action_count, const GDExtensionProductionResourceV1 *resources, uint32_t resource_count, const GDExtensionProductionUseV1 *uses, uint32_t use_count, const ProductionReadbackRows &readbacks, uint32_t readback_count, ProductionRequest *&out, uint32_t contract_version) {
	out = nullptr;
	// The private staging caller must already have authenticated this actual
	// attachment. This method stores associations; it does not import B's scope.
	ProductionAttachment *checked = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_attachment_locked({ instance, a.serial }, true, checked, rd);
	if (e != OK || checked != &a) {
		return e == OK ? ERR_INVALID_DATA : e;
	}
	if (contract_version == 1 && (!c.suboperations || !c.resources)) {
		return ERR_INVALID_PARAMETER;
	}
	GDExtensionProductionMetadataQuoteV1 unused = {};
	e = production_layout(c, unused);
	if (e != OK) {
		return e;
	}
	if (!production_dto_valid(c) || c.requests != 1 || !capacity_fits(c, a.quote.capacity) || !production_dto_valid(op) || !op.owner_request_nonce || !op.record_generation || op.canonical_end <= op.canonical_begin || (action_count && !actions.aligned()) || (resource_count && !aligned(resources)) || (use_count && !aligned(uses)) || (readback_count && !readbacks.aligned()) || (contract_version == 1 && (!action_count || !resource_count || op.canonical_end - op.canonical_begin != action_count)) || (contract_version != 1 && contract_version != 2) || action_count > c.suboperations || resource_count > c.resources || use_count > c.resource_uses || readback_count > c.readbacks) {
		return ERR_INVALID_PARAMETER;
	}
	for (uint32_t i = 0; i < resource_count; ++i) {
		const auto &r = resources[i];
		if (r.ownership == GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1 && (contract_version != 2 || r.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1)) {
			return ERR_INVALID_DATA;
		}
		if (r.resource_kind == GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1 &&
				(r.ownership != GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1 || r.required_usage || !r.physical_bytes || r.physical_bytes > UINT32_MAX)) {
			return ERR_INVALID_DATA;
		}
		if (((r.resource_kind < GDEXTENSION_PRODUCTION_STORAGE_V1 || r.resource_kind > GDEXTENSION_PRODUCTION_SET_V1) && r.resource_kind != GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1) || r.ownership < GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1 || r.ownership > GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1 || !r.original_generation || (r.resource_kind == GDEXTENSION_PRODUCTION_UNIT_V1 && r.ownership != GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1) || (r.resource_kind == GDEXTENSION_PRODUCTION_STORAGE_V1 && !r.physical_bytes)) {
			return ERR_INVALID_DATA;
		}
	}
	for (uint32_t i = 0; i < use_count; ++i) {
		const auto &u = uses[i];
		if (u.resource_slot >= resource_count || resources[u.resource_slot].resource_kind == GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1 || u.access < GDEXTENSION_PRODUCTION_READ_V1 || u.access > GDEXTENSION_PRODUCTION_ATOMIC_V1 || (u.required_usage & ~resources[u.resource_slot].required_usage) || !range(u.byte_offset, u.byte_size, resources[u.resource_slot].physical_bytes)) {
			return ERR_INVALID_DATA;
		}
	}
	uint64_t readback_bytes = 0;
	for (uint32_t i = 0; i < readback_count; ++i) {
		const auto &r = readbacks[i];
		if (r.reserved || r.resource_slot >= resource_count || resources[r.resource_slot].resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || !r.byte_size || !range(r.byte_offset, r.byte_size, resources[r.resource_slot].physical_bytes) || r.copy_action_ordinal >= action_count || r.byte_size > c.readback_bytes - readback_bytes) {
			return ERR_INVALID_DATA;
		}
		if (r.source_kind == GDEXTENSION_PRODUCTION_READBACK_REQUEST_WRITER_V1) {
			if (r.incoming.original_generation || r.incoming.backing_serial || r.producer_action_ordinal >= r.copy_action_ordinal) {
				return ERR_INVALID_DATA;
			}
		} else if (r.source_kind == GDEXTENSION_PRODUCTION_READBACK_NATIVE_OUTPUT_GUARD_V1) {
			if (contract_version != 2 || r.incoming.original_generation || r.incoming.backing_serial || r.producer_action_ordinal >= r.copy_action_ordinal ||
					resources[r.resource_slot].ownership != GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1 || resources[r.resource_slot].physical_bytes != 32 || r.byte_offset || r.byte_size != 32) {
				return ERR_INVALID_DATA;
			}
		} else if (r.source_kind == GDEXTENSION_PRODUCTION_READBACK_RETAINED_INCOMING_V1) {
			if (contract_version != 2 || r.producer_action_ordinal || !r.incoming.original_generation || !r.incoming.backing_serial ||
					resources[r.resource_slot].ownership != GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1 ||
					resources[r.resource_slot].original_generation != r.incoming.original_generation) {
				return ERR_INVALID_DATA;
			}
		} else {
			return ERR_INVALID_DATA;
		}
		readback_bytes += r.byte_size;
	}
	for (uint32_t i = 0; i < action_count; ++i) {
		const auto &v = actions[i];
		if (v.physical_unit < resource_count && resources[v.physical_unit].resource_kind == GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1) {
			return ERR_INVALID_DATA;
		}
		if (v.action_ordinal != i || v.source_recipe_reserved || v.canonical_suboperation < op.canonical_begin || v.canonical_suboperation >= op.canonical_end || (i && v.canonical_suboperation < actions[i - 1].canonical_suboperation) || (contract_version == 1 && v.canonical_suboperation != op.canonical_begin + i) || v.action_kind < GDEXTENSION_PRODUCTION_DISPATCH_V1 || (v.action_kind > GDEXTENSION_PRODUCTION_READBACK_V1 && (contract_version != 2 || v.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1)) || v.list_boundary > GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1 || v.first_use > use_count || v.use_count > use_count - v.first_use) {
			return ERR_INVALID_DATA;
		}
		if (v.action_kind == GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 && (v.physical_unit != UINT32_MAX || v.readback_slot != UINT32_MAX || v.list_boundary != GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1)) {
			return ERR_INVALID_DATA;
		}
		if (v.action_kind == GDEXTENSION_PRODUCTION_DISPATCH_V1 && (v.physical_unit >= resource_count || resources[v.physical_unit].resource_kind != GDEXTENSION_PRODUCTION_UNIT_V1)) {
			return ERR_INVALID_DATA;
		}
		if (v.action_kind == GDEXTENSION_PRODUCTION_READBACK_V1 && (v.readback_slot >= readback_count || readbacks[v.readback_slot].copy_action_ordinal != v.action_ordinal)) {
			return ERR_INVALID_DATA;
		}
	}
	for (uint32_t i = 0; i < readback_count; ++i) {
		const bool incoming = readbacks[i].source_kind == GDEXTENSION_PRODUCTION_READBACK_RETAINED_INCOMING_V1;
		bool producer = false, copy = false;
		for (uint32_t j = 0; j < action_count; ++j) {
			if (!incoming) {
				producer |= actions[j].action_ordinal == readbacks[i].producer_action_ordinal;
			}
			copy |= actions[j].action_ordinal == readbacks[i].copy_action_ordinal && actions[j].action_kind == GDEXTENSION_PRODUCTION_READBACK_V1 && actions[j].readback_slot == i;
		}
		if ((!incoming && !producer) || !copy) {
			return ERR_INVALID_DATA;
		}
		if (readbacks[i].source_kind == GDEXTENSION_PRODUCTION_READBACK_NATIVE_OUTPUT_GUARD_V1) {
			const auto &readback = readbacks[i];
			if (actions[readback.producer_action_ordinal].action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1) { return ERR_INVALID_DATA; }
			bool covered = false;
			for (uint32_t a = 0; a < action_count; ++a) {
				for (uint32_t u = 0; u < actions[a].use_count; ++u) {
					const auto &use = uses[actions[a].first_use + u];
					if (use.resource_slot != readback.resource_slot) { continue; }
					if (a != readback.copy_action_ordinal || use.access != GDEXTENSION_PRODUCTION_READ_V1 || use.byte_offset || use.byte_size != 32) { return ERR_INVALID_DATA; }
					covered = true;
				}
			}
			if (!covered) { return ERR_INVALID_DATA; }
			const auto &copy_action = actions[readback.copy_action_ordinal];
			for (uint32_t u = 0; u < use_count; ++u) {
				if (uses[u].resource_slot == readback.resource_slot && (u < copy_action.first_use || u - copy_action.first_use >= copy_action.use_count || uses[u].access != GDEXTENSION_PRODUCTION_READ_V1 || uses[u].byte_offset || uses[u].byte_size != 32)) { return ERR_INVALID_DATA; }
			}
			for (uint32_t other = 0; other < readback_count; ++other) {
				if (other != i && readbacks[other].resource_slot == readback.resource_slot) { return ERR_INVALID_DATA; }
			}
			continue;
		}
		if (incoming) {
			const auto &readback = readbacks[i];
			const auto &action = actions[uint32_t(readback.copy_action_ordinal)];
			bool covered = false;
			for (uint32_t u = 0; u < action.use_count; ++u) {
				const auto &use = uses[action.first_use + u];
				covered |= use.resource_slot == readback.resource_slot &&
						(use.access == GDEXTENSION_PRODUCTION_READ_V1 || use.access == GDEXTENSION_PRODUCTION_READ_WRITE_V1) &&
						readback.byte_offset >= use.byte_offset && range(readback.byte_offset - use.byte_offset, readback.byte_size, use.byte_size);
			}
			if (!covered) {
				return ERR_INVALID_DATA;
			}
			continue;
		}
		if (contract_version == 2) {
			const auto &readback = readbacks[i];
			const auto &writer = actions[uint32_t(readback.producer_action_ordinal)];
			bool covered = false;
			for (uint32_t u = 0; u < writer.use_count; ++u) {
				const auto &use = uses[writer.first_use + u];
				covered |= use.resource_slot == readback.resource_slot &&
						(use.access == GDEXTENSION_PRODUCTION_WRITE_V1 || use.access == GDEXTENSION_PRODUCTION_READ_WRITE_V1 || use.access == GDEXTENSION_PRODUCTION_ATOMIC_V1) &&
						readback.byte_offset >= use.byte_offset && range(readback.byte_offset - use.byte_offset, readback.byte_size, use.byte_size);
			}
			if (!covered || writer.action_kind == GDEXTENSION_PRODUCTION_READBACK_V1 ||
					(resources[readback.resource_slot].ownership == GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1 && writer.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1) ||
					(resources[readback.resource_slot].ownership != GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1 && writer.action_kind == GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1)) {
				return ERR_INVALID_DATA;
			}
		}
	}
	// Check this request's own slice, not just the accepted attachment pool.
	// Distinct declared storage uses bound exact worker dependencies per action.
	uint32_t worker_bound = 0;
	for (uint32_t i = 0; i < action_count; ++i) {
		const auto &action = actions[i];
		uint32_t storages = 0;
		for (uint32_t u = 0; u < action.use_count; ++u) {
			const uint32_t slot = uses[action.first_use + u].resource_slot;
			if (resources[slot].resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1) {
				continue;
			}
			bool duplicate = false;
			for (uint32_t before = 0; before < u; ++before) {
				duplicate |= uses[action.first_use + before].resource_slot == slot;
			}
			storages += !duplicate;
		}
		worker_bound = MAX(worker_bound, storages);
	}
	if (!rd || !rd->_thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	{
		RdUnlock unlock{ rd->_thread_safe_ };
		bool native_output = false;
		for (uint32_t i = 0; i < action_count; ++i) {
			native_output |= actions[i].action_kind == GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1;
		}
		worker_bound = native_output ? rd->transfer_worker_pool_max_size : MIN(worker_bound, rd->transfer_worker_pool_max_size);
		uint32_t memberships = 0;
		e = production_membership_bound_locked(*rd, c.graph_memberships, worker_bound, memberships);
		if (e != OK) {
			return e;
		}
		if (memberships > c.native_memberships) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
	}
	ProductionRequest *r = nullptr;
	for (uint32_t i = 0; i < a.quote.capacity.requests; ++i) {
		auto &candidate = a.requests[i];
		if (candidate.serial && candidate.operation.owner_request_nonce == op.owner_request_nonce && candidate.operation.record_generation == op.record_generation) {
			return ERR_ALREADY_IN_USE;
		}
		if (!candidate.serial) {
			r = &candidate;
		}
	}
	if (!r || production_serials.exhausted) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	auto free_cells = [](auto *cells, uint32_t total, uint32_t count) {
		using T = std::remove_pointer_t<decltype(cells)>;
		if (!count) {
			return cells;
		}
		uint32_t run = 0;
		for (uint32_t i = 0; i < total; ++i) {
			run = cells[i].request ? 0 : run + 1;
			if (run == count) {
				return &cells[i + 1 - count];
			}
		}
		return static_cast<T *>(nullptr);
	};
	auto *ac = free_cells(a.actions, a.quote.capacity.suboperations, c.suboperations);
	auto *rc = free_cells(a.resources, a.quote.capacity.resources, c.resources);
	auto *uc = free_cells(a.uses, a.quote.capacity.resource_uses, c.resource_uses);
	auto *bc = free_cells(a.readbacks, a.quote.capacity.readbacks, c.readbacks);
	auto *gc = free_cells(a.graphs, a.quote.capacity.graph_memberships, c.graph_memberships);
	auto *nc = free_cells(a.natives, a.quote.capacity.native_memberships, c.native_memberships);
	auto *tc = free_cells(a.returns, a.quote.capacity.return_cells, c.return_cells);
	GDExtensionProductionMetadataQuoteV1 request_quote = {};
	e = production_device_quote_locked(*rd, c, request_quote, worker_bound);
	if (e != OK) {
		return e;
	}
	auto *cc = free_cells(a.commands, uint32_t(a.quote.graph_slots), uint32_t(request_quote.graph_slots));
	auto *fc = free_cells(a.frames, uint32_t(a.quote.graph_frames), uint32_t(request_quote.graph_frames));
	if ((c.suboperations && !ac) || (c.resources && !rc) || (c.resource_uses && !uc) || (c.readbacks && !bc) || (c.graph_memberships && !gc) || (c.native_memberships && !nc) || (c.return_cells && !tc) || (request_quote.graph_slots && !cc) || (request_quote.graph_frames && !fc)) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	auto free_bytes = [&](uint64_t bytes, uint64_t total, bool readback, uint64_t &offset) {
		offset = readback ? 0 : a.scene_prefix_bytes;
		for (;;) {
			if (!range(offset, bytes, total)) {
				return false;
			}
			bool moved = false;
			for (uint32_t i = 0; i < a.quote.capacity.requests; ++i) {
				const auto &q = a.requests[i];
				if (!q.serial) {
					continue;
				}
				uint64_t start = readback ? q.readback_offset : q.payload_offset, size = readback ? q.capacity.readback_bytes : q.capacity.retained_payload_bytes;
				if (bytes && size && offset < start + size && start < offset + bytes) {
					offset = start + size;
					moved = true;
					break;
				}
			}
			if (!moved) {
				return true;
			}
		}
	};
	uint64_t payload = 0, readback = 0;
	if (!free_bytes(c.retained_payload_bytes, a.quote.capacity.retained_payload_bytes, false, payload) || !free_bytes(c.readback_bytes, a.quote.capacity.readback_bytes, true, readback)) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	// All validation and every finite slice succeed before publishing any cell.
	a.request_reserved_once = true;
	production_serials.take(r->serial);
	r->attachment = &a;
	r->operation = op;
	r->contract_version = contract_version;
	r->capacity = c;
	r->native_workers_bound = worker_bound;
	r->actions = ac;
	r->resources = rc;
	r->uses = uc;
	r->readbacks = bc;
	r->graphs = gc;
	r->natives = nc;
	r->returns = tc;
	r->commands = cc;
	r->frames = fc;
	r->command_count = uint32_t(request_quote.graph_slots);
	r->frame_count = uint32_t(request_quote.graph_frames);
	r->action_count = action_count;
	r->resource_count = resource_count;
	r->use_count = use_count;
	r->readback_count = readback_count;
	r->payload_offset = payload;
	r->readback_offset = readback;
	auto claim = [&](auto *cells, uint32_t count) { for(uint32_t i=0;i<count;++i){cells[i].request=r;
} };
	claim(ac, c.suboperations);
	claim(rc, c.resources);
	claim(uc, c.resource_uses);
	claim(bc, c.readbacks);
	claim(gc, c.graph_memberships);
	claim(nc, c.native_memberships);
	claim(tc, c.return_cells);
	claim(cc, r->command_count);
	claim(fc, r->frame_count);
	for (uint32_t i = 0; i < action_count; ++i) {
		ac[i].declaration = actions[i];
	}
	for (uint32_t i = 0; i < resource_count; ++i) {
		rc[i].declaration = resources[i];
		rc[i].request_serial = r->serial;
	}
	for (uint32_t i = 0; i < use_count; ++i) {
		uc[i].declaration = uses[i];
	}
	uint64_t rb = readback;
	for (uint32_t i = 0; i < readback_count; ++i) {
		bc[i].declaration = readbacks[i];
		bc[i].bytes_offset = rb;
		bc[i].capture.state = &bc[i].source;
		bc[i].capture.registry = this;
		bc[i].capture.request_serial = r->serial;
		bc[i].capture.bytes = a.readback_bytes + rb;
		bc[i].capture.byte_capacity = readbacks[i].byte_size;
		bc[i].source.request_serial = r->serial;
		rb += readbacks[i].byte_size;
	}
	++a.children;
	out = r;
	return OK;
}
Error Registry::production_contract_check(GDExtensionProductionRequestRefV1 ref, uint32_t expected, uint64_t *begin, bool *resolved, bool admit) {
	if (resolved) {
		*resolved = false;
	}
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	const Error error = production_request_locked(ref, admit, request, device);
	if (error != OK) {
		// Preserve the original V1 admission error for V1 requests, while an
		// authenticated request from the other wire contract is always rejected
		// as a version mismatch, even while its immutable control is returned.
		if (admit) {
			ProductionRequest *retained = nullptr;
			RenderingDevice *retained_device = nullptr;
			if (production_request_locked(ref, false, retained, retained_device) == OK && retained->contract_version != expected) {
				if (resolved) {
					*resolved = true;
				}
				return ERR_INVALID_DATA;
			}
		}
		return error;
	}
	if (resolved) {
		*resolved = true;
	}
	if (request->contract_version != expected) {
		return ERR_INVALID_DATA;
	}
	if (begin) {
		*begin = request->operation.canonical_begin;
	}
	return OK;
}

Error Registry::production_action_locked(ProductionRequest &r, uint64_t ordinal, uint32_t kind, ProductionActionCell *&out) {
	out = nullptr;
	if (r.cancelled || r.frozen || r.native_static_sealed) {
		return ERR_BUSY;
	}
	if (ordinal >= r.action_count) {
		return ERR_DOES_NOT_EXIST;
	}
	auto &a = r.actions[uint32_t(ordinal)];
	if (a.request != &r || a.declaration.action_ordinal != ordinal || a.declaration.action_kind != kind) {
		return ERR_INVALID_DATA;
	}
	out = &a;
	return OK;
}
bool Registry::production_use_matches_locked(const ProductionRequest &r, const ProductionActionCell &a, uint32_t slot, uint64_t offset, uint64_t bytes, uint32_t access) const {
	const auto &d = a.declaration;
	if (d.first_use > r.use_count || d.use_count > r.use_count - d.first_use) {
		return false;
	}
	for (uint32_t i = 0; i < d.use_count; ++i) {
		const auto &cell = r.uses[d.first_use + i];
		const auto &u = cell.declaration;
		if (cell.request != &r || u.resource_slot != slot || (u.access != access && u.access != GDEXTENSION_PRODUCTION_READ_WRITE_V1) || offset < u.byte_offset) {
			continue;
		}
		if (range(offset - u.byte_offset, bytes, u.byte_size)) {
			return true;
		}
	}
	return false;
}
bool Registry::production_has_incoming_readbacks_locked(const ProductionRequest &r) const {
	for (uint32_t i = 0; i < r.readback_count; ++i) {
		if (r.readbacks[i].declaration.source_kind == GDEXTENSION_PRODUCTION_READBACK_RETAINED_INCOMING_V1) {
			return true;
		}
	}
	return false;
}

Error Registry::production_freeze_locked(ProductionRequest &r) {
	// The original schedule path holds Registry only. Incoming reflection and
	// physical identity inspection additionally require this exact RD lock.
	if ((r.contract_version != 2 && !production_has_incoming_readbacks_locked(r)) || r.frozen || r.cancelled) {
		return production_freeze_rd_locked(r, nullptr);
	}
	RenderingDevice *rd = r.attachment ? r.attachment->held_device : nullptr;
	if (!rd || !rd->_thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	RdUnlock unlock{ rd->_thread_safe_ };
	return production_freeze_rd_locked(r, rd);
}

// Strict graph suffix pricing is required only for the prepared guard path.
bool ProductionRequest::guarded_schedule_required() const {
	if (contract_version != 2 || !actions) {
		return false;
	}
	for (uint32_t i = 0; i < action_count; ++i) {
		const ProductionActionCell &action = actions[i];
		if (action.output_plan_prepared || action.output_acquire_action_ordinal != UINT64_MAX) {
			return true;
		}
	}
	return false;
}

// Registry is held; rd, when supplied, is already locked by the original
// schedule or append caller. This helper never takes a recursive RD lock.
Error Registry::production_freeze_rd_locked(ProductionRequest &r, RenderingDevice *rd, bool p_stage_only) {
	if (r.cancelled) {
		return ERR_BUSY;
	}
	if (r.frozen) {
		return OK;
	}
	for (uint32_t i = 0; i < r.action_count; ++i) {
		if (!r.actions[i].prepared || (!p_stage_only && r.actions[i].output_camera_pending)) {
			return ERR_UNCONFIGURED;
		}
	}
	for (uint32_t i = 0; i < r.resource_count; ++i) {
		const auto &resource = r.resources[i];
		if (resource.output_guard_readonly) {
			if (resource.output_guard_history) {
				if (!production_output_guard_matches_locked(r, resource, *rd)) { return ERR_INVALID_DATA; }
				continue;
			}
			if (resource.output_alias_prepared || resource.owned_serial || resource.use_count || resource.declaration.physical_bytes != 32) { return ERR_INVALID_DATA; }
			{
				if (resource.imported || resource.output_guard_finalizer >= r.action_count) { return ERR_INVALID_DATA; }
				const auto &finalizer = r.actions[resource.output_guard_finalizer];
				if (finalizer.output_acquire_action_ordinal >= resource.output_guard_finalizer) { return ERR_INVALID_DATA; }
				const auto &acquire = r.actions[finalizer.output_acquire_action_ordinal];
				if (!acquire.output_plan_prepared || acquire.output_plan.finalize_action_ordinal != resource.output_guard_finalizer ||
						!production_output_guard_origin_matches_locked(r, resource, acquire)) { return ERR_INVALID_DATA; }
			}
			continue;
		}
		if (resource.output_alias_prepared) {
			if (resource.declaration.ownership != GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1 || resource.output_action_ordinal >= r.action_count || !r.actions[resource.output_action_ordinal].prepared || resource.imported || resource.owned_serial || resource.use_count) {
				return ERR_INVALID_DATA;
			}
			bool exact_role = false;
			for (const auto &role : r.actions[resource.output_action_ordinal].output_roles) {
				exact_role |= role.bytes && role.resource_slot == i && role.bytes == resource.declaration.physical_bytes && role.generation == resource.declaration.original_generation;
			}
			if (!exact_role) {
				return ERR_INVALID_DATA;
			}
			continue;
		}
		if (resource.output_set_deferred) {
			if (resource.declaration.resource_kind != GDEXTENSION_PRODUCTION_SET_V1 || resource.declaration.ownership != GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1 || !resource.owned_serial || resource.imported || resource.creating || resource.use_count || resource.first_binding > r.capacity.resource_uses || resource.binding_count > r.capacity.resource_uses - resource.first_binding) {
				return ERR_INVALID_DATA;
			}
			bool has_alias = false;
			for (uint32_t b = 0; b < resource.binding_count; ++b) {
				const auto slot = r.uses[resource.first_binding + b].binding.resource_slot;
				if (slot >= r.resource_count || (!r.resources[slot].imported && !r.resources[slot].output_alias_prepared)) {
					return ERR_INVALID_DATA;
				}
				has_alias |= r.resources[slot].output_alias_prepared;
			}
			if (!has_alias) {
				return ERR_INVALID_DATA;
			}
			continue;
		}
		if (!resource.imported && resource.declaration.resource_kind != GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1) {
			return ERR_UNCONFIGURED;
		}
	}
	if (r.contract_version == 2) {
		for (uint32_t i = 0; i < r.readback_count; ++i) {
			const auto &readback = r.readbacks[i].declaration;
			if (readback.source_kind == GDEXTENSION_PRODUCTION_READBACK_RETAINED_INCOMING_V1) {
				continue; // Real incoming validation below, before frozen publishes.
			}
			if (readback.producer_action_ordinal >= readback.copy_action_ordinal || readback.copy_action_ordinal >= r.action_count || readback.resource_slot >= r.resource_count) {
				return ERR_INVALID_DATA;
			}
			const auto &writer = r.actions[readback.producer_action_ordinal];
			const auto &resource = r.resources[readback.resource_slot];
			if (readback.source_kind == GDEXTENSION_PRODUCTION_READBACK_NATIVE_OUTPUT_GUARD_V1) {
				if (!resource.output_guard_readonly || resource.output_guard_history || resource.output_guard_finalizer != readback.producer_action_ordinal || resource.output_guard_consumer != readback.copy_action_ordinal || readback.byte_offset || readback.byte_size != 32) { return ERR_INVALID_DATA; }
				continue;
			}
			if (resource.output_alias_prepared) {
				// Freeze validates only the original unbound alias plan. READBACK
				// recording stays gated pending real factory/witness integration.
				const auto &copy = r.actions[readback.copy_action_ordinal];
				if (resource.request != &r || resource.request_serial != r.serial ||
						resource.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 ||
						resource.declaration.ownership != GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1 ||
						resource.output_action_ordinal != readback.producer_action_ordinal ||
						writer.request != &r || writer.declaration.action_ordinal != readback.producer_action_ordinal ||
						writer.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 || !writer.output_source_prepared ||
						copy.request != &r || copy.declaration.action_ordinal != readback.copy_action_ordinal ||
						copy.declaration.action_kind != GDEXTENSION_PRODUCTION_READBACK_V1 || copy.readback_slot != i ||
						copy.declaration.readback_slot != i || !readback.byte_size ||
						!range(readback.byte_offset, readback.byte_size, resource.declaration.physical_bytes) ||
						!production_use_matches_locked(r, copy, readback.resource_slot, readback.byte_offset, readback.byte_size, GDEXTENSION_PRODUCTION_READ_V1)) {
					return ERR_INVALID_DATA;
				}
				bool covered = false;
				for (const auto &role : writer.output_roles) {
					if (!role.bytes || role.resource_slot != readback.resource_slot || role.use_index >= r.use_count ||
							role.use_index < writer.declaration.first_use || role.use_index - writer.declaration.first_use >= writer.declaration.use_count) {
						continue;
					}
					const auto &use = r.uses[role.use_index];
					covered |= role.bytes == resource.declaration.physical_bytes && role.generation == resource.declaration.original_generation &&
							use.request == &r && use.declaration.resource_slot == readback.resource_slot &&
							use.declaration.access == GDEXTENSION_PRODUCTION_WRITE_V1 && !use.declaration.byte_offset &&
							use.declaration.byte_size == role.bytes && range(readback.byte_offset, readback.byte_size, role.bytes);
				}
				if (!covered) {
					return ERR_INVALID_DATA;
				}
				continue;
			}
			// Original import/create already authenticated storage generation,
			// RID and backing serial and retained the exact physical dependency.
			// Native record rechecks that same RID/backing before any effect.
			if (resource.request != &r || resource.request_serial != r.serial || !resource.imported || !resource.primary.is_valid() || !resource.backing_serial || resource.use_count != 1 || !resource.uses[0].linked() || !resource.uses[0].same_physical()) {
				return ERR_INVALID_DATA;
			}
			bool covered = false;
			if (writer.declaration.action_kind == GDEXTENSION_PRODUCTION_UPLOAD_V1) {
				const auto &v = writer.upload;
				covered = v.destination_slot == readback.resource_slot && readback.byte_offset >= v.destination_offset && range(readback.byte_offset - v.destination_offset, readback.byte_size, v.bytes);
			} else if (writer.declaration.action_kind == GDEXTENSION_PRODUCTION_COPY_V1) {
				const auto &v = writer.copy;
				covered = v.destination_slot == readback.resource_slot && readback.byte_offset >= v.destination_offset && range(readback.byte_offset - v.destination_offset, readback.byte_size, v.bytes);
			} else if (writer.declaration.action_kind == GDEXTENSION_PRODUCTION_DISPATCH_V1) {
				// Exact writable set/shader closure was checked by prepare_dispatch.
				// This does not claim that the shader produced a logical source value.
				for (uint32_t u = 0; u < writer.declaration.use_count; ++u) {
					const auto &use = r.uses[writer.declaration.first_use + u].declaration;
					covered |= use.resource_slot == readback.resource_slot && (use.access == GDEXTENSION_PRODUCTION_WRITE_V1 || use.access == GDEXTENSION_PRODUCTION_READ_WRITE_V1 || use.access == GDEXTENSION_PRODUCTION_ATOMIC_V1) && readback.byte_offset >= use.byte_offset && range(readback.byte_offset - use.byte_offset, readback.byte_size, use.byte_size);
				}
			}
			if (!covered) {
				return ERR_INVALID_DATA;
			}
		}
	}
	if (production_has_incoming_readbacks_locked(r)) {
		if (!rd) {
			return ERR_UNAVAILABLE;
		}
		const Error incoming = production_incoming_readbacks_validate_locked(r, *rd, 0);
		if (incoming != OK) {
			return incoming;
		}
	}
	if (r.guarded_schedule_required()) {
		if (!rd) {
			return ERR_UNAVAILABLE;
		}
		const Error schedule = production_schedule_locked(r, *rd, true);
		if (schedule != OK) {
			return schedule;
		}
	}
	if (p_stage_only) {
		r.native_static_sealed = true;
	} else {
		r.frozen = true;
	}
	return OK;
}
GDExtensionDeviceSessionResultV1 Registry::production_prepare_dispatch_v2(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionDispatchV2 *input, const void *push, uint32_t bytes, uint32_t expected_contract, bool history_fallback) {
	const Error contract = production_contract_check(ref, expected_contract);
	if (contract != OK) {
		return { int32_t(contract), 0, 0, 0 };
	}
	if (!aligned(input) || !production_dto_valid(*input) || input->source_recipe_reserved || input->reserved || input->push_bytes != bytes || (bytes && !push) || !input->groups_x || !input->groups_y || !input->groups_z) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	const auto value = *input;
	Lock lock(*this);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_request_locked(ref, true, r, rd);
	if (e != OK) {
		return result(e, 16);
	}
	ProductionActionCell *a = nullptr;
	e = production_action_locked(*r, value.action_ordinal, GDEXTENSION_PRODUCTION_DISPATCH_V1, a);
	if (e != OK) {
		return result(e, 16);
	}
	if (history_fallback) {
		if (!r->native_history_fallback || r->native_static_sealed || r->native_history_selected || r->action_count > r->capacity.suboperations / 2) { return result(ERR_INVALID_DATA); }
		a = &r->actions[r->action_count + value.action_ordinal];
		if (a->request != r || a->declaration.action_kind != GDEXTENSION_PRODUCTION_DISPATCH_V1) { return result(ERR_INVALID_DATA); }
	}
	if (value.canonical_suboperation != a->declaration.canonical_suboperation || value.source_recipe_substep != a->declaration.source_recipe_substep) {
		return result(ERR_INVALID_DATA);
	}
	if (value.unit_slot >= r->resource_count || value.unit_slot != a->declaration.physical_unit || r->resources[value.unit_slot].declaration.resource_kind != GDEXTENSION_PRODUCTION_UNIT_V1 || value.first_set_slot > r->resource_count || value.set_count > r->resource_count - value.first_set_slot) {
		return result(ERR_INVALID_DATA, 2);
	}
	for (uint32_t i = 0; i < value.set_count; ++i) {
		if (r->resources[value.first_set_slot + i].declaration.resource_kind != GDEXTENSION_PRODUCTION_SET_V1) {
			return result(ERR_INVALID_DATA, 2);
		}
	}
	if (!rd->_thread_safe_.try_lock()) {
		return result(ERR_BUSY, 16);
	}
	RdUnlock unlock{ rd->_thread_safe_ };
	if (bytes > rd->limit_get(RD::LIMIT_MAX_PUSH_CONSTANT_SIZE) || bytes > RenderingDevice::MAX_PUSH_CONSTANT_SIZE || value.groups_x > rd->limit_get(RD::LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X) || value.groups_y > rd->limit_get(RD::LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Y) || value.groups_z > rd->limit_get(RD::LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Z)) {
		return result(ERR_PARAMETER_RANGE_ERROR, 19);
	}
	e = production_dispatch_closure_locked(*r, *rd, *a, value);
	if (e != OK) {
		return result(e, 2);
	}
	if (a->prepared) {
		const auto &v = a->dispatch;
		bool same = v.unit_slot == value.unit_slot && v.first_set_slot == value.first_set_slot && v.set_count == value.set_count && v.groups_x == value.groups_x && v.groups_y == value.groups_y && v.groups_z == value.groups_z && v.push_bytes == bytes && (!bytes || !std::memcmp(r->attachment->payload + a->payload_offset, push, bytes));
		return result(same ? OK : ERR_INVALID_DATA, same ? 0 : 2);
	}
	if (!range(r->payload_used, bytes, r->capacity.retained_payload_bytes)) {
		return result(ERR_PARAMETER_RANGE_ERROR, 19);
	}
	a->payload_offset = r->payload_offset + r->payload_used;
	a->payload_bytes = bytes;
	if (bytes) {
		std::memcpy(r->attachment->payload + a->payload_offset, push, bytes);
	}
	r->payload_used += bytes;
	a->dispatch = value;
	a->prepared = true;
	return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::production_prepare_copy_v2(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionCopyV2 *input, uint32_t expected_contract) {
	const Error contract = production_contract_check(ref, expected_contract);
	if (contract != OK) {
		return { int32_t(contract), 0, 0, 0 };
	}
	if (!aligned(input) || !production_dto_valid(*input) || input->source_recipe_reserved || !input->bytes) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	const auto value = *input;
	Lock lock(*this);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_request_locked(ref, true, r, rd);
	if (e != OK) {
		return result(e, 16);
	}
	ProductionActionCell *a = nullptr;
	e = production_action_locked(*r, value.action_ordinal, GDEXTENSION_PRODUCTION_COPY_V1, a);
	if (e != OK) {
		return result(e, 16);
	}
	if (value.canonical_suboperation != a->declaration.canonical_suboperation || value.source_recipe_substep != a->declaration.source_recipe_substep) {
		return result(ERR_INVALID_DATA);
	}
	if (value.source_slot >= r->resource_count || value.destination_slot >= r->resource_count) {
		return result(ERR_INVALID_DATA, 2);
	}
	const auto &source = r->resources[value.source_slot].declaration, &dest = r->resources[value.destination_slot].declaration;
	if (source.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || dest.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || !range(value.source_offset, value.bytes, source.physical_bytes) || !range(value.destination_offset, value.bytes, dest.physical_bytes)) {
		return result(ERR_INVALID_DATA, 2);
	}
	if (!production_use_matches_locked(*r, *a, value.source_slot, value.source_offset, value.bytes, GDEXTENSION_PRODUCTION_READ_V1) || !production_use_matches_locked(*r, *a, value.destination_slot, value.destination_offset, value.bytes, GDEXTENSION_PRODUCTION_WRITE_V1)) {
		return result(ERR_INVALID_DATA, 2);
	}
	if (value.source_slot == value.destination_slot && value.source_offset < value.destination_offset + value.bytes && value.destination_offset < value.source_offset + value.bytes) {
		return result(ERR_INVALID_DATA, 2);
	}
	if (a->prepared) {
		const auto &v = a->copy;
		return result(v.source_slot == value.source_slot && v.destination_slot == value.destination_slot && v.source_offset == value.source_offset && v.destination_offset == value.destination_offset && v.bytes == value.bytes ? OK : ERR_INVALID_DATA);
	}
	a->copy = value;
	a->prepared = true;
	return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::production_prepare_upload_v2(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionUploadV2 *input, const void *data, uint64_t bytes, uint32_t expected_contract, bool history_fallback) {
	const Error contract = production_contract_check(ref, expected_contract);
	if (contract != OK) {
		return { int32_t(contract), 0, 0, 0 };
	}
	if (!aligned(input) || !production_dto_valid(*input) || input->source_recipe_reserved || input->reserved || !bytes || !data || bytes != input->bytes || bytes > SIZE_MAX) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	const auto value = *input;
	Lock lock(*this);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_request_locked(ref, true, r, rd);
	if (e != OK) {
		return result(e, 16);
	}
	return production_prepare_upload_locked(*r, value, data, bytes, history_fallback);
}
GDExtensionDeviceSessionResultV1 Registry::production_prepare_upload_locked(ProductionRequest &request, const GDExtensionProductionUploadV2 &value, const void *data, uint64_t bytes, bool history_fallback) {
	ProductionRequest *r = &request;
	if (!production_dto_valid(value) || value.reserved || value.source_recipe_reserved || !bytes || !data || bytes != value.bytes || bytes > SIZE_MAX) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	ProductionActionCell *a = nullptr;
	Error e = production_action_locked(*r, value.action_ordinal, GDEXTENSION_PRODUCTION_UPLOAD_V1, a);
	if (e != OK) {
		return result(e, 16);
	}
	if (history_fallback) {
		if (!r->native_history_fallback || r->native_static_sealed || r->native_history_selected || r->action_count > r->capacity.suboperations / 2) { return result(ERR_INVALID_DATA); }
		a = &r->actions[r->action_count + value.action_ordinal];
		if (a->request != r || a->declaration.action_kind != GDEXTENSION_PRODUCTION_UPLOAD_V1) { return result(ERR_INVALID_DATA); }
	}
	if (value.canonical_suboperation != a->declaration.canonical_suboperation || value.source_recipe_substep != a->declaration.source_recipe_substep) {
		return result(ERR_INVALID_DATA);
	}
	if (value.destination_slot >= r->resource_count || r->resources[value.destination_slot].declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || !range(value.destination_offset, bytes, r->resources[value.destination_slot].declaration.physical_bytes)) {
		return result(ERR_INVALID_DATA, 2);
	}
	if (!production_use_matches_locked(*r, *a, value.destination_slot, value.destination_offset, bytes, GDEXTENSION_PRODUCTION_WRITE_V1)) {
		return result(ERR_INVALID_DATA, 2);
	}
	if (a->prepared) {
		const auto &v = a->upload;
		return result(v.destination_slot == value.destination_slot && v.destination_offset == value.destination_offset && v.bytes == bytes && !std::memcmp(r->attachment->payload + a->payload_offset, data, size_t(bytes)) ? OK : ERR_INVALID_DATA);
	}
	if (!range(r->payload_used, bytes, r->capacity.retained_payload_bytes)) {
		return result(ERR_PARAMETER_RANGE_ERROR, 19);
	}
	a->payload_offset = r->payload_offset + r->payload_used;
	a->payload_bytes = bytes;
	std::memcpy(r->attachment->payload + a->payload_offset, data, size_t(bytes));
	r->payload_used += bytes;
	a->upload = value;
	a->prepared = true;
	return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::production_prepare_readback_v2(GDExtensionProductionRequestRefV1 ref, uint64_t ordinal, uint32_t slot, uint32_t expected_contract) {
	const Error contract = production_contract_check(ref, expected_contract);
	if (contract != OK) {
		return { int32_t(contract), 0, 0, 0 };
	}
	Lock lock(*this);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_request_locked(ref, true, r, rd);
	if (e != OK) {
		return result(e, 16);
	}
	ProductionActionCell *a = nullptr;
	e = production_action_locked(*r, ordinal, GDEXTENSION_PRODUCTION_READBACK_V1, a);
	if (e != OK) {
		return result(e, 16);
	}
	if (slot >= r->readback_count || a->declaration.readback_slot != slot || r->readbacks[slot].declaration.copy_action_ordinal != ordinal) {
		return result(ERR_INVALID_DATA, 2);
	}
	const auto &readback = r->readbacks[slot].declaration;
	if (!production_use_matches_locked(*r, *a, readback.resource_slot, readback.byte_offset, readback.byte_size, GDEXTENSION_PRODUCTION_READ_V1)) {
		return result(ERR_INVALID_DATA, 2);
	}
	if (a->prepared) {
		return result(a->readback_slot == slot ? OK : ERR_INVALID_DATA);
	}
	if (readback.source_kind == GDEXTENSION_PRODUCTION_READBACK_NATIVE_OUTPUT_GUARD_V1) {
		auto &resource = r->resources[readback.resource_slot];
		if (resource.imported || resource.owned_serial || resource.use_count || resource.output_alias_prepared ||
				(resource.output_guard_readonly && (resource.output_guard_finalizer != readback.producer_action_ordinal || resource.output_guard_consumer != ordinal))) {
			return result(ERR_INVALID_DATA, 2);
		}
		if (resource.output_guard_history) { return result(ERR_INVALID_DATA); }
		resource.output_guard_readonly = true;
		resource.output_guard_finalizer = readback.producer_action_ordinal;
		resource.output_guard_consumer = ordinal;
	}
	a->readback_slot = slot;
	a->prepared = true;
	return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::production_control_receipt(GDExtensionProductionRequestRefV1 ref, uint64_t expected, GDExtensionProductionControlReceiptV1 *out) {
	if (!aligned(out)) {
		return result(ERR_INVALID_PARAMETER);
	}
	*out = {};
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	const Error error = production_request_locked(ref, false, request, rd);
	if (error != OK) {
		return result(error);
	}
	const auto &cell = request->control;
	if (expected && cell.serial != expected) {
		return result(ERR_DOES_NOT_EXIST);
	}
	GDExtensionProductionControlReceiptV1 receipt{};
	receipt.abi = production_dto_abi<GDExtensionProductionControlReceiptV1>();
	receipt.registry_instance = instance;
	receipt.request_serial = request->serial;
	receipt.control_serial = cell.serial;
	if (!cell.serial) {
		*out = receipt; // NONE: reservation alone never admitted a control.
		return result(OK);
	}
	if (cell.input.request_serial != request->serial) {
		return result(ERR_INVALID_DATA);
	}
	switch (cell.input.kind) {
		case ProductionControlKind::OBSERVE: receipt.kind = GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_KIND_OBSERVE_V1; break;
		case ProductionControlKind::CANCEL: receipt.kind = GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_KIND_CANCEL_V1; break;
		case ProductionControlKind::RELEASE: receipt.kind = GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_KIND_RELEASE_V1; break;
		case ProductionControlKind::STORAGE_CREATE: receipt.kind = GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_KIND_STORAGE_CREATE_V1; break;
		case ProductionControlKind::SET_CREATE: receipt.kind = GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_KIND_SET_CREATE_V1; break;
		case ProductionControlKind::RASTER_CREATE: receipt.kind = GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_KIND_RASTER_CREATE_V1; break;
		case ProductionControlKind::RECORD: receipt.kind = GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_KIND_RECORD_V1; break;
	}
	switch (cell.state) {
		case ProductionControlState::PENDING: receipt.state = GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_PENDING_V1; break;
		case ProductionControlState::QUEUED: receipt.state = GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_QUEUED_V1; break;
		case ProductionControlState::EXECUTING: receipt.state = GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_EXECUTING_V1; break;
		case ProductionControlState::RETURNED: receipt.state = GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_RETURNED_V1; break;
		case ProductionControlState::OWNER_CONSUMED: receipt.state = GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_OWNER_CONSUMED_V1; break;
	}
	if (cell.record_history.schedule_result_valid) {
		receipt.schedule_result_valid = 1;
		receipt.last_schedule_result = cell.record_history.last_schedule_result;
	}
	if (cell.state == ProductionControlState::RETURNED || cell.state == ProductionControlState::OWNER_CONSUMED) {
		if (cell.output.request_serial != request->serial || cell.output.control_serial != cell.serial) {
			return result(ERR_INVALID_DATA);
		}
		receipt.result_valid = 1;
		receipt.control_result = cell.output.result;
	}
	*out = receipt;
	return result(OK);
}

Error Registry::production_observe_service(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionServiceBudgetV1 &budget) {
	// Zero query has no queue admission, ref retention, or native service.
	if (!budget.controls || !budget.receipts || !budget.passes || !budget.deadline_micros) {
		return OK;
	}
	uint64_t serial = 0;
	bool returned = false;
	{
		Lock lock(*this);
		ProductionRequest *request = nullptr;
		RenderingDevice *rd = nullptr;
		const Error error = production_request_locked(ref, false, request, rd);
		if (error != OK) {
			return error;
		}
		auto &cell = request->control;
		if (cell.state != ProductionControlState::OWNER_CONSUMED) {
			if (cell.input.kind != ProductionControlKind::OBSERVE || cell.input.consumed_operation) {
				return ERR_BUSY;
			}
			serial = cell.serial;
			if (cell.state == ProductionControlState::PENDING && !service_budget_active(cell.input.budget)) {
				// This exact turn never published. Return only its proven no-effect
				// result; a retry cannot renew the original immutable authorization.
				cell.output = {};
				cell.output.request_serial = request->serial;
				cell.output.control_serial = cell.serial;
				cell.output.result = result(OK);
				production_report_locked(*request, cell.output.report);
				cell.state = ProductionControlState::RETURNED;
			}
			returned = cell.state == ProductionControlState::RETURNED;
			if (returned && cell.output.output_generation.native_released) {
				// Only an exact original consumer accepting the full internal result
				// may consume it. The current public observe DTO has no such field.
				return ERR_UNAVAILABLE;
			}
		}
	}
	if (returned) {
		ProductionControlOutput output = {};
		const Error error = production_control_consume(ref, serial, &output);
		if (error != OK) {
			return error;
		}
		// A nonzero retry consumes only the original turn, regardless of its
		// offered deadline/reduction. A fresh turn requires a subsequent call.
		return Error(output.result.native_error);
	}
	// Expiry cannot report a successful service return while an original turn
	// remains outstanding. Zero queries above deliberately preserve custody.
	if (!service_budget_active(budget)) {
		return serial ? ERR_BUSY : OK;
	}
	if (!serial) {
		const Error error = production_control_accept(ref, ProductionControlKind::OBSERVE, &serial, &budget);
		if (error != OK) {
			return error;
		}
	}
	// A live caller reduction may retry the retained immutable pending turn.
	// Even owner == RT publishes to the original FIFO and never executes inline.
	const Error scheduled = production_control_schedule(ref, serial);
	if (scheduled != OK) {
		return scheduled;
	}
	return ERR_BUSY;
}

namespace {
template <typename Input, typename Output>
bool production_observe_disjoint(const Input *p_input, const Output *p_output) {
	const uintptr_t a = uintptr_t(p_input), b = uintptr_t(p_output);
	return a <= UINTPTR_MAX - sizeof(Input) && b <= UINTPTR_MAX - sizeof(Output) &&
			(a + sizeof(Input) <= b || b + sizeof(Output) <= a);
}
GDExtensionProductionOutputGenerationResultV1 project_output_generation(const ProductionOutputGenerationResult &p_original) {
	GDExtensionProductionOutputGenerationResultV1 out{};
	out.abi = production_dto_abi<GDExtensionProductionOutputGenerationResultV1>();
	if (!p_original.native_released) {
		return out;
	}
	out.registry_instance = p_original.registry_instance;
	out.request_serial = p_original.request_serial;
	out.control_serial = p_original.control_serial;
	out.root_serial = p_original.root_serial;
	out.particles = p_original.particles;
	out.generation = p_original.generation;
	out.root_source = p_original.root_source;
	out.unused_slots = p_original.unused_slots;
	out.returned_slots = p_original.returned_slots;
	out.terminal_kind = 1; // Original generation Released, never per-slot/E05 ACK.
	out.native_released = 1;
	return out;
}
} // namespace

GDExtensionDeviceSessionResultV1 Registry::production_observe_v3(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionServiceBudgetV1 *budget, GDExtensionProductionObserveResultV3 *out) {
	const Error contract = production_contract_check(ref, 2);
	if (contract != OK) {
		return result(contract);
	}
	if (!aligned(budget) || !production_dto_valid(*budget) || budget->reserved || !aligned(out) || !production_observe_disjoint(budget, out)) {
		return result(ERR_INVALID_PARAMETER);
	}
	// Existing service keeps terminal results RETURNED for this exact consumer.
	// It continues to consume ordinary nonterminal turns with the old rules.
	const Error service = production_observe_service(ref, *budget);
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	const Error resolved = production_request_locked(ref, false, request, rd);
	if (resolved != OK) {
		return result(resolved);
	}
	GDExtensionProductionObserveResultV3 projected{};
	projected.abi = production_dto_abi<GDExtensionProductionObserveResultV3>();
	production_report_locked(*request, projected.report);
	projected.output_generation.abi = production_dto_abi<GDExtensionProductionOutputGenerationResultV1>();
	const auto &cell = request->control;
	if (cell.state == ProductionControlState::RETURNED && cell.input.kind == ProductionControlKind::OBSERVE && cell.output.output_generation.native_released) {
		projected.output_generation = project_output_generation(cell.output.output_generation);
		*out = projected;
		return cell.output.result;
	}
	*out = projected;
	return result(service);
}

GDExtensionDeviceSessionResultV1 Registry::production_output_return_consume(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOutputGenerationRefV1 *expected, GDExtensionProductionOutputGenerationResultV1 *out) {
	const Error contract = production_contract_check(ref, 2);
	if (contract != OK) {
		return result(contract);
	}
	if (!aligned(expected) || !production_dto_valid(*expected) || !aligned(out) || !production_observe_disjoint(expected, out)) {
		return result(ERR_INVALID_PARAMETER);
	}
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	const Error resolved = production_request_locked(ref, false, request, rd);
	if (resolved != OK) {
		return result(resolved);
	}
	auto &cell = request->control;
	if (!expected->control_serial || cell.serial != expected->control_serial || cell.state == ProductionControlState::OWNER_CONSUMED) {
		return result(ERR_DOES_NOT_EXIST);
	}
	if (cell.state != ProductionControlState::RETURNED) {
		return result(ERR_BUSY);
	}
	const auto &terminal = cell.output.output_generation;
	if (cell.input.kind != ProductionControlKind::OBSERVE || cell.input.request_serial != request->serial ||
			cell.output.request_serial != request->serial || cell.output.control_serial != cell.serial || !terminal.native_released ||
			terminal.registry_instance != instance || terminal.request_serial != request->serial || terminal.control_serial != cell.serial ||
			!terminal.root_serial || !terminal.particles || !terminal.generation ||
			expected->registry_instance != terminal.registry_instance || expected->request_serial != terminal.request_serial ||
			expected->control_serial != terminal.control_serial || expected->root_serial != terminal.root_serial ||
			expected->particles != terminal.particles || expected->generation != terminal.generation) {
		return result(ERR_INVALID_DATA);
	}
	// Bind the real consumed terminal to its original paid placement row before
	// a subsequent control replaces this OBSERVE cell. No numeric caller grant.
	for (uint32_t i = 0; i < request->resource_count; ++i) {
		auto &resource = request->resources[i];
		if (resource.metadata_generation_bound && resource.metadata_root_serial == terminal.root_serial) {
			resource.metadata_consumed_control_serial = terminal.control_serial;
			resource.metadata_consumed_particles = terminal.particles;
			resource.metadata_consumed_generation = terminal.generation;
		}
	}
	const auto projected = project_output_generation(terminal);
	// Same original RETURNED -> OWNER_CONSUMED transition as internal consume,
	// under the original lock. No callback, native append or E05 ACK is issued.
	cell.state = ProductionControlState::OWNER_CONSUMED;
	*out = projected;
	return result(OK);
}

GDExtensionDeviceSessionResultV1 Registry::production_observe_v2(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionServiceBudgetV1 *budget, GDExtensionProductionReportV2 *out, uint32_t expected_contract) {
	const Error contract = production_contract_check(ref, expected_contract);
	if (contract != OK) {
		return { int32_t(contract), 0, 0, 0 };
	}
	if (!aligned(budget) || !production_dto_valid(*budget) || budget->reserved || !aligned(out)) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	const Error service = production_observe_service(ref, *budget);
	Lock lock(*this);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_request_locked(ref, false, r, rd);
	if (e != OK) {
		return result(e, 16);
	}
	production_report_locked(*r, *out);
	return result(service);
}
void Registry::production_report_locked(const ProductionRequest &r, GDExtensionProductionReportV2 &report) const {
	if (r.capture_inflight || r.continuation_inflight) {
		report = {};
		report.abi = production_dto_abi<GDExtensionProductionReportV2>();
		report.operation = r.operation;
		report.revision = r.revision;
		report.cause = result(ERR_BUSY, 16);
		return;
	}
	production_report_contents_locked(r, report);
}

Error Registry::production_report_contents_locked(const ProductionRequest &r, GDExtensionProductionReportV2 &report) const {
	report = {};
	report.abi = production_dto_abi<GDExtensionProductionReportV2>();
	report.operation = r.operation;
	report.revision = r.revision;
	// Private contents projection returns structural validity separately from
	// native diagnostics. Its caller retains the original Registry lock and,
	// when capture is nonzero, authenticates the one executing OBSERVE frame.
	if (r.resource_count > r.capacity.resources || (r.resource_count && !r.resources) || r.readback_count > r.capacity.readbacks || (r.readback_count && !r.readbacks) || (r.capacity.return_cells && !r.returns)) {
		report.cause = result(ERR_INVALID_DATA, 16);
		return ERR_INVALID_DATA;
	}
	// Recording/segment/ACK frontiers remain owned by the original record window.
	report.recorded_action_end = r.next_action;
	report.action_count = r.action_count;
	report.captured_segment = r.segment.segment;
	report.acknowledged_segment = r.acknowledged_segment;
	report.cancelled = r.cancelled;
	report.possible_obligations = uint32_t(r.effects_pending) + uint32_t(r.source_device_lost);
	if (r.graph_count > r.capacity.graph_memberships || r.native_count > r.capacity.native_memberships || uint64_t(r.graph_count) + r.native_count > UINT32_MAX) {
		report.cause = result(ERR_INVALID_DATA, 16);
		return ERR_INVALID_DATA;
	}
	report.fact_count = r.graph_count + r.native_count;
	for (uint32_t i = 0; i < r.resource_count; ++i) {
		report.possible_obligations += r.resources[i].transfer_pending;
		report.physical_returns_pending += r.resources[i].metadata_data != nullptr;
		report.physical_returns_pending += !r.resources[i].output_predecessor_view.is_empty();
	}
	Error cause = OK;
	for (uint32_t i = 0; i < r.graph_count; ++i) {
		if (!r.graphs || r.graphs[i].request != &r) {
			report.cause = result(ERR_INVALID_DATA, 16);
			return ERR_INVALID_DATA;
		}
		report.possible_obligations += r.graphs[i].pending;
	}
	for (uint32_t i = 0; i < r.native_count; ++i) {
		const ProductionNativeCell *owner = nullptr;
		const Error error = production_native_owner_locked(r, i, owner);
		if (error != OK) {
			report.cause = result(error, 16);
			return error;
		}
		if (r.natives[i].owner_index != i) {
			continue;
		}
		const bool complete = owner->observation.state == NativeSourceObservation::State::Complete;
		report.known_obligations += owner->member.source.issue == RDD::IssueFact::KnownAccepted || complete;
		report.possible_obligations += owner->member.source.issue == RDD::IssueFact::IssuedUnknown && !complete;
		report.complete_obligations += complete;
		if (cause == OK) {
			cause = owner->member.source.error != OK ? owner->member.source.error : owner->observation.completion_error != OK ? owner->observation.completion_error
					: owner->member.retention_error != OK																	  ? owner->member.retention_error
																															  : owner->release_error;
		}
	}
	for (uint32_t i = 0; i < r.capacity.return_cells; ++i) {
		report.physical_returns_pending += r.returns[i].pending;
	}
	for (uint32_t i = 0; i < r.readback_count; ++i) {
		const auto &readback = r.readbacks[i];
		if (readback.source.issued) {
			report.possible_obligations += readback.source.returned != readback.source.issued;
			if (cause == OK && readback.source.error != OK) {
				cause = readback.source.error;
			}
			if (readback.declaration.resource_slot < r.resource_count) {
				const auto &resource = r.resources[readback.declaration.resource_slot];
				report.ready_readbacks += resource.imported && same_request_readback_complete(readback.source, r.serial, resource.backing_serial, readback.declaration.byte_offset, readback.declaration.byte_size);
			}
		}
	}
	if (cause == OK && r.source_device_lost) {
		cause = ERR_UNAVAILABLE;
	}
	if (cause == OK && (report.possible_obligations || report.physical_returns_pending || report.known_obligations != report.complete_obligations)) {
		cause = ERR_BUSY;
	}
	report.cause = result(cause);
	return OK;
}
Error Registry::production_native_owner_locked(const ProductionRequest &r, uint32_t index, const ProductionNativeCell *&out) const {
	out = nullptr;
	if (r.native_count > r.capacity.native_memberships || r.graph_count > r.capacity.graph_memberships || index >= r.native_count || !r.natives) {
		return ERR_INVALID_DATA;
	}
	const auto &cell = r.natives[index];
	if (cell.request != &r || cell.owner_index > index || cell.owner_index >= r.native_count) {
		return ERR_INVALID_DATA;
	}
	const auto &owner = r.natives[cell.owner_index];
	if (owner.request != &r || owner.owner_index != cell.owner_index || !same_native_source(cell.member.source, owner.member.source) ||
			(cell.owner_index != index && (cell.member.retained || cell.member.owner_released))) {
		return ERR_INVALID_DATA;
	}
	if (cell.fact.membership_parent != UINT32_MAX && (cell.fact.membership_parent >= r.graph_count || !r.graphs || r.graphs[cell.fact.membership_parent].request != &r)) {
		return ERR_INVALID_DATA;
	}
	out = &owner;
	return OK;
}

Error Registry::production_native_fact_locked(const ProductionRequest &r, uint32_t index, GDExtensionProductionFactV2 &out) const {
	const ProductionNativeCell *owner = nullptr;
	Error error = production_native_owner_locked(r, index, owner);
	if (error != OK) {
		return error;
	}
	const auto &member = r.natives[index].member;
	out = r.natives[index].fact;
	out.native_serial = member.source.serial;
	out.completion_serial = member.source.completion_serial;
	out.backend_queue = member.source.queue_index;
	out.issue = uint32_t(member.source.issue); // Immutable original issue classification.
	out.issue_error = member.source.error;
	out.issue_native_code = member.source.native_code;
	out.retention_error = owner->member.retention_error;
	out.completion_native_code = owner->observation.completion_native_code;
	// Completion failure and issue failure have separate meanings. In particular
	// a quiet poll never turns the original issue error into a completion error.
	out.completion = member.source.issue == RDD::IssueFact::NotIssued ? 0 : owner->observation.state == NativeSourceObservation::State::Complete ? 2
			: owner->observation.completion_error != OK																							 ? 3
			: owner->observation.state == NativeSourceObservation::State::Unknown || owner->member.retention_error != OK						 ? 4
																																				 : 1;
	return OK;
}

Error Registry::production_poll_native_locked(ProductionRequest &r, RDD &driver, const GDExtensionProductionServiceBudgetV1 &budget, bool live, ProductionServiceUsage &used) {
	used = {};
	// These are caller reductions on already retained Core maintenance, not B's
	// missing installed owner-turn grant. Zero query does not even retain a ref.
	if (!service_budget_active(budget) || !r.native_count) {
		return OK;
	}
	if (r.continuation_inflight) {
		return ERR_BUSY;
	}
	if (r.native_count > r.capacity.native_memberships || !r.natives) {
		return ERR_INVALID_DATA;
	}
	if (r.revision > UINT64_MAX - uint64_t(r.capacity.graph_memberships) * 2 - uint64_t(r.capacity.native_memberships) * 2 - r.capacity.readbacks - r.capacity.resources - 8) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	used.controls = 1;
	if (!live) {
		if (!r.source_device_lost) {
			r.source_device_lost = true;
			++r.revision;
		}
		return ERR_UNAVAILABLE;
	}
	if (r.source_device_lost) {
		return ERR_UNAVAILABLE;
	}
	const auto origin = NativeSourceOrigin::production();
	// Fixed Core ceiling; caller values only reduce this maintenance turn.
	const uint32_t visits = MIN(r.native_count, 64u);
	uint32_t scanned = 0;
	const uint32_t receipts = MIN(budget.receipts, 128u);
	while (scanned < visits && used.passes < MIN(budget.passes, 64u) && used.receipts < receipts && before_service_deadline(budget)) {
		if (r.revision > UINT64_MAX - uint64_t(r.capacity.graph_memberships) * 2 - uint64_t(r.capacity.native_memberships) * 2 - r.capacity.readbacks - r.capacity.resources - 8) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
		const uint32_t index = r.observe_cursor % r.native_count;
		const ProductionNativeCell *owner = nullptr;
		Error error = production_native_owner_locked(r, index, owner);
		if (error != OK) {
			return error;
		}
		++scanned;
		r.observe_cursor = (index + 1) % r.native_count;
		auto &cell = r.natives[index];
		if (cell.owner_index != index || cell.member.owner_released || cell.member.source.issue == RDD::IssueFact::NotIssued || cell.observation.state == NativeSourceObservation::State::Complete) {
			continue;
		}
		// Terminal and alias rows consume only the fixed metadata scan bound.
		// Preserve this turn's service pass for actual observation or disposal.
		++used.passes;
		const auto old = cell.observation;
		const bool retained_before = cell.member.retained;
		const Error retention_before = cell.member.retention_error;
		const uint64_t request_serial = r.serial, control_serial = r.control.serial;
		auto *attachment = r.attachment;
		auto local_member = cell.member;
		auto local_observation = cell.observation;
		// EXECUTING prevents close/release/reuse of this fixed control and its
		// request. The existing entity edge and held RD lock retain the device.
		// Driver poll can synchronously notify this Registry of device loss:
		// never call it while holding the non-recursive Registry mutex.
		struct UnlockRegistry {
			Registry &registry;
			explicit UnlockRegistry(Registry &p_registry) : registry(p_registry) {
				registry.unlock_mutex_for_call();
			}
			~UnlockRegistry() { registry.relock_mutex_after_call(); }
		};
		auto validate_return = [&]() {
			CRASH_COND(r.serial != request_serial || r.attachment != attachment ||
					r.control.serial != control_serial ||
					r.control.state != ProductionControlState::EXECUTING ||
					cell.request != &r || cell.owner_index != index ||
					!same_native_source(cell.member.source, local_member.source));
		};
		if (!local_member.retained && before_service_deadline(budget)) {
			const bool driver_called = retainable_native_source(local_member.source);
			Error retained;
			{
				UnlockRegistry unlock(*this);
				retained = retain_native_source(driver, local_member);
			}
			validate_return();
			used.receipts += driver_called ? 1 : 0;
			if (retained != OK) {
				local_observation.retention_error = retained;
				local_observation.error =
						retained == ERR_BUSY || local_member.source.error == OK
						? retained
						: local_member.source.error;
				local_observation.native_code = local_member.source.error == OK
						? 0
						: local_member.source.native_code;
				local_observation.state = retained == ERR_BUSY
						? NativeSourceObservation::State::Pending
						: NativeSourceObservation::State::Unknown;
			}
		}
		if (device_records[attachment->device].live && local_member.retained &&
				used.receipts < receipts && before_service_deadline(budget)) {
			++used.receipts;
			{
				UnlockRegistry unlock(*this);
				// Already retained: exactly one original poll, no hidden retain.
				local_observation = poll_native_source(driver, local_member, origin);
			}
			validate_return();
		}
		const bool lost = !device_records[attachment->device].live;
		if (lost) {
			r.source_device_lost = true;
			local_observation.state = NativeSourceObservation::State::Unknown;
			if (local_observation.error == OK) {
				local_observation.error = ERR_UNAVAILABLE;
			}
		}
		// Account any actual retained reference even if its own callback marked
		// the device lost. No identity check or loss may silently drop that ref.
		cell.member = local_member;
		cell.observation = local_observation;
		if (lost || retained_before != cell.member.retained ||
				retention_before != cell.member.retention_error ||
				!same_native_observation(old, cell.observation)) {
			++r.revision;
		}
		if (lost) {
			return ERR_UNAVAILABLE;
		}
	}
	return OK;
}

GDExtensionDeviceSessionResultV1 Registry::production_facts(GDExtensionProductionRequestRefV1 ref, uint64_t revision, uint32_t first, GDExtensionProductionFactV1 *out, uint32_t capacity, uint32_t *written) {
	if (!aligned(written) || (capacity && (!aligned(out) || uint64_t(capacity) * sizeof(*out) > SIZE_MAX || !separate(out, size_t(capacity) * sizeof(*out), written, sizeof(*written))))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	const Error contract = production_contract_check(ref, 1);
	if (contract != OK) {
		return result(contract, 16);
	}
	Lock lock(*this);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_request_locked(ref, false, r, rd);
	if (error != OK) {
		return result(error, 16);
	}
	if (r->capture_inflight || r->continuation_inflight) {
		return result(ERR_BUSY, 16);
	}
	if (revision != r->revision) {
		return result(ERR_DOES_NOT_EXIST, 16);
	}
	if (r->graph_count > r->capacity.graph_memberships || r->native_count > r->capacity.native_memberships || uint64_t(r->graph_count) + r->native_count > UINT32_MAX) {
		return result(ERR_INVALID_DATA, 16);
	}
	const uint32_t total = r->graph_count + r->native_count;
	if (first > total) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	const uint32_t count = MIN(capacity, total - first);
	// Prevalidate the entire requested page, so errors do not partially publish.
	for (uint32_t i = first; i < first + count; ++i) {
		if (i < r->graph_count) {
			if (!r->graphs || r->graphs[i].request != r) {
				return result(ERR_INVALID_DATA, 16);
			}
		} else {
			const ProductionNativeCell *owner = nullptr;
			error = production_native_owner_locked(*r, i - r->graph_count, owner);
			if (error != OK) {
				return result(error, 16);
			}
		}
	}
	for (uint32_t i = 0; i < count; ++i) {
		const uint32_t index = first + i;
		if (index < r->graph_count) {
			out[i] = production_legacy_projection(r->graphs[index].fact, r->operation.canonical_begin);
		} else {
			GDExtensionProductionFactV2 value = {};
			production_native_fact_locked(*r, index - r->graph_count, value);
			out[i] = production_legacy_projection(value, r->operation.canonical_begin);
		}
	}
	*written = count;
	return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::production_facts_v2(GDExtensionProductionRequestRefV1 ref, uint64_t revision, uint32_t first, GDExtensionProductionFactV2 *out, uint32_t capacity, uint32_t *written, uint32_t expected_contract) {
	const Error contract = production_contract_check(ref, expected_contract);
	if (contract != OK) {
		return { int32_t(contract), 0, 0, 0 };
	}
	if (!aligned(written) || (capacity && (!aligned(out) || uint64_t(capacity) * sizeof(*out) > SIZE_MAX || !separate(out, size_t(capacity) * sizeof(*out), written, sizeof(*written))))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	Lock lock(*this);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_request_locked(ref, false, r, rd);
	if (error != OK) {
		return result(error, 16);
	}
	if (r->capture_inflight || r->continuation_inflight) {
		return result(ERR_BUSY, 16);
	}
	if (revision != r->revision) {
		return result(ERR_DOES_NOT_EXIST, 16);
	}
	if (r->graph_count > r->capacity.graph_memberships || r->native_count > r->capacity.native_memberships || uint64_t(r->graph_count) + r->native_count > UINT32_MAX) {
		return result(ERR_INVALID_DATA, 16);
	}
	const uint32_t total = r->graph_count + r->native_count;
	if (first > total) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	const uint32_t count = MIN(capacity, total - first);
	// Prevalidate the entire requested page, so errors do not partially publish.
	for (uint32_t i = first; i < first + count; ++i) {
		if (i < r->graph_count) {
			if (!r->graphs || r->graphs[i].request != r) {
				return result(ERR_INVALID_DATA, 16);
			}
		} else {
			const ProductionNativeCell *owner = nullptr;
			error = production_native_owner_locked(*r, i - r->graph_count, owner);
			if (error != OK) {
				return result(error, 16);
			}
		}
	}
	for (uint32_t i = 0; i < count; ++i) {
		const uint32_t index = first + i;
		if (index < r->graph_count) {
			out[i] = r->graphs[index].fact;
		} else {
			production_native_fact_locked(*r, index - r->graph_count, out[i]);
		}
	}
	*written = count;
	return result(OK);
}

GDExtensionDeviceSessionResultV1 Registry::production_copy(GDExtensionProductionRequestRefV1 ref, uint64_t revision, uint32_t slot, void *destination, uint64_t capacity, uint64_t *written) {
	if (!aligned(written) || capacity > SIZE_MAX || (capacity && (!destination || !separate(destination, size_t(capacity), written, sizeof(*written))))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_request_locked(ref, false, request, rd);
	if (error != OK) {
		return result(error, 16);
	}
	if (request->capture_inflight || request->continuation_inflight) {
		return result(ERR_BUSY, 16);
	}
	if (revision != request->revision) {
		return result(ERR_DOES_NOT_EXIST, 16);
	}
	if (slot >= request->readback_count) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	const auto &cell = request->readbacks[slot];
	const auto &declaration = cell.declaration;
	if (cell.request != request || declaration.resource_slot >= request->resource_count) {
		return result(ERR_INVALID_DATA, 16);
	}
	const auto &resource = request->resources[declaration.resource_slot];
	if (!resource.imported || !same_request_readback_complete(cell.source, request->serial, resource.backing_serial, declaration.byte_offset, declaration.byte_size)) {
		return result(cell.source.error != OK ? cell.source.error : ERR_BUSY, 16);
	}
	bool covered = false;
	for (uint32_t g = 0; g < request->graph_count; ++g) {
		const auto &graph = request->graphs[g];
		if (graph.fact.action_ordinal != declaration.copy_action_ordinal) {
			continue;
		}
		if (graph.pending || graph.fact.issue_error != OK) {
			return result(ERR_BUSY, 16);
		}
		bool issued = false;
		for (uint32_t n = 0; n < request->native_count; ++n) {
			if (request->natives[n].fact.membership_parent != g) {
				continue;
			}
			const ProductionNativeCell *owner = nullptr;
			if (production_native_owner_locked(*request, n, owner) != OK || owner->observation.state != NativeSourceObservation::State::Complete) {
				return result(ERR_BUSY, 16);
			}
			issued = true;
		}
		if (!issued) {
			return result(ERR_BUSY, 16);
		}
		covered = true;
	}
	if (!covered) {
		return result(ERR_BUSY, 16);
	}
	if (capacity < declaration.byte_size) {
		return result(ERR_PARAMETER_RANGE_ERROR, 19);
	}
	// Byte coverage and source association must both be exact. No native call,
	// progress, or implicit readback is permitted in this observation method.
	if (!range(cell.bytes_offset, declaration.byte_size, request->attachment->quote.capacity.readback_bytes) || cell.bytes_offset < request->readback_offset || !range(cell.bytes_offset - request->readback_offset, declaration.byte_size, request->capacity.readback_bytes) || !destination) {
		return result(ERR_INVALID_DATA, 16);
	}
	if (!separate(destination, size_t(declaration.byte_size), request->attachment->arena, size_t(request->attachment->quote.allocation_bytes)) || !separate(written, sizeof(*written), request->attachment->arena, size_t(request->attachment->quote.allocation_bytes))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	std::memcpy(destination, request->attachment->readback_bytes + cell.bytes_offset, size_t(declaration.byte_size));
	*written = declaration.byte_size;
	return result(OK);
}

GDExtensionDeviceSessionResultV1 Registry::production_cancel(GDExtensionProductionRequestRefV1 ref) {
	Lock lock(*this);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_request_locked(ref, false, r, rd);
	if (e != OK) {
		return result(e, 16);
	}
	production_native_record_reject_wait_locked(*r);
	if (r->control.state != ProductionControlState::OWNER_CONSUMED) {
		return result(ERR_BUSY, 16);
	}
	return production_cancel_locked(*r);
}
GDExtensionDeviceSessionResultV1 Registry::production_cancel_locked(ProductionRequest &request) {
	auto *r = &request;
	if (!r->cancelled) {
		if (r->revision == UINT64_MAX) {
			return result(ERR_PARAMETER_RANGE_ERROR, 19);
		}
		r->cancelled = true;
		++r->revision;
	}
	return result(r->effects_pending ? ERR_BUSY : OK);
}
GDExtensionDeviceSessionResultV1 Registry::production_release_v2(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionServiceBudgetV1 *input, GDExtensionProductionServiceBudgetV1 *remaining, GDExtensionProductionReportV2 *report, uint32_t expected_contract) {
	const Error contract = production_contract_check(ref, expected_contract);
	if (contract != OK) {
		return { int32_t(contract), 0, 0, 0 };
	}
	if (!aligned(input) || !production_dto_valid(*input) || input->reserved || !aligned(remaining) || !aligned(report) || !separate(input, sizeof(*input), remaining, sizeof(*remaining)) || !separate(input, sizeof(*input), report, sizeof(*report)) || !separate(remaining, sizeof(*remaining), report, sizeof(*report))) {
		return result(ERR_INVALID_PARAMETER, 19);
	}
	const auto offered = *input;
	*remaining = offered;
	*report = {};
	GDExtensionProductionServiceBudgetV1 held = {};
	uint64_t serial = 0;
	bool accept = false, publish = false, consume = false;
	{
		Lock lock(*this);
		ProductionRequest *request = nullptr;
		RenderingDevice *rd = nullptr;
		const Error error = production_request_locked(ref, false, request, rd);
		if (error != OK) {
			return result(error, 16);
		}
		production_native_record_reject_wait_locked(*request);
		production_report_locked(*request, *report);
		// Release is cleanup of an exhausted or explicitly cancelled plan. No
		// release preflight may unpin a still-declared, unrecorded suffix.
		if (!request->cancelled && request->next_action != request->action_count) {
			if (report->cause.native_error == OK) {
				report->cause = result(ERR_BUSY, 16);
			}
			return result(ERR_BUSY, 16);
		}
		for (uint32_t i = 0; i < request->capacity.resource_uses; ++i) {
			if (request->uses[i].output_consumer) {
				if (report->cause.native_error == OK) {
					report->cause = result(ERR_BUSY, 16);
				}
				return result(ERR_BUSY, 16); // Do not publish cleanup while native last-use owns the arena.
			}
		}
		auto &cell = request->control;
		if (cell.state != ProductionControlState::OWNER_CONSUMED) {
			if (cell.input.kind != ProductionControlKind::RELEASE) {
				return result(ERR_BUSY, 16); // Never overtake another immutable turn.
			}
			held = cell.input.budget;
			serial = cell.serial;
			if (cell.state == ProductionControlState::PENDING && !service_budget_active(held)) {
				// This exact cell never published. Expiry returns only a proven
				// no-effect metadata result; it does not renew the old authorization.
				cell.output = {};
				cell.output.request_serial = request->serial;
				cell.output.control_serial = cell.serial;
				cell.output.result = result(ERR_BUSY, 16);
				cell.output.report = *report;
				cell.state = ProductionControlState::RETURNED;
			}
			consume = cell.state == ProductionControlState::RETURNED;
			publish = cell.state == ProductionControlState::PENDING && service_budget_active(offered);
		} else {
			if (!service_budget_active(offered)) {
				return result(ERR_BUSY, 16); // Fresh zero/expired input is only metadata.
			}
			const auto ready = production_release_locked(*request, rd, false);
			if (ready.native_error != OK) {
				// A completed record can still own the original frame cursor.
				// The accepted RELEASE cell services only that exact owned tail;
				// the original reclaim preflight remains unchanged and strict.
				bool own_tail = false;
				if (rd && rd->_thread_safe_.try_lock()) {
					own_tail = request->maintenance_serial && rd->native_disposal_active && request->maintenance_serial == rd->native_disposal_continuation.serial && rd->native_disposal_continuation.production_owner == request;
					rd->_thread_safe_.unlock();
				}
				// A returned CPU map failure is a result error, not an outstanding
				// native obligation. Admit only the original maintenance tail after
				// all issued work is positively complete; preserve the error report.
				const bool returned_map_failure = [&]() {
					if (report->cause.native_error != ERR_CANT_ACQUIRE_RESOURCE || request->effects_pending || request->source_device_lost || report->possible_obligations || report->physical_returns_pending || report->known_obligations != report->complete_obligations) {
						return false;
					}
					for (uint32_t i = 0; i < request->graph_count; ++i) {
						const auto &graph = request->graphs[i];
						if (graph.request != request || graph.pending || graph.fact.issue_error != OK) {
							return false;
						}
					}
					for (uint32_t i = 0; i < request->native_count; ++i) {
						const ProductionNativeCell *owner = nullptr;
						if (production_native_owner_locked(*request, i, owner) != OK || owner->member.source.issue != RDD::IssueFact::KnownAccepted || owner->member.source.error != OK || owner->observation.state != NativeSourceObservation::State::Complete || owner->observation.error != OK || owner->observation.completion_error != OK || owner->observation.retention_error != OK || owner->member.retention_error != OK || owner->release_error != OK || !owner->member.retained) {
							return false;
						}
					}
					bool found_map_failure = false;
					for (uint32_t i = 0; i < request->readback_count; ++i) {
						const auto &readback = request->readbacks[i];
						if (readback.request != request || readback.source.returned != readback.source.issued || readback.source.completed > readback.source.issued) {
							return false;
						}
						if (readback.source.error == OK) {
							continue;
						}
						if (readback.source.error != ERR_CANT_ACQUIRE_RESOURCE || !readback.source.issued || readback.source.delivered) {
							return false;
						}
						found_map_failure = true;
					}
					return found_map_failure;
				}();
				if (ready.native_error != ERR_BUSY || !own_tail || request->capture_inflight || request->continuation_inflight || (request->compute_list_open || request->draw_list_open) || (report->cause.native_error != OK && !returned_map_failure) || (!request->cancelled && request->next_action != request->action_count)) {
					return ready;
				}
			}
			accept = publish = true;
		}
		if (!consume && !accept) {
			*remaining = held;
			remaining->controls = remaining->receipts = remaining->passes = 0;
			report->service_control_serial = serial;
			report->service_pending = 1; // Entire immutable slice is still escrowed.
		}
	}
	if (consume) {
		ProductionControlOutput returned = {};
		const Error error = production_control_consume(ref, serial, &returned);
		if (error != OK) {
			*remaining = held;
			remaining->controls = remaining->receipts = remaining->passes = 0;
			report->service_control_serial = serial;
			report->service_pending = 1;
			return result(error, 16);
		}
		*report = returned.report;
		report->service_control_serial = serial;
		report->service_pending = 0;
		*remaining = held; // Only the old ABI/deadline/authorization is returned.
		const auto &rt = returned.service_usage;
		const auto &owner = returned.owner_usage;
		const uint64_t passes = uint64_t(rt.passes) + owner.passes;
		const uint64_t receipts = uint64_t(rt.receipts) + owner.receipts;
		// The dispatched physical slice (RT or explicit local owner phase) and
		// its owner consumption share one control, never a second debit.
		if (returned.request_serial != ref.handle_serial || returned.control_serial != serial || rt.controls > 1 || owner.controls > 1 || (owner.controls && rt.controls != 1) || rt.controls > held.controls || passes > MIN(held.passes, 64u) || receipts > MIN(held.receipts, 128u)) {
			remaining->controls = remaining->receipts = remaining->passes = 0;
			report->cause = result(ERR_INVALID_DATA, 19);
			return report->cause;
		}
		remaining->controls -= rt.controls;
		remaining->passes -= uint32_t(passes);
		remaining->receipts -= uint32_t(receipts);
		const auto completed = returned.result.native_error == OK ? returned.owner_return : returned.result;
		if (completed.native_error != OK) {
			report->cause = completed;
		}
		return completed; // Never accept offered as a new turn in this same call.
	}
	if (accept) {
		const Error error = production_control_accept(ref, ProductionControlKind::RELEASE, &serial, &offered);
		if (error != OK) {
			return result(error, 16);
		}
		held = offered;
	}
	*remaining = held;
	remaining->controls = remaining->receipts = remaining->passes = 0;
	report->service_control_serial = serial;
	report->service_pending = 1;
	if (publish) {
		const Error scheduled = production_control_schedule(ref, serial);
		if (scheduled != OK && scheduled != ERR_BUSY && scheduled != ERR_UNAVAILABLE) {
			return result(scheduled, 16);
		}
	}
	return result(ERR_BUSY, 16); // Actual FIFO return is consumed on a later call.
}

ProductionOutputPin::ProductionOutputPin(ProductionOutputPin &&other) noexcept {
	if (other.registry) {
		other.registry->production_output_pin_move(*this, other);
	}
}
ProductionOutputPin &ProductionOutputPin::operator=(ProductionOutputPin &&other) noexcept {
	if (this != &other) {
		CRASH_COND(registry); // An existing last-use obligation cannot be overwritten.
		if (other.registry) {
			other.registry->production_output_pin_move(*this, other);
		}
	}
	return *this;
}
ProductionOutputPin::~ProductionOutputPin() {
	CRASH_COND(registry);
}
void Registry::production_output_pin_move(ProductionOutputPin &to, ProductionOutputPin &from) {
	Lock lock(*this);
	CRASH_COND(to.registry || from.registry != this || !from.request || from.request->serial != from.request_serial || from.cell_index >= from.request->capacity.resource_uses);
	auto &cell = from.request->uses[from.cell_index];
	CRASH_COND(cell.request != from.request || cell.output_role != ProductionUseCell::OutputRole::NATIVE_CONSUMER || cell.output_consumer != &from || cell.output_pin_serial != from.serial);
	to.registry = this;
	to.request = from.request;
	to.request_serial = from.request_serial;
	to.serial = from.serial;
	to.cell_index = from.cell_index;
	cell.output_consumer = &to;
	from.registry = nullptr;
	from.request = nullptr;
	from.request_serial = from.serial = 0;
	from.cell_index = UINT32_MAX;
}
bool Registry::production_output_pin_matches(const ProductionOutputPin &pin, GDExtensionProductionRequestRefV1 ref, RenderingDevice &device) {
	// Native scene commits validate the same root while holding this original
	// Registry lock. Borrow that read scope rather than recursively locking it.
	Lock lock(*this, Lock::BLOCK_OR_BORROW);
	return production_output_pin_matches_locked(pin, ref, device);
}
bool Registry::production_output_pin_matches_operation(const ProductionOutputPin &pin, GDExtensionProductionRequestRefV1 ref, RenderingDevice &device, const OriginalOperationIdentity &operation) {
	Lock lock(*this);
	if (!production_output_pin_matches_locked(pin, ref, device)) {
		return false;
	}
	const auto &request = *pin.request;
	const auto &attachment = *request.attachment;
	const OriginalOperationIdentity original{ request.operation.owner_request_nonce, attachment.library_generation,
		device_records[attachment.device].session, request.operation.record_generation, request.operation.canonical_begin, request.operation.canonical_end };
	// All fields come from this actual retained request/attachment/device. The
	// supplied projection cannot issue or replace that original identity.
	return same_original_operation_identity(original, operation);
}
// Read-only retained identity check under Registry and RD locks. It does not
// authorize a physical operation on the owner's thread.
bool Registry::production_output_pin_identity_matches_locked(const ProductionOutputPin &pin, GDExtensionProductionRequestRefV1 ref, RenderingDevice &device) {
	if (pin.registry != this || ref.registry_instance != instance || !pin.request || !pin.serial || pin.request_serial != ref.handle_serial || pin.request->serial != pin.request_serial || pin.cell_index >= pin.request->capacity.resource_uses) {
		return false;
	}
	const auto &request = *pin.request;
	const auto &attachment = *request.attachment;
	const auto &native = device_records[attachment.device];
	const auto &cell = request.uses[pin.cell_index];
	return native.object == &device && native.epoch == attachment.epoch && attachment.held_device == &device && cell.request == &request && cell.output_role == ProductionUseCell::OutputRole::NATIVE_CONSUMER && cell.output_consumer == &pin && cell.output_pin_serial == pin.serial && pin.cell_index >= request.use_count + request.binding_used;
}
bool Registry::production_output_pin_matches_locked(const ProductionOutputPin &pin, GDExtensionProductionRequestRefV1 ref, RenderingDevice &device) {
	return production_output_pin_identity_matches_locked(pin, ref, device) && device_records[pin.request->attachment->device].execution_thread == Thread::get_caller_id();
}
Error Registry::production_output_pin_new_work_matches_locked(const ProductionOutputPin &source, RenderingDevice &device, StringName *detail) {
	if (detail) *detail = StringName();
	auto fail = [&](Error error, const char *cause) {
		if (detail) *detail = StringName(cause);
		return error;
	};
	if (source.registry != this || !source.request || !production_output_pin_matches_locked(source, { instance, source.request_serial }, device)) {
		return fail(ERR_UNAUTHORIZED, "WrongOwner");
	}
	auto &request = *source.request;
	auto &attachment = *request.attachment;
	const auto &native = device_records[attachment.device];
	const auto &record = records[attachment.library];
	if (!record.occupied || record.snapshot.library_generation != attachment.library_generation || record.producer_registration_serial != attachment.producer_serial || !attachment.holder || attachment.holder->pending || attachment.holder->generation != attachment.library_generation || attachment.holder->handle_serial != record.producer_code_holder || !record.producer_attachment_identity) {
		return fail(ERR_INVALID_DATA, "WrongOwner");
	}
	if (!native.live || !device.is_main_instance || RenderingDevice::get_singleton() != &device) {
		return fail(ERR_UNAVAILABLE, "WrongDevice");
	}
	if (phase != Phase::RUNNING || device.native_finalization_returned || !request.frozen || request.cancelled || request.release_started || request.source_device_lost || attachment.closing || !attachment.opened || !record.attached || record.quiescing || record.close_started || record.close_error != OK || record.guard_enrollment != GuardEnrollment::QUALIFIED || device.production_finalization_started.load(std::memory_order_acquire)) {
		return fail(ERR_BUSY, "Revoked");
	}
	return OK;
}
Error Registry::production_output_pin_borrow(const ProductionOutputPin &source, RenderingDevice &device, ProductionOutputPin &pin) {
	Lock lock(*this);
	return production_output_pin_borrow_locked(source, device, pin);
}
Error Registry::production_output_pin_borrow_locked(const ProductionOutputPin &source, RenderingDevice &device, ProductionOutputPin &pin) {
	if (&source == &pin || !pin.is_empty()) {
		return ERR_ALREADY_IN_USE;
	}
	const Error admitted = production_output_pin_new_work_matches_locked(source, device);
	if (admitted != OK) {
		return admitted;
	}
	auto &request = *source.request;
	if (!request.next_output_pin_serial) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	if (request.use_count > request.capacity.resource_uses || request.binding_used > request.capacity.resource_uses - request.use_count) {
		return ERR_INVALID_DATA;
	}
	const uint32_t begin = request.use_count + request.binding_used;
	for (uint32_t i = begin; i < request.capacity.resource_uses; ++i) {
		auto &cell = request.uses[i];
		if (cell.request != &request || cell.dependency.physical || cell.output_role != ProductionUseCell::OutputRole::UNUSED || cell.output_consumer || cell.output_pin_serial || cell.disposal_group_serial) {
			continue;
		}
		// Retain the same accepted arena and its existing code/device lifetime.
		// No native/source record or accepted capacity is created by this edge.
		const uint64_t serial = request.next_output_pin_serial;
		request.next_output_pin_serial = serial == UINT64_MAX ? 0 : serial + 1;
		pin.registry = this;
		pin.request = &request;
		pin.request_serial = request.serial;
		pin.serial = serial;
		pin.cell_index = i;
		cell.output_role = ProductionUseCell::OutputRole::NATIVE_CONSUMER;
		cell.output_consumer = &pin;
		cell.output_pin_serial = serial;
		return OK;
	}
	return ERR_OUT_OF_MEMORY; // Only original UNUSED quoted cells can retain metadata.
}
Error Registry::production_output_owner_bind(ProductionOutputPin &pin, RenderingDevice &rd, RID particles, uint64_t generation, uint32_t slot, uint64_t lease, ProductionOutputOwnerKind kind) {
	Lock lock(*this);
	if (!production_output_pin_matches_locked(pin, { instance, pin.request_serial }, rd)) {
		return ERR_UNAUTHORIZED;
	}
	const Error context = production_output_owner_context_locked(pin, rd);
	if (context != OK) {
		return context;
	}
	auto &request = *pin.request;
	auto &row = request.uses[pin.cell_index];
	if (request.capture_inflight || request.cancelled || request.release_started || request.control.state != ProductionControlState::EXECUTING || request.control.input.kind != ProductionControlKind::RECORD || request.next_action || request.graph_count || request.native_count) {
		return ERR_BUSY;
	}
	if (!rd._production_output_owner_matches(kind, particles, generation, slot, lease, pin, { instance, request.serial })) {
		return ERR_UNAUTHORIZED;
	}
	if (row.output_generation) {
		return row.output_owner_kind == kind && row.output_particles == particles && row.output_generation == generation && row.output_slot == slot && row.output_lease == lease ? OK : ERR_ALREADY_IN_USE;
	}
	row.output_owner_kind = kind;
	row.output_particles = particles;
	row.output_generation = generation;
	row.output_slot = slot;
	row.output_lease = lease;
	row.output_prev = UINT32_MAX;
	row.output_next = request.output_owner_head;
	if (row.output_next != UINT32_MAX) {
		request.uses[row.output_next].output_prev = pin.cell_index;
	}
	request.output_owner_head = pin.cell_index;
	if (request.output_owner_cursor == UINT32_MAX) {
		request.output_owner_cursor = pin.cell_index;
	}
	if (kind == ProductionOutputOwnerKind::GENERATION) {
		for (uint32_t i = 0; i < request.resource_count; ++i) {
			auto &resource = request.resources[i];
			if (resource.request == &request && resource.request_serial == request.serial &&
					resource.declaration.resource_kind == GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1 &&
					resource.declaration.ownership == GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1 &&
					resource.metadata_created && !resource.metadata_returned && resource.metadata_data && resource.metadata_consumer &&
					resource.metadata_root_serial == pin.serial) {
				resource.metadata_generation_bound = true;
			}
		}
	}
	return OK;
}

// Authenticate existing exact owner custody only. A different request, even
// from the same attachment, needs an original successor association that this
// leaf cannot manufacture. Cleanup keeps the original non-new-work gate.
Error Registry::production_output_cleanup_context(ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const ProductionOutputPin &pin, RenderingDevice &rd, RID particles, uint64_t generation, uint32_t slot, uint64_t lease) {
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	const Error error = production_physical_request_locked(context, ref, request, device, false);
	if (error != OK) {
		return error;
	}
	if (device != &rd || !production_output_pin_matches_locked(pin, ref, rd) || pin.request != request) {
		return ERR_UNAUTHORIZED;
	}
	const auto &row = request->uses[pin.cell_index];
	if (row.output_owner_kind != ProductionOutputOwnerKind::SLOT || !generation || !lease || row.output_particles != particles || row.output_generation != generation || row.output_slot != slot || row.output_lease != lease ||
			!rd._production_output_owner_matches(ProductionOutputOwnerKind::SLOT, particles, generation, slot, lease, pin, ref)) {
		return ERR_UNAUTHORIZED;
	}
	return OK;
}

// Withdraw is cleanup of the independently installed original generation
// root. A slot pin or matching scalar tuple cannot authenticate that root.
Error Registry::production_output_generation_cleanup_context(ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const ProductionOutputPin &pin, RenderingDevice &rd, RID particles, uint64_t generation) {
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	const Error error = production_physical_request_locked(context, ref, request, device, false);
	if (error != OK) {
		return error;
	}
	if (device != &rd || !production_output_pin_matches_locked(pin, ref, rd) || pin.request != request) {
		return ERR_UNAUTHORIZED;
	}
	const auto &row = request->uses[pin.cell_index];
	if (row.output_owner_kind != ProductionOutputOwnerKind::GENERATION || !generation || !particles.is_valid() ||
			row.output_particles != particles || row.output_generation != generation || row.output_slot != UINT32_MAX || row.output_lease ||
			!rd._production_output_owner_matches(ProductionOutputOwnerKind::GENERATION, particles, generation, UINT32_MAX, 0, pin, ref)) {
		return ERR_UNAUTHORIZED;
	}
	return OK;
}

// Enter and return holding Registry and RD locks, within original OBSERVE.
// One exact linked root row per dispatch; no arena scan or callback registry.
Error Registry::production_output_owners_service_locked(ProductionRequest &request, RenderingDevice &rd) {
	auto &control = request.control;
	if (control.state != ProductionControlState::EXECUTING || control.input.kind != ProductionControlKind::OBSERVE || control.input.request_serial != request.serial) {
		return ERR_UNAUTHORIZED;
	}
	if (request.output_owner_cursor == UINT32_MAX) {
		return OK;
	}
	const auto &budget = control.input.budget;
	auto &usage = control.output.service_usage;
	if (!budget.controls || usage.passes >= MIN(budget.passes, 64u) || usage.receipts >= MIN(budget.receipts, 128u) || !budget.deadline_micros || OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros || request.capture_inflight || request.continuation_inflight || request.source_device_lost || request.revision > UINT64_MAX - uint64_t(request.capacity.graph_memberships) * 2 - uint64_t(request.capacity.native_memberships) * 2 - request.capacity.readbacks - request.capacity.resources - 8) {
		return ERR_BUSY;
	}
	const uint32_t index = request.output_owner_cursor;
	if (index >= request.capacity.resource_uses) {
		return ERR_INVALID_DATA;
	}
	const auto &row = request.uses[index];
	if (row.request != &request || row.output_role != ProductionUseCell::OutputRole::NATIVE_CONSUMER || !row.output_generation || !row.output_consumer || !production_output_pin_matches_locked(*row.output_consumer, { instance, request.serial }, rd)) {
		return ERR_INVALID_DATA;
	}
	const ProductionOutputOwnerKind kind = row.output_owner_kind;
	const RID particles = row.output_particles;
	const uint64_t generation = row.output_generation, lease = row.output_lease, pin_serial = row.output_pin_serial;
	const uint32_t slot = row.output_slot;
	ProductionOutputPin *const pin = row.output_consumer; // Synchronous original custody, never persisted.
	if (!rd._production_output_owner_matches(kind, particles, generation, slot, lease, *pin, { instance, request.serial })) {
		return ERR_INVALID_DATA;
	}
	// Advance before owner may unlink this row. unpin repairs this cursor.
	request.output_owner_cursor = row.output_next != UINT32_MAX ? row.output_next : request.output_owner_head;
	const uint64_t serial = request.serial, control_serial = control.serial;
	auto *const attachment = request.attachment;
	const uint64_t epoch = attachment->epoch;
	usage.controls = 1; // Same original OBSERVE control, not a second pass.
	// One O(1) locator dispatch per control. Actual component phases alone
	// debit passes/receipts, so a positive one-pass slice can make progress.
	production_native_effect_enter_locked(request);
	++request.capture_inflight;
	rd._thread_safe_.unlock();
	unlock_mutex_for_call();
	const Error result = rd._production_output_owner_service(kind, particles, generation, slot, lease, *pin, budget, usage);
	// Blocking acquisition is RD -> Registry, as in native source capture and
	// owned factory return. Registry-held entry points use RD.try_lock() and
	// return ERR_BUSY on contention; they must never wait for RD. The capture
	// pin retains this original request while both locks are released.
	rd._thread_safe_.lock();
	relock_mutex_after_call();
	CRASH_COND(!request.capture_inflight);
	--request.capture_inflight;
	if (request.serial != serial || request.control.serial != control_serial || request.control.state != ProductionControlState::EXECUTING || request.attachment != attachment || attachment->held_device != &rd || attachment->epoch != epoch || device_records[attachment->device].epoch != epoch || device_records[attachment->device].object != &rd) {
		return ERR_INVALID_DATA;
	}
	// Re-resolve the original finite row. The callback may have unpinned it;
	// neither the old pin pointer nor an owner pointer is touched after return.
	const auto &returned = request.uses[index];
	if (returned.request == &request && returned.output_role == ProductionUseCell::OutputRole::UNUSED && returned.output_owner_kind == ProductionOutputOwnerKind::NONE && !returned.output_consumer && !returned.output_pin_serial && !returned.output_generation && returned.output_prev == UINT32_MAX && returned.output_next == UINT32_MAX) {
		return result;
	}
	if (returned.request != &request || returned.output_role != ProductionUseCell::OutputRole::NATIVE_CONSUMER || returned.output_pin_serial != pin_serial || returned.output_owner_kind != kind || returned.output_particles != particles || returned.output_generation != generation || returned.output_slot != slot || returned.output_lease != lease) {
		return ERR_INVALID_DATA;
	}
	return result;
}

Error Registry::production_output_generation_return(ProductionOutputPin &pin, RenderingDevice &rd, RID particles,
		uint64_t generation, const GDExtensionProductionServiceBudgetV1 &budget, ProductionServiceUsage &usage) {
	Lock lock(*this);
	if (!production_output_pin_matches_locked(pin, { instance, pin.request_serial }, rd)) {
		return ERR_UNAUTHORIZED;
	}
	auto &request = *pin.request;
	auto &control = request.control;
	const auto &row = request.uses[pin.cell_index];
	if (row.output_owner_kind != ProductionOutputOwnerKind::GENERATION || row.output_particles != particles || row.output_generation != generation || row.output_slot != UINT32_MAX || row.output_lease ||
			!rd._production_output_owner_matches(ProductionOutputOwnerKind::GENERATION, particles, generation, UINT32_MAX, 0, pin, { instance, request.serial }) ||
			control.state != ProductionControlState::EXECUTING || control.input.kind != ProductionControlKind::OBSERVE || control.input.request_serial != request.serial ||
			&budget != &control.input.budget || &usage != &control.output.service_usage || request.capture_inflight != 1 || request.continuation_inflight) {
		return ERR_UNAUTHORIZED;
	}
	if (control.output.output_generation.native_released || !budget.controls || usage.passes >= MIN(budget.passes, 64u) ||
			usage.receipts >= MIN(budget.receipts, 128u) || !budget.deadline_micros || OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros) {
		return ERR_BUSY;
	}
	ProductionOutputGenerationResult terminal = {};
	const Error ready = rd._production_output_generation_ready(particles, generation, pin, { instance, request.serial }, terminal.unused_slots, terminal.returned_slots);
	if (ready != OK) {
		return ready;
	}
	const Error projected = production_report_contents_locked(request, terminal.root_source);
	if (projected != OK) {
		return projected;
	}
	// Each installed generation's placement owner remains alive until its RID
	// is actually freed. Sibling generations must be able to return their roots
	// independently, including after a sibling root was unpinned but before its
	// particles_free. A metadata bind to an uninstalled pin is not this proof.
	// The complete request report and reclaim gate still count every allocation.
	uint32_t owner_metadata_pending = 0;
	for (uint32_t i = 0; i < request.resource_count; ++i) {
		const auto &resource = request.resources[i];
		owner_metadata_pending += resource.request == &request && resource.request_serial == request.serial &&
				resource.declaration.resource_kind == GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1 &&
				resource.declaration.ownership == GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1 &&
				resource.metadata_created && !resource.metadata_returned && resource.metadata_data && resource.metadata_consumer &&
				resource.metadata_root_serial && resource.metadata_generation_bound;
	}
	if (request.source_device_lost || (request.compute_list_open || request.draw_list_open) || (!request.cancelled && request.next_action != request.action_count) ||
			terminal.root_source.possible_obligations || terminal.root_source.physical_returns_pending != owner_metadata_pending ||
			terminal.root_source.known_obligations != terminal.root_source.complete_obligations) {
		return ERR_BUSY;
	}
	if (OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros) {
		return ERR_BUSY;
	}
	terminal.registry_instance = instance;
	terminal.request_serial = request.serial;
	terminal.control_serial = control.serial;
	terminal.root_serial = pin.serial;
	terminal.particles = particles.get_id();
	terminal.generation = generation;
	terminal.native_released = true;
	// Save while the original root is still retained. No concurrent reader can
	// consume EXECUTING; a failed unpin rolls back this tentative result.
	control.output.output_generation = terminal;
	const Error error = production_output_unpin_locked(pin);
	if (error != OK) {
		control.output.output_generation = {};
		return error;
	}
	++usage.passes;
	usage.controls = 1;
	// The original control/request keeps arena and code alive through consume.
	return OK;
}

Error Registry::production_output_unpin(ProductionOutputPin &pin) {
	Lock lock(*this);
	return production_output_unpin_locked(pin);
}

Error Registry::production_output_unpin_locked(ProductionOutputPin &pin) {
	if (!pin.registry) {
		return ERR_DOES_NOT_EXIST;
	}
	if (pin.registry != this || !pin.request || pin.request->serial != pin.request_serial || !pin.serial || pin.cell_index >= pin.request->capacity.resource_uses) {
		return ERR_INVALID_DATA;
	}
	auto &request = *pin.request;
	if (request.continuation_inflight) {
		return ERR_BUSY;
	}
	auto &attachment = *request.attachment;
	const auto &native = device_records[attachment.device];
	if (native.execution_thread != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	auto &cell = request.uses[pin.cell_index];
	if (cell.request != &request || cell.output_role != ProductionUseCell::OutputRole::NATIVE_CONSUMER || cell.output_consumer != &pin || cell.output_pin_serial != pin.serial || pin.cell_index < request.use_count + request.binding_used) {
		return ERR_INVALID_DATA;
	}
	if (cell.output_owner_kind == ProductionOutputOwnerKind::GENERATION) {
		const auto &control = request.control;
		const auto &terminal = control.output.output_generation;
		if (control.state != ProductionControlState::EXECUTING || control.input.kind != ProductionControlKind::OBSERVE ||
				!terminal.native_released || terminal.registry_instance != instance || terminal.request_serial != request.serial ||
				terminal.control_serial != control.serial || terminal.root_serial != pin.serial ||
				terminal.particles != cell.output_particles.get_id() || terminal.generation != cell.output_generation) {
			return ERR_BUSY;
		}
	}
	auto owns_group = [&](uint64_t serial) {
		for (uint32_t index = request.use_count + request.binding_used; index < request.capacity.resource_uses; ++index) {
			const auto &group = request.uses[index];
			if (group.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP && group.disposal_group_serial == serial && group.disposal_group_pin_serial == pin.serial) {
				return true;
			}
		}
		return false;
	};
	for (uint32_t i = request.use_count + request.binding_used; i < request.capacity.resource_uses; ++i) {
		const auto &member = request.uses[i];
		if (!owns_group(member.disposal_group_serial)) {
			continue;
		}
		if (member.output_role == ProductionUseCell::OutputRole::DISPOSAL_RESERVED) {
			return ERR_BUSY;
		}
		if (member.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP && !member.disposal_group_dropped) {
			return ERR_BUSY;
		}
		if (member.output_role == ProductionUseCell::OutputRole::DISPOSAL_MEMBER) {
			const uint8_t state = member.disposal.state.load(std::memory_order_acquire);
			if (state != 3 && state != 4 && state != 6 && state != 7) {
				return ERR_BUSY;
			}
		}
	}
	if (cell.output_owner_kind == ProductionOutputOwnerKind::SLOT) {
		for (uint32_t i = 0; i < request.action_count; ++i) {
			auto &action = request.actions[i];
			if (action.output_root == pin.serial && action.output_particles == cell.output_particles && action.output_generation == cell.output_generation && action.output_slot == cell.output_slot && action.output_lease == cell.output_lease) {
				action.output_state = ProductionActionCell::OutputState::RETURNED;
			}
		}
	}
	// Return only this custody edge. No request/native completion, driver call,
	// unpin of backing resources, new admission or metadata refund is implied.
	if (cell.output_generation) {
		if (cell.output_prev != UINT32_MAX) {
			request.uses[cell.output_prev].output_next = cell.output_next;
		} else {
			request.output_owner_head = cell.output_next;
		}
		if (cell.output_next != UINT32_MAX) {
			request.uses[cell.output_next].output_prev = cell.output_prev;
		}
		if (request.output_owner_cursor == pin.cell_index) {
			request.output_owner_cursor = cell.output_next != UINT32_MAX ? cell.output_next : request.output_owner_head;
		}
		cell.output_owner_kind = ProductionOutputOwnerKind::NONE;
		cell.output_particles = RID();
		cell.output_generation = cell.output_lease = 0;
		cell.output_slot = cell.output_prev = cell.output_next = UINT32_MAX;
	}
	cell.output_demand_live.store(false, std::memory_order_release);
	cell.output_demand_ready = false;
	cell.output_demand = {};
	cell.output_role = ProductionUseCell::OutputRole::UNUSED;
	cell.output_consumer = nullptr;
	cell.output_pin_serial = 0;
	pin.registry = nullptr;
	pin.request = nullptr;
	pin.request_serial = pin.serial = 0;
	pin.cell_index = UINT32_MAX;
	return OK;
}
GDExtensionDeviceSessionResultV1 Registry::production_release(GDExtensionProductionRequestRefV1 ref) {
	Lock lock(*this);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_request_locked(ref, false, r, rd);
	if (e != OK) {
		return result(e, 16);
	}
	production_native_record_reject_wait_locked(*r);
	if (r->control.state != ProductionControlState::OWNER_CONSUMED) {
		return result(ERR_BUSY, 16);
	}
	return production_release_locked(*r, rd, true);
}
bool Registry::production_release_owner_tail_locked(const ProductionRequest &r, RenderingDevice *rd, bool require_proof) const {
	if (!r.serial || !r.attachment || !rd || !rd->driver || r.attachment->held_device != rd || r.capture_inflight || r.continuation_inflight || r.effects_pending || r.source_device_lost || r.maintenance_serial || r.compute_list_open || r.draw_list_open || (!r.cancelled && r.next_action != r.action_count)) {
		return false;
	}
	const auto &device = device_records[r.attachment->device];
	if (!device.live || device.object != rd || device.epoch != r.attachment->epoch || !rd->is_main_instance) {
		return false;
	}
	const auto &proof = r.release_ready;
	if (require_proof && (!proof.control_serial || proof.request_serial != r.serial || proof.revision != r.revision || proof.record_generation != r.operation.record_generation || proof.device_epoch != r.attachment->epoch || proof.next_pin_serial != r.next_output_pin_serial || proof.native_count != r.native_count || proof.graph_count != r.graph_count)) {
		return false;
	}
	if (r.resource_count > r.capacity.resources || r.action_count > r.capacity.suboperations || r.graph_count > r.capacity.graph_memberships || r.native_count > r.capacity.native_memberships || r.readback_count > r.capacity.readbacks) {
		return false;
	}
	for (uint32_t i = 0; i < r.capacity.resource_uses; ++i) {
		if (r.uses[i].output_consumer) {
			return false;
		}
	}
	for (uint32_t i = 0; i < r.resource_count; ++i) {
		if (r.resources[i].transfer_pending || r.resources[i].metadata_data || r.resources[i].metadata_consumer || !r.resources[i].output_predecessor_view.is_empty()) {
			return false;
		}
	}
	for (uint32_t i = 0; i < r.graph_count; ++i) {
		if (!r.graphs || r.graphs[i].request != &r || r.graphs[i].pending) {
			return false;
		}
	}
	for (uint32_t i = 0; i < r.capacity.return_cells; ++i) {
		if (r.returns[i].pending) {
			return false;
		}
	}
	for (uint32_t i = 0; i < r.readback_count; ++i) {
		if (r.readbacks[i].source.issued && r.readbacks[i].source.returned != r.readbacks[i].source.issued) {
			return false;
		}
	}
	for (uint32_t i = 0; i < r.action_count; ++i) {
		const auto &action = r.actions[i];
		if (action.scene_attempt.content.is_valid() || action.preinput_camera_source.is_valid() || (action.camera_ready_subscriber.registry && !action.camera_ready_subscriber.released) || (action.preinput_host_prepared && !action.preinput_host_returned)) {
			return false;
		}
	}
	bool pending = false;
	for (uint32_t i = 0; i < r.native_count; ++i) {
		const ProductionNativeCell *owner = nullptr;
		if (production_native_owner_locked(r, i, owner) != OK) {
			return false;
		}
		if (r.natives[i].owner_index != i || owner->member.source.issue == RDD::IssueFact::NotIssued) {
			continue;
		}
		if (owner->observation.state != NativeSourceObservation::State::Complete || !owner->member.retained) {
			return false;
		}
		pending |= !owner->member.owner_released;
	}
	return pending;
}

void Registry::production_release_owner_execute_locked(ProductionRequest &request, RenderingDevice *rd) {
	auto &cell = request.control;
	CRASH_COND(cell.state != ProductionControlState::PENDING || cell.input.kind != ProductionControlKind::RELEASE || !cell.input.release_owner_only);
	cell.state = ProductionControlState::EXECUTING;
	cell.output = {};
	cell.output.request_serial = request.serial;
	cell.output.control_serial = cell.serial;
	cell.output.service_usage.controls = cell.input.budget.controls ? 1 : 0;
	if (!production_release_owner_tail_locked(request, rd, true) || !service_budget_active(cell.input.budget)) {
		request.release_ready = {};
		cell.output.result = result(ERR_BUSY, 16);
	} else {
		// Actual original owner cursor work under this NEW cell's immutable slice.
		// The arena stays alive until a later consume collects this exact result.
		ProductionServiceUsage used{};
		cell.output.result = production_release_locked(request, rd, true, &cell.input.budget, &used, true);
		CRASH_COND(used.controls > 1 || used.passes > MIN(cell.input.budget.passes, 64u) || used.receipts > MIN(cell.input.budget.receipts, 128u));
		cell.output.service_usage.passes = used.passes;
		cell.output.service_usage.receipts = used.receipts;
	}
	production_report_locked(request, cell.output.report);
	cell.state = ProductionControlState::RETURNED;
	request.attachment->metadata_pending = request.attachment->metadata_receiver != nullptr;
	if (NativeDrainDiagnostic::enabled()) {
		NativeDrainDiagnostic::Span trace("release.owner.dispatch", request.serial, cell.serial, cell.output.service_usage.controls, cell.output.service_usage.passes, cell.output.service_usage.receipts, request.release_cursor);
		trace.result(cell.output.result.native_error, "fresh_owner_stage_not_rt_dispatch");
	}
}

GDExtensionDeviceSessionResultV1 Registry::production_release_locked(ProductionRequest &request, RenderingDevice *rd, bool reclaim, const GDExtensionProductionServiceBudgetV1 *budget, ProductionServiceUsage *usage, bool preserve_arena) {
	auto *r = &request;
	if (usage) {
		*usage = {};
	}
	if (request.native_prepared_unread) { return result(ERR_BUSY, 16); } // Exact native replies remain with their original owner.
	// Original RT owner must return this hold after pipeline physical return.
	// Owner/library release cannot reclaim an arena still named by a pipeline.
	for (uint32_t i = 0; i < r->resource_count; ++i) {
		if (r->resources[i].material_pipeline_shader_hold.is_active() || r->resources[i].material_pipeline_reservation.is_valid()) {
			return result(ERR_BUSY, 16);
		}
	}
	if (r->uses) {
		for (uint32_t i = 0; i < r->capacity.resource_uses; ++i) {
			if (r->uses[i].output_consumer) {
				return result(ERR_BUSY, 16); // Actual consumer last-use still owns this arena.
			}
		}
	}
	if (r->maintenance_serial) {
		if (!rd || !rd->_thread_safe_.try_lock()) {
			return result(ERR_BUSY, 16);
		}
		const bool active = rd->native_disposal_active && r->maintenance_serial == rd->native_disposal_continuation.serial;
		rd->_thread_safe_.unlock();
		if (active) {
			return result(ERR_BUSY, 16); // Finish the original accepted cursor before reclaim.
		}
		r->maintenance_serial = 0;
	}
	if (r->capture_inflight || r->continuation_inflight || r->effects_pending || r->source_device_lost) {
		return result(ERR_BUSY, 16);
	}
	if (r->graph_count > r->capacity.graph_memberships || r->native_count > r->capacity.native_memberships) {
		return result(ERR_INVALID_DATA, 16);
	}
	for (uint32_t i = 0; i < r->graph_count; ++i) {
		if (!r->graphs || r->graphs[i].request != r) {
			return result(ERR_INVALID_DATA, 16);
		}
		if (r->graphs[i].pending) {
			return result(ERR_BUSY, 16);
		}
	}
	for (uint32_t i = 0; i < r->resource_count; ++i) {
		auto &resource = r->resources[i];
		if (resource.transfer_pending) {
			return result(ERR_BUSY, 16);
		}
		if (!resource.metadata_data && !resource.metadata_consumer) {
			continue;
		}
		if (resource.metadata_kind == 2) {
			return result(output_metadata_selection_release_locked(*r, resource, rd, reclaim, budget, usage), 16);
		}
		if (reclaim || !resource.metadata_generation_bound || !resource.metadata_root_serial || !resource.metadata_consumed_control_serial ||
				!resource.metadata_consumed_particles || !resource.metadata_consumed_generation || !resource.metadata_data || !resource.metadata_consumer || resource.metadata_returned) {
			return result(ERR_BUSY, 16);
		}
		// Host readiness may admit only this exact consumed tail. Reclaim and
		// zero-budget inspection never destroy an owner or return its allocation.
		if (!budget && !usage) {
			return result(OK);
		}
		if (!rd || !rd->native_immutable_is_render_thread() || !r->attachment || device_records[r->attachment->device].execution_thread != Thread::get_caller_id() ||
				!budget || !usage || !service_budget_active(*budget) || !budget->passes || !budget->receipts) {
			return result(ERR_BUSY, 16);
		}
		if (r->control.state != ProductionControlState::EXECUTING || r->control.input.kind != ProductionControlKind::RELEASE || r->control.input.request_serial != r->serial || r->control.serial <= resource.metadata_consumed_control_serial) {
			return result(ERR_UNAUTHORIZED, 16);
		}
		if (r->revision == UINT64_MAX) {
			return result(ERR_PARAMETER_RANGE_ERROR, 16);
		}
		auto *storage = RendererRD::ParticlesStorage::get_singleton();
		const RID particles_rid = RID::from_uint64(resource.metadata_consumed_particles);
		auto *particles = storage ? storage->particles_owner.get_or_null(particles_rid) : nullptr;
		auto *owner = particles ? particles->external : nullptr;
		if (!owner || static_cast<void *>(owner) != resource.metadata_data || &owner->metadata_lease != resource.metadata_consumer ||
				owner->metadata_lease.registry != this || owner->metadata_lease.request != r || owner->metadata_lease.request_serial != r->serial ||
				owner->metadata_lease.resource_slot != i || owner->metadata_lease.data != resource.metadata_data ||
				owner->device != rd || owner->generation_registry != this || owner->generation_request.registry_instance != instance ||
				owner->generation_request.handle_serial != r->serial || owner->generation_root_serial != resource.metadata_root_serial ||
				owner->generation != resource.metadata_consumed_generation || !owner->generation_pin.is_empty() ||
				owner->state != RendererRD::ExternalOutputOwner::GenerationState::RELEASED || owner->generation_selection_unpublished ||
				owner->surface_cells || owner->view_cells || owner->read_cells || owner->generation_meshes || owner->generation_materials ||
				owner->preparation_rows || owner->preparation_capacity || owner->has_generation_selection_custody() ||
				owner->surface_capacity || owner->view_capacity || owner->read_capacity || owner->has_live_slots()) {
			return result(ERR_INVALID_DATA, 16);
		}
		// Pin the original arena across real dependency notifications and metadata
		// return. Never call particles_free or its destructor under Registry.
		++r->capture_inflight;
		++usage->passes;
		++usage->receipts;
		unlock_mutex_for_call();
		storage->particles_free(particles_rid);
		relock_mutex_after_call();
		--r->capture_inflight;
		if (storage->owns_particles(particles_rid) || !resource.metadata_returned || resource.metadata_data || resource.metadata_consumer) {
			return result(ERR_BUSY, 16);
		}
		return result(ERR_BUSY, 16); // One real owner destruction per paid turn.
	}
	bool native_refs_pending = false, native_issued = false;
	for (uint32_t i = 0; i < r->native_count; ++i) {
		const ProductionNativeCell *owner = nullptr;
		const Error error = production_native_owner_locked(*r, i, owner);
		if (error != OK) {
			return result(error, 16);
		}
		if (r->natives[i].owner_index != i) {
			continue;
		}
		if (owner->member.source.issue == RDD::IssueFact::NotIssued) {
			if (owner->member.retained || owner->member.owner_released) {
				return result(ERR_INVALID_DATA, 16);
			}
			continue;
		}
		native_issued = true;
		if (owner->observation.state != NativeSourceObservation::State::Complete || !owner->member.retained) {
			return result(ERR_BUSY, 16);
		}
		native_refs_pending |= !owner->member.owner_released;
	}
	if (native_issued && (!rd || !r->attachment || !device_records[r->attachment->device].live)) {
		return result(ERR_UNAVAILABLE, 16);
	}
	for (uint32_t i = 0; i < r->capacity.return_cells; ++i) {
		if (r->returns[i].pending) {
			return result(ERR_BUSY, 16);
		}
	}
	for (uint32_t i = 0; i < r->readback_count; ++i) {
		if (r->readbacks[i].source.issued && r->readbacks[i].source.returned != r->readbacks[i].source.issued) {
			return result(ERR_BUSY, 16);
		}
	}
	for (uint32_t i = 0; i < r->action_count; ++i) {
		auto &content = r->actions[i].scene_attempt.content;
		if (!content.is_valid()) {
			continue;
		}
		if (reclaim) {
			return result(ERR_BUSY, 16);
		}
		if (!budget && !usage) {
			return result(OK);
		} // Readiness only, no owner call.
		if (!budget || !usage || !service_budget_active(*budget) || usage->passes >= MIN(budget->passes, 64u) || usage->receipts >= MIN(budget->receipts, 128u)) {
			return result(ERR_BUSY, 16);
		}
		++r->capture_inflight;
		++usage->passes;
		++usage->receipts;
		Ref<SceneReadContent> returning = content;
		unlock_mutex_for_call();
		const Error returned = returning->return_after_reads();
		relock_mutex_after_call();
		if (returned == OK) {
			content.unref();
		} // returning still pins destructor.
		unlock_mutex_for_call();
		returning.unref(); // Native owner destruction never runs under Registry.
		relock_mutex_after_call();
		--r->capture_inflight;
		return result(returned == OK ? ERR_BUSY : returned, 16); // One charged owner return per turn.
	}
	// The predecessor pin outlives all actual consumer graphs/native observations.
	// Return it only in the original charged RT RELEASE, never metadata reclaim.
	for (uint32_t i = 0; i < r->resource_count; ++i) {
		auto &view = r->resources[i].output_predecessor_view;
		if (view.is_empty()) { continue; }
		if (reclaim) { return result(ERR_BUSY, 16); }
		if (!budget && !usage) { return result(OK); } // Pure readiness, no source return.
		if (!rd || !rd->native_immutable_is_render_thread() || !budget || !usage || !service_budget_active(*budget) ||
				usage->passes >= MIN(budget->passes, 64u) || usage->receipts >= MIN(budget->receipts, 128u) ||
				!rd->_thread_safe_.try_lock()) { return result(ERR_BUSY, 16); }
		++usage->passes;
		++usage->receipts;
		const Error returned = production_output_view_return_locked(view);
		rd->_thread_safe_.unlock();
		return result(returned == OK ? ERR_BUSY : returned, 16); // Exactly one original source return.
	}
	// Camera source is the original existing sample, retained in the paid action.
	// Its final buffer/source destruction belongs to the original RT service.
	for (uint32_t i = 0; i < r->action_count; ++i) {
		auto &action = r->actions[i];
		if (action.preinput_host_prepared && !action.preinput_host_returned && !reclaim && budget && usage && service_budget_active(*budget) && rd && rd->native_immutable_is_render_thread()) {
			// Copied Host values have no external owner/destructor or extra return
			// credit. They expire with this original request's RT release path.
			action.output_scene = {};
			action.output_scene_prepared = false;
			action.preinput_host_transform = {};
			action.preinput_host_returned = true;
		}
		if (action.camera_ready_subscriber.registry && !action.camera_ready_subscriber.released) {
			if (reclaim) {
				return result(ERR_BUSY, 16);
			}
			if (!budget && !usage) {
				return result(OK); // Readiness only, no renderer mutation.
			}
			if (!rd || !rd->native_immutable_is_render_thread() || !RSG::viewport || !budget || !usage || !service_budget_active(*budget) || usage->passes >= MIN(budget->passes, 64u) || usage->receipts >= MIN(budget->receipts, 128u)) {
				return result(ERR_BUSY, 16);
			}
			auto *returning = &action.camera_ready_subscriber;
			if (returning->is_camera_source_detaching()) {
				return result(ERR_BUSY, 16); // Original renderer close chain still owns this facet.
			}
			const RID viewport = action.camera_ready_viewport;
			++r->capture_inflight;
			++usage->passes;
			++usage->receipts;
			unlock_mutex_for_call();
			// The viewport may already have closed or freed the subscription.
			// This RT turn also stops a still queued delivery without a next draw.
			RSG::viewport->viewport_unsubscribe_camera_source(viewport, returning);
			if (!returning->subscribed) {
				returning->camera_source_closed(); // Never installed: no renderer links exist.
			}
			relock_mutex_after_call();
			if (!returning->closed) {
				// A viewport-wide detach may still own this facet in its close chain.
				// A false unsubscribe cannot authorize reclaim ahead of its callback.
				--r->capture_inflight;
				return result(ERR_BUSY, 16);
			}
			// Renderer has removed every intrusive/pending link before closed.
			// The original arena pin prevents reentrant reclaim until this point.
			returning->subscribed = false;
			returning->released = true;
			action.camera_ready_viewport = RID();
			--r->capture_inflight;
			return result(ERR_BUSY, 16); // One original charged owner return.
		}
		if (action.preinput_camera_source.is_null()) {
			continue;
		}
		if (reclaim) {
			return result(ERR_BUSY, 16);
		}
		if (!budget && !usage) {
			return result(OK); // Readiness only, never release source custody.
		}
		if (!rd || !rd->native_immutable_is_render_thread() || !budget || !usage || !service_budget_active(*budget) || usage->passes >= MIN(budget->passes, 64u) || usage->receipts >= MIN(budget->receipts, 128u)) {
			return result(ERR_BUSY, 16);
		}
		++r->capture_inflight;
		++usage->passes;
		++usage->receipts;
		Ref<RendererViewport::Viewport::CameraCapture> returning = action.preinput_camera_source;
		action.preinput_camera_source.unref(); // returning pins final destruction.
		action.preinput_camera_returned = true;
		unlock_mutex_for_call();
		returning.unref();
		relock_mutex_after_call();
		--r->capture_inflight;
		return result(ERR_BUSY, 16); // Exactly one charged original source return.
	}
	// Native observer references may only be dropped in the original owner's
	// final-release phase. RT readiness neither releases them nor signs R01 ACK.
	if (!reclaim && native_issued) {
		return result(OK);
	}
	bool has_pins = false;
	for (uint32_t i = 0; i < r->resource_count; ++i) {
		has_pins |= r->resources[i].use_count != 0;
	}
	if ((has_pins || native_refs_pending) && (!rd || !rd->_thread_safe_.try_lock())) {
		return result(ERR_BUSY, 16);
	}
	const bool locked = has_pins || native_refs_pending;
	Error e = OK;
	if (native_refs_pending) {
		if (!rd->driver || !budget || !service_budget_active(*budget) || r->revision > UINT64_MAX - 2) {
			rd->_thread_safe_.unlock();
			return result(r->revision > UINT64_MAX - 2 ? ERR_PARAMETER_RANGE_ERROR : ERR_BUSY, 16);
		}
		ProductionServiceUsage used = { 1, 0, 0 };
		r->release_started = true;
		const uint32_t visits = MIN(MIN(budget->passes, r->native_count), 64u);
		const uint32_t receipts = MIN(budget->receipts, 128u);
		while (used.passes < visits && used.receipts < receipts && before_service_deadline(*budget)) {
			if (r->revision > UINT64_MAX - 2) {
				e = ERR_PARAMETER_RANGE_ERROR;
				break;
			}
			const uint32_t index = r->release_cursor % r->native_count;
			r->release_cursor = (index + 1) % r->native_count;
			++used.passes;
			auto &cell = r->natives[index];
			if (cell.owner_index != index || !cell.member.retained || cell.member.owner_released) {
				continue;
			}
			if (!before_service_deadline(*budget)) {
				break;
			}
			++used.receipts; // Exactly one original driver reference-return attempt.
			auto returned_member = cell.member;
			const uint64_t request_serial = r->serial;
			++r->capture_inflight;
			unlock_mutex_for_call();
			const Error returned = release_native_source(*rd->driver, returned_member);
			relock_mutex_after_call();
			CRASH_COND(r->serial != request_serial || !r->capture_inflight || cell.request != r);
			cell.member = returned_member;
			--r->capture_inflight;
			if (cell.release_error != returned || returned == OK) {
				if (r->release_ready.revision == r->revision) { ++r->release_ready.revision; }
				++r->revision;
			}
			cell.release_error = returned;
			if (e == OK && returned != OK) {
				e = returned;
			}
		}
		if (usage) {
			*usage = used;
		}
		for (uint32_t i = 0; i < r->native_count; ++i) {
			const auto &cell = r->natives[i];
			if (cell.owner_index == i && cell.member.retained && !cell.member.owner_released && e == OK) {
				e = ERR_BUSY;
			}
		}
		if (e != OK) {
			rd->_thread_safe_.unlock();
			return result(e, 16);
		}
	}
	if (preserve_arena) {
		if (locked) { rd->_thread_safe_.unlock(); }
		return result(OK); // Owner-step facts returned; arena is not yet reclaimed.
	}
	e = production_unpin_resources_locked(*r);
	if (locked) {
		rd->_thread_safe_.unlock();
	}
	if (e != OK) {
		return result(e, 16);
	}
	// RT metadata cleanup returns before its owner may reset the arena slot.
	if (!reclaim) {
		return result(OK);
	}
	auto reset = [](auto *cells, uint32_t count) { using T=std::remove_pointer_t<decltype(cells)>;for(uint32_t i=0;i<count;++i){cells[i].~T();new (&cells[i]) T();} };
	reset(r->actions, r->capacity.suboperations);
	reset(r->resources, r->capacity.resources);
	reset(r->uses, r->capacity.resource_uses);
	reset(r->readbacks, r->capacity.readbacks);
	reset(r->graphs, r->capacity.graph_memberships);
	reset(r->natives, r->capacity.native_memberships);
	reset(r->returns, r->capacity.return_cells);
	reset(r->commands, r->command_count);
	reset(r->frames, r->frame_count);
	CRASH_COND(!r->attachment->children);
	--r->attachment->children;
	r->~ProductionRequest();
	new (r) ProductionRequest();
	return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::production_installation_observe(GDExtensionProductionAttachmentRefV1 ref, const GDExtensionProductionServiceBudgetV1 *budget, GDExtensionProductionInstallationReportV1 *out) {
	if (!aligned(budget) || !production_dto_valid(*budget) || budget->reserved || !aligned(out)) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	Lock lock(*this);
	ProductionAttachment *a = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_attachment_locked(ref, false, a, rd);
	if (e != OK) {
		return result(e, 16);
	}
	*out = { production_dto_abi<GDExtensionProductionInstallationReportV1>(), uint32_t(a->closing ? GDEXTENSION_PRODUCTION_INSTALL_RETURN_PENDING_V1 : GDEXTENSION_PRODUCTION_INSTALL_ACCEPTED_V1), 0, a->quote.allocation_bytes, 0, result(a->children ? ERR_BUSY : OK) };
	return result(OK);
}
Error Registry::production_entity_return_locked(ProductionAttachment &attachment) {
	auto *rd = attachment.held_device;
	if (attachment.children) {
		return ERR_BUSY;
	}
	if (!rd || device_records[attachment.device].object != rd || device_records[attachment.device].epoch != attachment.epoch ||
			rd->production_entity_registry.load(std::memory_order_acquire) != this || !attachment.holder ||
			attachment.holder->generation != attachment.library_generation || attachment.holder->terminal_action != attachment.terminal_action ||
			!attachment.holder->production_children) {
		return ERR_INVALID_DATA;
	}
	if ((attachment.entity_previous && attachment.entity_previous->entity_next != &attachment) ||
			(!attachment.entity_previous && rd->production_entity_holds != &attachment) ||
			(attachment.entity_next && attachment.entity_next->entity_previous != &attachment)) {
		return ERR_INVALID_DATA;
	}
	if (attachment.entity_previous) {
		attachment.entity_previous->entity_next = attachment.entity_next;
	} else {
		rd->production_entity_holds = attachment.entity_next;
	}
	if (attachment.entity_next) {
		attachment.entity_next->entity_previous = attachment.entity_previous;
	}
	if (!rd->production_entity_holds) {
		rd->production_entity_registry.store(nullptr, std::memory_order_release);
	}
	attachment.held_device = nullptr;
	attachment.entity_previous = nullptr;
	attachment.entity_next = nullptr;
	return OK;
}
GDExtensionDeviceSessionResultV1 Registry::production_close(GDExtensionProductionAttachmentRefV1 ref) {
	Lock lock(*this);
	ProductionAttachment *a = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_attachment_locked(ref, false, a, rd);
	if (e != OK) {
		return result(e, 16);
	}
	if (a->metadata_active || a->camera_ready_active || a->camera_ready_close_active) {
		return result(ERR_BUSY, 16);
	}
	if (a->camera_ready_relay.serial && !a->camera_ready_relay_closed) {
		if (find_normal_relay_locked(a->camera_ready_relay)) { return result(ERR_BUSY, 16); }
		// Original forget is possible only after actual holder closure.
		a->camera_ready_closed = a->camera_ready_relay_closed = true;
	}
	for (uint32_t i = 0; i < a->quote.capacity.requests; ++i) {
		if (a->requests[i].control.state != ProductionControlState::OWNER_CONSUMED) {
			return result(ERR_BUSY, 16);
		}
	}
	for (uint32_t i = 0; i < a->quote.capacity.requests; ++i) {
		const auto &request = a->requests[i];
		if (request.serial && !request.cancelled && request.revision == UINT64_MAX) {
			return result(ERR_PARAMETER_RANGE_ERROR, 19);
		}
	}
	a->closing = true;
	scene_owner_close_locked(*a); // Only an accepted close revokes Scene admission.
	for (uint32_t i = 0; i < a->quote.capacity.requests; ++i) {
		auto &request = a->requests[i];
		if (request.serial && !request.cancelled) {
			request.cancelled = true;
			++request.revision;
		}
	}
	if (rd && rd->render_thread_id == Thread::get_caller_id() && rd->_thread_safe_.try_lock()) {
		RdUnlock unlock{ rd->_thread_safe_ };
		for (uint32_t i = 0; i < a->quote.capacity.requests; ++i) {
			if (a->requests[i].serial && (a->requests[i].compute_list_open || a->requests[i].draw_list_open)) {
				production_close_lists_locked(a->requests[i], *rd);
			}
		}
	}
	if (a->children) {
		return result(ERR_BUSY, 16);
	}
	e = production_entity_return_locked(*a);
	if (e != OK) {
		return result(e, 16);
	}
	const uint32_t library = a->library, device = a->device;
	auto &record = records[library];
	auto &d = device_records[device];
	CRASH_COND(!record.private_refs || !d.refs || !a->holder->production_children || record.cleanup_reservations < a->quote.cleanup_slots || record.cleanup_by_phase[uint8_t(CleanupPhase::BINDING)] < a->quote.cleanup_slots);
	--record.private_refs;
	a->metadata_reference.registry = nullptr; // Same original counted edge was returned above.
	--d.refs;
	--a->holder->production_children;
	record.cleanup_reservations -= uint32_t(a->quote.cleanup_slots);
	record.cleanup_by_phase[uint8_t(CleanupPhase::BINDING)] -= uint32_t(a->quote.cleanup_slots);
	for (auto *&entry : production_attachments) {
		if (entry == a) {
			entry = nullptr;
			break;
		}
	}
	auto destroy = [](auto *cells, uint32_t count) { using T=std::remove_pointer_t<decltype(cells)>;for(uint32_t i=0;i<count;++i){cells[i].~T();
} };
	destroy(a->requests, a->quote.capacity.requests);
	destroy(a->actions, a->quote.capacity.suboperations);
	destroy(a->resources, a->quote.capacity.resources);
	destroy(a->uses, a->quote.capacity.resource_uses);
	destroy(a->readbacks, a->quote.capacity.readbacks);
	destroy(a->graphs, a->quote.capacity.graph_memberships);
	destroy(a->natives, a->quote.capacity.native_memberships);
	destroy(a->returns, a->quote.capacity.return_cells);
	destroy(a->commands, uint32_t(a->quote.graph_slots));
	destroy(a->frames, uint32_t(a->quote.graph_frames));
	void *arena = a->arena;
	a->~ProductionAttachment();
	Memory::free_static(arena);
	reclaim_device_locked(device);
	reclaim(library);
	try_finalize();
	return result(OK);
}

static_assert(std::is_trivially_copyable_v<ProductionControlInput>);
static_assert(std::is_trivially_copyable_v<ProductionOutputGenerationResult>);
static_assert(std::is_trivially_copyable_v<ProductionControlOutput>);
static_assert(std::is_trivially_copyable_v<ProductionControlCell>);

Error Registry::production_control_accept(GDExtensionProductionRequestRefV1 ref, ProductionControlKind kind, uint64_t *out, const GDExtensionProductionServiceBudgetV1 *budget) {
	GDExtensionProductionServiceBudgetV1 limit = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), 0, 0, 0, 0, 0 };
	if (budget) {
		if (!aligned(budget) || !production_dto_valid(*budget) || budget->reserved) {
			return ERR_INVALID_PARAMETER;
		}
		limit = *budget;
	}
	if (!aligned(out) || uint32_t(kind) > uint32_t(ProductionControlKind::RELEASE)) {
		return ERR_INVALID_PARAMETER;
	}
	Lock lock(*this);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_request_locked(ref, false, r, rd);
	if (error != OK) {
		return error;
	}
	if (r->continuation_inflight) {
		return ERR_BUSY;
	}
	auto &cell = r->control;
	if (cell.state != ProductionControlState::OWNER_CONSUMED) {
		// A retry retrieves the same retained operation, never overwrites it.
		if (cell.input.kind != kind || !same_service_budget(cell.input.budget, limit)) {
			return ERR_BUSY;
		}
		*out = cell.serial;
		return OK;
	}
	if (!r->next_control_serial) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	const bool owner_only = kind == ProductionControlKind::RELEASE && service_budget_active(limit) && production_release_owner_tail_locked(*r, rd, true);
	if (!owner_only) { r->release_ready = {}; }
	cell = {};
	cell.serial = r->next_control_serial;
	r->next_control_serial = cell.serial == UINT64_MAX ? 0 : cell.serial + 1;
	cell.input = { r->serial, r->operation.record_generation, kind, limit, UINT32_MAX, owner_only };
	cell.state = ProductionControlState::PENDING;
	// The existing request child retains this entire arena and original holder.
	// Close and direct Release refuse this cell until its owner consumes return.
	*out = cell.serial;
	return OK;
}

Error Registry::production_control_schedule(GDExtensionProductionRequestRefV1 ref, uint64_t serial) {
	NativeDrainDiagnostic::Span drain_trace("control.schedule", serial);
	Lock lock(*this);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_request_locked(ref, false, r, rd);
	if (error != OK) {
		return drain_trace.result(error, "request_lookup_failed");
	}
	auto &cell = r->control;
	drain_trace.state(r->serial, serial, uint64_t(cell.state), uint64_t(cell.input.kind), r->maintenance_serial, 0);
	if (!serial || cell.serial != serial || cell.state == ProductionControlState::OWNER_CONSUMED) {
		return drain_trace.result(ERR_DOES_NOT_EXIST, "serial_or_state_mismatch");
	}
	if (cell.state != ProductionControlState::PENDING) {
		return drain_trace.result(OK, "existing_control");
	}
	if (cell.input.release_owner_only) {
		if (this != &get_singleton() || cell.input.kind != ProductionControlKind::RELEASE || cell.input.request_serial != r->serial || cell.input.record_generation != r->operation.record_generation) { return drain_trace.result(ERR_INVALID_DATA, "owner_phase_identity_rejected"); }
		cell.record_history.schedule_attempted = true;
		cell.record_history.queue_possible = false;
		cell.record_history.admitted = true;
		cell.record_history.schedule_result_valid = true;
		cell.record_history.last_schedule_result = result(OK);
		production_release_owner_execute_locked(*r, rd);
		return drain_trace.result(OK, "original_owner_stage_returned");
	}
	auto *server = Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton());
	if (!server || this != &get_singleton()) {
		cell.record_history.schedule_result_valid = true;
		cell.record_history.last_schedule_result = result(ERR_UNAVAILABLE);
		return drain_trace.result(ERR_UNAVAILABLE, "server_unavailable");
	}
	// Publication holds the original Registry lock; CQMT does not execute inline
	// and releases its lock before dispatch. The scalar command owns no DSO code.
	cell.record_history.schedule_attempted = true;
	cell.record_history.queue_possible = true;
	cell.state = ProductionControlState::QUEUED;
	if (!server->_try_production_control_dispatch(instance, r->attachment->serial, r->serial, serial)) {
		cell.state = ProductionControlState::PENDING;
		cell.record_history.queue_possible = false; // FIFO false means no publication.
		cell.record_history.schedule_result_valid = true;
		cell.record_history.last_schedule_result = result(ERR_BUSY);
		drain_trace.state(r->serial, serial, uint64_t(cell.state), uint64_t(cell.input.kind), r->maintenance_serial, 0);
		return drain_trace.result(ERR_BUSY, "queue_refused");
	}
	cell.record_history.admitted = true;
	cell.record_history.schedule_result_valid = true;
	cell.record_history.last_schedule_result = result(OK);
	drain_trace.state(r->serial, serial, uint64_t(cell.state), uint64_t(cell.input.kind), r->maintenance_serial, 1);
	return drain_trace.result(OK, "queue_accepted");
}

bool Registry::production_native_record_reject_wait_locked(ProductionRequest &request) {
	auto &cell = request.control;
	if (!request.serial || !request.attachment || request.contract_version != 2 || !request.native_prepared_executor || (!request.frozen && !request.native_static_sealed) ||
			!cell.serial || cell.state != ProductionControlState::QUEUED || !cell.scene_waiting || cell.input.kind != ProductionControlKind::RECORD ||
			cell.input.request_serial != request.serial || cell.input.record_generation != request.operation.record_generation ||
			!cell.record_history.admitted || request.capture_inflight || request.continuation_inflight) { return false; }
	// CQMT transferred this exact original admitted mailbox to window waiting.
	// No current native window is supplied: original receive refuses before its
	// CallbackScope/body, retaining all prior effects/segment/ACK obligations.
	cell.scene_waiting = false;
	production_control_execute_locked(request, nullptr);
	return true; // RETURNED still requires exact owner consume, never replacement.
}

void Registry::production_control_execute_locked(ProductionRequest &request, RenderingDevice *rd, const ProductionCameraInvocationBorrow *p_native_invocation) {
	NativeDrainDiagnostic::Span drain_trace("control.execute", request.serial, request.control.serial, uint64_t(request.control.input.kind), request.maintenance_serial, uintptr_t(rd), 0);
	auto &cell = request.control;
	CRASH_COND(cell.state != ProductionControlState::QUEUED);
	cell.state = ProductionControlState::EXECUTING;
	cell.output = {};
	cell.output.request_serial = request.serial;
	cell.output.control_serial = cell.serial;
	switch (cell.input.kind) {
		case ProductionControlKind::RECORD:
			if (cell.input.output_cancel) {
				if (!service_budget_active(cell.input.budget)) { cell.output.result = result(ERR_BUSY); break; }
				cell.output.service_usage = { 1, 1, 1 };
			}
			if (request.native_stage_control == cell.serial) { cell.output.service_usage = { 1, 1, 1 }; }
			if (request.native_prepared_executor) { cell.output.segment = request.segment; } // Refusal preserves prior known prefix/ACK custody.
			if (request.native_batch_enabled && !cell.input.output_cancel && rd && p_native_invocation) {
				request.native_batch_control = cell.serial; request.native_batch_head = request.native_batch_tail = request.native_batch_read_cursor = 0;
				request.native_batch_read_started = false; request.native_batch_pending_ack = 0;
				for (;;) {
					const uint32_t before_action = request.next_action, before_graph = request.graph_count;
					request.native_batch_continue = false;
					const Error received = production_record_receive_locked(request, *rd, p_native_invocation);
					cell.output.result = result(received);
					if (received != OK || !request.native_batch_continue || request.next_action >= request.action_count || request.effects_pending || request.source_device_lost || request.cancelled ||
							(request.next_action == before_action && request.graph_count == before_graph)) { break; }
					if (production_native_stage_charge_locked(request, false) != OK) { break; }
					// Additional actual frame: original control remains EXECUTING;
					// no fabricated owner consume/application/ACK occurs here.
					++cell.output.service_usage.passes; ++cell.output.service_usage.receipts;
				}
			} else {
				cell.output.result = result(rd ? production_record_receive_locked(request, *rd, p_native_invocation) : ERR_UNAVAILABLE);
			}
			break;
		case ProductionControlKind::OBSERVE: {
			if (cell.input.predecessor_bind) {
				if (!rd || !rd->_thread_safe_.try_lock()) {
					cell.output.result = result(ERR_BUSY);
				} else {
					cell.output.result = result(production_output_predecessor_service_locked(request, *rd));
					rd->_thread_safe_.unlock();
				}
				break;
			}
			if (cell.input.consumed_operation) {
				if (!rd || !service_budget_active(cell.input.budget)) {
					cell.output.result = result(ERR_BUSY);
				} else if (!rd->_thread_safe_.try_lock()) {
					cell.output.result = result(ERR_BUSY);
				} else {
					RdUnlock unlock{ rd->_thread_safe_ };
					cell.output.result = result(output_consumed_service_locked(request, *rd));
				}
				break;
			}
			Error error = OK;
			if (rd && service_budget_active(cell.input.budget)) {
				if (!rd->_thread_safe_.try_lock()) {
					cell.output.result = result(ERR_BUSY);
					break;
				}
				RdUnlock unlock{ rd->_thread_safe_ };
				const Error camera_subscription = camera_ready_subscribe_request_locked(request, *rd, cell.input.budget, cell.output.service_usage);
				if (camera_subscription != OK) {
					cell.output.result = result(camera_subscription);
					break;
				}
			}
			if (request.native_count && service_budget_active(cell.input.budget)) {
				if (!rd || !rd->driver) {
					error = ERR_UNAVAILABLE;
				} else if (!rd->_thread_safe_.try_lock()) {
					error = ERR_BUSY;
				} else {
					RdUnlock unlock{ rd->_thread_safe_ };
					error = production_poll_native_locked(request, *rd->driver, cell.input.budget, device_records[request.attachment->device].live, cell.output.service_usage);
				}
			}
			if (rd && service_budget_active(cell.input.budget) && rd->_thread_safe_.try_lock()) {
				RdUnlock unlock{ rd->_thread_safe_ };
#ifdef TESTS_ENABLED
				// The private component fixture has directory/metadata work after
				// native sources settle. Invoke only its original OBSERVE service;
				// do not turn its presence into an empty native disposal cursor.
				if (test_production_before_maintenance && cell.input.budget.controls && cell.output.service_usage.passes < MIN(cell.input.budget.passes, 64u) && cell.output.service_usage.receipts < MIN(cell.input.budget.receipts, 128u) && cell.input.budget.deadline_micros && OS::get_singleton()->get_ticks_usec() < cell.input.budget.deadline_micros && !request.capture_inflight && !request.continuation_inflight && !request.source_device_lost && request.revision <= UINT64_MAX - uint64_t(request.capacity.graph_memberships) * 2 - uint64_t(request.capacity.native_memberships) * 2 - request.capacity.readbacks - request.capacity.resources - 8) {
					const uint64_t request_serial = request.serial, control_serial = request.control.serial;
					auto *const attachment = request.attachment;
					const uint64_t epoch = attachment->epoch;
					++request.capture_inflight;
					rd->_thread_safe_.unlock();
					unlock_mutex_for_call();
					const Error component = test_production_before_maintenance(request, *rd);
					// Match the production callback's blocking RD -> Registry order.
					rd->_thread_safe_.lock();
					relock_mutex_after_call();
					CRASH_COND(!request.capture_inflight);
					--request.capture_inflight;
					if (request.serial != request_serial || request.control.serial != control_serial || request.control.state != ProductionControlState::EXECUTING || request.attachment != attachment || attachment->held_device != rd || attachment->epoch != epoch || device_records[attachment->device].epoch != epoch || device_records[attachment->device].object != rd) {
						cell.output.result = result(ERR_INVALID_DATA);
						break;
					}
					if (component != OK && component != ERR_BUSY) {
						cell.output.result = result(component);
						break;
					}
					if (cell.output.service_usage.passes >= MIN(cell.input.budget.passes, 64u) || cell.output.service_usage.receipts >= MIN(cell.input.budget.receipts, 128u)) {
						error = ERR_BUSY;
					}
				}
#endif
				const Error output_service = production_output_owners_service_locked(request, *rd);
				if (output_service != OK && output_service != ERR_BUSY) {
					cell.output.result = result(output_service);
					break;
				}
				if (error == OK) {
					error = output_service;
				}
				bool progress = request.maintenance_serial && rd->native_disposal_active && request.maintenance_serial == rd->native_disposal_continuation.serial;
				for (uint32_t i = 0; i < request.resource_count; ++i) {
					const auto &slot = request.resources[i];
					MutexLock physical(slot.owned_physical.mutex);
					progress |= slot.retiring && slot.owned_physical.enqueued && !slot.owned_physical.free_event && !slot.returned;
				}
				for (uint32_t i = 0; i < request.graph_count; ++i) {
					progress |= request.graphs[i].pending;
				}
				for (uint32_t i = 0; i < request.readback_count; ++i) {
					progress |= request.readbacks[i].source.issued && request.readbacks[i].source.returned != request.readbacks[i].source.issued;
				}
				for (uint32_t g = request.use_count + request.binding_used; g < request.capacity.resource_uses; ++g) {
					const auto &group = request.uses[g];
					if (group.output_role != ProductionUseCell::OutputRole::DISPOSAL_GROUP || !group.disposal_group_release) {
						continue;
					}
					progress |= !group.disposal_group_dropped;
					for (uint32_t i = request.use_count + request.binding_used; i < request.capacity.resource_uses; ++i) {
						const auto &member = request.uses[i];
						if (member.output_role == ProductionUseCell::OutputRole::DISPOSAL_MEMBER && member.disposal_group_serial == group.disposal_group_serial) {
							const uint8_t state = member.disposal.state.load(std::memory_order_acquire);
							progress |= state == 1 || state == 2;
						}
					}
				}
				if (progress) {
					const Error maintenance = production_maintenance_locked(request, *rd, cell.input.budget, cell.output.service_usage);
					if (error == OK) {
						error = maintenance;
					}
				}
				const Error cleanup = production_sets_step_locked(request, *rd, cell.input.budget, cell.output.service_usage);
				if (error == OK) {
					error = cleanup;
				}
			}
			cell.output.result = result(error);
		} break;
		case ProductionControlKind::RASTER_CREATE:
			cell.output.result = result(rd ? production_raster_prepare_execute_locked(request, *rd) : production_raster_prepare_finish_locked(request, ERR_UNAVAILABLE));
			break;
		case ProductionControlKind::STORAGE_CREATE:
		case ProductionControlKind::SET_CREATE:
			cell.output.result = result(rd ? production_create_execute_locked(request, *rd, cell.input.kind, cell.input.resource_slot) : ERR_UNAVAILABLE);
			break;
		case ProductionControlKind::CANCEL:
			cell.output.result = production_cancel_locked(request);
			if ((request.compute_list_open || request.draw_list_open) && rd && rd->_thread_safe_.try_lock()) {
				RdUnlock unlock{ rd->_thread_safe_ };
				const Error closed = production_close_lists_locked(request, *rd);
				if (closed != OK) {
					cell.output.result = result(closed);
				}
			}
			break;
		case ProductionControlKind::RELEASE: {
			cell.output.service_usage.controls = cell.input.budget.controls ? 1 : 0; // One authorized dispatch; zero-budget internal metadata keeps its original path.
			Error maintenance = OK;
			if (rd && request.maintenance_serial) {
				if (!service_budget_active(cell.input.budget) || !rd->_thread_safe_.try_lock()) {
					maintenance = ERR_BUSY;
				} else {
					RdUnlock unlock{ rd->_thread_safe_ };
					if (rd->native_disposal_active && request.maintenance_serial == rd->native_disposal_continuation.serial && rd->native_disposal_continuation.production_owner == &request) {
						maintenance = production_maintenance_locked(request, *rd, cell.input.budget, cell.output.service_usage);
					}
				}
			}
			if (maintenance != OK) {
				cell.output.result = result(maintenance, 16);
				break;
			}
			// The original immutable authorization already paid for this dispatch
			// and any maintenance above. The release leaf resets its usage output,
			// so offer only the residual slice and merge a separate usage result.
			auto release_budget = cell.input.budget;
			auto &used = cell.output.service_usage;
			if (used.controls > 1 || used.controls > release_budget.controls || used.passes > MIN(release_budget.passes, 64u) || used.receipts > MIN(release_budget.receipts, 128u)) {
				cell.output.result = result(ERR_INVALID_DATA, 19);
				break;
			}
			release_budget.passes = MIN(release_budget.passes, 64u) - used.passes;
			release_budget.receipts = MIN(release_budget.receipts, 128u) - used.receipts;
			ProductionServiceUsage release_usage = {};
			cell.output.result = production_release_locked(request, rd, false, &release_budget, &release_usage);
			if (release_usage.controls || release_usage.passes > release_budget.passes || release_usage.receipts > release_budget.receipts) {
				cell.output.result = result(ERR_INVALID_DATA, 19);
				break;
			}
			used.passes += release_usage.passes;
			used.receipts += release_usage.receipts;
		} break;
	}
	if (cell.input.kind == ProductionControlKind::RELEASE && cell.output.result.native_error == OK && production_release_owner_tail_locked(request, rd, false)) {
		request.release_ready = { request.serial, cell.serial, request.revision, request.operation.record_generation, request.attachment->epoch, request.next_output_pin_serial, request.native_count, request.graph_count };
	}
	production_report_locked(request, cell.output.report);
	drain_trace.result(cell.output.result.native_error, "control.returned");
	cell.state = ProductionControlState::RETURNED;
	request.attachment->metadata_pending = request.attachment->metadata_receiver != nullptr;
	// No recursive schedule, driver wait, native completion, or source ACK here.
}

void Registry::production_control_dispatch(uint64_t registry, uint64_t attachment, uint64_t request, uint64_t serial) {
	NativeDrainDiagnostic::Span drain_trace("control.dispatch", registry, attachment, request, serial);
	auto *server = Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton());
	if (!server || Thread::get_caller_id() != server->server_thread) {
		drain_trace.result(ERR_UNAUTHORIZED, "wrong_thread_or_no_server");
		return;
	}
	Lock lock(*this);
	if (registry != instance || !serial) {
		drain_trace.result(ERR_INVALID_PARAMETER, "foreign_registry_or_zero_serial");
		return;
	}
	for (auto *a : production_attachments) {
		if (!a || a->serial != attachment) {
			continue;
		}
		for (uint32_t i = 0; i < a->quote.capacity.requests; ++i) {
			auto &r = a->requests[i];
			auto &cell = r.control;
			if (!request || r.serial != request || cell.serial != serial || cell.state != ProductionControlState::QUEUED) {
				continue;
			}
			auto &owner = records[a->library];
			auto &device = device_records[a->device];
			// This validates the original enrolled object and retained queue admission.
			// It does not create a qualification TLS permission or a numeric grant.
			if (cell.input.release_owner_only || r.attachment != a || cell.input.request_serial != r.serial ||
					cell.input.record_generation != r.operation.record_generation ||
					!owner.occupied || owner.snapshot.library_generation != a->library_generation ||
					owner.producer_registration_serial != a->producer_serial ||
					!owner.producer_attachment_identity || !a->holder ||
					a->holder->handle_serial != owner.producer_code_holder ||
					a->holder->generation != a->library_generation ||
					a->holder->terminal_action != a->terminal_action || !a->holder->production_children ||
					device.epoch != a->epoch || device.execution_thread != Thread::get_caller_id() ||
					!a->held_device || device.object != a->held_device ||
					a->held_device->production_entity_registry.load(std::memory_order_acquire) != this) {
				cell.output = {};
				cell.output.request_serial = r.serial;
				cell.output.control_serial = cell.serial;
				if (cell.input.kind == ProductionControlKind::RASTER_CREATE) {
					production_raster_prepare_finish_locked(r, ERR_INVALID_DATA);
				}
				cell.output.result = result(ERR_INVALID_DATA, 16);
				cell.state = ProductionControlState::RETURNED;
				drain_trace.result(ERR_INVALID_DATA, "original_identity_rejected");
				return;
			}
			// A loss/sealed preflight does not erase already accepted cleanup. Pending
			// native effects are still checked by the real release helper and retained.
			drain_trace.result(OK, "entered_execute");
			if (r.native_prepared_executor && cell.input.kind == ProductionControlKind::RECORD && !cell.input.output_cancel) {
				bool closed_anchor = false;
				for (uint32_t j = 0; j < r.action_count; ++j) {
					const auto &action = r.actions[j];
					closed_anchor |= action.output_source_prepared && action.camera_ready_subscriber.is_closed();
				}
				if (a->closing || a->camera_ready_closed || a->scene_closed || !device.live || r.cancelled || r.release_started || r.source_device_lost || closed_anchor) {
					cell.scene_waiting = false;
					production_control_execute_locked(r, nullptr); // Original CQMT admission rejection, no native frame/body.
					return;
				}
				cell.scene_waiting = true;
				return; // Genuine camera invocation owns native execution; never CQMT.
			}
			if (a->scene_registration && cell.input.kind == ProductionControlKind::RECORD && !cell.input.output_cancel && !a->scene_occurrence && !a->scene_closed) {
				cell.scene_waiting = true;
				return; // Original queued mailbox waits for actual SceneBeforeParticles.
			}
			cell.scene_waiting = false;
			production_control_execute_locked(r, a->held_device);
			return;
		}
		drain_trace.result(ERR_DOES_NOT_EXIST, "no_matching_request");
		return;
	}
	drain_trace.result(ERR_DOES_NOT_EXIST, "no_matching_attachment");
}

Error Registry::production_control_consume(GDExtensionProductionRequestRefV1 ref, uint64_t serial, ProductionControlOutput *out) {
	if (!aligned(out)) {
		return ERR_INVALID_PARAMETER;
	}
	Lock lock(*this);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_request_locked(ref, false, r, rd);
	if (error != OK) {
		return error;
	}
	auto &cell = r->control;
	if (!serial || cell.serial != serial || cell.state == ProductionControlState::OWNER_CONSUMED) {
		return ERR_DOES_NOT_EXIST;
	}
	if (cell.state != ProductionControlState::RETURNED) {
		return ERR_BUSY;
	}
	auto retained = cell.output;
	if (cell.input.kind == ProductionControlKind::RELEASE && NativeDrainDiagnostic::enabled()) {
		NativeDrainDiagnostic::Span trace(cell.input.release_owner_only ? "release.consume.owner.stage" : "release.consume.rt", r->serial, serial, retained.service_usage.controls, retained.service_usage.passes, retained.service_usage.receipts, r->native_count);
		trace.result(retained.result.native_error, "immutable_dispatched_result");
	}
	if (cell.input.kind == ProductionControlKind::RELEASE && retained.result.native_error == OK) {
		// The RT has returned. Reclaim is now owned by the original owner; it must
		// succeed before the result is consumed or the request slot becomes reusable.
		auto owner_budget = cell.input.budget;
		const auto &rt = retained.service_usage;
		if (rt.controls > 1 || rt.controls > owner_budget.controls || rt.receipts > MIN(owner_budget.receipts, 128u) || rt.passes > MIN(owner_budget.passes, 64u)) {
			retained.owner_return = result(ERR_INVALID_DATA, 19);
			cell.state = ProductionControlState::OWNER_CONSUMED;
			*out = retained;
			return OK;
		}
		// One original authorization spans RT and its owner continuation. No
		// refill or fresh deadline is permitted after the asynchronous return.
		owner_budget.receipts = MIN(owner_budget.receipts, 128u) - rt.receipts;
		owner_budget.passes = MIN(owner_budget.passes, 64u) - rt.passes;
		if (NativeDrainDiagnostic::enabled()) {
			NativeDrainDiagnostic::Span trace("release.consume.owner.before", r->serial, serial, owner_budget.deadline_micros, OS::get_singleton()->get_ticks_usec(), r->release_cursor, r->native_count);
			trace.result(before_service_deadline(owner_budget) ? OK : ERR_BUSY, "original_deadline_predicate");
		}
		if (!before_service_deadline(owner_budget)) {
			owner_budget.controls = owner_budget.receipts = owner_budget.passes = 0;
		}
		const auto released = production_release_locked(*r, rd, true, &owner_budget, &retained.owner_usage);
		retained.owner_return = released;
		if (NativeDrainDiagnostic::enabled()) {
			NativeDrainDiagnostic::Span trace("release.consume.owner.after", ref.handle_serial, serial, retained.owner_usage.controls, retained.owner_usage.passes, retained.owner_usage.receipts, r->serial == ref.handle_serial ? r->release_cursor : 0);
			trace.result(retained.owner_return.native_error, "actual_owner_return");
		}
		if (released.native_error != OK) {
			// This owner's turn has consumed its bounded credit. Keep all unreturned
			// cells/edges and accept a fresh exact control turn for a later retry.
			cell.state = ProductionControlState::OWNER_CONSUMED;
		}
	} else {
		cell.state = ProductionControlState::OWNER_CONSUMED;
	}
	if (cell.input.kind == ProductionControlKind::RECORD && r->native_prepared_executor) {
		for (uint32_t i = 0; i < r->action_count; ++i) {
			auto &action = r->actions[i];
			if (action.native_prepared_result_valid && action.native_prepared_result.ref.control_serial == serial) {
				action.native_prepared_record_consumed = true;
			}
		}
	}
	*out = retained;
	return OK;
}
} // namespace GDExtensionDeviceExit
