#!/usr/bin/env python3
"""Check particle resource replacement paths keep changed signals synchronized."""

import argparse
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]


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


def load(source_ref: str | None, path: str) -> str:
    if source_ref:
        return subprocess.check_output(["git", "show", f"{source_ref}:{path}"], cwd=ROOT, text=True)
    return (ROOT / path).read_text()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()

    checks = [
        (
            "CPUParticles3D mesh",
            "scene/3d/cpu_particles_3d.cpp",
            "void CPUParticles3D::set_mesh(",
            ["disconnect_changed", "connect_changed", "update_configuration_warnings();"],
        ),
        (
            "CPUParticles2D texture",
            "scene/2d/cpu_particles_2d.cpp",
            "void CPUParticles2D::set_texture(",
            ["disconnect_changed", "connect_changed", "_texture_changed"],
        ),
        (
            "GPUParticles2D texture",
            "scene/2d/gpu_particles_2d.cpp",
            "void GPUParticles2D::set_texture(",
            ["disconnect_changed", "connect_changed", "_texture_changed"],
        ),
        (
            "GPUParticles2D process material",
            "scene/2d/gpu_particles_2d.cpp",
            "void GPUParticles2D::set_process_material(",
            ['disconnect("emission_shape_changed"', 'connect("emission_shape_changed"'],
        ),
        (
            "GPUParticles3D skin",
            "scene/3d/gpu_particles_3d.cpp",
            "void GPUParticles3D::set_skin(",
            ["disconnect_changed", "connect_changed", "_skinning_changed"],
        ),
        (
            "GPUParticles3D process material",
            "scene/3d/gpu_particles_3d.cpp",
            "void GPUParticles3D::set_process_material(",
            ['disconnect("emission_shape_changed"', 'connect("emission_shape_changed"', "update_gizmos();"],
        ),
    ]

    passed = 0
    for name, path, signature, needles in checks:
        body = function_body(load(args.source_ref, path), signature)
        if not all(needle in body for needle in needles):
            raise AssertionError(f"{name} must keep replacement/clear signal synchronization")
        passed += 1

    print(f"particle resource sync contract: {passed}/{len(checks)} checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
