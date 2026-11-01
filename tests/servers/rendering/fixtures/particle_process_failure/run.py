#!/usr/bin/env python3
"""Exercise the production particle dispatch tail against a recording fake RD."""

import argparse
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[5]
SOURCE = "servers/rendering/renderer_rd/storage_rd/particles_storage.cpp"


def harness(source):
    process_start = source.find("bool ParticlesStorage::_particles_process")
    if process_start < 0:
        process_start = source.index("void ParticlesStorage::_particles_process")
    marker = "\tif (!RD::get_singleton()->compute_pipeline_is_valid"
    if marker not in source[process_start:]:
        marker = "\tERR_FAIL_NULL(m);"
    start = source.index(marker, process_start)
    end = source.index("\n}\n", start)
    body = source[start:end]
    body = (
        body
        .replace("\treturn;", "\treturn false;")
        .replace("return fail_process();", "return false;")
        .replace(
            "_particles_commit_process_state(p_particles, sub_emitter, sub_emitter_was_emitting, true);",
            "p_particles->force_sub_emit = false;",
        )
    )
    body += "\n\treturn true;\n"
    return (
        r"""
#include <cstdio>
#include <vector>
#define ERR_PRINT(message) ((void)0)
#define ERR_FAIL_NULL(value) do { if (!(value)) return false; } while (0)
#define ERR_FAIL_NULL_V(value, result) do { if (!(value)) return result; } while (0)
struct RID { int id = 0; bool is_valid() const { return id != 0; } };
struct TextureList { bool empty = true; bool is_empty() const { return empty; } };
struct Pipeline { RID get_rid() const { return {1}; } };
struct Shader { Pipeline pipeline; unsigned ubo_size = 0; TextureList texture_uniforms; bool uses_collision = false; };
struct Material { Shader *shader_data; RID uniform_set; int used = 0; void set_as_used() { used++; } };
struct Particles { struct Emission { int particle_count = 0; }; bool has_collision_cache = false; RID particles_material_uniform_set{3}; RID collision_textures_uniform_set{4}; bool trails_enabled = false; std::vector<int> trail_bind_poses; unsigned amount = 4; bool force_sub_emit = false; bool clear = false; bool emitting = false; bool inactive = false; double inactive_time = 0; double speed_scale = 1.0; double trail_params_speed_scale = 1.0; RID emission_storage_buffer{1}; Emission *emission_buffer = nullptr; };
struct ParticlesShader { struct PushConstant { bool trail_pass = false; }; RID base_uniform_set{2}; };
struct RD {
    using ComputeListID = long long;
    static constexpr ComputeListID INVALID_ID = -1;
    int begins = 0, ends = 0, binds = 0, dispatches = 0, custom_binds = 0;
    bool list_available = true, active = false;
    static RD *get_singleton() { static RD value; return &value; }
    bool compute_pipeline_is_valid(RID rid) { return rid.id == 1; }
    bool uniform_set_is_valid(RID rid) { return rid.id >= 2 && rid.id <= 5; }
    ComputeListID compute_list_begin() { begins++; if (!list_available || active) return INVALID_ID; active = true; return 8; }
    void compute_list_end() { ends++; active = false; }
    void compute_list_bind_compute_pipeline(ComputeListID, RID) { binds++; }
    void compute_list_bind_uniform_set(ComputeListID, RID, unsigned set) { binds++; if (set == 3) custom_binds++; }
    void compute_list_set_push_constant(ComputeListID, const void *, unsigned) { binds++; }
    void compute_list_dispatch_threads(ComputeListID, unsigned, unsigned, unsigned) { dispatches++; }
    void compute_list_add_barrier(ComputeListID) { binds++; }
};
bool process(Material *m, Particles *p_particles) {
    ParticlesShader particles_shader;
    ParticlesShader::PushConstant push_constant;
    constexpr int BASE_UNIFORM_SET = 0, MATERIAL_UNIFORM_SET = 1, COLLISION_TEXTURTES_UNIFORM_SET = 2;
    unsigned process_amount = 4;
    Particles sub_emitter_storage;
    Particles *sub_emitter = nullptr;
    bool sub_emitter_was_emitting = false;
"""
        + body
        + r"""
}
struct Case { const char *name; unsigned ubo; bool texture; int uniform; bool active; bool trails; bool success; int expected_begin; };
int main() {
    const Case cases[] = {
        {"optional empty uniform", 0, false, 0, false, false, true, 1},
        {"valid scalar uniform", 16, false, 5, false, false, true, 1},
        {"valid texture uniform", 0, true, 5, false, false, true, 1},
        {"missing scalar uniform", 16, false, 0, false, false, false, 0},
        {"missing texture uniform", 0, true, 0, false, false, false, 0},
        {"stale scalar uniform", 16, false, 6, false, false, false, 0},
        {"stale texture uniform", 0, true, 6, false, false, false, 0},
        {"existing compute list", 16, false, 5, true, false, false, 1},
        {"valid trail dispatch", 16, false, 5, false, true, true, 1},
    };
    int failures = 0;
    RD *rd = RD::get_singleton();
    for (const Case &test : cases) {
        *rd = RD();
        rd->active = test.active;
        Shader shader;
        shader.ubo_size = test.ubo;
        shader.texture_uniforms.empty = !test.texture;
        Material material{&shader, {test.uniform}};
        Particles particles;
        particles.trails_enabled = test.trails;
        if (test.trails) particles.trail_bind_poses = {0, 1};
        bool result = process(&material, &particles);
        bool ok = result == test.success && rd->begins == test.expected_begin && rd->ends == int(test.success);
        ok = ok && rd->dispatches == (test.success ? (test.trails ? 2 : 1) : 0);
        ok = ok && rd->active == test.active;
        ok = ok && rd->custom_binds == int(test.success && test.uniform == 5);
        ok = ok && material.used == int(test.success && test.uniform == 5);
        if (!test.success) ok = ok && rd->binds == 0;
        if (!ok) {
            failures++;
            std::fprintf(stderr, "%s: success=%d begin=%d end=%d bind=%d dispatch=%d active=%d\n", test.name, result, rd->begins, rd->ends, rd->binds, rd->dispatches, rd->active);
        }
    }
    std::printf("particle process dispatch harness: %d/%zu cases passed\n", int(sizeof(cases)/sizeof(cases[0]))-failures, sizeof(cases)/sizeof(cases[0]));
    return failures ? 1 : 0;
}
"""
    )


