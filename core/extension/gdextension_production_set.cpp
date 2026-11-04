/**************************************************************************/
/*  gdextension_production_set.cpp                                        */
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

#pragma once

#include "core/extension/gdextension_production_request.h"
#include "core/extension/gdextension_qualification_internal.h"

namespace GDExtensionDeviceExit {
namespace {
template <class T>
bool aligned(const T *p_value) {
	return p_value && uintptr_t(p_value) % alignof(T) == 0;
}
bool abi(const GDExtensionDeviceExitAbi &p_value) {
	return p_value.abi_version == 1 && p_value.struct_size == sizeof(p_value) && !p_value.reserved;
}
bool source_equal(const GDExtensionQualificationSourceV1 &a, const GDExtensionQualificationSourceV1 &b) {
	return a.session_serial == b.session_serial && a.generation == b.generation && memcmp(a.digest, b.digest, sizeof(a.digest)) == 0;
}
bool binding_equal(const GDExtensionProductionSetBindingV1 &a, const GDExtensionProductionSetBindingV1 &b) {
	return a.abi.abi_version == b.abi.abi_version && a.abi.struct_size == b.abi.struct_size && a.abi.reserved == b.abi.reserved && a.set == b.set && a.binding == b.binding && a.array_element == b.array_element && a.resource_slot == b.resource_slot && a.offset == b.offset && a.bytes == b.bytes;
}
GDExtensionDeviceSessionResultV1 result(Error p_error) {
	return { int32_t(p_error), p_error == OK ? 0u : 16u, 0, 0 };
}
struct RdUnlock {
	Mutex &mutex;
	~RdUnlock() { mutex.unlock(); }
};
} // namespace

GDExtensionDeviceSessionResultV1 Registry::production_import_set(GDExtensionProductionRequestRefV1 p_ref, uint32_t p_slot, const GDExtensionQualificationResourceBackingV1 *p_backing, uint32_t p_unit_slot, uint32_t p_shader_set, const GDExtensionProductionSetBindingV1 *p_bindings, uint32_t p_count) {
	if (!aligned(p_backing) || !abi(p_backing->abi) || (p_count && !aligned(p_bindings))) {
		return result(ERR_INVALID_PARAMETER);
	}
	MutexLock lock(mutex);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = production_request_locked(p_ref, true, request, rd);
	if (error != OK) {
		return result(error);
	}
	ProductionResourceSlot *slot = nullptr;
	ProductionResourceSlot *unit_slot = nullptr;
	if ((error = production_resource_slot_locked(*request, p_slot, slot)) != OK || (error = production_resource_slot_locked(*request, p_unit_slot, unit_slot)) != OK) {
		return result(error);
	}
	if (request->frozen || slot->declaration.resource_kind != GDEXTENSION_PRODUCTION_SET_V1 || slot->declaration.ownership != GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1 || slot->declaration.physical_bytes || slot->declaration.required_usage || slot->declaration.original_slot != UINT32_MAX || !unit_slot->imported || unit_slot->declaration.resource_kind != GDEXTENSION_PRODUCTION_UNIT_V1 || unit_slot->use_count != 2 || p_backing->resource.registry_instance != instance || !p_backing->resource.handle_serial || p_count > request->capacity.resource_uses) {
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
		QualificationBridge::UniformSet *child = nullptr;
		for (auto *candidate : bridge->sets) {
			if (candidate && candidate->serial == p_backing->resource.handle_serial) {
				child = candidate;
				break;
			}
		}
		if (!child) {
			continue;
		}
		const auto &stored = child->backing;
		const auto &origin = child->origin;
		auto *set = rd->uniform_set_owner.get_or_null(RID::from_uint64(child->rid));
		if (child->kind != 3 || child->creating || child->retiring || child->acknowledged || child->maintenance_error != OK || !abi(stored.abi) || stored.kind != 3 || stored.state != 1 || stored.resource.registry_instance != instance || stored.resource.handle_serial != child->serial || stored.rid != child->rid || stored.object_serial != child->object_serial || stored.byte_size || p_backing->kind != stored.kind || p_backing->state != stored.state || p_backing->rid != stored.rid || p_backing->object_serial != stored.object_serial || p_backing->byte_size != stored.byte_size || !source_equal(stored.source, child->source) || !source_equal(p_backing->source, child->source) || !abi(origin.abi) || !source_equal(origin.source, child->source) || origin.library_generation != request->attachment->library_generation || origin.binding_count != p_count || child->bindings.size() != p_count || origin.shader_set != p_shader_set || slot->declaration.original_part != origin.shader_set || slot->declaration.original_generation != origin.source.generation || origin.shader_rid != unit_slot->secondary.get_id() || !set || !set->driver_id || set->shader_id != unit_slot->secondary || set->shader_set != p_shader_set || set->qualification_return != &child->physical || !set->shared_textures_to_update.is_empty() || !set->pending_clear_textures.is_empty() || child->physical.bridge != bridge->serial || child->physical.child != child->serial || child->physical.rid != child->rid || child->physical.serial != child->object_serial) {
			return result(ERR_INVALID_DATA);
		}
		for (uint32_t i = 0; i < unit_slot->use_count; ++i) {
			const auto &use = unit_slot->uses[i];
			if (use.request != request || use.request_serial != request->serial || !use.same_physical()) {
				return result(ERR_INVALID_DATA);
			}
		}
		QualificationBridge::NativeUnit *unit = nullptr;
		for (auto *candidate : bridge->units) {
			if (candidate && origin.unit.registry_instance == instance && candidate->serial == origin.unit.handle_serial && &candidate->shader == unit_slot->uses[0].physical && &candidate->pipeline == unit_slot->uses[1].physical) {
				unit = candidate;
				break;
			}
		}
		auto *shader = rd->shader_owner.get_or_null(unit_slot->secondary);
		auto *pipeline = rd->compute_pipeline_owner.get_or_null(unit_slot->primary);
		if (!unit || unit->creating || unit->retiring || unit->acknowledged || unit->maintenance_error != OK || unit->backing.known_mask != 3 || unit->backing.possible_mask || unit->backing.state != 1 || unit->backing.native_error || origin.shader_serial != unit->backing.shader_serial || unit->backing.shader_rid != origin.shader_rid || unit->backing.pipeline_rid != unit_slot->primary.get_id() || !unit_slot->uses[0].same_physical() || !unit_slot->uses[1].same_physical() || !shader || !shader->driver_id || shader->qualification_return != &unit->shader || !pipeline || !pipeline->driver_id || pipeline->shader != unit_slot->secondary || pipeline->shader_driver_id != shader->driver_id || pipeline->qualification_return != &unit->pipeline || p_shader_set >= uint32_t(shader->uniform_sets.size()) || shader->uniform_sets[p_shader_set].size() != p_count || p_shader_set >= uint32_t(pipeline->set_formats.size()) || pipeline->set_formats[p_shader_set] != set->format) {
			return result(ERR_INVALID_DATA);
		}
		if (unit->pipeline.use_closed.load() || unit->pipeline.enqueued || unit->pipeline.reserved_event || unit->pipeline.free_event) {
			return result(ERR_UNAVAILABLE);
		}
		// Resolve every declared binding through an already imported original
		// slot. The existing slot edges retain the complete dependency closure;
		// no extra dependency node or unquoted allocation is introduced here.
		for (uint32_t i = 0; i < p_count; ++i) {
			const auto &binding = p_bindings[i];
			const GDExtensionQualificationUniformBindingV1 *original = nullptr;
			for (const auto &candidate : child->bindings) {
				if (candidate.binding != binding.binding) {
					continue;
				}
				if (original) {
					return result(ERR_INVALID_DATA);
				}
				original = &candidate;
			}
			if (!original) {
				return result(ERR_INVALID_DATA);
			}
			const auto &actual = *original;
			if (!abi(binding.abi) || binding.set != p_shader_set || binding.array_element || binding.resource_slot >= request->resource_count || (i && p_bindings[i - 1].binding >= binding.binding)) {
				return result(ERR_INVALID_DATA);
			}
			auto &resource = request->resources[binding.resource_slot];
			if (resource.request != request || resource.request_serial != request->serial || !resource.imported || resource.declaration.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || resource.use_count != 1 || resource.uses[0].request != request || resource.uses[0].request_serial != request->serial || !resource.uses[0].same_physical() || resource.uses[0].physical->bridge != bridge->serial || actual.type != 1 || actual.binding != binding.binding || actual.resource.registry_instance != instance || actual.resource.handle_serial != resource.uses[0].physical->child || actual.rid != resource.primary.get_id() || actual.object_serial != resource.backing_serial || actual.secondary.registry_instance || actual.secondary.handle_serial || actual.secondary_rid || actual.secondary_serial || binding.offset || binding.bytes != resource.declaration.physical_bytes) {
				return result(ERR_INVALID_DATA);
			}
			auto *buffer = rd->storage_buffer_owner.get_or_null(resource.primary);
			if (!buffer || !buffer->driver_id || buffer->qualification_return != resource.uses[0].physical || buffer->backing_serial != resource.backing_serial || buffer->size != binding.bytes || !set->qualification_dependencies.has(resource.uses[0].physical)) {
				return result(ERR_INVALID_DATA);
			}
			bool reflected = false;
			for (const auto &uniform : shader->uniform_sets[p_shader_set]) {
				if (uniform.binding == binding.binding && uniform.type == RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER) {
					reflected = true;
					break;
				}
			}
			if (!reflected) {
				return result(ERR_INVALID_DATA);
			}
		}
		// Require the actual native set's full original physical closure, not
		// just the subset named by a matching RID. Repeated buffer bindings share
		// one original dependency, as in the original set creator.
		if (!set->qualification_dependencies.has(&unit->shader)) {
			return result(ERR_INVALID_DATA);
		}
		for (uint32_t i = 0; i < set->qualification_dependencies.size(); ++i) {
			auto *dependency = set->qualification_dependencies[i];
			if (!dependency || set->qualification_dependencies.find(dependency) != int32_t(i) || dependency->use_closed.load() || dependency->enqueued || dependency->reserved_event || dependency->free_event) {
				return result(ERR_UNAVAILABLE);
			}
			bool found = dependency == &unit->shader;
			for (uint32_t j = 0; j < p_count && !found; ++j) {
				found = request->resources[p_bindings[j].resource_slot].uses[0].physical == dependency;
			}
			if (!found) {
				return result(ERR_INVALID_DATA);
			}
		}
		if (slot->imported) {
			if (slot->primary.get_id() != child->rid || slot->backing_serial != child->object_serial || slot->use_count != 1 || slot->imported_unit_slot != p_unit_slot || slot->shader_set != p_shader_set || slot->binding_count != p_count || slot->first_binding < request->use_count || slot->first_binding > request->capacity.resource_uses || p_count > request->capacity.resource_uses - slot->first_binding || slot->uses[0].physical != &child->physical || !slot->uses[0].same_physical()) {
				return result(ERR_INVALID_DATA);
			}
			for (uint32_t i = 0; i < p_count; ++i) {
				if (!binding_equal(request->uses[slot->first_binding + i].binding, p_bindings[i])) {
					return result(ERR_INVALID_DATA);
				}
			}
		} else if (request->use_count > request->capacity.resource_uses || request->binding_used > request->capacity.resource_uses - request->use_count || p_count > request->capacity.resource_uses - request->use_count - request->binding_used) {
			return result(ERR_PARAMETER_RANGE_ERROR);
		}
		const uint32_t first = slot->imported ? slot->first_binding : request->use_count + request->binding_used;
		for (uint32_t i = 0; i < p_count; ++i) {
			if (request->uses[first + i].request != request || request->uses[first + i].dependency.physical) {
				return result(ERR_INVALID_DATA);
			}
		}
		error = slot->uses[0].attach(child->physical, *request, request->serial);
		if (error != OK) {
			return result(error);
		}
		if (!slot->imported) {
			for (uint32_t i = 0; i < p_count; ++i) {
				request->uses[first + i].binding = p_bindings[i];
			}
			request->binding_used += p_count;
		}
		slot->primary = RID::from_uint64(child->rid);
		slot->backing_serial = child->object_serial;
		slot->imported_unit_slot = p_unit_slot;
		slot->shader_set = p_shader_set;
		slot->first_binding = first;
		slot->binding_count = p_count;
		slot->use_count = 1;
		slot->imported = true;
		return result(OK);
	}
	return result(ERR_DOES_NOT_EXIST);
}
} // namespace GDExtensionDeviceExit
