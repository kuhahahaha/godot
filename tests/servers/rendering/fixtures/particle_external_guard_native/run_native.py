#!/usr/bin/env python3
"""Run one fixed, tests-only main-RD guard preparation fixture."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
MODES = ("INIT", "VALIDATE", "FINALIZE", "INIT_EMPTY", "FINALIZE_EMPTY")
PASS = "PARTICLE_EXTERNAL_GUARD_NATIVE_PASS five_modes=1 checks=90 idempotent=1 owner_released=1 reprepare=1 physical_return_claimed=0"
MISSING = "PARTICLE_EXTERNAL_GUARD_NATIVE_MISSING tests-only fixture class not registered"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def expected_checks() -> set[str]:
    labels = {
        "original main RD render-thread owner",
        "outside all active command lists",
        "first.prepare",
        "repeat.prepare",
        "reprepare.prepare",
    }
    for phase in ("first", "reprepare"):
        labels.update(f"{phase}.{suffix}" for suffix in ("ready", "five_distinct_shaders", "five_distinct_pipelines"))
        labels.update(
            f"{phase}.{mode}.{suffix}"
            for mode in MODES
            for suffix in (
                "actual_shader",
                "actual_pipeline",
                "original_shader_association",
                "reflected_compute_layout",
            )
        )
    labels.update(f"repeat.{mode}.same_original_rids" for mode in MODES)
    for phase in ("first_release", "final_release"):
        labels.update(f"{phase}.{suffix}" for suffix in ("owner_release", "not_ready"))
        labels.update(
            f"{phase}.{mode}.{suffix}"
            for mode in MODES
            for suffix in ("getters_empty", "shader_owner_removed", "pipeline_owner_removed")
        )
    return labels


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--godot", type=Path, required=True)
    parser.add_argument("--sha256", required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--driver", choices=("metal", "vulkan"), default="metal")
    parser.add_argument(
        "--expect-missing-fixture", action="store_true", help="headless registration RED only; never GPU success"
    )
    args = parser.parse_args()
    if not args.godot.is_absolute() or not re.fullmatch(r"[0-9a-f]{64}", args.sha256):
        parser.error("--godot must be absolute and --sha256 must be the explicit lowercase host hash")
    host = args.godot.resolve(strict=True)
    if sha256(host) != args.sha256:
        parser.error("actual host SHA-256 mismatch")
    args.out.mkdir(parents=True, exist_ok=False)
    command = [str(host), "--path", str(HERE), "--render-thread", "safe", "--script", "run.gd"]
    command += (
        ["--headless"]
        if args.expect_missing_fixture
        else ["--rendering-driver", args.driver, "--rendering-method", "mobile"]
    )
    environment = dict(os.environ, ANGE_MACOS_BACKGROUND_RENDER="1")
    for key in tuple(environment):
        if key.startswith("GODOT_SYSTEM_SCALAR_TEST_") or key.startswith("DYLD_"):
            environment.pop(key, None)
    try:
        process = subprocess.run(command, cwd=HERE, env=environment, text=True, capture_output=True, timeout=180)
        stdout, stderr, returncode = process.stdout, process.stderr, process.returncode
    except subprocess.TimeoutExpired as timeout:

        def decode(value: bytes | str | None) -> str:
            return value.decode(errors="replace") if isinstance(value, bytes) else value or ""

        stdout, stderr, returncode = decode(timeout.stdout), decode(timeout.stderr), 124
    (args.out / "stdout.log").write_text(stdout)
    (args.out / "stderr.log").write_text(stderr)
    text = stdout + stderr
    stable = sha256(host) == args.sha256
    fatal = bool(re.search(r"ERROR:|ASSERT|CrashHandler|leaked at exit|FIXTURE_FATAL|SCRIPT ERROR", text))
    native_report = None
    if args.expect_missing_fixture:
        passed = returncode == 77 and text.count(MISSING) == 1 and PASS not in text and not fatal and stable
    else:
        matches = re.findall(r"^PARTICLE_EXTERNAL_GUARD_NATIVE_RESULT=(.+)$", text, re.M)
        valid_report = False
        if len(matches) == 1:
            native_report = json.loads(matches[0])
            checks = native_report.get("checks", [])
            labels = [row.get("check") for row in checks if isinstance(row, dict)]
            valid_report = (
                native_report.get("complete") is True
                and native_report.get("started") is True
                and native_report.get("error") == 0
                and native_report.get("mode_count") == 5
                and native_report.get("failures") == []
                and len(checks) == len(labels) == 90
                and set(labels) == expected_checks()
                and all(row.get("ok") is True for row in checks)
                and all(
                    native_report.get(key) is False
                    for key in ("physical_return_claimed", "dispatch_recorded", "source_authority_supplied")
                )
                and native_report.get("device", {}).get("driver") == args.driver
            )
        backend = "Metal" if args.driver == "metal" else "Vulkan"
        startup = re.findall(r"^(Metal|Vulkan) [0-9.]+ - Forward Mobile - Using Device #[0-9]+: .+$", text, re.M)
        passed = (
            returncode == 0 and text.count(PASS) == 1 and valid_report and startup == [backend] and not fatal and stable
        )
    result = {
        "command": command,
        "cwd": str(HERE),
        "host_sha256": args.sha256,
        "host_unchanged": stable,
        "process_timeout_seconds": 180,
        "returncode": returncode,
        "passed": passed,
        "registration_red_only": args.expect_missing_fixture,
        "gpu_preparation_pass": passed and not args.expect_missing_fixture,
        "physical_return_claimed": False,
        "report": native_report,
        "stdout_sha256": sha256(args.out / "stdout.log"),
        "stderr_sha256": sha256(args.out / "stderr.log"),
    }
    (args.out / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
