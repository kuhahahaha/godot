/**************************************************************************/
/*  test_b4_external_bounds_gpu.cpp                                       */
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

#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"
#include "servers/rendering/rendering_device.h"
#include "tests/test_macros.h"

#include <cstdlib>
#include <cstring>
#include <limits>

#ifdef VULKAN_ENABLED
#include "drivers/vulkan/rendering_context_driver_vulkan.h"
#endif

TEST_FORCE_LINK(test_b4_external_bounds_gpu)

#ifdef VULKAN_ENABLED
#define B4_BOUNDS_CHECK_OR_RETURN(m_condition) \
	do { \
		const bool b4_bounds_ok = bool(m_condition); \
		CHECK_MESSAGE(b4_bounds_ok, #m_condition); \
		if (!b4_bounds_ok) { \
			return; \
		} \
	} while (false)

namespace TestB4ExternalBoundsGPU {
using Storage = RendererRD::MaterialStorage;

// Called at doctest registration, before the Godot OS singleton exists.
bool gpu_tests_enabled() {
	const char *value = std::getenv("GODOT_TEST_B4_MATERIAL_GPU");
	return value && std::strcmp(value, "1") == 0;
}

bool same_component(real_t p_a, real_t p_b) {
	return (Math::is_nan(p_a) && Math::is_nan(p_b)) || p_a == p_b;
}

bool same_bounds(const AABB &p_a, const AABB &p_b) {
	return same_component(p_a.position.x, p_b.position.x) &&
			same_component(p_a.position.y, p_b.position.y) &&
			same_component(p_a.position.z, p_b.position.z) &&
			same_component(p_a.size.x, p_b.size.x) &&
			same_component(p_a.size.y, p_b.size.y) &&
			same_component(p_a.size.z, p_b.size.z);
}

// This fixture owns the actual storage and device RIDs. Meshes are released
// before their borrowed buffers, including on a failed precondition.
struct Resources {
	RenderingDevice &rd;
	Storage &materials;
	RendererRD::MeshStorage &meshes;
	RID mesh;
	RID ordinary_mesh;
	RID material;
	RID current;
	RID previous;
	RID index;
	RID index_array;

	~Resources() {
		if (ordinary_mesh.is_valid()) {
			meshes.mesh_free(ordinary_mesh);
		}
		if (mesh.is_valid()) {
			meshes.mesh_free(mesh);
		}
		for (RID rid : { index_array, index, previous, current }) {
			if (rid.is_valid()) {
				rd.free_rid(rid);
			}
		}
		if (material.is_valid()) {
			materials.material_free(material);
		}
	}
};

TEST_CASE("[B4][MaterialGPU] external surface bounds are finite and nonnegative" * doctest::skip(!gpu_tests_enabled())) {
	B4_BOUNDS_CHECK_OR_RETURN(gpu_tests_enabled());
	B4_BOUNDS_CHECK_OR_RETURN(RenderingDevice::get_singleton() == nullptr);
	RenderingContextDriverVulkan context;
	B4_BOUNDS_CHECK_OR_RETURN(context.initialize() == OK);
	RenderingDevice rd;
	B4_BOUNDS_CHECK_OR_RETURN(rd.initialize(&context) == OK);
	RendererRD::TextureStorage textures;
	Storage materials;
	RendererRD::MeshStorage meshes;
	Resources resources{ rd, materials, meshes };
	resources.mesh = meshes.mesh_allocate();
	B4_BOUNDS_CHECK_OR_RETURN(resources.mesh.is_valid());
	meshes.mesh_initialize(resources.mesh);
	resources.material = materials.material_allocate();
	B4_BOUNDS_CHECK_OR_RETURN(resources.material.is_valid());
	materials.material_initialize(resources.material);

	Vector<uint8_t> vertices;
	vertices.resize(144 * 3);
	Vector<uint8_t> indices;
	indices.resize(3 * sizeof(uint32_t));
	resources.current = rd.vertex_buffer_create(vertices.size(), vertices, RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT);
	B4_BOUNDS_CHECK_OR_RETURN(resources.current.is_valid());
	resources.previous = rd.vertex_buffer_create(vertices.size(), vertices, RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT);
	B4_BOUNDS_CHECK_OR_RETURN(resources.previous.is_valid());
	resources.index = rd.index_buffer_create(3, RenderingDevice::INDEX_BUFFER_FORMAT_UINT32, indices, false, RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT);
	B4_BOUNDS_CHECK_OR_RETURN(resources.index.is_valid());
	resources.index_array = rd.index_array_create(resources.index, 0, 3);
	B4_BOUNDS_CHECK_OR_RETURN(resources.index_array.is_valid());

	RendererRD::MeshStorage::ExternalSurfaceDescriptor desc;
	desc.vertex_count = 3;
	desc.index_count = 3;
	desc.current_vertex_buffer = resources.current;
	desc.previous_vertex_buffer = resources.previous;
	desc.vertex_buffer_size = desc.previous_vertex_buffer_size = vertices.size();
	desc.index_buffer = resources.index;
	desc.index_array = resources.index_array;
	desc.index_buffer_size = indices.size();
	desc.material = resources.material;
	desc.generation = 1;
	desc.current_allocation_generation = 1;
	desc.previous_allocation_generation = 2;

	const real_t nan = std::numeric_limits<real_t>::quiet_NaN();
	const real_t infinity = std::numeric_limits<real_t>::infinity();
	const real_t largest = std::numeric_limits<real_t>::max();
	const AABB invalid_bounds[] = {
		AABB(Vector3(nan, 0, 0), Vector3(1, 1, 0)),
		AABB(Vector3(0, 0, nan), Vector3(1, 1, 0)),
		AABB(Vector3(0, 0, 0), Vector3(1, nan, 0)),
		AABB(Vector3(infinity, 0, 0), Vector3(1, 1, 0)),
		AABB(Vector3(0, 0, -infinity), Vector3(1, 1, 0)),
		AABB(Vector3(0, 0, 0), Vector3(1, infinity, 0)),
		AABB(Vector3(0, 0, 0), Vector3(1, 1, -infinity)),
		AABB(Vector3(0, 0, 0), Vector3(-1, 1, 0)),
		AABB(Vector3(0, 0, 0), Vector3(1, -1, 0)),
		AABB(Vector3(0, 0, 0), Vector3(1, 1, -1)),
		AABB(Vector3(largest, 0, 0), Vector3(largest, 1, 0)),
		AABB(Vector3(0, largest, 0), Vector3(1, largest, 0)),
		AABB(Vector3(0, 0, largest), Vector3(1, 1, largest)),
	};
	for (const AABB &bounds : invalid_bounds) {
		desc.aabb = bounds;
		uint32_t surface = 0xdeadbeef;
		uint64_t generation = 0x123456789abcdef0ULL;
		ERR_PRINT_OFF;
		const Error result = meshes.mesh_add_external_surface(resources.mesh, desc, surface, generation);
		ERR_PRINT_ON;
		CHECK(result != OK);
		CHECK(surface == 0xdeadbeef);
		CHECK(generation == 0x123456789abcdef0ULL);
		CHECK(meshes.mesh_get_surface_count(resources.mesh) == 0);
		if (result == OK) {
			B4_BOUNDS_CHECK_OR_RETURN(surface < uint32_t(meshes.mesh_get_surface_count(resources.mesh)));
			meshes.mesh_surface_remove(resources.mesh, surface);
		}
	}

	// A zero-thickness surface is valid, and a second finite surface must
	// produce the hand-computed union while retaining both surfaces.
	const AABB first_bounds(Vector3(-1, -1, 0), Vector3(2, 2, 0));
	const AABB second_bounds(Vector3(2, 2, 0), Vector3(2, 2, 0));
	desc.aabb = first_bounds;
	uint32_t surface = UINT32_MAX;
	uint64_t generation = 0;
	const Error first = meshes.mesh_add_external_surface(resources.mesh, desc, surface, generation);
	B4_BOUNDS_CHECK_OR_RETURN(first == OK);
	CHECK(meshes.mesh_get_aabb(resources.mesh) == first_bounds);
	CHECK(surface == 0);
	CHECK(generation == 1);
	desc.aabb = second_bounds;
	desc.generation = 2;
	const Error second = meshes.mesh_add_external_surface(resources.mesh, desc, surface, generation);
	B4_BOUNDS_CHECK_OR_RETURN(second == OK);
	CHECK(surface == 1);
	CHECK(generation == 2);
	CHECK(meshes.mesh_get_surface_count(resources.mesh) == 2);
	const AABB expected_union(Vector3(-1, -1, 0), Vector3(5, 5, 0));
	CHECK(meshes.mesh_get_aabb(resources.mesh) == expected_union);
	void *first_surface = meshes.mesh_get_surface(resources.mesh, uint32_t(0));
	void *second_surface = meshes.mesh_get_surface(resources.mesh, uint32_t(1));
	B4_BOUNDS_CHECK_OR_RETURN(first_surface != nullptr && second_surface != nullptr);
	CHECK(meshes.mesh_surface_get_aabb(first_surface) == first_bounds);
	CHECK(meshes.mesh_surface_get_aabb(second_surface) == second_bounds);

	// Invalid descriptor must not disturb already installed valid surfaces.
	for (const AABB &bad : { invalid_bounds[0], invalid_bounds[7] }) {
		desc.aabb = bad;
		surface = 0xdeadbeef;
		generation = 0x123456789abcdef0ULL;
		ERR_PRINT_OFF;
		const Error result = meshes.mesh_add_external_surface(resources.mesh, desc, surface, generation);
		ERR_PRINT_ON;
		CHECK(result != OK);
		CHECK(surface == 0xdeadbeef);
		CHECK(generation == 0x123456789abcdef0ULL);
		const int installed_count = meshes.mesh_get_surface_count(resources.mesh);
		CHECK(installed_count == 2);
		CHECK(meshes.mesh_get_aabb(resources.mesh) == expected_union);
		if (installed_count >= 2) {
			void *observed_first = meshes.mesh_get_surface(resources.mesh, uint32_t(0));
			void *observed_second = meshes.mesh_get_surface(resources.mesh, uint32_t(1));
			B4_BOUNDS_CHECK_OR_RETURN(observed_first != nullptr && observed_second != nullptr);
			CHECK(meshes.mesh_surface_get_aabb(observed_first) == first_bounds);
			CHECK(meshes.mesh_surface_get_aabb(observed_second) == second_bounds);
			CHECK(meshes.mesh_surface_get_material(resources.mesh, 0) == resources.material);
			CHECK(meshes.mesh_surface_get_material(resources.mesh, 1) == resources.material);
		}
		if (result == OK) {
			B4_BOUNDS_CHECK_OR_RETURN(surface < uint32_t(meshes.mesh_get_surface_count(resources.mesh)));
			meshes.mesh_surface_remove(resources.mesh, surface);
		}
	}
	B4_BOUNDS_CHECK_OR_RETURN(meshes.mesh_get_surface_count(resources.mesh) == 2);
	meshes.mesh_surface_remove(resources.mesh, 1);
	meshes.mesh_surface_remove(resources.mesh, 0);

	// Each box has finite endpoints; their union would need infinite extent.
	desc.generation = 1;
	desc.aabb = AABB(Vector3(-largest, 0, 0), Vector3(1, 1, 0));
	const Error low = meshes.mesh_add_external_surface(resources.mesh, desc, surface, generation);
	B4_BOUNDS_CHECK_OR_RETURN(low == OK);
	const AABB previous_bounds = meshes.mesh_get_aabb(resources.mesh);
	CHECK(previous_bounds == desc.aabb);
	desc.aabb = AABB(Vector3(largest, 0, 0), Vector3(1, 1, 0));
	surface = 0xdeadbeef;
	generation = 0x123456789abcdef0ULL;
	ERR_PRINT_OFF;
	const Error merged = meshes.mesh_add_external_surface(resources.mesh, desc, surface, generation);
	ERR_PRINT_ON;
	CHECK(merged != OK);
	CHECK(surface == 0xdeadbeef);
	CHECK(generation == 0x123456789abcdef0ULL);
	CHECK(meshes.mesh_get_surface_count(resources.mesh) == 1);
	CHECK(meshes.mesh_get_aabb(resources.mesh) == previous_bounds);
	if (merged == OK) {
		B4_BOUNDS_CHECK_OR_RETURN(surface < uint32_t(meshes.mesh_get_surface_count(resources.mesh)));
		meshes.mesh_surface_remove(resources.mesh, surface);
	}
	B4_BOUNDS_CHECK_OR_RETURN(meshes.mesh_get_surface_count(resources.mesh) == 1);
	meshes.mesh_surface_remove(resources.mesh, 0);

	// Ordinary Mesh admission is unchanged. A subsequent external surface
	// must fail if the existing aggregate itself is invalid.
	const AABB invalid_existing[] = {
		AABB(Vector3(nan, 0, 0), Vector3(1, 1, 0)),
		AABB(Vector3(0, 0, 0), Vector3(-1, 1, 0)),
		AABB(Vector3(largest, 0, 0), Vector3(largest, 1, 0)),
	};
	for (const AABB &bad : invalid_existing) {
		resources.ordinary_mesh = meshes.mesh_allocate();
		B4_BOUNDS_CHECK_OR_RETURN(resources.ordinary_mesh.is_valid());
		meshes.mesh_initialize(resources.ordinary_mesh);
		RenderingServerTypes::SurfaceData ordinary;
		ordinary.format = RSE::ARRAY_FORMAT_VERTEX | RSE::ARRAY_FLAG_FORMAT_CURRENT_VERSION;
		ordinary.primitive = RSE::PRIMITIVE_TRIANGLES;
		ordinary.vertex_count = 3;
		ordinary.vertex_data.resize(3 * 3 * sizeof(float));
		ordinary.material = resources.material;
		ordinary.aabb = bad;
		meshes.mesh_add_surface(resources.ordinary_mesh, ordinary);
		B4_BOUNDS_CHECK_OR_RETURN(meshes.mesh_get_surface_count(resources.ordinary_mesh) == 1);
		void *ordinary_surface = meshes.mesh_get_surface(resources.ordinary_mesh, uint32_t(0));
		B4_BOUNDS_CHECK_OR_RETURN(ordinary_surface != nullptr);
		CHECK(same_bounds(meshes.mesh_get_aabb(resources.ordinary_mesh), bad));
		CHECK(same_bounds(meshes.mesh_surface_get_aabb(ordinary_surface), bad));
		desc.aabb = first_bounds;
		surface = 0xdeadbeef;
		generation = 0x123456789abcdef0ULL;
		ERR_PRINT_OFF;
		const Error result = meshes.mesh_add_external_surface(resources.ordinary_mesh, desc, surface, generation);
		ERR_PRINT_ON;
		CHECK(result != OK);
		CHECK(surface == 0xdeadbeef);
		CHECK(generation == 0x123456789abcdef0ULL);
		const int ordinary_count = meshes.mesh_get_surface_count(resources.ordinary_mesh);
		CHECK(ordinary_count == 1);
		CHECK(same_bounds(meshes.mesh_get_aabb(resources.ordinary_mesh), bad));
		if (ordinary_count >= 1) {
			void *observed = meshes.mesh_get_surface(resources.ordinary_mesh, uint32_t(0));
			B4_BOUNDS_CHECK_OR_RETURN(observed != nullptr);
			CHECK(same_bounds(meshes.mesh_surface_get_aabb(observed), bad));
			CHECK(meshes.mesh_surface_get_material(resources.ordinary_mesh, 0) == resources.material);
		}
		meshes.mesh_free(resources.ordinary_mesh);
		resources.ordinary_mesh = RID();
	}
}
} // namespace TestB4ExternalBoundsGPU
#undef B4_BOUNDS_CHECK_OR_RETURN
#else
TEST_CASE("[B4][MaterialGPU] external surface bounds are finite and nonnegative" * doctest::skip()) {
	FAIL("Vulkan is not enabled in this build.");
}
#endif
