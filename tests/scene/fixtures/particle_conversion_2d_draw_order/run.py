#!/usr/bin/env python3
"""Check reverse-lifetime 2D GPU draw order maps to CPU lifetime semantics."""
import argparse
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
PATH = "scene/2d/cpu_particles_2d.cpp"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    source = subprocess.check_output(["git", "show", f"{args.source_ref}:{PATH}"], cwd=ROOT, text=True) if args.source_ref else (ROOT / PATH).read_text()
    marker = "void CPUParticles2D::convert_from_particles(Node *p_particles) {"
    start = source.index(marker)
    end = source.index("\n}", start)
    body = source[start:end]
    mapping = "set_draw_order(gpu_particles->get_draw_order() == GPUParticles2D::DRAW_ORDER_INDEX ? DRAW_ORDER_INDEX : DRAW_ORDER_LIFETIME);"
    assert mapping in body, "reverse lifetime must map to supported CPU lifetime order"
    print("particle 2D conversion draw order: 1/1 source check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
