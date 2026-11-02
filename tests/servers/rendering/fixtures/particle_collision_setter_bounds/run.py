#!/usr/bin/env python3
"""Check collision storage setter guards against a source revision."""

import argparse
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[5]
SOURCES = [
    "servers/rendering/renderer_rd/storage_rd/particles_storage.cpp",
    "drivers/gles3/storage/particles_storage.cpp",
]


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
    signatures = {
        "type": "void ParticlesStorage::particles_collision_set_collision_type(",
        "sphere": "void ParticlesStorage::particles_collision_set_sphere_radius(",
        "box": "void ParticlesStorage::particles_collision_set_box_extents(",
        "strength": "void ParticlesStorage::particles_collision_set_attractor_strength(",
        "directionality": "void ParticlesStorage::particles_collision_set_attractor_directionality(",
        "attenuation": "void ParticlesStorage::particles_collision_set_attractor_attenuation(",
        "transform": "void ParticlesStorage::particles_collision_instance_set_transform(",
    }
    expected = {
        "type": "ERR_FAIL_INDEX(int(p_type), RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE + 1)",
        "sphere": "p_radius < 0.0",
        "box": "p_extents.x < 0.0",
        "strength": "Math::is_finite(p_strength)",
        "directionality": "p_directionality < 0.0 || p_directionality > 1.0",
        "attenuation": "p_curve < 0.0 || p_curve > 8.0",
        "transform": "p_transform.is_finite()",
    }
    checks = 0
    for path in SOURCES:
        source = subprocess.check_output(["git", "show", f"{args.source_ref}:{path}"], cwd=ROOT, text=True) if args.source_ref else (ROOT / path).read_text()
        for name, signature in signatures.items():
            body = function_body(source, signature)
            if expected[name] not in body:
                raise AssertionError(f"{path}: missing finite guard for {name}")
            checks += 1
    print(f"particle collision setter bounds: {checks}/{checks} checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
