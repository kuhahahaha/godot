#!/usr/bin/env python3
"""Check repeated GPU-to-CPU conversion clears removed curves and ramps."""
import argparse
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
SOURCES = {
    "scene/2d/cpu_particles_2d.cpp": ("CPUParticles2D", "proc_mat"),
    "scene/3d/cpu_particles_3d.cpp": ("CPUParticles3D", "material"),
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    for path, (name, material) in SOURCES.items():
        source = subprocess.check_output(["git", "show", f"{args.source_ref}:{path}"], cwd=ROOT, text=True) if args.source_ref else (ROOT / path).read_text()
        start = source.index(f"void {name}::convert_from_particles(Node *p_particles) {{")
        body = source[start:source.index("\n}", start)]
        assert "set_color_ramp(gt.is_valid() ? gt->get_gradient() : Ref<Gradient>());" in body, f"{path}: missing color ramp clear"
        assert "set_color_initial_ramp(gti.is_valid() ? gti->get_gradient() : Ref<Gradient>());" in body, f"{path}: missing initial ramp clear"
        assert "set_param_curve(m_param, ctex.is_valid() ? ctex->get_curve() : Ref<Curve>());" in body, f"{path}: missing parameter curve clear"
        assert f"{material}->get_param_texture" in body
    print("particle conversion resource reset: 6/6 source checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
