#pragma once
// Test-only original Mesh fixture shared by the existing Mesh and Material TUs.
// No production roster fields, selection leases or source holds are fabricated.
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
namespace RendererRD {
struct NativeMaterialMeshFixture {
 MeshStorage *storage = nullptr, *previous = nullptr;
 RID mesh, vertex_buffer, index_buffer, index_array, vertex_array;
 uint32_t versions = 0;
};
NativeMaterialMeshFixture create_native_material_mesh_fixture(RenderingDevice &, RID, bool);
void check_native_material_mesh_fixture(const NativeMaterialMeshFixture &, bool);
void destroy_native_material_mesh_fixture(NativeMaterialMeshFixture &);
}
