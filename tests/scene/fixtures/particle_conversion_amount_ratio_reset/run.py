#!/usr/bin/env python3
"""Check CPU-to-GPU conversion resets unsupported CPU amount ratio."""
import argparse
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
SOURCES = {
    "scene/2d/gpu_particles_2d.cpp": "GPUParticles2D",
    "scene/3d/gpu_particles_3d.cpp": "GPUParticles3D",
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    for path, name in SOURCES.items():
        source = subprocess.check_output(["git", "show", f"{args.source_ref}:{path}"], cwd=ROOT, text=True) if args.source_ref else (ROOT / path).read_text()
        start = source.index(f"void {name}::convert_from_particles(Node *p_particles) {{")
        body = source[start:source.index("\n}", start)]
        assert "set_amount_ratio(1.0f);" in body, f"{path}: target can keep old emission ratio"
        assert body.index("set_amount(cpu_particles->get_amount());") < body.index("set_amount_ratio(1.0f);"), f"{path}: ratio reset must follow amount copy"
    print("particle conversion amount ratio reset: 2/2 source checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
