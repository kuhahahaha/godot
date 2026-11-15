#!/usr/bin/env python3
"""Check GLES3 heightfield sizing handles degenerate and non-finite extents."""

import argparse
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[5]
SOURCE_PATH = "drivers/gles3/storage/particles_storage.cpp"


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
        subprocess.check_output(["git", "show", f"{args.source_ref}:{SOURCE_PATH}"], cwd=ROOT, text=True)
        if args.source_ref
        else (ROOT / SOURCE_PATH).read_text()
    )
    body = function_body(source, "GLuint ParticlesStorage::particles_collision_get_heightfield_framebuffer(")

    checks = {
        "degenerate extents fallback": "particles_collision->extents.x <= 0.0 || particles_collision->extents.z <= 0.0",
        "non-finite extents fallback": "!Math::is_finite(particles_collision->extents.x)",
        "X-dominant minimum height": "size.y = MAX(1, int32_t(particles_collision->extents.z / particles_collision->extents.x * size.x));",
        "Z-dominant minimum width": "size.x = MAX(1, int32_t(particles_collision->extents.x / particles_collision->extents.z * size.y));",
    }
    for name, needle in checks.items():
        if needle not in body:
            raise AssertionError(f"missing GLES3 heightfield guard: {name}")
    if "size.y = size.x;" not in body:
        raise AssertionError("degenerate GLES3 heightfield extents must use a square resolution")

    extent_setter = function_body(source, "void ParticlesStorage::particles_collision_set_box_extents(")
    if "particles_collision->type == RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE" not in extent_setter:
        raise AssertionError("heightfield extent changes must invalidate their cached framebuffer")
    if "particles_collision->extents.x != p_extents.x || particles_collision->extents.z != p_extents.z" not in extent_setter:
        raise AssertionError("heightfield framebuffer sizing must track X/Z aspect changes")
    if "texture_free_data(particles_collision->heightfield_texture)" not in extent_setter or "glDeleteFramebuffers(1, &particles_collision->heightfield_fb)" not in extent_setter:
        raise AssertionError("changing heightfield X/Z extents must retire both cached resources")

    print(f"particle GLES3 heightfield size: {len(checks) + 4}/{len(checks) + 4} checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
