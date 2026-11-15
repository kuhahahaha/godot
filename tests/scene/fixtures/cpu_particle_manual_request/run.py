#!/usr/bin/env python3
"""Extract production update control flow; simulation recording does not synthesize active.

This harness checks admission and scheduling, not real particle birth/death or rendering.
A separate native probe must validate particle lifecycle behavior.
"""

import argparse
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]


def body(source, name):
    start = source.index("void " + name + "::_update_internal() {")
    start = source.index("{", start) + 1
    end, depth = start, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start : end - 1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    parser.add_argument("--source-root", type=Path, default=ROOT)
    args = parser.parse_args()
    source_root = args.source_root
    prefix = r"""
#include <cmath>
#include <cstdio>
#ifdef REAL_T_IS_DOUBLE
using real_t = double;
#else
using real_t = float;
#endif
namespace Math { template <typename T> bool is_finite(T value) { return std::isfinite(value); } }
#define ERR_FAIL_COND_MSG(condition, message) do { if (condition) { std::fprintf(stderr, "%s\n", message); return; } } while (false)
struct Particle {
    struct { bool is_empty() const { return false; } } particles;
    bool active = false, emitting = false, processing = false, redraw = false;
    double time = 0, frame_remainder = 0, pre_process_time = 0, speed_scale = 0;
    real_t _request_process_time = 0, _request_process_time_residual = 0;
    double emitted = 0, residual = 0;
    int cycle = 0, fixed_fps = 30, uploads = 0, interpolation_refreshes = 0;
    double delta = 0;
    bool is_visible_in_tree() { return true; }
    void _refresh_interpolation_state() { interpolation_refreshes++; }
    double get_process_delta_time() { return delta; }
    void set_process_internal(bool v) { processing = v; }
    void _set_do_redraw(bool v) { redraw = v; }
    void _set_redraw(bool v) { redraw = v; }
    void _update_particle_data_buffer() { uploads++; }
    void _particles_process(double dt) {
        dt *= speed_scale; time += dt;
        // Production _particles_process never sets node active=true. In these
        // short intervals (0.75 < lifetime 2), the real tail does not clear it.
        if (emitting) { emitted += dt; } else { residual += dt; }
    }
    void update() {
"""
    suffix = r"""
    }
};
int main() {
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char *name) { checks++; if (!ok) { failures++; std::fprintf(stderr, "FAIL: %s\n", name); } };
    auto eq = [](double a, double b) { return std::abs(a - b) < 1e-6; };
    Particle idle; idle.update();
    check(idle.uploads == 0 && !idle.redraw, "ordinary idle update stays idle");
    Particle p; p._request_process_time = 0.5; p.update();
    check(eq(p.emitted, 0.5) && p.emitting, "idle emission request executes");
    check(p.uploads == 1, "manual simulation updates particle data");
    check(p.speed_scale == 0 && p._request_process_time == 0, "speed scale restored and request consumed");
    p.update(); check(eq(p.emitted, 0.5), "consumed request is not replayed");
    Particle both; both._request_process_time = 0.5; both._request_process_time_residual = 0.25; both.update();
    check(eq(both.emitted, 0.5) && eq(both.residual, 0.25) && !both.emitting, "combined request emits then trails");
    check(both.active && both.processing, "manual emission restores activity and scheduling");
    both.delta = 1.0 / 30.0; both.speed_scale = 1;
    const double before_tail = both.residual;
    if (both.processing) { both.update(); }
    check(both.residual > before_tail && both.redraw, "scheduled next frame advances residual without hiding");
    check(p.active && p.processing, "emission-only request restores scheduling");
    Particle tail; tail._request_process_time_residual = 0.25; tail.update();
    check(eq(tail.residual, 0.25) && tail._request_process_time_residual == 0, "idle residual request executes and is consumed");
    check(!tail.active && !tail.processing, "residual-only does not revive an idle system");
    Particle unprocessable; unprocessable.speed_scale = 2; unprocessable._request_process_time = 16777216.0; unprocessable.update();
    check(unprocessable.emitted == 0 && unprocessable._request_process_time == 16777216.0, "unrepresentable request is rejected before partial simulation");
    check(unprocessable.speed_scale == 2, "rejected request preserves speed scale");
    check(!unprocessable.emitting && unprocessable.time == 0 && unprocessable.uploads == 0, "rejected request preserves simulation state");
    Particle bad_tail; bad_tail.speed_scale = 3; bad_tail._request_process_time = 0.5; bad_tail._request_process_time_residual = 16777216.0; bad_tail.update();
    check(bad_tail.time == 0 && bad_tail.emitted == 0 && bad_tail.residual == 0 && !bad_tail.emitting, "unrepresentable residual rejects before the emission phase");
    check(bad_tail._request_process_time == 0.5 && bad_tail._request_process_time_residual == 16777216.0 && bad_tail.speed_scale == 3, "rejected residual preserves both requests and speed scale");
    Particle bad_preprocess; bad_preprocess.emitting = true; bad_preprocess.pre_process_time = 16777216.0; bad_preprocess.speed_scale = 4; bad_preprocess._request_process_time_residual = 0.25; bad_preprocess.update();
    check(bad_preprocess.time == 0 && bad_preprocess.emitting && bad_preprocess.uploads == 0, "unrepresentable preprocess rejects before any simulation");
    check(bad_preprocess._request_process_time_residual == 0.25 && bad_preprocess.speed_scale == 4, "rejected preprocess preserves request and speed scale");
    check(!unprocessable.redraw && !unprocessable.processing && !unprocessable.active && unprocessable.interpolation_refreshes == 0, "rejected emission preserves draw, processing, activity and interpolation");
    check(!bad_tail.redraw && !bad_tail.processing && !bad_tail.active && bad_tail.interpolation_refreshes == 0, "rejected residual preserves draw, processing, activity and interpolation");
    check(!bad_preprocess.redraw && !bad_preprocess.processing && !bad_preprocess.active && bad_preprocess.interpolation_refreshes == 0, "rejected preprocess preserves draw, processing, activity and interpolation");
    Particle existing; existing.active = true; existing.emitting = true; existing.redraw = true; existing.processing = true;
    existing.time = 0.25; existing.frame_remainder = 0.01; existing.cycle = 3; existing._request_process_time_residual = 16777216.0; existing.update();
    check(existing.active && existing.emitting && existing.redraw && existing.processing && existing.time == 0.25 && existing.frame_remainder == 0.01 && existing.cycle == 3 && existing.interpolation_refreshes == 0, "rejected request preserves already enabled flags and progress");
    std::printf("CPU manual request: %d/%d checks passed\n", checks-failures, checks);
    return failures ? 1 : 0;
}
"""
    failed = False
    for dimension in ("2d", "3d"):
        path = f"scene/{dimension}/cpu_particles_{dimension}.cpp"
        source = (
            subprocess.check_output(["git", "show", args.source_ref + ":" + path], cwd=source_root, text=True)
            if args.source_ref
            else (source_root / path).read_text()
        )
        with tempfile.TemporaryDirectory(prefix="cpu-manual-request-") as temp:
            cpp, exe = Path(temp) / "test.cpp", Path(temp) / "test"
            cpp.write_text(prefix + body(source, "CPUParticles" + dimension.upper()) + suffix)
            for precision in ("float", "double"):
                flags = ["-DREAL_T_IS_DOUBLE"] if precision == "double" else []
                subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", *flags, str(cpp), "-o", str(exe)], check=True)
                print(f"{dimension}, real_t={precision}:", flush=True)
                failed |= subprocess.run([str(exe)], timeout=10).returncode != 0
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
