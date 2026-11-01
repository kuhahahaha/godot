/**************************************************************************/
/*  test_b4_material_gpu.cpp                                              */
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
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"
#include "servers/rendering/rendering_device.h"
#include "tests/test_macros.h"

#include <cstdlib>
#include <cstring>
#ifdef VULKAN_ENABLED
#include "drivers/vulkan/rendering_context_driver_vulkan.h"
#endif

TEST_FORCE_LINK(test_b4_material_gpu)

#ifdef VULKAN_ENABLED
// RID ownership here records logical removal, not physical GPU retirement.
class TestB4RenderingDeviceAccess {
public:
	static bool owns_vertex_buffer(const RenderingDevice &p_device, RID p_rid) { return p_device.vertex_buffer_owner.owns(p_rid); }
	static bool owns_index_buffer(const RenderingDevice &p_device, RID p_rid) { return p_device.index_buffer_owner.owns(p_rid); }
	static bool owns_index_array(const RenderingDevice &p_device, RID p_rid) { return p_device.index_array_owner.owns(p_rid); }
};

namespace RendererRD {
class TestB4TextureCleanupAccess {
public:
	static void discard_uninitialized(TextureStorage &p_storage, RID p_texture) {
		p_storage.texture_owner.initialize_rid(p_texture, TextureStorage::Texture{});
		p_storage.texture_owner.free(p_texture);
	}
};
} // namespace RendererRD

