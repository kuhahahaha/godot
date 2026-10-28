/**************************************************************************/
/*  test_mesh_storage_external.cpp                                       */
/**************************************************************************/
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
#include "tests/test_macros.h"

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
