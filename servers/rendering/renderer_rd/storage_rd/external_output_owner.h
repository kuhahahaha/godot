/**************************************************************************/
/*  external_output_owner.h                                               */
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
#include "core/templates/local_vector.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_packet.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_request.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"

namespace RendererRD {
class ParticlesStorage;
class TestExternalOutputOwnerStateAccess;
class ExternalOutputStorage;

// The retained output state belongs to exactly one original Particles RID.
// Source tags are frozen equality projections, never source or budget grants.
// This object has no world, operation issuer, scheduler, or automatic submission.
class ExternalOutputOwner {
	friend class ParticlesStorage;
	friend class ExternalPacketViewLease;
	friend class ExternalSurfaceSelection;
	friend class ExternalPacketBinding;
#ifdef TESTS_ENABLED
	friend class TestExternalOutputOwnerStateAccess;
#endif
	static constexpr uint32_t MAX_VIEWS = 8;
	static constexpr uint32_t MAX_INFLIGHT = 4;
	static constexpr uint32_t MAX_SLOTS = MAX_VIEWS * MAX_INFLIGHT;

	enum class GenerationState : uint8_t {
		CONFIGURED,
		ACTIVE,
		WITHDRAWING,
		RETIRING,
		FAULTED_RETIRING,
		RELEASED,
	};
	enum class LeaseState : uint8_t {
		EMPTY,
		RESERVED,
		WRITING,
		SEALED_RECORDED,
		ADOPTED,
		EMPTY_OR_FAULT_PACKET,
		RETIRING,
		RELEASED,
	};
	struct SurfaceBinding {
		MeshStorage::NativeSurfaceHold geometry;
		MaterialStorage::NativeMaterialHold material;
		uint64_t command_offset = 0;
		uint32_t command_extent = 0;
		uint64_t geometry_group = 0, material_group = 0;
		bool geometry_transferred = false, material_transferred = false;
		bool geometry_metadata_returned = false, material_metadata_returned = false;
		bool geometry_directory_returned = false;
		bool geometry_group_returned = false, material_group_returned = false;
	};
	struct Slot {
		LeaseState state = LeaseState::EMPTY;
		ExternalOutputRequest request;
		PackedByteArray lease_token;
		PackedByteArray record_token;
		PackedByteArray packet_token;
		ExternalOutputStorage *storage = nullptr;
		SurfaceBinding *surfaces = nullptr;
		uint32_t surface_count = 0;
		// Correlation only; the original finite accepted-arena reverse custody
		// must be installed by Registry before this slot can become Reserved.
		// A copied DTO cannot retain a request or authorize any append.
		GDExtensionProductionRequestRefV1 original_request = {};
		GDExtensionDeviceExit::Registry *original_registry = nullptr;
		GDExtensionDeviceExit::ProductionOutputPin original_pin;
		uint64_t original_revision = 0;
		uint64_t lease_serial = 0;
		uint64_t owned_group = 0, output_membership_group = 0;
		uint32_t retire_surface = 0, retire_resource = 0;
		bool output_group_returned = false;
		bool publication_closed = false;
		bool pass_uses_closed = false;
		Transform3D current_host, previous_host;
		bool retained_host_snapshot = false;
	};
	struct View {
		PackedByteArray tag;
		uint64_t epoch = 0;
		uint64_t last_present = 0;
		// History points only to a retained slot of this same owner.
		uint32_t history_slot = UINT32_MAX;
		bool occupied = false;
		bool closed = false;
	};

	// Explicit original parent metadata capacity; no implicit per-pass default.
	// Arrays are installed once before any custody and never relocated live.
	struct SurfaceCell {
		ExternalSurfaceSelection *consumer = nullptr;
		GDExtensionDeviceExit::ProductionOutputPin pin;
		uint64_t serial = 0;
		uint32_t slot = UINT32_MAX, surface = UINT32_MAX;
	};
	SurfaceCell *surface_cells = nullptr;
	uint32_t surface_capacity = 0;
	uint64_t next_surface_serial = 1;
	struct ViewCell {
		ExternalPacketViewLease *consumer = nullptr;
		GDExtensionDeviceExit::ProductionOutputPin pin;
		uint64_t serial = 0;
		uint32_t slot = UINT32_MAX;
	};
	struct ReadCell {
		enum class State : uint8_t { UNUSED,
			RESERVED,
			RECORDED,
			UNKNOWN,
			RETURNED };
		ExternalPacketBinding *consumer = nullptr;
		GDExtensionDeviceExit::ProductionOutputPin pin;
		uint64_t serial = 0;
		uint32_t slot = UINT32_MAX;
		State state = State::UNUSED;
	};
	ViewCell *view_cells = nullptr;
	ReadCell *read_cells = nullptr;
	uint32_t view_capacity = 0, read_capacity = 0;
	uint64_t next_view_serial = 1, next_read_serial = 1;

