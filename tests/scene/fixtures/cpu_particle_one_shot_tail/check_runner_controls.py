#!/usr/bin/env python3
"""Calibrate runner rejection paths with immediate /bin/sh executables."""
import json
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent
MARKER = 'CPU_ONESHOT_ENGINE={"failures":[]}'
CASES = {
    "missing": "#!/bin/sh\nprintf '%s\\n' 'no marker'\nexit 0\n",
    "duplicate": f"#!/bin/sh\nprintf '%s\\n' '{MARKER}' '{MARKER}'\nexit 0\n",
    "exit-mismatch": f"#!/bin/sh\nprintf '%s\\n' '{MARKER}'\nexit 1\n",
    "timeout": "#!/bin/sh\nexec /bin/sleep 10\n",
    "error-diagnostic": f"#!/bin/sh\nprintf '%s\\n' '{MARKER}'\nprintf '%s\\n' 'ERROR: rejected diagnostic' >&2\nexit 0\n",
    "rid-leak": f"#!/bin/sh\nprintf '%s\\n' '{MARKER}'\nprintf '%s\\n' 'WARNING: 1 RID of type CanvasItem was leaked.' >&2\nexit 0\n",
    "clean-pass": f"#!/bin/sh\nprintf '%s\\n' '{MARKER}'\nexit 0\n",
}


def main():
    controls = HERE / "runner-controls"
    controls.mkdir(exist_ok=True)
    rows = []
    for name, script in CASES.items():
        fake = controls / (name + ".sh")
        fake.write_text(script)
        fake.chmod(0o755)
        output = controls / (name + ".json")
        command = [sys.executable, str(HERE / "run.py"), "--godot", str(fake), "--output", str(output), "--timeout", "0.2" if name == "timeout" else "5"]
        run = subprocess.run(command, capture_output=True, text=True, timeout=15)
        result = json.loads(output.read_text())
        count = sum(line.startswith("CPU_ONESHOT_ENGINE=") for line in result["stdout"].splitlines())
        expected_count = {"missing": 0, "duplicate": 2, "timeout": 0}.get(name, 1)
        assert count == expected_count, (name, count)
        assert result["timed_out"] == (name == "timeout"), name
        assert run.returncode == (0 if name == "clean-pass" else 1), name
        if name != "timeout":
            assert result["engine_exit"] == (1 if name == "exit-mismatch" else 0), name
        if name == "missing":
            assert result["errors"] == ["Expected exactly one result marker, found 0"]
        elif name == "duplicate":
            assert result["errors"] == ["Expected exactly one result marker, found 2"]
        elif name == "exit-mismatch":
            assert result["result"] == {"failures": []}
            assert result["errors"] == ["Engine exit 1 disagrees with failures (expected 0)"]
        elif name in ("error-diagnostic", "rid-leak"):
            assert result["errors"] == ["Engine reported ERROR or RID/ObjectDB leak diagnostics"]
        rows.append({"case": name, "fake_script": script, "runner_command": command,
                     "runner_exit": run.returncode, "runner_stdout": run.stdout, "runner_stderr": run.stderr,
                     "marker_count": count, "receipt": result})
    (HERE / "runner-controls.json").write_text(json.dumps(rows, indent=2) + "\n")
    print("runner controls: 7 specific paths verified (non-timeout paths completed within 5s)")


if __name__ == "__main__":
    main()
