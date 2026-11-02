#!/usr/bin/env python3
"""Qualify live GLES3 draw-pass resize and cleanup on Compatibility."""
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
    samples = re.findall(r"PARTICLE_DRAW_PASS stage=(one|two|shrunk) passes=(\d+) red=(\d+) green=(\d+) aabb_x=([\d.]+)", log)
    expected = [("one", 1), ("two", 2), ("shrunk", 1)]
    checks = {
        "exit_zero": proc.returncode == 0,
        "compatibility_initialized": "OpenGL API 4.1" in log and "Compatibility" in log,
        "three_stages": [(s[0], int(s[1])) for s in samples] == expected,
        "colors_and_aabb": len(samples) == 3 and all(int(s[2]) > 4 and float(s[4]) > 0.2 for s in samples) and int(samples[0][3]) == 0 and int(samples[1][3]) > 4 and int(samples[2][3]) == 0,
        "node_freed": "PARTICLE_DRAW_PASS_COMPLETE failures=0 freed=true" in log,
        "no_errors": not any(x in log for x in ("ERROR:", "SCRIPT ERROR:", "PARTICLE_DRAW_PASS_FAILURE")),
        "binary_unchanged": hashlib.sha256(binary.read_bytes()).hexdigest() == digest,
    }
    result = {"binary_sha256": digest, "command": command, "process_rc": proc.returncode, "checks": checks, "samples": samples}
    (root / "RESULTS.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 0 if all(checks.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
