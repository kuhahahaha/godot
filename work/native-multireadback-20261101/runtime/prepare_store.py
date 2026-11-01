#!/usr/bin/env python3
"""Build only the native Store/E05 destination control, preserving the existing four controls."""
import hashlib
import json
from pathlib import Path
import subprocess
RUNTIME = Path(__file__).resolve().parent
REPO = RUNTIME.parents[2]
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
source = RUNTIME / "v2_store_destination_probe.cpp"
output = RUNTIME / "v2_store_destination.dylib"
command = ["clang++", "-std=c++17", "-dynamiclib", "-arch", "arm64", "-mmacosx-version-min=11.0", "-O2", "-Wall", "-Wextra", "-Wno-unused-parameter", "-Wno-deprecated-declarations", "-DTICKET_MODE=3", "-I", str(REPO), str(source), "-o", str(output)]
result = subprocess.run(command, cwd=REPO, capture_output=True, text=True)
(RUNTIME / "build-store_destination.log").write_text(result.stdout + result.stderr)
receipt = {"case": "store_destination", "mode": 3, "source_sha256": sha(source), "header_sha256": sha(RUNTIME / "gdextension_interface_v2.pinned.h"), "command": command, "rc": result.returncode, "sha256": sha(output) if not result.returncode else None}
(RUNTIME / "STORE_BUILD.json").write_text(json.dumps(receipt, indent=2) + "\n")
if result.returncode:
    print(result.stderr)
    raise SystemExit(result.returncode)
project = RUNTIME / "store_destination"
(project / ".godot").mkdir(parents=True, exist_ok=True)
(project / "project.godot").write_text('config_version=5\n[application]\nconfig/name="Original Store/E05 destination native control"\n[rendering]\nrenderer/rendering_method="mobile"\n')
(project / "probe.gd").write_text('extends SceneTree\nfunc _initialize():\n print("MULTI_V2_LOOP_INITIALIZED")\n quit(0)\n')
(project / "probe.gdextension").write_text('[configuration]\nentry_symbol="phs1_probe_init"\ncompatibility_minimum="4.7"\ndevice_exit_guard_version=1\n[libraries]\nmacos="../v2_store_destination.dylib"\n')
(project / ".godot/extension_list.cfg").write_text("res://probe.gdextension\n")
print(f"prepared store_destination: {output}", flush=True)