def inactivity_harness(source):
    update_start = source.index("void ParticlesStorage::update_particles()")
    # Extract the retry snapshot and failure handler along with the complete
    # inactivity transition, so this verifies production state ownership.
    start = source.index("\n", source.index("\t\tconst bool retry_candidate = ", update_start))
    end = source.index("\n\t\t_particles_update_buffers(particles);", start)
    retry = source[start:end]
    start = source.index("\t\tif (particles->inactive && !particles->emitting)", end)
    end = source.index("\t\t// TODO: Should use display refresh rate", start)
    inactivity = source[start:end]
    prefix = harness(source).split("struct Case {", 1)[0]
    return (
        prefix
        + r"""
struct Node {
    bool listed = false;
    bool in_list() { return listed; }
    void remove_from_list() { listed = false; }
};
struct RetryParticles : Particles {
    bool dirty = false, retry_pending = false, frame_delta_committed = false, pre_process_pending = false;
    unsigned frame_delta_committed_frame = ~0U;
    double lifetime = 1, phase = 0, prev_phase = 0, frame_remainder = 0, pre_process_time_remaining = 0;
    int prev_ticks = 0;
    Node update_list;
};
struct RendererCompositorRD {
    static RendererCompositorRD *get_singleton() { static RendererCompositorRD value; return &value; }
    double get_frame_delta_time() { return 1.0 / 60.0; }
};
bool update(Material *material, RetryParticles *particles) {
    struct { void add(Node *node) { node->listed = true; } } retry_list;
    bool retrying = false;
    particles->update_list.listed = false;
    for (int once = 0; once < 1; once++) {
"""
        + retry
        + inactivity
        + r"""
        if (!process(material, particles)) {
            queue_retry();
            continue;
        }
        return true;
    }
    return false;
}
int main() {
    RD *rd = RD::get_singleton();
    Shader shader;
    Material material{&shader, {}};
    int failures = 0;
    for (bool waking : {false, true}) {
        *rd = RD();
        RetryParticles particles;
        particles.inactive = waking;
        particles.emitting = waking;
        // One frame remains below the cutoff; replaying its delta crosses it.
        particles.inactive_time = 1.18;
        rd->list_available = false;
        bool failed = !update(&material, &particles);
        bool restored = particles.inactive == waking && particles.inactive_time == 1.18 &&
                particles.dirty && particles.retry_pending && particles.update_list.in_list() && rd->dispatches == 0;
        rd->list_available = true;
        bool recovered = update(&material, &particles);
        bool committed = !particles.inactive && !particles.dirty && !particles.retry_pending &&
                !particles.update_list.in_list() && rd->dispatches == 1 &&
                particles.inactive_time == (waking ? 0.0 : 1.18 + 1.0 / 60.0);
        if (!failed || !restored || !recovered || !committed) {
            failures++;
            std::fprintf(stderr, "inactivity retry waking=%d: failed=%d restored=%d recovered=%d committed=%d inactive=%d time=%.8f dispatches=%d\n",
                    waking, failed, restored, recovered, committed, particles.inactive, particles.inactive_time, rd->dispatches);
        }
    }
    std::printf("particle inactivity retry harness: %d/2 cases passed\n", 2 - failures);
    return failures ? 1 : 0;
}
"""
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    source = (
        subprocess.check_output(["git", "show", args.source_ref + ":" + SOURCE], cwd=ROOT, text=True)
        if args.source_ref
        else (ROOT / SOURCE).read_text()
    )
    with tempfile.TemporaryDirectory(prefix="particle-process-dispatch-") as temp:
        cpp, binary = Path(temp) / "test.cpp", Path(temp) / "test"
        cpp.write_text(harness(source))
        subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", str(cpp), "-o", str(binary)], check=True)
        result = subprocess.run([str(binary)]).returncode
        # Historical source revisions may predate deferred retries entirely.
        if "auto queue_retry = [&]()" in source:
            cpp.write_text(inactivity_harness(source))
            subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", str(cpp), "-o", str(binary)], check=True)
            result |= subprocess.run([str(binary)]).returncode
        return result


if __name__ == "__main__":
    raise SystemExit(main())
