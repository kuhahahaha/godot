#!/usr/bin/env python3
"""Exercise H12 source selection through real SceneCull and main-RD raster.

This is native raster qualification only: no Current, managed sample, producer
completion, COW, or E34 acceptance is established.
"""

import argparse
import json
import math
import os
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
CASES = (
    ("cast_shadow_off", 0.5),
    ("alpha_discard", 0.0),
    ("hidden", 0.0),
    ("particles_excluded", 0.0),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--display-driver", default="x11")
    parser.add_argument("--rendering-driver", default="vulkan")
    args = parser.parse_args()
    completed = 0
    for method, renderer in (("mobile", "Forward Mobile"), ("forward_plus", "Forward+")):
        for case_index, (case, expected) in enumerate(CASES):
            command = [str(args.binary.resolve()), "--display-driver", args.display_driver,
                       "--rendering-driver", args.rendering_driver, "--rendering-method", method,
                       "--path", str(HERE), "--script", "res://run.gd"]
            env = {key: value for key, value in os.environ.items()
                   if not key.startswith("ANGE_HEIGHTFIELD_")}
            env["ANGE_HEIGHTFIELD_SOURCE_SELECTION"] = case
            print(f"HEIGHTFIELD_SOURCE_SELECTION_CASE renderer={method} case={case}", flush=True)
            print("COMMAND " + json.dumps(command), flush=True)
            result = subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, timeout=90)
            print(result.stdout, flush=True)
            if result.returncode != 0:
                raise SystemExit(f"{method}/{case} exited {result.returncode}")
            if "ERROR:" in result.stdout or "WARNING:" in result.stdout:
                raise SystemExit(f"{method}/{case} emitted an error or cleanup warning")
            if f" - {renderer} - " not in result.stdout or args.rendering_driver.lower() not in result.stdout.lower():
                raise SystemExit(f"Missing actual {args.rendering_driver}/{renderer} evidence")
            samples = [json.loads(line.split("HEIGHTFIELD_CLEAR_NATIVE_PASS ", 1)[1])
                       for line in result.stdout.splitlines() if "HEIGHTFIELD_CLEAR_NATIVE_PASS " in line]
            if len(samples) != 1:
                raise SystemExit(f"{method}/{case} lacks a unique readback observation")
            sample = samples[0]
            first = sample.get("first_raw", float("nan"))
            second = sample.get("second_raw", float("nan"))
            if (not sample.get("pass") or not sample.get("driver")
                    or sample.get("source_selection_case") != case_index
                    or not sample.get("source_selection_scene_cull")
                    or not sample.get("source_selection_distinct_targets")
                    or not sample.get("source_selection_control_rasterized")
                    or not math.isclose(first, 0.5, abs_tol=0.001)
                    or not math.isclose(second, expected, abs_tol=0.001)):
                raise SystemExit(f"{method}/{case} failed H12: {sample}")
            completed += 1
    print(f"HEIGHTFIELD_SOURCE_SELECTION_PASS cases={completed}", flush=True)


if __name__ == "__main__":
    main()
