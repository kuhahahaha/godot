#!/usr/bin/env python3
"""Check GPUParticles2D physics emitter velocity with a zero interval."""

import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]


def extract(source):
    marker = "case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {"
    start = source.index(marker) + len(marker)
    depth, end = 1, start
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start : end - 1]


def main():
    source = (ROOT / "scene/2d/gpu_particles_2d.cpp").read_text()
    body = extract(source)
    harness = (
        r"""
#include <cmath>
#include <cstdio>
struct Vector2 { double x=0,y=0; Vector2 operator-(Vector2 o) const { return {x-o.x,y-o.y}; } };
struct Vector3 { double x=0,y=0,z=0; Vector3() = default; Vector3(double a,double b,double c):x(a),y(b),z(c){} Vector3 operator/(double d) const { return {x/d,y/d,z/d}; } bool operator!=(Vector3 o) const { return x!=o.x || y!=o.y || z!=o.z; } };
struct RS { Vector3 velocity{}; int writes=0; static RS *get_singleton(){ static RS v; return &v; } void particles_set_emitter_velocity(int,Vector3 v){ velocity=v; writes++; } };
struct P {
 int particles=1; Vector2 position{}, previous_position{}; Vector3 previous_velocity{}; double physics_delta=1.0/30.0;
 Vector2 get_global_position(){return position;} double get_physics_process_delta_time(){return physics_delta;}
 void physics_process(){
"""
        + body
        + r"""
 }
};
int main(){ P p; auto *rs=RS::get_singleton(); p.position={2,4}; p.physics_process(); if (std::abs(rs->velocity.x-60)>1e-8 || std::abs(rs->velocity.y-120)>1e-8) return 1; p.position={9,9}; p.physics_delta=0; p.physics_process(); if (rs->velocity.x!=0 || rs->velocity.y!=0) return 2; std::puts("particle emitter velocity 2D: 2/2 checks passed"); }
"""
    )
    with tempfile.TemporaryDirectory(prefix="particle-emitter-2d-") as temp:
        cpp, binary = Path(temp) / "test.cpp", Path(temp) / "test"
        cpp.write_text(harness)
        subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", str(cpp), "-o", str(binary)], check=True)
        return subprocess.run([str(binary)], timeout=10).returncode


if __name__ == "__main__":
    raise SystemExit(main())
