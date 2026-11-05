/**************************************************************************/
/*  particles_storage_retire.cpp                                          */
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

#include "core/os/os.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_owner.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"

namespace RendererRD {
bool ParticlesStorage::_production_output_owner_matches(RID particle_rid, uint64_t generation, uint32_t slot, uint64_t lease_serial, const GDExtensionDeviceExit::ProductionOutputPin &pin, RenderingDevice &device, GDExtensionProductionRequestRefV1 request) {
	ParticlesStorage *storage = get_singleton();
	if (!storage || !generation || !lease_serial || pin.is_empty()) {
		return false;
	}
	const Particles *particles = storage->particles_owner.get_or_null(particle_rid);
	if (!particles || !particles->external) {
		return false;
	}
	const ExternalOutputOwner &owner = *particles->external;
	if (owner.generation != generation || owner.device != &device || slot >= owner.slot_count || owner.state == ExternalOutputOwner::GenerationState::RELEASED) {
		return false;
	}
	const auto &actual = owner.slots[slot];
	return request.registry_instance && request.handle_serial && actual.original_request.registry_instance == request.registry_instance && actual.original_request.handle_serial == request.handle_serial && actual.lease_serial == lease_serial && &actual.original_pin == &pin && actual.state != ExternalOutputOwner::LeaseState::EMPTY && actual.state != ExternalOutputOwner::LeaseState::RELEASED;
}

Error ParticlesStorage::_production_output_service(RID p_particles, uint64_t p_generation, uint32_t p_slot,
		uint64_t p_lease_serial, GDExtensionDeviceExit::ProductionOutputPin &p_pin,
		const GDExtensionProductionServiceBudgetV1 &p_budget, GDExtensionDeviceExit::ProductionServiceUsage &r_usage) {
	ParticlesStorage *storage = get_singleton();
	if (!storage || !p_generation || !p_lease_serial || p_pin.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	Particles *particles = storage->particles_owner.get_or_null(p_particles);
	if (!particles || !particles->external) {
		return ERR_DOES_NOT_EXIST;
	}
	auto &owner = *particles->external;
	if (owner.generation != p_generation || !owner.device || p_slot >= owner.slot_count) {
		return ERR_INVALID_PARAMETER;
	}
	auto &slot = owner.slots[p_slot];
	if (&p_pin != &slot.original_pin || slot.lease_serial != p_lease_serial || !slot.original_registry ||
			!slot.original_registry->production_output_pin_matches(p_pin, slot.original_request, *owner.device)) {
		return ERR_UNAUTHORIZED;
	}
	GDExtensionDeviceExit::ProductionDisposalObservation observation;
	// Exact original control/budget/usage identity and original complete-source
	// facts are authenticated before logical history or physical metadata changes.
	Error error = owner.device->_production_output_retire_service(p_pin, 0, p_budget, r_usage, observation);
	if (error != OK) {
		return error;
	}
	if (slot.state != ExternalOutputOwner::LeaseState::RETIRING || !slot.publication_closed ||
			!slot.pass_uses_closed || owner.slot_has_custody(p_slot)) {
		return ERR_BUSY;
	}
	auto spend = [&]() {
		if (r_usage.passes >= MIN(p_budget.passes, 64u) || r_usage.receipts >= MIN(p_budget.receipts, 128u) ||
				OS::get_singleton()->get_ticks_usec() >= p_budget.deadline_micros) {
			return false;
		}
		++r_usage.passes;
		r_usage.controls = 1;
		return true;
	};
	for (uint32_t i = 0; i < owner.config.max_views; ++i) {
		if (owner.views[i].history_slot == p_slot) {
			if (!owner.views[i].closed || !spend()) {
				return ERR_BUSY;
			}
			// Closing the view removes a future logical reference only. Native
			// groups and old payload storage remain pinned below until returned.
			owner.views[i].history_slot = UINT32_MAX;
			return OK;
		}
	}
	if (slot.retire_surface < slot.surface_count) {
		if (!slot.surfaces) {
			return ERR_INVALID_DATA;
		}
		auto &surface = slot.surfaces[slot.retire_surface];
		if (!surface.geometry.is_active() && (surface.geometry_group || surface.geometry.has_retirement_directory())) {
			return ERR_INVALID_DATA;
		}
		if (surface.geometry.is_active()) {
			if (!surface.geometry_transferred) {
				// Missing original capture/transfer prefix cannot be inferred
				// from a live wrapper. Retain it for original prefix recovery.
				return ERR_BUSY;
			}
			if (surface.geometry_group) {
				error = owner.device->_production_output_retire_service(p_pin, surface.geometry_group, p_budget, r_usage, observation);
				if (error == OK) {
					surface.geometry_group = 0;
					surface.geometry_group_returned = true;
				}
				return error;
			}
			if (!surface.geometry_group_returned) {
				return ERR_INVALID_DATA;
			}
			if (!surface.geometry_directory_returned) {
				if (!surface.geometry.has_retirement_directory()) {
					return ERR_INVALID_DATA;
				}
				const uint32_t before = r_usage.passes;
				error = MeshStorage::_native_hold_return_directory(p_pin, surface.geometry, p_budget, r_usage);
				if (error == OK) {
					surface.geometry_directory_returned = true;
					return OK;
				}
				if (error != ERR_BUSY || r_usage.passes != before) {
					return error;
				}
				bool marked = false;
				return MeshStorage::_native_hold_mark_next_retirement_root(p_pin, surface.geometry, p_budget, r_usage, marked);
			}
			if (!spend()) {
				return ERR_BUSY;
			}
			if (!surface.geometry_metadata_returned) {
				error = MeshStorage::_native_hold_begin_return(surface.geometry);
				if (error == OK) {
					surface.geometry_metadata_returned = true;
				}
				return error;
			}
			return MeshStorage::_native_hold_finish_return(surface.geometry);
		}
		if (!surface.material.is_active() && surface.material_group) {
			return ERR_INVALID_DATA;
		}
		if (surface.material.is_active()) {
			if (!surface.material_transferred) {
				return ERR_BUSY;
			}
			if (surface.material_group) {
				error = owner.device->_production_output_retire_service(p_pin, surface.material_group, p_budget, r_usage, observation);
				if (error == OK) {
					surface.material_group = 0;
					surface.material_group_returned = true;
				}
				return error;
			}
			if (!surface.material_group_returned) {
				return ERR_INVALID_DATA;
			}
			if (!spend()) {
				return ERR_BUSY;
			}
			if (!surface.material_metadata_returned) {
				error = MaterialStorage::_native_hold_begin_return(surface.material);
				if (error == OK) {
					surface.material_metadata_returned = true;
				}
				return error;
			}
			return MaterialStorage::_native_hold_finish_return(surface.material);
		}
		if (!spend()) {
			return ERR_BUSY;
		}
		++slot.retire_surface;
		return OK;
	}
	if (slot.surfaces) {
		if (!spend()) {
			return ERR_BUSY;
		}
		memdelete_arr(slot.surfaces);
		slot.surfaces = nullptr;
		slot.surface_count = slot.retire_surface = 0;
		return OK;
	}
	if (slot.owned_group) {
		// Before dropping an owned group, every real storage RID must belong
		// to that exact original group. Primitive RID equality is insufficient.
		if (slot.output_membership_group && slot.output_membership_group != slot.owned_group) {
			return ERR_INVALID_DATA;
		}
		if (slot.storage && !slot.output_membership_group) {
			for (uint32_t i = 0; i < ExternalOutputStorage::RESOURCE_COUNT; ++i) {
				const RID rid = slot.storage->resources[i].rid;
				if (rid.is_valid() && !owner.device->_production_output_hold_contains(p_pin, slot.owned_group, rid)) {
					return ERR_INVALID_DATA;
				}
			}
			if (!spend()) {
				return ERR_BUSY;
			}
			slot.output_membership_group = slot.owned_group;
			return OK;
		}
		error = owner.device->_production_output_retire_service(p_pin, slot.owned_group, p_budget, r_usage, observation);
		if (error == OK) {
			slot.owned_group = 0;
			slot.output_group_returned = true;
			if (slot.storage) {
				slot.storage->retirement_requested = true;
			}
		}
		return error;
	}
	if (slot.storage) {
		if (!slot.output_group_returned || !slot.storage->retirement_requested || slot.storage->unknown) {
			return ERR_INVALID_DATA;
		}
		if (slot.retire_resource < ExternalOutputStorage::RESOURCE_COUNT) {
			const auto &resource = slot.storage->resources[slot.retire_resource];
			if (resource.rid.is_valid()) {
				if (OS::get_singleton()->get_ticks_usec() >= p_budget.deadline_micros) {
					return ERR_BUSY;
				}
				NativeDisposalWitness::Report report;
				error = owner.device->_native_disposal_observe(resource.witness, report);
				if (report.state == NativeDisposalWitness::State::UNKNOWN) {
					if (!spend()) {
						return ERR_BUSY;
					}
					slot.storage->unknown = true;
					return error == OK ? ERR_INVALID_DATA : error;
				}
				if (error != OK) {
					return error;
				}
				if (report.state != NativeDisposalWitness::State::RETURNED) {
					return ERR_BUSY; // Preserve budget for original native cursor.
				}
			}
			if (!spend()) {
				return ERR_BUSY;
			}
			if (resource.rid.is_valid()) {
				++r_usage.receipts;
			}
			++slot.retire_resource;
			return OK;
		}
		if (!spend()) {
			return ERR_BUSY;
		}
		// Each exact original witness was observed physically returned above.
		// No ordinary disposal cursor or speculative free is used here.
		delete slot.storage;
		slot.storage = nullptr;
		return OK;
	}
	if (!spend()) {
		return ERR_BUSY;
	}
	// Registry independently blocks any original groups still pending. E05
	// source/result acknowledgment remains its original owner responsibility.
	error = owner.return_slot_pin(p_slot);
	if (error != OK) {
		return error;
	}
	if ((owner.state == ExternalOutputOwner::GenerationState::WITHDRAWING ||
				owner.state == ExternalOutputOwner::GenerationState::RETIRING ||
				owner.state == ExternalOutputOwner::GenerationState::FAULTED_RETIRING) &&
			!owner.has_live_slots()) {
		// This is original native owner closure only; G still retains its E05
		// source obligations and cannot infer its own acknowledgment here.
		error = owner.return_reserved_cells();
		if (error != OK) {
			return error;
		}
		owner.state = ExternalOutputOwner::GenerationState::RELEASED;
	}
	return OK;
}
} // namespace RendererRD
