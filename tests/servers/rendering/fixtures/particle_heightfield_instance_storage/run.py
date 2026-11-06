#!/usr/bin/env python3
"""Exercise production RD heightfield allocation for two instances of one base."""

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[5]
SOURCE = ROOT / "servers/rendering/renderer_rd/storage_rd/particles_storage.cpp"


def body(source, signature):
    begin = source.index("{", source.index(signature))
    depth = 0
    for end in range(begin, len(source)):
        depth += (source[end] == "{") - (source[end] == "}")
        if depth == 0:
            return source[begin:end + 1]
    raise ValueError(signature)


def main():
    source = SOURCE.read_text()
    signature = "RID ParticlesStorage::particles_collision_get_heightfield_framebuffer("
    start = source.index(signature)
    end = source.index("{", start)
    method = source[start:end] + body(source, signature)
    harness = r'''
#include <algorithm>
#include <cstdio>
#include <vector>
#define MAX(a,b) std::max((a),(b))
#define ERR_FAIL_NULL_V(p,v) if (!(p)) return (v)
#define ERR_FAIL_COND_V(p,v) if (p) return (v)
struct RID { int id=0; bool is_null() const { return !id; } bool is_valid() const { return id; } bool operator==(RID other) const { return id==other.id; } bool operator!=(RID other) const { return id!=other.id; } };
struct Vector3 { float x=1,y=1,z=1; };
struct Size2i { int x=0,y=0; };
namespace RSE { enum ParticlesCollisionType { PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE=1 }; enum ParticlesCollisionHeightfieldResolution { PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_1024=2, PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_MAX=6 }; }
struct ParticlesCollision { RSE::ParticlesCollisionType type=RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE; Vector3 extents; RSE::ParticlesCollisionHeightfieldResolution heightfield_resolution=RSE::PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_1024; RID heightfield_fb,heightfield_texture; Size2i heightfield_fb_size; };
struct ParticlesCollisionInstance { RID collision; RID heightfield_fb, heightfield_texture; Size2i heightfield_fb_size; };
template<class T> struct Owner { T *items[4]{}; T *get_or_null(RID id) const { return id.id>0&&id.id<4?items[id.id]:nullptr; } };
struct RD { enum { DATA_FORMAT_D32_SFLOAT, TEXTURE_TYPE_2D, TEXTURE_USAGE_SAMPLING_BIT=1, TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT=2 }; struct TextureFormat { int format=0,width=0,height=0,texture_type=0,usage_bits=0; }; struct TextureView {}; static RD *get_singleton() { static RD device; return &device; } int next=10; RID texture_create(TextureFormat,TextureView) { return {next++}; } RID framebuffer_create(std::vector<RID>) { return {next++}; } void free_rid(RID) {} };
template<class T> struct Vector : std::vector<T> { void push_back(T x) { std::vector<T>::push_back(x); } };
struct ParticlesStorage { Owner<ParticlesCollision> particles_collision_owner; Owner<ParticlesCollisionInstance> particles_collision_instance_owner; RID particles_collision_get_heightfield_framebuffer(RID) const; };
''' + method + r'''
int main() {
    ParticlesStorage storage; ParticlesCollision base;
    ParticlesCollisionInstance first{},second{}; first.collision={1}; second.collision={1};
    storage.particles_collision_owner.items[1]=&base;
    storage.particles_collision_instance_owner.items[1]=&first;
    storage.particles_collision_instance_owner.items[2]=&second;
    RID a=storage.particles_collision_get_heightfield_framebuffer({1});
    RID b=storage.particles_collision_get_heightfield_framebuffer({2});
    bool good=a.is_valid()&&b.is_valid()&&a!=b&&first.heightfield_texture.is_valid()&&second.heightfield_texture.is_valid()&&first.heightfield_texture!=second.heightfield_texture;
    if (!good) std::fprintf(stderr,"FAIL: shared base did not allocate distinct instance depth targets\n");
    return good?0:1;
}
'''
    with tempfile.TemporaryDirectory(prefix="particle-heightfield-instance-") as temp:
        cpp = Path(temp) / "test.cpp"
        exe = Path(temp) / "test"
        cpp.write_text(harness)
        subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", str(cpp), "-o", str(exe)], check=True)
        return subprocess.run([str(exe)], timeout=10).returncode


if __name__ == "__main__":
    raise SystemExit(main())
