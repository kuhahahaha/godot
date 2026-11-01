#!/usr/bin/env python3
"""Exercise production 3D particle notifications across render and physics ticks."""

import argparse
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]


def case_body(source, name):
    start = source.index("case " + name + ": {")
    start = source.index("{", start) + 1
    depth = 1
    end = start
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start : end - 1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    source_path = "scene/3d/gpu_particles_3d.cpp"
    source = (
        subprocess.check_output(["git", "show", args.source_ref + ":" + source_path], cwd=ROOT, text=True)
        if args.source_ref
        else (ROOT / source_path).read_text()
    )
    # Motion along one axis is sufficient to detect competing position histories.
    harness = (
        r"""
#include <cmath>
#include <cstdio>
using Vector3 = double;
#define SceneStringName(name) #name
struct RS {
    double velocity = 0;
    int writes = 0;
    static RS *get_singleton() { static RS value; return &value; }
    void particles_set_emitter_velocity(int, double value) { velocity = value; writes++; }
};
struct Particle {
    int particles = 1;
    double position = 0, previous_position = 0, previous_velocity = 0;
    double process_delta = 1.0 / 120.0, physics_delta = 1.0 / 30.0;
    bool one_shot = false, active = false, emitting = true, signal_canceled = false;
    bool processing = true;
    double time = 0, emission_time = 0, active_time = 0;
    int finished = 0;
    double get_global_position() { return position; }
    double get_process_delta_time() { return process_delta; }
    double get_physics_process_delta_time() { return physics_delta; }
    void set_process_internal(bool value) { processing = value; }
    void emit_signal(const char *) { finished++; }
    void process() {
"""
        + case_body(source, "NOTIFICATION_INTERNAL_PROCESS")
        + r"""
    }
    void physics_process() {
"""
        + case_body(source, "NOTIFICATION_INTERNAL_PHYSICS_PROCESS")
        + r"""
    }
};
int main() {
    int failures = 0, checks = 0;
    auto check = [&](bool ok, const char *name) { checks++; if (!ok) { failures++; std::fprintf(stderr, "FAIL: %s\n", name); } };
    auto eq = [](double a, double b) { return std::abs(a - b) < 1e-8; };
    Particle p;
    RS *server = RS::get_singleton();
    p.position = 2;
    p.physics_process();
    check(eq(server->velocity, 60), "physics movement supplies emitter velocity");
    const int writes = server->writes;
    for (int i = 0; i < 4; i++) p.process();
    check(eq(server->velocity, 60), "render ticks preserve physics velocity");
    check(server->writes == writes, "render ticks do not overwrite the emitter sample");
    p.position = 3;
    p.process();
    check(eq(p.previous_position, 2), "render movement does not consume physics position history");
    p.position = 4;
    p.physics_process();
    check(eq(server->velocity, 60), "next physics tick uses the whole physics interval");
    p.physics_process();
    check(eq(server->velocity, 0), "stationary physics tick resets velocity");
    p.position = 8;
    p.physics_delta = 0;
    p.physics_process();
    check(eq(server->velocity, 0), "zero physics interval produces zero velocity");
    p.one_shot = true; p.active = true;
    p.emission_time = 0.01; p.active_time = 0.02; p.process_delta = 0.03;
    p.process();
    check(!p.emitting && !p.active && p.finished == 1 && !p.processing,
            "render ticks still finish a one-shot particle cycle");
    std::printf("particle emitter velocity: %d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
"""
    )
    with tempfile.TemporaryDirectory(prefix="particle-emitter-velocity-") as temp:
        cpp, binary = Path(temp) / "test.cpp", Path(temp) / "test"
        cpp.write_text(harness)
        subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", str(cpp), "-o", str(binary)], check=True)
        return subprocess.run([str(binary)], timeout=10).returncode


if __name__ == "__main__":
    raise SystemExit(main())
