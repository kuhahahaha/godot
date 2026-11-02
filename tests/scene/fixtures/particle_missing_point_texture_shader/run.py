#!/usr/bin/env python3
"""Point emission must deactivate while its position texture is absent."""
from pathlib import Path

root = Path(__file__).resolve().parents[4]
source = (root / "scene/resources/particle_process_material.cpp").read_text()
assert 'uniform bool emission_texture_points_valid = false;' in source
assert 'if (!emission_texture_points_valid)' in source
assert 'material_set_param(_get_material(), "emission_texture_points_valid", p_points.is_valid())' in source
print("missing point texture shader guard present")
