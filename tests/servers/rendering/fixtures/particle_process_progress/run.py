#!/usr/bin/env python3
"""Run the production update loops with bounded, recording particle dispatches."""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[5]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source-ref')
    args = parser.parse_args()
    source_path = 'servers/rendering/renderer_rd/storage_rd/particles_storage.cpp'
    source = subprocess.check_output(['git', 'show', args.source_ref + ':' + source_path], cwd=ROOT, text=True) if args.source_ref else (ROOT / source_path).read_text()
    start = source.index('\t\tfloat todo = ', source.index('void ParticlesStorage::update_particles()'))
    end = source.index('\t\t// Ensure that memory is initialized', start)
    body = source[start:end]
    # Include the input validation immediately before the loop, when present.
    marker = '\t\t// Validate process times before comparisons'
    if marker in source:
        body = source[source.index(marker):end]
    retry_start = source.index('\t\tconst bool retry_candidate = ', source.index('void ParticlesStorage::update_particles()'))
    retry_candidate_end = source.index('\n', retry_start)
    retry_recheck_start = source.index('\t\tbool retrying = ', retry_candidate_end)
    retry_recheck_end = source.index('\n', retry_recheck_start)
    retry_candidate_logic = source[retry_start:retry_candidate_end]
    retry_recheck_logic = source[retry_recheck_start:retry_recheck_end]
    harness = r'''
#include <cfloat>
#include <cmath>
#include <cstdio>
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ERR_PRINT(message) ((void)0)
#ifdef REAL_T_IS_DOUBLE
using real_t = double;
#else
using real_t = float;
#endif
namespace Math { template<class T> bool is_finite(T value) { return std::isfinite(value); } }
struct Particles {
    bool clear = false, emitting = true, dirty = false, retry_pending = false, frame_delta_committed = false, pre_process_pending = false, restart_request = false;
    double pre_process_time = 0;
    real_t request_process_time = 0, request_process_time_residual = 0;
    double speed_scale = 1, frame_remainder = 0;
    real_t pre_process_time_remaining = 0;
    unsigned frame_delta_committed_frame = UINT32_MAX;
    int frames = 0;
};
struct RendererCompositorRD {
    double delta = 1.0 / 60.0;
    static RendererCompositorRD *get_singleton() { static RendererCompositorRD value; return &value; }
    double get_frame_delta_time() { return delta; }
};
int attempts = 0;
int fail_on_attempt = 0;
bool fail_dispatch = false;
bool fail_preflight = false;
bool simulate_resource_rebuild = false;
unsigned current_frame = 0;
bool _particles_process(Particles *particles, double delta) {
    // Bound old broken loops without executing GPU work or waiting for a timeout.
    if (++attempts > 32 || fail_dispatch || attempts == fail_on_attempt) return false;
    if (!std::isfinite(delta)) return false;
    particles->clear = false;
    particles->frames++;
    return true;
}
bool update(Particles *particles, int fixed_fps = 60, bool advance_frame = true) {
    if (advance_frame) current_frame++;
    unsigned frame = current_frame;
''' + retry_candidate_logic + r'''
    particles->retry_pending = false;
    particles->dirty = false;
    if (simulate_resource_rebuild) {
        particles->frame_remainder = 0;
        particles->frame_delta_committed = false;
        particles->frame_delta_committed_frame = UINT32_MAX;
    }
''' + retry_recheck_logic + r'''
    if (particles->restart_request) {
        particles->restart_request = false;
        particles->clear = true;
        particles->frame_remainder = 0;
        particles->frame_delta_committed = false;
        particles->frame_delta_committed_frame = UINT32_MAX;
        retrying = false;
    }
    for (int once = 0; once < 1; once++) {
''' + '        auto queue_retry = [&]() { particles->dirty = true; particles->retry_pending = true; };\n        if (fail_preflight) { queue_retry(); return false; }\n' + body + r'''
        particles->frame_delta_committed = false;
        particles->frame_delta_committed_frame = UINT32_MAX;
''' + r'''
        return true;
    }
    return false;
}
int main() {
    int failures = 0, checks = 0;
    auto check = [&](bool value, const char *name) { checks++; if (!value) { failures++; std::fprintf(stderr, "FAIL: %s\n", name); } };
    for (double scale : {1e10, double(FLT_MAX), DBL_MAX}) {
        for (bool clear : {false, true}) {
            Particles p; p.speed_scale = scale; p.clear = clear; p.frame_remainder = 0.001;
            attempts = 0;
            check(!update(&p) && attempts == 0 && p.frames == 0 && p.clear == clear && p.frame_remainder == 0.001 && p.speed_scale == scale, "large scale fails before dispatch and remainder commit");
        }
    }
    for (double delta : {double(NAN), double(INFINITY), DBL_MAX}) {
        Particles p; attempts = 0; RendererCompositorRD::get_singleton()->delta = delta;
        check(!update(&p) && attempts == 0 && p.frame_remainder == 0, "invalid frame delta fails before dispatch");
    }
    RendererCompositorRD::get_singleton()->delta = 0;
    { Particles p; p.speed_scale = DBL_MAX; attempts = 0;
      check(!update(&p) && attempts == 0 && p.frame_remainder == 0, "zero delta cannot commit NaN remainder"); }
    RendererCompositorRD::get_singleton()->delta = 1.0 / 60.0;
    for (int field = 0; field < 3; field++) {
        for (float value : {NAN, INFINITY, -INFINITY, FLT_MAX}) {
            Particles p; p.clear = true; attempts = 0;
            if (field == 0) p.pre_process_time = value;
            if (field == 1) p.request_process_time = value;
            if (field == 2) p.request_process_time_residual = value;
            check(!update(&p) && attempts == 0 && p.frames == 0 && p.clear, "invalid process time fails before dispatch");
        }
    }
    { Particles p; p.clear = true; p.pre_process_time = 0.01f; p.speed_scale = DBL_MAX;
      attempts = 0; fail_dispatch = true;
      check(!update(&p) && p.speed_scale == DBL_MAX && p.clear && p.pre_process_time == 0.01f, "failed preprocess restores double scale");
      fail_dispatch = false; }
    { Particles p; p.clear = true; p.pre_process_time = 0.75; attempts = 0; fail_on_attempt = 2;
      RendererCompositorRD::get_singleton()->delta = 0;
      check(!update(&p, 4) && !p.clear && p.pre_process_pending && p.pre_process_time_remaining == 0.5, "partial preprocess keeps remaining time without replaying clear");
      fail_on_attempt = 0;
      check(update(&p, 4) && !p.clear && p.frames == 3 && !p.pre_process_pending, "preprocess retry consumes only remaining time"); }
    { Particles p; p.clear = true; p.pre_process_time = DBL_MAX; attempts = 0;
      check(!update(&p) && attempts == 0 && p.clear, "double preprocess overflow fails before dispatch"); }
    { Particles p; p.clear = true; p.pre_process_time = 0.01; p.speed_scale = 1.00000000001; attempts = 0;
      check(update(&p) && p.speed_scale == 1.00000000001, "successful preprocess restores exact double scale"); }
    { Particles p; p.frame_remainder = DBL_MAX; attempts = 0;
      check(!update(&p) && attempts == 0 && p.frame_remainder == DBL_MAX, "remainder conversion overflow is not committed"); }
    { Particles p; p.speed_scale = FLT_MAX; attempts = 0;
      RendererCompositorRD::get_singleton()->delta = DBL_MAX;
      check(!update(&p, 0) && attempts == 0, "variable delta multiplication overflow fails before dispatch");
      RendererCompositorRD::get_singleton()->delta = 1.0 / 60.0; }
    { Particles p; attempts = 0;
      check(update(&p) && attempts == 1 && p.frames == 1 && p.frame_remainder == 0, "ordinary frame advances"); }
    { Particles p; p.clear = true; p.speed_scale = 0; attempts = 0;
      check(update(&p) && attempts == 1 && !p.clear, "paused clear still initializes"); }
    { Particles p; p.frame_remainder = 0.001; attempts = 0; fail_dispatch = true;
      check(!update(&p) && attempts == 1 && p.frame_remainder > 0.017 && p.frame_remainder < 0.018, "dispatch failure preserves accumulated remainder");
      fail_dispatch = false;
      check(update(&p) && p.frames == 2, "next update retries normally"); }
    RendererCompositorRD::get_singleton()->delta = 0;
    { Particles p; p.request_process_time = 0.75; p.dirty = true; attempts = 0; fail_on_attempt = 2;
      check(!update(&p, 4) && p.frames == 1 && p.request_process_time == 0.5, "partial request preserves only unprocessed time");
      fail_on_attempt = 0;
      check(update(&p, 4) && p.frames == 3 && p.request_process_time == 0, "request retry does not replay committed steps"); }
    { Particles p; p.request_process_time_residual = 0.75; p.dirty = true; attempts = 0; fail_on_attempt = 2;
      check(!update(&p, 4) && p.frames == 1 && p.request_process_time_residual == 0.5 && p.emitting, "partial residual preserves remainder and restores emitting");
      fail_on_attempt = 0;
      check(update(&p, 4) && p.frames == 3 && !p.emitting && p.request_process_time_residual == 0, "residual retry consumes remainder exactly once"); }
    { Particles p; p.request_process_time = 0.5; p.request_process_time_residual = 0.5; p.dirty = true; attempts = 0; fail_on_attempt = 3;
      check(!update(&p, 4) && p.frames == 2 && p.request_process_time == 0 && p.request_process_time_residual == 0.5 && p.emitting, "residual failure does not replay successful request");
      fail_on_attempt = 0;
      check(update(&p, 4) && p.frames == 4 && !p.emitting, "combined request retry advances only pending residual"); }
    { Particles p; p.dirty = true; attempts = 0; RendererCompositorRD::get_singleton()->delta = 1.0 / 60.0;
      check(update(&p) && p.frames == 1, "ordinary queued request still advances frame delta"); }
    { Particles p; p.frame_remainder = 0.75; attempts = 0; fail_on_attempt = 2;
      RendererCompositorRD::get_singleton()->delta = 0;
      check(!update(&p, 4) && p.frames == 1 && p.frame_remainder == 0.5, "fixed FPS failure preserves only unprocessed remainder");
      fail_on_attempt = 0;
      check(update(&p, 4) && p.frames == 3 && p.frame_remainder == 0, "fixed FPS retry does not replay committed steps"); }
    { Particles p; p.frame_remainder = 0.75; attempts = 0; fail_on_attempt = 2;
      RendererCompositorRD::get_singleton()->delta = 0.25;
      check(!update(&p, 4) && p.frames == 1 && p.frame_remainder > 0.599 && p.frame_remainder < 0.601, "partial fixed step commits tail before retry");
      fail_on_attempt = 0;
      check(update(&p, 4, false) && p.frames == 3 && p.frame_remainder > 0.099 && p.frame_remainder < 0.101, "same-frame retry does not add delta twice");
    }
    { Particles p; p.frame_remainder = 0.9; attempts = 0; fail_on_attempt = 2;
      RendererCompositorRD::get_singleton()->delta = 0.25;
      check(!update(&p, 4) && p.frames == 1 && p.frame_remainder > 0.749 && p.frame_remainder < 0.751, "next-frame case preserves partial tail");
      fail_on_attempt = 0;
      check(update(&p, 4, true) && p.frames == 4 && p.frame_remainder > 0.099 && p.frame_remainder < 0.101, "next-frame retry adds only the new delta"); }
    { Particles p; p.frame_remainder = 0.5; p.retry_pending = true; p.frame_delta_committed = false;
      fail_preflight = true; RendererCompositorRD::get_singleton()->delta = 0.25;
      check(!update(&p, 4) && p.frame_remainder == 0.5, "early resource failure preserves old remainder");
      fail_preflight = false;
      check(update(&p, 4) && p.frames == 2 && p.frame_remainder > 0.099 && p.frame_remainder < 0.101, "early failure recovery adds current frame delta"); }
    { Particles p; p.frame_remainder = 0.5; p.retry_pending = true; p.frame_delta_committed = true; p.frame_delta_committed_frame = current_frame + 1; p.restart_request = true;
      RendererCompositorRD::get_singleton()->delta = 0.25;
      check(update(&p, 4, false) && p.frames == 1 && p.frame_remainder == 0, "restart invalidates stale retry marker"); }
    { Particles p; p.frame_remainder = 0.5; p.retry_pending = true; p.frame_delta_committed = true; p.frame_delta_committed_frame = current_frame;
      RendererCompositorRD::get_singleton()->delta = 0.25; simulate_resource_rebuild = false;
      check(update(&p, 4, false) && p.frames == 2 && p.frame_remainder == 0, "same-frame retry without rebuild preserves committed remainder"); }
    { Particles p; p.frame_remainder = 0.5; p.retry_pending = true; p.frame_delta_committed = true; p.frame_delta_committed_frame = current_frame;
      RendererCompositorRD::get_singleton()->delta = 0.25; simulate_resource_rebuild = true;
      check(update(&p, 4, false) && p.frames == 0 && p.frame_remainder > 0.099 && p.frame_remainder < 0.101, "resource rebuild invalidates retry and consumes new epoch delta");
      simulate_resource_rebuild = false; }
    std::printf("particle process progress harness: %d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
'''
    harness = '#include <initializer_list>\n' + harness
    with tempfile.TemporaryDirectory(prefix='particle-process-progress-') as temp:
        cpp, binary = Path(temp) / 'test.cpp', Path(temp) / 'test'
        cpp.write_text(harness)
        result = 0
        for precision in ([], ['-DREAL_T_IS_DOUBLE']):
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', *precision, str(cpp), '-o', str(binary)], check=True)
            result |= subprocess.run([str(binary)], timeout=10).returncode
        return result


if __name__ == '__main__':
    raise SystemExit(main())
