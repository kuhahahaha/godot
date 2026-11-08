/**************************************************************************/
/*  test_mesh_storage_external.cpp                                        */
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

#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
#include "tests/test_macros.h"

#include <type_traits>

TEST_FORCE_LINK(test_mesh_storage_external)

TEST_CASE("[B4][K05] external surface descriptor requires distinct generations") {
	RendererRD::MeshStorage::ExternalSurfaceDescriptor descriptor;
	CHECK(descriptor.vertex_count == 0);
	CHECK(descriptor.index_count == 0);
	CHECK(descriptor.generation == 0);
	CHECK(descriptor.current_allocation_generation == 0);
	CHECK(descriptor.previous_allocation_generation == 0);
	CHECK(descriptor.index_array.is_null());
	descriptor.current_allocation_generation = 7;
	descriptor.previous_allocation_generation = 8;
	CHECK(descriptor.current_allocation_generation != descriptor.previous_allocation_generation);
}

#include "servers/rendering/renderer_rd/forward_clustered/scene_shader_forward_clustered.h"
#include "servers/rendering/renderer_rd/forward_mobile/scene_shader_forward_mobile.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"

class Indexed144FormatDriver : public TransferTestDriverBase {
public:
	uint8_t memory[32][512] = {};
	bool live[32] = {};
	uint32_t allocations = 0, frees = 0;
	BufferID buffer_create(uint64_t bytes, BitField<BufferUsageBits>, MemoryAllocationType, uint64_t) override {
		CHECK(bytes <= 512);
		CHECK(allocations + 1 < 32);
		if (bytes > 512 || allocations + 1 >= 32) {
			return {};
		}
		const uint32_t id = ++allocations;
		live[id] = true;
		for (uint32_t i = 0; i < bytes; ++i) {
			memory[id][i] = uint8_t(id * 17 + i);
		}
		return BufferID(id);
	}
	void buffer_free(BufferID id) override {
		CHECK((id.id > 0 && id.id < 32 && live[id.id]));
		if (!id.id || id.id >= 32 || !live[id.id]) {
			return;
		}
		live[id.id] = false;
		++frees;
	}
	void set_object_name(ObjectType, ID, const String &) override {}
	bool fail_format = false;
	uint32_t formats = 0;
	Vector<VertexAttribute> last_attributes;
	VertexFormatID vertex_format_create(Span<VertexAttribute> p_attributes, const VertexAttributeBindingsMap &) override {
		if (fail_format) {
			return VertexFormatID();
		}
		last_attributes.clear();
		for (const auto &attribute : p_attributes) {
			last_attributes.push_back(attribute);
		}
		return VertexFormatID(++formats);
	}
};

class TestIndexed144DeviceAccess {
public:
	static void attach(RenderingDevice &p_device, RenderingDeviceDriver *p_driver) { p_device.driver = p_driver; }
	static RID buffer(RenderingDevice &p_device, uint64_t p_id) {
		RenderingDevice::Buffer buffer;
		buffer.driver_id = RenderingDeviceDriver::BufferID(p_id);
		buffer.size = 144 * 3;
		return p_device.vertex_buffer_owner.make_rid(buffer);
	}
	static uint64_t binding(RenderingDevice &p_device, RID p_array, uint32_t p_binding) { return p_device.vertex_array_owner.get_or_null(p_array)->buffers[p_binding].id; }
	static void size(RenderingDevice &p_device, RID p_buffer, uint32_t p_size) { p_device.vertex_buffer_owner.get_or_null(p_buffer)->size = p_size; }
	static uint32_t arrays(RenderingDevice &p_device) { return p_device.vertex_array_owner.get_owned_list().size(); }
	static bool owns(RenderingDevice &p_device, RID p_buffer) { return p_device.vertex_buffer_owner.owns(p_buffer); }
	static void begin_native(RenderingDevice &device, Indexed144FormatDriver &driver) {
		device.driver = &driver;
		device.is_main_instance = true;
		device.frames.resize(1);
	}
	static void end_native(RenderingDevice &device) {
		device._free_pending_resources(0);
		CHECK(device.vertex_buffer_owner.get_rid_count() == 0);
		CHECK(device.index_buffer_owner.get_rid_count() == 0);
		CHECK(device.vertex_array_owner.get_rid_count() == 0);
		CHECK(device.index_array_owner.get_rid_count() == 0);
		CHECK(device.native_immutable_hold_count == 0);
		CHECK(device.dependency_map.is_empty());
		CHECK(device.reverse_dependency_map.is_empty());
		device.vertex_formats.clear();
		device.vertex_format_cache.clear();
		device.frames.clear();
		device.driver = nullptr;
		device.native_finalization_returned = true;
	}
	static uint8_t fetch(RenderingDevice &device, Indexed144FormatDriver &driver, RID array, uint32_t binding) {
		const auto *original = device.vertex_array_owner.get_or_null(array);
		CHECK(original != nullptr);
		if (!original || binding >= uint32_t(original->buffers.size())) {
			return 0;
		}
		const uint64_t id = original->buffers[binding].id;
		CHECK((id > 0 && id < 32 && driver.live[id]));
		return id && id < 32 && driver.live[id] ? driver.memory[id][original->offsets[binding]] : 0;
	}
	static void dispose(RenderingDevice &device) { device._free_pending_resources(0); }
	static void detach(RenderingDevice &p_device) {
		for (RID rid : p_device.vertex_array_owner.get_owned_list()) {
			p_device.free_rid(rid);
		}
		for (RID rid : p_device.vertex_buffer_owner.get_owned_list()) {
			MutexLock rd_lock(p_device._thread_safe_);
			p_device._free_dependencies(rid, rd_lock);
			p_device.vertex_buffer_owner.free(rid);
		}
		// These format IDs are test-double metadata, not driver allocations.
		p_device.vertex_formats.clear();
		p_device.vertex_format_cache.clear();
		CHECK(p_device.dependency_map.is_empty());
		CHECK(p_device.reverse_dependency_map.is_empty());
		p_device.driver = nullptr;
	}
};

