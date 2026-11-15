#!/usr/bin/env python3
"""Check that GPU 2D texture scaling cannot commit an invalid collision radius."""

import argparse
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]
SOURCE = "scene/2d/gpu_particles_2d.cpp"


def function_body(source: str, signature: str) -> str:
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
    source = (
        subprocess.check_output(["git", "show", f"{args.source_ref}:{SOURCE}"], cwd=ROOT, text=True)
        if args.source_ref
        else (ROOT / SOURCE).read_text()
    )
    image_source = (
        subprocess.check_output(["git", "show", f"{args.source_ref}:scene/resources/image_texture.cpp"], cwd=ROOT, text=True)
        if args.source_ref
        else (ROOT / "scene/resources/image_texture.cpp").read_text()
    )
    update = function_body(source, "void GPUParticles2D::_update_collision_size()")
    setter = function_body(source, "void GPUParticles2D::set_collision_base_size(")
    texture = function_body(source, "void GPUParticles2D::set_texture(")
    size_override = function_body(image_source, "void ImageTexture::set_size_override(")
    checks = [
        "float csize" in update,
        "!Math::is_finite(csize)" in update,
        "csize = 0.0f;" in update,
        "!Math::is_finite(scaled_size)" in setter and setter.index("!Math::is_finite(scaled_size)") < setter.index("collision_base_size = p_size;"),
        "!Math::is_finite(scaled_size)" in texture and texture.index("!Math::is_finite(scaled_size)") < texture.index("texture = p_texture;"),
        "emit_changed();" in size_override,
        "particles_set_collision_base_size(particles, csize);" in update,
        "_update_collision_size();" in function_body(source, "void GPUParticles2D::_texture_changed()"),
    ]
    passed = sum(checks)
    print(f"particle collision texture scaling: {passed}/{len(checks)} checks passed")
    return 0 if passed == len(checks) else 1


if __name__ == "__main__":
    raise SystemExit(main())
