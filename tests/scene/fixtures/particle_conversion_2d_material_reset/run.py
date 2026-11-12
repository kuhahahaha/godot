#!/usr/bin/env python3
"""Check repeated 2D CPU/GPU conversion clears removed CanvasItem material."""
import argparse
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
SOURCES = {
    "scene/2d/cpu_particles_2d.cpp": "CPUParticles2D",
    "scene/2d/gpu_particles_2d.cpp": "GPUParticles2D",
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    for path, name in SOURCES.items():
        source = subprocess.check_output(["git", "show", f"{args.source_ref}:{path}"], cwd=ROOT, text=True) if args.source_ref else (ROOT / path).read_text()
        start = source.index(f"void {name}::convert_from_particles(Node *p_particles) {{")
        body = source[start:source.index("\n}", start)]
        assert "set_material(mat);" in body, f"{path}: conversion does not set material"
        assert "if (mat.is_valid())" not in body, f"{path}: removed source material leaves stale target material"
    print("particle 2D conversion material reset: 2/2 source checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
