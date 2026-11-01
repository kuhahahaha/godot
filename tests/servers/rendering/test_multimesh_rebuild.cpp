/**************************************************************************/
/*  test_multimesh_rebuild.cpp                                            */
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
#include "servers/rendering/storage/mesh_storage.h"
#include "tests/test_macros.h"

#ifdef VULKAN_ENABLED
#include "drivers/vulkan/rendering_context_driver_vulkan.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"
#include "servers/rendering/rendering_device.h"

#include <cstdlib>
#include <cstring>
#endif

TEST_FORCE_LINK(test_multimesh_rebuild)

namespace RendererRD {
class TestMultimeshRebuildAccess {
public:
#ifdef VULKAN_ENABLED
	enum Failure { SIZE,
		MISSING_BUFFER,
		CACHE_MISMATCH,
		CACHE_RESIZE };
	static bool prepare(MeshStorage &p_storage, RID p_rid, Failure p_failure) {
		MeshStorage::MultiMesh *m = p_storage.multimesh_owner.get_or_null(p_rid);
		if (!m) {
			return false;
		}
		if (p_failure == MISSING_BUFFER) {
			RenderingDevice::get_singleton()->free_rid(m->buffer);
			m->buffer = RID();
		}
		if (p_failure == CACHE_MISMATCH || p_failure == CACHE_RESIZE) {
			return m->data_cache.resize(p_failure == CACHE_MISMATCH ? 1 : 8) == OK;
		}
		return true;
	}
	static void track(MeshStorage &p_storage, RID p_rid, DependencyTracker &p_tracker) {
		MeshStorage::MultiMesh *m = p_storage.multimesh_owner.get_or_null(p_rid);
		if (m) {
			p_tracker.update_dependency(&m->dependency);
		}
	}
	static bool enable(MeshStorage &p_storage, RID p_rid, Failure p_failure) {
		MeshStorage::MultiMesh *m = p_storage.multimesh_owner.get_or_null(p_rid);
		if (!m) {
			return true;
		}
		const int saved_instances = m->instances;
		if (p_failure == SIZE) {
			m->instances = 67108864;
		}
		MeshStorage::test_fail_motion_cache_resize = p_failure == CACHE_RESIZE;
		const bool result = p_storage._multimesh_enable_motion_vectors(m);
		MeshStorage::test_fail_motion_cache_resize = false;
		m->instances = saved_instances;
		return result;
	}
#endif
	static uint32_t upload_size(uint32_t p_regions, uint32_t p_region_bytes, uint32_t p_buffer_bytes) {
		return MeshStorage::_multimesh_dirty_upload_size(p_regions, p_region_bytes, p_buffer_bytes);
	}
};
} //namespace RendererRD

TEST_CASE("[B4][MultiMesh] rounded dirty uploads cannot wrap into a zero byte success") {
	using Access = RendererRD::TestMultimeshRebuildAccess;
	// A legal 134217727-instance 2D allocation is 4294967264 bytes.
	// Its 262144 rounded regions span exactly 2^32 bytes before clamping.
	CHECK(Access::upload_size(262144, 16384, 4294967264u) == 4294967264u);
	CHECK(Access::upload_size(2, 24576, 30000) == 30000);
	CHECK(Access::upload_size(1, 24576, 49152) == 24576);
	CHECK(Access::upload_size(0, 24576, 49152) == 0);
}

TEST_CASE("[B4][MultiMesh] allocation preflight bounds the complete buffer before mutation") {
	struct Case {
		int instances;
		RSE::MultimeshTransformFormat format;
		bool colors;
		bool custom;
		bool motion;
		Error error;
		uint32_t bytes;
	};
	const Case cases[] = {
		{ 0, RSE::MULTIMESH_TRANSFORM_2D, false, false, false, OK, 0 },
		{ 3, RSE::MULTIMESH_TRANSFORM_2D, false, false, false, OK, 96 },
		{ 3, RSE::MULTIMESH_TRANSFORM_2D, true, true, false, OK, 192 },
		{ 3, RSE::MULTIMESH_TRANSFORM_3D, true, true, false, OK, 240 },
		{ 3, RSE::MULTIMESH_TRANSFORM_3D, true, true, true, OK, 480 },
		{ -1, RSE::MULTIMESH_TRANSFORM_3D, false, false, false, ERR_INVALID_PARAMETER, 17 },
		{ 0, RSE::MultimeshTransformFormat(99), false, false, false, ERR_INVALID_PARAMETER, 17 },
		{ 134217727, RSE::MULTIMESH_TRANSFORM_2D, false, false, false, OK, 4294967264u },
		{ 134217728, RSE::MULTIMESH_TRANSFORM_2D, false, false, false, ERR_INVALID_PARAMETER, 17 },
		{ 53687091, RSE::MULTIMESH_TRANSFORM_3D, true, true, false, OK, 4294967280u },
		{ 53687092, RSE::MULTIMESH_TRANSFORM_3D, true, true, false, ERR_INVALID_PARAMETER, 17 },
		{ 26843545, RSE::MULTIMESH_TRANSFORM_3D, true, true, true, OK, 4294967200u },
		{ 26843546, RSE::MULTIMESH_TRANSFORM_3D, true, true, true, ERR_INVALID_PARAMETER, 17 },
	};
	for (const Case &test : cases) {
		uint32_t bytes = 17;
		CHECK(RendererMeshStorage::_multimesh_buffer_size(test.instances, test.format, test.colors, test.custom, test.motion, bytes) == test.error);
		CHECK(bytes == test.bytes);
	}
}

