#!/usr/bin/env python3
"""Capture gated R1 positives and exact pre-danger negative controls."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


parser = argparse.ArgumentParser()
parser.add_argument("binary", type=Path)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
binary = args.binary.resolve()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
root = Path(__file__).resolve().parents[2]
suffixes = [
    ("pump_origin", "pump-origin finish returns from step and each Main boundary services it"),
    ("barrier_cleanup", "barrier precedes PARK and acknowledgement waits for trailing cleanup"),
    ("late_publisher", "late admitted publisher and end-sync release tail precede sealed pump finish"),
    ("boundary_race", "rejected Main boundary admission still services a raced foreign stop"),
]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


manifest = {"binary": str(binary), "binary_sha256_before": sha(binary), "runs": []}
commands = [("registry", "*PhysicsWrapperR1Gates*", "valid", None)]
commands.extend((name, "*PhysicsWrapperR1Gates*" + suffix, "valid", None) for name, suffix in suffixes)
commands.extend([
    ("early_park_negative", "*PhysicsWrapperR1Gates*" + suffixes[1][1], "early-park", "PHYSICS_R3_PARK_BEFORE_BARRIER_PRE_WAIT"),
    ("early_ack_negative", "*PhysicsWrapperR1Gates*" + suffixes[1][1], "early-ack", "PHYSICS_R3_ACK_BEFORE_CLEANUP_RETURN"),
])
for name, selector, control, marker in commands:
    command = [str(binary), "--headless", "--test", "--test-case=" + selector]
    if name == "registry":
        command.append("--list-test-cases")
    environment = os.environ.copy()
    environment["GODOT_TEST_PHYSICS_WRAPPER_R3_CONTROL"] = control
    record = {"name": name, "argv": command, "env": {"GODOT_TEST_PHYSICS_WRAPPER_R3_CONTROL": control}}
    try:
        result = subprocess.run(command, cwd=root, env=environment, capture_output=True, text=True, timeout=45)
        record.update(rc=result.returncode, stdout=result.stdout, stderr=result.stderr)
        text = result.stdout + result.stderr
        clean = "ERROR:" not in text and "WARNING:" not in text and "PHYSICS_R1_FIXTURE_" not in text
        if name == "registry":
            record["matched_expectation"] = result.returncode == 0 and clean and all("[PhysicsWrapperR1Gates] " + suffix in result.stdout for _, suffix in suffixes)
        elif marker:
            record["expected_marker"] = marker
            record["matched_expectation"] = result.returncode == 86 and clean and result.stderr.count(marker) == 1
        else:
            record["matched_expectation"] = result.returncode == 0 and clean and bool(re.search(r"test cases:\s*1\s*\|\s*1 passed\s*\|\s*0 failed", result.stdout)) and "Status: SUCCESS!" in result.stdout
    except subprocess.TimeoutExpired as error:
        record.update(rc=None, timeout=True, stdout=str(error.stdout or ""), stderr=str(error.stderr or ""), matched_expectation=False)
    (output / (name + ".json")).write_text(json.dumps(record, indent=2) + "\n")
    manifest["runs"].append(record)
    print(name, record["rc"], record["matched_expectation"], flush=True)
    if name == "registry" and not record["matched_expectation"]:
        break
manifest["binary_sha256_after"] = sha(binary)
manifest["all_matched"] = len(manifest["runs"]) == len(commands) and all(run["matched_expectation"] for run in manifest["runs"]) and manifest["binary_sha256_before"] == manifest["binary_sha256_after"]
(output / "RUN_MANIFEST.json").write_text(json.dumps(manifest, indent=2) + "\n")
raise SystemExit(0 if manifest["all_matched"] else 1)
