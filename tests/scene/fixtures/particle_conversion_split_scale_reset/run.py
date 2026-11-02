#!/usr/bin/env python3
"""Check GPU-to-CPU conversion clears obsolete split-scale state."""
import argparse
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
SOURCES = {
    "scene/2d/cpu_particles_2d.cpp": ("CPUParticles2D", ("x", "y")),
    "scene/3d/cpu_particles_3d.cpp": ("CPUParticles3D", ("x", "y", "z")),
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    checks = 0
    for path, (name, axes) in SOURCES.items():
        source = subprocess.check_output(["git", "show", f"{args.source_ref}:{path}"], cwd=ROOT, text=True) if args.source_ref else (ROOT / path).read_text()
        start = source.index(f"void {name}::convert_from_particles(Node *p_particles) {{")
        body = source[start:source.index("\n}", start)]
        anchor = "Ref<CurveXYZTexture> scale3D ="
        segment = body[body.index(anchor):body.index("if (scale3D.is_valid())", body.index(anchor))]
        assert "split_scale = scale3D.is_valid();" in segment, f"{path}: split mode remains stale"
        checks += 1
        for axis in axes:
            assert f"scale_curve_{axis}.unref();" in segment, f"{path}: axis {axis} curve remains stale"
            checks += 1
    print(f"particle conversion split scale reset: {checks}/{checks} source checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
