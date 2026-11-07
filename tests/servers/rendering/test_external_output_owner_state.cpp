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
#include "core/os/memory_allocation_test_hooks.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_packet.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "tests/servers/rendering/test_external_output_custody_bridge.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_external_output_owner_state)

namespace RendererRD {
ExternalLinkedOwnerTest::ExternalLinkedOwnerTest(RenderingDevice &rd, bool p_empty_storage) :
		expected_device(&rd), empty_storage_mode(p_empty_storage) {
	storage = memnew(ParticlesStorage(this));
	if (empty_storage_mode) {
		return;
	}
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
	if (empty_storage_mode) {
		// A failed test cannot disguise a leaked real Create result as cleanup.
		CRASH_COND(owner || storage->particles_owner.get_rid_count());
		memdelete(storage);
		return;
	}
	// Fail closed on leaked custody; never manufacture RELEASED during cleanup.
	CRASH_COND(!released());
	storage->particles_owner.get_or_null(particles)->external = nullptr;
	memdelete(owner);
	storage->particles_owner.free(particles);
	memdelete(storage);
}
bool ExternalLinkedOwnerTest::adopt_created(RID p_particles) {
	if (!empty_storage_mode || owner) {
		return false;
	}
	auto *created = storage->particles_owner.get_or_null(p_particles);
	if (!created || !created->external || created->external->device != expected_device ||
			created->external->metadata_lease.is_empty() || created->external->generation_pin.is_empty()) {
		return false;
	}
	particles = p_particles;
	owner = created->external;
	return true;
}
bool ExternalLinkedOwnerTest::exists() const {
	const auto *created = storage->particles_owner.get_or_null(particles);
	return created && owner && created->external == owner;
}
void ExternalLinkedOwnerTest::free_created() {
	CRASH_COND(!empty_storage_mode || !exists());
	storage->particles_free(particles);
	if (!storage->particles_owner.get_or_null(particles)) {
		owner = nullptr;
	}
}
void ExternalLinkedOwnerTest::create_with_metadata_counts(GDExtensionDeviceExit::ProductionPhysicalContextRef context,
		GDExtensionProductionRequestRefV1 request, const GDExtensionOutputCreateInputV1 &input, uint32_t metadata_slot,
		uint32_t views, uint32_t reads, uint32_t surfaces, GDExtensionOutputCreateResultV1 &result) {
	storage->_external_create_typed(context, request, input, metadata_slot, views, reads, surfaces, result);
}
Error ExternalLinkedOwnerTest::metadata_directory_bytes(uint32_t views, uint32_t reads, uint32_t surfaces, uint64_t &bytes) {
	ExternalOutputOwner::MetadataLayout layout;
	const Error error = ExternalOutputOwner::plan_metadata_layout(views, reads, surfaces, layout);
	if (error == OK) {
		bytes = layout.total_bytes;
	}
	return error;
}
bool ExternalLinkedOwnerTest::metadata_directory_in_slab(uint32_t views, uint32_t reads, uint32_t surfaces) const {
	if (!exists() || !owner->metadata_directory_installed || !owner->metadata_directories_placement ||
			owner->metadata_lease.get_data() != owner) {
		return false;
	}
	ExternalOutputOwner::MetadataLayout layout;
	if (ExternalOutputOwner::plan_metadata_layout(views, reads, surfaces, layout) != OK ||
			layout.total_bytes > owner->metadata_lease.get_size()) {
		return false;
	}
	const auto *base = static_cast<const uint8_t *>(owner->metadata_lease.get_data());
	return owner->view_capacity == views && owner->read_capacity == reads && owner->surface_capacity == surfaces &&
			static_cast<const void *>(owner->view_cells) == (views ? base + layout.view_offset : nullptr) &&
			static_cast<const void *>(owner->read_cells) == (reads ? base + layout.read_offset : nullptr) &&
			static_cast<const void *>(owner->surface_cells) == (surfaces ? base + layout.surface_offset : nullptr);
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
Error ExternalLinkedOwnerTest::install_generation(GDExtensionDeviceExit::ProductionPhysicalContextRef context,
		GDExtensionProductionRequestRefV1 request, GDExtensionDeviceExit::Registry &registry) {
	return install_generation_at(context, request, registry, particles);
}
Error ExternalLinkedOwnerTest::install_generation_at(GDExtensionDeviceExit::ProductionPhysicalContextRef context,
		GDExtensionProductionRequestRefV1 request, GDExtensionDeviceExit::Registry &registry, RID locator) {
	return owner->install_generation_root(context, request, registry, *owner->device, locator);
}
GDExtensionDeviceExit::ProductionOutputPin &ExternalLinkedOwnerTest::generation_pin() {
	CRASH_COND(!exists());
	return owner->generation_pin;
}
uint64_t ExternalLinkedOwnerTest::generation_serial() const {
	return exists() ? owner->generation_root_serial : 0;
}
bool ExternalLinkedOwnerTest::generation_uninstalled() const {
	return owner->generation_pin.is_empty() && !owner->generation_registry && !owner->generation_root_serial &&
			!owner->generation_request.registry_instance && !owner->generation_request.handle_serial;
}
Error ExternalLinkedOwnerTest::prepare_generation_directory() {
	// Explicit finite component metadata, prepared before actual RECORD. This
	// does not pretend that the missing public U08/Create admission was issued.
	return owner->install_reserved_cells(1, 1);
}
bool ExternalLinkedOwnerTest::generation_directory_empty() const {
	return !owner->view_cells && !owner->read_cells && !owner->surface_cells &&
			!owner->view_capacity && !owner->read_capacity && !owner->surface_capacity;
}
GDExtensionDeviceExit::ProductionOutputPin &ExternalLinkedOwnerTest::generation_directory_pin() {
	// An actual finite directory cell, without inventing a view consumer/phase.
	CRASH_COND(!owner->view_cells || owner->view_capacity != 1);
	return owner->view_cells[0].pin;
}
void ExternalLinkedOwnerTest::withdraw_generation() {
	// Call the original admission-close transition; zero Acquire has no history
	// slot to invent and no public phase/source/lease token to manufacture.
	owner->withdraw();
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
void ExternalLinkedOwnerTest::poll_model_begin_storage() {
	CRASH_COND(!empty_storage_mode || !exists() || owner->slot_count != 1);
	auto &slot = owner->slots[0];
	CRASH_COND(slot.storage || !slot.original_pin.is_empty() || slot.retire_resource);
	// Deliberately private CPU storage incarnation; no Acquire/lease is issued.
	slot.storage = new ExternalOutputStorage(*owner->device);
}
RID &ExternalLinkedOwnerTest::poll_model_resource_rid(uint32_t index) {
	CRASH_COND(!owner->slots[0].storage || index >= ExternalOutputStorage::RESOURCE_COUNT);
	return owner->slots[0].storage->resources[index].rid;
}
void ExternalLinkedOwnerTest::poll_model_consume_receipt(uint32_t index) {
	auto &slot = owner->slots[0];
	CRASH_COND(!slot.storage || index != slot.retire_resource || index >= ExternalOutputStorage::RESOURCE_COUNT || slot.storage->resources[index].rid.is_null());
	// Models the original retirement receipt order only. The caller separately
	// establishes the actual driver-free witness before this metadata projection.
	++slot.physical_returned;
	++slot.retire_resource;
}
void ExternalLinkedOwnerTest::poll_model_end_storage(uint32_t receipts) {
	auto &slot = owner->slots[0];
	CRASH_COND(!slot.storage || slot.retire_resource != receipts);
	delete slot.storage;
	slot.storage = nullptr;
	// Original return_slot_pin resets cursor and retains cumulative return count.
	slot.retire_resource = 0;
}
void ExternalLinkedOwnerTest::prepare_storage(uint32_t capacity, uint32_t surfaces) {
	CRASH_COND(owner->has_live_slots() || owner->slots[0].storage || owner->slots[0].surfaces);
	owner->config.profile = ExternalOutputConfig::INSTANCE_192;
	owner->config.capacity = capacity;
	auto &slot = owner->slots[0];
	slot.surfaces = memnew_arr(ExternalOutputOwner::SurfaceBinding, surfaces);
	slot.surface_count = surfaces;
	slot.storage = new ExternalOutputStorage(*owner->device);
}
Error ExternalLinkedOwnerTest::allocate_storage(GDExtensionProductionPhysicalContextRefV1 context) {
	return storage->_external_storage_prepare_owned_prefix(context, particles, 0);
}
uint64_t ExternalLinkedOwnerTest::storage_group() const {
	return owner->slots[0].owned_group;
}
RID ExternalLinkedOwnerTest::storage_rid(uint32_t role) const {
	CRASH_COND(role >= ExternalOutputStorage::RESOURCE_COUNT || !owner->slots[0].storage);
	return owner->slots[0].storage->resources[role].rid;
}
uint32_t ExternalLinkedOwnerTest::storage_bytes(uint32_t role) const {
	CRASH_COND(role >= ExternalOutputStorage::RESOURCE_COUNT || !owner->slots[0].storage);
	return owner->slots[0].storage->resources[role].bytes;
}
uint64_t ExternalLinkedOwnerTest::storage_total_bytes() const {
	CRASH_COND(!owner->slots[0].storage);
	return owner->slots[0].storage->total_bytes;
}
NativeDisposalWitness &ExternalLinkedOwnerTest::storage_witness(uint32_t role) {
	CRASH_COND(role >= ExternalOutputStorage::RESOURCE_COUNT || !owner->slots[0].storage);
	return owner->slots[0].storage->resources[role].witness;
}
bool ExternalLinkedOwnerTest::storage_tokens_empty() const {
	const auto &slot = owner->slots[0];
	return slot.lease_token.is_empty() && slot.record_token.is_empty() && slot.packet_token.is_empty();
}
bool ExternalLinkedOwnerTest::storage_empty() const {
	return !owner->slots[0].storage;
}
void ExternalLinkedOwnerTest::discard_unattempted_storage() {
	auto &slot = owner->slots[0];
	CRASH_COND(!slot.storage || slot.storage->attempted || slot.owned_group);
	delete slot.storage;
	slot.storage = nullptr;
	memdelete_arr(slot.surfaces);
	slot.surfaces = nullptr;
	slot.surface_count = 0;
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
	if (!exists() || owner->state != ExternalOutputOwner::GenerationState::RELEASED || !owner->generation_pin.is_empty()) {
		return false;
	}
	for (uint32_t i = 0; i < owner->slot_count; ++i) {
		if (!owner->slots[i].original_pin.is_empty()) {
			return false;
		}
	}
	return true;
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
	static void metadata_layout() {
		using Layout = ExternalOutputOwner::MetadataLayout;
		Layout zero;
		REQUIRE(ExternalOutputOwner::plan_metadata_layout(0, 0, 0, zero) == OK);
		CHECK(zero.total_bytes == sizeof(ExternalOutputOwner));
		CHECK(zero.view_offset == 0);
		CHECK(zero.read_offset == 0);
		CHECK(zero.surface_offset == 0);
		Layout layout;
		// Only the planner is observed: assertions and the test framework are
		// outside the allocator observation window.
		const bool was_enabled = MemoryAllocationTest::enabled;
		const uint64_t attempts = MemoryAllocationTest::attempts;
		MemoryAllocationTest::enabled = true;
		const Error planned = ExternalOutputOwner::plan_metadata_layout(2, 3, 4, layout);
		MemoryAllocationTest::enabled = was_enabled;
		CHECK(MemoryAllocationTest::attempts == attempts);
		REQUIRE(planned == OK);
		CHECK(layout.view_count == 2);
		CHECK(layout.read_count == 3);
		CHECK(layout.surface_count == 4);
		CHECK(layout.view_offset >= sizeof(ExternalOutputOwner));
		CHECK(layout.view_offset % alignof(ExternalOutputOwner::ViewCell) == 0);
		CHECK(layout.read_offset % alignof(ExternalOutputOwner::ReadCell) == 0);
		CHECK(layout.surface_offset % alignof(ExternalOutputOwner::SurfaceCell) == 0);
		CHECK(layout.read_offset >= layout.view_offset + 2 * sizeof(ExternalOutputOwner::ViewCell));
		CHECK(layout.surface_offset >= layout.read_offset + 3 * sizeof(ExternalOutputOwner::ReadCell));
		CHECK(layout.total_bytes == layout.surface_offset + 4 * sizeof(ExternalOutputOwner::SurfaceCell));
		CHECK(layout.alignment >= alignof(ExternalOutputOwner));
		CHECK(layout.alignment >= alignof(ExternalOutputOwner::ViewCell));
		CHECK(layout.alignment >= alignof(ExternalOutputOwner::ReadCell));
		CHECK(layout.alignment >= alignof(ExternalOutputOwner::SurfaceCell));
		const Layout before = layout;
		CHECK(ExternalOutputOwner::plan_metadata_layout(UINT32_MAX, 0, 0, layout) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(memcmp(&layout, &before, sizeof(layout)) == 0);
		CHECK(ExternalOutputOwner::plan_metadata_layout(0, UINT32_MAX, 0, layout) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(memcmp(&layout, &before, sizeof(layout)) == 0);
		CHECK(ExternalOutputOwner::plan_metadata_layout(0, 0, UINT32_MAX, layout) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(memcmp(&layout, &before, sizeof(layout)) == 0);
		const uint64_t start = (sizeof(ExternalOutputOwner) + alignof(ExternalOutputOwner::ViewCell) - 1) &
				~uint64_t(alignof(ExternalOutputOwner::ViewCell) - 1);
		const uint32_t largest = uint32_t((UINT32_MAX - start) / sizeof(ExternalOutputOwner::ViewCell));
		REQUIRE(ExternalOutputOwner::plan_metadata_layout(largest, 0, 0, layout) == OK);
		CHECK(layout.total_bytes == start + uint64_t(largest) * sizeof(ExternalOutputOwner::ViewCell));
		const Layout boundary = layout;
		CHECK(ExternalOutputOwner::plan_metadata_layout(largest + 1, 0, 0, layout) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(memcmp(&layout, &boundary, sizeof(layout)) == 0);
	}
	static void inline_generation_config() {
		ExternalOutputConfig config;
		config.max_views = config.max_inflight = 1;
		config.producer_tag = tag(1);
		config.device_tag = tag(2);
		config.layout_digest = tag(3);
		config.mapping_digest = tag(4);
		config.numeric_profile_digest = tag(5);
		ExternalOutputOwner owner(config, 1, true);
		CHECK(owner.config.producer_tag.is_empty());
		CHECK(owner.config.device_tag.is_empty());
		CHECK(owner.config.layout_digest.is_empty());
		CHECK(owner.config.mapping_digest.is_empty());
		CHECK(owner.config.numeric_profile_digest.is_empty());
		// Parser allocations and subsequent parser writes cannot become the
		// lifetime or value source for the persistent native generation.
		config.layout_digest.set(0, 9);
		config.mapping_digest = PackedByteArray();
		auto selected = request(0, 1);
		selected.layout_digest = tag(3);
		selected.mapping_digest = tag(4);
		uint32_t view = UINT32_MAX, slot = UINT32_MAX;
		CHECK(owner.select_slot(selected, view, slot) == OK);
		selected.layout_digest = config.layout_digest;
		CHECK(owner.select_slot(selected, view, slot) == ERR_INVALID_DATA);
		selected.layout_digest = PackedByteArray();
		CHECK(owner.select_slot(selected, view, slot) == ERR_INVALID_DATA);
		owner.state = ExternalOutputOwner::GenerationState::RELEASED;
	}
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
			// A stale wrapper must not discharge the actual current pin. Restore
			// only the injected wrapper field, never the Registry or cell state.
			++moved.serial;
			CHECK(moved.release_checked() == ERR_INVALID_DATA);
			CHECK(cell.consumer == &moved);
			CHECK(cell.serial == 1);
			CHECK_FALSE(cell.pin.is_empty());
			CHECK(o.slot_has_custody(0));
			--moved.serial;
			moved.slot = 1;
			CHECK(moved.release_checked() == ERR_INVALID_DATA);
			CHECK(cell.consumer == &moved);
			CHECK(cell.slot == 0);
			CHECK_FALSE(cell.pin.is_empty());
			moved.slot = 0;
			CHECK(moved.release_checked() == OK);
			CHECK(cell.pin.is_empty());
			CHECK_FALSE(o.slot_has_custody(0));
			// A returned wrapper cannot consume another custody edge or mutate
			// the original request accounting on a duplicate release.
			CHECK(moved.release_checked() == ERR_DOES_NOT_EXIST);
			CHECK(moved.is_empty());
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
			++moved.serial;
			CHECK(o.surface_return(moved) == ERR_INVALID_DATA);
			CHECK(cell.consumer == &moved);
			CHECK(cell.serial == 1);
			CHECK_FALSE(cell.pin.is_empty());
			--moved.serial;
			++moved.generation;
			CHECK(o.surface_return(moved) == ERR_INVALID_DATA);
			CHECK(cell.consumer == &moved);
			CHECK(moved.geometry_source == &slot.surfaces[0].geometry);
			CHECK_FALSE(cell.pin.is_empty());
			CHECK(o.slot_has_custody(0));
			--moved.generation;
			CHECK(o.surface_return(moved) == OK);
			CHECK(cell.pin.is_empty());
			CHECK_FALSE(o.slot_has_custody(0));
			// A released surface selection is no longer valid at the owner-private entry.
			CHECK(o.surface_return(moved) == ERR_INVALID_PARAMETER);
			CHECK(moved.is_empty());
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
			// Late record is still refused and cannot convert this existing
			// borrow into a native use or allow a stale cancellation to unpin it.
			CHECK(moved.record_checked(0) == ERR_UNAVAILABLE);
			CHECK(cell.state == ExternalOutputOwner::ReadCell::State::RESERVED);
			++moved.serial;
			CHECK(moved.cancel_before_draw() == ERR_INVALID_DATA);
			CHECK(o.read_return(moved, true) == ERR_INVALID_DATA);
			CHECK(cell.consumer == &moved);
			CHECK(cell.serial == 1);
			CHECK(cell.state == ExternalOutputOwner::ReadCell::State::RESERVED);
			CHECK_FALSE(cell.pin.is_empty());
			--moved.serial;
			moved.slot = 1;
			CHECK(moved.cancel_before_draw() == ERR_INVALID_DATA);
			CHECK(o.read_return(moved, true) == ERR_INVALID_DATA);
			CHECK(cell.consumer == &moved);
			CHECK(cell.slot == 0);
			CHECK(cell.state == ExternalOutputOwner::ReadCell::State::RESERVED);
			CHECK_FALSE(cell.pin.is_empty());
			CHECK(o.slot_has_custody(0));
			moved.slot = 0;
			if (mode == ExternalCustodyTestMode::READ_UNKNOWN) {
				// Exercise only the original return guard's finite state input;
				// these markers are not evidence that a native draw was recorded.
				// The pin itself is the real borrowed pin supplied by Registry.
				const uint64_t original_serial = cell.serial;
				for (auto state : { ExternalOutputOwner::ReadCell::State::RECORDED, ExternalOutputOwner::ReadCell::State::UNKNOWN }) {
					cell.state = state;
					CHECK(moved.cancel_before_draw() == ERR_BUSY);
					CHECK_FALSE(moved.is_empty());
					CHECK(cell.consumer == &moved);
					CHECK(cell.serial == original_serial);
					CHECK(cell.state == state);
					CHECK_FALSE(cell.pin.is_empty());
					CHECK(o.slot_has_custody(0));
					CHECK(o.return_slot_pin(0) == ERR_BUSY);
					CHECK(o.return_reserved_cells() == ERR_BUSY);
					// Repeated withdrawal closes only future work. It cannot
					// settle the retained read or erase its consumer directory.
					o.withdraw();
					CHECK(cell.consumer == &moved);
					CHECK(cell.serial == original_serial);
					CHECK(cell.state == state);
					CHECK_FALSE(cell.pin.is_empty());
				}
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
				// Cancellation is terminal for this binding; retrying must be
				// side-effect free and must not unpin a different read cell.
				CHECK(moved.cancel_before_draw() == ERR_DOES_NOT_EXIST);
				CHECK(moved.is_empty());
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

TEST_CASE("[B4][GenerationCreate] retained configuration does not retain parser allocations") {
	RendererRD::TestExternalOutputOwnerStateAccess::inline_generation_config();
}

TEST_CASE("[B4][GenerationMetadataLayout] finite placement slices are aligned disjoint and checked without allocation") {
	RendererRD::TestExternalOutputOwnerStateAccess::metadata_layout();
}

TEST_CASE("[B4][ExternalOutput] finite slot preflight and withdrawal preserve occupied metadata") {
	RendererRD::TestExternalOutputOwnerStateAccess::finite_policy();
}

TEST_CASE("[B4][ExternalOutput] finite custody cells refuse unissued acquisition and retain unknown metadata") {
	RendererRD::TestExternalOutputOwnerStateAccess::finite_cells_and_unissued_refusal();
}
