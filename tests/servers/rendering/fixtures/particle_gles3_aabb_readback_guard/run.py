#!/usr/bin/env python3
"""Check GLES3 AABB/current-data readback rejects unallocated buffers."""

import argparse
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[5]
SOURCE_REL = "drivers/gles3/storage/particles_storage.cpp"


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
    source = (
        subprocess.check_output(["git", "show", f"{args.source_ref}:{SOURCE_REL}"], cwd=ROOT, text=True)
        if args.source_ref
        else (ROOT / SOURCE_REL).read_text()
    )
    aabb = function_body(source, "AABB ParticlesStorage::particles_get_current_aabb(")
    data = function_body(source, "Vector<uint8_t> ParticlesStorage::particles_get_current_data(")
    if "particles->back_instance_buffer == 0 && particles->sort_buffer == 0" not in aabb:
        raise AssertionError("GLES3 AABB readback must reject unallocated instance buffers")
    if "if (read_buffer == 0)" not in data:
        raise AssertionError("GLES3 current-data readback must reject an unallocated read buffer")
    if aabb.count("return AABB();") < 2 or "if (read_buffer == 0)" not in aabb or data.count("return Vector<uint8_t>();") < 2:
        raise AssertionError("GLES3 readback guards must return empty values")
    if "is_2d ? sizeof(ParticleInstanceData2D) : sizeof(ParticleInstanceData3D)" not in aabb:
        raise AssertionError("GLES3 AABB readback must size 2D and 3D buffers by mode")
    if "if (is_2d)" not in aabb or "ParticleInstanceData2D &particle_data" not in aabb:
        raise AssertionError("GLES3 AABB readback must parse the 2D instance layout")
    if "ParticleInstanceData3D &particle_data" not in aabb:
        raise AssertionError("GLES3 AABB readback must retain the 3D instance layout")
    print("particle GLES3 AABB readback guard: 7/7 checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
