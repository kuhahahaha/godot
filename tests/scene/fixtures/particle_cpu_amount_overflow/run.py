#!/usr/bin/env python3
"""Check CPU particle amount arithmetic guards in both scene implementations."""
import argparse
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]
SOURCES = {
    "scene/2d/cpu_particles_2d.cpp": "8 + 4 + 4",
    "scene/3d/cpu_particles_3d.cpp": "12 + 4 + 4",
}


def body(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening : index + 1]
    raise AssertionError(f"unclosed {signature}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    for path, stride in SOURCES.items():
        source = subprocess.check_output(["git", "show", f"{args.source_ref}:{path}"], cwd=ROOT, text=True) if args.source_ref else (ROOT / path).read_text()
        name = "CPUParticles2D" if "/2d/" in path else "CPUParticles3D"
        setter = body(source, f"void {name}::set_amount(int p_amount)")
        guard = f"p_amount > INT_MAX / ({stride})"
        resize = f"particle_data.resize(({stride}) * p_amount)"
        assert guard in setter, f"{path}: missing storage multiplication guard"
        assert resize in setter, f"{path}: expected storage expression changed"
        assert setter.index(guard) < setter.index("particles.resize(p_amount)"), f"{path}: guard runs after state mutation"
    print("CPU particle amount overflow: 2/2 source checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
