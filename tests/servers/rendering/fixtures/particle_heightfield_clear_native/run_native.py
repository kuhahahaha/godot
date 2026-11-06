#!/usr/bin/env python3
"""Check independent D32 clear and production geometry capture on both Metal renderers."""

import json
import math
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[4]
for height in (None, -0.5, 0.0, 0.5):
    for method, renderer in (("mobile", "Forward Mobile"),
                             ("forward_plus", "Forward+")):
        command = [
            str(ROOT / "bin/godot.macos.editor.arm64"),
            "--display-driver", "macos", "--rendering-driver", "metal",
            "--rendering-method", method, "--path", str(HERE),
            "--script", "res://run.gd",
        ]
        env = os.environ.copy()
        env["ANGE_HEIGHTFIELD_GEOMETRY"] = "0" if height is None else "1"
        env["ANGE_HEIGHTFIELD_GEOMETRY_HEIGHT"] = str(height or 0.0)
        result = subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=90)
        print(f"HEIGHTFIELD_NATIVE_CASE renderer={method} height={height}")
        print(result.stdout)
        if result.returncode != 0:
            sys.exit(result.returncode)
        if "ERROR:" in result.stdout or "WARNING:" in result.stdout:
            sys.exit("Native capture emitted an error or cleanup warning")
        if f"Metal 4.0 - {renderer} - Using Device #0: Apple" not in result.stdout:
            sys.exit(f"Missing Metal/{renderer} renderer evidence")
        samples = [json.loads(line.split("HEIGHTFIELD_CLEAR_NATIVE_PASS ", 1)[1])
                   for line in result.stdout.splitlines()
                   if "HEIGHTFIELD_CLEAR_NATIVE_PASS " in line]
        # Fixed orthographic field spans world y=-1..+1, with reversed depth.
        expected = 0.0 if height is None else (height + 1.0) / 2.0
        if len(samples) != 1 or not samples[0].get("pass") or \
                not str(samples[0].get("driver", "")).startswith("Apple") or \
                not math.isclose(samples[0].get("first_raw", float("nan")), 0.75, abs_tol=0.001) or \
                not math.isclose(samples[0].get("second_raw", float("nan")), expected, abs_tol=0.001):
            sys.exit(f"Missing independent D32 samples on {renderer}, height={height}")
