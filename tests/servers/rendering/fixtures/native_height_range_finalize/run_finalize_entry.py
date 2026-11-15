#!/usr/bin/env python3
"""Run isolated actual RenderingDevice finalization and LAST_REF controls."""

import argparse
import json
import os
import re
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--host", required=True, type=Path)
parser.add_argument("--out", required=True, type=Path)
parser.add_argument(
    "--expect-guard-removed",
    action="store_true",
    help="Only for root controlled mutation: require dependent-free sentinel exit 87",
)
args = parser.parse_args()
args.out.mkdir(parents=True, exist_ok=True)
rows = []
scenarios = (("busy", 87),) if args.expect_guard_removed else (("busy", 0), ("last_ref", 78))
for mode, expected in scenarios:
    env = os.environ.copy()
    env["GODOT_NHR_FINALIZE_CHILD"] = mode
    command = [
        str(args.host.resolve()),
        "--headless",
        "--test",
        "--test-case=[RenderingDevice][NativeHeightRange] actual finalize child",
    ]
    result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=60)
    output = result.stdout + result.stderr
    (args.out / (mode + ".log")).write_text(output)
    if args.expect_guard_removed:
        passed = result.returncode == 87 and "NHR_FINALIZE_BEFORE" in output and "NHR_DEPENDENT_FREE" in output
        passed = passed and "NHR_FINALIZE_BUSY_RETRY" not in output
    else:
        passed = (
            result.returncode == expected
            and "NHR_FINALIZE_BUSY_RETRY releases=2 recycle=1 owner_live=1 returned=0" in output
        )
        passed = passed and "NHR_DEPENDENT_FREE" not in output and "NHR_FINALIZE_ASSERTION_FAILED" not in output
        if mode == "last_ref":
            passed = (
                passed and "NHR_BEFORE_LAST_REF" in output and "GDEXTENSION_CLOSE_FAILED error=044 route=3" in output
            )
    # Child _Exit deliberately bypasses doctest's summary. No failed assertion
    # may be hidden behind an otherwise expected exit/positive marker. Keep
    # expected RenderingDevice finalize-BUSY ERR_FAIL diagnostics admissible.
    forbidden = re.search(
        r"NHR_FINALIZE_ASSERTION_FAILED|is NOT correct!|FATAL ERROR|"
        r"(?:CHECK|REQUIRE)(?:_\w+)?\([^\n]*\)[^\n]*(?:failed|FAILED)|"
        r"(?:test cases|assertions):[^\n]*[1-9]\d* failed",
        output,
    )
    passed = passed and forbidden is None
    rows.append(dict(mode=mode, expected_exit=expected, actual_exit=result.returncode, passed=passed, command=command))
(args.out / "results.json").write_text(json.dumps(rows, indent=2) + "\n")
raise SystemExit(0 if all(row["passed"] for row in rows) else 1)
