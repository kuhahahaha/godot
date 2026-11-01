/**************************************************************************/
/*  test_b4_render_list.cpp                                               */
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

// This controlled fixture calls the real list producer using real mesh,
// shader, material, geometry and surface-cache owners. The packet and the
// cache generation mutation are test inputs, not an external-output issuer.
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_b4_render_list)

#if defined(TESTS_ENABLED) && defined(RD_ENABLED) && defined(MACOS_ENABLED)
#include "core/templates/paged_array.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/shader.h"
#include "servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.h"
#include "servers/rendering/renderer_rd/forward_mobile/render_forward_mobile.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/render_data_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_data_rd.h"

#include <cstdio>
#include <type_traits>
#include <utility>

namespace RendererSceneRenderImplementation {
class TestB4RenderListAccess {
public:
	template <typename R>
	static bool run_multimesh_dependencies(R *r, RID mesh) {
		constexpr bool mobile = std::is_same_v<R, RenderForwardMobile>;
		using Instance = std::conditional_t<mobile, RenderForwardMobile::GeometryInstanceForwardMobile, RenderForwardClustered::GeometryInstanceForwardClustered>;
		RendererRD::MeshStorage *storage = RendererRD::MeshStorage::get_singleton();
		if (!r || !storage) {
			return false;
		}
		RID multimesh = storage->multimesh_allocate();
		if (!multimesh.is_valid()) {
			return false;
		}
		storage->multimesh_initialize(multimesh);
		struct Cleanup {
			R *renderer;
			RendererRD::MeshStorage *storage;
			RID multimesh;
			Instance *instance = nullptr;
			~Cleanup() {
				if (instance) {
					renderer->geometry_instance_free(instance);
				}
				storage->multimesh_free(multimesh);
			}
		} cleanup{ r, storage, multimesh };
		storage->multimesh_allocate_data(multimesh, 3, RSE::MULTIMESH_TRANSFORM_3D, false, false, false);
		storage->multimesh_set_mesh(multimesh, mesh);
		if (!storage->multimesh_get_buffer_rd_rid(multimesh).is_valid()) {
			return false;
		}
		Instance *instance = static_cast<Instance *>(r->geometry_instance_create(multimesh));
		if (!instance) {
			return false;
		}
		cleanup.instance = instance;
		// SceneCull uses this public setter to start initial dependency collection.
		instance->set_surface_materials(Vector<RID>());
		r->_update_dirty_geometry_instances();
		auto observe = [&](uint32_t p_step, bool p_passed) {
			if (!p_passed) {
				std::printf("B4_MM_DEPENDENCY_CHECK step=%u count=%u dirty=%d cache=%d\n", p_step, uint32_t(instance->instance_count), int(instance->data->dirty_dependencies), int(instance->surface_caches != nullptr));
			}
			return p_passed;
		};
		bool ok = observe(1, instance->instance_count == 3 && instance->surface_caches && !instance->data->dirty_dependencies);
		storage->multimesh_set_visible_instances(multimesh, 1);
		ok &= observe(2, instance->instance_count == 1);
		storage->multimesh_set_visible_instances(multimesh, 0);
		ok &= observe(3, instance->instance_count == 0);
		storage->multimesh_set_visible_instances(multimesh, -1);
		ok &= observe(4, instance->instance_count == 3);
		storage->multimesh_allocate_data(multimesh, 5, RSE::MULTIMESH_TRANSFORM_3D, false, false, false);
		ok &= observe(5, instance->data->dirty_dependencies && instance->surface_caches == nullptr);
		r->_update_dirty_geometry_instances();
		ok &= observe(6, instance->instance_count == 5 && instance->surface_caches && !instance->data->dirty_dependencies);
		// Empty buffer and empty mesh must retain the owner subscription, so
		// subsequent normal allocations and mesh restoration refresh the cache.
		storage->multimesh_allocate_data(multimesh, 0, RSE::MULTIMESH_TRANSFORM_3D, false, false, false);
		ok &= observe(7, instance->data->dirty_dependencies && instance->surface_caches == nullptr);
		r->_update_dirty_geometry_instances();
		ok &= observe(8, instance->instance_count == 0 && instance->surface_caches == nullptr);
		storage->multimesh_allocate_data(multimesh, 3, RSE::MULTIMESH_TRANSFORM_3D, false, false, false);
		ok &= observe(9, instance->data->dirty_dependencies);
		r->_update_dirty_geometry_instances();
		ok &= observe(10, instance->instance_count == 3 && instance->surface_caches);
		storage->multimesh_set_mesh(multimesh, RID());
		ok &= observe(11, instance->data->dirty_dependencies && instance->surface_caches == nullptr);
		r->_update_dirty_geometry_instances();
		ok &= observe(12, instance->surface_caches == nullptr);
		storage->multimesh_set_mesh(multimesh, mesh);
		ok &= observe(13, instance->data->dirty_dependencies);
		r->_update_dirty_geometry_instances();
		ok &= observe(14, instance->instance_count == 3 && instance->surface_caches && !instance->data->dirty_dependencies);
		// Retaining the original mesh dependency still invalidates the cache.
		const RID material = storage->mesh_surface_get_material(mesh, 0);
		storage->mesh_surface_set_material(mesh, 0, material);
		ok &= observe(15, instance->data->dirty_dependencies && instance->surface_caches == nullptr);
		r->_update_dirty_geometry_instances();
		ok &= observe(16, instance->instance_count == 3 && instance->surface_caches && !instance->data->dirty_dependencies);
		return ok;
	}
	template <typename R>
	static bool run(R *r, RID mesh) {
		constexpr bool mobile = std::is_same_v<R, RenderForwardMobile>;
		using Instance = std::conditional_t<mobile, RenderForwardMobile::GeometryInstanceForwardMobile, RenderForwardClustered::GeometryInstanceForwardClustered>;
		using Cache = typename R::GeometryInstanceSurfaceDataCache;
		if (!r) {
			return false;
		}
		Instance *instance = static_cast<Instance *>(r->geometry_instance_create(mesh));
		if (!instance) {
			return false;
		}
		r->_update_dirty_geometry_instances();
		Cache *a = instance->surface_caches;
		Cache *b = a ? a->next : nullptr;
		Cache *bad = b ? b->next : nullptr;
		if (!bad || bad->next) {
			r->geometry_instance_free(instance);
			return false;
		}
		// All three surface entries are real; mutate only the expected generation
		// to exercise the current packet admission seam deterministically.
		a->material_generation = b->material_generation = 12;
		bad->material_generation = 13;
		if (!(a->flags & Cache::FLAG_PASS_OPAQUE) || !(b->flags & Cache::FLAG_PASS_ALPHA) || !(bad->flags & Cache::FLAG_PASS_OPAQUE)) {
			std::printf("B4_LIST_REAL_MATERIALS=0\n");
			r->geometry_instance_free(instance);
			return false;
		}
		std::printf("B4_LIST_REAL_MATERIALS=1\n");
		typename R::RenderList saved[R::RENDER_LIST_MAX];
		for (int i = 0; i < R::RENDER_LIST_MAX; ++i) {
			saved[i] = std::move(r->render_list[i]);
		}
		bool ok = true;
		{
			PagedArrayPool<RenderGeometryInstance *> pool(8);
			PagedArray<RenderGeometryInstance *> instances;
			instances.set_page_pool(&pool);
			instances.push_back(instance);
			RenderSceneDataRD scene;
			scene.cam_projection.set_perspective(60, 1, 0.1, 100);
			scene.screen_mesh_lod_threshold = 0;
			RenderDataRD data;
			data.scene_data = &scene;
			data.instances = &instances;
			data.packet_identity.external = true;
			data.packet_identity.material_generation = 12;
			if constexpr (mobile) {
				r->_fill_render_list(R::RENDER_LIST_OPAQUE, &data, R::PASS_MODE_COLOR);
			} else {
				r->_fill_render_list(R::RENDER_LIST_OPAQUE, &data, R::PASS_MODE_COLOR, false, false, true);
			}
			bool color = data.packet_identity.retired && r->render_list[R::RENDER_LIST_OPAQUE].elements.size() == 0 && r->render_list[R::RENDER_LIST_ALPHA].elements.size() == 0;
			if constexpr (!mobile) {
				color = color && r->render_list[R::RENDER_LIST_MOTION].elements.size() == 0;
			}
			std::printf("B4_LIST_COLOR_ATOMIC=%d\n", color);
			ok &= color;
			r->render_list[R::RENDER_LIST_SECONDARY].clear();
			r->render_list[R::RENDER_LIST_SECONDARY].add_element(a);
			data.packet_identity.retired = false;
			data.packet_identity.external = true;
			data.packet_identity.material_generation = 12;
			if constexpr (mobile) {
				r->_fill_render_list(R::RENDER_LIST_SECONDARY, &data, R::PASS_MODE_SHADOW, true);
			} else {
				r->_fill_render_list(R::RENDER_LIST_SECONDARY, &data, R::PASS_MODE_SHADOW, false, false, false, true);
			}
			auto &shadow = r->render_list[R::RENDER_LIST_SECONDARY].elements;
			bool append = data.packet_identity.retired && shadow.size() == 1 && shadow[0] == a;
			std::printf("B4_LIST_SHADOW_APPEND=%d\n", append);
			ok &= append;
			bad->material_generation = 12;
			data.packet_identity.retired = false;
			data.packet_identity.external = true;
			data.packet_identity.material_generation = 12;
			if constexpr (mobile) {
				r->_fill_render_list(R::RENDER_LIST_OPAQUE, &data, R::PASS_MODE_COLOR);
			} else {
				r->_fill_render_list(R::RENDER_LIST_OPAQUE, &data, R::PASS_MODE_COLOR, false, false, true);
			}
			bool recovery = !data.packet_identity.retired && r->render_list[R::RENDER_LIST_OPAQUE].elements.size() == 2 && r->render_list[R::RENDER_LIST_ALPHA].elements.size() == 1;
			if constexpr (!mobile) {
				recovery = recovery && r->render_list[R::RENDER_LIST_MOTION].elements.size() == 2;
			}
			std::printf("B4_LIST_RECOVERY=%d\n", recovery);
			ok &= recovery;
			bad->material_generation = 13;
			data.packet_identity.external = false;
			data.packet_identity.retired = false;
			if constexpr (mobile) {
				r->_fill_render_list(R::RENDER_LIST_OPAQUE, &data, R::PASS_MODE_COLOR);
			} else {
				r->_fill_render_list(R::RENDER_LIST_OPAQUE, &data, R::PASS_MODE_COLOR, false, false, true);
			}
			bool ordinary = !data.packet_identity.retired && r->render_list[R::RENDER_LIST_OPAQUE].elements.size() == 2 && r->render_list[R::RENDER_LIST_ALPHA].elements.size() == 1;
			if constexpr (!mobile) {
				ordinary = ordinary && r->render_list[R::RENDER_LIST_MOTION].elements.size() == 2;
			}
			std::printf("B4_LIST_ORDINARY=%d\n", ordinary);
			ok &= ordinary;
			a->material_generation = 13;
			bad->material_generation = 12;
			data.packet_identity.external = true;
			data.packet_identity.retired = false;
			data.packet_identity.material_generation = 12;
			if constexpr (mobile) {
				r->_fill_render_list(R::RENDER_LIST_OPAQUE, &data, R::PASS_MODE_COLOR);
			} else {
				r->_fill_render_list(R::RENDER_LIST_OPAQUE, &data, R::PASS_MODE_COLOR, false, false, true);
			}
			bool first = data.packet_identity.retired && r->render_list[R::RENDER_LIST_OPAQUE].elements.size() == 0 && r->render_list[R::RENDER_LIST_ALPHA].elements.size() == 0;
			if constexpr (!mobile) {
				first = first && r->render_list[R::RENDER_LIST_MOTION].elements.size() == 0;
			}
			std::printf("B4_LIST_FIRST_MISMATCH=%d\n", first);
			ok &= first;
			a->material_generation = 12;
			r->render_list[R::RENDER_LIST_OPAQUE].add_element(a);
			r->render_list[R::RENDER_LIST_ALPHA].add_element(b);
			if constexpr (!mobile) {
				r->render_list[R::RENDER_LIST_MOTION].add_element(a);
			}
			data.packet_identity.external = true;
			data.packet_identity.retired = true;
			data.packet_identity.material_generation = 12;
			if constexpr (mobile) {
				r->_fill_render_list(R::RENDER_LIST_OPAQUE, &data, R::PASS_MODE_COLOR);
			} else {
				r->_fill_render_list(R::RENDER_LIST_OPAQUE, &data, R::PASS_MODE_COLOR, false, false, true);
			}
			bool retired = data.packet_identity.retired && r->render_list[R::RENDER_LIST_OPAQUE].elements.size() == 0 && r->render_list[R::RENDER_LIST_ALPHA].elements.size() == 0;
			if constexpr (!mobile) {
				retired = retired && r->render_list[R::RENDER_LIST_MOTION].elements.size() == 0;
			}
			std::printf("B4_LIST_RETIRED=%d\n", retired);
			ok &= retired;
			instances.clear();
			r->render_list[R::RENDER_LIST_OPAQUE].add_element(a);
			r->render_list[R::RENDER_LIST_ALPHA].add_element(b);
			if constexpr (!mobile) {
				r->render_list[R::RENDER_LIST_MOTION].add_element(a);
			}
			data.packet_identity.external = true;
			data.packet_identity.retired = false;
			data.packet_identity.material_generation = 12;
			if constexpr (mobile) {
				r->_fill_render_list(R::RENDER_LIST_OPAQUE, &data, R::PASS_MODE_COLOR);
			} else {
				r->_fill_render_list(R::RENDER_LIST_OPAQUE, &data, R::PASS_MODE_COLOR, false, false, true);
			}
			bool empty = !data.packet_identity.retired && r->render_list[R::RENDER_LIST_OPAQUE].elements.size() == 0 && r->render_list[R::RENDER_LIST_ALPHA].elements.size() == 0;
			if constexpr (!mobile) {
				empty = empty && r->render_list[R::RENDER_LIST_MOTION].elements.size() == 0;
			}
			r->render_list[R::RENDER_LIST_SECONDARY].clear();
			r->render_list[R::RENDER_LIST_SECONDARY].add_element(a);
			if constexpr (mobile) {
				r->_fill_render_list(R::RENDER_LIST_SECONDARY, &data, R::PASS_MODE_SHADOW, true);
			} else {
				r->_fill_render_list(R::RENDER_LIST_SECONDARY, &data, R::PASS_MODE_SHADOW, false, false, false, true);
			}
			empty = empty && r->render_list[R::RENDER_LIST_SECONDARY].elements.size() == 1 && r->render_list[R::RENDER_LIST_SECONDARY].elements[0] == a;
			std::printf("B4_LIST_EMPTY=%d\n", empty);
			ok &= empty;
			for (int i = 0; i < R::RENDER_LIST_MAX; ++i) {
				r->render_list[i].clear();
			}
		}
		r->geometry_instance_free(instance);
		for (int i = 0; i < R::RENDER_LIST_MAX; ++i) {
			r->render_list[i] = std::move(saved[i]);
		}
		return ok;
	}
};
} // namespace RendererSceneRenderImplementation

