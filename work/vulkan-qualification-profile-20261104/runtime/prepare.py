#!/usr/bin/env python3
"""Compile only the small standalone native controls; never build Godot."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

RUNTIME = Path(__file__).resolve().parent
REPO = RUNTIME.parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--pin-header", type=Path, help="Explicitly replace the local ABI pin before compiling")
args = parser.parse_args()
header = RUNTIME / "gdextension_interface_v2.pinned.h"
if args.pin_header:
    shutil.copyfile(args.pin_header.resolve(), header)
receipt = {"scope": "standalone Godot native control; not an ANGE owner/request acceptance", "header_sha256": hashlib.sha256(header.read_bytes()).hexdigest(), "source_sha256": hashlib.sha256((RUNTIME / "typed_probe.cpp").read_bytes()).hexdigest(), "builds": []}
receipt["include_sha256"] = {name: hashlib.sha256((RUNTIME / name).read_bytes()).hexdigest() for name in ("typed_setup.inc", "typed_retire.inc")}
for case, mode in (("positive", 1), ("cancel_prefix", 2)):
    output = RUNTIME / f"v2_{case}.dylib"
    command = ["clang++", "-std=c++17", "-dynamiclib", "-arch", "arm64", "-mmacosx-version-min=11.0", "-O2", "-Wall", "-Wextra", "-Wno-unused-parameter", "-Wno-deprecated-declarations", f"-DTICKET_MODE={mode}", "-I", str(REPO), str(RUNTIME / "typed_probe.cpp"), "-o", str(output)]
    result = subprocess.run(command, cwd=REPO, capture_output=True, text=True)
    (RUNTIME / f"build-{case}.log").write_text(result.stdout + result.stderr)
    receipt["builds"].append({"case": case, "mode": mode, "command": command, "rc": result.returncode, "sha256": hashlib.sha256(output.read_bytes()).hexdigest() if result.returncode == 0 else None})
    (RUNTIME / "BUILD.json").write_text(json.dumps(receipt, indent=2) + "\n")
    if result.returncode:
        print(result.stderr)
        raise SystemExit(result.returncode)
    project = RUNTIME / case
    (project / ".godot").mkdir(parents=True, exist_ok=True)
    (project / "project.godot").write_text('config_version=5\n[application]\nconfig/name="Original Metal V2 native control"\n[rendering]\nrenderer/rendering_method="mobile"\n')
    (project / "probe.gd").write_text('extends SceneTree\nfunc _initialize():\n print("MULTI_V2_LOOP_INITIALIZED")\n quit(0)\n')
    (project / "probe.gdextension").write_text('[configuration]\nentry_symbol="phs1_probe_init"\ncompatibility_minimum="4.7"\ndevice_exit_guard_version=1\n[libraries]\nmacos="../v2_' + case + '.dylib"\n')
    (project / ".godot/extension_list.cfg").write_text("res://probe.gdextension\n")
    print(f"prepared {case}: {output}", flush=True)
