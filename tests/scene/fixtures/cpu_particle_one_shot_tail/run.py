#!/usr/bin/env python3
"""Run public-API CPU particle regression and preserve a strict engine receipt."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
MARKER = "CPU_ONESHOT_ENGINE="


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--godot", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--timeout", type=float, default=30)
    args = parser.parse_args()
    binary = args.godot.expanduser().resolve()
    output = args.output.expanduser().resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    receipt = {"binary": str(binary), "fixture_sha256": {}, "command": [], "engine_exit": None,
               "timed_out": False, "stdout": "", "stderr": "", "errors": [], "result": None,
               "acceptance_scope": "headless CPU particle/public-buffer only; not GPU, renderer, or pixel acceptance"}
    try:
        before = binary.stat()
        receipt["binary_sha256"] = hashlib.sha256(binary.read_bytes()).hexdigest()
        receipt["binary_size"] = before.st_size
        receipt["binary_mtime_ns"] = before.st_mtime_ns
        with tempfile.TemporaryDirectory(prefix="godot-cpu-one-shot-tail-") as temporary:
            project = Path(temporary)
            for name in ("run.gd", "project.godot"):
                source = HERE / name
                receipt["fixture_sha256"][name] = hashlib.sha256(source.read_bytes()).hexdigest()
                shutil.copyfile(source, project / name)
            (project / ".godot").mkdir()
            (project / ".godot/global_script_class_cache.cfg").write_text("list=[]\n")
            command = [str(binary), "--headless", "--path", str(project), "--script", "res://run.gd"]
            receipt["command"] = command
            try:
                run = subprocess.run(command, capture_output=True, text=True, timeout=args.timeout)
                receipt.update(engine_exit=run.returncode, stdout=run.stdout, stderr=run.stderr)
            except subprocess.TimeoutExpired as exc:
                def decoded(value):
                    return value.decode(errors="replace") if isinstance(value, bytes) else value or ""
                receipt.update(timed_out=True, stdout=decoded(exc.stdout), stderr=decoded(exc.stderr))
                receipt["errors"].append("Engine timed out")
        after = binary.stat()
        if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
            receipt["errors"].append("Binary changed during run")
        lines = [line for line in receipt["stdout"].splitlines() if line.startswith(MARKER)]
        if len(lines) != 1:
            receipt["errors"].append(f"Expected exactly one result marker, found {len(lines)}")
        else:
            result = json.loads(lines[0][len(MARKER):])
            receipt["result"] = result
            if not isinstance(result, dict) or not isinstance(result.get("failures"), list):
                receipt["errors"].append("Result must contain a failures array")
            else:
                expected_exit = 1 if result["failures"] else 0
                if receipt["engine_exit"] != expected_exit:
                    receipt["errors"].append(f"Engine exit {receipt['engine_exit']} disagrees with failures (expected {expected_exit})")
                if result["failures"]:
                    receipt["errors"].append("Engine reported regression failures")
        diagnostics = receipt["stdout"] + "\n" + receipt["stderr"]
        if "ERROR:" in diagnostics or re.search(r"\b(?:RID|ObjectDB)\b[^\n]*\bleaked\b", diagnostics, re.IGNORECASE):
            receipt["errors"].append("Engine reported ERROR or RID/ObjectDB leak diagnostics")
    except (OSError, ValueError, TypeError) as exc:
        receipt["errors"].append(f"{type(exc).__name__}: {exc}")
    receipt["passed"] = not receipt["errors"]
    output.with_suffix(".stdout.log").write_text(receipt["stdout"])
    output.with_suffix(".stderr.log").write_text(receipt["stderr"])
    output.write_text(json.dumps(receipt, indent=2) + "\n")
    print(f"CPU one-shot tail: {'PASS' if receipt['passed'] else 'FAIL'}; engine_exit={receipt['engine_exit']}; receipt={output}")
    return 0 if receipt["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
