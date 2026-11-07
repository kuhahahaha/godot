#!/usr/bin/env python3
"""Exercise immutable Scene height candidates on two real main-RD renderers.

Requires a tests=yes module_particles_runtime_tests_enabled=yes editor and GPU.
This adds indexed checked capture and frozen-source observations to every
resource-mode assertion. CaptureRecorded does not grant Current or Complete.
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
    parser.add_argument("--geometry-refusal", action="store_true",
                        help="Require current RD refusal of a real two-instance MultiMesh and same-session recovery")
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
                    "ANGE_HEIGHTFIELD_GEOMETRY_REFUSAL",
                    "ANGE_HEIGHTFIELD_LEGACY_NEIGHBOR_CASE"):
            env.pop(key, None)
        env["ANGE_HEIGHTFIELD_FRAME_RESOURCES"] = "1"
        env["ANGE_HEIGHTFIELD_CANDIDATE"] = "1"
        if args.geometry_refusal:
            env["ANGE_HEIGHTFIELD_GEOMETRY_REFUSAL"] = "1"
        print(f"HEIGHTFIELD_CANDIDATE_CASE renderer={method}", flush=True)
        print("COMMAND " + json.dumps(command), flush=True)
        result = subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=90)
        print(result.stdout, flush=True)
        if result.returncode != 0:
            raise SystemExit(f"Resource case exited {result.returncode}")
        if args.geometry_refusal:
            lines = result.stdout.splitlines()
            begins = [i for i, line in enumerate(lines) if line == "HEIGHTFIELD_EXPECTED_GEOMETRY_REFUSAL_BEGIN"]
            ends = [i for i, line in enumerate(lines) if line == "HEIGHTFIELD_EXPECTED_GEOMETRY_REFUSAL_END"]
            errors = [(i, line) for i, line in enumerate(lines) if "ERROR:" in line]
            condition = ('Condition "!draw_list.state.pipeline.is_valid() || p_instances != 1 || '
                         'p_procedural_vertices != 0" is true. Returning: result.failure(ERR_UNAVAILABLE)')
            if (len(begins) != 1 or len(ends) != 1 or len(errors) != 1
                    or not begins[0] < errors[0][0] < ends[0]
                    or condition not in errors[0][1]
                    or "_draw_list_draw_recorded" not in "\n".join(lines[errors[0][0]:ends[0]])
                    or "WARNING:" in result.stdout):
                raise SystemExit("Expected exactly the scoped RD instance-profile refusal and no other error/warning")
        elif "ERROR:" in result.stdout or "WARNING:" in result.stdout:
            raise SystemExit("Resource case emitted an error or cleanup warning")
        if f" - {renderer} - " not in result.stdout or args.rendering_driver.lower() not in result.stdout.lower():
            raise SystemExit(f"Missing actual {args.rendering_driver}/{renderer} evidence")
        samples = [json.loads(line.split("HEIGHTFIELD_CLEAR_NATIVE_PASS ", 1)[1])
                   for line in result.stdout.splitlines() if "HEIGHTFIELD_CLEAR_NATIVE_PASS " in line]
        if len(samples) != 1:
            raise SystemExit("Missing unique completed resource observation")
        sample = samples[0]
        reports = sample.get("physical_reports", [])
        identities = [sample.get(key) for key in ("first_depth_rid", "second_depth_rid",
                      "first_framebuffer_rid", "second_framebuffer_rid")]
        if (not sample.get("pass") or not sample.get("driver")
                or not sample.get("retained_busy") or not sample.get("distinct_rids")
                or not sample.get("failure_cut_restored") or not sample.get("ordinary_after_cut")
                or sample.get("checked_clears") != [True, True]
                or not sample.get("candidate_pass")
                or not sample.get("candidate_snapshot_frozen")
                or not sample.get("candidate_failed_without_business_prefix")
                or not sample.get("candidate_postbind_restored")
                or not sample.get("candidate_capture_once")
                or sample.get("candidate_state") != "CaptureRecorded"
                or not sample.get("candidate_checked_handles")
                or sample.get("candidate_draws") != 1
                or sample.get("candidate_index_binds") != 1
                or sample.get("candidate_uniform_binds", 0) < 3
                or not math.isclose(sample.get("first_raw", float("nan")), 0.75, abs_tol=0.001)
                or not math.isclose(sample.get("second_raw", float("nan")), 0.25, abs_tol=0.001)
                or not sample.get("all_physical_returned") or len(reports) != 4
                or len(set(identities)) != 4 or not all(identities)
                or {entry.get("rid") for entry in reports} != set(identities)
                or len({entry.get("serial") for entry in reports}) != 4
                or not all(entry.get("returned") and entry.get("serial", 0) > 0 for entry in reports)):
            raise SystemExit(f"Incomplete independent raw/retention/physical-return evidence on {renderer}")
        if args.geometry_refusal:
            refusal_serial = sample.get("geometry_refusal_depth_serial", 0)
            recovery_serial = sample.get("geometry_recovery_depth_serial", 0)
            if (sample.get("geometry_refusal_instances") != 2
                    or sample.get("geometry_refusal_error") != 2  # ERR_UNAVAILABLE, core/error/error_list.h
                    or sample.get("geometry_refusal_state") != "FailedRetiring"
                    or not sample.get("geometry_refusal_at_rd_draw")
                    or not sample.get("geometry_refusal_restored")
                    or not sample.get("geometry_refusal_ordinary_recovered")
                    or not math.isclose(sample.get("geometry_refusal_ordinary_raw", float("nan")), 0.25, abs_tol=0.001)
                    or sample.get("geometry_refusal_occurrence") != 1
                    or sample.get("geometry_recovery_occurrence") != 2
                    or not refusal_serial or not recovery_serial or refusal_serial == recovery_serial):
                raise SystemExit(f"Incomplete real MultiMesh refusal/recovery evidence on {renderer}")
    if args.geometry_refusal:
        print("HEIGHTFIELD_GEOMETRY_REFUSAL_MATRIX_PASS 2 real GPU cases")
    else:
        print("HEIGHTFIELD_CANDIDATE_MATRIX_PASS 2 real GPU cases")


if __name__ == "__main__":
    main()
