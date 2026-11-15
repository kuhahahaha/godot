#!/usr/bin/env python3
"""Run the production extent setter with observable cache ownership edges."""
import argparse
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[5]
SOURCE = 'servers/rendering/renderer_rd/storage_rd/particles_storage.cpp'

def body(source, signature):
    begin = source.index('{', source.index(signature))
    depth = 0
    for i in range(begin, len(source)):
        if source[i] == '{': depth += 1
        elif source[i] == '}':
            depth -= 1
            if depth == 0: return source[begin:i+1]
    raise ValueError(signature)

def definition(source, signature):
    start = source.index(signature)
    return source[start:source.index('{', start)] + body(source, signature)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source-ref')
    args = parser.parse_args()
    source = subprocess.check_output(['git','show',f'{args.source_ref}:{SOURCE}'],cwd=ROOT,text=True) if args.source_ref else (ROOT / SOURCE).read_text()
    methods = '\n'.join(definition(source, signature) for signature in (
        'void ParticlesStorage::_heightfield_free_instance_target(',
        'void ParticlesStorage::_heightfield_invalidate_base_instances(',
        'void ParticlesStorage::particles_collision_set_box_extents(',
    ))
    prefix = r'''
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>
#define ERR_FAIL_NULL(p) if (!(p)) return
#define ERR_FAIL_COND_MSG(p,m) if (p) return
struct RID { int id=0; bool is_valid() const { return id!=0; } };
struct Vector3 { float x,y,z; bool is_finite() const { return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z); } };
namespace RSE { enum { PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE=1 }; }
struct Dependency { enum { DEPENDENCY_CHANGED_AABB }; int changes=0; void changed_notify(int) { changes++; } };
struct ParticlesCollision { int type=1; Vector3 extents{1,1,1}; uint64_t scene_shape_revision=1; Dependency dependency; std::vector<RID> instances{{1},{2}}; };
struct ParticlesCollisionInstance { RID heightfield_fb, heightfield_texture; };
struct CollisionOwner { ParticlesCollision value; ParticlesCollision *get_or_null(RID rid) { return rid.id==1?&value:nullptr; } };
struct InstanceOwner { ParticlesCollisionInstance values[2]; ParticlesCollisionInstance *get_or_null(RID rid) { return rid.id>=1&&rid.id<=2?&values[rid.id-1]:nullptr; } };
struct RD { std::vector<int> freed; static RD *get_singleton() { static RD rd; return &rd; } void free_rid(RID rid) { freed.push_back(rid.id); } };
struct ParticlesStorage {
    CollisionOwner particles_collision_owner;
    InstanceOwner particles_collision_instance_owner;
    void _heightfield_free_instance_target(ParticlesCollisionInstance *);
    void _heightfield_invalidate_base_instances(RID);
    void particles_collision_set_box_extents(RID, const Vector3 &);
};
''' + methods
    suffix = r'''
int main() {
    int checks=0, failures=0;
    auto check=[&](bool ok,const char *label) { checks++; if(!ok) { failures++; std::printf("FAIL %s\n",label); } };
    ParticlesStorage storage;
    auto reset=[&]() { storage.particles_collision_owner.value=ParticlesCollision(); storage.particles_collision_instance_owner.values[0]={{7},{8}}; storage.particles_collision_instance_owner.values[1]={{9},{10}}; RD::get_singleton()->freed.clear(); };
    for (Vector3 next : {Vector3{2,1,1}, Vector3{1,1,2}}) {
        reset(); storage.particles_collision_set_box_extents(RID{1}, next);
        auto &p=storage.particles_collision_owner.value;
        const auto &instances=storage.particles_collision_instance_owner.values;
        check(!instances[0].heightfield_fb.is_valid()&&!instances[0].heightfield_texture.is_valid()&&!instances[1].heightfield_fb.is_valid()&&!instances[1].heightfield_texture.is_valid(),"aspect mutation drops both instance targets");
        check(RD::get_singleton()->freed==std::vector<int>({7,8,9,10}),"instance targets retired once in dependency order");
        check(p.extents.x==next.x&&p.extents.z==next.z&&p.dependency.changes==1,"new extent and AABB notification");
    }
    reset(); storage.particles_collision_set_box_extents(RID{1},Vector3{1,2,1});
    check(RD::get_singleton()->freed.empty()&&storage.particles_collision_instance_owner.values[0].heightfield_fb.is_valid()&&storage.particles_collision_instance_owner.values[1].heightfield_fb.is_valid(),"Y-only retains both targets");
    reset(); storage.particles_collision_set_box_extents(RID{1},Vector3{1,1,1});
    check(RD::get_singleton()->freed.empty()&&storage.particles_collision_instance_owner.values[0].heightfield_fb.is_valid()&&storage.particles_collision_instance_owner.values[1].heightfield_fb.is_valid(),"equal extents retain both targets");
    reset(); storage.particles_collision_owner.value.type=0; storage.particles_collision_set_box_extents(RID{1},Vector3{2,1,1});
    check(RD::get_singleton()->freed.empty(),"other collision type keeps cache");
    reset(); storage.particles_collision_set_box_extents(RID{1},Vector3{-1,1,1});
    check(RD::get_singleton()->freed.empty()&&storage.particles_collision_owner.value.extents.x==1,"invalid extent preserves state");
    reset(); storage.particles_collision_instance_owner.values[0]={}; storage.particles_collision_instance_owner.values[1]={};
    storage.particles_collision_set_box_extents(RID{1},Vector3{2,1,1});
    check(RD::get_singleton()->freed.empty(),"absent cache does not free invalid RIDs");
    std::printf("RD heightfield instance targets: %d/%d checks passed\n", checks-failures,checks);
    return failures?1:0;
}
'''
    with tempfile.TemporaryDirectory() as tmp:
        cpp=Path(tmp)/'test.cpp'; exe=Path(tmp)/'test'; cpp.write_text(prefix+suffix)
        subprocess.run(['c++','-std=c++17',str(cpp),'-o',str(exe)],check=True)
        return subprocess.run([str(exe)]).returncode
if __name__=='__main__': raise SystemExit(main())
