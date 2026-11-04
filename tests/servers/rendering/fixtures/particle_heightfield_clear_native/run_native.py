#!/usr/bin/env python3
"""Check the production Mobile heightfield's repeated-capture depth clear on Metal."""

import json
import math
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[4]
command = [
    str(ROOT / "bin/godot.macos.editor.arm64"),
    "--display-driver", "macos", "--rendering-driver", "metal",
    "--rendering-method", "mobile", "--path", str(HERE),
    "--script", "res://run.gd",
]
result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, timeout=90)
print(result.stdout)
if result.returncode != 0:
    sys.exit(result.returncode)
if "Metal 4.0 - Forward Mobile - Using Device #0: Apple" not in result.stdout:
    sys.exit("Missing Metal/Mobile renderer evidence")
samples = [json.loads(line.split("HEIGHTFIELD_CLEAR_NATIVE_PASS ", 1)[1])
           for line in result.stdout.splitlines()
           if "HEIGHTFIELD_CLEAR_NATIVE_PASS " in line]
if len(samples) != 1 or not samples[0].get("pass") or not str(samples[0].get("driver", "")).startswith("Apple") or \
        not math.isclose(samples[0].get("first_raw", float("nan")), 0.75, abs_tol=0.001) or \
        not math.isclose(samples[0].get("second_raw", float("nan")), 0.0, abs_tol=0.001):
    sys.exit("Missing distinct per-instance D32 sample results")
