#!/usr/bin/env python3
"""Zero GPU amount ratio must reject every restart before particle activation."""
from pathlib import Path

root = Path(__file__).resolve().parents[4]
source = (root / "scene/resources/particle_process_material.cpp").read_text()
assert 'if (AMOUNT_RATIO <= 0.0 || rand_from_seed(alt_seed) > AMOUNT_RATIO)' in source
print("zero amount ratio shader branch present")
