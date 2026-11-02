#!/usr/bin/env python3
"""Check CPU/GPU 2D and 3D conversion copies deterministic seed state."""
import argparse
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
SOURCES = {
    "scene/2d/cpu_particles_2d.cpp": ("CPUParticles2D", "gpu_particles"),
    "scene/3d/cpu_particles_3d.cpp": ("CPUParticles3D", "gpu_particles"),
    "scene/2d/gpu_particles_2d.cpp": ("GPUParticles2D", "cpu_particles"),
    "scene/3d/gpu_particles_3d.cpp": ("GPUParticles3D", "cpu_particles"),
}


def body(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        depth += (source[index] == "{") - (source[index] == "}")
        if depth == 0:
            return source[opening:index + 1]
    raise AssertionError(signature)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    for path, (name, source_name) in SOURCES.items():
        source = subprocess.check_output(["git", "show", f"{args.source_ref}:{path}"], cwd=ROOT, text=True) if args.source_ref else (ROOT / path).read_text()
        text = body(source, f"void {name}::convert_from_particles(")
        fixed = f"set_use_fixed_seed({source_name}->get_use_fixed_seed());"
        seed = f"set_seed({source_name}->get_seed());"
        assert fixed in text and seed in text, f"{path}: conversion loses fixed seed or seed"
        assert text.index(fixed) < text.index("set_emitting(") and text.index(seed) < text.index("set_emitting("), f"{path}: seed applied after emitting"
    print("particle conversion fixed seed: 4/4 source checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
