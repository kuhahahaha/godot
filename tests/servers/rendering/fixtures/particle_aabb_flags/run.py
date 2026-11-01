#!/usr/bin/env python3
"""Execute the production AABB reducer on CPU snapshots with shader flag bits."""

from __future__ import annotations

import argparse
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[5]
SOURCE = "servers/rendering/renderer_rd/storage_rd/particles_storage.cpp"
HEADER = "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
SHADER = "servers/rendering/renderer_rd/shaders/particles_copy.glsl"


def read_source(path: str, ref: str | None) -> str:
    if ref:
        return subprocess.check_output(["git", "show", f"{ref}:{path}"], cwd=ROOT, text=True)
    return (ROOT / path).read_text()


def function(source: str, name: str) -> str:
    start = source.index(name)
    return source[start : source.index("\n}\n", start) + 2].replace("ParticlesStorage::", "")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    source = read_source(SOURCE, args.source_ref)
    header = read_source(HEADER, args.source_ref)
    shader = read_source(SHADER, args.source_ref)
    struct_start = header.index("\tstruct ParticleData {")
    particle_data = header[struct_start : header.index("\n\t};", struct_start) + 4]
    # Use the rendering shader's flag constants to build realistic snapshots.
    shader_flags = "\n".join(line for line in shader.splitlines() if line.startswith("#define PARTICLE_FLAG_"))
    harness = (
        r"""
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
using uint = uint32_t;
#ifdef REAL_T_IS_DOUBLE
using real_t = double;
#else
using real_t = float;
#endif
#define MAX(a, b) std::max((a), (b))
#define ERR_PRINT(message) ((void)0)
#define ERR_FAIL_NULL_V(ptr, value) if (!(ptr)) return value
#define ERR_FAIL_COND_V(condition, value) if (condition) return value
struct RID { int id = 0; bool is_valid() const { return id != 0; } bool is_null() const { return id == 0; } };
template<class T> struct Vector : std::vector<T> { const T *ptr() const { return this->data(); } };
struct Vector3 {
    real_t x = 0, y = 0, z = 0;
    Vector3() = default;
    Vector3(real_t px, real_t py, real_t pz) : x(px), y(py), z(pz) {}
    Vector3 operator+(Vector3 b) const { return {x+b.x, y+b.y, z+b.z}; }
    Vector3 operator-(Vector3 b) const { return {x-b.x, y-b.y, z-b.z}; }
};
struct Transform3D {
    Vector3 origin;
    Transform3D affine_inverse() const { return {Vector3()-origin}; }
    Vector3 xform(Vector3 p) const { return p+origin; }
};
struct AABB {
    Vector3 position, size;
    void expand_to(Vector3 p) {
        Vector3 end = position+size;
        end = {std::max(end.x,p.x), std::max(end.y,p.y), std::max(end.z,p.z)};
        position = {std::min(position.x,p.x), std::min(position.y,p.y), std::min(position.z,p.z)};
        size = end-position;
    }
    void grow_by(real_t v) { position = position-Vector3(v,v,v); size = size+Vector3(2*v,2*v,2*v); }
    float get_longest_axis_size() const { return std::max({size.x,size.y,size.z}); }
};
"""
        + shader_flags
        + "\n"
        + particle_data
        + r"""
struct Particles {
    int amount = 1;
    unsigned userdata_count = 0;
    bool trails_enabled = false, use_local_coords = true;
    Vector<Transform3D> trail_bind_poses;
    Transform3D emission_transform;
    Vector<RID> draw_passes;
    RID particle_buffer{1};
};
struct Owner { Particles value; const Particles *get_or_null(RID) const { return &value; } } particles_owner;
struct RD {
    Vector<uint8_t> snapshot;
    static RD *get_singleton() { static RD value; return &value; }
    Vector<uint8_t> buffer_get_data(RID) { return snapshot; }
};
struct MeshStorage {
    static MeshStorage *get_singleton() { static MeshStorage value; return &value; }
    AABB mesh_get_aabb(RID, RID) { return {{}, {2,1,1}}; }
};
"""
        + function(source, "bool ParticlesStorage::_particles_get_data_size")
        + "\n"
        + function(source, "AABB ParticlesStorage::particles_get_current_aabb")
        + r"""
struct Sample { uint32_t flags; Vector3 position; };
AABB capture(std::initializer_list<Sample> samples, unsigned userdata = 0, bool trails = false, bool global = false, bool mesh = false) {
    Particles &p = particles_owner.value;
    p = Particles();
    p.amount = trails ? 1 : int(samples.size());
    p.userdata_count = userdata;
    p.trails_enabled = trails;
    if (trails) p.trail_bind_poses.resize(samples.size());
    p.use_local_coords = !global;
    if (global) p.emission_transform.origin = {10,20,30};
    if (mesh) p.draw_passes.push_back({2});
    const size_t stride = sizeof(ParticleData) + userdata*4*sizeof(float);
    auto &snapshot = RD::get_singleton()->snapshot;
    snapshot.assign(stride*samples.size(), 0);
    size_t i = 0;
    for (const auto &sample : samples) {
        ParticleData data{};
        data.active = sample.flags;
        data.xform[12] = sample.position.x;
        data.xform[13] = sample.position.y;
        data.xform[14] = sample.position.z;
        std::memcpy(snapshot.data()+i++*stride, &data, sizeof(data));
    }
    return particles_get_current_aabb({1});
}
int main() {
    int checks = 0, failures = 0;
    auto check = [&](AABB box, Vector3 pos, Vector3 size, const char *name) {
        checks++;
        if (box.position.x != pos.x || box.position.y != pos.y || box.position.z != pos.z ||
                box.size.x != size.x || box.size.y != size.y || box.size.z != size.z) {
            failures++;
            std::fprintf(stderr, "FAIL: %s: position=(%g,%g,%g), size=(%g,%g,%g)\n", name,
                    double(box.position.x),double(box.position.y),double(box.position.z),
                    double(box.size.x),double(box.size.y),double(box.size.z));
        }
    };
    check(capture({{PARTICLE_FLAG_ACTIVE,{2,3,4}}, {PARTICLE_FLAG_ACTIVE,{5,7,9}}}), {2,3,4}, {3,4,5}, "active particles define bounds");
    for (uint32_t invisible : {0u, PARTICLE_FLAG_STARTED, uint(7 << 16), uint((7 << 16) | PARTICLE_FLAG_STARTED)}) {
        check(capture({{invisible,{-100,-200,-300}}, {PARTICLE_FLAG_ACTIVE,{2,3,4}}}), {2,3,4}, {}, "invisible flags before active particle are excluded");
        check(capture({{PARTICLE_FLAG_ACTIVE,{2,3,4}}, {invisible,{100,200,300}}}), {2,3,4}, {}, "invisible flags after active particle are excluded");
        check(capture({{invisible,{100,200,300}}}), {}, {}, "no visible particles produce empty bounds");
    }
    check(capture({{PARTICLE_FLAG_TRAILED,{1,2,3}}, {PARTICLE_FLAG_ACTIVE,{5,7,9}}}, 0, true), {1,2,3}, {4,5,6}, "trailed particles remain visible before activation");
    check(capture({{uint((7 << 16) | PARTICLE_FLAG_TRAILED),{1,2,3}}, {uint((7 << 16) | PARTICLE_FLAG_ACTIVE | PARTICLE_FLAG_STARTED),{5,7,9}}}, 3, true), {1,2,3}, {4,5,6}, "userdata stride and cycle flags preserve active and trail positions");
    check(capture({{uint(7 << 16),{-100,-200,-300}}, {PARTICLE_FLAG_ACTIVE,{12,23,34}}}, 0, false, true), {2,3,4}, {}, "global snapshot excludes stale position before local transform");
    check(capture({{uint(7 << 16),{-100,-200,-300}}, {PARTICLE_FLAG_ACTIVE,{2,3,4}}}, 0, false, false, true), {0,1,2}, {4,4,4}, "mesh padding grows only visible particle bounds");
    std::printf("particle AABB flags (%s): %d/%d checks passed\n", sizeof(real_t) == sizeof(double) ? "double" : "float", checks-failures, checks);
    return failures ? 1 : 0;
}
"""
    )
    result = 0
    with tempfile.TemporaryDirectory(prefix="particle-aabb-flags-") as temp:
        cpp = Path(temp) / "test.cpp"
        cpp.write_text(harness)
        for precision in ("float", "double"):
            binary = Path(temp) / precision
            command = ["c++", "-std=c++17", "-Wall", "-Wextra", "-Wno-sign-compare"]
            if precision == "double":
                command.append("-DREAL_T_IS_DOUBLE")
            subprocess.run(command + [str(cpp), "-o", str(binary)], check=True)
            result |= subprocess.run([str(binary)], timeout=10).returncode
    return result


if __name__ == "__main__":
    raise SystemExit(main())
