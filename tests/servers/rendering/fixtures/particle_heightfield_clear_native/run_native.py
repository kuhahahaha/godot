#!/usr/bin/env python3
"""Check the production Mobile heightfield's repeated-capture depth clear on Metal."""

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
if 'HEIGHTFIELD_CLEAR_NATIVE_PASS {"driver":"Apple' not in result.stdout or '"raw":0.0' not in result.stdout:
    sys.exit("Missing actual D32 sample result")
