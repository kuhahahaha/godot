#!/usr/bin/env python3
"""Exercise retained Scene D32/framebuffer ownership on two real main-RD renderers.

Requires a tests=yes module_particles_runtime_tests_enabled=yes editor and GPU.
This verifies the resource component and checked empty-list cleanup only; it does
not publish a HeightFrame or qualify a Scene sample/producer-completion bridge.
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
    parser.add_argument("--quoted", action="store_true", help="Require original native quote, same-owner allocation/sink and retained projection return")
    parser.add_argument("--display-driver", default="x11")
    parser.add_argument("--rendering-driver", default="vulkan")
    args = parser.parse_args()
    for method, renderer in (("mobile", "Forward Mobile"), ("forward_plus", "Forward+")):
        command = [str(args.binary.resolve()), "--display-driver", args.display_driver,
                   "--rendering-driver", args.rendering_driver, "--rendering-method", method,
                   "--path", str(HERE), "--script", "res://run.gd"]
        env = os.environ.copy()
        for key in ("ANGE_HEIGHTFIELD_GEOMETRY", "ANGE_HEIGHTFIELD_GEOMETRY_HEIGHT",
                    "ANGE_HEIGHTFIELD_EXTENTS_Y", "ANGE_HEIGHTFIELD_TRANSFORM_Y",
                    "ANGE_HEIGHTFIELD_TRANSFORM_CASE", "ANGE_HEIGHTFIELD_LEGACY_NORMAL",
                    "ANGE_HEIGHTFIELD_LEGACY_NEIGHBOR_CASE"):
            env.pop(key, None)
        env.pop("ANGE_HEIGHTFIELD_CANDIDATE", None)
        env.pop("ANGE_HEIGHTFIELD_GEOMETRY_REFUSAL", None)
        env.pop("ANGE_HEIGHTFIELD_QUOTED_RESOURCES", None)
        if args.quoted:
            env["ANGE_HEIGHTFIELD_QUOTED_RESOURCES"] = "1"
        env["ANGE_HEIGHTFIELD_FRAME_RESOURCES"] = "1"
        print(f"HEIGHTFIELD_RESOURCES_CASE renderer={method}", flush=True)
        print("COMMAND " + json.dumps(command), flush=True)
        result = subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=90)
        print(result.stdout, flush=True)
        if result.returncode != 0:
            raise SystemExit(f"Resource case exited {result.returncode}")
        if "ERROR:" in result.stdout or "WARNING:" in result.stdout:
            raise SystemExit("Resource case emitted an error or cleanup warning")
        if f" - {renderer} - " not in result.stdout or args.rendering_driver.lower() not in result.stdout.lower():
            raise SystemExit(f"Missing actual {args.rendering_driver}/{renderer} evidence")
        samples = [json.loads(line.split("HEIGHTFIELD_CLEAR_NATIVE_PASS ", 1)[1])
                   for line in result.stdout.splitlines() if "HEIGHTFIELD_CLEAR_NATIVE_PASS " in line]
        if len(samples) != 1:
            raise SystemExit("Missing unique completed resource observation")
        sample = samples[0]
        if args.quoted and (not sample.get("quoted_allocation_verified") or not sample.get("quoted_projection_returned")):
            raise SystemExit(f"Missing original quoted allocation/projection evidence on {renderer}")
        reports = sample.get("physical_reports", [])
        identities = [sample.get(key) for key in ("first_depth_rid", "second_depth_rid",
                      "first_framebuffer_rid", "second_framebuffer_rid")]
        if (not sample.get("pass") or not sample.get("driver")
                or not sample.get("retained_busy") or not sample.get("distinct_rids")
                or not sample.get("failure_cut_restored") or not sample.get("ordinary_after_cut")
                or sample.get("checked_clears") != [True, True]
                or not math.isclose(sample.get("first_raw", float("nan")), 0.75, abs_tol=0.001)
                or not math.isclose(sample.get("second_raw", float("nan")), 0.25, abs_tol=0.001)
                or not sample.get("all_physical_returned") or len(reports) != 4
                or len(set(identities)) != 4 or not all(identities)
                or {entry.get("rid") for entry in reports} != set(identities)
                or len({entry.get("serial") for entry in reports}) != 4
                or not all(entry.get("returned") and entry.get("serial", 0) > 0 for entry in reports)):
            raise SystemExit(f"Incomplete independent raw/retention/physical-return evidence on {renderer}")
    print("HEIGHTFIELD_QUOTED_RESOURCES_MATRIX_PASS 2 real GPU cases" if args.quoted else "HEIGHTFIELD_RESOURCES_MATRIX_PASS 2 real GPU cases")


if __name__ == "__main__":
    main()
