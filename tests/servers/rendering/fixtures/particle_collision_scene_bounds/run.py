#!/usr/bin/env python3
"""Check bounded particle collision scene setters against a source revision."""

import argparse
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[5]
SOURCE_PATH = "scene/3d/gpu_particles_collision_3d.cpp"


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
    source = subprocess.check_output(["git", "show", f"{args.source_ref}:{SOURCE_PATH}"], cwd=ROOT, text=True) if args.source_ref else (ROOT / SOURCE_PATH).read_text()
    checks = {
        "collision sphere radius": (
            "void GPUParticlesCollisionSphere3D::set_radius(",
            "p_radius < 0.0",
        ),
        "collision box size": (
            "void GPUParticlesCollisionBox3D::set_size(",
            "p_size.x < 0.0",
        ),
        "collision SDF thickness": (
            "void GPUParticlesCollisionSDF3D::set_thickness(",
            "p_thickness < 0.0",
        ),
        "collision SDF thickness upper bound": (
            "void GPUParticlesCollisionSDF3D::set_thickness(",
            "p_thickness > 2.0",
        ),
        "collision SDF size": (
            "void GPUParticlesCollisionSDF3D::set_size(",
            "p_size.y < 0.0",
        ),
        "collision heightfield size": (
            "void GPUParticlesCollisionHeightField3D::set_size(",
            "p_size.z < 0.0",
        ),
        "attractor sphere radius": (
            "void GPUParticlesAttractorSphere3D::set_radius(",
            "p_radius < 0.0",
        ),
        "attractor box size": (
            "void GPUParticlesAttractorBox3D::set_size(",
            "p_size.x < 0.0",
        ),
        "attractor vector field size": (
            "void GPUParticlesAttractorVectorField3D::set_size(",
            "p_size.y < 0.0",
        ),
    }
    for name, (signature, needle) in checks.items():
        if needle not in function_body(source, signature):
            raise AssertionError(f"missing non-negative guard for {name}")
    print(f"particle collision scene bounds: {len(checks)}/{len(checks)} checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
