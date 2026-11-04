#!/usr/bin/env python3
"""Execute the RD scene-sphere source accessor with two live source instances."""

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
    method = body(source, "bool ParticlesStorage::particles_collision_instance_get_scene_sphere(")
    harness = r'''
#include <cstdint>
#include <cstdio>
struct RID { int id=0; };
struct Transform3D { int origin=0; };
namespace RSE { enum { PARTICLES_COLLISION_TYPE_SPHERE_COLLIDE=2 }; }
struct ParticlesCollision { int type=2; float radius=1; uint64_t scene_shape_revision=1; };
struct ParticlesCollisionInstance { RID collision; Transform3D transform; uint64_t scene_source_key=0; bool active=true; };
template<class T> struct Owner {
    T *items[4]{};
    T *get_or_null(RID rid) const { return rid.id>0 && rid.id<4 ? items[rid.id] : nullptr; }
};
struct ParticlesStorage {
    Owner<ParticlesCollision> particles_collision_owner;
    Owner<ParticlesCollisionInstance> particles_collision_instance_owner;
    bool particles_collision_instance_get_scene_sphere(RID, uint64_t &, uint64_t &, Transform3D &, float &) const;
};
bool ParticlesStorage::particles_collision_instance_get_scene_sphere(RID p_instance, uint64_t &r_source_key, uint64_t &r_shape_revision, Transform3D &r_transform, float &r_radius) const ''' + method + r'''
int main() {
    ParticlesStorage storage;
    ParticlesCollision shape{2,3,9};
    ParticlesCollisionInstance left{{1},{11},7,true};
    ParticlesCollisionInstance right{{1},{22},8,true};
    storage.particles_collision_owner.items[1]=&shape;
    storage.particles_collision_instance_owner.items[1]=&left;
    storage.particles_collision_instance_owner.items[2]=&right;
    uint64_t key=0,rev=0; Transform3D transform; float radius=0;
    auto check=[&](bool ok,const char *name) { if (!ok) std::fprintf(stderr,"FAIL %s\n",name); return ok; };
    bool good=true;
    good &= check(storage.particles_collision_instance_get_scene_sphere({1},key,rev,transform,radius) && key==7 && rev==9 && transform.origin==11 && radius==3,"left instance");
    good &= check(storage.particles_collision_instance_get_scene_sphere({2},key,rev,transform,radius) && key==8 && rev==9 && transform.origin==22 && radius==3,"right instance shares shape but not identity");
    shape.type=0;
    good &= check(!storage.particles_collision_instance_get_scene_sphere({1},key,rev,transform,radius),"non-sphere excluded");
    shape.type=2; right.active=false;
    good &= check(!storage.particles_collision_instance_get_scene_sphere({2},key,rev,transform,radius),"inactive excluded");
    good &= check(!storage.particles_collision_instance_get_scene_sphere({3},key,rev,transform,radius),"unknown RID excluded");
    return good?0:1;
}
'''
    with tempfile.TemporaryDirectory(prefix="particle-scene-sphere-source-") as temp:
        cpp = Path(temp) / "test.cpp"
        exe = Path(temp) / "test"
        cpp.write_text(harness)
        subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", str(cpp), "-o", str(exe)], check=True)
        return subprocess.run([str(exe)], timeout=10).returncode


if __name__ == "__main__":
    raise SystemExit(main())