#define B4_CHECK_OR_RETURN(m_condition) \
	do { \
		const bool b4_ok = bool(m_condition); \
		CHECK_MESSAGE(b4_ok, #m_condition); \
		if (!b4_ok) { \
			return; \
		} \
	} while (false)

namespace TestB4MaterialGPU {
using Storage = RendererRD::MaterialStorage;

// Real MaterialStorage and RD are used. These minimal shader-data objects
// isolate the owner/generation test from renderer-specific shader compilation.
struct ShaderData : Storage::ShaderData {
	String code;
	void set_code(const String &p_code) override { code = p_code; }
	bool is_animated() const override { return false; }
	bool casts_shadows() const override { return true; }
	RenderingServerTypes::ShaderNativeSourceCode get_native_source_code() const override { return {}; }
	Pair<ShaderRD *, RID> get_native_shader_and_version() const override { return {}; }
};
struct MaterialData : Storage::MaterialData {
	void set_render_priority(int) override {}
	void set_next_pass(RID) override {}
	bool update_parameters(const HashMap<StringName, Variant> &, bool, bool) override { return true; }
};
Storage::ShaderData *shader_factory() {
	return memnew(ShaderData);
}
Storage::MaterialData *material_factory(Storage::ShaderData *) {
	return memnew(MaterialData);
}

// Runs during doctest registration, before Godot's OS singleton exists.
bool gpu_tests_enabled() {
	const char *value = std::getenv("GODOT_TEST_B4_MATERIAL_GPU");
	return value && std::strcmp(value, "1") == 0;
}

// Every RID belongs to the real storage/device used in this test. In
// particular, the deliberately uninitialized reservation must be initialized
// before RID_Owner permits it to be freed. This also runs on explicit early returns.
struct Resources {
	RenderingDevice &rd;
	RendererRD::TextureStorage &textures;
	Storage &materials;
	RendererRD::MeshStorage *meshes = nullptr;
	RID shader;
	RID a;
	RID b;
	RID uninitialized;
	RID texture;
	bool texture_initialized = false;
	RID mesh;
	RID ordinary_mesh;
	RID current;
	RID previous;
	RID index;
	RID index_array;

	Resources(RenderingDevice &p_rd, RendererRD::TextureStorage &p_textures, Storage &p_materials, RendererRD::MeshStorage *p_meshes = nullptr) :
			rd(p_rd), textures(p_textures), materials(p_materials), meshes(p_meshes) {}

	~Resources() {
		if (ordinary_mesh.is_valid()) {
			meshes->mesh_free(ordinary_mesh);
		}
		if (mesh.is_valid()) {
			meshes->mesh_free(mesh);
		}
		for (RID rid : { index_array, index, previous, current }) {
			if (rid.is_valid()) {
				rd.free_rid(rid);
			}
		}
		if (uninitialized.is_valid()) {
			materials.material_initialize(uninitialized);
			materials.material_free(uninitialized);
		}
		if (b.is_valid()) {
			materials.material_free(b);
		}
		if (a.is_valid()) {
			materials.material_free(a);
		}
		if (shader.is_valid()) {
			materials.shader_free(shader);
		}
		if (texture.is_valid()) {
			if (texture_initialized) {
				textures.texture_free(texture);
			} else {
				RendererRD::TestB4TextureCleanupAccess::discard_uninitialized(textures, texture);
			}
		}
	}
};

TEST_CASE("[B4][MaterialGPU] shader owners track source and texture revisions" * doctest::skip(!gpu_tests_enabled())) {
	B4_CHECK_OR_RETURN(gpu_tests_enabled());
	B4_CHECK_OR_RETURN(RenderingDevice::get_singleton() == nullptr);
	RenderingContextDriverVulkan context;
	B4_CHECK_OR_RETURN(context.initialize() == OK);
	RenderingDevice rd;
	B4_CHECK_OR_RETURN(rd.initialize(&context) == OK);
	RendererRD::TextureStorage textures;
	Storage materials;
	Resources resources{ rd, textures, materials };
	materials.shader_set_data_request_function(Storage::SHADER_TYPE_3D, shader_factory);
	materials.material_set_data_request_function(Storage::SHADER_TYPE_3D, material_factory);
	materials.shader_set_data_request_function(Storage::SHADER_TYPE_2D, shader_factory);
	materials.material_set_data_request_function(Storage::SHADER_TYPE_2D, material_factory);

	resources.shader = materials.shader_allocate();
	const RID shader = resources.shader;
	B4_CHECK_OR_RETURN(shader.is_valid());
	materials.shader_initialize(shader, false);
	materials.shader_set_code(shader, "shader_type spatial; void fragment() {} ");
	resources.a = materials.material_allocate();
	const RID a = resources.a;
	B4_CHECK_OR_RETURN(a.is_valid());
	materials.material_initialize(a);
	resources.b = materials.material_allocate();
	const RID b = resources.b;
	B4_CHECK_OR_RETURN(b.is_valid());
	materials.material_initialize(b);
	materials.material_set_shader(a, shader);
	materials.material_set_shader(b, shader);
	auto *shader_data = materials.material_get_shader_data(a);
	B4_CHECK_OR_RETURN(shader_data != nullptr);
	const uint64_t a_before = materials.material_get_generation(a);
	const uint64_t b_before = materials.material_get_generation(b);
	const String revised_source = "shader_type spatial; void fragment() { ALBEDO = vec3(0.5); }";
	materials.shader_set_code(shader, revised_source);
	CHECK(materials.shader_get_code(shader) == revised_source);
	CHECK(static_cast<ShaderData *>(shader_data)->code == revised_source);
	CHECK(materials.material_get_generation(a) > a_before);
	CHECK(materials.material_get_generation(b) > b_before);
	auto *data_a = materials.material_get_data(a, Storage::SHADER_TYPE_3D);
	auto *data_b = materials.material_get_data(b, Storage::SHADER_TYPE_3D);
	B4_CHECK_OR_RETURN(data_a != nullptr);
	B4_CHECK_OR_RETURN(data_b != nullptr);
	CHECK(data_a->generation == materials.material_get_generation(a));
	CHECK(data_b->generation == materials.material_get_generation(b));

	Ref<Image> image = Image::create_empty(1, 1, false, Image::FORMAT_RGBA8);
	B4_CHECK_OR_RETURN(image.is_valid());
	resources.texture = textures.texture_allocate();
	const RID texture = resources.texture;
	B4_CHECK_OR_RETURN(texture.is_valid());
	textures.texture_2d_initialize(texture, image);
	resources.texture_initialized = textures.texture_get_rd_texture(texture).is_valid();
	B4_CHECK_OR_RETURN(resources.texture_initialized);
	const uint64_t before_texture = materials.material_get_generation(a);
	const uint64_t b_before_texture = materials.material_get_generation(b);
	const StringName key("probe_texture");
	materials.shader_set_default_texture_parameter(shader, key, texture, 0);
	CHECK(materials.material_get_generation(a) > before_texture);
	CHECK(materials.material_get_generation(b) > b_before_texture);
	CHECK(data_a->generation == materials.material_get_generation(a));
	CHECK(data_b->generation == materials.material_get_generation(b));
	CHECK(materials.shader_get_default_texture_parameter(shader, key, 0) == texture);
	const auto *bound_textures = shader_data->default_texture_params.getptr(key);
	B4_CHECK_OR_RETURN(bound_textures != nullptr);
	const RID *bound_texture = bound_textures->getptr(0);
	B4_CHECK_OR_RETURN(bound_texture != nullptr);
	CHECK(*bound_texture == texture);
	materials.shader_set_default_texture_parameter(shader, key, b, 0);
	CHECK(materials.shader_get_default_texture_parameter(shader, key, 0).is_null());
	CHECK_FALSE(shader_data->default_texture_params.has(key));
	const uint64_t a_before_switch = materials.material_get_generation(a);
	const uint64_t b_before_switch = materials.material_get_generation(b);
	const String canvas_source = "shader_type canvas_item; void fragment() { COLOR = vec4(0.25); }";
	materials.shader_set_code(shader, canvas_source);
	CHECK(materials.shader_get_code(shader) == canvas_source);
	auto *canvas_shader_data = materials.material_get_shader_data(a);
	B4_CHECK_OR_RETURN(canvas_shader_data != nullptr);
	CHECK(static_cast<ShaderData *>(canvas_shader_data)->code == canvas_source);
	auto *canvas_a = materials.material_get_data(a, Storage::SHADER_TYPE_2D);
	auto *canvas_b = materials.material_get_data(b, Storage::SHADER_TYPE_2D);
	B4_CHECK_OR_RETURN(canvas_a != nullptr);
	B4_CHECK_OR_RETURN(canvas_b != nullptr);
	CHECK(materials.material_get_generation(a) > a_before_switch);
	CHECK(materials.material_get_generation(b) > b_before_switch);
	CHECK(canvas_a->generation == materials.material_get_generation(a));
	CHECK(canvas_b->generation == materials.material_get_generation(b));
}

TEST_CASE("[B4][MaterialGPU] external surface requires initialized material and preserves it" * doctest::skip(!gpu_tests_enabled())) {
	B4_CHECK_OR_RETURN(gpu_tests_enabled());
	B4_CHECK_OR_RETURN(RenderingDevice::get_singleton() == nullptr);
	RenderingContextDriverVulkan context;
	B4_CHECK_OR_RETURN(context.initialize() == OK);
	RenderingDevice rd;
	B4_CHECK_OR_RETURN(rd.initialize(&context) == OK);
	RendererRD::TextureStorage textures;
	Storage materials;
	RendererRD::MeshStorage meshes;
	Resources resources{ rd, textures, materials, &meshes };

	resources.mesh = meshes.mesh_allocate();
	const RID mesh = resources.mesh;
	B4_CHECK_OR_RETURN(mesh.is_valid());
	meshes.mesh_initialize(mesh);
	resources.a = materials.material_allocate();
	const RID material = resources.a;
	B4_CHECK_OR_RETURN(material.is_valid());
	materials.material_initialize(material);
	resources.b = materials.material_allocate();
	const RID replacement = resources.b;
	B4_CHECK_OR_RETURN(replacement.is_valid());
	materials.material_initialize(replacement);
	resources.uninitialized = materials.material_allocate();
	const RID uninitialized = resources.uninitialized;
	B4_CHECK_OR_RETURN(uninitialized.is_valid());
	Vector<uint8_t> vertex_bytes;
	vertex_bytes.resize(144 * 3);
	resources.current = rd.vertex_buffer_create(vertex_bytes.size(), vertex_bytes, RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT);
	resources.previous = rd.vertex_buffer_create(vertex_bytes.size(), vertex_bytes, RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT);
	const RID current = resources.current;
	const RID previous = resources.previous;
	Vector<uint8_t> index_bytes;
	index_bytes.resize(3 * sizeof(uint32_t));
	resources.index = rd.index_buffer_create(3, RenderingDevice::INDEX_BUFFER_FORMAT_UINT32, index_bytes, false, RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT);
	const RID index = resources.index;
	if (index.is_valid()) {
		resources.index_array = rd.index_array_create(index, 0, 3);
	}
	const RID index_array = resources.index_array;
	B4_CHECK_OR_RETURN(current.is_valid());
	B4_CHECK_OR_RETURN(previous.is_valid());
	B4_CHECK_OR_RETURN(index.is_valid());
	B4_CHECK_OR_RETURN(index_array.is_valid());
	RendererRD::MeshStorage::ExternalSurfaceDescriptor desc;
	desc.vertex_count = 3;
	desc.index_count = 3;
	desc.current_vertex_buffer = current;
	desc.previous_vertex_buffer = previous;
	desc.vertex_buffer_size = desc.previous_vertex_buffer_size = vertex_bytes.size();
	desc.index_buffer = index;
	desc.index_array = index_array;
	desc.index_buffer_size = index_bytes.size();
	desc.aabb = AABB(Vector3(-1, -1, 0), Vector3(2, 2, 0));
	desc.generation = 1;
	desc.current_allocation_generation = 1;
	desc.previous_allocation_generation = 2;
	uint32_t surface = UINT32_MAX;
	uint64_t generation = 0;
	desc.material = uninitialized;
	ERR_PRINT_OFF;
	Error uninitialized_result = meshes.mesh_add_external_surface(mesh, desc, surface, generation);
	ERR_PRINT_ON;
	CHECK(uninitialized_result != OK);
	CHECK(meshes.mesh_get_surface_count(mesh) == 0);
	if (uninitialized_result == OK) {
		meshes.mesh_surface_remove(mesh, surface);
	}
	desc.material = current; // A real RD buffer RID, not a material RID.
	ERR_PRINT_OFF;
	Error foreign_result = meshes.mesh_add_external_surface(mesh, desc, surface, generation);
	ERR_PRINT_ON;
	CHECK(foreign_result != OK);
	CHECK(meshes.mesh_get_surface_count(mesh) == 0);
	if (foreign_result == OK) {
		meshes.mesh_surface_remove(mesh, surface);
	}
	desc.material = material;
	B4_CHECK_OR_RETURN(meshes.mesh_add_external_surface(mesh, desc, surface, generation) == OK);
	CHECK(surface == 0);
	CHECK(generation == 1);
	meshes.mesh_surface_set_material(mesh, surface, material);
	CHECK(meshes.mesh_surface_get_material(mesh, surface) == material);
	ERR_PRINT_OFF;
	meshes.mesh_surface_set_material(mesh, surface, replacement);
	ERR_PRINT_ON;
	CHECK(meshes.mesh_surface_get_material(mesh, surface) == material);

	resources.ordinary_mesh = meshes.mesh_allocate();
	const RID ordinary_mesh = resources.ordinary_mesh;
	B4_CHECK_OR_RETURN(ordinary_mesh.is_valid());
	meshes.mesh_initialize(ordinary_mesh);
	RenderingServerTypes::SurfaceData ordinary;
	ordinary.format = RSE::ARRAY_FORMAT_VERTEX | RSE::ARRAY_FLAG_FORMAT_CURRENT_VERSION;
	ordinary.primitive = RSE::PRIMITIVE_TRIANGLES;
	ordinary.vertex_count = 3;
	ordinary.vertex_data.resize(3 * 3 * sizeof(float));
	ordinary.material = material;
	meshes.mesh_add_surface(ordinary_mesh, ordinary);
	B4_CHECK_OR_RETURN(meshes.mesh_get_surface_count(ordinary_mesh) == 1);
	meshes.mesh_surface_set_material(ordinary_mesh, 0, replacement);
	CHECK(meshes.mesh_surface_get_material(ordinary_mesh, 0) == replacement);
}

TEST_CASE("[B4][MaterialGPU] fixture releases resources on early return" * doctest::skip(!gpu_tests_enabled())) {
	B4_CHECK_OR_RETURN(gpu_tests_enabled());
	B4_CHECK_OR_RETURN(RenderingDevice::get_singleton() == nullptr);
	RenderingContextDriverVulkan context;
	B4_CHECK_OR_RETURN(context.initialize() == OK);
	RenderingDevice rd;
	B4_CHECK_OR_RETURN(rd.initialize(&context) == OK);
	RendererRD::TextureStorage textures;
	Storage materials;
	RendererRD::MeshStorage meshes;
	RID shader, material_a, material_b, reserved_material, reserved_texture;
	RID mesh, ordinary_mesh, current, previous, index, index_array;
	bool created = false;
	auto exit_early = [&]() {
		Resources resources{ rd, textures, materials, &meshes };
		resources.shader = shader = materials.shader_allocate();
		B4_CHECK_OR_RETURN(shader.is_valid());
		materials.shader_initialize(shader, false);
		resources.a = material_a = materials.material_allocate();
		B4_CHECK_OR_RETURN(material_a.is_valid());
		materials.material_initialize(material_a);
		resources.b = material_b = materials.material_allocate();
		B4_CHECK_OR_RETURN(material_b.is_valid());
		materials.material_initialize(material_b);
		resources.uninitialized = reserved_material = materials.material_allocate();
		resources.texture = reserved_texture = textures.texture_allocate();
		resources.mesh = mesh = meshes.mesh_allocate();
		B4_CHECK_OR_RETURN(mesh.is_valid());
		meshes.mesh_initialize(mesh);
		resources.ordinary_mesh = ordinary_mesh = meshes.mesh_allocate();
		B4_CHECK_OR_RETURN(ordinary_mesh.is_valid());
		meshes.mesh_initialize(ordinary_mesh);
		Vector<uint8_t> vertices;
		vertices.resize(144 * 3);
		Vector<uint8_t> indices;
		indices.resize(3 * sizeof(uint32_t));
		resources.current = current = rd.vertex_buffer_create(vertices.size(), vertices, RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT);
		resources.previous = previous = rd.vertex_buffer_create(vertices.size(), vertices, RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT);
		resources.index = index = rd.index_buffer_create(3, RenderingDevice::INDEX_BUFFER_FORMAT_UINT32, indices, false, RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT);
		if (index.is_valid()) {
			resources.index_array = index_array = rd.index_array_create(index, 0, 3);
		}
		B4_CHECK_OR_RETURN(shader.is_valid() && material_a.is_valid() && material_b.is_valid());
		B4_CHECK_OR_RETURN(reserved_material.is_valid() && reserved_texture.is_valid());
		B4_CHECK_OR_RETURN(mesh.is_valid() && ordinary_mesh.is_valid());
		B4_CHECK_OR_RETURN(current.is_valid() && previous.is_valid() && index.is_valid() && index_array.is_valid());
		B4_CHECK_OR_RETURN(rd.validate_external_indexed_surface_buffers(current, previous, index, index_array, 3, 3));
		B4_CHECK_OR_RETURN(TestB4RenderingDeviceAccess::owns_vertex_buffer(rd, current));
		B4_CHECK_OR_RETURN(TestB4RenderingDeviceAccess::owns_vertex_buffer(rd, previous));
		B4_CHECK_OR_RETURN(TestB4RenderingDeviceAccess::owns_index_buffer(rd, index));
		B4_CHECK_OR_RETURN(TestB4RenderingDeviceAccess::owns_index_array(rd, index_array));
		created = true;
		return; // Exercise the same destructor path as a failed precondition.
	};
	exit_early();
	B4_CHECK_OR_RETURN(created);
	CHECK_FALSE(materials.owns_shader(shader));
	CHECK_FALSE(materials.owns_material(material_a));
	CHECK_FALSE(materials.owns_material(material_b));
	CHECK_FALSE(materials.owns_material(reserved_material));
	CHECK_FALSE(textures.owns_texture(reserved_texture));
	CHECK_FALSE(meshes.owns_mesh(mesh));
	CHECK_FALSE(meshes.owns_mesh(ordinary_mesh));
	CHECK_FALSE(TestB4RenderingDeviceAccess::owns_vertex_buffer(rd, current));
	CHECK_FALSE(TestB4RenderingDeviceAccess::owns_vertex_buffer(rd, previous));
	CHECK_FALSE(TestB4RenderingDeviceAccess::owns_index_buffer(rd, index));
	CHECK_FALSE(TestB4RenderingDeviceAccess::owns_index_array(rd, index_array));
}

} // namespace TestB4MaterialGPU
#undef B4_CHECK_OR_RETURN
#else
TEST_CASE("[B4][MaterialGPU] shader owners track source and texture revisions" * doctest::skip()) {
	FAIL("Vulkan is not enabled in this build.");
}
TEST_CASE("[B4][MaterialGPU] external surface requires initialized material and preserves it" * doctest::skip()) {
	FAIL("Vulkan is not enabled in this build.");
}
TEST_CASE("[B4][MaterialGPU] fixture releases resources on early return" * doctest::skip()) {
	FAIL("Vulkan is not enabled in this build.");
}
#endif
