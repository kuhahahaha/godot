#!/usr/bin/env python3
"""Exercise production particle buffer rebuilds with injected allocation failures."""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[5]
SOURCE = "servers/rendering/renderer_rd/storage_rd/particles_storage.cpp"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    source = subprocess.check_output(["git", "show", args.source_ref + ":" + SOURCE], cwd=ROOT, text=True) if args.source_ref else (ROOT / SOURCE).read_text()
    start = source.index("void ParticlesStorage::_particles_update_buffers")
    start = source.index("\tbool uses_motion_vectors =", start)
    end = source.index("\n}\n", start)
    body = source[start:end]
    harness = r'''
#include <cstdint>
#include <cstdio>
#include <set>
#include <vector>
#define ERR_PRINT(message) ((void)0)
struct RID { int id = 0; bool is_valid() const { return id != 0; } bool is_null() const { return id == 0; } };
template<class T> using Vector = std::vector<T>;
struct PackedByteArray { void resize_initialized(uint32_t) {} };
namespace RSE { enum ParticlesDrawOrder { PARTICLES_DRAW_ORDER_INDEX, OTHER }; enum { PARTICLES_MODE_2D, PARTICLES_MODE_3D }; }
struct Dependency { enum { DEPENDENCY_CHANGED_PARTICLES_INSTANCES }; int changes = 0; void changed_notify(int) { changes++; } };
struct Viewport { int count = 1; int get_num_viewports_with_motion_vectors() { return count; } };
namespace RSG { Viewport value; Viewport *viewport = &value; }
struct RendererCompositorStorage { static RendererCompositorStorage *get_singleton() { static RendererCompositorStorage v; return &v; } int get_num_compositor_effects_with_motion_vectors() { return 0; } };
struct ParticleData { float data[28]; };
struct Particles {
    int amount = 4, mode = RSE::PARTICLES_MODE_3D;
    unsigned userdata_count = 0;
    RSE::ParticlesDrawOrder draw_order = RSE::PARTICLES_DRAW_ORDER_INDEX;
    bool instance_motion_vectors_enabled = false, restart_request = false;
    RID particle_buffer, particle_instance_buffer, particles_copy_uniform_set, particles_transforms_buffer_uniform_set;
    unsigned instance_motion_vectors_current_offset = 0, instance_motion_vectors_previous_offset = 0;
    uint64_t instance_motion_vectors_last_change = 0;
    Dependency dependency;
};
struct RD {
    enum { UNIFORM_TYPE_STORAGE_BUFFER };
    struct Uniform { int uniform_type = 0, binding = 0; void append_id(RID) {} };
    int next = 1, buffer_creates = 0, uniform_creates = 0;
    bool fail_buffer = false, fail_uniform = false;
    std::set<int> live;
    static RD *get_singleton() { static RD v; return &v; }
    bool uniform_set_is_valid(RID rid) { return live.count(rid.id); }
    void free_rid(RID rid) { live.erase(rid.id); }
    RID storage_buffer_create(uint32_t, const PackedByteArray & = PackedByteArray()) { buffer_creates++; if (fail_buffer) { fail_buffer = false; return {}; } RID rid{next++}; live.insert(rid.id); return rid; }
    RID uniform_set_create(const Vector<Uniform> &, RID, int) { uniform_creates++; if (fail_uniform) { fail_uniform = false; return {}; } RID rid{next++}; live.insert(rid.id); return rid; }
};
struct Shader { RID version_get_shader(int, int) { return {1}; } };
struct { Shader copy_shader; int copy_shader_version = 1; } particles_shader;
bool _particles_get_total_amount(Particles *p, uint32_t &amount) { amount = p->amount; return amount > 0; }
bool _particles_get_buffer_size(uint64_t n, uint64_t size, uint64_t header, uint32_t &out) { uint64_t total = n * size + header; if (total > UINT32_MAX) return false; out = uint32_t(total); return true; }
void _particles_free_data(Particles *p) { auto rd = RD::get_singleton(); rd->free_rid(p->particle_buffer); rd->free_rid(p->particle_instance_buffer); rd->free_rid(p->particles_copy_uniform_set); p->particle_buffer = {}; p->particle_instance_buffer = {}; p->particles_copy_uniform_set = {}; }
void update(Particles *particles, unsigned userdata_count = 0) {
''' + body + r'''
}
int main() {
    int checks = 0, failures = 0;
    auto check = [&](bool result, const char *name) { checks++; if (!result) { failures++; std::fprintf(stderr, "FAIL: %s\n", name); } };
    RD *rd = RD::get_singleton();
    for (int failure = 0; failure < 2; failure++) {
        *rd = RD();
        Particles p;
        RSG::viewport->count = 0;
        update(&p);
        const RID original = p.particle_buffer;
        check(original.is_valid() && p.particle_instance_buffer.is_valid() && !p.instance_motion_vectors_enabled, "initial single-frame buffers ready");
        RSG::viewport->count = 1;
        rd->fail_buffer = failure == 0;
        rd->fail_uniform = failure == 1;
        update(&p);
        check(p.particle_buffer.id == original.id && p.particle_instance_buffer.is_null() && !p.instance_motion_vectors_enabled, "failed upgrade preserves simulation and pending capability");
        check(p.dependency.changes == 0, "failed upgrade does not notify readiness");
        update(&p);
        check(p.particle_buffer.id == original.id && p.particle_instance_buffer.is_valid() && rd->uniform_set_is_valid(p.particles_copy_uniform_set) && p.instance_motion_vectors_enabled, "next update retries and completes upgrade");
        check(p.dependency.changes == 1, "successful upgrade notifies once");
        const int creates = rd->buffer_creates;
        update(&p);
        check(rd->buffer_creates == creates, "ready buffers are reused");
        RSG::viewport->count = 0;
        update(&p, 1);
        check(!p.instance_motion_vectors_enabled, "full rebuild publishes actual single-frame layout");
        _particles_free_data(&p);
        check(rd->live.empty(), "all mock resources released");
    }
    for (int failure = 0; failure < 2; failure++) {
        *rd = RD();
        Particles p;
        RSG::viewport->count = 0;
        update(&p);
        const RID original = p.particle_buffer;
        RSG::viewport->count = 1;
        rd->fail_buffer = failure == 0;
        rd->fail_uniform = failure == 1;
        update(&p);
        RSG::viewport->count = 0;
        update(&p);
        check(p.particle_buffer.id == original.id && p.particle_instance_buffer.is_valid() && rd->uniform_set_is_valid(p.particles_copy_uniform_set) && !p.instance_motion_vectors_enabled, "failed upgrade retries after motion vector demand disappears");
        check(p.dependency.changes == 1, "recovered single-frame layout notifies once");
        rd->free_rid(p.particles_copy_uniform_set);
        update(&p);
        check(p.particle_buffer.id == original.id && p.particle_instance_buffer.is_valid() && rd->uniform_set_is_valid(p.particles_copy_uniform_set), "invalid copy uniform rebuilds instances without resetting simulation");
        check(p.dependency.changes == 2, "invalid copy uniform recovery notifies once");
        _particles_free_data(&p);
        check(rd->live.empty(), "recovered instance resources released");
    }
    std::printf("particle buffer retry: %d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
'''
    with tempfile.TemporaryDirectory(prefix="particle-buffer-retry-") as temp:
        cpp, binary = Path(temp) / "test.cpp", Path(temp) / "test"
        cpp.write_text(harness)
        subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", str(cpp), "-o", str(binary)], check=True)
        return subprocess.run([str(binary)], timeout=10).returncode


if __name__ == "__main__":
    raise SystemExit(main())
