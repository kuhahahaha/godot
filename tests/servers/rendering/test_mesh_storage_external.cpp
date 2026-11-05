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
			p_device._free_dependencies(rid);
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
		const auto original_byte = TestIndexed144DeviceAccess::fetch(device, driver, accepted_array, 0);
		Hold moved(static_cast<Hold &&>(first));
		CHECK_FALSE(first.is_valid());
		CHECK(first.get_vertex_array().is_null());
		CHECK(moved.get_vertex_array() == accepted_array);
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
		CHECK(device.native_immutable_resource_is_held(backing));
		if (external) {
			// The actual external backing owner can request close while retained.
			// RD keeps physical storage, rejects new acquisition, and frees once
			// the final original hold returns; it does not lose a void free request.
			device.free_rid(backing);
			CHECK(device.native_immutable_resource_is_retiring(backing));
			CHECK(TestIndexed144DeviceAccess::fetch(device, driver, accepted_array, 0) == original_byte);
			RD::NativeImmutableHold new_physical;
			RID root = accepted_array;
			CHECK(device.native_immutable_hold_acquire(VectorView<RID>(&root, 1), new_physical) == ERR_BUSY);
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
}

TEST_CASE("[B4][NativeSurfaceHold] Indexed144 surface keeps exact borrowed native backings and deferred owner free") {
	RendererRD::TestMeshStorageExternalAccess::immutable_surface(true);
}