	ExternalOutputConfig config;
	GenerationState state = GenerationState::CONFIGURED;
	uint64_t generation = 0;
	RenderingDevice *device = nullptr;
	uint32_t slot_count = 0;
	uint64_t next_lease_serial = 1;
	View views[MAX_VIEWS];
	Slot slots[MAX_SLOTS];
	// Original parent capacity/accounting and identity association must be
	// installed before constructing this owner. No Dictionary value is a charge.
	ExternalOutputOwner(const ExternalOutputConfig &p_config, uint64_t p_generation) :
			config(p_config), generation(p_generation), slot_count(p_config.max_views * p_config.max_inflight) {
		CRASH_COND(!generation || !p_config.max_views || p_config.max_views > MAX_VIEWS || !p_config.max_inflight || p_config.max_inflight > MAX_INFLIGHT);
	}
	ExternalOutputOwner(const ExternalOutputOwner &) = delete;
	ExternalOutputOwner &operator=(const ExternalOutputOwner &) = delete;

public:
	~ExternalOutputOwner() {
		CRASH_COND(surface_cells || view_cells || read_cells);
		// No destructor fabricates native/source return or drops accepted uses.
		CRASH_COND(state != GenerationState::RELEASED);
		for (uint32_t i = 0; i < slot_count; ++i) {
			CRASH_COND(slots[i].storage || slots[i].surfaces || !slots[i].original_pin.is_empty());
		}
	}

private:
	// Pure finite preflight only. Original Registry custody and original U08
	// charge must both succeed before a caller commits a Reserved slot.
	Error select_slot(const ExternalOutputRequest &p_request, uint32_t &r_view, uint32_t &r_slot) const;
	Error install_reserved_cells(uint32_t p_views, uint32_t p_reads);
	Error install_reserved_surface_cells(uint32_t p_surfaces);
	Error surface_return(ExternalSurfaceSelection &);
	void surface_move(ExternalSurfaceSelection &, ExternalSurfaceSelection &);
	Error return_reserved_cells();
	// A real existing pin is mandatory even for metadata-only reservation.
	Error reserve_slot(const ExternalOutputRequest &, GDExtensionDeviceExit::Registry &, GDExtensionProductionRequestRefV1, const GDExtensionDeviceExit::ProductionOutputPin &, RenderingDevice &, uint32_t &);
	Error cancel_slot(uint32_t);
	bool slot_has_custody(uint32_t) const;
	Error return_slot_pin(uint32_t);
	Error view_acquire(uint32_t, ExternalPacketViewLease &);
	Error view_return(ExternalPacketViewLease &);
	void view_move(ExternalPacketViewLease &, ExternalPacketViewLease &);
	Error read_return(ExternalPacketBinding &, bool);
	void read_move(ExternalPacketBinding &, ExternalPacketBinding &);

	// Withdrawal changes admission only. Existing captures, buffers, bindings,
	// original request references and history stay retained for real retirement.
	void withdraw() {
		if (state == GenerationState::RELEASED || state == GenerationState::FAULTED_RETIRING || state == GenerationState::WITHDRAWING || state == GenerationState::RETIRING) {
			return;
		}
		state = GenerationState::WITHDRAWING;
		for (uint32_t i = 0; i < config.max_views; ++i) {
			views[i].closed = true;
		}
		for (uint32_t i = 0; i < slot_count; ++i) {
			slots[i].publication_closed = true;
			slots[i].pass_uses_closed = true;
			if (slots[i].state != LeaseState::EMPTY && slots[i].state != LeaseState::RELEASED) {
				slots[i].state = LeaseState::RETIRING;
			}
		}
	}
	bool admits_work() const {
		return state == GenerationState::CONFIGURED || state == GenerationState::ACTIVE;
	}
	bool has_live_slots() const {
		for (uint32_t i = 0; i < slot_count; ++i) {
			if (slots[i].state != LeaseState::EMPTY && slots[i].state != LeaseState::RELEASED) {
				return true;
			}
		}
		return false;
	}
};
} // namespace RendererRD
