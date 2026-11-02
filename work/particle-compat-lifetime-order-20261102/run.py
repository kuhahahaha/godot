#!/usr/bin/env python3
"""Qualify both GLES3 lifetime draw-order paths on a live Compatibility renderer."""
import hashlib
import json
import re
import subprocess
from pathlib import Path


def main() -> int:
    root = Path(__file__).resolve().parent
    repo = root.parents[1]
    binary = repo / "bin/godot.macos.editor.arm64"
    digest = hashlib.sha256(binary.read_bytes()).hexdigest()
    command = [str(binary), "--path", str(root / "project"), "--rendering-driver", "opengl3", "--rendering-method", "gl_compatibility", "--fixed-fps", "60", "--verbose"]
    proc = subprocess.run(command, cwd=repo, text=True, capture_output=True, timeout=60)
    log = proc.stdout + proc.stderr
    (root / "run.log").write_text(log)
    stages = re.findall(r"PARTICLE_LIFETIME stage=(lifetime|reverse) pixels=(\d+) aabb=\[P: \([^)]*\), S: \(([^,]+), ([^,]+), ([^)]+)\)\]", log)
    checks = {
        "exit_zero": proc.returncode == 0,
        "compatibility_initialized": "OpenGL API 4.1" in log and "Compatibility" in log,
        "both_orders_render_and_spread": [s[0] for s in stages] == ["lifetime", "reverse"] and all(int(s[1]) > 4 and all(float(v) > 0 for v in s[2:]) for s in stages),
        "node_freed": "PARTICLE_LIFETIME_COMPLETE failures=0 freed=true" in log,
        "no_errors": not any(x in log for x in ("ERROR:", "SCRIPT ERROR:", "PARTICLE_LIFETIME_FAILURE")),
        "binary_unchanged": hashlib.sha256(binary.read_bytes()).hexdigest() == digest,
    }
    result = {"binary_sha256": digest, "command": command, "process_rc": proc.returncode, "checks": checks, "stages": stages}
    (root / "RESULTS.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 0 if all(checks.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
