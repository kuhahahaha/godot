/**************************************************************************/
/*  gdextension_production_resources.cpp                                  */
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
#include "core/extension/gdextension_qualification_internal.h"

namespace GDExtensionDeviceExit {
namespace {
template <class T>
bool aligned(const T *p_value) {
	return p_value && uintptr_t(p_value) % alignof(T) == 0;
}
bool abi(const GDExtensionDeviceExitAbi &p_abi) {
	return p_abi.abi_version == 1 && p_abi.struct_size == sizeof(p_abi) && !p_abi.reserved;
}
GDExtensionDeviceSessionResultV1 result(Error p_error) {
	return { int32_t(p_error), p_error == OK ? 0u : 16u, 0, 0 };
}
struct RdUnlock {
	Mutex &mutex;
	~RdUnlock() { mutex.unlock(); }
};
} // namespace

GDExtensionDeviceSessionResultV1 Registry::production_import_storage(GDExtensionProductionRequestRefV1 p_ref, uint32_t p_slot, const GDExtensionStorageBackingV1 *p_backing) {
	if (!aligned(p_backing) || !abi(p_backing->abi)) {
		return result(ERR_INVALID_PARAMETER);
	}
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_request_locked(p_ref, true, request, rd);
	if (error != OK) {
		return result(error);
	}
	ProductionResourceSlot *slot = nullptr;
	error = production_resource_slot_locked(*request, p_slot, slot);
	if (error != OK) {
		return result(error);
	}
	const auto &decl = slot->declaration;
	if (decl.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || decl.ownership != GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1 || p_backing->backing.registry_instance != instance || !p_backing->backing.handle_serial || request->frozen) {
		return result(ERR_INVALID_DATA);
	}
	if (!rd->_thread_safe_.try_lock()) {
		return result(ERR_BUSY);
	}
	RdUnlock unlock{ rd->_thread_safe_ };
	for (auto *bridge : qualification_bridges) {
		if (!bridge || bridge->library != request->attachment->library || bridge->device != request->attachment->device || bridge->epoch != request->attachment->epoch) {
			continue;
		}
		for (auto *child : bridge->storage) {
			if (!child || child->serial != p_backing->backing.handle_serial) {
				continue;
			}
			const auto &stored = child->backing;
			auto *buffer = rd->storage_buffer_owner.get_or_null(RID::from_uint64(stored.rid));
			if (child->creating || child->retiring || child->acknowledged || child->maintenance_error != OK || !abi(stored.abi) || stored.state != 1 || stored.native_error != OK || stored.backing.registry_instance != instance || stored.backing.handle_serial != child->serial || stored.rid != p_backing->rid || stored.backing_serial != p_backing->backing_serial || stored.byte_size != p_backing->byte_size || stored.usage_bits != p_backing->usage_bits || stored.native_error != p_backing->native_error || stored.state != p_backing->state || child->origin.allocation_slot != decl.original_slot || child->origin.part != decl.original_part || child->origin.allocation_generation != decl.original_generation || child->origin.bytes != decl.physical_bytes || stored.byte_size != decl.physical_bytes || (decl.required_usage & ~stored.usage_bits) || !buffer || !buffer->driver_id || buffer->qualification_return != &child->physical || buffer->backing_serial != stored.backing_serial || buffer->size != stored.byte_size || child->physical.bridge != bridge->serial || child->physical.child != child->serial || child->physical.rid != stored.rid || child->physical.serial != stored.backing_serial) {
				return result(ERR_INVALID_DATA);
			}
			if (slot->imported && (slot->primary.get_id() != stored.rid || slot->backing_serial != stored.backing_serial || slot->use_count != 1)) {
				return result(ERR_INVALID_DATA);
			}
			error = slot->uses[0].attach(child->physical, *request, request->serial);
			if (error != OK) {
				return result(error);
			}
			slot->primary = RID::from_uint64(stored.rid);
			slot->backing_serial = stored.backing_serial;
			slot->use_count = 1;
			slot->imported = true;
			return result(OK);
		}
	}
	return result(ERR_DOES_NOT_EXIST);
}

GDExtensionDeviceSessionResultV1 Registry::production_import_unit(GDExtensionProductionRequestRefV1 p_ref, uint32_t p_slot, const GDExtensionNativeUnitBackingV1 *p_backing) {
	if (!aligned(p_backing) || !abi(p_backing->abi)) {
		return result(ERR_INVALID_PARAMETER);
	}
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_request_locked(p_ref, true, request, rd);
	if (error != OK) {
		return result(error);
	}
	ProductionResourceSlot *slot = nullptr;
	error = production_resource_slot_locked(*request, p_slot, slot);
	if (error != OK) {
		return result(error);
	}
	const auto &decl = slot->declaration;
	if (decl.resource_kind != GDEXTENSION_PRODUCTION_UNIT_V1 || decl.ownership != GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1 || p_backing->unit.registry_instance != instance || !p_backing->unit.handle_serial || request->frozen || decl.physical_bytes || decl.required_usage) {
		return result(ERR_INVALID_DATA);
	}
	if (!rd->_thread_safe_.try_lock()) {
		return result(ERR_BUSY);
	}
	RdUnlock unlock{ rd->_thread_safe_ };
	for (auto *bridge : qualification_bridges) {
		if (!bridge || bridge->library != request->attachment->library || bridge->device != request->attachment->device || bridge->epoch != request->attachment->epoch) {
			continue;
		}
		for (auto *child : bridge->units) {
			if (!child || child->serial != p_backing->unit.handle_serial) {
				continue;
			}
			const auto &stored = child->backing;
			auto *shader = rd->shader_owner.get_or_null(RID::from_uint64(stored.shader_rid));
			auto *pipeline = rd->compute_pipeline_owner.get_or_null(RID::from_uint64(stored.pipeline_rid));
			if (child->creating || child->retiring || child->acknowledged || child->maintenance_error != OK || !abi(stored.abi) || stored.unit.registry_instance != instance || stored.unit.handle_serial != child->serial || stored.known_mask != 3 || stored.possible_mask || stored.native_error != OK || stored.state != 1 || stored.shader_rid != p_backing->shader_rid || stored.pipeline_rid != p_backing->pipeline_rid || stored.shader_serial != p_backing->shader_serial || stored.pipeline_serial != p_backing->pipeline_serial || stored.known_mask != p_backing->known_mask || stored.possible_mask != p_backing->possible_mask || stored.state != p_backing->state || stored.effective_flags != p_backing->effective_flags || stored.native_error != p_backing->native_error || stored.phase != p_backing->phase || stored.event_seq != p_backing->event_seq || child->origin.library_generation != request->attachment->library_generation || child->origin.record_slot != decl.original_slot || child->origin.physical_unit != decl.original_part || child->origin.preparation_generation != decl.original_generation || !shader || !shader->driver_id || !pipeline || !pipeline->driver_id || pipeline->shader.get_id() != stored.shader_rid || pipeline->shader_driver_id != shader->driver_id || shader->qualification_return != &child->shader || pipeline->qualification_return != &child->pipeline || child->shader.bridge != bridge->serial || child->pipeline.bridge != bridge->serial || child->shader.child != child->serial || child->pipeline.child != child->serial || child->shader.rid != stored.shader_rid || child->pipeline.rid != stored.pipeline_rid || child->shader.serial != stored.shader_serial || child->pipeline.serial != stored.pipeline_serial) {
				return result(ERR_INVALID_DATA);
			}
			if (slot->imported && (slot->primary.get_id() != stored.pipeline_rid || slot->secondary.get_id() != stored.shader_rid || slot->use_count != 2)) {
				return result(ERR_INVALID_DATA);
			}
			// Check both original physical children before installing either edge.
			if (child->shader.use_closed.load() || child->pipeline.use_closed.load() || child->shader.enqueued || child->pipeline.enqueued || child->shader.reserved_event || child->pipeline.reserved_event || child->shader.free_event || child->pipeline.free_event) {
				return result(ERR_UNAVAILABLE);
			}
			error = slot->uses[0].attach(child->shader, *request, request->serial);
			if (error != OK) {
				return result(error);
			}
			error = slot->uses[1].attach(child->pipeline, *request, request->serial);
			if (error != OK) {
				// The first edge was either preexisting or this no-effect prefix.
				if (!slot->imported) {
					const Error rollback = slot->uses[0].detach(*request, request->serial);
					if (rollback != OK) {
						return result(rollback);
					}
				}
				return result(error);
			}
			slot->primary = RID::from_uint64(stored.pipeline_rid);
			slot->secondary = RID::from_uint64(stored.shader_rid);
			slot->backing_serial = stored.pipeline_serial;
			slot->use_count = 2;
			slot->imported = true;
			return result(OK);
		}
	}
	return result(ERR_DOES_NOT_EXIST);
}

Error Registry::production_unpin_resources_locked(ProductionRequest &p_request) {
	for (uint32_t i = 0; i < p_request.capacity.resource_uses; ++i) {
		if (p_request.uses[i].output_consumer) {
			return ERR_BUSY;
		}
	}
	if (p_request.effects_pending) {
		return ERR_BUSY;
	}
	if (p_request.use_count > p_request.capacity.resource_uses || p_request.binding_used > p_request.capacity.resource_uses - p_request.use_count || (p_request.binding_used && !p_request.uses)) {
		return ERR_INVALID_DATA;
	}
	const uint32_t binding_end = p_request.use_count + p_request.binding_used;
	for (uint32_t i = p_request.use_count; i < binding_end; ++i) {
		if (p_request.uses[i].request != &p_request || p_request.uses[i].dependency.physical) {
			return ERR_INVALID_DATA;
		}
	}
	// Validate the whole reverse relation before dropping any retained child.
	for (uint32_t i = 0; i < p_request.resource_count; i++) {
		auto &slot = p_request.resources[i];
		if (slot.use_count > 2) {
			return ERR_INVALID_DATA;
		}
		if (slot.output_alias_prepared && slot.output_witness.cell.is_valid() && slot.output_witness.cell->state != NativeDisposalWitness::State::RETURNED) {
			return ERR_BUSY;
		}
		for (uint32_t j = 0; j < slot.use_count; j++) {
			const auto &use = slot.uses[j];
			if (use.request != &p_request || use.request_serial != p_request.serial || !use.same_physical() || !use.linked()) {
				return ERR_INVALID_DATA;
			}
		}
	}
	for (uint32_t i = 0; i < p_request.resource_count; i++) {
		auto &slot = p_request.resources[i];
		while (slot.use_count) {
			Error error = slot.uses[slot.use_count - 1].detach(p_request, p_request.serial);
			if (error != OK) {
				return error;
			}
			slot.use_count--;
		}
		if (slot.output_alias_prepared) {
			// All original root pins have returned above. Release this observer
			// only after its sole disposal owner observed actual driver return.
			slot.output_witness = NativeDisposalWitness();
		}
		slot.imported = false;
		slot.first_binding = UINT32_MAX;
		slot.binding_count = 0;
		slot.imported_unit_slot = UINT32_MAX;
		slot.shader_set = UINT32_MAX;
	}
	for (uint32_t i = p_request.use_count; i < binding_end; ++i) {
		p_request.uses[i].binding = {};
	}
	p_request.binding_used = 0;
	return OK;
}
} // namespace GDExtensionDeviceExit
