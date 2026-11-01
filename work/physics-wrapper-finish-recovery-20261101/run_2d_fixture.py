#!/usr/bin/env python3
"""Capture starter fixture behavior; do not turn arbitrary failures into RED."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess


parser = argparse.ArgumentParser()
parser.add_argument("binary", type=Path)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
binary = args.binary.resolve()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
root = Path(__file__).resolve().parents[2]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


manifest = {"binary": str(binary), "binary_sha256_before": sha(binary), "runs": []}
cases = [
    ("foreign_collide_control", "ordinary foreign body-collide caller policy control", "control should pass"),
    ("inline_control", "ordinary inline lifecycle control", "control should pass"),
    ("threaded_control", "ordinary threaded lifecycle preserves finish affinity", "control should pass"),
    ("inline_owner", "inline normal-owner finish waits for owner return", "inspect deferred finish and exact-once assertions"),
    ("generated_owner", "inline generated-owner finish waits for owner return", "inspect deferred finish assertions"),
    ("query_owner", "inline query finish ends active sync once after traversal", "inspect deferred finish and end_sync assertions"),
    ("stopped_entries", "inline stopped generated and custom entries preserve outputs", "inspect no-backend-call and stopped defaults assertions"),
    ("preflush_cleanup", "direct getter preflush payload cleanup defers finish through getter", "inspect cleanup return and getter ordering assertions"),
]
commands = [("registry", "*PhysicsWrapper2DStop*", "require all eight exact case names", True)]
commands.extend((name, "*PhysicsWrapper2DStop*" + suffix, expectation, False) for name, suffix, expectation in cases)
for name, selector, expectation, listing in commands:
    command = [str(binary), "--headless", "--test", "--test-case=" + selector]
    if listing:
        command.append("--list-test-cases")
    record = {"name": name, "argv": command, "expectation": expectation}
    try:
        result = subprocess.run(command, cwd=root, capture_output=True, text=True, timeout=45)
        record.update(rc=result.returncode, stdout=result.stdout, stderr=result.stderr)
    except subprocess.TimeoutExpired as error:
        record.update(rc=None, timeout=True, stdout=str(error.stdout or ""), stderr=str(error.stderr or ""))
    (output / (name + ".json")).write_text(json.dumps(record, indent=2) + "\n")
    manifest["runs"].append(record)
    print(name, "rc=" + str(record["rc"]), flush=True)
    # A missing/disabled test registry is a tool/configuration failure. Running
    # selectors after that cannot establish behavior or a meaningful RED.
    if listing and (record["rc"] != 0 or not all("[PhysicsWrapper2DStop] " + suffix in record["stdout"] for _, suffix, _ in cases)):
        manifest["registry_unavailable_or_incomplete"] = True
        break
manifest["binary_sha256_after"] = sha(binary)
manifest["automatic_behavior_red_claim"] = False
(output / "RUN_MANIFEST.json").write_text(json.dumps(manifest, indent=2) + "\n")
raise SystemExit(2 if manifest.get("registry_unavailable_or_incomplete") else 0)
