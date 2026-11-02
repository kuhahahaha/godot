"""Capture a pinned executable run without interpreting it as native acceptance."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess
import time
import math
import argparse
import os
import signal
import shutil

REQUIRED_INPUTS = {
    "editor", "script", "sdk", "dso", "request", "oracle", "issuer", "project",
}
REQUIRED_VERSION_FIELDS = {
    "godot_revision", "ange_revision", "editor_version", "sdk_version", "dso_version",
    "renderer", "device",
}


def _digest(path_string: str):
    path = Path(path_string)
    if not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _snapshot(inputs):
    return {name: {
        "path": spec["path"],
        "sha256": _digest(spec["path"]),
    } for name, spec in inputs.items()}


def _validate(spec):
    if spec.get("scenario") not in {"success", "failure", "cancel"}:
        raise ValueError("scenario must be explicit and supported")
    timeout = spec.get("timeout_seconds", 60)
    if not isinstance(timeout, (int, float)) or not math.isfinite(timeout) or timeout <= 0:
        raise ValueError("timeout must be finite and positive")
    missing = REQUIRED_INPUTS - set(spec.get("inputs", {}))
    if missing:
        raise ValueError("missing required input(s): " + ", ".join(sorted(missing)))
    missing_versions = REQUIRED_VERSION_FIELDS - set(spec.get("declared_versions", {}))
    if missing_versions:
        raise ValueError("missing declared version(s): " + ", ".join(sorted(missing_versions)))
    if any(not isinstance(value, str) or not value.strip()
           for value in spec["declared_versions"].values()):
        raise ValueError("declared versions must be nonempty strings")
    command = spec.get("command")
    if not isinstance(command, list) or not command:
        raise ValueError("command must be a non-empty list")
    editor = str(Path(spec["inputs"]["editor"]["path"]).resolve())
    if any(not isinstance(part, str) for part in command):
        raise ValueError("command arguments must be strings")
    command_editor = str(Path(command[0].replace("{editor}", editor)).resolve())
    if command_editor != editor:
        raise ValueError("command must use the pinned editor input")
    for item in spec["inputs"].values():
        if not isinstance(item.get("path"), str) or not isinstance(item.get("sha256"), str):
            raise ValueError("each input must contain path and sha256")
        if not Path(item["path"]).is_absolute():
            raise ValueError("input paths must be absolute")
    outputs = spec.get("outputs", {})
    if not isinstance(outputs, dict):
        raise ValueError("outputs must be a name-to-absolute-path object")
    resolved_outputs = set()
    for name, path in outputs.items():
        if not isinstance(name, str) or not name or "/" in name or name in {".", ".."}:
            raise ValueError("output names must be simple nonempty names")
        if not isinstance(path, str) or not Path(path).is_absolute():
            raise ValueError("output paths must be absolute")
        if Path(path).exists() or Path(path).is_symlink():
            raise ValueError("output paths must not exist before launch")
        resolved_path = Path(path).resolve()
        if resolved_path in resolved_outputs:
            raise ValueError("output paths must be unique")
        resolved_outputs.add(resolved_path)
        if resolved_path in {Path(item["path"]).resolve() for item in spec["inputs"].values()}:
            raise ValueError("output path overlaps pinned input")


def capture(spec, output):
    _validate(spec)
    output = Path(output)
    output.mkdir(parents=True, exist_ok=False)
    before = _snapshot(spec["inputs"])
    if any(item["sha256"] is None or item["sha256"] != spec["inputs"][name]["sha256"]
           for name, item in before.items()):
        result = {
            "status": "INPUT_PIN_MISMATCH", "returncode": None, "timed_out": False,
            "inputs_before": before, "inputs_after": before,
            "production_acceptance": "NOT_PASSED",
            "native_cleanup": "REQUIRES_SAME_OWNER_EVIDENCE",
            "spec": spec,
        }
        (output / "capture.json").write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
        return result

    command = [part.replace("{editor}", spec["inputs"]["editor"]["path"])
                   .replace("{script}", spec["inputs"]["script"]["path"])
               for part in spec["command"]]
    started = time.monotonic()
    timed_out = False
    try:
        process = subprocess.Popen(command, cwd=spec.get("cwd"), stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, start_new_session=True)
        try:
            stdout, stderr = process.communicate(timeout=float(spec.get("timeout_seconds", 60)))
        except subprocess.TimeoutExpired:
            timed_out = True
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            stdout, stderr = process.communicate()
    except OSError as error:
        stdout, stderr = b"", str(error).encode()
        returncode = None
        launch_failed = True
    else:
        returncode = process.returncode
        launch_failed = False
    (output / "stdout.bin").write_bytes(stdout)
    (output / "stderr.bin").write_bytes(stderr)
    output_artifacts = {}
    if spec.get("outputs"):
        artifact_dir = output / "artifacts"
        artifact_dir.mkdir()
        for name, source_path in spec["outputs"].items():
            source = Path(source_path)
            if source.is_file() and not source.is_symlink():
                destination = artifact_dir / name
                try:
                    shutil.copyfile(source, destination)
                except OSError as error:
                    output_artifacts[name] = {"source_path": source_path, "status": "COPY_FAILED", "error": str(error)}
                else:
                    output_artifacts[name] = {
                        "source_path": source_path,
                        "capture_path": str(destination),
                        "size": destination.stat().st_size,
                        "sha256": _digest(str(destination)),
                    }
            else:
                output_artifacts[name] = {"source_path": source_path, "status": "MISSING"}
    after = _snapshot(spec["inputs"])
    drift = before != after
    if timed_out:
        status = "PROCESS_TIMEOUT"
        cleanup = "NOT_ESTABLISHED_AFTER_FORCED_STOP"
    elif drift:
        status = "INPUT_DRIFT"
        cleanup = "REQUIRES_SAME_OWNER_EVIDENCE"
    elif launch_failed:
        status = "LAUNCH_FAILED"
        cleanup = "REQUIRES_SAME_OWNER_EVIDENCE"
    elif any(item.get("status") in {"MISSING", "COPY_FAILED"} for item in output_artifacts.values()):
        status = "OUTPUT_INCOMPLETE"
        cleanup = "REQUIRES_SAME_OWNER_EVIDENCE"
    elif returncode != 0:
        status = "PROCESS_FAILED"
        cleanup = "REQUIRES_SAME_OWNER_EVIDENCE"
    else:
        status = "PROCESS_CAPTURED"
        cleanup = "REQUIRES_SAME_OWNER_EVIDENCE"
    result = {
        "status": status, "returncode": returncode, "timed_out": timed_out,
        "duration_seconds": round(time.monotonic() - started, 6),
        "inputs_before": before, "inputs_after": after,
        "spec": spec, "cwd": spec.get("cwd"),
        "command": command,
        "logs": {name: hashlib.sha256((output / name).read_bytes()).hexdigest()
                  for name in ("stdout.bin", "stderr.bin")},
        "output_artifacts": output_artifacts,
        "production_acceptance": "NOT_PASSED", "native_cleanup": cleanup,
    }
    (output / "capture.json").write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--spec", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    try:
        spec = json.loads(Path(args.spec).read_text())
        result = capture(spec, args.output)
    except (OSError, ValueError) as error:
        print(str(error))
        return 2
    print(json.dumps(result, sort_keys=True))
    return 0 if result["status"] == "PROCESS_CAPTURED" else 1


if __name__ == "__main__":
    raise SystemExit(main())
