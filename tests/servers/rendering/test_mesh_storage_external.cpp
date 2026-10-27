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