#ifdef _WIN32
#define B4_FIXTURE_EXPORT __declspec(dllexport)
#else
#define B4_FIXTURE_EXPORT __attribute__((used, visibility("default")))
#endif
extern "C" B4_FIXTURE_EXPORT int b4_render_list_fixture_run() {
	using namespace RendererSceneRenderImplementation;
	Ref<Shader> opaque_shader, alpha_shader;
	opaque_shader.instantiate();
	alpha_shader.instantiate();
	opaque_shader->set_code("shader_type spatial; render_mode unshaded, cull_disabled; void fragment() { ALBEDO = vec3(1.0); }");
	alpha_shader->set_code("shader_type spatial; render_mode unshaded, cull_disabled; void fragment() { ALBEDO = vec3(1.0); ALPHA = 0.5; }");
	Ref<ShaderMaterial> opaque, alpha;
	opaque.instantiate();
	alpha.instantiate();
	opaque->set_shader(opaque_shader);
	alpha->set_shader(alpha_shader);
	PackedVector3Array vertices;
	vertices.push_back(Vector3(-1, -1, 0));
	vertices.push_back(Vector3(1, -1, 0));
	vertices.push_back(Vector3(0, 1, 0));
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	for (int i = 0; i < 3; ++i) {
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
		mesh->surface_set_material(i, i == 1 ? alpha : opaque);
	}
	if (mesh->get_surface_count() != 3) {
		return ERR_INVALID_DATA;
	}
	bool is_mobile = RenderForwardMobile::get_singleton() != nullptr;
	bool is_clustered = RenderForwardClustered::get_singleton() != nullptr;
	if (is_mobile == is_clustered) {
		return ERR_UNAVAILABLE;
	}
	bool ok = is_mobile ? TestB4RenderListAccess::run(RenderForwardMobile::get_singleton(), mesh->get_rid()) : TestB4RenderListAccess::run(RenderForwardClustered::get_singleton(), mesh->get_rid());
	bool multimesh_ok = is_mobile ? TestB4RenderListAccess::run_multimesh_dependencies(RenderForwardMobile::get_singleton(), mesh->get_rid()) : TestB4RenderListAccess::run_multimesh_dependencies(RenderForwardClustered::get_singleton(), mesh->get_rid());
	std::printf("B4_LIST_MULTIMESH_DEPENDENCY=%d\n", multimesh_ok);
	ok &= multimesh_ok;
	std::printf("B4_LIST_ALL=%d\n", ok);
	std::fflush(stdout);
	return OK; // Runner asserts observations and verifies clean extension unload on red.
}
#endif
