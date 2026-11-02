#!/usr/bin/env python3
"""Check GLES3 lifetime-order copy paths use checked split arithmetic."""

import argparse
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[5]
SOURCE_REL = "drivers/gles3/storage/particles_storage.cpp"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    source = (
        subprocess.check_output(["git", "show", f"{args.source_ref}:{SOURCE_REL}"], cwd=ROOT, text=True)
        if args.source_ref
        else (ROOT / SOURCE_REL).read_text()
    )
    helper = "bool ParticlesStorage::_particles_get_lifetime_split(uint64_t p_amount, double p_phase, uint32_t &r_split)"
    if helper not in source:
        raise AssertionError("missing GLES3 lifetime split helper")
    helper_body = source[source.index(helper) : source.index("Dependency *ParticlesStorage::particles_get_dependency(", source.index(helper))]
    if "p_amount == 0 || p_amount > UINT32_MAX" not in helper_body:
        raise AssertionError("GLES3 lifetime split helper must reject zero or oversized amount")
    if "!Math::is_finite(p_phase) || p_phase < 0.0" not in helper_body:
        raise AssertionError("GLES3 lifetime split helper must reject invalid phase")
    if "!Math::is_finite(scaled_phase)" not in helper_body:
        raise AssertionError("GLES3 lifetime split helper must reject overflowed product")
    if source.count("_particles_get_lifetime_split(uint64_t(particles->amount)") != 2:
        raise AssertionError("GLES3 lifetime split helper must guard both copy paths")
    print("particle GLES3 lifetime split: 5/5 checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
