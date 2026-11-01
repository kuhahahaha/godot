#!/usr/bin/env python3
"""Verify an immutable host with 2D/3D wrapper and ordinary solver neighbors."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys


parser = argparse.ArgumentParser()
parser.add_argument("binary", type=Path)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
binary = args.binary.resolve()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
work = Path(__file__).resolve().parent
root = work.parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def sources():
    names = [
        "servers/physics_2d/physics_server_2d_wrap_mt.cpp",
        "servers/physics_2d/physics_server_2d_wrap_mt.h",
        "servers/physics_3d/physics_server_3d_wrap_mt.cpp",
        "servers/physics_3d/physics_server_3d_wrap_mt.h",
        "tests/servers/physics_2d/test_wrapper_stop_2d.cpp",
        "tests/servers/physics_2d/test_wrapper_r1_gates_2d.cpp",
        "tests/servers/physics_3d/test_wrapper_stop.cpp",
        "tests/servers/physics_3d/test_wrapper_r1_gates.cpp",
        "tests/servers/physics_3d/test_query_retirement.cpp",
        "core/object/worker_thread_pool.cpp",
    ]
    return {name: sha(root / name) for name in names}


def capture(name, command, environment=None, timeout=60):
    record = {"name": name, "argv": command, "cwd": str(root)}
    if environment:
        record["env"] = environment
    env = os.environ.copy()
    env.update(environment or {})
    process = subprocess.Popen(command, cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, start_new_session=True)
    try:
        stdout, stderr = process.communicate(timeout=timeout)
        record.update(rc=process.returncode, stdout=stdout, stderr=stderr)
    except subprocess.TimeoutExpired:
        # Each command owns a fresh process group. Stop only that verification
        # command and its descendants, including an engine below a runner.
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        stdout, stderr = process.communicate()
        record.update(rc=None, timeout=True, stdout=stdout, stderr=stderr)
    (output / (name + ".json")).write_text(json.dumps(record, indent=2) + "\n")
    return record


def clean(record):
    text = record["stdout"] + record["stderr"]
    return "ERROR:" not in text and "WARNING:" not in text


def positive(record):
    return record["rc"] == 0 and clean(record) and bool(re.search(r"test cases:\s*1\s*\|\s*1 passed\s*\|\s*0 failed", record["stdout"])) and bool(re.search(r"assertions:\s*[1-9]\d*\s*\|", record["stdout"])) and "Status: SUCCESS!" in record["stdout"]


def recording_manifest(path, expected):
    manifest = json.loads((path / "RUN_MANIFEST.json").read_text())
    runs = manifest["runs"]
    registry = runs[0]
    registry_ok = registry["rc"] == 0 and clean(registry) and bool(re.search(r"unskipped test cases passing the current filters:\s*" + str(expected) + r"\s*$", registry["stdout"]))
    return len(runs) == expected + 1 and registry_ok and all(positive(run) for run in runs[1:]) and manifest["binary_sha256_before"] == manifest["binary_sha256_after"]


manifest = {"binary": str(binary), "binary_sha256_before": sha(binary), "source_before": sources(), "checks": []}
for name, script, count in [
    ("2d_recording", "run_2d_fixture.py", 8),
    ("2d_gated", "run_2d_r1_gates.py", None),
    ("3d_recording", "run_fixture.py", 7),
    ("3d_gated", "run_r1_gates.py", None),
]:
    destination = output / name
    record = capture(name, [sys.executable, str(work / script), str(binary), "--output", str(destination)], timeout=180)
    try:
        matched = recording_manifest(destination, count) if count else json.loads((destination / "RUN_MANIFEST.json").read_text())["all_matched"]
    except (OSError, ValueError, KeyError, IndexError):
        matched = False
    manifest["checks"].append({"name": name, "matched": record["rc"] == 0 and matched, "detail": str(destination / "RUN_MANIFEST.json")})
    print(name, manifest["checks"][-1]["matched"], flush=True)

selector = "*PhysicsQueryRetirement*real backend dispatch control"
registry = capture("solver_registry", [str(binary), "--headless", "--test", "--test-case=" + selector, "--list-test-cases"], {"GODOT_TEST_PHYSICS_3D_BACKEND": "GodotPhysics3D", "GODOT_TEST_PHYSICS_3D_THREADED": "0"})
registry_ok = registry["rc"] == 0 and clean(registry) and bool(re.search(r"unskipped test cases passing the current filters:\s*1\s*$", registry["stdout"]))
manifest["checks"].append({"name": "solver_registry", "matched": registry_ok})
if registry_ok:
    for backend, prefix in [("GodotPhysics3D", "godot"), ("Jolt Physics", "jolt")]:
        for threaded in ("0", "1"):
            name = prefix + "_" + threaded
            result = capture(name, [str(binary), "--headless", "--test", "--test-case=" + selector], {"GODOT_TEST_PHYSICS_3D_BACKEND": backend, "GODOT_TEST_PHYSICS_3D_THREADED": threaded})
            manifest["checks"].append({"name": name, "matched": positive(result)})
            print(name, manifest["checks"][-1]["matched"], flush=True)

child = capture("small_pool_both_r1", [str(binary), "--headless", "--verbose", "--path", str(work / "small-pool-child"), "--script", "probe.gd", "--max-fps", "60"], timeout=20)
required = [
    "WorkerThreadPool: 1 threads",
    "dedicated threads were requested (2) than threads available (1)",
    "dedicated threads were requested (3) than threads available (2)",
    "R1_SMALL_POOL_PHYSICS_FRAME 1",
    "R1_SMALL_POOL_PHYSICS_FRAME 2",
    "R1_SMALL_POOL_PHYSICS_FRAME 3",
    "R1_SMALL_POOL_BOTH_SYNC_EPOCHS_COMPLETED",
]
small_ok = child["rc"] == 0 and clean(child) and not child["stderr"] and all(text in child["stdout"] for text in required)
manifest["checks"].append({"name": "small_pool_both_r1", "matched": small_ok})
print("small_pool_both_r1", small_ok, flush=True)
manifest["binary_sha256_after"] = sha(binary)
manifest["source_after"] = sources()
manifest["source_drift"] = [name for name, before in manifest["source_before"].items() if manifest["source_after"][name] != before]
manifest["all_matched"] = all(check["matched"] for check in manifest["checks"]) and len(manifest["checks"]) == 10 and manifest["binary_sha256_before"] == manifest["binary_sha256_after"] and not manifest["source_drift"]
(output / "VERIFICATION_MANIFEST.json").write_text(json.dumps(manifest, indent=2) + "\n")
raise SystemExit(0 if manifest["all_matched"] else 1)
