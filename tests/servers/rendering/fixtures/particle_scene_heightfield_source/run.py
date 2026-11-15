#!/usr/bin/env python3
"""Exercise the RD heightfield source accessor against real source instances."""

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[5]
SOURCE = ROOT / "servers/rendering/renderer_rd/storage_rd/particles_storage.cpp"


def body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for pos in range(brace, len(source)):
        depth += (source[pos] == "{") - (source[pos] == "}")
        if depth == 0:
            return source[brace : pos + 1]
    raise ValueError(signature)


def main() -> int:
    source = SOURCE.read_text()
    method = body(source, "bool ParticlesStorage::particles_collision_instance_get_scene_heightfield(")
    extent_setter = body(source, "void ParticlesStorage::particles_collision_set_box_extents(")
    resolution_setter = body(source, "void ParticlesStorage::particles_collision_set_height_field_resolution(")
    if "particles_collision->scene_shape_revision++;" not in extent_setter or "particles_collision->scene_shape_revision++;" not in resolution_setter:
        raise AssertionError("heightfield geometry setters must advance source revision")
    harness = r'''
#include <cstdint>
#include <cstdio>
struct RID { int id=0; };
struct Transform3D { int origin=0; };
struct Vector3 { float x=0,y=0,z=0; };
namespace RSE { enum ParticlesCollisionType { PARTICLES_COLLISION_TYPE_SPHERE_COLLIDE=3, PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE=6 }; enum ParticlesCollisionHeightfieldResolution { PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_1024=2 }; }
struct ParticlesCollision { RSE::ParticlesCollisionType type=RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE; Vector3 extents{2,3,4}; RSE::ParticlesCollisionHeightfieldResolution heightfield_resolution=RSE::PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_1024; uint64_t scene_shape_revision=9; };
struct ParticlesCollisionInstance { RID collision; Transform3D transform; uint64_t scene_source_key=0; bool active=true; };
template<class T> struct Owner { T *items[4]{}; T *get_or_null(RID rid) const { return rid.id>0 && rid.id<4 ? items[rid.id]:nullptr; } };
struct ParticlesStorage {
    Owner<ParticlesCollision> particles_collision_owner;
    Owner<ParticlesCollisionInstance> particles_collision_instance_owner;
    bool particles_collision_instance_get_scene_heightfield(RID,uint64_t&,uint64_t&,Transform3D&,Vector3&,RSE::ParticlesCollisionHeightfieldResolution&) const;
};
bool ParticlesStorage::particles_collision_instance_get_scene_heightfield(RID p_instance,uint64_t &r_source_key,uint64_t &r_shape_revision,Transform3D &r_transform,Vector3 &r_extents,RSE::ParticlesCollisionHeightfieldResolution &r_resolution) const ''' + method + r'''
int main() {
    ParticlesStorage storage;
    ParticlesCollision shape;
    ParticlesCollisionInstance left{{1},{11},7,true},right{{1},{22},8,true};
    storage.particles_collision_owner.items[1]=&shape;
    storage.particles_collision_instance_owner.items[1]=&left;
    storage.particles_collision_instance_owner.items[2]=&right;
    uint64_t key=0,rev=0; Transform3D transform; Vector3 extents; RSE::ParticlesCollisionHeightfieldResolution resolution=RSE::PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_1024;
    bool good=true;
    auto check=[&](bool ok,const char *label){ if(!ok) std::fprintf(stderr,"FAIL %s\n",label); good &= ok; };
    check(storage.particles_collision_instance_get_scene_heightfield({1},key,rev,transform,extents,resolution) && key==7 && rev==9 && transform.origin==11 && extents.x==2 && extents.z==4 && resolution==2,"first instance");
    check(storage.particles_collision_instance_get_scene_heightfield({2},key,rev,transform,extents,resolution) && key==8 && transform.origin==22,"shared base keeps second identity and pose");
    shape.type=RSE::PARTICLES_COLLISION_TYPE_SPHERE_COLLIDE;
    check(!storage.particles_collision_instance_get_scene_heightfield({1},key,rev,transform,extents,resolution),"other type excluded");
    shape.type=RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE; right.active=false;
    check(!storage.particles_collision_instance_get_scene_heightfield({2},key,rev,transform,extents,resolution),"inactive excluded");
    check(!storage.particles_collision_instance_get_scene_heightfield({3},key,rev,transform,extents,resolution),"unknown RID excluded");
    return good?0:1;
}
'''
    with tempfile.TemporaryDirectory(prefix="particle-scene-heightfield-source-") as temp:
        cpp = Path(temp) / "test.cpp"
        exe = Path(temp) / "test"
        cpp.write_text(harness)
        subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", str(cpp), "-o", str(exe)], check=True)
        return subprocess.run([str(exe)], timeout=10).returncode


if __name__ == "__main__":
    raise SystemExit(main())
