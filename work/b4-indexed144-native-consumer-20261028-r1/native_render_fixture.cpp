// Private native receiver only. Linked into the private test host, never registered as a production API.
#include "core/os/os.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/main/viewport.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/mesh.h"
#include "scene/resources/material.h"
#include "scene/resources/shader.h"
#include "scene/resources/3d/world_3d.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/renderer_rd/shader_rd.h"
#include <cstdio>
#include <cstring>

class ColdGroupProbe : public ShaderRD {
public:
 ColdGroupProbe() { setup(nullptr,nullptr,R"(#version 450
#VERSION_DEFINES
layout(local_size_x=1,local_size_y=1,local_size_z=1) in;
layout(set=0,binding=0,std430) buffer Output { uint value; } output_buffer;
void main(){
#CODE : WRITE
}
)","K05ColdGroupReload");
  Vector<VariantDefine> variants;variants.push_back(VariantDefine(0,"",true));variants.push_back(VariantDefine(1,"",false));initialize(variants);
 }
};
bool check_cold_group_reload(RenderingDevice *rd) {
 ColdGroupProbe shader;RID versions[2];
 HashMap<String,String> code;code["WRITE"]="output_buffer.value=1u;";
 for(auto &version:versions){version=shader.version_create(false);shader.version_set_compute_code(version,code,"","",Vector<String>());if(shader.version_get_shader(version,0).is_null())return false;}
 for(int i=0;i<2;i++){code["WRITE"]=String("output_buffer.value=")+itos(i+2)+"u;";shader.version_set_compute_code(versions[i],code,"","",Vector<String>());}
 shader.enable_group(1);
 bool good=true;
 for(int i=0;i<2;i++){
  RID native=shader.version_get_shader(versions[i],0);RID buffer=rd->storage_buffer_create(4);
  RenderingDevice::Uniform u;u.uniform_type=RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER;u.binding=0;u.append_id(buffer);Vector<RenderingDevice::Uniform> uniforms;uniforms.push_back(u);
  RID set=rd->uniform_set_create(uniforms,native,0);RID pipeline=rd->compute_pipeline_create(native);
  auto list=rd->compute_list_begin();rd->compute_list_bind_compute_pipeline(list,pipeline);rd->compute_list_bind_uniform_set(list,set,0);rd->compute_list_dispatch(list,1,1,1);rd->compute_list_end();
  Vector<uint8_t> bytes=rd->buffer_get_data(buffer);uint32_t observed=0;if(bytes.size()==4)std::memcpy(&observed,bytes.ptr(),4);
  std::printf("K05_COLD_GROUP_RELOAD material=%d expected=%d observed=%u\n",i,i+2,observed);good=good&&observed==uint32_t(i+2);
  rd->free_rid(pipeline);rd->free_rid(set);rd->free_rid(buffer);shader.version_free(versions[i]);
 }
 std::fflush(stdout);return good;
}
namespace {
Node *fixture = nullptr;
RID external_mesh, current_buffer, previous_buffer, index_buffer, index_array;
Ref<ArrayMesh> ordinary_mesh;
Ref<Shader> material_shader;
Ref<ShaderMaterial> material;
}
extern "C" __attribute__((visibility("default"))) int k05_private_fixture_create() {
 auto *tree=Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
 auto *storage=RendererRD::MeshStorage::get_singleton();
 auto *rd=RenderingDevice::get_singleton();
 if(!tree || !storage || !rd || fixture)return ERR_UNAVAILABLE;
 if(!check_cold_group_reload(rd))return ERR_INVALID_DATA;
 material_shader.instantiate();
 material_shader->set_code(R"(
shader_type spatial;
render_mode unshaded, cull_disabled, fog_disabled;
varying vec3 probe_normal;
varying vec3 probe_tangent;
varying vec3 probe_binormal;
varying vec4 probe_custom;
void vertex(){ probe_normal=NORMAL;probe_tangent=TANGENT;probe_binormal=BINORMAL;probe_custom=CUSTOM0; }
void fragment(){ ALBEDO=vec3(0.3+0.15*probe_normal.z+0.1*probe_tangent.x,0.5+0.2*probe_binormal.y,0.15+0.1*COLOR.r+0.1*UV.x+0.05*UV2.y+0.1*probe_custom.x); }
)");
 material.instantiate();material->set_shader(material_shader);
 const Vector3 positions[]={Vector3(-.75,-.65,0),Vector3(.75,-.65,0),Vector3(0,.75,0)};
 PackedVector3Array vertices,normals;PackedFloat32Array tangents;PackedColorArray colors;PackedVector2Array uv,uv2;PackedFloat32Array custom;PackedInt32Array indices;
 Vector<uint8_t> current;current.resize(144*3);std::memset(current.ptrw(),0,current.size());custom.resize(4*3);
 for(int i=0;i<3;i++){
  vertices.push_back(positions[i]);normals.push_back(Vector3(0,.6,.8));
  for(float v:{1.f,0.f,0.f,-1.f})tangents.push_back(v);
  colors.push_back(Color(.8,.4,.2,1));uv.push_back(Vector2(.2,.7));uv2.push_back(Vector2(.1,.9));indices.push_back(i);
  float *dst=reinterpret_cast<float*>(current.ptrw()+144*i);
  dst[0]=positions[i].x;dst[1]=positions[i].y;dst[2]=positions[i].z;dst[3]=1;
  dst[4]=0;dst[5]=.6;dst[6]=.8;dst[7]=0;
  dst[8]=1;dst[9]=0;dst[10]=0;dst[11]=-1;
  dst[12]=.8;dst[13]=.4;dst[14]=.2;dst[15]=1;
  dst[16]=.2;dst[17]=.7;dst[18]=.1;dst[19]=.9;
  dst[20]=.25;dst[21]=.5;dst[22]=.75;dst[23]=1;
  std::memcpy(custom.ptrw()+4*i,dst+20,16);
 }
 Array arrays;arrays.resize(Mesh::ARRAY_MAX);arrays[Mesh::ARRAY_VERTEX]=vertices;arrays[Mesh::ARRAY_NORMAL]=normals;arrays[Mesh::ARRAY_TANGENT]=tangents;arrays[Mesh::ARRAY_COLOR]=colors;arrays[Mesh::ARRAY_TEX_UV]=uv;arrays[Mesh::ARRAY_TEX_UV2]=uv2;arrays[Mesh::ARRAY_CUSTOM0]=custom;arrays[Mesh::ARRAY_INDEX]=indices;
 ordinary_mesh.instantiate();ordinary_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES,arrays,Array(),Dictionary(),uint64_t(Mesh::ARRAY_CUSTOM_RGBA_FLOAT)<<Mesh::ARRAY_FORMAT_CUSTOM0_SHIFT);if(ordinary_mesh->get_surface_count()!=1)return ERR_INVALID_DATA;ordinary_mesh->surface_set_material(0,material);
 current_buffer=rd->vertex_buffer_create(current.size(),current,RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT);
 previous_buffer=rd->vertex_buffer_create(current.size(),current,RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT);
 const uint32_t order[]={0,1,2};Vector<uint8_t> index_bytes;index_bytes.resize(sizeof(order));std::memcpy(index_bytes.ptrw(),order,sizeof(order));
 index_buffer=rd->index_buffer_create(3,RenderingDevice::INDEX_BUFFER_FORMAT_UINT32,index_bytes,false,RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT);index_array=rd->index_array_create(index_buffer,0,3);
 external_mesh=storage->mesh_allocate();storage->mesh_initialize(external_mesh);
 RendererRD::MeshStorage::ExternalSurfaceDescriptor descriptor;
 descriptor.vertex_count=3;descriptor.index_count=3;descriptor.current_vertex_buffer=current_buffer;descriptor.previous_vertex_buffer=previous_buffer;descriptor.vertex_buffer_size=current.size();descriptor.previous_vertex_buffer_size=current.size();descriptor.index_buffer=index_buffer;descriptor.index_array=index_array;descriptor.index_buffer_size=index_bytes.size();descriptor.material=material->get_rid();descriptor.aabb=AABB(Vector3(-.75,-.65,-.01),Vector3(1.5,1.4,.02));descriptor.generation=1;descriptor.current_allocation_generation=1;descriptor.previous_allocation_generation=2;
 uint32_t surface=UINT32_MAX;uint64_t generation=0;
 Error result=storage->mesh_add_external_surface(external_mesh,descriptor,surface,generation);
 if(result!=OK)return result;
 std::printf("K05_NATIVE_SURFACE admitted=1 surface=%u generation=%llu aabb_empty=%u\n",surface,(unsigned long long)generation,unsigned(storage->mesh_get_aabb(external_mesh).size==Vector3()));
 const float mutated_vertex=20.f;Vector<uint8_t> attempted_vertex;attempted_vertex.resize(4);std::memcpy(attempted_vertex.ptrw(),&mutated_vertex,4);
 storage->mesh_surface_update_vertex_region(external_mesh,0,0,attempted_vertex);
 const uint32_t mutated_index=2;Vector<uint8_t> attempted_index;attempted_index.resize(4);std::memcpy(attempted_index.ptrw(),&mutated_index,4);
 storage->mesh_surface_update_index_region(external_mesh,0,0,attempted_index);
 const bool vertex_immutable=rd->buffer_get_data(current_buffer)==current;
 const bool index_immutable=rd->buffer_get_data(index_buffer)==index_bytes;
 std::printf("K05_BORROWED_REGION_WRITE vertex_unchanged=%u index_unchanged=%u\n",unsigned(vertex_immutable),unsigned(index_immutable));std::fflush(stdout);
 if(!vertex_immutable||!index_immutable)return ERR_INVALID_DATA;
 const bool bounds_ok=storage->mesh_get_aabb(external_mesh)==descriptor.aabb;
 const uint64_t required_format=RenderingServerEnums::ARRAY_FORMAT_VERTEX|RenderingServerEnums::ARRAY_FORMAT_NORMAL|RenderingServerEnums::ARRAY_FORMAT_TANGENT|RenderingServerEnums::ARRAY_FORMAT_COLOR|RenderingServerEnums::ARRAY_FORMAT_TEX_UV|RenderingServerEnums::ARRAY_FORMAT_TEX_UV2|RenderingServerEnums::ARRAY_FORMAT_CUSTOM0|RenderingServerEnums::ARRAY_FORMAT_CUSTOM1|RenderingServerEnums::ARRAY_FORMAT_CUSTOM2|RenderingServerEnums::ARRAY_FORMAT_CUSTOM3|RenderingServerEnums::ARRAY_FORMAT_INDEX;
 const bool format_ok=(storage->mesh_surface_get_format(storage->mesh_get_surface(external_mesh,uint32_t(0)))&required_format)==required_format;
 RID deformation=storage->mesh_instance_create(external_mesh);
 const bool deformation_rejected=deformation.is_null();
 if(deformation.is_valid())storage->mesh_instance_free(deformation);
 std::printf("K05_NATIVE_CONTRACT bounds=%u attributes=%u deformation_rejected=%u\n",unsigned(bounds_ok),unsigned(format_ok),unsigned(deformation_rejected));std::fflush(stdout);
 if(!bounds_ok||!format_ok||!deformation_rejected)return ERR_INVALID_DATA;
 fixture=memnew(Node);fixture->set_name("K05NativeFixture");tree->get_root()->add_child(fixture);
 for(int i=0;i<2;i++){
  auto *viewport=memnew(SubViewport);viewport->set_name(i?"External":"Ordinary");viewport->set_size(Size2i(96,96));Ref<World3D> world;world.instantiate();viewport->set_world_3d(world);viewport->set_transparent_background(true);viewport->set_update_mode(SubViewport::UPDATE_ALWAYS);fixture->add_child(viewport);
  auto *camera=memnew(Camera3D);viewport->add_child(camera);camera->set_position(Vector3(0,0,3));camera->set_orthogonal(2.2,.1,10);camera->set_current(true);
  auto *instance=memnew(MeshInstance3D);viewport->add_child(instance);if(i)instance->set_base(external_mesh);else instance->set_mesh(ordinary_mesh);
 }
 std::puts("K05_NATIVE_FIXTURE_READY original_surface_and_renderers=1");std::fflush(stdout);return OK;
}
extern "C" __attribute__((visibility("default"))) void k05_private_fixture_cleanup() {
 auto *storage=RendererRD::MeshStorage::get_singleton();auto *rd=RenderingDevice::get_singleton();
 if(fixture){if(fixture->get_parent())fixture->get_parent()->remove_child(fixture);memdelete(fixture);fixture=nullptr;}
 if(external_mesh.is_valid()){storage->mesh_free(external_mesh);external_mesh=RID();}
 for(RID *rid:{&index_array,&index_buffer,&previous_buffer,&current_buffer}){if(rid->is_valid())rd->free_rid(*rid);*rid=RID();}
 ordinary_mesh.unref();material.unref();material_shader.unref();std::puts("K05_NATIVE_FIXTURE_CLEANUP_REQUESTED");std::fflush(stdout);
}