namespace RendererRD {
class TestMeshStorageExternalAccess {
public:
	using Hold = MeshStorage::NativeSurfaceHold;
	static_assert(!std::is_copy_constructible_v<Hold>);
	static_assert(!std::is_copy_assignable_v<Hold>);
	static_assert(std::is_move_constructible_v<Hold>);
	static_assert(!std::is_copy_constructible_v<RD::NativeImmutableHold>);
	static MeshStorage::Mesh::Surface *surface(RenderingDevice &device, bool external) {
		auto *surface = memnew(MeshStorage::Mesh::Surface);
		surface->external = external;
		surface->vertex_count = 3;
		surface->primitive = RSE::PRIMITIVE_TRIANGLES;
		surface->format = RSE::ARRAY_FORMAT_VERTEX;
		surface->vertex_buffer_size = external ? 144 * 3 : 12 * 3;
		surface->vertex_buffer = device.vertex_buffer_create(surface->vertex_buffer_size, {}, RD::BUFFER_CREATION_AS_STORAGE_BIT);
		if (external) {
			surface->previous_vertex_buffer = device.vertex_buffer_create(144 * 3, {}, RD::BUFFER_CREATION_AS_STORAGE_BIT);
			surface->current_allocation_generation = 7;
			surface->previous_allocation_generation = 8;
			surface->index_count = 3;
			surface->index_buffer_size = 12;
			surface->index_buffer = device.index_buffer_create(3, RD::INDEX_BUFFER_FORMAT_UINT32, {}, false);
			surface->index_array = device.index_array_create(surface->index_buffer, 0, 3);
			CHECK(device.validate_external_indexed_surface_buffers(surface->vertex_buffer, surface->previous_vertex_buffer, surface->index_buffer, surface->index_array, 3, 3));
			RID array;
			RD::VertexFormatID format = 0;
			MeshStorage::_mesh_surface_get_external_arrays(surface, 1, false, false, array, format);
		} else {
			// Original ordinary native Version metadata; the actual VA is created
			// through RD and its exact original dependency map owns the backing.
			RD::VertexAttribute attribute;
			attribute.location = 0;
			attribute.format = RD::DATA_FORMAT_R32G32B32_SFLOAT;
			attribute.stride = 12;
			const RD::VertexFormatID format = device.vertex_format_create({ attribute });
			const RID array = device.vertex_array_create(3, format, { surface->vertex_buffer });
			surface->versions = static_cast<MeshStorage::Mesh::Surface::Version *>(memalloc(sizeof(MeshStorage::Mesh::Surface::Version)));
			new (surface->versions) MeshStorage::Mesh::Surface::Version;
			surface->versions[0].input_mask = 1;
			surface->versions[0].vertex_array = array;
			surface->versions[0].vertex_format = format;
			surface->version_count = 1;
		}
		return surface;
	}
	static void immutable_surface(bool external) {
		RenderingDevice device;
		Indexed144FormatDriver driver;
		TestIndexed144DeviceAccess::begin_native(device, driver);
		MeshStorage::Mesh mesh;
		mesh.surfaces = static_cast<MeshStorage::Mesh::Surface **>(memalloc(sizeof(MeshStorage::Mesh::Surface *)));
		mesh.surface_count = 1;
		mesh.surfaces[0] = surface(device, external);
		auto *old = mesh.surfaces[0];
		// Independent LOD/attribute/blend roots do not follow the selected VA.
		old->attribute_buffer = device.vertex_buffer_create(16, {}, RD::BUFFER_CREATION_AS_STORAGE_BIT);
		old->blend_shape_buffer = device.vertex_buffer_create(16, {}, RD::BUFFER_CREATION_AS_STORAGE_BIT);
		old->lods = memnew_arr(MeshStorage::Mesh::Surface::LOD, 1);
		old->lod_count = 1;
		old->lods[0].index_count = 3;
		old->lods[0].index_buffer = device.index_buffer_create(3, RD::INDEX_BUFFER_FORMAT_UINT32, {}, false);
		old->lods[0].index_array = device.index_array_create(old->lods[0].index_buffer, 0, 3);
		old->aabb = AABB(Vector3(-1, -2, -3), Vector3(4, 5, 6));
		old->uv_scale = Vector4(2, 3, 4, 5);
		const RID backing = old->vertex_buffer;
		const RID previous = old->previous_vertex_buffer;
		const RID index = old->index_buffer;
		const RID index_array = old->index_array;
		RID root = old->versions[0].vertex_array;
		RD::NativeImmutableHold refused_capacity;
		CHECK(device.native_immutable_hold_acquire(VectorView<RID>(&root, 1), refused_capacity, 1) == ERR_OUT_OF_MEMORY);
		CHECK_FALSE(refused_capacity.is_valid());
		CHECK_FALSE(device.native_immutable_resource_is_held(root));
		CHECK_FALSE(device.native_immutable_resource_is_held(backing));
		Hold first, second;
		CHECK(MeshStorage::_mesh_surface_hold_acquire(old, 1, false, false, 0, first) == OK);
		CHECK(MeshStorage::_mesh_surface_hold_acquire(old, 1, false, false, 0, second) == OK);
		CHECK(first.is_valid());
		const RID accepted_array = first.get_vertex_array();
		CHECK(MeshStorage::_native_hold_freeze_directory(first) == ERR_BUSY);
		CHECK(old->output_directory_holds == 0);
		const auto original_byte = TestIndexed144DeviceAccess::fetch(device, driver, accepted_array, 0);
		Hold moved(static_cast<Hold &&>(first));
		CHECK_FALSE(first.is_valid());
		CHECK(first.get_vertex_array().is_null());
		CHECK(moved.get_vertex_array() == accepted_array);
		uint32_t retirement_roots = 0;
		CHECK(MeshStorage::_native_hold_retirement_root_count(moved, retirement_roots) == OK);
		CHECK(retirement_roots == 7 + old->version_count + 2 * old->lod_count);
		bool saw_attribute = false, saw_blend = false, saw_lod_buffer = false, saw_lod_array = false;
		for (uint32_t ordinal = 0; ordinal < retirement_roots; ++ordinal) {
			RID actual_root;
			CHECK(MeshStorage::_native_hold_retirement_root_at(moved, ordinal, actual_root) == OK);
			saw_attribute |= actual_root == old->attribute_buffer;
			saw_blend |= actual_root == old->blend_shape_buffer;
			saw_lod_buffer |= actual_root == old->lods[0].index_buffer;
			saw_lod_array |= actual_root == old->lods[0].index_array;
		}
		CHECK(saw_attribute);
		CHECK(saw_blend);
		CHECK(saw_lod_buffer);
		CHECK(saw_lod_array);
		RID beyond(accepted_array);
		CHECK(MeshStorage::_native_hold_retirement_root_at(moved, retirement_roots, beyond) == ERR_INVALID_PARAMETER);
		CHECK(beyond.is_null());

		// Original direct RD writers also reject before staging/native effects.
		uint8_t changed = 255;
		CHECK(device.buffer_update(backing, 0, 1, &changed) == ERR_BUSY);
		CHECK(device.buffer_clear(backing, 0, 4) == ERR_BUSY);
		CHECK(device.buffer_copy(backing, backing, 0, 0, 4) == ERR_BUSY);

		CHECK(TestIndexed144DeviceAccess::fetch(device, driver, accepted_array, 0) == original_byte);
		if (external) {
			RID other;
			RD::VertexFormatID format = 0;
			MeshStorage::_mesh_surface_get_external_arrays(old, 1 | (1 << 2), false, false, other, format);
			CHECK(old->version_count == 2); // Original versions may realloc; the hold keeps values.
			CHECK(moved.get_vertex_array() == accepted_array);
		}
		MeshStorage::_mesh_surface_clear(&mesh, 0);
		mesh.surface_count = 0;
		CHECK(old->immutable_retired);
		CHECK(old->immutable_holds == 2);
		CHECK(TestIndexed144DeviceAccess::fetch(device, driver, accepted_array, 0) == original_byte);
		Hold refused;
		CHECK(MeshStorage::_mesh_surface_hold_acquire(old, 1, false, false, 0, refused) == ERR_INVALID_PARAMETER);
		CHECK_FALSE(refused.is_valid());
		// Replacing the live mesh owns distinct native bytes; accepted old fetch
		// stays at the old allocation after old mesh/surface detachment.
		mesh.surfaces[0] = surface(device, external);
		mesh.surface_count = 1;
		Hold replacement;
		CHECK(MeshStorage::_mesh_surface_hold_acquire(mesh.surfaces[0], 1, false, false, 0, replacement) == OK);
		CHECK(TestIndexed144DeviceAccess::fetch(device, driver, replacement.get_vertex_array(), 0) != original_byte);
		CHECK(TestIndexed144DeviceAccess::fetch(device, driver, moved.get_vertex_array(), 0) == original_byte);
		// Derive from the actual retained old Surface after COW. A current-mesh
		// lookup would select replacement's distinct bytes and fail this check.
		Hold derived;
		CHECK(MeshStorage::mesh_surface_hold_derive(moved, 1, false, false, 0, derived) == OK);
		CHECK(derived.is_valid());
		CHECK(derived.get_vertex_array() == accepted_array);
		CHECK(derived.get_format() == moved.get_format());
		CHECK(derived.get_aabb() == AABB(Vector3(-1, -2, -3), Vector3(4, 5, 6)));
		CHECK(derived.get_uv_scale() == Vector4(2, 3, 4, 5));
		CHECK(derived.get_vertex_array() != replacement.get_vertex_array());
		CHECK(TestIndexed144DeviceAccess::fetch(device, driver, derived.get_vertex_array(), 0) == original_byte);
		CHECK(old->immutable_holds == 3);
		CHECK(MeshStorage::_native_hold_freeze_directory(moved) == OK);
		CHECK(MeshStorage::_native_hold_freeze_directory(moved) == OK);
		CHECK(old->output_directory_holds == 1);
		CHECK(MeshStorage::_native_hold_freeze_directory(derived) == OK);
		CHECK(old->output_directory_holds == 2);
		// Moving a frozen hold transfers exactly one shared directory claim.
		Hold frozen(static_cast<Hold &&>(derived));
		CHECK(MeshStorage::_native_hold_thaw_directory(derived) == ERR_UNAUTHORIZED);
		CHECK(old->output_directory_holds == 2);
		Hold existing;
		CHECK(MeshStorage::mesh_surface_hold_derive(moved, 1, false, false, 0, existing) == OK);
		CHECK(existing.get_vertex_array() == accepted_array);
		CHECK(MeshStorage::mesh_surface_hold_release(existing) == OK);
		if (external) {
			const uint32_t prepared = old->version_count;
			RID refused_array = accepted_array;
			RD::VertexFormatID refused_format = moved.get_vertex_format();
			MeshStorage::_mesh_surface_get_external_arrays(old, 1 | (1 << 4), false, false, refused_array, refused_format);
			CHECK(refused_array.is_null());
			CHECK(refused_format == 0);
			CHECK(old->version_count == prepared);
			MeshStorage::_mesh_surface_get_external_arrays(old, 1, false, false, refused_array, refused_format);
			CHECK(refused_array == accepted_array);
			CHECK(refused_format == moved.get_vertex_format());
		}
		CHECK(MeshStorage::mesh_surface_hold_release(frozen) == OK);
		CHECK(old->output_directory_holds == 1);
		CHECK(MeshStorage::_native_hold_thaw_directory(moved) == OK);
		CHECK(MeshStorage::_native_hold_thaw_directory(moved) == OK);
		CHECK(old->output_directory_holds == 0);
		if (external) {
			const uint32_t prepared = old->version_count;
			RID resumed_array;
			RD::VertexFormatID resumed_format = 0;
			MeshStorage::_mesh_surface_get_external_arrays(old, 1 | (1 << 4), false, false, resumed_array, resumed_format);
			CHECK(resumed_array.is_valid());
			CHECK(resumed_format != 0);
			CHECK(old->version_count == prepared + 1);
		}
		CHECK(MeshStorage::_native_hold_freeze_directory(second) == OK);
		CHECK(old->output_directory_holds == 1);
		CHECK(old->immutable_holds == 2);
		Hold no_source;
		CHECK(MeshStorage::mesh_surface_hold_derive(first, 1, false, false, 0, no_source) != OK);
		CHECK_FALSE(no_source.is_valid());
		CHECK(MeshStorage::mesh_surface_hold_release(replacement) == OK);
		const RID replacement_current = mesh.surfaces[0]->vertex_buffer;
		const RID replacement_previous = mesh.surfaces[0]->previous_vertex_buffer;
		const RID replacement_index = mesh.surfaces[0]->index_buffer;
		MeshStorage::_mesh_surface_clear(&mesh, 0);
		mesh.surface_count = 0;
		if (external) {
			device.free_rid(replacement_current);
			device.free_rid(replacement_previous);
			device.free_rid(replacement_index);
		}
		TestIndexed144DeviceAccess::dispose(device);
		CHECK(TestIndexed144DeviceAccess::fetch(device, driver, accepted_array, 0) == original_byte);
		CHECK(MeshStorage::mesh_surface_hold_release(second) == OK);
		CHECK(old->output_directory_holds == 0);
		CHECK(device.native_immutable_resource_is_held(backing));
		if (external) {
			// The actual external backing owner can request close while retained.
			// RD keeps physical storage, rejects new acquisition, and frees once
			// the final original hold returns; it does not lose a void free request.
			device.free_rid(backing);
			CHECK(device.native_immutable_resource_is_retiring(backing));
			CHECK(TestIndexed144DeviceAccess::fetch(device, driver, accepted_array, 0) == original_byte);
			RD::NativeImmutableHold new_physical;
			RID retiring_root = accepted_array;
			CHECK(device.native_immutable_hold_acquire(VectorView<RID>(&retiring_root, 1), new_physical) == ERR_BUSY);
		}
		CHECK(MeshStorage::mesh_surface_hold_release(moved) == OK);
		CHECK_FALSE(moved.is_valid());
		CHECK(moved.get_vertex_array().is_null());
		CHECK_FALSE(device.native_immutable_resource_is_held(backing));
		if (external) {
			device.free_rid(previous);
			device.free_rid(index);
			// Original index-array ownership follows its borrowed index backing.
			CHECK_FALSE(device.native_immutable_resource_is_held(index_array));
		}
		TestIndexed144DeviceAccess::dispose(device);
		CHECK(driver.frees == driver.allocations);
		memfree(mesh.surfaces);
		TestIndexed144DeviceAccess::end_native(device);
	}
	static void ordinary_directory() {
		// Initialize the actual storage object without unrelated Skeleton shader
		// startup. Every getter input used here has an actual original RD buffer.
		RenderingDevice device;
		Indexed144FormatDriver driver;
		TestIndexed144DeviceAccess::begin_native(device, driver);
		auto *previous_storage = MeshStorage::get_singleton();
		auto *storage = memnew(MeshStorage(MeshStorage::NativeHoldTestTag{}));
		const RID normal = device.vertex_buffer_create(16, {}, RD::BUFFER_CREATION_AS_STORAGE_BIT);
		storage->mesh_default_rd_buffers[MeshStorage::DEFAULT_RD_BUFFER_NORMAL] = normal;
		MeshStorage::Mesh mesh;
		mesh.surfaces = static_cast<MeshStorage::Mesh::Surface **>(memalloc(sizeof(MeshStorage::Mesh::Surface *)));
		mesh.surface_count = 1;
		auto *original = surface(device, false);
		mesh.surfaces[0] = original;
		// Enroll the original surface in an actual mesh RID without creating a VA.
		const RID selected_rid = storage->mesh_allocate();
		storage->mesh_initialize(selected_rid);
		auto *selected_mesh = storage->mesh_owner.get_or_null(selected_rid);
		selected_mesh->surfaces = mesh.surfaces;
		selected_mesh->surface_count = 1;
		MeshStorage::NativeSelectionLease selection, sibling;
		const uint32_t versions_before = original->version_count;
		// Counter saturation is a CPU boundary model, not admitted custody.
		selected_mesh->selection_holds = UINT32_MAX;
		CHECK(storage->mesh_selection_acquire(selected_rid, selection) == ERR_PARAMETER_RANGE_ERROR);
		CHECK_FALSE(selection.is_active());
		CHECK(selected_mesh->selection_holds == UINT32_MAX);
		CHECK(original->version_count == versions_before);
		selected_mesh->selection_holds = 0;
		CHECK(storage->mesh_selection_acquire(selected_rid, selection) == OK);
		CHECK(storage->mesh_selection_acquire(selected_rid, selection) == ERR_ALREADY_IN_USE);
		CHECK(storage->mesh_selection_acquire(selected_rid, sibling) == OK);
		CHECK(original->version_count == versions_before);
		CHECK(original->immutable_holds == 0);
		const RID material_before = original->material;
		MeshStorage::ExternalSurfaceDescriptor descriptor;
		descriptor.vertex_count = descriptor.index_count = 3;
		descriptor.current_vertex_buffer = device.vertex_buffer_create(432, {}, RD::BUFFER_CREATION_AS_STORAGE_BIT);
		descriptor.previous_vertex_buffer = device.vertex_buffer_create(432, {}, RD::BUFFER_CREATION_AS_STORAGE_BIT);
		descriptor.index_buffer = device.index_buffer_create(3, RD::INDEX_BUFFER_FORMAT_UINT32, {}, false);
		descriptor.index_array = device.index_array_create(descriptor.index_buffer, 0, 3);
		descriptor.vertex_buffer_size = descriptor.previous_vertex_buffer_size = 432;
		descriptor.index_buffer_size = 12;
		descriptor.primitive = RSE::PRIMITIVE_TRIANGLES;
		descriptor.material = RID::from_uint64(999);
		descriptor.generation = 1;
		descriptor.current_allocation_generation = 1;
		descriptor.previous_allocation_generation = 2;
		uint32_t added_index = UINT32_MAX;
		uint64_t added_generation = 0;
		CHECK(storage->mesh_add_external_surface(selected_rid, descriptor, added_index, added_generation) == ERR_BUSY);
		CHECK(added_index == UINT32_MAX);
		CHECK(added_generation == 0);
		// Prove this exact descriptor is accepted by the unheld original API.
		const RID descriptor_neighbor = storage->mesh_allocate();
		storage->mesh_initialize(descriptor_neighbor);
		CHECK(storage->mesh_add_external_surface(descriptor_neighbor, descriptor, added_index, added_generation) == OK);
		CHECK(storage->mesh_get_surface_count(descriptor_neighbor) == 1);
		storage->mesh_free(descriptor_neighbor);
		device.free_rid(descriptor.current_vertex_buffer);
		device.free_rid(descriptor.previous_vertex_buffer);
		device.free_rid(descriptor.index_buffer);
		ERR_PRINT_OFF;
		storage->mesh_free(selected_rid);
		storage->mesh_clear(selected_rid);
		storage->mesh_surface_remove(selected_rid, 0);
		storage->mesh_surface_set_material(selected_rid, 0, RID::from_uint64(999));
		CHECK(storage->mesh_instance_create(selected_rid).is_null());
		ERR_PRINT_ON;
		CHECK(storage->mesh_owner.get_or_null(selected_rid) == selected_mesh);
		CHECK(selected_mesh->surface_count == 1);
		CHECK(selected_mesh->surfaces[0] == original);
		CHECK(original->material == material_before);
		CHECK(storage->mesh_selection_release(selection) == OK);
		CHECK(selected_mesh->selection_holds == 1);
		CHECK(storage->mesh_selection_release(selection) == ERR_UNAUTHORIZED);
		CHECK(storage->mesh_selection_release(sibling) == OK);
		CHECK(selected_mesh->selection_holds == 0);
		// Restore the existing fixture's local original owner for directory tests.
		selected_mesh->surfaces = nullptr;
		selected_mesh->surface_count = 0;
		storage->mesh_free(selected_rid);
		Hold retained;
		CHECK(MeshStorage::_mesh_surface_hold_acquire(original, 1, false, false, 0, retained) == OK);
		CHECK(MeshStorage::_native_hold_freeze_directory(retained) == ERR_BUSY);
		const uint64_t neighbor_mask = 1 | (1 << RSE::ARRAY_NORMAL);
		RID neighbor;
		RD::VertexFormatID format = 0;
		const uint32_t initial = original->version_count;
		storage->mesh_surface_get_vertex_arrays_and_format(original, neighbor_mask, false, false, neighbor, format);
		CHECK(neighbor.is_valid());
		CHECK(format != 0);
		CHECK(original->version_count == initial + 1);
		MeshStorage::_mesh_surface_clear(&mesh, 0);
		mesh.surface_count = 0;
		CHECK(MeshStorage::_native_hold_freeze_directory(retained) == OK);
		const RID prepared = neighbor;
		const RD::VertexFormatID prepared_format = format;
		storage->mesh_surface_get_vertex_arrays_and_format(original, neighbor_mask, false, false, neighbor, format);
		CHECK(neighbor == prepared);
		CHECK(format == prepared_format);
		Hold derived;
		CHECK(MeshStorage::mesh_surface_hold_derive(retained, neighbor_mask, false, false, 0, derived) == OK);
		CHECK(derived.get_vertex_array() == prepared);
		const uint32_t frozen = original->version_count;
		storage->mesh_surface_get_vertex_arrays_and_format(original, neighbor_mask, true, false, neighbor, format);
		CHECK(neighbor.is_null());
		CHECK(format == 0);
		CHECK(original->version_count == frozen);
		CHECK(MeshStorage::_native_hold_thaw_directory(retained) == OK);
		storage->mesh_surface_get_vertex_arrays_and_format(original, neighbor_mask, true, false, neighbor, format);
		CHECK(neighbor.is_valid());
		CHECK(format != 0);
		CHECK(original->version_count == frozen + 1);
		CHECK(MeshStorage::_native_hold_freeze_directory(retained) == OK);
		CHECK(MeshStorage::mesh_surface_hold_release(derived) == OK);
		CHECK(MeshStorage::mesh_surface_hold_release(retained) == OK);
		device.free_rid(normal);
		memdelete(storage);
		MeshStorage::singleton = previous_storage;
		TestIndexed144DeviceAccess::dispose(device);
		CHECK(driver.frees == driver.allocations);
		memfree(mesh.surfaces);
		TestIndexed144DeviceAccess::end_native(device);
	}
	static void prospective_plan(bool external) {
		RenderingDevice device;
		Indexed144FormatDriver driver;
		TestIndexed144DeviceAccess::begin_native(device, driver);
		auto *previous_storage = MeshStorage::get_singleton();
		auto *storage = memnew(MeshStorage(MeshStorage::NativeHoldTestTag{}));
		const RID normal = device.vertex_buffer_create(16, {}, RD::BUFFER_CREATION_AS_STORAGE_BIT);
		storage->mesh_default_rd_buffers[MeshStorage::DEFAULT_RD_BUFFER_NORMAL] = normal;
		MeshStorage::Mesh mesh;
		mesh.surfaces = static_cast<MeshStorage::Mesh::Surface **>(memalloc(sizeof(MeshStorage::Mesh::Surface *)));
		mesh.surface_count = 1;
		auto *original = surface(device, external);
		mesh.surfaces[0] = original;
		const RID current = original->vertex_buffer, previous = original->previous_vertex_buffer, index = original->index_buffer;
		Hold retained;
		const Error acquired = MeshStorage::_mesh_surface_hold_acquire(original, 1, false, false, 0, retained);
		CHECK(acquired == OK);
		if (acquired == OK) {
			using Plan = MeshStorage::NativeVertexArrayPlan;
			Plan plan;
			const uint32_t formats = driver.formats;
			const uint32_t arrays = TestIndexed144DeviceAccess::arrays(device);
			const uint32_t versions = original->version_count;
			const uint32_t holds = original->immutable_holds;
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 1, false, false, 0, plan) == OK);
			CHECK(plan.existing_vertex_array == retained.get_vertex_array());
			CHECK(plan.existing_vertex_format == retained.get_vertex_format());
			CHECK(plan.vertex_count == 3);
			CHECK(plan.buffers.size() == 1);
			if (!plan.buffers.is_empty()) {
				CHECK(plan.buffers[0] == original->vertex_buffer);
			}
			CHECK(plan.attributes.size() == (external ? 10 : 1));
			if (!plan.attributes.is_empty()) {
				CHECK(plan.attributes[0].location == 0);
				CHECK(plan.attributes[0].stride == (external ? 144 : 12));
				CHECK(plan.attributes[0].format == (external ? RD::DATA_FORMAT_R32G32B32A32_SFLOAT : RD::DATA_FORMAT_R32G32B32_SFLOAT));
			}
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 3, false, false, 0, plan) == OK);
			CHECK(plan.existing_vertex_array.is_null());
			CHECK(plan.existing_vertex_format == 0);
			CHECK(plan.attributes.size() == (external ? 10 : 2));
			CHECK(plan.buffers.size() == (external ? 1 : 2));
			if (!external) {
				CHECK(plan.offsets.size() == 2);
				if (plan.buffers.size() == 2 && plan.offsets.size() == 2) {
					CHECK(plan.buffers[1] == normal);
					CHECK(plan.offsets[0] == 0);
					CHECK(plan.offsets[1] == 0);
				}
				storage->mesh_default_rd_buffers[MeshStorage::DEFAULT_RD_BUFFER_NORMAL] = RID();
				CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 3, false, false, 0, plan) == ERR_INVALID_DATA);
				storage->mesh_default_rd_buffers[MeshStorage::DEFAULT_RD_BUFFER_NORMAL] = normal;
			} else {
				CHECK(plan.offsets.is_empty());
				CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 1ull << 31, false, false, 0, plan) == ERR_UNAVAILABLE);
			}
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 1, true, false, 0, plan) == ERR_UNAVAILABLE);
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 1, false, true, 0, plan) == ERR_UNAVAILABLE);
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 1, false, false, 1, plan) == ERR_UNAVAILABLE);
			original->immutable_holds = UINT32_MAX;
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 1, false, false, 0, plan) == ERR_INVALID_PARAMETER);
			original->immutable_holds = holds;
			// A published failed Version must not be reclassified as future work.
			const RID saved_array = original->versions[0].vertex_array;
			original->versions[0].vertex_array = RID();
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 1, false, false, 0, plan) == ERR_CANT_CREATE);
			CHECK(plan.existing_vertex_array.is_null());
			CHECK(plan.existing_vertex_format == 0);
			original->versions[0].vertex_array = saved_array;
			CHECK(driver.formats == formats);
			CHECK(TestIndexed144DeviceAccess::arrays(device) == arrays);
			CHECK(original->version_count == versions);
			CHECK(original->immutable_holds == holds);
			// Detached retained source, frozen existing and missing keys.
			MeshStorage::_mesh_surface_clear(&mesh, 0);
			mesh.surface_count = 0;
			CHECK(MeshStorage::_native_hold_freeze_directory(retained) == OK);
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 1, false, false, 0, plan) == OK);
			CHECK(!plan.buffers.is_empty());
			if (!plan.buffers.is_empty()) {
				CHECK(plan.buffers[0] == original->vertex_buffer);
			}
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 3, false, false, 0, plan) == ERR_UNAVAILABLE);
			CHECK(MeshStorage::_native_hold_thaw_directory(retained) == OK);
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 3, false, false, 0, plan) == OK);
			Hold derived;
			CHECK(MeshStorage::mesh_surface_hold_derive(retained, 3, false, false, 0, derived) == OK);
			CHECK(derived.is_valid());
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 3, false, false, 0, plan) == OK);
			CHECK(plan.existing_vertex_array == derived.get_vertex_array());
			CHECK(plan.existing_vertex_format == derived.get_vertex_format());
			CHECK(MeshStorage::mesh_surface_hold_release(derived) == OK);
			CHECK(MeshStorage::mesh_surface_hold_release(retained) == OK);
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 1, false, false, 0, plan) == ERR_UNAUTHORIZED);
		} else {
			MeshStorage::_mesh_surface_clear(&mesh, 0);
			mesh.surface_count = 0;
		}
		if (external) {
			device.free_rid(current);
			device.free_rid(previous);
			device.free_rid(index);
		}
		device.free_rid(normal);
		memdelete(storage);
		MeshStorage::singleton = previous_storage;
		TestIndexed144DeviceAccess::dispose(device);
		CHECK(driver.frees == driver.allocations);
		memfree(mesh.surfaces);
		TestIndexed144DeviceAccess::end_native(device);
	}
	static void incremental_surface(bool external) {
		// Actual local holds exercise metadata steps, not Core pin/packet authority.
		RenderingDevice device;
		Indexed144FormatDriver driver;
		TestIndexed144DeviceAccess::begin_native(device, driver);
		MeshStorage::Mesh mesh;
		mesh.surfaces = static_cast<MeshStorage::Mesh::Surface **>(memalloc(sizeof(MeshStorage::Mesh::Surface *)));
		mesh.surface_count = 1;
		auto *original = surface(device, external);
		mesh.surfaces[0] = original;
		const RID current = original->vertex_buffer, previous = original->previous_vertex_buffer, index = original->index_buffer;
		Hold exact;
		CHECK(MeshStorage::_mesh_surface_hold_acquire(original, 1, false, false, 0, exact) == OK);
		MeshStorage::_mesh_surface_clear(&mesh, 0);
		mesh.surface_count = 0;
		CHECK(MeshStorage::_native_hold_freeze_directory(exact) == OK);
		CHECK(original->output_directory_holds == 1);
		uint32_t count = 0;
		CHECK(MeshStorage::_native_hold_retirement_root_count(exact, count) == OK);
		LocalVector<RID> roots;
		for (uint32_t i = 0; i < count; ++i) {
			RID root;
			CHECK(MeshStorage::_native_hold_retirement_root_at(exact, i, root) == OK);
			if (root.is_valid()) {
				roots.push_back(root);
			}
		}
		RD::NativeImmutableHold complete;
		CHECK(device.native_immutable_hold_acquire(VectorView<RID>(roots.ptr(), roots.size()), complete) == OK);
		// A locally held directory is not an original accepted Core binding.
		// Unsupported selected-only transfer refuses without moving custody.
		CHECK(MeshStorage::_native_hold_transfer_source(exact) == nullptr);
		CHECK(MeshStorage::_native_hold_mark_transferred(exact) == ERR_INVALID_DATA);
		CHECK(exact.is_valid());
		CHECK(original->output_directory_holds == 1);
		CHECK(MeshStorage::_native_hold_begin_return(exact) == ERR_BUSY);
		CHECK(MeshStorage::_native_hold_finish_return(exact) == ERR_BUSY);
		CHECK(driver.frees == 0);
		CHECK(device.native_immutable_hold_release(complete) == OK);
		CHECK(MeshStorage::mesh_surface_hold_release(exact) == OK);
		if (external) {
			device.free_rid(current);
			device.free_rid(previous);
			device.free_rid(index);
		}
		TestIndexed144DeviceAccess::dispose(device);
		CHECK(driver.frees == driver.allocations);
		memfree(mesh.surfaces);
		TestIndexed144DeviceAccess::end_native(device);
	}
	static void run(RenderingDevice &p_device, Indexed144FormatDriver &p_driver, bool p_failure) {
		MeshStorage::Mesh mesh;
		mesh.surface_count = 1;
		mesh.surfaces = static_cast<MeshStorage::Mesh::Surface **>(memalloc(sizeof(MeshStorage::Mesh::Surface *)));
		auto *surface = memnew(MeshStorage::Mesh::Surface);
		mesh.surfaces[0] = surface;
		surface->external = true;
		surface->vertex_count = 3;
		surface->current_allocation_generation = 7;
		surface->previous_allocation_generation = 8;
		surface->vertex_buffer = TestIndexed144DeviceAccess::buffer(p_device, 71);
		surface->previous_vertex_buffer = TestIndexed144DeviceAccess::buffer(p_device, 72);
		const RID current = surface->vertex_buffer, previous = surface->previous_vertex_buffer;
		RID va;
		RD::VertexFormatID format = 0;
		if (p_failure) {
			p_driver.fail_format = true;
			ERR_PRINT_OFF;
			MeshStorage::_mesh_surface_get_external_arrays(surface, 1, false, false, va, format);
			ERR_PRINT_ON;
			CHECK(va.is_null());
			CHECK(surface->version_count == 0);
			CHECK(TestIndexed144DeviceAccess::arrays(p_device) == 0);
			p_driver.fail_format = false;
			TestIndexed144DeviceAccess::size(p_device, current, 144 * 2);
			ERR_PRINT_OFF;
			MeshStorage::_mesh_surface_get_external_arrays(surface, 1, false, false, va, format);
			ERR_PRINT_ON;
			CHECK(va.is_null());
			CHECK(surface->version_count == 0);
			CHECK(TestIndexed144DeviceAccess::arrays(p_device) == 0);
			TestIndexed144DeviceAccess::size(p_device, current, 144 * 3);
		}
		MeshStorage::_mesh_surface_get_external_arrays(surface, 1, false, false, va, format);
		CHECK(va.is_valid());
		CHECK(surface->version_count == 1);
		const RID position_only = va;
		MeshStorage::_mesh_surface_get_external_arrays(surface, 1, false, false, va, format);
		CHECK(va == position_only);
		CHECK(surface->version_count == 1);
		MeshStorage::_mesh_surface_get_external_arrays(surface, 1 | (1 << 2), false, false, va, format);
		CHECK(va.is_valid());
		CHECK(va != position_only);
		CHECK(surface->version_count == 2);
		const RID tangent_view = va;
		surface->previous_allocation_generation++;
		MeshStorage::_mesh_surface_get_external_arrays(surface, 1 | (1 << 2), false, false, va, format);
		CHECK(va != tangent_view);
		CHECK(surface->version_count == 3);
		const RID previous_changed = va;
		surface->current_allocation_generation++;
		MeshStorage::_mesh_surface_get_external_arrays(surface, 1 | (1 << 2), false, false, va, format);
		CHECK(va != previous_changed);
		CHECK(surface->version_count == 4);
		MeshStorage::_mesh_surface_get_external_arrays(surface, (1 << 0) | (1 << 1) | (1 << 2) | (1 << 12) | (1 << 13) | (1 << 14), true, false, va, format);
		CHECK(va.is_valid());
		CHECK(surface->version_count == 5);
		CHECK(p_driver.last_attributes.size() == 13);
		bool previous_tangent = false;
		for (const auto &attribute : p_driver.last_attributes) {
			CHECK(attribute.stride == 144);
			if (attribute.location == 14) {
				previous_tangent = true;
				CHECK(attribute.binding == 1);
				CHECK(attribute.offset == 32);
				CHECK(attribute.format == RD::DATA_FORMAT_R32G32B32A32_SFLOAT);
			}
		}
		CHECK(previous_tangent);
		CHECK(TestIndexed144DeviceAccess::binding(p_device, va, 0) == 71);
		CHECK(TestIndexed144DeviceAccess::binding(p_device, va, 1) == 72);
		surface->vertex_buffer = TestIndexed144DeviceAccess::buffer(p_device, 81);
		surface->previous_vertex_buffer = TestIndexed144DeviceAccess::buffer(p_device, 82);
		surface->current_allocation_generation++;
		surface->previous_allocation_generation++;
		MeshStorage::_mesh_surface_get_external_arrays(surface, 1 | (7 << 12), true, false, va, format);
		CHECK(va.is_valid());
		CHECK(TestIndexed144DeviceAccess::binding(p_device, va, 0) == 81);
		CHECK(TestIndexed144DeviceAccess::binding(p_device, va, 1) == 82);
		const RID triangle_array = va;
		MeshStorage::_mesh_surface_get_external_arrays(surface, 1 | (7 << 12), true, true, va, format);
		CHECK(va.is_valid());
		CHECK(va != triangle_array);
		MeshStorage::_mesh_surface_clear(&mesh, 0);
		memfree(mesh.surfaces);
		mesh.surfaces = nullptr;
		mesh.surface_count = 0;
		CHECK(TestIndexed144DeviceAccess::arrays(p_device) == 0);
		CHECK(TestIndexed144DeviceAccess::owns(p_device, current));
		CHECK(TestIndexed144DeviceAccess::owns(p_device, previous));
	}
};
} //namespace RendererRD

