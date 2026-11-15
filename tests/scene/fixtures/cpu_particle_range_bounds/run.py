#!/usr/bin/env python3
"""Check CPU particle setters enforce the declared scalar and geometry ranges."""

import argparse
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]


CONTRACTS = {
    "scene/2d/cpu_particles_2d.cpp": {
        "set_spread": ("p_spread < 0.0f", "p_spread > 180.0f"),
        "set_emission_sphere_radius": ("p_radius < 0.0f",),
        "set_emission_rect_extents": ("p_extents.x < 0.0f", "p_extents.y < 0.0f"),
        "set_emission_ring_inner_radius": ("p_inner_radius < 0.0f",),
        "set_emission_ring_radius": ("p_ring_radius < 0.0f",),
    },
    "scene/3d/cpu_particles_3d.cpp": {
        "set_spread": ("p_spread < 0.0f", "p_spread > 180.0f"),
        "set_flatness": ("p_flatness < 0.0f", "p_flatness > 1.0f"),
        "set_emission_sphere_radius": ("p_radius < 0.0f",),
        "set_emission_box_extents": ("p_extents.x < 0.0f", "p_extents.y < 0.0f", "p_extents.z < 0.0f"),
        "set_emission_ring_height": ("p_height < 0.0f",),
        "set_emission_ring_radius": ("p_radius < 0.0f",),
        "set_emission_ring_inner_radius": ("p_radius < 0.0f",),
        "set_emission_ring_cone_angle": ("p_angle < 0.0f", "p_angle > 90.0f"),
    },
}


def load_source(path: str, source_ref: str | None) -> str:
    if source_ref:
        return subprocess.check_output(["git", "show", f"{source_ref}:{path}"], cwd=ROOT, text=True)
    return (ROOT / path).read_text()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    checks = 0
    failures = []
    for path, contracts in CONTRACTS.items():
        source = load_source(path, args.source_ref)
        for function, tokens in contracts.items():
            match = re.search(rf"void (?:CPUParticles2D|CPUParticles3D)::{function}[^{{]*\{{(?P<body>.*?)\n\}}", source, re.S)
            body = match.group("body") if match else ""
            for token in tokens:
                checks += 1
                if token not in body:
                    failures.append(f"{path}:{function}: missing {token}")
    if failures:
        for failure in failures:
            print("FAIL:", failure)
        print(f"CPU particle range guards: {checks - len(failures)}/{checks} checks passed")
        return 1
    print(f"CPU particle range guards: {checks}/{checks} checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
