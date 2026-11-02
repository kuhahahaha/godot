#!/usr/bin/env python3
"""Check CPU/GPU particle speed-scale setters enforce their non-negative contract."""

import argparse
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]


def load_source(path: str, source_ref: str | None) -> str:
    if source_ref:
        return subprocess.check_output(["git", "show", f"{source_ref}:{path}"], cwd=ROOT, text=True)
    return (ROOT / path).read_text()


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace : index + 1]
    raise ValueError(signature)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    checks = 0
    failures = []
    for path, class_name in {
        "scene/2d/cpu_particles_2d.cpp": "CPUParticles2D",
        "scene/2d/gpu_particles_2d.cpp": "GPUParticles2D",
        "scene/3d/cpu_particles_3d.cpp": "CPUParticles3D",
        "scene/3d/gpu_particles_3d.cpp": "GPUParticles3D",
    }.items():
        body = function_body(load_source(path, args.source_ref), f"void {class_name}::set_speed_scale(")
        checks += 1
        if "p_scale < 0.0" not in body:
            failures.append(f"{path}: missing non-negative speed-scale guard")
    rd_source = load_source("servers/rendering/renderer_rd/storage_rd/particles_storage.cpp", args.source_ref)
    checks += 1
    if "p_scale >= 0.0" not in function_body(rd_source, "bool ParticlesStorage::_particles_speed_scale_is_valid("):
        failures.append("servers/rendering/renderer_rd/storage_rd/particles_storage.cpp: missing non-negative speed-scale guard")
    gles_source = load_source("drivers/gles3/storage/particles_storage.cpp", args.source_ref)
    checks += 1
    if "p_scale < 0.0" not in function_body(gles_source, "void ParticlesStorage::particles_set_speed_scale("):
        failures.append("drivers/gles3/storage/particles_storage.cpp: missing non-negative speed-scale guard")
    if failures:
        for failure in failures:
            print("FAIL:", failure)
        print(f"particle speed scale bounds: {checks - len(failures)}/{checks} checks passed")
        return 1
    print(f"particle speed scale bounds: {checks}/{checks} checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
