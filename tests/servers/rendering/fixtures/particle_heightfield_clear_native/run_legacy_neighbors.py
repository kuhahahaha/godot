#!/usr/bin/env python3
"""Check six ordinary particle collision/attractor controls on real Metal.

Runs each source separately on Mobile and Forward+, for twelve GPU cases.
Requires tests=yes module_particles_runtime_tests_enabled=yes. These legacy
controls have no external Scene connection and do not certify full H25,
Scene sampling or mixed-source behavior.
"""

import argparse
import json
import math
import os
import re
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
PREFIX = "HEIGHTFIELD_CLEAR_NATIVE_PASS "
CASES = {
    "sphere": {"normal": (0.0, 1.0, 0.0), "depth": 0.25, "position": (0.0, 1.25, 0.0), "textures_used": 0},
    "box": {"normal": (1.0, 0.0, 0.0), "depth": 0.125, "position": (1.25, 0.0, 0.0), "textures_used": 0},
    "sdf": {"normal": (1.0, 0.0, 0.0), "depth": 0.125, "position": (0.25, 0.0, 0.0), "textures_used": 1},
    "sphere_attractor": {
        "force": (-1.0, 0.0, 0.0),
        "velocity": (-1.0 / 60.0, 0.0, 0.0),
        "position": (1.0, 0.0, 0.0),
        "textures_used": 0,
    },
    "box_attractor": {
        "force": (-1.0, 0.0, 0.0),
        "velocity": (-1.0 / 60.0, 0.0, 0.0),
        "position": (1.0, 0.0, 0.0),
        "textures_used": 0,
    },
    "vector_attractor": {
        "force": (-2.0, 0.0, 0.0),
        "velocity": (-2.0 / 60.0, 0.0, 0.0),
        "position": (0.0, 0.0, 0.0),
        "textures_used": 1,
    },
}


def close_number(value, expected):
    return (
        type(value) in (int, float)
        and math.isfinite(value)
        and math.isclose(value, expected, rel_tol=0.0, abs_tol=2**-12)
    )


def close_vector(value, expected):
    return (
        isinstance(value, list)
        and len(value) == 3
        and all(close_number(component, wanted) for component, wanted in zip(value, expected))
    )


def exact_integer(sample, name, expected):
    return type(sample.get(name)) is int and sample[name] == expected


def valid_sample(sample, case):
    expected = CASES[case]
    collision = "depth" in expected
    if not (
        isinstance(sample, dict)
        and sample.get("pass") is True
        and sample.get("neighbor_case") == case
        and sample.get("active") is True
        and exact_integer(sample, "frame_counter", 1)
        and exact_integer(sample, "collider_count", int(collision))
        and exact_integer(sample, "attractor_count", int(not collision))
        and exact_integer(sample, "textures_used", expected["textures_used"])
        and isinstance(sample.get("driver"), str)
        and bool(sample["driver"])
    ):
        return False
    if collision:
        # Fixed H25 inputs: sphere surface center distance 1 and radius 1;
        # box exterior distance .125; signed SDF x=.125. Particle radius .25.
        return (
            sample.get("collided") is True
            and close_vector(sample.get("normal"), expected["normal"])
            and close_number(sample.get("depth"), expected["depth"])
            and close_vector(sample.get("position"), expected["position"])
        )
    # Sphere/box falloff at distance 1 with radius/extent 2 gives half of
    # strength 2 toward the origin. The constant vector gives force -2X.
    # Integrating for 1/60s changes velocity, while position stays at start.
    return (
        close_vector(sample.get("force"), expected["force"])
        and close_vector(sample.get("velocity"), expected["velocity"])
        and close_vector(sample.get("position"), expected["position"])
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--display-driver", default="macos")
    args = parser.parse_args()
    for case in CASES:
        for method, renderer in (("mobile", "Forward Mobile"), ("forward_plus", "Forward+")):
            label = f"{renderer}/{case}"
            command = [
                str(args.binary.resolve()),
                "--display-driver",
                args.display_driver,
                "--rendering-driver",
                "metal",
                "--rendering-method",
                method,
                "--path",
                str(HERE),
                "--script",
                "res://run.gd",
            ]
            env = os.environ.copy()
            for name in (
                "ANGE_HEIGHTFIELD_LEGACY_NORMAL",
                "ANGE_HEIGHTFIELD_GEOMETRY",
                "ANGE_HEIGHTFIELD_GEOMETRY_HEIGHT",
                "ANGE_HEIGHTFIELD_TRANSFORM_CASE",
                "ANGE_HEIGHTFIELD_EXTENTS_Y",
                "ANGE_HEIGHTFIELD_TRANSFORM_Y",
            ):
                env.pop(name, None)
            env["ANGE_HEIGHTFIELD_LEGACY_NEIGHBOR_CASE"] = case
            print(f"HEIGHTFIELD_LEGACY_NEIGHBOR_CASE renderer={method} source={case}", flush=True)
            print("COMMAND " + json.dumps(command), flush=True)
            result = subprocess.run(
                command,
                env=env,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                timeout=90,
            )
            print(result.stdout, flush=True)
            if result.returncode != 0:
                raise SystemExit(f"Legacy neighbor exited {result.returncode} on {label}")
            if "ERROR:" in result.stdout or "WARNING:" in result.stdout:
                raise SystemExit(f"Legacy neighbor emitted an error or warning on {label}")
            startup = re.findall(
                rf"^Metal [0-9.]+ - {re.escape(renderer)} - Using Device #[0-9]+: .+$",
                result.stdout,
                re.MULTILINE,
            )
            if len(startup) != 1:
                raise SystemExit(f"Missing real Metal/{renderer} renderer evidence")
            samples = [
                json.loads(line[len(PREFIX) :]) for line in result.stdout.splitlines() if line.startswith(PREFIX)
            ]
            if len(samples) != 1 or not valid_sample(samples[0], case):
                raise SystemExit(f"Legacy neighbor oracle failed on {label}")
    print("HEIGHTFIELD_LEGACY_NEIGHBORS_MATRIX_PASS 12 real Metal GPU cases")


if __name__ == "__main__":
    main()
