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

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source-ref')
    args = parser.parse_args()
    source = subprocess.check_output(['git','show',f'{args.source_ref}:{SOURCE}'],cwd=ROOT,text=True) if args.source_ref else (ROOT / SOURCE).read_text()
    setter = body(source, 'void ParticlesStorage::particles_collision_set_box_extents(')
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
struct ParticlesCollision { int type=1; Vector3 extents{1,1,1}; RID heightfield_fb{7}, heightfield_texture{8}; uint64_t scene_shape_revision=1; Dependency dependency; };
struct Owner { ParticlesCollision value; ParticlesCollision *get_or_null(RID rid) { return rid.id==1?&value:nullptr; } } particles_collision_owner;
struct RD { std::vector<int> freed; static RD *get_singleton() { static RD rd; return &rd; } void free_rid(RID rid) { freed.push_back(rid.id); } };
void set_extents(RID p_particles_collision, const Vector3 &p_extents)
'''
    suffix = r'''
int main() {
    int checks=0, failures=0;
    auto check=[&](bool ok,const char *label) { checks++; if(!ok) { failures++; std::printf("FAIL %s\n",label); } };
    auto reset=[]() { particles_collision_owner.value=ParticlesCollision(); RD::get_singleton()->freed.clear(); };
    for (Vector3 next : {Vector3{2,1,1}, Vector3{1,1,2}}) {
        reset(); set_extents(RID{1}, next);
        auto &p=particles_collision_owner.value;
        check(!p.heightfield_fb.is_valid()&&!p.heightfield_texture.is_valid(),"aspect mutation drops both cached RIDs");
        check(RD::get_singleton()->freed==std::vector<int>({7,8}),"cache retired once in dependency order");
        check(p.extents.x==next.x&&p.extents.z==next.z&&p.dependency.changes==1,"new extent and AABB notification");
    }
    reset(); set_extents(RID{1},Vector3{1,2,1});
    check(RD::get_singleton()->freed.empty(),"Y-only keeps cache");
    reset(); set_extents(RID{1},Vector3{1,1,1});
    check(RD::get_singleton()->freed.empty(),"equal extents keep cache");
    reset(); particles_collision_owner.value.type=0; set_extents(RID{1},Vector3{2,1,1});
    check(RD::get_singleton()->freed.empty(),"other collision type keeps cache");
    reset(); set_extents(RID{1},Vector3{-1,1,1});
    check(RD::get_singleton()->freed.empty()&&particles_collision_owner.value.extents.x==1,"invalid extent preserves state");
    reset(); particles_collision_owner.value.heightfield_fb=RID(); particles_collision_owner.value.heightfield_texture=RID();
    set_extents(RID{1},Vector3{2,1,1});
    check(RD::get_singleton()->freed.empty(),"absent cache does not free invalid RIDs");
    std::printf("RD heightfield extent cache: %d/%d checks passed\n", checks-failures,checks);
    return failures?1:0;
}
'''
    with tempfile.TemporaryDirectory() as tmp:
        cpp=Path(tmp)/'test.cpp'; exe=Path(tmp)/'test'; cpp.write_text(prefix+setter+suffix)
        subprocess.run(['c++','-std=c++17',str(cpp),'-o',str(exe)],check=True)
        return subprocess.run([str(exe)]).returncode
if __name__=='__main__': raise SystemExit(main())
