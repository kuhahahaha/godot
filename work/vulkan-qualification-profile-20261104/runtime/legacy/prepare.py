#!/usr/bin/env python3
"""Rebuild only the two applicable V1 native neighbor DSOs against the current pin."""
import hashlib
import json
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
HEADER = HERE.parent / "gdextension_interface_v2.pinned.h"
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
receipt = {"scope": "V1 Record312/Report184/kinds1-9 native neighbor controls; no ANGE acceptance", "header_sha256": sha(HEADER), "builds": []}
for name in ("qrt_ticket", "qrt_upload"):
    source = HERE / f"{name}_probe.cpp"
    output = HERE / f"{name}_probe.dylib"
    command = ["clang++", "-std=c++17", "-dynamiclib", "-arch", "arm64", "-mmacosx-version-min=11.0", "-O2", "-Wall", "-Wextra", "-Wno-unused-parameter", "-Wno-deprecated-declarations", "-I", str(REPO), str(source), "-o", str(output)]
    result = subprocess.run(command, cwd=REPO, capture_output=True, text=True)
    (HERE / f"build-{name}.log").write_text(result.stdout + result.stderr)
    receipt["builds"].append({"case": name, "command": command, "rc": result.returncode, "source_sha256": sha(source), "sha256": sha(output) if result.returncode == 0 else None})
    (HERE / "BUILD.json").write_text(json.dumps(receipt, indent=2) + "\n")
    if result.returncode:
        print(result.stderr)
        raise SystemExit(result.returncode)
    print(f"prepared V1 neighbor: {output}", flush=True)
