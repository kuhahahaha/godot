#!/usr/bin/env python3
"""Check that animation material detection accumulates across mesh surfaces."""

import argparse
import subprocess
from pathlib import Path


root = Path(__file__).resolve().parents[4]
path = "scene/3d/gpu_particles_3d.cpp"
parser = argparse.ArgumentParser()
parser.add_argument("--source-ref")
args = parser.parse_args()
source = (
    subprocess.check_output(["git", "show", f"{args.source_ref}:{path}"], cwd=root, text=True)
    if args.source_ref
    else (root / path).read_text()
)
start = source.index("PackedStringArray GPUParticles3D::get_configuration_warnings() const")
end = source.index("void GPUParticles3D::restart(", start)
body = source[start:end]
surface_start = body.index("for (int j = 0; j < draw_passes[i]->get_surface_count(); j++)")
surface_end = body.index("if (anim_material_found) {", surface_start)
surface_body = body[surface_start:surface_end]

checks = [
    "anim_material_found = anim_material_found || Object::cast_to<ShaderMaterial>" in surface_body,
    "anim_material_found = anim_material_found || (spat && spat->get_billboard_mode()" in surface_body,
    "if (anim_material_found) {" in body,
    "if (get_material_override().is_valid()) {" in body,
]
passed = sum(checks)
print(f"particle animation warning surfaces: {passed}/{len(checks)} checks passed")
raise SystemExit(0 if passed == len(checks) else 1)
