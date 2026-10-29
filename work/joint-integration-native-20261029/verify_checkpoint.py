#!/usr/bin/env python3
"""Read frozen inputs; replay patches only in an own temporary Git index."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

p = Path(__file__).resolve().parent
repo = p.parents[1]


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b""):
            h.update(block)
    return {"bytes": path.stat().st_size, "sha256": h.hexdigest()}


manifest = json.loads((p / "MANIFEST.json").read_text())
for name, expected in manifest["files"].items():
    assert digest(p / name) == expected, name
inputs = json.loads((p / "INPUTS.json").read_text())
assert digest(Path(inputs["core"]["frozen_path"])) == {
    key: inputs["core"][key] for key in ("bytes", "sha256")
}
core_header = (p / "artifacts/gdextension_interface.gen.h").read_text()
sdk_header = (p / "artifacts/sdk-gdextension_interface.h").read_text()
assert core_header[core_header.index("#pragma once"):] == sdk_header[sdk_header.index("#pragma once"):]
expected = json.loads((p / "SOURCE_TREE_BLOBS.json").read_text())
selected = json.loads((p / "SOURCE_PATHS.json").read_text())["selected"]
with tempfile.TemporaryDirectory(prefix="ange-native-intake-index-") as scratch:
    environment = {**os.environ, "GIT_INDEX_FILE": str(Path(scratch) / "index")}
    commands = [
        ["git", "read-tree", inputs["common_base_local_team_4_7_2"]],
        ["git", "apply", "--cached", "--check", str(p / "PRODUCTION_DELTA.patch")],
        ["git", "apply", "--cached", str(p / "PRODUCTION_DELTA.patch")],
    ]
    for command in commands:
        subprocess.run(command, cwd=repo, env=environment, check=True, capture_output=True)
    listing = subprocess.check_output(
        ["git", "ls-files", "--stage", "-z", "--", *selected, "drivers/metal/SCsub"],
        cwd=repo, env=environment,
    )
    actual = {}
    for record in listing.split(b"\0"):
        if record:
            info, name = record.split(b"\t", 1)
            mode, oid, stage = info.decode().split()
            assert stage == "0"
            actual[name.decode()] = {"mode": mode, "oid": oid}
    assert actual == expected
receipt = json.loads((p / "A_OBSERVED_RUNTIME_RECEIPT.json").read_text())
assert receipt["inputs_unchanged"] and receipt["child_reaped"] and not receipt["timed_out"]
assert receipt["returncode"] == 2
assert receipt["inputs_before"][str(repo / "bin/godot.macos.editor.dev.arm64")] == inputs["core"]["sha256"]
assert digest(p / "A_OBSERVED_RUNTIME.log")["sha256"] == receipt["runtime_log_sha256"]
print(json.dumps({
    "verified_manifest_files": len(manifest["files"]),
    "verified_source_targets": len(expected),
    "frozen_editor_sha256": inputs["core"]["sha256"],
    "c_abi_body_equal": True,
    "a_runtime_rc": receipt["returncode"],
    "scope": "Frozen input integrity; no build, GPU run, merge or qualification",
}, indent=2))
