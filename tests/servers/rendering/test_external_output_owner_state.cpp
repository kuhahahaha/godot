/**************************************************************************/
/*  test_external_output_owner_state.cpp                                  */
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

#include "servers/rendering/renderer_rd/storage_rd/external_output_owner.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_packet.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "tests/servers/rendering/test_external_output_custody_bridge.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_external_output_owner_state)

namespace RendererRD {
ExternalLinkedOwnerTest::ExternalLinkedOwnerTest(RenderingDevice &rd) {
	storage = memnew(ParticlesStorage(this));
	ExternalOutputConfig config;
	config.max_views = 1;
	config.max_inflight = 2;
	owner = memnew(ExternalOutputOwner(config, generation));
	owner->device = &rd;
	particles = storage->particles_owner.allocate_rid();
	storage->particles_owner.initialize_rid(particles);
	storage->particles_owner.get_or_null(particles)->external = owner;
}
ExternalLinkedOwnerTest::~ExternalLinkedOwnerTest() {
	// Fail closed on leaked custody; never manufacture RELEASED during cleanup.
	CRASH_COND(!released());
	storage->particles_owner.get_or_null(particles)->external = nullptr;
	memdelete(owner);
	storage->particles_owner.free(particles);
	memdelete(storage);
}
GDExtensionDeviceExit::ProductionOutputPin &ExternalLinkedOwnerTest::pin(uint32_t slot) {
	CRASH_COND(slot >= 2);
	return owner->slots[slot].original_pin;
}
void ExternalLinkedOwnerTest::install(uint32_t index, GDExtensionDeviceExit::Registry &registry, GDExtensionProductionRequestRefV1 request) {
	CRASH_COND(index >= 2);
	auto &slot = owner->slots[index];
	slot.original_registry = &registry;
	slot.original_request = request;
	slot.lease_serial = lease(index);
	slot.state = ExternalOutputOwner::LeaseState::RESERVED;
}
void ExternalLinkedOwnerTest::prepare_lease_tokens() {
	// Private test metadata, not an admitted lease issuer. Allocate before RECORD.
	for (uint32_t i = 0; i < 2; ++i) {
		auto &token = owner->slots[i].lease_token;
		token.resize(32);
		memset(token.ptrw(), 17 + i, 32);
	}
}
void ExternalLinkedOwnerTest::copy_lease_token(uint32_t index, uint8_t *bytes) const {
	CRASH_COND(index >= 2 || owner->slots[index].lease_token.size() != 32);
	memcpy(bytes, owner->slots[index].lease_token.ptr(), 32);
}
bool ExternalLinkedOwnerTest::slot_reserved(uint32_t index) const {
	CRASH_COND(index >= 2);
	const auto &slot = owner->slots[index];
	return slot.state == ExternalOutputOwner::LeaseState::RESERVED && !slot.publication_closed && !slot.pass_uses_closed;
}
bool ExternalLinkedOwnerTest::withdrawing() const {
	return owner->state == ExternalOutputOwner::GenerationState::WITHDRAWING;
}
bool ExternalLinkedOwnerTest::view_open() const {
	return !owner->views[0].closed;
}
void ExternalLinkedOwnerTest::install_geometry(
		MeshStorage::NativeSurfaceHold &hold, uint64_t group) {
	CRASH_COND(!group || !hold.is_active() || owner->slots[0].surfaces);
	auto &slot = owner->slots[0];
	slot.surfaces = memnew_arr(ExternalOutputOwner::SurfaceBinding, 1);
	slot.surface_count = 1;
	slot.surfaces[0].geometry = std::move(hold);
	slot.surfaces[0].geometry_group = group;
	slot.surfaces[0].geometry_transferred = true;
}
bool ExternalLinkedOwnerTest::geometry_group_returned() const {
	const auto &slot = owner->slots[0];
	return slot.surfaces && slot.surfaces[0].geometry_group_returned;
}
bool ExternalLinkedOwnerTest::geometry_active() const {
	const auto &slot = owner->slots[0];
	return slot.surfaces && slot.surfaces[0].geometry.is_active();
}
void ExternalLinkedOwnerTest::install_pipeline(uint64_t group) {
	CRASH_COND(!group || owner->slots[0].surfaces);
	auto &slot = owner->slots[0];
	slot.surfaces = memnew_arr(ExternalOutputOwner::SurfaceBinding, 1);
	slot.surface_count = 1;
	slot.surfaces[0].pipeline_owned_group = group;
}
RID &ExternalLinkedOwnerTest::pipeline_root() {
	CRASH_COND(!owner->slots[0].surfaces);
	return owner->slots[0].surfaces[0].created_pipeline;
}
bool ExternalLinkedOwnerTest::pipeline_returned() const {
	return owner->slots[0].surfaces && owner->slots[0].surfaces[0].pipeline_group_returned;
}
MaterialStorage::NativeMaterialHold &ExternalLinkedOwnerTest::material_hold() {
	CRASH_COND(!owner->slots[0].surfaces);
	return owner->slots[0].surfaces[0].material;
}
void ExternalLinkedOwnerTest::install_material(uint64_t group) {
	CRASH_COND(!owner->slots[0].surfaces || !group || !owner->slots[0].surfaces[0].material.is_active());
	auto &surface = owner->slots[0].surfaces[0];
	surface.material_group = group;
	surface.material_transferred = true;
}
bool ExternalLinkedOwnerTest::material_active() const {
	return owner->slots[0].surfaces && owner->slots[0].surfaces[0].material.is_active();
}
void ExternalLinkedOwnerTest::withdraw() {
	// Synthetic logical-history marker only, never a consumed native tuple.
	owner->views[0].history_slot = 0;
	owner->withdraw();
}
bool ExternalLinkedOwnerTest::history_cleared() const {
	return owner->views[0].history_slot == UINT32_MAX;
}
bool ExternalLinkedOwnerTest::released() const {
	return owner->state == ExternalOutputOwner::GenerationState::RELEASED &&
			owner->slots[0].original_pin.is_empty() && owner->slots[1].original_pin.is_empty();
}

// CPU finite-table policy only. No native owner was admitted, no Core pin was
// acquired, no RID/lease/packet was published, and no return is being attested.
class TestExternalOutputOwnerStateAccess {
	static PackedByteArray tag(uint32_t value) {
		PackedByteArray bytes;
		bytes.resize_initialized(32);
		bytes.set(0, uint8_t(value));
		return bytes;
	}
	struct Table {
		static ExternalOutputConfig config() {
			ExternalOutputConfig c;
			c.max_views = 8;
			c.max_inflight = 4;
			return c;
		}
		ExternalOutputOwner owner{ config(), 1 };
		~Table() {
			// Destroy the synthetic metadata table, never a live production owner.
			for (uint32_t i = 0; i < owner.slot_count; ++i) {
				CRASH_COND(owner.slots[i].storage || owner.slots[i].surfaces || !owner.slots[i].original_pin.is_empty());
			}
			owner.state = ExternalOutputOwner::GenerationState::RELEASED;
		}
	};
	static ExternalOutputRequest request(uint32_t view, uint32_t present) {
		ExternalOutputRequest r;
		r.common.generation = 1;
		r.common.view_tag = tag(view + 1);
		r.common.output_tag = tag(present + 10);
		r.common.present_seq = present;
		r.view_epoch = 1;
		return r;
	}

public:
	static void issued_pin(GDExtensionDeviceExit::Registry &registry, GDExtensionProductionRequestRefV1 ref,
			GDExtensionDeviceExit::ProductionOutputPin &issued, ExternalCustodyTestMode mode,
			MeshStorage::NativeSurfaceHold *geometry) {
		REQUIRE_FALSE(issued.is_empty());
		Table table;
		auto &o = table.owner;
		REQUIRE(o.install_reserved_cells(1, 1) == OK);
		REQUIRE(o.install_reserved_surface_cells(1) == OK);
		auto &slot = o.slots[0];
		slot.original_registry = &registry;
		slot.original_request = ref;
		slot.state = ExternalOutputOwner::LeaseState::RESERVED;
		if (mode == ExternalCustodyTestMode::SLOT_PENDING || mode == ExternalCustodyTestMode::SLOT_RETURN) {
			// The caller's actual pin already owns pending original disposal
			// groups; closing metadata cannot discharge those physical groups.
			slot.original_pin = std::move(issued);
			o.withdraw();
			if (mode == ExternalCustodyTestMode::SLOT_PENDING) {
				CHECK(o.return_slot_pin(0) == ERR_BUSY);
				CHECK_FALSE(slot.original_pin.is_empty());
				issued = std::move(slot.original_pin);
			} else {
				CHECK(o.return_slot_pin(0) == OK);
				CHECK(slot.original_pin.is_empty());
				CHECK(slot.state == ExternalOutputOwner::LeaseState::RELEASED);
			}
		} else if (mode == ExternalCustodyTestMode::VIEW_RETURN) {
			ExternalPacketViewLease view;
			auto &cell = o.view_cells[0];
			cell.pin = std::move(issued);
			cell.consumer = &view;
			cell.slot = view.slot = 0;
			cell.serial = view.serial = 1;
			view.owner = &o;
			view.view_cell = 0;
			ExternalPacketViewLease moved(std::move(view));
			CHECK(view.is_empty());
			CHECK(cell.consumer == &moved);
			CHECK_FALSE(cell.pin.is_empty());
			o.withdraw();
			CHECK(o.slot_has_custody(0));
			CHECK(moved.release_checked() == OK);
			CHECK(cell.pin.is_empty());
			CHECK_FALSE(o.slot_has_custody(0));
		} else if (mode == ExternalCustodyTestMode::SURFACE_RETURN) {
			REQUIRE(geometry != nullptr);
			slot.surfaces = memnew_arr(ExternalOutputOwner::SurfaceBinding, 1);
			slot.surface_count = 1;
			slot.surfaces[0].geometry = std::move(*geometry);
			ExternalSurfaceSelection surface;
			auto &cell = o.surface_cells[0];
			cell.pin = std::move(issued);
			cell.consumer = &surface;
			cell.slot = cell.surface = surface.cell = 0;
			cell.serial = surface.serial = 1;
			surface.generation = o.generation;
			surface.owner = &o;
			surface.geometry_source = &slot.surfaces[0].geometry;
			ExternalSurfaceSelection moved(std::move(surface));
			CHECK(surface.is_empty());
			CHECK(cell.consumer == &moved);
			CHECK(moved.geometry_source == &slot.surfaces[0].geometry);
			o.withdraw();
			CHECK(o.slot_has_custody(0));
			CHECK(o.surface_return(moved) == OK);
			CHECK(cell.pin.is_empty());
			CHECK_FALSE(o.slot_has_custody(0));
			// Return the same real geometry hold to its original fixture owner.
			*geometry = std::move(slot.surfaces[0].geometry);
			memdelete_arr(slot.surfaces);
			slot.surfaces = nullptr;
			slot.surface_count = 0;
		} else {
			ExternalPacketBinding read;
			auto &cell = o.read_cells[0];
			cell.pin = std::move(issued);
			cell.consumer = &read;
			cell.slot = read.slot = 0;
			cell.serial = read.serial = 1;
			cell.state = ExternalOutputOwner::ReadCell::State::RESERVED;
			read.owner = &o;
			read.read_cell = 0;
			ExternalPacketBinding moved(std::move(read));
			CHECK(read.is_empty());
			CHECK(cell.consumer == &moved);
			o.withdraw();
			if (mode == ExternalCustodyTestMode::READ_UNKNOWN) {
				CHECK(o.read_return(moved, true) == OK);
				CHECK(moved.is_empty());
				CHECK(cell.state == ExternalOutputOwner::ReadCell::State::UNKNOWN);
				CHECK_FALSE(cell.pin.is_empty());
				CHECK(o.slot_has_custody(0));
				CHECK(o.return_slot_pin(0) == ERR_BUSY);
				CHECK(o.return_reserved_cells() == ERR_BUSY);
				// There is no production UNKNOWN settlement API. Extract test
				// wiring back to the real fixture, which returns this actual pin
				// through Registry; do not claim owner/native completion here.
				issued = std::move(cell.pin);
				cell.state = ExternalOutputOwner::ReadCell::State::UNUSED;
			} else {
				CHECK(moved.cancel_before_draw() == OK);
				CHECK(cell.pin.is_empty());
				CHECK(cell.state == ExternalOutputOwner::ReadCell::State::RETURNED);
				CHECK_FALSE(o.slot_has_custody(0));
			}
		}
		// Tear down only test wiring. Any unreturned real pin was moved back
		// to the original fixture; this does not attest request retirement.
		slot.original_registry = nullptr;
		slot.original_request = {};
		slot.state = ExternalOutputOwner::LeaseState::EMPTY;
		CHECK(o.return_reserved_cells() == OK);
	}
	static void finite_cells_and_unissued_refusal() {
		Table table;
		auto &o = table.owner;
		CHECK(o.install_reserved_surface_cells(0) == ERR_INVALID_PARAMETER);
		REQUIRE(o.install_reserved_surface_cells(1) == OK);
		auto *original_surfaces = o.surface_cells;
		CHECK(o.install_reserved_surface_cells(2) == ERR_ALREADY_IN_USE);
		CHECK(o.surface_cells == original_surfaces);
		CHECK(o.surface_capacity == 1);
		CHECK(o.install_reserved_cells(0, 1) == ERR_INVALID_PARAMETER);
		REQUIRE(o.install_reserved_cells(1, 1) == OK);
		CHECK(o.view_capacity == 1);
		CHECK(o.read_capacity == 1);
		auto *original_views = o.view_cells;
		auto *original_reads = o.read_cells;
		CHECK(o.install_reserved_cells(2, 2) == ERR_ALREADY_IN_USE);
		CHECK(o.view_cells == original_views);
		CHECK(o.read_cells == original_reads);
		ExternalPacketViewLease view;
		CHECK(o.view_acquire(0, view) == ERR_UNAVAILABLE);
		CHECK(view.is_empty());
		CHECK(o.slots[0].state == ExternalOutputOwner::LeaseState::EMPTY);
		CHECK(o.slots[0].original_pin.is_empty());
		CHECK(o.view_cells[0].pin.is_empty());
		CHECK(o.read_cells[0].pin.is_empty());
		CHECK_FALSE(o.slot_has_custody(0));
		// Synthetic finite metadata state only. No fabricated pin/native use:
		// UNKNOWN occupancy alone must still block cell/slot cleanup.
		o.read_cells[0].slot = 0;
		o.read_cells[0].state = ExternalOutputOwner::ReadCell::State::UNKNOWN;
		CHECK(o.slot_has_custody(0));
		CHECK(o.return_reserved_cells() == ERR_BUSY);
		CHECK(o.read_capacity == 1);
		CHECK(o.read_cells == original_reads);
		o.slots[0].state = ExternalOutputOwner::LeaseState::RETIRING;
		o.slots[0].publication_closed = true;
		o.slots[0].pass_uses_closed = true;
		CHECK(o.return_slot_pin(0) == ERR_BUSY);
		// Restore only that synthetic metadata; this is no completion receipt.
		o.read_cells[0].state = ExternalOutputOwner::ReadCell::State::RETURNED;
		o.slots[0].state = ExternalOutputOwner::LeaseState::EMPTY;
		CHECK(o.return_reserved_cells() == OK);
		CHECK(o.surface_cells == nullptr);
		CHECK(o.surface_capacity == 0);
		CHECK(o.view_cells == nullptr);
		CHECK(o.read_cells == nullptr);
		CHECK(o.view_capacity == 0);
		CHECK(o.read_capacity == 0);
		ExternalPacketBinding read;
		CHECK(o.read_return(read, false) == ERR_INVALID_PARAMETER);
		CHECK(o.read_return(read, true) == ERR_INVALID_PARAMETER);
		CHECK(view.release_checked() == ERR_DOES_NOT_EXIST);
		CHECK(read.cancel_before_draw() == ERR_DOES_NOT_EXIST);
		CHECK(read.record_checked(0) == ERR_UNAVAILABLE);
	}
	static void finite_policy() {
		Table table;
		auto &o = table.owner;
		CHECK(o.slot_count == 32);
		for (uint32_t view = 0; view < 8; ++view) {
			for (uint32_t present = 1; present <= 4; ++present) {
				auto r = request(view, present);
				uint32_t selected_view = UINT32_MAX, selected_slot = UINT32_MAX;
				REQUIRE(o.select_slot(r, selected_view, selected_slot) == OK);
				CHECK(selected_view == view);
				CHECK(selected_slot == view * 4 + present - 1);
				// Model occupied rows solely to exercise the finite policy.
				o.views[view].occupied = true;
				o.views[view].tag = r.common.view_tag;
				o.views[view].epoch = r.view_epoch;
				o.slots[selected_slot].request = r;
				o.slots[selected_slot].state = ExternalOutputOwner::LeaseState::RESERVED;
			}
		}
		uint32_t view = 0xabcdef, slot = 0x123456;
		CHECK(o.select_slot(request(8, 5), view, slot) == ERR_BUSY);
		CHECK((view == 0xabcdef && slot == 0x123456));
		CHECK(o.select_slot(request(0, 5), view, slot) == ERR_BUSY);
		CHECK(o.select_slot(request(0, 1), view, slot) == ERR_ALREADY_IN_USE);
		auto stale = request(0, 5);
		stale.common.generation = 2;
		CHECK(o.select_slot(stale, view, slot) == ERR_INVALID_PARAMETER);
		o.next_lease_serial = 0;
		CHECK(o.select_slot(request(0, 5), view, slot) == ERR_PARAMETER_RANGE_ERROR);
		o.next_lease_serial = UINT64_MAX;
		o.slots[0].state = ExternalOutputOwner::LeaseState::RELEASED;
		CHECK(o.select_slot(request(0, 5), view, slot) == OK);
		CHECK((view == 0 && slot == 0));
		CHECK(o.next_lease_serial == UINT64_MAX); // Preflight neither consumes nor wraps.
		o.views[0].history_slot = 0;
		CHECK(o.select_slot(request(0, 6), view, slot) == ERR_BUSY);
		// Malformed immutable profiles do not alter either output index.
		auto mismatch = request(0, 6);
		mismatch.layout_digest = tag(80);
		CHECK(o.select_slot(mismatch, view, slot) == ERR_INVALID_DATA);
		mismatch = request(0, 6);
		mismatch.mapping_digest = tag(81);
		CHECK(o.select_slot(mismatch, view, slot) == ERR_INVALID_DATA);
		mismatch = request(0, 6);
		mismatch.pass_mask = 0x20;
		CHECK(o.select_slot(mismatch, view, slot) == ERR_INVALID_DATA);
		o.views[0].closed = true;
		CHECK(o.select_slot(request(0, 6), view, slot) == ERR_UNAVAILABLE);
		o.views[0].closed = false;
		o.views[0].last_present = 9;
		CHECK(o.select_slot(request(0, 9), view, slot) == ERR_INVALID_PARAMETER);
		auto changed_epoch = request(0, 1);
		changed_epoch.view_epoch = 2;
		CHECK(o.select_slot(changed_epoch, view, slot) == ERR_BUSY);
		// Synthetic retired rows test only the selection policy. The real
		// return transition must come from original native/source observation.
		for (uint32_t old = 0; old < 4; ++old) {
			o.slots[old].state = ExternalOutputOwner::LeaseState::RELEASED;
		}
		o.views[0].history_slot = UINT32_MAX;
		CHECK(o.select_slot(changed_epoch, view, slot) == OK);
		CHECK(o.views[0].epoch == 1); // Pure preflight does not adopt/authorize epoch2.
		// The minimum envelope remains one view by one inflight slot.
		Table minimum;
		minimum.owner.config.max_views = minimum.owner.config.max_inflight = 1;
		minimum.owner.slot_count = 1;
		CHECK(minimum.owner.select_slot(request(0, 1), view, slot) == OK);
		CHECK((view == 0 && slot == 0));
		minimum.owner.views[0].occupied = true;
		minimum.owner.views[0].tag = request(0, 1).common.view_tag;
		minimum.owner.views[0].epoch = 1;
		minimum.owner.slots[0].state = ExternalOutputOwner::LeaseState::RESERVED;
		minimum.owner.slots[0].request = request(0, 1);
		CHECK(minimum.owner.select_slot(request(1, 2), view, slot) == ERR_BUSY);
		CHECK(minimum.owner.select_slot(request(0, 2), view, slot) == ERR_BUSY);
		o.withdraw();
		CHECK_FALSE(o.admits_work());
		CHECK(o.has_live_slots());
		CHECK(o.select_slot(request(0, 6), view, slot) == ERR_UNAVAILABLE);
		for (uint32_t i = 0; i < 32; ++i) {
			CHECK(o.slots[i].publication_closed);
			CHECK(o.slots[i].pass_uses_closed);
		}
		o.withdraw();
		CHECK(o.state == ExternalOutputOwner::GenerationState::WITHDRAWING);
	}
};
void external_custody_issued_pin_test(GDExtensionDeviceExit::Registry &registry, GDExtensionProductionRequestRefV1 ref,
		GDExtensionDeviceExit::ProductionOutputPin &pin, ExternalCustodyTestMode mode,
		MeshStorage::NativeSurfaceHold *geometry) {
	TestExternalOutputOwnerStateAccess::issued_pin(registry, ref, pin, mode, geometry);
}
} // namespace RendererRD

TEST_CASE("[B4][ExternalOutput] finite slot preflight and withdrawal preserve occupied metadata") {
	RendererRD::TestExternalOutputOwnerStateAccess::finite_policy();
}

TEST_CASE("[B4][ExternalOutput] finite custody cells refuse unissued acquisition and retain unknown metadata") {
	RendererRD::TestExternalOutputOwnerStateAccess::finite_cells_and_unissued_refusal();
}
