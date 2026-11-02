#!/usr/bin/env python3
"""Run the GLES3 particle readback probe with the current editor binary."""
import hashlib
import json
import re
import subprocess
from pathlib import Path


def main() -> int:
    root = Path(__file__).resolve().parent
    repo = root.parents[1]
    binary = repo / "bin/godot.macos.editor.arm64"
    command = [str(binary), "--path", str(root / "project"), "--rendering-driver", "opengl3", "--rendering-method", "gl_compatibility", "--fixed-fps", "60", "--verbose"]
    digest = hashlib.sha256(binary.read_bytes()).hexdigest()
    run = subprocess.run(command, cwd=repo, text=True, capture_output=True, timeout=60)
    log = run.stdout + run.stderr
    (root / "run.log").write_text(log)
    initial_data = re.search(r"PARTICLE_READBACK stage=initial_data bytes=(\d+)", log)
    initial = re.findall(r"PARTICLE_READBACK stage=initial_(2d|3d) (?:rect|aabb)=([^\n]+)", log)
    active = re.search(r"PARTICLE_READBACK stage=active frames=(\d+) rect2d=\[P: \([^)]*\), S: \(([^,]+), ([^)]+)\)\] aabb3d=\[P: \([^)]*\), S: \(([^,]+), ([^,]+), ([^)]+)\)\]", log)
    checks = {
        "exit_zero": run.returncode == 0,
        "compatibility_initialized": "OpenGL API 4.1" in log and "Compatibility" in log,
        "unallocated_current_data_empty": bool(initial_data) and int(initial_data.group(1)) == 0,
        "initial_readbacks_empty": len(initial) == 2 and all("S: (0.0, 0.0" in value for _, value in initial),
        "active_2d_3d_spread": bool(active) and all(float(value) > 0 for value in active.groups()[1:]),
        "lifecycle_complete": "PARTICLE_READBACK_COMPLETE frames=60 failures=0 freed2d=true freed3d=true" in log,
        "no_errors": not any(value in log for value in ("ERROR:", "SCRIPT ERROR:", "PARTICLE_READBACK_FAILURE")),
        "binary_unchanged": hashlib.sha256(binary.read_bytes()).hexdigest() == digest,
    }
    result = {"binary_sha256": digest, "command": command, "process_rc": run.returncode, "checks": checks, "initial_data_bytes": int(initial_data.group(1)) if initial_data else None, "initial": initial, "active": active.groups() if active else None}
    (root / "RESULTS.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 0 if all(checks.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
