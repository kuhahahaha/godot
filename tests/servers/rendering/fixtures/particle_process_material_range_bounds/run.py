#!/usr/bin/env python3
"""Check ParticleProcessMaterial range setters match their property contracts."""

import argparse
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[5]
SOURCE_PATH = "scene/resources/particle_process_material.cpp"


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace : index + 1]
    raise ValueError(signature)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    source = (
        subprocess.check_output(["git", "show", f"{args.source_ref}:{SOURCE_PATH}"], cwd=ROOT, text=True)
        if args.source_ref
        else (ROOT / SOURCE_PATH).read_text()
    )
    checks = {
        "param": "!Math::is_finite(p_value.x) || !Math::is_finite(p_value.y)",
        "emission_point_count": "p_count > 1000000",
        "turbulence_noise_strength": "p_turbulence_noise_strength > 20.0",
        "turbulence_noise_speed_random": "p_turbulence_noise_speed_random > 4.0",
        "sub_emitter_frequency": "p_frequency > 100.0",
        "sub_emitter_amount_at_end": "p_amount > 32",
        "sub_emitter_amount_at_collision": "p_amount > 32",
        "sub_emitter_amount_at_start": "p_amount > 32",
    }
    for name, needle in checks.items():
        body = function_body(source, f"void ParticleProcessMaterial::set_{name}(")
        if needle not in body:
            raise AssertionError(f"{name} must enforce its declared upper bound")
    point_count = function_body(source, "void ParticleProcessMaterial::set_emission_point_count(")
    if "p_count < 1" not in point_count:
        raise AssertionError("point emission must reject zero before generating a negative shader texel index")
    if '"1,1000000,1"' not in source:
        raise AssertionError("point emission property range must match the runtime lower bound")
    print(f"particle process material range bounds: {len(checks) + 2}/{len(checks) + 2} checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