TEST_CASE("[B4][K05] actual Indexed144 cache retains exact mask generations and borrowed buffers") {
	RenderingDevice device;
	Indexed144FormatDriver driver;
	TestIndexed144DeviceAccess::attach(device, &driver);
	RendererRD::TestMeshStorageExternalAccess::run(device, driver, false);
	TestIndexed144DeviceAccess::detach(device);
}

TEST_CASE("[B4][K05] failed original VA construction publishes no cached version") {
	RenderingDevice device;
	Indexed144FormatDriver driver;
	TestIndexed144DeviceAccess::attach(device, &driver);
	RendererRD::TestMeshStorageExternalAccess::run(device, driver, true);
	TestIndexed144DeviceAccess::detach(device);
}

TEST_CASE("[B4][K05] original renderer pipeline keys distinguish Indexed144 from ordinary Mesh") {
	using namespace RendererSceneRenderImplementation;
	SceneShaderForwardMobile::ShaderData::PipelineKey mobile{};
	SceneShaderForwardClustered::ShaderData::PipelineKey clustered{};
	const uint32_t ordinary_mobile = mobile.hash(), ordinary_clustered = clustered.hash();
	mobile.indexed144 = true;
	clustered.indexed144 = true;
	CHECK(mobile.hash() != ordinary_mobile);
	CHECK(clustered.hash() != ordinary_clustered);
}

