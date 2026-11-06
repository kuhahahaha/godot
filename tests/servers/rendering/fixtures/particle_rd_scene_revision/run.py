#!/usr/bin/env python3
"""Exercise real collision setters at the scene shape revision boundary."""

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[5]
SOURCE = ROOT / "servers/rendering/renderer_rd/storage_rd/particles_storage.cpp"
METHODS = (
    "particles_collision_set_collision_type",
    "particles_collision_set_sphere_radius",
    "particles_collision_set_box_extents",
    "particles_collision_set_height_field_resolution",
)


def body(source: str, signature: str) -> str:
    start = source.index("{", source.index("void ParticlesStorage::" + signature + "("))
    depth = 0
    for pos in range(start, len(source)):
        depth += (source[pos] == "{") - (source[pos] == "}")
        if depth == 0:
            return source[start : pos + 1]
    raise ValueError(signature)


def main() -> int:
    source = SOURCE.read_text()
    definitions = "\n".join(
        "void ParticlesStorage::" + name + signature + " " + body(source, name)
        for name, signature in (
            (METHODS[0], "(RID p_particles_collision, RSE::ParticlesCollisionType p_type)"),
            (METHODS[1], "(RID p_particles_collision, real_t p_radius)"),
            (METHODS[2], "(RID p_particles_collision, const Vector3 &p_extents)"),
            (METHODS[3], "(RID p_particles_collision, RSE::ParticlesCollisionHeightfieldResolution p_resolution)"),
        )
    )
    prefix = r'''
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>
#define ERR_FAIL_NULL(p) if (!(p)) return
#define ERR_FAIL_INDEX(i, n) if ((i) < 0 || (i) >= (n)) return
#define ERR_FAIL_COND_MSG(p, m) if (p) return
using real_t = float;
struct RID { int id=0; bool is_valid() const { return id!=0; } };
struct Vector3 { float x,y,z; bool is_finite() const { return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z); } };
namespace Math { bool is_finite(float x) { return std::isfinite(x); } }
namespace RSE {
enum ParticlesCollisionType { PARTICLES_COLLISION_TYPE_SPHERE_COLLIDE=0, PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE=1 };
enum ParticlesCollisionHeightfieldResolution { PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_256=0, PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_512=1, PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_MAX=2 };
}
struct Dependency { enum { DEPENDENCY_CHANGED_AABB }; int changes=0; void changed_notify(int) { changes++; } };
struct ParticlesCollision {
    RSE::ParticlesCollisionType type=RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE;
    float radius=1;
    Vector3 extents{1,1,1};
    RSE::ParticlesCollisionHeightfieldResolution heightfield_resolution=RSE::PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_256;
    uint64_t scene_shape_revision=1;
    Dependency dependency;
};
struct Owner { ParticlesCollision value; ParticlesCollision *get_or_null(RID rid) { return rid.id==1?&value:nullptr; } };
struct RD { std::vector<int> freed; static RD *get_singleton() { static RD rd; return &rd; } void free_rid(RID rid) { freed.push_back(rid.id); } };
struct ParticlesStorage {
    Owner particles_collision_owner;
    void _heightfield_invalidate_base_instances(RID) { RD::get_singleton()->free_rid({7}); RD::get_singleton()->free_rid({8}); }
    void particles_collision_set_collision_type(RID, RSE::ParticlesCollisionType);
    void particles_collision_set_sphere_radius(RID, real_t);
    void particles_collision_set_box_extents(RID, const Vector3 &);
    void particles_collision_set_height_field_resolution(RID, RSE::ParticlesCollisionHeightfieldResolution);
};
'''
    suffix = r'''
int main() {
    ParticlesStorage storage;
    auto *shape=&storage.particles_collision_owner.value;
    auto check=[](bool ok,const char *name) { if (!ok) std::fprintf(stderr,"FAIL %s\n",name); return ok; };
    bool good=true;
    auto reset=[&]() { *shape=ParticlesCollision(); shape->scene_shape_revision=UINT64_MAX; RD::get_singleton()->freed.clear(); };
    reset(); storage.particles_collision_set_collision_type({1},RSE::PARTICLES_COLLISION_TYPE_SPHERE_COLLIDE);
    good &= check(shape->scene_shape_revision==UINT64_MAX && shape->type==RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE && RD::get_singleton()->freed.empty() && shape->dependency.changes==0,"type refuses before mutation and free");
    reset(); storage.particles_collision_set_sphere_radius({1},2);
    good &= check(shape->scene_shape_revision==UINT64_MAX && shape->radius==1 && shape->dependency.changes==0,"sphere refuses before mutation");
    reset(); storage.particles_collision_set_box_extents({1},{2,1,1});
    good &= check(shape->scene_shape_revision==UINT64_MAX && shape->extents.x==1 && RD::get_singleton()->freed.empty() && shape->dependency.changes==0,"extents refuse before mutation and free");
    reset(); storage.particles_collision_set_height_field_resolution({1},RSE::PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_512);
    good &= check(shape->scene_shape_revision==UINT64_MAX && shape->heightfield_resolution==RSE::PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_256 && RD::get_singleton()->freed.empty(),"resolution refuses before mutation and free");
    reset(); storage.particles_collision_set_box_extents({1},{1,1,1});
    good &= check(shape->scene_shape_revision==UINT64_MAX && RD::get_singleton()->freed.empty(),"unchanged extents remain legal");
    shape->scene_shape_revision=UINT64_MAX-1;
    storage.particles_collision_set_box_extents({1},{2,1,1});
    good &= check(shape->scene_shape_revision==UINT64_MAX && shape->extents.x==2,"last available revision is legal");
    return good?0:1;
}
'''
    with tempfile.TemporaryDirectory(prefix="particle-scene-revision-") as temp:
        cpp = Path(temp) / "test.cpp"
        exe = Path(temp) / "test"
        cpp.write_text(prefix + definitions + suffix)
        subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", str(cpp), "-o", str(exe)], check=True)
        return subprocess.run([str(exe)], timeout=10).returncode


if __name__ == "__main__":
    raise SystemExit(main())
