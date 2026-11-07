#!/usr/bin/env python3
"""Exercise SceneCull heightfield selection from each receiver's real pairs."""

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
    method = body(SOURCE.read_text(), "bool RendererSceneCull::scene_heightfield_snapshot_pair(")
    harness = r'''
#include <cstdint>
#include <cstdio>
struct RID { int id=0; };
struct Transform3D { int origin=0; };
struct AABB { float min=0,max=1; bool intersects_inclusive(const AABB &other) const { return min<=other.max && max>=other.min; } };
struct Vector3 { float x=0,y=0,z=0; };
namespace RSE { enum { INSTANCE_PARTICLES=1, INSTANCE_PARTICLES_COLLISION=2 }; enum ParticlesCollisionHeightfieldResolution { PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_1024=2 }; }
struct SceneHeightfieldSource { uint64_t source_key=0,shape_revision=0; RID collision_instance; Transform3D transform; Vector3 extents; RSE::ParticlesCollisionHeightfieldResolution resolution=RSE::PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_1024; };
struct Instance; struct InstancePair;
template<class T> struct SelfList { T *pair=nullptr; SelfList *next_link=nullptr; T *self() const { return pair; } SelfList *next() const { return next_link; } };
struct PairList { SelfList<InstancePair> *head=nullptr; SelfList<InstancePair> *first() const { return head; } };
struct InstanceParticlesCollisionData { RID instance; uint32_t cull_mask=1; };
struct Instance {
    int base_type=0; RID base; bool visible=true; void *scenario=nullptr; uint32_t layer_mask=1;
    bool scene_query_committed=false; AABB scene_query_world; AABB transformed_aabb;
    PairList pairs; InstanceParticlesCollisionData collision_data; void *base_data=&collision_data;
};
struct InstancePair { Instance *a=nullptr,*b=nullptr; SelfList<InstancePair> link; InstancePair(Instance *x,Instance *y):a(x),b(y) { link.pair=this; } };
template<class T> struct Owner { T *items[8]{}; T *get_or_null(RID rid) const { return rid.id>0 && rid.id<8 ? items[rid.id]:nullptr; } };
namespace RendererRD { struct ParticlesStorage {
    static ParticlesStorage *single;
    bool active[8]{}; uint64_t keys[8]{}, revs[8]{}; int origins[8]{}; Vector3 extents[8]{};
    static ParticlesStorage *get_singleton() { return single; }
    bool particles_collision_instance_get_scene_heightfield(RID id,uint64_t &key,uint64_t &rev,Transform3D &transform,Vector3 &extents,RSE::ParticlesCollisionHeightfieldResolution &resolution) const {
        if (id.id<=0 || id.id>=8 || !active[id.id]) return false;
        key=keys[id.id]; rev=revs[id.id]; transform.origin=origins[id.id]; extents=this->extents[id.id]; resolution=RSE::PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_1024; return true;
    }
}; ParticlesStorage *ParticlesStorage::single=nullptr; }
struct RendererSceneCull {
    Owner<Instance> instance_owner;
    bool scene_heightfield_snapshot_pair(RID,SceneHeightfieldSource&,bool&) const;
};
bool RendererSceneCull::scene_heightfield_snapshot_pair(RID p_receiver,SceneHeightfieldSource &r_source,bool &r_present) const ''' + method + r'''
int main() {
    RendererRD::ParticlesStorage storage; RendererRD::ParticlesStorage::single=&storage;
    storage.active[1]=storage.active[2]=true; storage.keys[1]=11; storage.keys[2]=22; storage.revs[1]=storage.revs[2]=7;
    storage.origins[1]=1; storage.origins[2]=2; storage.extents[1]=storage.extents[2]={1,2,3};
    RendererSceneCull scene; int scenario=1;
    Instance receiver; receiver.base_type=1; receiver.base={1}; receiver.scenario=&scenario;
    Instance other_receiver; other_receiver.base_type=1; other_receiver.base={1}; other_receiver.scenario=&scenario;
    Instance left; left.base_type=2; left.base={2}; left.scenario=&scenario; left.collision_data.instance={1};
    Instance right; right.base_type=2; right.base={3}; right.scenario=&scenario; right.collision_data.instance={2};
    Instance foreign; foreign.base_type=2; foreign.base={4}; foreign.collision_data.instance={2};
    InstancePair a{&receiver,&left},b{&receiver,&right},c{&receiver,&foreign},d{&other_receiver,&right};
    a.link.next_link=&b.link; b.link.next_link=&c.link;
    receiver.pairs.head=&a.link; other_receiver.pairs.head=&d.link;
    scene.instance_owner.items[1]=&receiver; scene.instance_owner.items[2]=&other_receiver;
    SceneHeightfieldSource out; bool present=false,good=true;
    auto check=[&](bool ok,const char *label){ if(!ok) std::fprintf(stderr,"FAIL %s\n",label); good &= ok; };
    check(!scene.scene_heightfield_snapshot_pair({1},out,present) && !present,"two fields reject whole sample");
    check(scene.scene_heightfield_snapshot_pair({2},out,present) && present && out.source_key==22 && out.transform.origin==2,"same base second receiver selects own field");
    right.collision_data.cull_mask=0;
    check(scene.scene_heightfield_snapshot_pair({1},out,present) && present && out.source_key==11,"cull mask removes one field");
    receiver.visible=false;
    check(scene.scene_heightfield_snapshot_pair({1},out,present) && !present,"hidden receiver empty");
    receiver.visible=true; left.visible=false;
    check(scene.scene_heightfield_snapshot_pair({1},out,present) && !present,"hidden source empty");
    left.visible=true; right.collision_data.cull_mask=0;
    storage.origins[1]=9; storage.extents[1]={2,4,6}; storage.revs[1]=8;
    check(scene.scene_heightfield_snapshot_pair({1},out,present) && present && out.source_key==11 && out.shape_revision==8 && out.transform.origin==9 && out.extents.x==2 && out.extents.y==4 && out.extents.z==6,"source transform extents revision update");
    right.collision_data.cull_mask=1;
    storage.active[1]=false;
    check(scene.scene_heightfield_snapshot_pair({1},out,present) && present && out.source_key==22,"released source removed while other source remains");
    storage.active[1]=true; storage.keys[1]=33; storage.revs[1]=12;
    check(!scene.scene_heightfield_snapshot_pair({1},out,present) && !present,"recreated second field rejects ambiguous pair");
    right.collision_data.cull_mask=0;
    check(scene.scene_heightfield_snapshot_pair({1},out,present) && present && out.source_key==33 && out.shape_revision==12,"recreated source key and revision are fresh");
    check(out.collision_instance.id==1,"selected field retains original collision instance RID");
    receiver.scene_query_committed=true; receiver.scene_query_world={0,1};
    left.transformed_aabb={2,3};
    check(scene.scene_heightfield_snapshot_pair({1},out,present) && !present,"committed query excludes disjoint field");
    left.transformed_aabb={1,2};
    check(scene.scene_heightfield_snapshot_pair({1},out,present) && present && out.collision_instance.id==1,"committed query includes touching field with original RID");
    check(!scene.scene_heightfield_snapshot_pair({7},out,present) && !present,"unknown receiver rejects");
    return good?0:1;
}
'''
    with tempfile.TemporaryDirectory(prefix="particle-scene-heightfield-pairs-") as temp:
        cpp = Path(temp) / "test.cpp"
        exe = Path(temp) / "test"
        cpp.write_text(harness)
        subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", str(cpp), "-o", str(exe)], check=True)
        return subprocess.run([str(exe)], timeout=10).returncode


if __name__ == "__main__":
    raise SystemExit(main())
