#!/usr/bin/env python3
"""Sample production D32 heightfields on main RD, on Mobile and Forward+.

Requires a tests=yes module_particles_runtime_tests_enabled=yes editor and a
real display/GPU. This is a legacy numeric regression, not E34 acceptance.
"""

import argparse
import json
import math
import os
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--display-driver", default="x11")
    parser.add_argument("--rendering-driver", default="vulkan")
    args = parser.parse_args()
    observed = {}
    for height, extents_y, transform_y in ((None, 1.0, 0.0), (-0.5, 1.0, 0.0), (0.0, 1.0, 0.0), (0.5, 1.0, 0.0), (0.25, 2.0, -0.5)):
        for method, renderer in (("mobile", "Forward Mobile"), ("forward_plus", "Forward+")):
            command = [
                str(args.binary.resolve()),
                "--display-driver",
                args.display_driver,
                "--rendering-driver",
                args.rendering_driver,
                "--rendering-method",
                method,
                "--path",
                str(HERE),
                "--script",
                "res://run.gd",
            ]
            env = os.environ.copy()
            env["ANGE_HEIGHTFIELD_GEOMETRY"] = "0" if height is None else "1"
            env["ANGE_HEIGHTFIELD_GEOMETRY_HEIGHT"] = str(height or 0.0)
            env.pop("ANGE_HEIGHTFIELD_EXTENTS_Y", None)
            env.pop("ANGE_HEIGHTFIELD_TRANSFORM_Y", None)
            if extents_y != 1.0 or transform_y != 0.0:
                env["ANGE_HEIGHTFIELD_EXTENTS_Y"] = str(extents_y)
                env["ANGE_HEIGHTFIELD_TRANSFORM_Y"] = str(transform_y)
            print(f"HEIGHTFIELD_NATIVE_CASE renderer={method} height={height} extents_y={extents_y} transform_y={transform_y}", flush=True)
            print("COMMAND " + json.dumps(command), flush=True)
            result = subprocess.run(
                command, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90
            )
            print(result.stdout, flush=True)
            if result.returncode != 0:
                raise SystemExit(f"Native case exited {result.returncode}")
            if "ERROR:" in result.stdout or "WARNING:" in result.stdout:
                raise SystemExit("Native capture emitted an error or cleanup warning")
            if f" - {renderer} - " not in result.stdout:
                raise SystemExit(f"Missing real {renderer} renderer evidence")
            samples = [
                json.loads(line.split("HEIGHTFIELD_CLEAR_NATIVE_PASS ", 1)[1])
                for line in result.stdout.splitlines()
                if "HEIGHTFIELD_CLEAR_NATIVE_PASS " in line
            ]
            expected = 0.0 if height is None else (height - transform_y + extents_y) / (2.0 * extents_y)
            if (
                len(samples) != 1
                or not samples[0].get("pass")
                or not samples[0].get("driver")
                or not math.isclose(samples[0].get("first_raw", float("nan")), 0.75, abs_tol=0.001)
                or not math.isclose(samples[0].get("second_raw", float("nan")), expected, abs_tol=0.001)
            ):
                raise SystemExit(f"Missing independent D32 samples on {renderer}, height={height}")
            observed[(height, extents_y, transform_y, method)] = samples[0]["second_raw"]
        if not math.isclose(observed[(height, extents_y, transform_y, "mobile")], observed[(height, extents_y, transform_y, "forward_plus")], abs_tol=0.001):
            raise SystemExit(f"Mobile/Forward+ depth mismatch at height={height}")
    print("HEIGHTFIELD_NATIVE_MATRIX_PASS 10 real GPU cases")


if __name__ == "__main__":
    main()
