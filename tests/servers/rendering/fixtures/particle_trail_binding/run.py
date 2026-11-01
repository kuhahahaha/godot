#!/usr/bin/env python3
"""Compile production trail setup against a recording resource mock (not GPU QA)."""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[5]
SOURCE = "servers/rendering/renderer_rd/storage_rd/particles_storage.cpp"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-file", type=Path)
    args = parser.parse_args()
    source = (args.source_file or ROOT / SOURCE).read_text()

    def function(name):
        start = source.index(name)
        start = source.index("{", start) + 1
        return source[start:source.index("\n}\n", start)]

    setter = function("void ParticlesStorage::particles_set_trail_bind_poses")
    uniform = function("bool ParticlesStorage::_particles_ensure_trail_uniform")
    start = source.index("\t\t\tif (particles->trail_bind_poses.size() > 1 && particles->trail_bind_pose_buffer.is_null())", source.index("void ParticlesStorage::update_particles"))
    allocation = source[start:source.index("\n\t\t\tif (particles->trail_bind_pose_buffer.is_valid() && particles->trail_bind_poses_dirty)", start)]
    harness = r'''
#include <cstdio>
#include <cstdint>
#include <map>
#include <vector>
#define ERR_PRINT(message) ((void)0)
#define ERR_FAIL_NULL(value) do { if (!(value)) return; } while (0)
struct RID { int id = 0; bool is_valid() const { return id != 0; } bool is_null() const { return id == 0; } };
template<class T> using Vector = std::vector<T>;
struct Transform3D {};
struct Dependency { enum { DEPENDENCY_CHANGED_PARTICLES }; void changed_notify(int) {} };
struct Particles {
    Vector<Transform3D> trail_bind_poses;
    RID trail_bind_pose_buffer, trail_bind_pose_uniform_set, unused_trail_storage_buffer;
    bool trail_bind_poses_dirty = false, clear = false;
    int prev_ticks = 7, phase = 1, prev_phase = 1, frees = 0;
    Dependency dependency;
};
struct RD {
    enum { UNIFORM_TYPE_STORAGE_BUFFER };
    struct Uniform { int uniform_type = 0, binding = 0; RID buffer; void append_id(RID rid) { buffer = rid; } };
    bool fail_buffer = false;
    int next = 1, uniform_attempts = 0;
    std::map<int, int> bound_buffers;
    static RD *get_singleton() { static RD rd; return &rd; }
    RID storage_buffer_create(uint32_t) { if (fail_buffer) { fail_buffer = false; return {}; } return {next++}; }
    RID uniform_set_create(const Vector<Uniform> &uniforms, RID, int) { uniform_attempts++; RID result{next++}; bound_buffers[result.id] = uniforms[0].buffer.id; return result; }
};
struct Shader { RID version_get_shader(int, int) { return {100}; } };
struct { Shader copy_shader; int copy_shader_version = 1; } particles_shader;
struct Owner { Particles *value = nullptr; Particles *get_or_null(RID) { return value; } } particles_owner;
void _particles_free_data(Particles *p) { p->frees++; p->trail_bind_pose_buffer = {}; p->trail_bind_pose_uniform_set = {}; }
bool _particles_get_buffer_size(uint64_t count, uint64_t size, uint64_t header, uint32_t &out) { out = count * size + header; return true; }
bool _particles_ensure_unused_trail_buffer(Particles *p) { if (p->unused_trail_storage_buffer.is_null()) p->unused_trail_storage_buffer = RD::get_singleton()->storage_buffer_create(64); return p->unused_trail_storage_buffer.is_valid(); }
bool _particles_ensure_trail_uniform(Particles *particles, RID p_shader) {
''' + uniform + r'''
}
void set_poses(Particles *p, const Vector<Transform3D> &p_bind_poses) {
    particles_owner.value = p;
    RID p_particles;
''' + setter + r'''
}
bool prepare(Particles *particles) {
    bool retry = false;
    auto queue_retry = [&]() { retry = true; };
    for (int once = 0; once < 1; once++) {
''' + allocation + r'''
    }
    return !retry;
}
int main() {
    int checks = 0, failures = 0;
    auto check = [&](bool result, const char *name) { checks++; if (!result) { failures++; std::fprintf(stderr, "FAIL: %s\n", name); } };
    RD *rd = RD::get_singleton();
    for (int initial_count : {0, 1}) {
        Particles p;
        p.trail_bind_poses.resize(initial_count);
        check(prepare(&p), "initial fallback setup succeeds");
        check(rd->bound_buffers[p.trail_bind_pose_uniform_set.id] == p.unused_trail_storage_buffer.id, "initial uniform uses fallback");
        set_poses(&p, Vector<Transform3D>(3));
        check(p.frees == 1 && p.trail_bind_pose_uniform_set.is_null() && p.clear, "pose count change invalidates fallback layout and restarts simulation");
        check(prepare(&p), "new trail setup succeeds");
        check(rd->bound_buffers[p.trail_bind_pose_uniform_set.id] == p.trail_bind_pose_buffer.id, "new uniform binds real trail buffer");
        const int uniform_id = p.trail_bind_pose_uniform_set.id;
        set_poses(&p, Vector<Transform3D>(3));
        check(p.frees == 1 && p.trail_bind_pose_uniform_set.id == uniform_id && p.trail_bind_poses_dirty, "same-size pose update retains layout and marks upload dirty");
    }
    Particles p;
    p.trail_bind_poses.resize(3);
    const int previous_attempts = rd->uniform_attempts;
    rd->fail_buffer = true;
    check(!prepare(&p), "real trail allocation failure requests retry");
    check(p.trail_bind_pose_uniform_set.is_null() && rd->uniform_attempts == previous_attempts, "failed real trail allocation does not publish fallback uniform");
    check(prepare(&p), "next attempt retries real allocation");
    check(rd->bound_buffers[p.trail_bind_pose_uniform_set.id] == p.trail_bind_pose_buffer.id, "recovered uniform binds real trail buffer");
    std::printf("particle trail binding: %d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
'''
    with tempfile.TemporaryDirectory(prefix="particle-trail-binding-") as temp:
        cpp, binary = Path(temp) / "test.cpp", Path(temp) / "test"
        cpp.write_text(harness)
        subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", str(cpp), "-o", str(binary)], check=True)
        return subprocess.run([str(binary)], timeout=10).returncode


if __name__ == "__main__":
    raise SystemExit(main())
