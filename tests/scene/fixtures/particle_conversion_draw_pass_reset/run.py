#!/usr/bin/env python3
"""Check CPU-to-GPU 3D conversion removes obsolete draw passes."""
import argparse
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
PATH = "scene/3d/gpu_particles_3d.cpp"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    source = subprocess.check_output(["git", "show", f"{args.source_ref}:{PATH}"], cwd=ROOT, text=True) if args.source_ref else (ROOT / PATH).read_text()
    start = source.index("void GPUParticles3D::convert_from_particles(Node *p_particles) {")
    body = source[start:source.index("\n}", start)]
    reset = "set_draw_passes(1);"
    mesh = "set_draw_pass_mesh(0, cpu_particles->get_mesh());"
    assert reset in body and mesh in body, "conversion must reset pass count and copy source mesh"
    assert body.index(reset) < body.index(mesh), "pass count must reset before mesh assignment"
    print("particle conversion draw-pass reset: 2/2 source checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
