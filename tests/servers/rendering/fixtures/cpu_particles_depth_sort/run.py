#!/usr/bin/env python3
"""Run the fixed public CPU3D/MultiMesh depth-order regression on a pinned host."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import re
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[4]
CASES = ("translation", "rotation", "nonuniform_scale", "shear")
RED_FAILURES = ["translation", "nonuniform_scale", "shear"]
DEPTHS = {
    "translation": (-1.0, 0.0),
    "rotation": (-math.cos(0.6), -math.sin(0.6)),
    "nonuniform_scale": (0.25 * math.cos(0.7), 4 * math.sin(0.7)),
    "shear": (-1.0, 0.0),
}
SOURCES = (
    "scene/3d/cpu_particles_3d.cpp",
    "scene/3d/cpu_particles_3d.h",
    "tests/servers/rendering/fixtures/cpu_particles_depth_sort/probe.gd",
    "tests/servers/rendering/fixtures/cpu_particles_depth_sort/project.godot",
    "tests/servers/rendering/fixtures/cpu_particles_depth_sort/run.py",
)


def sha(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def manifest() -> dict:
    return {name: {"bytes": (ROOT / name).stat().st_size, "sha256": sha(ROOT / name)} for name in SOURCES}


def strict_json(text: str):
    def unique(pairs):
        result = {}
        for k, v in pairs:
            if k in result:
                raise ValueError("duplicate key: " + k)
            result[k] = v
        return result

    def invalid_constant(value):
        raise ValueError("non-finite JSON value: " + value)

    return json.loads(text, object_pairs_hook=unique, parse_constant=invalid_constant)


def evaluate(stdout: str, stderr: str, returncode: int, expect_red: bool) -> tuple[dict, object]:
    text = stdout + "\n" + stderr
    matches = re.findall(r"^CPU_PARTICLES_DEPTH_RESULT (\{[^\n]+\})$", stdout, re.M)
    report = None
    if len(matches) == 1:
        try:
            report = strict_json(matches[0])
        except ValueError:
            pass
    checks = {
        "one_result": len(matches) == 1 and text.count("CPU_PARTICLES_DEPTH_RESULT ") == 1,
        "expected_exit": type(returncode) is int and returncode == (43 if expect_red else 0),
        "not_timeout": returncode != 124,
        "no_engine_or_exit_errors": not re.search(
            r"ERROR:|SCRIPT ERROR|Parse Error|CrashHandler|Assertion|FATAL|leaked at exit|RID allocations.*leaked|ObjectDB instances leaked",
            text,
        ),
        "actual_metal_mobile_startup": len(
            re.findall(r"^Metal [0-9.]+ - Forward Mobile - Using Device #[0-9]+: .+$", text, re.M)
        )
        == 1,
        "report_shape": isinstance(report, dict),
    }
    if not isinstance(report, dict):
        return checks, report
    rows = report.get("cases")
    checks["exact_case_order"] = (
        isinstance(rows, list)
        and len(rows) == 4
        and all(isinstance(row, dict) for row in rows)
        and [row.get("case") for row in rows] == list(CASES)
    )
    checks["exact_failures"] = report.get("failures") == (RED_FAILURES if expect_red else [])
    checks["expected_pass_boolean"] = report.get("pass") is (not expect_red)
    if not checks["exact_case_order"]:
        return checks, report
    for row in rows:
        name = row["case"]
        expected_ok = not expect_red or name == "rotation"
        values = row.get("world_depths")
        finite = (
            isinstance(values, list)
            and len(values) == 64
            and all(type(x) in (int, float) and math.isfinite(x) for x in values)
        )
        checks[name + ":1280_floats_and_both_points"] = (
            row.get("exact_multimesh_bytes") is True and row.get("both_fixed_points_emitted") is True
        )
        checks[name + ":64_finite_world_depths"] = finite
        checks[name + ":expected_case_boolean"] = (
            row.get("ok") is expected_ok and row.get("world_depth_nondecreasing") is expected_ok
        )
        if finite:
            low, high = DEPTHS[name]
            matches_low = [abs(x - low) <= 1e-5 for x in values]
            matches_high = [abs(x - high) <= 1e-5 for x in values]
            checks[name + ":independent_expected_depth_values"] = (
                all(a or b for a, b in zip(matches_low, matches_high)) and any(matches_low) and any(matches_high)
            )
            monotonic = all(b + 1e-5 >= a for a, b in zip(values, values[1:]))
            checks[name + ":independent_monotonic_or_target_inversion"] = monotonic is expected_ok
    return checks, report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--godot", type=Path, required=True)
    parser.add_argument("--sha256", required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument(
        "--expect-red", action="store_true", help="require exact three sorting failures and passing rotation control"
    )
    args = parser.parse_args()
    host = args.godot
    if not host.is_absolute() or not host.is_file() or not re.fullmatch(r"[0-9a-f]{64}", args.sha256):
        parser.error("an absolute existing host and its explicit lowercase SHA256 are required")
    host = host.resolve()
    if host.is_relative_to((ROOT / "bin").resolve()):
        parser.error("pin the host outside the mutable build-tree bin directory")
    if sha(host) != args.sha256:
        parser.error("host SHA256 mismatch")
    before = manifest()
    args.out.mkdir(parents=True, exist_ok=False)
    command = [
        str(host),
        "--render-thread",
        "safe",
        "--path",
        str(HERE),
        "--script",
        str(HERE / "probe.gd"),
        "--rendering-driver",
        "metal",
        "--rendering-method",
        "mobile",
        "--resolution",
        "64x64",
        "--fixed-fps",
        "64",
        "--audio-driver",
        "Dummy",
    ]
    environment = {k: v for k, v in os.environ.items() if not k.startswith(("GODOT_", "DYLD_"))}
    environment["ANGE_MACOS_BACKGROUND_RENDER"] = "1"
    timeout = 90
    try:
        process = subprocess.run(command, cwd=ROOT, env=environment, capture_output=True, timeout=timeout)
        stdout, stderr, code = process.stdout, process.stderr, process.returncode
    except subprocess.TimeoutExpired as error:
        stdout, stderr, code = error.stdout or b"", error.stderr or b"", 124
    (args.out / "stdout.log").write_bytes(stdout)
    (args.out / "stderr.log").write_bytes(stderr)
    checks, report = evaluate(stdout.decode(errors="replace"), stderr.decode(errors="replace"), code, args.expect_red)
    checks["host_hash_unchanged"] = host.is_file() and sha(host) == args.sha256
    try:
        checks["input_hashes_unchanged"] = manifest() == before
    except OSError:
        checks["input_hashes_unchanged"] = False
    result = {
        "validation_pass": all(checks.values()),
        "expect_red": args.expect_red,
        "returncode": code,
        "timeout_seconds": timeout,
        "command": command,
        "cwd": str(ROOT),
        "host": str(host),
        "host_sha256": args.sha256,
        "sources": before,
        "source_manifest_is_not_build_provenance": True,
        "checks": checks,
        "result": report,
        "stdout_sha256": sha(args.out / "stdout.log"),
        "stderr_sha256": sha(args.out / "stderr.log"),
        "scope": "actual public CPU3D producer and native MultiMesh depth order; no pixel or product qualification",
    }
    (args.out / "RESULT.json").write_text(json.dumps(result, indent=2) + "\n")
    files = {p.name: {"bytes": p.stat().st_size, "sha256": sha(p)} for p in args.out.iterdir() if p.is_file()}
    (args.out / "FILES.json").write_text(json.dumps(files, indent=2) + "\n")
    print(
        json.dumps({
            "validation_pass": result["validation_pass"],
            "expect_red": args.expect_red,
            "failed_checks": [k for k, v in checks.items() if not v],
        })
    )
    return 0 if result["validation_pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
