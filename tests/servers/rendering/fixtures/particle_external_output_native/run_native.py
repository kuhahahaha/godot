#!/usr/bin/env python3
"""Run one fixed, tests-only main-RD owned-prefix disposal fixture."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[4]
PASS = "PARTICLE_EXTERNAL_OUTPUT_NATIVE_PASS checks=27 buffers=7 bytes=916 no_next_draw=1 physical_prefix_returned=1 whole_output_released=0"
MISSING = "PARTICLE_EXTERNAL_OUTPUT_NATIVE_MISSING tests-only fixture class not registered"
CHECKS = {
    "owner",
    "render_loop_disabled",
    "outside_lists",
    "allocate",
    "live_counts",
    "live_bytes",
    "live_original_identities",
    "live_distinct_identities",
    "live_destroy_busy",
    "live_owner_retained",
    "retire",
    "enqueued_observe",
    "enqueued_counts",
    "enqueued_original_identities",
    "logical_owners_removed",
    "enqueued_destroy_busy",
    "enqueued_owner_retained",
    "repeat_retire",
    "repeat_observe",
    "repeat_unchanged",
    "maintenance_returned",
    "returned_observe",
    "returned_counts",
    "returned_original_identities",
    "no_next_draw",
    "destroy_returned",
    "destroy_pointer_empty",
}
SOURCES = (
    "modules/particles_runtime_tests/particles_external_output_test_fixture.h",
    "modules/particles_runtime_tests/particles_external_output_test_fixture.cpp",
    "modules/particles_runtime_tests/register_types.cpp",
    "modules/particles_runtime_tests/config.py",
    "servers/rendering/renderer_rd/storage_rd/external_output_storage.h",
    "servers/rendering/renderer_rd/storage_rd/external_output_storage.cpp",
    "servers/rendering/native_disposal_witness.h",
    "servers/rendering/rendering_device.h",
    "servers/rendering/rendering_device.cpp",
    "drivers/metal/rendering_device_driver_metal.cpp",
    "drivers/metal/rendering_device_driver_metal3.cpp",
    "main/main.cpp",
    "tests/servers/rendering/fixtures/particle_external_output_native/project.godot",
    "tests/servers/rendering/fixtures/particle_external_output_native/run.gd",
    "tests/servers/rendering/fixtures/particle_external_output_native/run_native.py",
    "tests/servers/rendering/fixtures/particle_external_output_native/README.md",
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def source_manifest() -> dict[str, dict[str, int | str]]:
    return {name: {"bytes": (ROOT / name).stat().st_size, "sha256": sha256(ROOT / name)} for name in SOURCES}


def valid_report(report: object, driver: str) -> bool:
    if not isinstance(report, dict):
        return False
    integer_keys = ("schema", "error", "maintenance_calls", "deadline_usec", "elapsed_usec")
    if any(type(report.get(key)) is not int for key in integer_keys):
        return False
    counts = report.get("counts")
    if not isinstance(counts, dict) or any(type(value) is not int for value in counts.values()):
        return False
    checks = report.get("checks")
    if not isinstance(checks, list) or len(checks) != 27:
        return False
    if not all(isinstance(row, dict) and isinstance(row.get("check"), str) and row.get("ok") is True for row in checks):
        return False
    labels = [row["check"] for row in checks]
    device = report.get("device")
    return (
        len(set(labels)) == 27
        and set(labels) == CHECKS
        and report.get("schema") == 1
        and report.get("started") is True
        and report.get("complete") is True
        and report.get("error") == 0
        and report.get("failures") == []
        and report.get("physical_prefix_returned") is True
        and report.get("counts") == {"created": 7, "live": 0, "enqueued": 0, "returned": 7, "unknown": 0, "bytes": 916}
        and type(report.get("maintenance_calls")) is int
        and 1 <= report["maintenance_calls"] <= 0xFFFFFFFF
        and report.get("deadline_usec") == 20000000
        and type(report.get("elapsed_usec")) is int
        and 0 <= report["elapsed_usec"] < 20000000
        and all(
            report.get(key) is False
            for key in (
                "retained_on_failure",
                "whole_output_released",
                "source_authority_supplied",
                "dispatch_recorded",
                "device_holder_claimed",
            )
        )
        and isinstance(device, dict)
        and device.get("driver") == driver
        and device.get("renderer") == "mobile"
        and bool(device.get("name"))
        and bool(device.get("api"))
    )


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
    sources = source_manifest()
    args.out.mkdir(parents=True, exist_ok=False)
    command = [str(host), "--path", str(HERE), "--render-thread", "safe", "--script", "run.gd"]
    command += (
        ["--headless"]
        if args.expect_missing_fixture
        else ["--rendering-driver", args.driver, "--rendering-method", "mobile"]
    )
    environment = dict(os.environ, ANGE_MACOS_BACKGROUND_RENDER="1")
    for key in tuple(environment):
        if key.startswith("GODOT_") or key.startswith("DYLD_"):
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
    sources_stable = sources == source_manifest()
    # A template binary without an editor-generated global script cache emits
    # this startup diagnostic before loading the fixture script. It does not
    # affect the renderer/retirement run; keep every other fatal diagnostic
    # strict and visible.
    benign_startup = "ERROR: Could not load global script cache."
    fatal_text = text.replace(benign_startup, "")
    fatal = bool(re.search(r"ERROR:|ASSERT|CrashHandler|leaked at exit|FIXTURE_FATAL|SCRIPT ERROR", fatal_text))
    native_report = None
    if args.expect_missing_fixture:
        passed = (
            returncode == 77
            and text.count(MISSING) == 1
            and PASS not in text
            and not fatal
            and stable
            and sources_stable
        )
    else:
        matches = re.findall(r"^PARTICLE_EXTERNAL_OUTPUT_NATIVE_RESULT=(.+)$", text, re.M)
        if len(matches) == 1:
            try:
                native_report = json.loads(matches[0])
            except json.JSONDecodeError:
                pass
        backend = "Metal" if args.driver == "metal" else "Vulkan"
        startup = re.findall(r"^(Metal|Vulkan) [0-9.]+ - Forward Mobile - Using Device #[0-9]+: .+$", text, re.M)
        passed = (
            returncode == 0
            and text.count(PASS) == 1
            and valid_report(native_report, args.driver)
            and startup == [backend]
            and not fatal
            and stable
            and sources_stable
        )
    result = {
        "command": command,
        "cwd": str(HERE),
        "host_sha256": args.sha256,
        "host_unchanged": stable,
        "sources_before": sources,
        "sources_unchanged": sources_stable,
        "source_identity_is_not_build_provenance": True,
        "process_timeout_seconds": 180,
        "returncode": returncode,
        "passed": passed,
        "registration_red_only": args.expect_missing_fixture,
        "physical_prefix_returned": passed and not args.expect_missing_fixture,
        "whole_output_released": False,
        "device_holder_claimed": False,
        "report": native_report,
        "stdout_sha256": sha256(args.out / "stdout.log"),
        "stderr_sha256": sha256(args.out / "stderr.log"),
    }
    (args.out / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
