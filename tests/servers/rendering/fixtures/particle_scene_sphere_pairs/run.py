#!/usr/bin/env python3
"""Exercise the production SceneCull pair snapshot against two receivers."""

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[5]
SOURCE = ROOT / "servers/rendering/renderer_scene_cull.cpp"


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
    method = body(SOURCE.read_text(), "bool RendererSceneCull::scene_sphere_snapshot_pairs(")
    harness = r'''
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>
struct RID { int id=0; };
struct Transform3D { int origin=0; };
namespace RSE { enum { INSTANCE_PARTICLES=1, INSTANCE_PARTICLES_COLLISION=2 }; }
struct SceneSphereSource { uint64_t source_key,shape_revision; Transform3D transform; float radius; };
template<class T> struct Vector : std::vector<T> {
    template<class C> void sort_custom() { std::sort(this->begin(),this->end(),C{}); }
};
struct Instance;
struct InstancePair;
template<class T> struct SelfList { T *pair=nullptr; SelfList *next_link=nullptr; T *self() const { return pair; } SelfList *next() const { return next_link; } };
struct PairList { SelfList<InstancePair> *head=nullptr; SelfList<InstancePair> *first() const { return head; } };
struct InstanceParticlesCollisionData { RID instance; uint32_t cull_mask=1; };
struct Instance {
    int base_type=0; RID base; bool visible=true; void *scenario=nullptr; uint32_t layer_mask=1;
    PairList pairs; InstanceParticlesCollisionData collision_data; void *base_data=&collision_data;
};
struct InstancePair { Instance *a=nullptr,*b=nullptr; SelfList<InstancePair> link; InstancePair(Instance *x,Instance *y):a(x),b(y) { link.pair=this; } };
template<class T> struct Owner { T *items[8]{}; T *get_or_null(RID rid) const { return rid.id>0 && rid.id<8 ? items[rid.id]:nullptr; } };
namespace RendererRD { struct ParticlesStorage {
    static ParticlesStorage *single;
    static ParticlesStorage *get_singleton() { return single; }
    bool particles_collision_instance_get_scene_sphere(RID id,uint64_t &key,uint64_t &rev,Transform3D &transform,float &radius) const {
        if (id.id!=1 && id.id!=2) return false;
        key=id.id==1?9:3; rev=7; transform.origin=id.id==1?11:22; radius=2; return true;
    }
}; ParticlesStorage *ParticlesStorage::single=nullptr; }
struct RendererSceneCull {
    Owner<Instance> instance_owner;
    bool scene_sphere_snapshot_pairs(RID,uint32_t,Vector<SceneSphereSource>&) const;
};
bool RendererSceneCull::scene_sphere_snapshot_pairs(RID p_receiver,uint32_t p_max_sources,Vector<SceneSphereSource> &r_sources) const ''' + method + r'''
int main() {
    RendererRD::ParticlesStorage storage; RendererRD::ParticlesStorage::single=&storage;
    RendererSceneCull scene;
    int scenario=1;
    Instance receiver; receiver.base_type=1; receiver.base={1}; receiver.scenario=&scenario;
    Instance other_receiver; other_receiver.base_type=1; other_receiver.base={1}; other_receiver.scenario=&scenario;
    Instance left; left.base_type=2; left.base={2}; left.scenario=&scenario; left.collision_data.instance={1};
    Instance right; right.base_type=2; right.base={3}; right.scenario=&scenario; right.collision_data.instance={2};
    Instance foreign; foreign.base_type=2; foreign.base={4}; foreign.collision_data.instance={2};
    InstancePair a{&receiver,&left},b{&receiver,&right},c{&receiver,&foreign},d{&other_receiver,&right};
    a.link.next_link=&b.link; b.link.next_link=&c.link;
    receiver.pairs.head=&a.link; other_receiver.pairs.head=&d.link;
    scene.instance_owner.items[1]=&receiver;
    scene.instance_owner.items[2]=&other_receiver;
    Vector<SceneSphereSource> out;
    bool ok=true;
    auto check=[&](bool v,const char *label){ if(!v) std::fprintf(stderr,"FAIL %s\n",label); ok &= v; };
    check(scene.scene_sphere_snapshot_pairs({1},2,out) && out.size()==2 && out[0].source_key==3 && out[1].source_key==9,"receiver gets only own sorted pair sources");
    check(scene.scene_sphere_snapshot_pairs({2},2,out) && out.size()==1 && out[0].source_key==3,"same base second receiver keeps its own pairs");
    check(!scene.scene_sphere_snapshot_pairs({1},1,out),"source limit rejects whole snapshot");
    receiver.visible=false;
    check(scene.scene_sphere_snapshot_pairs({1},2,out) && out.empty(),"hidden receiver is empty");
    receiver.visible=true; right.collision_data.cull_mask=0;
    check(scene.scene_sphere_snapshot_pairs({1},2,out) && out.size()==1 && out[0].source_key==9,"mask excludes source");
    check(!scene.scene_sphere_snapshot_pairs({7},2,out),"invalid receiver rejects");
    return ok?0:1;
}
'''
    with tempfile.TemporaryDirectory(prefix="particle-scene-sphere-pairs-") as temp:
        cpp = Path(temp) / "test.cpp"
        exe = Path(temp) / "test"
        cpp.write_text(harness)
        subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", str(cpp), "-o", str(exe)], check=True)
        return subprocess.run([str(exe)], timeout=10).returncode


if __name__ == "__main__":
    raise SystemExit(main())
