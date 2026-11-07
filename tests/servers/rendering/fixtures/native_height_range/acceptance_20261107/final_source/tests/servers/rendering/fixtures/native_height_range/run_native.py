#!/usr/bin/env python3
"""Verify real Metal clear ranges on bounded and ordinary original RD paths."""

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
CHECKS = {
    "cleanup_continuation_settled",
    "range_return_is_not_physical_return",
    "first_range_returned",
    "quote_issued",
    "retire_depth",
    "loop_disabled_for_setup",
    "quoted_depth_created",
    "correct_issue_path",
    "loop_disabled_for_return",
    "quote_observed",
    "both_physical_returned",
    "native_ranges_complete",
    "ranges_closed",
    "closed_not_complete",
    "framebuffer_enqueued_not_returned",
    "main_rd_rt",
    "pair_live_before_issue",
    "retire_framebuffer",
    "identities_preserved",
    "no_producer_claim",
    "ranges_prepared",
    "depth_enqueued_not_returned",
    "framebuffer_created",
    "fixed_case",
    "second_range_still_complete",
    "standalone_ready",
    "ranges_recorded",
    "completion_is_not_physical_return",
    "framebuffer_returned_depth_live",
    "initial_clear_completed",
    "native_complete_after_return",
    "pair_captured",
    "second_range_returned",
    "initial_clear_recorded",
}
SOURCES = (
    "modules/particles_runtime_tests/native_height_range_test_fixture.h",
    "modules/particles_runtime_tests/native_height_range_test_fixture.cpp",
    "modules/particles_runtime_tests/register_types.cpp",
    "modules/particles_runtime_tests/config.py",
    "modules/particles_runtime_tests/SCsub",
    "servers/rendering/native_height_range.h",
    "servers/rendering/native_depth_allocation_projection.h",
    "servers/rendering/native_disposal_witness.h",
    "servers/rendering/rendering_device.h",
    "servers/rendering/rendering_device.cpp",
    "servers/rendering/rendering_device_driver.h",
    "servers/rendering/rendering_device_graph.h",
    "servers/rendering/rendering_device_graph.cpp",
    "drivers/metal/metal_allocator.h",
    "drivers/metal/metal_allocator.cpp",
    "drivers/metal/rendering_device_driver_metal.h",
    "drivers/metal/rendering_device_driver_metal.cpp",
    "main/main.cpp",
    "tests/servers/rendering/fixtures/native_height_range/project.godot",
    "tests/servers/rendering/fixtures/native_height_range/run.gd",
    "tests/servers/rendering/fixtures/native_height_range/run_native.py",
    "tests/servers/rendering/fixtures/native_height_range/README.md",
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def source_manifest() -> dict[str, dict[str, int | str]]:
    return {name: {"bytes": (ROOT / name).stat().st_size, "sha256": sha256(ROOT / name)} for name in SOURCES}


def positive_int(value: object) -> bool:
    return type(value) is int and value > 0


ERROR_FIELDS = ("append_error", "mapping_error", "issue_error", "retention_error", "completion_error", "return_error")
COUNT_FIELDS = ("graph_members", "sealed_members", "issued_sources", "retained_sources", "completed_sources")
BOOL_FIELDS = (
    "recording_closed",
    "has_recorded_effect",
    "mapping_closed",
    "issue_closed",
    "native_complete",
    "observers_returned",
)


def valid_range(row: object) -> bool:
    return (
        isinstance(row, dict)
        and all(type(row.get(k)) is int and row[k] >= 0 for k in COUNT_FIELDS)
        and all(type(row.get(k)) is int and row[k] == 0 for k in ERROR_FIELDS)
        and all(type(row.get(k)) is bool for k in BOOL_FIELDS)
        and type(row.get("observe_error")) is int
        and row["observe_error"] in (0, 44)
    )


def _valid_report(report: object, case_index: int) -> bool:
    if not isinstance(report, dict):
        return False
    if any(
        type(report.get(k)) is not int
        for k in (
            "schema",
            "error",
            "case_index",
            "phase",
            "callbacks",
            "maintenance_calls",
            "range_maintenance_calls",
            "observe_polls",
            "observer_releases",
            "elapsed_usec",
            "deadline_usec",
            "recorded_frame",
            "draws_before_range_issue",
            "draws_at_native_complete",
            "draws_at_physical_return",
            "texture_reservation_bytes",
            "cleanup_recording_error",
        )
    ):
        return False
    if not (
        report["schema"] == 1
        and report["error"] == 0
        and report["case_index"] == case_index
        and report["phase"] == 7
        and report["path"] == ("bounded" if case_index == 0 else "ordinary")
    ):
        return False
    if (
        report.get("cleanup_continuation_active") is not False
        or type(report.get("cleanup_recording_error")) is not int
        or report["cleanup_recording_error"] != 0
    ):
        return False
    if any(
        report.get(k) is not True for k in ("started", "complete", "physical_prefix_returned", "independent_ranges")
    ):
        return False
    if any(
        report.get(k) is not False
        for k in (
            "retained_on_failure",
            "producer_complete",
            "whole_output_released",
            "reservation_refunded",
            "pending_clear_before_range",
        )
    ):
        return False
    if not (
        report.get("failures") == []
        and (report["callbacks"] == 1 if case_index == 0 else report["callbacks"] >= 3)
        and report["maintenance_calls"] > 0
        and report["observe_polls"] >= 2
        and report["observer_releases"] >= 2
        and report["texture_reservation_bytes"] > 0
        and report["recorded_frame"] >= 0
        and report["deadline_usec"] == 40_000_000
        and 0 <= report["elapsed_usec"] < report["deadline_usec"]
    ):
        return False
    before, completed, returned = (
        report[k] for k in ("draws_before_range_issue", "draws_at_native_complete", "draws_at_physical_return")
    )
    if case_index == 0:
        if before != completed or returned != before or report["range_maintenance_calls"] <= 0:
            return False
    elif completed <= before or returned < completed or report["range_maintenance_calls"] != 0:
        return False
    checks = report.get("checks")
    if (
        not isinstance(checks, list)
        or len(checks) != len(CHECKS)
        or any(
            not isinstance(r, dict) or r.get("ok") is not True or not isinstance(r.get("check"), str) for r in checks
        )
        or {r["check"] for r in checks} != CHECKS
    ):
        return False
    device = report.get("device")
    if (
        not isinstance(device, dict)
        or device.get("driver") != "metal"
        or device.get("renderer") != "mobile"
        or not device.get("name")
        or not device.get("api")
    ):
        return False
    stages = report.get("ranges")
    if not isinstance(stages, dict) or set(stages) != {
        "closed",
        "poll",
        "completed",
        "first_returned",
        "both_returned",
        "physical_returned",
    }:
        return False
    for stage, rows in stages.items():
        if not isinstance(rows, list) or len(rows) != 2 or not all(valid_range(row) for row in rows):
            return False
        for index, row in enumerate(rows):
            if (
                not row["recording_closed"]
                or not row["has_recorded_effect"]
                or row["graph_members"] <= 0
                or row["sealed_members"] > row["graph_members"]
                or row["completed_sources"] > row["retained_sources"]
                or row["retained_sources"] > row["issued_sources"]
            ):
                return False
            if row["graph_members"] != stages["closed"][index]["graph_members"]:
                return False
            if stage == "closed":
                if (
                    row["native_complete"]
                    or row["observers_returned"]
                    or row["issued_sources"] != 0
                    or row["sealed_members"] != 0
                ):
                    return False
            elif stage != "poll":
                if (
                    not row["mapping_closed"]
                    or not row["issue_closed"]
                    or not row["native_complete"]
                    or row["observe_error"] != 0
                    or row["sealed_members"] != row["graph_members"]
                    or not row["issued_sources"] == row["retained_sources"] == row["completed_sources"] > 0
                ):
                    return False
                expected_return = stage in ("both_returned", "physical_returned") or (
                    stage == "first_returned" and index == 0
                )
                if row["observers_returned"] is not expected_return:
                    return False
    physical = report.get("physical")
    expected = {
        "recorded": (1, 1, False),
        "native_complete": (1, 1, False),
        "observers_returned": (1, 1, False),
        "framebuffer_enqueued": (1, 2, False),
        "framebuffer_returned": (1, 3, False),
        "depth_enqueued": (2, 3, False),
        "returned": (3, 3, True),
    }
    if not isinstance(physical, dict) or set(physical) != set(expected):
        return False
    identities: dict[str, tuple[int, int]] = {}
    for stage, (depth_state, framebuffer_state, both) in expected.items():
        row = physical[stage]
        if (
            not isinstance(row, dict)
            or type(row.get("observe_error")) is not int
            or row["observe_error"] != 0
            or row.get("all_created_children_returned") is not both
        ):
            return False
        for key, state in (("depth", depth_state), ("framebuffer", framebuffer_state)):
            witness = row.get(key)
            if (
                not isinstance(witness, dict)
                or type(witness.get("state")) is not int
                or witness["state"] != state
                or not positive_int(witness.get("rid"))
                or not positive_int(witness.get("serial"))
            ):
                return False
            identity = (witness["rid"], witness["serial"])
            if key in identities and identities[key] != identity:
                return False
            identities[key] = identity
    return (
        identities["depth"][0] != identities["framebuffer"][0]
        and identities["depth"][1] != identities["framebuffer"][1]
    )


def valid_report(report: object, case_index: int) -> bool:
    try:
        return _valid_report(report, case_index)
    except (KeyError, TypeError, IndexError, ValueError):
        return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--godot", required=True, type=Path)
    parser.add_argument("--sha256", required=True)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    if not args.godot.is_absolute() or not re.fullmatch(r"[0-9a-f]{64}", args.sha256):
        parser.error("explicit absolute --godot and lowercase --sha256 required")
    host = args.godot.resolve(strict=True)
    if sha256(host) != args.sha256:
        parser.error("host SHA-256 mismatch")
    args.out.mkdir(parents=True, exist_ok=False)
    sources = source_manifest()
    environment = dict(os.environ, ANGE_MACOS_BACKGROUND_RENDER="1")
    for key in tuple(environment):
        if key.startswith(("GODOT_", "DYLD_")):
            environment.pop(key, None)
    results = []
    for case_index in (0, 1):
        directory = args.out / str(case_index)
        directory.mkdir()
        command = [
            str(host),
            "--path",
            str(HERE),
            "--render-thread",
            "safe",
            "--rendering-driver",
            "metal",
            "--rendering-method",
            "mobile",
            "--script",
            "run.gd",
            "--",
            f"--case={case_index}",
        ]
        try:
            process = subprocess.run(command, cwd=HERE, env=environment, capture_output=True, text=True, timeout=90)
            stdout, stderr, returncode = process.stdout, process.stderr, process.returncode
        except subprocess.TimeoutExpired as timeout:

            def decode(value: bytes | str | None) -> str:
                return value.decode(errors="replace") if isinstance(value, bytes) else value or ""

            stdout, stderr, returncode = decode(timeout.stdout), decode(timeout.stderr), 124
        (directory / "stdout.log").write_text(stdout)
        (directory / "stderr.log").write_text(stderr)
        text = stdout + stderr
        matches = re.findall(r"^NATIVE_HEIGHT_RANGE_RESULT=(.+)$", text, re.M)
        report = None
        if len(matches) == 1:
            try:
                report = json.loads(matches[0])
            except json.JSONDecodeError:
                pass
        startup = re.findall(r"^Metal [0-9.]+ - Forward Mobile - Using Device #[0-9]+: .+$", text, re.M)
        fatal = bool(re.search(r"ERROR:|WARNING:|ASSERT|CrashHandler|leaked at exit|SCRIPT ERROR", text))
        passed = (
            returncode == 0
            and len(startup) == 1
            and not fatal
            and text.count(f"NATIVE_HEIGHT_RANGE_PASS case={case_index}") == 1
            and valid_report(report, case_index)
        )
        results.append({
            "case_index": case_index,
            "command": command,
            "returncode": returncode,
            "passed": passed,
            "typed_unavailable": returncode == 77,
            "report": report,
            "stdout_sha256": sha256(directory / "stdout.log"),
            "stderr_sha256": sha256(directory / "stderr.log"),
        })
    stable = sha256(host) == args.sha256 and sources == source_manifest()
    result = {
        "passed": stable and all(row["passed"] for row in results),
        "host_sha256": args.sha256,
        "sources_before": sources,
        "host_and_sources_unchanged": stable,
        "source_identity_is_not_build_provenance": True,
        "cases": results,
    }
    (args.out / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
