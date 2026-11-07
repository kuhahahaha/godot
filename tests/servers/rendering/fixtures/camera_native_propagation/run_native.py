#!/usr/bin/env python3
"""Run real Metal Mobile and Forward+ camera propagation; fail closed."""

import argparse
import json
import os
import re
import struct
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
PREFIX = "CAMERA_NATIVE_PROPAGATION "
CHECKS = {
    "real_viewport_0",
    "unregistered_cache_null",
    "registered_1",
    "registered_2",
    "old_ref_retained",
    "new_ref_and_token",
    "old_payload_immutable",
    "invalidated_cache_null",
    "old_ref_survives_invalidate",
    "actual_viewport_freed",
    "old_ref_survives_free",
    "old_ref_released",
} | {
    f"{name}_{stage}"
    for name in (
        "real_viewport",
        "production_cache_nonnull",
        "actual_viewport",
        "complete_token",
        "presentation",
        "camera_payload",
    )
    for stage in (1, 2)
}


def valid_report(report, capture_dir):
    if not isinstance(report, dict) or report.get("pass") is not True or report.get("stage") != 4:
        return False
    checks = report.get("checks")
    if not isinstance(checks, dict) or not CHECKS <= checks.keys() or not all(v is True for v in checks.values()):
        return False
    if report.get("authority") != "test-source-only-no-view-grant-accepted-authority":
        return False
    images = report.get("images")
    if not isinstance(images, dict) or set(images) != {"unregistered", "first", "second"}:
        return False
    paths = set()
    for value in images.values():
        if not isinstance(value, dict) or not isinstance(value.get("path"), str):
            return False
        path = Path(value["path"])
        if not path.is_absolute() or capture_dir not in path.resolve().parents or not path.is_file():
            return False
        paths.add(path.resolve())
        with path.open("rb") as stream:
            header = stream.read(24)
        if not (
            header[:8] == b"\x89PNG\r\n\x1a\n"
            and header[12:16] == b"IHDR"
            and len(header) == 24
            and struct.unpack(">II", header[16:24]) == (128, 128)
        ):
            return False
        if not (
            value.get("saved") is True
            and value.get("width") == 128
            and value.get("height") == 128
            and type(value.get("nonuniform_pixels")) is int
            and 0 < value["nonuniform_pixels"] < 128 * 128
            and type(value.get("draw_calls")) is int
            and value["draw_calls"] > 0
            and isinstance(value.get("sha256"), str)
            and re.fullmatch(r"[0-9a-f]{64}", value["sha256"])
        ):
            return False
    return len(paths) == 3 and images["first"]["sha256"] != images["second"]["sha256"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--capture-dir", required=True, type=Path)
    parser.add_argument("--display-driver", default="macos")
    args = parser.parse_args()
    failures = []
    for method, renderer in (("mobile", "Forward Mobile"), ("forward_plus", "Forward+")):
        capture_dir = args.capture_dir.resolve() / method
        capture_dir.mkdir(parents=True, exist_ok=True)
        command = [
            str(args.binary.resolve()),
            "--display-driver",
            args.display_driver,
            "--rendering-driver",
            "metal",
            "--rendering-method",
            method,
            "--path",
            str(HERE),
            "--script",
            "res://run.gd",
        ]
        env = os.environ.copy()
        env["ANGE_CAMERA_NATIVE_CAPTURE_DIR"] = str(capture_dir)
        print("COMMAND " + json.dumps(command), flush=True)
        try:
            result = subprocess.run(
                command, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90
            )
        except subprocess.TimeoutExpired as exc:
            output = exc.stdout or b""
            if isinstance(output, bytes):
                output = output.decode(errors="replace")
            print(output, flush=True)
            failures.append(f"{method}: timeout")
            continue
        print(result.stdout, flush=True)
        (capture_dir / "run.log").write_text(result.stdout)
        startup = re.findall(
            rf"^Metal [0-9.]+ - {re.escape(renderer)} - Using Device #[0-9]+: .+$", result.stdout, re.MULTILINE
        )
        try:
            reports = [
                json.loads(line[len(PREFIX) :]) for line in result.stdout.splitlines() if line.startswith(PREFIX)
            ]
        except json.JSONDecodeError:
            reports = []
        passed = (
            result.returncode == 0
            and not re.search(r"\b(?:ERROR|WARNING):", result.stdout)
            and len(startup) == 1
            and len(reports) == 1
            and valid_report(reports[0], capture_dir)
        )
        print(
            "CAMERA_NATIVE_PROPAGATION_CASE "
            + json.dumps({
                "method": method,
                "renderer_device": startup,
                "exit_code": result.returncode,
                "pass": passed,
            }),
            flush=True,
        )
        if not passed:
            failures.append(f"{method}: failed process, diagnostics, real renderer, or required checks")
    if failures:
        raise SystemExit("\n".join(failures))
    print("CAMERA_NATIVE_PROPAGATION_MATRIX_PASS")


if __name__ == "__main__":
    main()
