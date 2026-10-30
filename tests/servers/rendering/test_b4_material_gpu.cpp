/**************************************************************************/
/*  test_b4_material_gpu.cpp                                      */
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

#include <limits>
#ifdef VULKAN_ENABLED
#include "drivers/vulkan/rendering_context_driver_vulkan.h"
#endif

TEST_FORCE_LINK(test_b4_material_gpu)

#ifdef VULKAN_ENABLED
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

TEST_CASE("[B4][MaterialGPU] shader owners track source and texture revisions") {
	if (!OS::get_singleton()->has_environment("GODOT_TEST_B4_MATERIAL_GPU")) {
		MESSAGE("UNRUN: set GODOT_TEST_B4_MATERIAL_GPU=1 to require a Vulkan device.");
		return;
	}
	REQUIRE(RenderingDevice::get_singleton() == nullptr);
	RenderingContextDriverVulkan context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);
	RendererRD::TextureStorage textures;
	Storage materials;
	materials.shader_set_data_request_function(Storage::SHADER_TYPE_3D, shader_factory);
	materials.material_set_data_request_function(Storage::SHADER_TYPE_3D, material_factory);

	const RID shader = materials.shader_allocate();
	materials.shader_initialize(shader, false);
	materials.shader_set_code(shader, "shader_type spatial; void fragment() {} ");
	const RID a = materials.material_allocate();
	const RID b = materials.material_allocate();
	materials.material_initialize(a);
	materials.material_initialize(b);
	materials.material_set_shader(a, shader);
	materials.material_set_shader(b, shader);
	auto *shader_data = materials.material_get_shader_data(a);
	REQUIRE(shader_data != nullptr);
	const uint64_t a_before = materials.material_get_generation(a);
	const uint64_t b_before = materials.material_get_generation(b);
	materials.shader_set_code(shader, "shader_type spatial; void fragment() { ALBEDO = vec3(0.5); }");
	CHECK(materials.material_get_generation(a) > a_before);
	CHECK(materials.material_get_generation(b) > b_before);
	CHECK(materials.material_get_data(a, Storage::SHADER_TYPE_3D)->generation == materials.material_get_generation(a));

	Ref<Image> image = Image::create_empty(1, 1, false, Image::FORMAT_RGBA8);
	const RID texture = textures.texture_allocate();
	textures.texture_2d_initialize(texture, image);
	const uint64_t before_texture = materials.material_get_generation(a);
	const StringName key("probe_texture");
	materials.shader_set_default_texture_parameter(shader, key, texture, 0);
	CHECK(materials.material_get_generation(a) > before_texture);
	CHECK(materials.shader_get_default_texture_parameter(shader, key, 0) == texture);
	CHECK(shader_data->default_texture_params[key][0] == texture);
	materials.shader_set_default_texture_parameter(shader, key, b, 0);
	CHECK(materials.shader_get_default_texture_parameter(shader, key, 0).is_null());
	CHECK_FALSE(shader_data->default_texture_params.has(key));
	textures.texture_free(texture);
	materials.material_free(a);
	materials.material_free(b);
	materials.shader_free(shader);
}

TEST_CASE("[B4][MaterialGPU] external surface requires initialized material and preserves it") {
	if (!OS::get_singleton()->has_environment("GODOT_TEST_B4_MATERIAL_GPU")) {
		MESSAGE("UNRUN: set GODOT_TEST_B4_MATERIAL_GPU=1 to require a Vulkan device.");
		return;
	}
	REQUIRE(RenderingDevice::get_singleton() == nullptr);
	RenderingContextDriverVulkan context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);
	RendererRD::TextureStorage textures;
	Storage materials;
	RendererRD::MeshStorage meshes;

	const RID mesh = meshes.mesh_allocate();
	meshes.mesh_initialize(mesh);
	const RID material = materials.material_allocate();
	const RID replacement = materials.material_allocate();
	const RID uninitialized = materials.material_allocate();
	materials.material_initialize(material);
	materials.material_initialize(replacement);
	Vector<uint8_t> vertex_bytes;
	vertex_bytes.resize(144 * 3);
	const RID current = rd.vertex_buffer_create(vertex_bytes.size(), vertex_bytes, RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT);
	const RID previous = rd.vertex_buffer_create(vertex_bytes.size(), vertex_bytes, RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT);
	Vector<uint8_t> index_bytes;
	index_bytes.resize(3 * sizeof(uint32_t));
	const RID index = rd.index_buffer_create(3, RenderingDevice::INDEX_BUFFER_FORMAT_UINT32, index_bytes, false, RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT);
	const RID index_array = rd.index_array_create(index, 0, 3);
	REQUIRE(current.is_valid());
	REQUIRE(previous.is_valid());
	REQUIRE(index.is_valid());
	REQUIRE(index_array.is_valid());
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
	REQUIRE(meshes.mesh_add_external_surface(mesh, desc, surface, generation) == OK);
	CHECK(surface == 0);
	CHECK(generation == 1);
	meshes.mesh_surface_set_material(mesh, surface, material);
	CHECK(meshes.mesh_surface_get_material(mesh, surface) == material);
	ERR_PRINT_OFF;
	meshes.mesh_surface_set_material(mesh, surface, replacement);
	ERR_PRINT_ON;
	CHECK(meshes.mesh_surface_get_material(mesh, surface) == material);

	const RID ordinary_mesh = meshes.mesh_allocate();
	meshes.mesh_initialize(ordinary_mesh);
	RenderingServerTypes::SurfaceData ordinary;
	ordinary.format = RSE::ARRAY_FORMAT_VERTEX | RSE::ARRAY_FLAG_FORMAT_CURRENT_VERSION;
	ordinary.primitive = RSE::PRIMITIVE_TRIANGLES;
	ordinary.vertex_count = 3;
	ordinary.vertex_data.resize(3 * 3 * sizeof(float));
	ordinary.material = material;
	meshes.mesh_add_surface(ordinary_mesh, ordinary);
	REQUIRE(meshes.mesh_get_surface_count(ordinary_mesh) == 1);
	meshes.mesh_surface_set_material(ordinary_mesh, 0, replacement);
	CHECK(meshes.mesh_surface_get_material(ordinary_mesh, 0) == replacement);

	meshes.mesh_free(ordinary_mesh);
	meshes.mesh_free(mesh);
	rd.free_rid(index_array);
	rd.free_rid(index);
	rd.free_rid(previous);
	rd.free_rid(current);
	materials.material_initialize(uninitialized);
	materials.material_free(uninitialized);
	materials.material_free(replacement);
	materials.material_free(material);
}

} // namespace TestB4MaterialGPU
#endif
