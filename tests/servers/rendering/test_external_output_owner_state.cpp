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

#include "core/os/memory_allocation_test_hooks.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_owner.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_packet.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/rendering_server.h"
#include "servers/rendering/rendering_server_globals.h"
#include "servers/rendering/rendering_method.h"
#include "tests/servers/rendering/test_external_output_custody_bridge.h"
#include "tests/test_macros.h"

#include <cstdio>

TEST_FORCE_LINK(test_external_output_owner_state)

namespace RendererRD {
namespace {
// CPU native owner identity only: no real buffer attachments or GPU proof.
class ReadCaptureTestBuffers : public RenderSceneBuffers {
public:
	void configure(const RenderSceneBuffersConfiguration *) override {}
	void set_fsr_sharpness(float) override {}
	void set_texture_mipmap_bias(float) override {}
	void set_anisotropic_filtering_level(RSE::ViewportAnisotropicFiltering) override {}
	void set_use_debanding(bool) override {}
};
} // namespace
Error ExternalLinkedOwnerTest::consume_host_inverse(const GDExtensionProductionHostTransformV1 &host,
		const PackedByteArray &token, const Transform3D &current, ParticlesExternalGuard::GeometryProofHeader &proof) {
	return ParticlesStorage::_external_guard_copy_host_inverse(host, token, current, proof);
}

void ExternalLinkedOwnerTest::scene_only_profile_query() {
	RenderingDevice device;
	ParticlesStorage storage(static_cast<ExternalLinkedOwnerTest *>(nullptr));
	const RendererParticlesStorage &interface = storage;
	CHECK_FALSE(interface.particles_is_external_scene_only(RID()));
	const RID ordinary = storage.particles_allocate();
	storage.particles_initialize(ordinary);
	CHECK_FALSE(interface.particles_is_external(ordinary));
	CHECK_FALSE(interface.particles_is_external_scene_only(ordinary));
	struct ProfileCase {
		ExternalOutputConfig::Profile profile;
		bool scene_only;
	};
	// All cases deliberately have zero capacity, passes and installed surfaces:
	// those values cannot stand in for the original owner's explicit profile.
	const ProfileCase cases[] = {
		{ ExternalOutputConfig::SCENE_ONLY, true },
		{ ExternalOutputConfig::INSTANCE_192, false },
		{ ExternalOutputConfig::INDEXED_144, false },
	};
	for (const ProfileCase &test : cases) {
		ExternalOutputConfig config;
		config.profile = test.profile;
		config.max_views = 1;
		config.max_inflight = 1;
		ExternalOutputOwner owner(config, 1);
		const RID rid = storage.particles_allocate();
		storage.particles_initialize(rid);
		storage.particles_owner.get_or_null(rid)->external = &owner;
		CHECK(interface.particles_is_external(rid));
		CHECK(interface.particles_is_external_scene_only(rid) == test.scene_only);
		CHECK(storage.particles_external_surface_count(rid) == 0);
		// No native resource/lease was admitted by this private component setup.
		storage.particles_owner.get_or_null(rid)->external = nullptr;
		storage.particles_owner.free(rid);
		owner.state = ExternalOutputOwner::GenerationState::RELEASED;
		CHECK_FALSE(interface.particles_is_external_scene_only(rid));
	}
	storage.particles_owner.free(ordinary);
}

ExternalLinkedOwnerTest::ExternalLinkedOwnerTest(RenderingDevice &rd, bool p_empty_storage, bool p_query_enabled) :
		expected_device(&rd), empty_storage_mode(p_empty_storage) {
	storage = memnew(ParticlesStorage(this));
	if (empty_storage_mode) {
		return;
	}
	initialize_owner(rd, p_query_enabled);
}
ExternalLinkedOwnerTest::ExternalLinkedOwnerTest(RenderingDevice &rd, ParticlesStorage &p_storage, bool p_query_enabled) :
		expected_device(&rd) {
	CRASH_COND(ParticlesStorage::get_singleton() != &p_storage || RenderingDevice::get_singleton() != &rd);
	CRASH_COND(!RenderingServer::get_singleton()->is_on_render_thread());
	storage = &p_storage;
	owns_storage = false;
	initialize_owner(rd, p_query_enabled);
}
void ExternalLinkedOwnerTest::initialize_owner(RenderingDevice &rd, bool p_query_enabled) {
	ExternalOutputConfig config;
	config.max_views = 1;
	config.max_inflight = 2;
	config.query_enabled = p_query_enabled;
	if (p_query_enabled) {
		config.query_bounds = AABB(Vector3(-1, -1, -1), Vector3(2, 2, 2));
	}
	if (p_query_enabled) {
		// Original immutable native fixture inputs, not a G binding grant.
		for (PackedByteArray *tag : { &config.producer_tag, &config.device_tag, &config.layout_digest, &config.mapping_digest, &config.numeric_profile_digest }) {
			tag->resize(32);
			tag->fill(0x42);
		}
	}
	owner = memnew(ExternalOutputOwner(config, generation, p_query_enabled));
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
	if (!owns_storage) {
		// Original particles_free performs dependency notification and removes
		// this released owner. The borrowed storage/device outlive the fixture.
		storage->particles_free(particles);
		return;
	}
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
	return owner->view_capacity == views && owner->read_capacity == reads && owner->surface_capacity == surfaces && owner->preparation_capacity == layout.preparation_count &&
			static_cast<const void *>(owner->preparation_rows) == (layout.preparation_count ? base + layout.preparation_offset : nullptr) &&
			static_cast<const void *>(owner->view_cells) == (views ? base + layout.view_offset : nullptr) &&
			static_cast<const void *>(owner->read_cells) == (reads ? base + layout.read_offset : nullptr) &&
			static_cast<const void *>(owner->surface_cells) == (surfaces ? base + layout.surface_offset : nullptr);
}
Error ExternalLinkedOwnerTest::selection_metadata_bytes(const ExternalOutputConfig &config, uint64_t &bytes, uint32_t *alignment, uint32_t p_reads) {
	ExternalOutputOwner::MetadataLayout layout;
	const Error error = ExternalOutputOwner::plan_generation_selection_layout(config, 1, p_reads, 7, layout);
	if (error == OK) {
		// The production generation bank is paid by its explicit global read limit,
		// independently of the slot/surface binding count. Keep the expected
		// slices independent of the planner's count and offset fields.
		uint64_t expected = sizeof(ExternalOutputOwner);
		auto append = [&](uint32_t count, size_t stride, size_t align) {
			if (count) {
				expected = ((expected + align - 1) & ~(uint64_t(align) - 1)) + uint64_t(count) * stride;
			}
		};
		append(1, sizeof(ExternalOutputOwner::ViewCell), alignof(ExternalOutputOwner::ViewCell));
		append(p_reads, sizeof(ExternalOutputOwner::ReadCell), alignof(ExternalOutputOwner::ReadCell));
		append(config.max_views * config.max_inflight, sizeof(ExternalOutputStorage), alignof(ExternalOutputStorage));
		append(config.max_views * config.max_inflight * config.surface_materials.size(), sizeof(ExternalOutputOwner::SurfaceBinding), alignof(ExternalOutputOwner::SurfaceBinding));
		append(p_reads, sizeof(ExternalPipelinePreparation), alignof(ExternalPipelinePreparation));
		append(config.surfaces.size(), sizeof(ExternalOutputOwner::GenerationMeshSelection), alignof(ExternalOutputOwner::GenerationMeshSelection));
		append(config.surface_materials.size(), sizeof(ExternalOutputOwner::GenerationMaterialSelection), alignof(ExternalOutputOwner::GenerationMaterialSelection));
		append(7, sizeof(ExternalOutputOwner::SurfaceCell), alignof(ExternalOutputOwner::SurfaceCell));
		append(config.max_views * config.max_inflight * (240 + config.surface_materials.size() * 144), 1, alignof(uint32_t));
		CHECK(layout.total_bytes == expected);
		bytes = layout.total_bytes;
		if (alignment) {
			*alignment = layout.alignment;
		}
	}
	return error;
}
Error ExternalLinkedOwnerTest::create_selection_component(GDExtensionDeviceExit::ProductionPhysicalContextRef context,
		GDExtensionProductionRequestRefV1 request, const ExternalOutputConfig &config, GDExtensionDeviceExit::ProductionOutputMetadataLease &metadata, int failure, uint32_t p_reads) {
	CRASH_COND(!empty_storage_mode || owner);
	ExternalOutputOwner::MetadataLayout layout;
	Error error = ExternalOutputOwner::plan_generation_selection_layout(config, 1, p_reads, 7, layout);
	if (error != OK) {
		return error;
	}
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	CRASH_COND(metadata.is_empty() || metadata.get_size() != layout.total_bytes);
	owner = new (metadata.get_data()) ExternalOutputOwner(config, generation, true, true);
	owner->device = expected_device;
	CRASH_COND(owner->install_metadata_directory(context, request, registry, metadata, 1, p_reads, 7, true) != OK);
	particles = storage->particles_owner.allocate_rid();
	storage->particles_owner.initialize_rid(particles);
	storage->particles_owner.get_or_null(particles)->external = owner;
	CRASH_COND(owner->install_generation_root(context, request, registry, *expected_device, particles, &metadata) != OK);
	owner->metadata_lease = static_cast<GDExtensionDeviceExit::ProductionOutputMetadataLease &&>(metadata);
	if (failure == 1) {
		// Real original mutation between snapshot and acquisition invalidates only
		// the second material, retaining the successful mesh+first-material prefix.
		MaterialStorage::get_singleton()->material_set_render_priority(config.surface_materials[1], 1);
	}
	if (failure == 2) {
		// Installed original root, invalid callback context: no first lease yet.
		error = owner->acquire_generation_selection({}, request);
		CHECK_FALSE(owner->generation_selection_attempted);
		CHECK(owner->state == ExternalOutputOwner::GenerationState::CONFIGURED);
	} else {
		error = owner->acquire_generation_selection(context, request);
	}
	if (error != OK) {
		if (failure == 1) {
			// Prove the component's own partial-prefix failure transition; the
			// outer guard below must not conceal a missing in-loop fail().
			CHECK(owner->generation_selection_attempted);
			CHECK(owner->state == ExternalOutputOwner::GenerationState::FAULTED_RETIRING);
			CHECK(owner->generation_selection_unpublished);
			CHECK_FALSE(owner->admits_work());
			for (uint32_t i = 0; i < owner->config.max_views; ++i) {
				CHECK(owner->views[i].closed);
			}
			for (uint32_t i = 0; i < owner->slot_count; ++i) {
				CHECK(owner->slots[i].publication_closed);
				CHECK(owner->slots[i].pass_uses_closed);
			}
		}
		// Explicit internal transaction owner guard, not implemented public Create.
		// acquire's pre-attempt refusals do not perform this caller obligation.
		owner->withdraw();
		owner->generation_selection_unpublished = true;
		owner->state = ExternalOutputOwner::GenerationState::FAULTED_RETIRING;
	}
	return error;
}
Error ExternalLinkedOwnerTest::acquire_selection_sources(GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 request) {
	return owner ? owner->acquire_generation_surface_sources(context, request) : ERR_DOES_NOT_EXIST;
}
uint32_t ExternalLinkedOwnerTest::selection_sources() const {
	return owner ? owner->retained_surface_count : 0;
}
Error ExternalLinkedOwnerTest::generation_returnable() const {
	uint32_t unused = 0, returned = 0;
	return owner ? owner->generation_returnable(unused, returned) : ERR_DOES_NOT_EXIST;
}
Error ExternalLinkedOwnerTest::derive_selection_source(uint32_t index, uint64_t mask, MeshStorage::NativeSurfaceHold &hold) const {
	if (!owner || index >= owner->retained_surface_count) {
		return ERR_DOES_NOT_EXIST;
	}
	return MeshStorage::mesh_surface_hold_derive(owner->generation_materials[index].geometry_source, mask, false, false, 0, hold);
}
bool ExternalLinkedOwnerTest::selection_source_matches(uint32_t index, RID mesh, uint32_t surface, RID material, uint64_t generation, const AABB &bounds, uint64_t format) const {
	if (!owner || index >= owner->retained_surface_count) {
		return false;
	}
	const auto &row = owner->generation_materials[index];
	return row.mesh == mesh && row.native_surface == surface && row.material == material && row.generation == generation && row.geometry_source.is_valid() &&
			row.geometry_source.get_aabb() == bounds && row.geometry_source.get_format() == format && row.geometry_source.get_primitive() == RSE::PRIMITIVE_TRIANGLES &&
			row.geometry_source.get_vertex_array().is_null() && row.geometry_source.get_vertex_format() == 0;
}
bool ExternalLinkedOwnerTest::bound_material_contact_refuses_uncompiled(uint32_t index) const {
	if (!owner) {
		return false;
	}
	ExternalOutputOwner::BoundMaterialContact contact;
	contact.output_generation = UINT64_MAX;
	contact.mesh_revision = UINT64_MAX;
	contact.native_surface = 123;
	contact.vertex_count = 456;
	const Error result = owner->read_bound_material_contact(index, contact);
	return result != OK && contact.output_generation == UINT64_MAX && contact.mesh_revision == UINT64_MAX &&
			contact.native_surface == 123 && contact.vertex_count == 456 && contact.mesh.is_null() && contact.material.material.is_null();
}
bool ExternalLinkedOwnerTest::selection_source_metadata_matches(uint32_t index, RSE::PrimitiveType primitive, uint64_t format, const AABB &bounds, const Vector4 &uv) const {
	return owner && index < owner->retained_surface_count && owner->generation_materials[index].geometry_source.metadata_matches(primitive, format, bounds, uv);
}
void ExternalLinkedOwnerTest::unprepared_guard_refusal(GDExtensionDeviceExit::ProductionPhysicalContextRef context,
		GDExtensionProductionRequestRefV1 request, const ExternalOutputConfig &config, GDExtensionDeviceExit::ProductionOutputMetadataLease &metadata) {
	CRASH_COND(!empty_storage_mode || owner || metadata.is_empty());
	CHECK(storage->get_external_guard() == nullptr);
	CHECK_FALSE(storage->is_external_guard_prepared());
	const Error expected = storage->get_external_guard_preparation_error();
	REQUIRE(expected == ERR_UNCONFIGURED);
	GDExtensionOutputMetadataLimitsV1 limits{};
	limits.abi = { 1, sizeof(limits), 0 };
	limits.view_lease_capacity = 1;
	limits.read_lease_capacity = 2;
	limits.surface_lease_capacity = 7;
	GDExtensionOutputMetadataRequirementsV1 requirements{};
	CHECK(storage->_external_metadata_requirements(config, limits, requirements) == expected);
	CHECK(requirements.abi.abi_version == 0);
	CHECK(requirements.required_metadata_bytes == 0);
	CHECK(requirements.required_alignment == 0);
	CHECK(requirements.slot_count == 0);
	const void *data = metadata.get_data();
	const uint64_t bytes = metadata.get_size();
	const uint64_t generation_before = storage->next_external_generation;
	GDExtensionOutputCreateResultV1 result{};
	result.abi = { 1, sizeof(result), 0 };
	storage->_external_create_received(context, request, config, metadata, 1, 2, 7, result, true);
	CHECK(result.status.error == expected);
	CHECK(result.particles == 0);
	CHECK(result.generation == 0);
	CHECK(result.capabilities == 0);
	CHECK(metadata.get_data() == data);
	CHECK(metadata.get_size() == bytes);
	CHECK(storage->next_external_generation == generation_before);
	CHECK(storage->particles_owner.get_rid_count() == 0);
	CHECK(owner == nullptr);
}
uint32_t ExternalLinkedOwnerTest::selection_meshes() const {
	CRASH_COND(!exists());
	return owner->retained_mesh_count;
}
uint32_t ExternalLinkedOwnerTest::selection_materials() const {
	CRASH_COND(!exists());
	return owner->retained_material_count;
}
bool ExternalLinkedOwnerTest::selection_faulted() const {
	return exists() && owner->state == ExternalOutputOwner::GenerationState::FAULTED_RETIRING && owner->generation_selection_unpublished;
}
bool ExternalLinkedOwnerTest::selection_inline_matches(const ExternalOutputConfig &config) const {
	if (!exists() || !owner->generation_config_inline || !owner->config_tags_inline || !owner->config.surfaces.is_empty() || !owner->config.surface_materials.is_empty() ||
			owner->generation_mesh_count != uint32_t(config.surfaces.size()) || owner->generation_material_count != uint32_t(config.surface_materials.size()) || owner->binding_capacity != owner->slot_count * config.surface_materials.size() || owner->surface_capacity != 7 || owner->preparation_capacity != owner->read_capacity || !owner->preparation_rows) {
		return false;
	}
	const uintptr_t start = uintptr_t(owner->metadata_lease.get_data()), end = start + owner->metadata_lease.get_size();
	if (uintptr_t(owner->generation_meshes) < start || uintptr_t(owner->generation_meshes + owner->generation_mesh_count) > end || uintptr_t(owner->generation_materials) < start || uintptr_t(owner->generation_materials + owner->generation_material_count) > end || uintptr_t(owner->preparation_rows) < start || uintptr_t(owner->preparation_rows + owner->preparation_capacity) > end) {
		return false;
	}
	for (uint32_t i = 0; i < owner->generation_mesh_count; ++i) {
		if (owner->generation_meshes[i].mesh != config.surfaces[i]) {
			return false;
		}
	}
	for (uint32_t i = 0; i < owner->generation_material_count; ++i) {
		if (owner->generation_materials[i].material != config.surface_materials[i]) {
			return false;
		}
	}
	const PackedByteArray *tags[] = { &config.producer_tag, &config.device_tag, &config.layout_digest, &config.mapping_digest, &config.numeric_profile_digest };
	for (uint32_t i = 0; i < 5; ++i) {
		if (tags[i]->size() != 32 || memcmp(owner->frozen_config_tags[i], tags[i]->ptr(), 32) != 0) {
			return false;
		}
	}
	return true;
}
void ExternalLinkedOwnerTest::preparation_bank_fragmentation(GDExtensionDeviceExit::ProductionOutputPin (&issued)[3]) {
	// The original pin-borrow case runs this one-row ledger check independently
	// of Acquire, native factories and the renderer.
	for (uint32_t requested : { uint32_t(ExternalOutputConfig::PASS_OPAQUE), uint32_t(ExternalOutputConfig::PASS_DEPTH) }) {
		CHECK_FALSE(issued[0].is_empty());
		if (issued[0].is_empty()) {
			break;
		}
		ExternalOutputConfig allowed;
		allowed.profile = ExternalOutputConfig::INSTANCE_192;
		allowed.pass_mask = ExternalOutputConfig::PASS_OPAQUE | ExternalOutputConfig::PASS_DEPTH;
		allowed.max_views = allowed.max_inflight = 1;
		ExternalOutputOwner owner(allowed, 1);
		ExternalOutputOwner::GenerationMaterialSelection source;
		owner.generation_selection_complete = true;
		owner.generation_materials = &source;
		owner.generation_material_count = 1;
		ExternalPipelinePreparation row;
		owner.preparation_rows = &row;
		owner.preparation_capacity = 1;
		auto &slot = owner.slots[0];
		slot.state = ExternalOutputOwner::LeaseState::SEALED_RECORDED;
		slot.request.pass_mask = requested;
		slot.original_pin = std::move(issued[0]);
		slot.lease_serial = 1;
		slot.preparation_reserved_count = 1;
		row.owner = &owner;
		row.owner_slot = 0;
		row.generation = owner.generation;
		row.serial = slot.lease_serial;
		row.bank_groups_reserved = true;

		ExternalPacketPipelineRequest selection;
		selection.pass = ExternalPacketPass::DEPTH;
		uint32_t index = UINT32_MAX;
		bool reused = false;
		if (requested == ExternalOutputConfig::PASS_OPAQUE) {
			CHECK(owner.select_preparation_tuple(0, 0, selection, index, reused) == ERR_UNAVAILABLE);
			CHECK(index == UINT32_MAX);
			CHECK_FALSE(reused);
			CHECK(slot.preparation_selected_count == 0);
			CHECK(row.tuple_state == ExternalPipelinePreparation::TupleState::UNSELECTED);
			selection.pass = ExternalPacketPass::OPAQUE;
		}
		CHECK(owner.select_preparation_tuple(0, 0, selection, index, reused) == OK);
		CHECK(index == 0);
		CHECK_FALSE(reused);
		CHECK(slot.preparation_selected_count == 1);
		CHECK(row.tuple_state == ExternalPipelinePreparation::TupleState::SELECTED);
		CHECK(row.selection == selection);

		issued[0] = std::move(slot.original_pin);
		owner.preparation_rows = nullptr;
		owner.preparation_capacity = 0;
		owner.generation_materials = nullptr;
		owner.generation_material_count = 0;
		owner.generation_selection_complete = false;
		owner.state = ExternalOutputOwner::GenerationState::RELEASED;
	}

	// CPU owner-bank mechanics against actual independently issued original
	// pins. No manufactured pin, native group, scope, selection or completion.
	ExternalOutputConfig config;
	config.max_views = 1;
	config.max_inflight = 3;
	ExternalOutputOwner bank(config, 1);
	ExternalPipelinePreparation rows[5];
	ExternalOutputOwner::SurfaceBinding surfaces[3][2];
	bank.preparation_rows = rows;
	bank.preparation_capacity = 5;
	bank.metadata_directories_placement = true;
	auto expect = [](bool value) { CHECK(value); return value; };
	// Cleanup is outside the exercise lambda: failed checks return to explicit
	// cleanup even in Godot's no-exception REQUIRE configuration.
	auto exercise = [&]() {
		for (uint32_t i = 0; i < 3; ++i) {
			auto &slot = bank.slots[i];
			if (!expect(!issued[i].is_empty())) {
				return;
			}
			slot.original_pin = std::move(issued[i]);
			slot.state = ExternalOutputOwner::LeaseState::RESERVED;
			slot.lease_serial = 10 + i;
			slot.surfaces = surfaces[i];
			slot.surfaces_placement = true;
			slot.surface_count = 2;
		}
		if (!expect(bank.bind_preparation_rows(0) == OK) || !expect(bank.bind_preparation_rows(1) == OK)) {
			return;
		}
		expect(surfaces[0][0].preparation_index == 0 && surfaces[0][1].preparation_index == 1);
		expect(surfaces[1][0].preparation_index == 2 && surfaces[1][1].preparation_index == 3);
		expect(bank.bind_preparation_rows(1) == ERR_ALREADY_IN_USE);
		// Only row 4 is free. A two-row claim must not consume even that row.
		expect(bank.bind_preparation_rows(2) == ERR_OUT_OF_MEMORY);
		for (uint32_t i = 0; i < 2; ++i) {
			expect(!surfaces[2][i].preparation_row && surfaces[2][i].preparation_index == UINT32_MAX);
			expect(bank.preparation_row_matches(0, i, rows[i]));
			expect(bank.preparation_row_matches(1, i, rows[2 + i]));
		}
		expect(!rows[4].owner && !rows[4].serial && !rows[4].generation);
		if (!expect(bank.return_preparation_row(0, 0) == OK) || !expect(bank.bind_preparation_rows(2) == OK)) {
			return;
		}
		// Slot 2 now occupies noncontiguous rows 0 and 4, outside its old
		// slot*surface slice. The still-live slot 0 row 1 is untouched.
		expect(surfaces[2][0].preparation_index == 0 && surfaces[2][1].preparation_index == 4);
		expect(bank.preparation_row_matches(2, 0, rows[0]));
		expect(bank.preparation_row_matches(2, 1, rows[4]));
		expect(bank.preparation_row_matches(0, 1, rows[1]));
		// Swapping both the pointer and its locator still cannot swap surfaces,
		// including this legacy fixture with no selected source geometry.
		std::swap(surfaces[2][0].preparation_row, surfaces[2][1].preparation_row);
		std::swap(surfaces[2][0].preparation_index, surfaces[2][1].preparation_index);
		expect(!bank.preparation_row_matches(2, 0, rows[4]));
		expect(bank.return_preparation_row(2, 0) == ERR_INVALID_DATA);
		std::swap(surfaces[2][0].preparation_row, surfaces[2][1].preparation_row);
		std::swap(surfaces[2][0].preparation_index, surfaces[2][1].preparation_index);
		const auto correct_index = surfaces[2][0].preparation_index;
		surfaces[2][0].preparation_index = 4;
		expect(!bank.preparation_row_matches(2, 0, rows[0]));
		expect(bank.return_preparation_row(2, 0) == ERR_INVALID_DATA);
		surfaces[2][0].preparation_index = correct_index;
		++bank.slots[2].lease_serial;
		expect(!bank.preparation_row_matches(2, 0, rows[0]));
		expect(bank.return_preparation_row(2, 0) == ERR_INVALID_DATA);
		--bank.slots[2].lease_serial;
		// Interleave returns from three slots and reuse earlier holes without
		// disturbing later retained rows or requiring contiguous capacity.
		if (!expect(bank.return_preparation_row(1, 0) == OK) || !expect(bank.return_preparation_row(0, 1) == OK)) {
			return;
		}
		bank.slots[0].retire_preparation_index = 4;
		if (!expect(bank.bind_preparation_rows(0) == OK)) {
			return;
		}
		expect(surfaces[0][0].preparation_index == 1 && surfaces[0][1].preparation_index == 2);
		expect(bank.slots[0].retire_preparation_index == 0);
		expect(bank.preparation_row_matches(1, 1, rows[3]));
		expect(bank.preparation_row_matches(2, 0, rows[0]));
		expect(bank.preparation_row_matches(2, 1, rows[4]));
	};
	exercise();
	for (uint32_t i = 0; i < 3; ++i) {
		auto &slot = bank.slots[i];
		for (uint32_t surface = 0; slot.surfaces && surface < slot.surface_count; ++surface) {
			expect(bank.return_preparation_row(i, surface) == OK);
		}
		// Return each original pin to the caller even after an exercise failure.
		// The original Registry fixture explicitly unpins every surviving pin.
		if (!slot.original_pin.is_empty()) {
			issued[i] = std::move(slot.original_pin);
		}
		slot.surfaces = nullptr;
		slot.surface_count = 0;
	}
	for (const auto &row : rows) {
		expect(!row.owner && row.owner_slot == UINT32_MAX && row.owner_surface == UINT32_MAX && !row.serial && !row.generation);
	}
	bank.preparation_rows = nullptr;
	bank.preparation_capacity = 0;
	bank.metadata_directories_placement = false;
	bank.state = ExternalOutputOwner::GenerationState::RELEASED;
}

bool ExternalLinkedOwnerTest::preparation_rows_match_slots(bool p_probe_duplicate) const {
	if (!exists() || !owner->generation_config_inline || !owner->generation_material_count ||
			!owner->preparation_rows || owner->preparation_capacity != owner->read_capacity) {
		return false;
	}
	if (p_probe_duplicate) {
		// Use only the actual Acquire-installed slot and pin. A duplicate or
		// out-of-range metadata bind must not replace its stable row identities.
		if (owner->bind_preparation_rows(owner->slot_count) != ERR_INVALID_PARAMETER) {
			return false;
		}
		for (uint32_t i = 0; i < owner->slot_count; ++i) {
			if (owner->slots[i].surfaces && owner->bind_preparation_rows(i) != ERR_ALREADY_IN_USE) {
				return false;
			}
		}
	}
	for (uint32_t i = 0; i < owner->preparation_capacity; ++i) {
		const auto &row = owner->preparation_rows[i];
		if (!row.source_sealed || row.max_resources || row.pipeline_pin || row.pipeline_original_root || row.pipeline_owned_group || row.geometry_reserved_group || row.material_reserved_group || row.bank_groups_reserved || row.owned_pipeline_root.is_valid() || row.pipeline_scope ||
				row.pipeline_context.registry_instance || row.pipeline_context.frame_serial || row.native_cache_queried ||
				row.last_error != ERR_UNCONFIGURED || row.selected_geometry != UINT32_MAX || row.created_pipeline.is_valid() || row.geometry[0].is_active() || row.geometry[1].is_active() || row.material.is_active()) {
			return false;
		}
		uint32_t references = 0;
		for (uint32_t slot_index = 0; slot_index < owner->slot_count; ++slot_index) {
			const auto &slot = owner->slots[slot_index];
			for (uint32_t surface_index = 0; slot.surfaces && surface_index < slot.surface_count; ++surface_index) {
				const auto &surface = slot.surfaces[surface_index];
				if (surface.preparation_index != i) {
					continue;
				}
				++references;
				if (!owner->preparation_row_matches(slot_index, surface_index, row) || surface.preparation ||
						surface.pipeline_owned_group || surface.created_pipeline.is_valid() || surface.pipeline_group_returned) {
					return false;
				}
			}
		}
		if (row.owner ? references != 1 : (references || row.owner_slot != UINT32_MAX || row.owner_surface != UINT32_MAX || row.generation || row.serial || row.source_geometry || row.source_material.is_valid() || row.source_material_generation)) {
			return false;
		}
	}
	// Detect out-of-range locators too: the bank-row walk cannot visit them.
	for (uint32_t slot_index = 0; slot_index < owner->slot_count; ++slot_index) {
		const auto &slot = owner->slots[slot_index];
		for (uint32_t surface = 0; slot.surfaces && surface < slot.surface_count; ++surface) {
			if (!slot.surfaces[surface].preparation_row || !owner->preparation_row_matches(slot_index, surface, *slot.surfaces[surface].preparation_row)) {
				return false;
			}
		}
	}
	return true;
}
bool ExternalLinkedOwnerTest::preparation_pipeline_reservations_match(uint32_t p_slot) const {
	if (!owner || p_slot >= owner->slot_count) {
		return false;
	}
	const auto &slot = owner->slots[p_slot];
	if (!slot.surfaces || !slot.surface_count) {
		return false;
	}
	for (uint32_t i = 0; i < slot.surface_count; ++i) {
		const auto &surface = slot.surfaces[i];
		const auto *row = surface.preparation_row;
		if (!row || surface.preparation != row || !row->pipeline_owned_group || row->owned_pipeline_root.is_valid() ||
				!row->source_sealed || !row->max_resources || row->pipeline_pin != &slot.original_pin ||
				row->pipeline_original_root != &row->owned_pipeline_root ||
				row->pipeline_context.registry_instance || row->pipeline_context.frame_serial) {
			return false;
		}
	}
	return true;
}
bool ExternalLinkedOwnerTest::preparation_bank_reservations_match(uint32_t p_slot, uint32_t p_count, uint32_t p_graph) const {
	if (!owner || p_slot >= owner->slot_count || owner->slots[p_slot].preparation_reserved_count != p_count ||
			owner->preparation_capacity != 4 || owner->read_capacity != 4 || p_count != 3 || owner->free_preparation_rows() != 1) {
		return false;
	}
	const auto &slot = owner->slots[p_slot];
	uint32_t found = 0;
	for (uint32_t i = 0; i < owner->preparation_capacity; ++i) {
		const auto &row = owner->preparation_rows[i];
		if (row.owner != owner || row.owner_slot != p_slot) {
			continue;
		}
		++found;
		if (!owner->preparation_bank_row_matches(p_slot, i, row) || !row.source_sealed || row.pipeline_scope || !row.bank_groups_reserved ||
				row.max_resources != p_graph || row.pipeline_pin != &slot.original_pin || row.pipeline_original_root != &row.owned_pipeline_root ||
				!row.pipeline_owned_group || !row.geometry_reserved_group || !row.material_reserved_group || !row.transforms_owned_group || row.owned_pipeline_root.is_valid()) {
			return false;
		}
		if (row.owner_surface == UINT32_MAX && (row.source_geometry || row.source_material.is_valid() || row.source_material_generation)) {
			return false;
		}
	}
	return found == p_count;
}
Error ExternalLinkedOwnerTest::preparation_tuple_select(uint32_t slot, uint32_t surface, const ExternalPacketPipelineRequest &selection, uint32_t &index, bool &reused) {
	return owner->select_preparation_tuple(slot, surface, selection, index, reused);
}
bool ExternalLinkedOwnerTest::preparation_tuple_matches(uint32_t index, const ExternalPacketPipelineRequest &selection) const {
	return owner && index < owner->preparation_capacity && owner->preparation_rows[index].selection == selection;
}
bool ExternalLinkedOwnerTest::preparation_tuple_finished(uint32_t index) const {
	if (!owner || index >= owner->preparation_capacity) {
		return false;
	}
	const auto &row = owner->preparation_rows[index];
	return row.finish_complete && row.geometry_adopted && row.material_adopted && row.pipeline_sealed;
}
RID ExternalLinkedOwnerTest::preparation_tuple_root(uint32_t index) const {
	return owner && index < owner->preparation_capacity ? owner->preparation_rows[index].owned_pipeline_root : RID();
}
uint64_t ExternalLinkedOwnerTest::preparation_bank_group(uint32_t p_slot, uint32_t p_ordinal, uint32_t p_kind) const {
	uint32_t ordinal = 0;
	for (uint32_t i = 0; owner && i < owner->preparation_capacity; ++i) {
		const auto &row = owner->preparation_rows[i];
		if (row.owner == owner && row.owner_slot == p_slot && ordinal++ == p_ordinal) {
			return p_kind == 0 ? row.pipeline_owned_group : (p_kind == 1 ? row.geometry_reserved_group : (p_kind == 2 ? row.material_reserved_group : (p_kind == 3 ? row.transforms_owned_group : 0)));
		}
	}
	return 0;
}
uint64_t ExternalLinkedOwnerTest::preparation_pipeline_group(uint32_t p_slot, uint32_t p_surface) const {
	CRASH_COND(!owner || p_slot >= owner->slot_count || !owner->slots[p_slot].surfaces || p_surface >= owner->slots[p_slot].surface_count);
	const auto &surface = owner->slots[p_slot].surfaces[p_surface];
	return surface.preparation_row ? surface.preparation_row->pipeline_owned_group : surface.pipeline_owned_group;
}
bool ExternalLinkedOwnerTest::preparation_scope_link_matches(uint32_t p_slot, uint32_t p_surface, const GDExtensionDeviceExit::ProductionOutputPreparationScope *p_scope, bool p_sealed) const {
	CRASH_COND(!owner || p_slot >= owner->slot_count || !owner->slots[p_slot].surfaces || p_surface >= owner->slots[p_slot].surface_count);
	const auto &slot = owner->slots[p_slot];
	const auto &surface = slot.surfaces[p_surface];
	const auto *row = surface.preparation_row;
	return row && row == surface.preparation && row->pipeline_scope == p_scope && row->source_sealed == p_sealed &&
			row->pipeline_pin == &slot.original_pin && row->pipeline_original_root == &row->owned_pipeline_root && row->pipeline_owned_group != 0;
}
Error ExternalLinkedOwnerTest::preparation_row_preflight(uint32_t p_slot, uint32_t p_surface) {
	return owner->slots[p_slot].surfaces[p_surface].preparation_row->_pipeline_preflight();
}
Error ExternalLinkedOwnerTest::preparation_row_cleanup(uint32_t p_slot, uint32_t p_surface) {
	return owner->slots[p_slot].surfaces[p_surface].preparation_row->_cleanup_preflight();
}
Error ExternalLinkedOwnerTest::preparation_material_future_plan(uint32_t p_slot, uint32_t p_surface, RID p_shader, RID p_set, MaterialStorage::NativeMaterialFuturePipelinePlan &r_plan) {
	return owner->slots[p_slot].surfaces[p_surface].preparation_row->_material_future_pipeline_plan(p_shader, p_set, r_plan);
}
Error ExternalLinkedOwnerTest::preparation_vertex_array_plan(uint32_t slot, uint32_t surface, MeshStorage::NativeVertexArrayPlan &geometry) {
	if (!owner || slot >= owner->slot_count || !owner->slots[slot].surfaces || surface >= owner->slots[slot].surface_count) {
		return ERR_INVALID_PARAMETER;
	}
	auto *row = owner->slots[slot].surfaces[surface].preparation_row;
	if (!row || !row->source_geometry) {
		return ERR_UNAVAILABLE;
	}
	return MeshStorage::mesh_surface_hold_plan_vertex_array(*row->source_geometry, 1, false, false, 0, geometry);
}
Error ExternalLinkedOwnerTest::preparation_aggregate(uint32_t slot, uint32_t surface, RID shader, RenderingDevice::FramebufferFormatID format, MaterialStorage::NativeMaterialFuturePipelinePlan &out, RID uniform_set) {
	MeshStorage::NativeVertexArrayPlan geometry;
	Error error = preparation_vertex_array_plan(slot, surface, geometry);
	if (error != OK) {
		return error;
	}
	auto *row = owner->slots[slot].surfaces[surface].preparation_row;
	return row->_prospective_admission(shader, 1, geometry, uniform_set, format, RenderingDevice::RENDER_PRIMITIVE_TRIANGLES,
			RenderingDevice::PipelineRasterizationState(), RenderingDevice::PipelineMultisampleState(),
			RenderingDevice::PipelineDepthStencilState(), RenderingDevice::PipelineColorBlendState(), 0, {}, out);
}
// Storage-level genuine prefix only: no backend material cast or draw executor.
Error ExternalLinkedOwnerTest::preparation_storage_prefix(uint32_t slot, uint32_t surface, RID shader, RenderingDevice::FramebufferFormatID format, bool with_material, RID uniform_set) {
	MeshStorage::NativeVertexArrayPlan geometry;
	Error error = preparation_vertex_array_plan(slot, surface, geometry);
	if (error != OK) {
		return error;
	}
	auto *row = owner->slots[slot].surfaces[surface].preparation_row;
	MaterialStorage::NativeMaterialFuturePipelinePlan material;
	error = row->_prospective_admission(shader, 1, geometry, uniform_set, format, RenderingDevice::RENDER_PRIMITIVE_TRIANGLES,
			RenderingDevice::PipelineRasterizationState(), RenderingDevice::PipelineMultisampleState(),
			RenderingDevice::PipelineDepthStencilState(), RenderingDevice::PipelineColorBlendState(), 0, {}, material);
	if (error != OK) {
		return error;
	}
	error = MeshStorage::mesh_surface_hold_derive(*row->source_geometry, 1, false, false, 0, row->geometry[0]);
	if (error != OK) {
		return error;
	}
	error = row->_prospective_execution_geometry_matches(geometry);
	if (error != OK) {
		return error;
	}
	const RID pipeline = row->_create_pipeline(shader, format, row->geometry[0].get_vertex_format(), RenderingDevice::RENDER_PRIMITIVE_TRIANGLES,
			RenderingDevice::PipelineRasterizationState(), RenderingDevice::PipelineMultisampleState(),
			RenderingDevice::PipelineDepthStencilState(), RenderingDevice::PipelineColorBlendState(), 0, 0, {});
	if (pipeline.is_null()) {
		return row->last_error;
	}
	return with_material ? MaterialStorage::get_singleton()->material_native_hold_acquire(row->source_material, shader, pipeline, uniform_set,
			row->material, row->max_resources, row->source_material_generation, &material) : OK;
}
RID ExternalLinkedOwnerTest::preparation_row_factory(uint32_t p_slot, uint32_t p_surface, RID p_shader, RenderingDevice::FramebufferFormatID p_format) {
	return owner->slots[p_slot].surfaces[p_surface].preparation_row->_create_pipeline(p_shader, p_format, RenderingDevice::INVALID_ID,
			RenderingDevice::RENDER_PRIMITIVE_TRIANGLES, RenderingDevice::PipelineRasterizationState(), RenderingDevice::PipelineMultisampleState(),
			RenderingDevice::PipelineDepthStencilState(), RenderingDevice::PipelineColorBlendState(), 0, 0, {});
}
Error ExternalLinkedOwnerTest::cancel_pipeline_slot(uint32_t p_slot) {
	CRASH_COND(!owner);
	return owner->cancel_slot(p_slot);
}
Error ExternalLinkedOwnerTest::return_pipeline_row(uint32_t p_slot, uint32_t p_surface) {
	CRASH_COND(!owner);
	return owner->return_preparation_row(p_slot, p_surface);
}
Error ExternalLinkedOwnerTest::prepare_pipeline_prefix(GDExtensionProductionPhysicalContextRefV1 p_context, uint32_t p_slot, uint32_t p_surface, uint32_t p_max_resources) {
	CRASH_COND(!exists() || p_slot >= owner->slot_count || !owner->slots[p_slot].surfaces || p_surface >= owner->slots[p_slot].surface_count);
	auto *row = owner->slots[p_slot].surfaces[p_surface].preparation_row;
	CRASH_COND(!row);
	// Locate only the row installed by the actual Acquire. Do not open it,
	// manufacture a pass, or change the original action's factory authority.
	return storage->_external_pipeline_prepare_owned_prefix(p_context, particles, p_slot, p_surface, *row, p_max_resources);
}
uint64_t ExternalLinkedOwnerTest::selection_material_generation(uint32_t index) const {
	CRASH_COND(!exists() || index >= owner->generation_material_count);
	return owner->generation_materials[index].generation;
}
void ExternalLinkedOwnerTest::forget_auto_returned() {
	CRASH_COND(!empty_storage_mode || storage->particles_owner.get_or_null(particles));
	owner = nullptr;
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
	return !owner->view_cells && !owner->read_cells && !owner->surface_cells && !owner->generation_meshes && !owner->generation_materials &&
			!owner->view_capacity && !owner->read_capacity && !owner->surface_capacity && !owner->preparation_rows && !owner->preparation_capacity;
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
	auto &surface = owner->slots[0].surfaces[0];
	return surface.preparation_row ? surface.preparation_row->owned_pipeline_root : surface.created_pipeline;
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
	static void read_receipt_layout() {
		Table table;
		auto &o = table.owner;
		ExternalOutputOwner::MetadataLayout one, two;
		REQUIRE(ExternalOutputOwner::plan_metadata_layout(0, 1, 0, one, 0, 0, 0, 0, 0) == OK);
		REQUIRE(ExternalOutputOwner::plan_metadata_layout(0, 2, 0, two, 0, 0, 0, 0, 0) == OK);
		CHECK(two.total_bytes - one.total_bytes == sizeof(ExternalOutputOwner::ReadCell));
		CHECK(ExternalOutputOwner::ReadCell::GRAPH_CAPACITY >= 32 + 6);
		o.slots[0].read_settle_cursor = 63;
		o.slots[1].read_settle_cursor = 127;
		CHECK(o.slots[0].read_settle_cursor == 63);
		CHECK(o.slots[1].read_settle_cursor == 127);
	}
	static void preparation_bank_layout() {
		using Owner = ExternalOutputOwner;
		Owner::MetadataLayout bank;
		// No surface/storage slice is needed to pay finite tuple metadata.
		REQUIRE(Owner::plan_metadata_layout(0, 0, 0, bank, 0, 0, 0, 0, 3) == OK);
		CHECK(bank.binding_count == 0);
		CHECK(bank.storage_count == 0);
		CHECK(bank.preparation_count == 3);
		const uint64_t start = (uint64_t(sizeof(Owner)) + alignof(ExternalPipelinePreparation) - 1) & ~(uint64_t(alignof(ExternalPipelinePreparation)) - 1);
		CHECK(bank.preparation_offset == start);
		CHECK(bank.total_bytes == start + 3 * sizeof(ExternalPipelinePreparation));
		Owner::MetadataLayout no_bank;
		REQUIRE(Owner::plan_metadata_layout(0, 0, 0, no_bank, 0, 0, 3, 0, 0) == OK);
		CHECK(no_bank.binding_count == 3);
		CHECK(no_bank.preparation_count == 0);
		CHECK(no_bank.preparation_offset == 0);
		const uint64_t bindings = (uint64_t(sizeof(Owner)) + alignof(Owner::SurfaceBinding) - 1) & ~(uint64_t(alignof(Owner::SurfaceBinding)) - 1);
		CHECK(no_bank.total_bytes == bindings + 3 * sizeof(Owner::SurfaceBinding));
		const uint32_t maximum = uint32_t((UINT32_MAX - start) / sizeof(ExternalPipelinePreparation));
		Owner::MetadataLayout boundary;
		REQUIRE(Owner::plan_metadata_layout(0, 0, 0, boundary, 0, 0, 0, 0, maximum) == OK);
		CHECK(boundary.preparation_count == maximum);
		CHECK(boundary.total_bytes == start + uint64_t(maximum) * sizeof(ExternalPipelinePreparation));
		const Owner::MetadataLayout accepted = bank;
		CHECK(Owner::plan_metadata_layout(0, 0, 0, bank, 0, 0, 0, 0, maximum + 1) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(bank.total_bytes == accepted.total_bytes);
		CHECK(bank.alignment == accepted.alignment);
		CHECK(bank.preparation_count == accepted.preparation_count);
		CHECK(bank.preparation_offset == accepted.preparation_offset);
		CHECK(bank.binding_count == accepted.binding_count);
		CHECK(bank.surface_binding_offset == accepted.surface_binding_offset);
		CHECK(bank.view_count == accepted.view_count);
		CHECK(bank.read_count == accepted.read_count);
		CHECK(bank.surface_count == accepted.surface_count);
		CHECK(bank.view_offset == accepted.view_offset);
		CHECK(bank.read_offset == accepted.read_offset);
		CHECK(bank.surface_offset == accepted.surface_offset);
		CHECK(bank.storage_count == accepted.storage_count);
		CHECK(bank.storage_offset == accepted.storage_offset);
		CHECK(bank.mesh_selection_count == accepted.mesh_selection_count);
		CHECK(bank.material_selection_count == accepted.material_selection_count);
		CHECK(bank.mesh_selection_offset == accepted.mesh_selection_offset);
		CHECK(bank.material_selection_offset == accepted.material_selection_offset);
	}
	static void generation_selection_layout() {
		using Owner = ExternalOutputOwner;
		Owner::MetadataLayout base, selected;
		REQUIRE(Owner::plan_metadata_layout(0, 0, 0, base) == OK);
		CHECK(base.total_bytes == sizeof(Owner));
		// The holder owns the three RID values; its real row size is quoted,
		// including those bytes rather than only a dynamic Vector wrapper.
		CHECK(sizeof(RD::NativeImmutableHold) >= sizeof(Vector<RID>) + 3 * sizeof(RID) + sizeof(uint32_t));
		Owner::MetadataLayout one_source;
		REQUIRE(Owner::plan_metadata_layout(0, 0, 0, one_source, 0, 1) == OK);
		const uint64_t source_begin = (uint64_t(base.total_bytes) + alignof(Owner::GenerationMaterialSelection) - 1) & ~(uint64_t(alignof(Owner::GenerationMaterialSelection)) - 1);
		CHECK(one_source.material_selection_offset == source_begin);
		CHECK(one_source.total_bytes == source_begin + sizeof(Owner::GenerationMaterialSelection));
		CHECK(sizeof(Owner::GenerationMaterialSelection) >= sizeof(MeshStorage::NativeSurfaceHold));
		CHECK(sizeof(MeshStorage::NativeSurfaceHold) >= sizeof(RD::NativeImmutableHold));
		REQUIRE(Owner::plan_metadata_layout(1, 2, 7, selected, 2, 3, 12, 4) == OK);
		CHECK(selected.view_count == 1);
		CHECK(selected.read_count == 2);
		CHECK(selected.surface_count == 7);
		CHECK(selected.binding_count == 12);
		CHECK(selected.preparation_count == 12);
		CHECK(selected.storage_count == 4);
		CHECK(selected.mesh_selection_count == 2);
		CHECK(selected.material_selection_count == 3);
		// Each exact next offset catches both short and oversized native slices.
		uint64_t end = sizeof(Owner);
		auto slice = [&](uint32_t offset, uint32_t count, size_t stride, size_t alignment) {
			const uint64_t expected = (end + alignment - 1) & ~(uint64_t(alignment) - 1);
			CHECK(offset % alignment == 0);
			CHECK(offset == expected);
			CHECK(end <= offset);
			end = uint64_t(offset) + uint64_t(count) * stride;
			CHECK(end <= selected.total_bytes);
		};
		slice(selected.view_offset, 1, sizeof(Owner::ViewCell), alignof(Owner::ViewCell));
		slice(selected.read_offset, 2, sizeof(Owner::ReadCell), alignof(Owner::ReadCell));
		slice(selected.storage_offset, 4, sizeof(ExternalOutputStorage), alignof(ExternalOutputStorage));
		slice(selected.surface_binding_offset, 12, sizeof(Owner::SurfaceBinding), alignof(Owner::SurfaceBinding));
		slice(selected.preparation_offset, 12, sizeof(ExternalPipelinePreparation), alignof(ExternalPipelinePreparation));
		slice(selected.mesh_selection_offset, 2, sizeof(Owner::GenerationMeshSelection), alignof(Owner::GenerationMeshSelection));
		slice(selected.material_selection_offset, 3, sizeof(Owner::GenerationMaterialSelection), alignof(Owner::GenerationMaterialSelection));
		slice(selected.surface_offset, 7, sizeof(Owner::SurfaceCell), alignof(Owner::SurfaceCell));
		CHECK(selected.guard_proof_bytes == 4 * 240 + 12 * 144);
		slice(selected.guard_proof_offset, selected.guard_proof_bytes, 1, alignof(uint32_t));
		CHECK(end == selected.total_bytes);
		const Owner::MetadataLayout accepted = selected;
		auto unchanged = [&]() {
			CHECK(selected.view_count == accepted.view_count);
			CHECK(selected.read_count == accepted.read_count);
			CHECK(selected.surface_count == accepted.surface_count);
			CHECK(selected.view_offset == accepted.view_offset);
			CHECK(selected.read_offset == accepted.read_offset);
			CHECK(selected.surface_offset == accepted.surface_offset);
			CHECK(selected.storage_offset == accepted.storage_offset);
			CHECK(selected.surface_binding_offset == accepted.surface_binding_offset);
			CHECK(selected.preparation_offset == accepted.preparation_offset);
			CHECK(selected.total_bytes == accepted.total_bytes);
			CHECK(selected.alignment == accepted.alignment);
			CHECK(selected.mesh_selection_count == accepted.mesh_selection_count);
			CHECK(selected.material_selection_count == accepted.material_selection_count);
			CHECK(selected.mesh_selection_offset == accepted.mesh_selection_offset);
			CHECK(selected.material_selection_offset == accepted.material_selection_offset);
			CHECK(selected.binding_count == accepted.binding_count);
			CHECK(selected.storage_count == accepted.storage_count);
			CHECK(selected.preparation_count == accepted.preparation_count);
		};
		CHECK(Owner::plan_metadata_layout(0, 0, 0, selected, UINT32_MAX, 0) == ERR_PARAMETER_RANGE_ERROR);
		unchanged();
		CHECK(Owner::plan_metadata_layout(0, 0, 0, selected, 0, UINT32_MAX) == ERR_PARAMETER_RANGE_ERROR);
		unchanged();
		CHECK(Owner::plan_metadata_layout(0, 0, 0, selected, 0, 0, UINT32_MAX - 1, 0) == ERR_PARAMETER_RANGE_ERROR);
		unchanged();
		CHECK(Owner::plan_metadata_layout(0, 0, 0, selected, 0, 0, 0, UINT32_MAX - 1) == ERR_PARAMETER_RANGE_ERROR);
		unchanged();
		// Largest accepted and first rejected counts for each new native row.
		auto boundary = [&](bool meshes, size_t stride, size_t alignment) {
			const uint64_t offset = (uint64_t(sizeof(Owner)) + alignment - 1) & ~(uint64_t(alignment) - 1);
			const uint32_t maximum = uint32_t((UINT32_MAX - offset) / stride);
			Owner::MetadataLayout result;
			REQUIRE(Owner::plan_metadata_layout(0, 0, 0, result, meshes ? maximum : 0, meshes ? 0 : maximum) == OK);
			CHECK(result.total_bytes == offset + uint64_t(maximum) * stride);
			selected = accepted;
			CHECK(Owner::plan_metadata_layout(0, 0, 0, selected, meshes ? maximum + 1 : 0, meshes ? 0 : maximum + 1) == ERR_PARAMETER_RANGE_ERROR);
			unchanged();
		};
		boundary(true, sizeof(Owner::GenerationMeshSelection), alignof(Owner::GenerationMeshSelection));
		boundary(false, sizeof(Owner::GenerationMaterialSelection), alignof(Owner::GenerationMaterialSelection));
		// Preparation rows are charged for every slot/surface binding, even
		// before a native pass is selected. Dropping that slice underquotes
		// the last legal block and accepts the first overflowing block.
		const uint64_t binding_start = (uint64_t(sizeof(Owner)) + alignof(Owner::SurfaceBinding) - 1) & ~uint64_t(alignof(Owner::SurfaceBinding) - 1);
		uint32_t maximum = uint32_t((UINT32_MAX - binding_start) / (sizeof(Owner::SurfaceBinding) + sizeof(ExternalPipelinePreparation)));
		auto required = [&](uint32_t count) {
			const uint64_t binding_end = binding_start + uint64_t(count) * sizeof(Owner::SurfaceBinding);
			const uint64_t preparation_start = (binding_end + alignof(ExternalPipelinePreparation) - 1) & ~uint64_t(alignof(ExternalPipelinePreparation) - 1);
			return preparation_start + uint64_t(count) * sizeof(ExternalPipelinePreparation);
		};
		if (required(maximum) > UINT32_MAX) {
			--maximum;
		}
		Owner::MetadataLayout finite;
		REQUIRE(Owner::plan_metadata_layout(0, 0, 0, finite, 0, 0, maximum, 0) == OK);
		CHECK(finite.total_bytes == required(maximum));
		selected = accepted;
		CHECK(Owner::plan_metadata_layout(0, 0, 0, selected, 0, 0, maximum + 1, 0) == ERR_PARAMETER_RANGE_ERROR);
		unchanged();
	}

	static void metadata_layout() {
		using Layout = ExternalOutputOwner::MetadataLayout;
		Layout zero;
		REQUIRE(ExternalOutputOwner::plan_metadata_layout(0, 0, 0, zero) == OK);
		CHECK(zero.total_bytes == sizeof(ExternalOutputOwner));
		CHECK(zero.view_offset == 0);
		CHECK(zero.read_offset == 0);
		CHECK(zero.surface_offset == 0);
		CHECK(zero.preparation_offset == 0);
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
		CHECK(layout.guard_proof_bytes == ExternalOutputOwner::MAX_SLOTS * 240 + 4 * 144);
		CHECK(layout.guard_proof_offset >= layout.surface_offset + 4 * sizeof(ExternalOutputOwner::SurfaceCell));
		CHECK(layout.total_bytes == layout.guard_proof_offset + layout.guard_proof_bytes);
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
			// Component fixture supplies an actual Registry pin and retained
			// view cell. This is not evidence of a production view issuer.
			o.device = RenderingDevice::get_singleton();
			slot.state = ExternalOutputOwner::LeaseState::SEALED_RECORDED;
			ExternalPacketBinding reserved, second;
			const Error reservation = o.read_reserve_from_view(moved, reserved);
			CHECK(reservation == OK);
			if (reservation == OK) {
				CHECK_FALSE(reserved.is_empty());
				CHECK(o.read_cells[0].state == ExternalOutputOwner::ReadCell::State::RESERVED);
				CHECK_FALSE(o.read_cells[0].pin.is_empty());
				CHECK(reserved.get_pipeline().is_null());
				CHECK(reserved.get_vertex_array().is_null());
				CHECK(o.read_reserve_from_view(moved, reserved) == ERR_ALREADY_IN_USE);
				CHECK(o.read_reserve_from_view(moved, second) == ERR_OUT_OF_MEMORY);
				CHECK(second.is_empty());
				if (!second.is_empty()) {
					CHECK(second.cancel_before_draw() == OK);
				}
				// Independent capture-custody substrate, not a production selected-view
				// admission. The existing fixture supplies an actual original pin.
				auto expect = [](bool value) { CHECK(value); return value; };
				RID viewport, camera;
				// Failed setup exits only this lambda. Native resources and the
				// reserved original-pin borrow are always cleaned up below.
				auto exercise_capture = [&]() {
					if (!expect(RSG::viewport != nullptr) || !expect(RSG::scene != nullptr)) {
						return;
					}
					viewport = RSG::viewport->viewport_allocate();
					if (!expect(viewport.is_valid())) {
						return;
					}
					RSG::viewport->viewport_initialize(viewport);
					camera = RSG::scene->camera_allocate();
					if (!expect(camera.is_valid())) {
						return;
					}
					RSG::scene->camera_initialize(camera);
					RSG::viewport->viewport_set_size(viewport, 64, 48, 1);
					RSG::viewport->viewport_attach_camera(viewport, camera);
					auto *native = RSG::viewport->viewport_owner.get_or_null(viewport);
					if (!expect(native != nullptr)) {
						return;
					}
					if (!expect(native->render_buffers.is_null()) || !expect(RSG::viewport->viewport_register_presentation_key(viewport, { 4098, 3, 7 }))) {
						return;
					}
					RendererSceneRender::CameraData data{};
					data.set_camera(Transform3D(Basis(), Vector3(1, 2, 3)), Projection(), false, false, Vector2(), 0, 1);
					data.source_viewport = viewport;
					slot.request.view_epoch = 3;
					slot.request.common.present_seq = 7;
					// The copied payload and token are valid. This invocation has no buffers.
					const auto payload_token = RSG::viewport->viewport_begin_camera_capture(viewport, data);
					RendererSceneRender::CameraData copied{};
					if (!expect(payload_token.is_valid()) || !expect(RSG::viewport->viewport_get_camera_capture(viewport, payload_token, copied))) {
						return;
					}
					CHECK(copied.main_transform == data.main_transform);
					slot.request.common.current_camera_token = payload_token;
					CHECK(o.read_retain_native_camera(reserved, *RSG::viewport, viewport) == ERR_INVALID_DATA);
					CHECK_FALSE(o.read_cells[0].camera_retained);
					if (o.read_cells[0].camera_retained) {
						return;
					}
					native->render_buffers = Ref<RenderSceneBuffers>(memnew(ReadCaptureTestBuffers));
					if (!expect(native->render_buffers.is_valid())) {
						return;
					}
					slot.request.common.current_camera_token = RSG::viewport->viewport_begin_camera_capture(viewport, data, camera, native->render_buffers);
					const auto captured = slot.request.common.current_camera_token;
					if (!expect(captured.is_valid())) {
						return;
					}
					++slot.request.common.present_seq;
					CHECK(o.read_retain_native_camera(reserved, *RSG::viewport, viewport) == ERR_INVALID_DATA);
					--slot.request.common.present_seq;
					if (!expect(o.read_retain_native_camera(reserved, *RSG::viewport, viewport) == OK)) {
						return;
					}
					CHECK(o.read_retain_native_camera(reserved, *RSG::viewport, viewport) == ERR_ALREADY_IN_USE);
					auto &retained = o.read_cells[0];
					CHECK(retained.camera_origin[0] == 1.0f);
					CHECK(retained.camera_forward[2] == -1.0f);
					data.main_transform.origin = Vector3(9, 8, 7);
					if (!expect(RSG::viewport->viewport_begin_camera_capture(viewport, data, camera, native->render_buffers).is_valid())) {
						return;
					}
					CHECK(retained.camera_token == captured);
					CHECK(retained.camera_origin[0] == 1.0f);
				};
				exercise_capture();
				if (viewport.is_valid()) {
					RSG::viewport->free(viewport);
				}
				if (camera.is_valid()) {
					RSG::scene->free(camera);
				}
				auto &retained = o.read_cells[0];
				if (retained.camera_retained) {
					CHECK(retained.camera_invocation.render_buffers.is_valid());
					CHECK(retained.camera_origin[0] == 1.0f);
				}
				if (retained.camera_retained) {
					// Observe identity without adding an owning Ref.
					auto *original_buffers = retained.camera_invocation.render_buffers.ptr();
					// No concurrent owner access: the original RT waits for this
					// worker before inspecting either wrapper or cell. The genuine
					// Registry rejects unpin on the wrong execution thread.
#ifdef THREADS_ENABLED
					struct FailedReturn {
						ExternalPacketBinding *binding;
						Error result = OK;
					} failed{ &reserved };
					Thread worker;
					worker.start([](void *p_data) {
						auto &attempt = *static_cast<FailedReturn *>(p_data);
						attempt.result = attempt.binding->cancel_before_draw();
					}, &failed);
					worker.wait_to_finish();
					CHECK(failed.result == ERR_UNAUTHORIZED);
					CHECK_FALSE(reserved.is_empty());
					CHECK(retained.consumer == &reserved);
					CHECK(retained.state == ExternalOutputOwner::ReadCell::State::RESERVED);
					CHECK_FALSE(retained.pin.is_empty());
					CHECK(retained.camera_retained);
					CHECK(retained.camera_invocation.render_buffers.is_valid());
					CHECK(retained.camera_invocation.render_buffers.ptr() == original_buffers);
					CHECK(retained.camera_origin[0] == 1.0f);
#endif
					const uint64_t original_serial = retained.serial;
					ExternalPacketBinding camera_moved(std::move(reserved));
					CHECK(reserved.is_empty());
					CHECK(retained.consumer == &camera_moved);
					CHECK(retained.camera_invocation.render_buffers.ptr() == original_buffers);
					CHECK(o.read_return(camera_moved, true) == OK);
					CHECK(camera_moved.is_empty());
					CHECK(retained.consumer == nullptr);
					CHECK(retained.serial == original_serial);
					CHECK(retained.state == ExternalOutputOwner::ReadCell::State::UNKNOWN);
					CHECK_FALSE(retained.pin.is_empty());
					CHECK(retained.camera_retained);
					CHECK(retained.camera_invocation.render_buffers.ptr() == original_buffers);
					CHECK(retained.camera_origin[0] == 1.0f);
					// Test-only undo of the UNKNOWN input above: no native draw
					// happened. This is not a production settlement implementation.
					retained.state = ExternalOutputOwner::ReadCell::State::RESERVED;
					retained.consumer = &reserved;
					reserved.owner = &o;
					reserved.serial = original_serial;
					reserved.slot = retained.slot;
					reserved.read_cell = 0;
				}
				CHECK(reserved.cancel_before_draw() == OK);
				CHECK(reserved.is_empty());
				CHECK(o.read_cells[0].pin.is_empty());
				CHECK_FALSE(retained.camera_retained);
				CHECK(retained.camera_invocation.render_buffers.is_null());
			}
			const auto next_read = o.next_read_serial;
			o.next_read_serial = 0;
			CHECK(o.read_reserve_from_view(moved, second) == ERR_PARAMETER_RANGE_ERROR);
			CHECK(second.is_empty());
			if (!second.is_empty()) {
				CHECK(second.cancel_before_draw() == OK);
			}
			o.next_read_serial = next_read;
			++moved.serial;
			CHECK(o.read_reserve_from_view(moved, second) == ERR_UNAUTHORIZED);
			if (!second.is_empty()) {
				CHECK(second.cancel_before_draw() == OK);
			}
			--moved.serial;
			o.withdraw();
			CHECK(o.read_reserve_from_view(moved, second) == ERR_UNAUTHORIZED);
			CHECK(second.is_empty());
			if (!second.is_empty()) {
				CHECK(second.cancel_before_draw() == OK);
			}
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
			CHECK(moved.record_checked(0, nullptr, 4) == ERR_UNAVAILABLE);
			CHECK_FALSE(cell.raster_bound);
			CHECK_FALSE(cell.list_linked);
			CHECK(cell.transfer_count == 0);
			CHECK(cell.state == ExternalOutputOwner::ReadCell::State::RESERVED);
			// Existing original pin: RESERVED alone cannot override a captured
			// graph obligation. A bool/no-use cancellation never returns it.
			cell.graph_count = 1;
			CHECK(moved.cancel_before_draw() == ERR_BUSY);
			CHECK_FALSE(cell.pin.is_empty());
			CHECK(cell.consumer == &moved);
			cell.graph_count = 0;
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
				// Synthetic source-data sentinel tests only the return guard; it
				// does not assert capture admission or actual native completion.
				cell.camera_retained = true;
				cell.camera_origin[0] = 17.0f;
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
				// UNKNOWN retains real custody, but the synthetic return-state
				// sentinel is never accepted as native adoption/consumption.
				const GDExtensionOutputTokenV1 no_consumption{};
				CHECK(slot.native_color_consumption_serial == 0);
				CHECK_FALSE(o.native_color_consumption_matches(0, no_consumption));
				CHECK(moved.is_empty());
				CHECK(cell.state == ExternalOutputOwner::ReadCell::State::UNKNOWN);
				CHECK(cell.camera_retained);
				CHECK(cell.camera_origin[0] == 17.0f);
				CHECK_FALSE(cell.pin.is_empty());
				CHECK(o.slot_has_custody(0));
				CHECK(o.return_slot_pin(0) == ERR_BUSY);
				CHECK(o.return_reserved_cells() == ERR_BUSY);
				// There is no production UNKNOWN settlement API. Extract test
				// wiring back to the real fixture, which returns this actual pin
				// through Registry; do not claim owner/native completion here.
				issued = std::move(cell.pin);
				// Remove only the synthetic sentinel after original pin extraction.
				cell.camera_retained = false;
				cell.camera_origin[0] = 0;
				cell.state = ExternalOutputOwner::ReadCell::State::UNUSED;
			} else {
				CHECK(moved.cancel_before_draw() == OK);
				// Actual original-pin known-no-use return must not issue a
				// native color consumption receipt, even though return succeeds.
				const GDExtensionOutputTokenV1 no_consumption{};
				CHECK(slot.native_color_consumption_serial == 0);
				CHECK(slot.native_color_read_serial == 0);
				CHECK(slot.native_color_close_graph == UINT32_MAX);
				CHECK(memcmp(slot.native_color_consumption_token.bytes, no_consumption.bytes, 32) == 0);
				CHECK_FALSE(o.native_color_consumption_matches(0, no_consumption));
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

TEST_CASE("[B4][EngineGuardLifecycle] CPU component storage keeps engine preparation unavailable") {
	RenderingDevice device;
	{
		RendererRD::ExternalLinkedOwnerTest fixture(device, true);
		const RendererRD::ParticlesStorage *storage = RendererRD::ParticlesStorage::get_singleton();
		REQUIRE(storage != nullptr);
		CHECK(storage->get_external_guard() == nullptr);
		CHECK(storage->get_external_guard_preparation_error() == ERR_UNCONFIGURED);
		CHECK_FALSE(storage->is_external_guard_prepared());
	}
	CHECK(RendererRD::ParticlesStorage::get_singleton() == nullptr);
}

TEST_CASE("[B4][PreparationBankLayout] tuple metadata count is independent and bounded before placement") {
	RendererRD::TestExternalOutputOwnerStateAccess::preparation_bank_layout();
}

TEST_CASE("[B4][GenerationSelectionLayout] original mesh and material lease rows are separate checked native slices") {
	RendererRD::TestExternalOutputOwnerStateAccess::generation_selection_layout();
}

TEST_CASE("[B4][SceneOnlyRouting] original metadata profile is distinct from zero-surface render profiles") {
	RendererRD::ExternalLinkedOwnerTest::scene_only_profile_query();
}

TEST_CASE("[B4][ReadReceiptLayout] original read graph and transfer bookkeeping is paid and slot scans are independent") {
	RendererRD::TestExternalOutputOwnerStateAccess::read_receipt_layout();
}
