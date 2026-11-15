#!/usr/bin/env python3
"""Check particle fixed-FPS input bounds at each scene and renderer entry."""

import argparse
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]
TARGETS = {
    "scene/2d/cpu_particles_2d.cpp": ("void CPUParticles2D::set_fixed_fps(", "p_count < 0 || p_count > 1000"),
    "scene/3d/cpu_particles_3d.cpp": ("void CPUParticles3D::set_fixed_fps(", "p_count < 0 || p_count > 1000"),
    "scene/2d/gpu_particles_2d.cpp": ("void GPUParticles2D::set_fixed_fps(", "p_count < 0 || p_count > 1000"),
    "scene/3d/gpu_particles_3d.cpp": ("void GPUParticles3D::set_fixed_fps(", "p_count < 0 || p_count > 1000"),
    "servers/rendering/renderer_rd/storage_rd/particles_storage.cpp": ("bool ParticlesStorage::_particles_fixed_fps_is_valid(", "p_fps >= 0 && p_fps <= 1000"),
    "drivers/gles3/storage/particles_storage.cpp": ("void ParticlesStorage::particles_set_fixed_fps(", "p_fps < 0 || p_fps > 1000"),
}


def body(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        depth += (source[index] == "{") - (source[index] == "}")
        if depth == 0:
            return source[opening : index + 1]
    raise ValueError(signature)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    passed = 0
    for path, (signature, condition) in TARGETS.items():
        source = (
            subprocess.check_output(["git", "show", f"{args.source_ref}:{path}"], cwd=ROOT, text=True)
            if args.source_ref
            else (ROOT / path).read_text()
        )
        if signature in source and condition in body(source, signature):
            passed += 1
        else:
            print(f"FAIL: {path} lacks fixed FPS upper bound")
    print(f"particle fixed FPS bounds: {passed}/{len(TARGETS)} checks passed")
    return 0 if passed == len(TARGETS) else 1


if __name__ == "__main__":
    raise SystemExit(main())