#ifdef VULKAN_ENABLED
namespace TestK02Motion {
bool gpu_tests_enabled() {
	const char *value = std::getenv("GODOT_TEST_B4_MATERIAL_GPU");
	return value && std::strcmp(value, "1") == 0;
}

#define K02_CHECK_OR_RETURN(m_condition) \
	do { \
		const bool k02_ok = bool(m_condition); \
		CHECK_MESSAGE(k02_ok, #m_condition); \
		if (!k02_ok) { \
			return; \
		} \
	} while (false)

struct Resources {
	RendererRD::MeshStorage &storage;
	RID mesh;
	RID multimesh;
	explicit Resources(RendererRD::MeshStorage &p_storage) :
			storage(p_storage) {}

	~Resources() {
		if (multimesh.is_valid()) {
			storage.multimesh_free(multimesh);
		}
		if (mesh.is_valid()) {
			storage.mesh_free(mesh);
		}
	}
};

void changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *p_tracker) {
	if (p_notification == Dependency::DEPENDENCY_CHANGED_MULTIMESH) {
		++*static_cast<int *>(p_tracker->userdata);
	}
}

TEST_CASE("[B4][MultiMeshGPU] motion preflight failures invalidate draw resources and notify" * doctest::skip(!gpu_tests_enabled())) {
	using Access = RendererRD::TestMultimeshRebuildAccess;
	K02_CHECK_OR_RETURN(gpu_tests_enabled());
	K02_CHECK_OR_RETURN(RenderingDevice::get_singleton() == nullptr);
	RenderingContextDriverVulkan context;
	K02_CHECK_OR_RETURN(context.initialize() == OK);
	RenderingDevice rd;
	K02_CHECK_OR_RETURN(rd.initialize(&context) == OK);
	RendererRD::TextureStorage textures;
	RendererRD::MaterialStorage materials;
	RendererRD::MeshStorage storage;

	// Each case starts with real small resources. Injected owner state is a
	// controlled negative; it is not a large GPU allocation or retirement proof.
	for (Access::Failure failure : { Access::SIZE, Access::MISSING_BUFFER, Access::CACHE_MISMATCH, Access::CACHE_RESIZE }) {
		CAPTURE(int(failure));
		Resources resources{ storage };
		resources.mesh = storage.mesh_allocate();
		K02_CHECK_OR_RETURN(resources.mesh.is_valid());
		storage.mesh_initialize(resources.mesh);
		RenderingServerTypes::SurfaceData surface;
		surface.format = RSE::ARRAY_FORMAT_VERTEX | RSE::ARRAY_FLAG_FORMAT_CURRENT_VERSION;
		surface.primitive = RSE::PRIMITIVE_TRIANGLES;
		surface.vertex_count = 3;
		K02_CHECK_OR_RETURN(surface.vertex_data.resize(3 * 3 * sizeof(float)) == OK);
		storage.mesh_add_surface(resources.mesh, surface);
		K02_CHECK_OR_RETURN(storage.mesh_get_surface_count(resources.mesh) == 1);
		resources.multimesh = storage.multimesh_allocate();
		K02_CHECK_OR_RETURN(resources.multimesh.is_valid());
		storage.multimesh_initialize(resources.multimesh);
		storage.multimesh_allocate_data(resources.multimesh, 1, RSE::MULTIMESH_TRANSFORM_2D, false, false, true);
		storage.multimesh_set_mesh(resources.multimesh, resources.mesh);
		K02_CHECK_OR_RETURN(storage.multimesh_get_buffer_rd_rid(resources.multimesh).is_valid());
		K02_CHECK_OR_RETURN(storage.multimesh_get_command_buffer_rd_rid(resources.multimesh).is_valid());
		K02_CHECK_OR_RETURN(Access::prepare(storage, resources.multimesh, failure));
		int notifications = 0;
		DependencyTracker tracker;
		tracker.userdata = &notifications;
		tracker.changed_callback = changed;
		Access::track(storage, resources.multimesh, tracker);
		ERR_PRINT_OFF;
		const bool enabled = Access::enable(storage, resources.multimesh, failure);
		ERR_PRINT_ON;
		CHECK_FALSE(enabled);
		CHECK_FALSE(storage.multimesh_get_buffer_rd_rid(resources.multimesh).is_valid());
		CHECK_FALSE(storage.multimesh_get_command_buffer_rd_rid(resources.multimesh).is_valid());
		CHECK(notifications == 1);
	}
}
} // namespace TestK02Motion
#else
TEST_CASE("[B4][MultiMeshGPU] motion preflight failures invalidate draw resources and notify" * doctest::skip()) {
	FAIL("Vulkan is not enabled in this build.");
}
#endif
