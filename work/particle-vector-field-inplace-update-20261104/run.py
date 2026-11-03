#!/usr/bin/env python3
"""Verify live Vulkan Mobile vector-field sampling after ImageTexture3D.update()."""
import hashlib
import json
import re
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
EDITOR = ROOT / "bin/godot.macos.editor.arm64"
PIN = "5feadf94534a134a4a1d204dcc16b6fed54839ece6bcb365fa753f03792b33a4"
STAGES = ("plus_x", "updated_minus_x")

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    before = sha(EDITOR)
    if before != PIN:
        raise SystemExit(f"editor pin mismatch: {before}")
    command = [str(EDITOR), "--path", str(HERE), "--rendering-driver", "vulkan",
               "--rendering-method", "mobile", "--fixed-fps", "60", "--script", "res://probe.gd"]
    try:
        process = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
        output, rc, timed_out = process.stdout + process.stderr, process.returncode, False
    except subprocess.TimeoutExpired as exc:
        output = (exc.stdout or b"").decode(errors="replace") + (exc.stderr or b"").decode(errors="replace")
        rc, timed_out = None, True
    log = HERE / "run.log"
    log.write_text(output)
    rows = re.findall(r"^VECTOR_FIELD_STAGE (\{.*\})$", output, re.MULTILINE)
    stages = {row["stage"]: row["motion"] for row in map(json.loads, rows)}
    result_rows = re.findall(r"^VECTOR_FIELD_RESULT (\{.*\})$", output, re.MULTILINE)
    final = json.loads(result_rows[-1]) if len(result_rows) == 1 else {}
    healthy = {name: stages.get(name, {}).get("active") == 8 and
               stages.get(name, {}).get("bytes") == 1024 and
               stages.get(name, {}).get("finite") is True for name in STAGES}
    x = {name: stages.get(name, {}).get("mean_x") for name in STAGES}
    checks = {
        "exit_zero": rc == 0,
        "not_timed_out": not timed_out,
        "editor_pin_unchanged": before == sha(EDITOR) == PIN,
        "vulkan_mobile": "Vulkan" in output and "Forward Mobile" in output,
        "two_stages": set(stages) == set(STAGES),
        "same_resource": final.get("same_resource") is True,
        "same_rid": final.get("same_rid") is True,
        "finite_active_readback": all(healthy.values()),
        "plus_x_positive": x["plus_x"] is not None and x["plus_x"] > 1.0,
        "updated_minus_x_negative": x["updated_minus_x"] is not None and x["updated_minus_x"] < -1.0,
        "no_engine_errors": "ERROR:" not in output and "SCRIPT ERROR:" not in output,
    }
    result = {"command": command, "cwd": str(ROOT), "timeout_seconds": 60,
              "editor_sha_before": before, "editor_sha_after": sha(EDITOR),
              "probe_sha": sha(HERE / "probe.gd"), "log_sha": sha(log),
              "returncode": rc, "timed_out": timed_out,
              "stages": stages, "final": final, "checks": checks}
    (HERE / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 0 if all(checks.values()) else 1

if __name__ == "__main__":
    raise SystemExit(main())
