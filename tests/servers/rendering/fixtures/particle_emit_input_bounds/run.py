#!/usr/bin/env python3
"""Check scene and storage finite guards for manual particle emission."""

import argparse
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[5]
SOURCES = {
    "scene/2d/gpu_particles_2d.cpp": "void GPUParticles2D::emit_particle(",
    "scene/3d/gpu_particles_3d.cpp": "void GPUParticles3D::emit_particle(",
    "servers/rendering/renderer_rd/storage_rd/particles_storage.cpp": "void ParticlesStorage::particles_emit(",
    "drivers/gles3/storage/particles_storage.cpp": "void ParticlesStorage::particles_emit(",
}


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
    guard = "Particle emission inputs must contain only finite values."
    checks = 0
    for path, signature in SOURCES.items():
        source = subprocess.check_output(["git", "show", f"{args.source_ref}:{path}"], cwd=ROOT, text=True) if args.source_ref else (ROOT / path).read_text()
        body = function_body(source, signature)
        if guard not in body:
            raise AssertionError(f"{path}: missing manual emission finite guard")
        if path.startswith("scene/") and body.index(guard) > body.index("RS::get_singleton()->particles_emit("):
            raise AssertionError(f"{path}: guard must run before RenderingServer submission")
        checks += 1
    print(f"particle emit input bounds: {checks}/{checks} checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
