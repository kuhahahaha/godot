#!/usr/bin/env python3
"""Check ordinary particle collision against a real flat D32 field on Metal.

Runs identity and three rotated/scaled cases on Mobile and Forward+.
Requires an editor built with tests=yes module_particles_runtime_tests_enabled=yes.
This is a legacy collision control, not Scene sampling acceptance.
"""

import argparse
import json
import math
import os
from pathlib import Path
import re
import subprocess

HERE = Path(__file__).resolve().parent
PREFIX = "HEIGHTFIELD_CLEAR_NATIVE_PASS "
ORACLES = {
    "": ((0.0, 1.0, 0.0), (0.0, 0.1, 0.0)),
    "rotated": ((-1.0, 0.0, 0.0), (9.9, 20.0, 30.0)),
    "rotated_uniform": ((-1.0, 0.0, 0.0), (9.9, 20.0, 30.0)),
    "rotated_nonuniform": ((-1.0, 0.0, 0.0), (9.9, 20.0, 30.0)),
}


def close_number(value, expected):
    return (
        type(value) in (int, float)
        and math.isfinite(value)
        and math.isclose(value, expected, rel_tol=0.0, abs_tol=0.001)
    )


def close_vector(value, expected):
    return (
        isinstance(value, list)
        and len(value) == len(expected)
        and all(close_number(component, wanted) for component, wanted in zip(value, expected))
    )


def valid_sample(sample, expected_normal, expected_position):
    # The world radius is .1 for every case, including scaled fields. A plane
    # through the particle center overlaps by .1 in its outward direction.
    # The fixed world-space oracles do not evaluate the tested transform.
    return (
        isinstance(sample, dict)
        and sample.get("pass") is True
        and sample.get("legacy_normal") is True
        and sample.get("active") is True
        and type(sample.get("frame_counter")) is int
        and sample["frame_counter"] == 1
        and type(sample.get("collider_count")) is int
        and sample["collider_count"] == 1
        and isinstance(sample.get("driver"), str)
        and bool(sample["driver"])
        and close_vector(sample.get("normal"), expected_normal)
        and close_number(sample.get("depth"), 0.1)
        and close_vector(sample.get("position"), expected_position)
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--display-driver", default="macos")
    args = parser.parse_args()
    for transform_case, (normal, position) in ORACLES.items():
        for method, renderer in (("mobile", "Forward Mobile"), ("forward_plus", "Forward+")):
            label = f"{renderer}/{transform_case or 'identity'}"
            command = [
                str(args.binary.resolve()),
                "--display-driver", args.display_driver,
                "--rendering-driver", "metal",
                "--rendering-method", method,
                "--path", str(HERE),
                "--script", "res://run.gd",
            ]
            env = os.environ.copy()
            env.pop("ANGE_HEIGHTFIELD_LEGACY_NEIGHBOR_CASE", None)
            env["ANGE_HEIGHTFIELD_LEGACY_NORMAL"] = "1"
            for name in (
                "ANGE_HEIGHTFIELD_GEOMETRY",
                "ANGE_HEIGHTFIELD_GEOMETRY_HEIGHT",
                "ANGE_HEIGHTFIELD_TRANSFORM_CASE",
                "ANGE_HEIGHTFIELD_EXTENTS_Y",
                "ANGE_HEIGHTFIELD_TRANSFORM_Y",
            ):
                env.pop(name, None)
            if transform_case:
                env["ANGE_HEIGHTFIELD_TRANSFORM_CASE"] = transform_case
            print(f"HEIGHTFIELD_LEGACY_NATIVE_CASE renderer={method} transform_case={transform_case or 'identity'} world_radius=0.1", flush=True)
            print("COMMAND " + json.dumps(command), flush=True)
            result = subprocess.run(
                command, env=env, text=True, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, timeout=90,
            )
            print(result.stdout, flush=True)
            if result.returncode != 0:
                raise SystemExit(f"Legacy native case exited {result.returncode} on {label}")
            if "ERROR:" in result.stdout or "WARNING:" in result.stdout:
                raise SystemExit(f"Legacy native case emitted an error or warning on {label}")
            startup = re.findall(
                rf"^Metal [0-9.]+ - {re.escape(renderer)} - Using Device #[0-9]+: .+$",
                result.stdout, re.MULTILINE,
            )
            if len(startup) != 1:
                raise SystemExit(f"Missing real Metal/{renderer} renderer evidence")
            samples = [
                json.loads(line[len(PREFIX):])
                for line in result.stdout.splitlines()
                if line.startswith(PREFIX)
            ]
            if len(samples) != 1 or not valid_sample(samples[0], normal, position):
                raise SystemExit(f"Legacy particle plane oracle failed on {label}")
    print("HEIGHTFIELD_LEGACY_NATIVE_MATRIX_PASS 8 real Metal GPU cases")


if __name__ == "__main__":
    main()