TEST_CASE("[B4][NativeSurfaceHold] ordinary native surface survives mesh replacement and explicit last return") {
	RendererRD::TestMeshStorageExternalAccess::immutable_surface(false);
	RendererRD::TestMeshStorageExternalAccess::incremental_surface(false);
	RendererRD::TestMeshStorageExternalAccess::ordinary_directory();
}

TEST_CASE("[B4][NativeSurfaceHold] Indexed144 surface keeps exact borrowed native backings and deferred owner free") {
	RendererRD::TestMeshStorageExternalAccess::immutable_surface(true);
	RendererRD::TestMeshStorageExternalAccess::incremental_surface(true);
}

TEST_CASE("[B4][MaterialPreparation] Mobile color preparation preserves transparent blending") {
	using MobileShader = RendererSceneRenderImplementation::SceneShaderForwardMobile;
	// This is the production predicate used before derive/pipeline allocation
	// for OPAQUE, TRANSPARENT and PROBE. No mirrored test-only admission rule.
	SUBCASE("transparent surfaces never select opaque fog-mask variants") {
		CHECK_FALSE(MobileShader::ShaderData::external_color_version_supported(MobileShader::SHADER_VERSION_COLOR_FOG_MASK, true));
		CHECK_FALSE(MobileShader::ShaderData::external_color_version_supported(MobileShader::SHADER_VERSION_LIGHTMAP_FOG_MASK, true));
		CHECK(MobileShader::ShaderData::external_color_version_supported(MobileShader::SHADER_VERSION_COLOR_PASS, true));
		CHECK(MobileShader::ShaderData::external_color_version_supported(MobileShader::SHADER_VERSION_LIGHTMAP_COLOR_PASS, true));
	}
	SUBCASE("opaque and alpha-clipped classifier result retains fog-mask support") {
		CHECK(MobileShader::ShaderData::external_color_version_supported(MobileShader::SHADER_VERSION_COLOR_FOG_MASK, false));
		CHECK(MobileShader::ShaderData::external_color_version_supported(MobileShader::SHADER_VERSION_LIGHTMAP_FOG_MASK, false));
	}
	SUBCASE("noncolor and unadmitted multiview variants remain refused") {
		CHECK_FALSE(MobileShader::ShaderData::external_color_version_supported(MobileShader::SHADER_VERSION_SHADOW_PASS, false));
		CHECK_FALSE(MobileShader::ShaderData::external_color_version_supported(MobileShader::SHADER_VERSION_COLOR_PASS_MULTIVIEW, true));
	}
}

TEST_CASE("[B4][NativeVertexArrayPlan] retained ordinary source plans without factories") {
	RendererRD::TestMeshStorageExternalAccess::prospective_plan(false);
}

TEST_CASE("[B4][NativeVertexArrayPlan] retained Indexed144 source plans without factories") {
	RendererRD::TestMeshStorageExternalAccess::prospective_plan(true);
}
