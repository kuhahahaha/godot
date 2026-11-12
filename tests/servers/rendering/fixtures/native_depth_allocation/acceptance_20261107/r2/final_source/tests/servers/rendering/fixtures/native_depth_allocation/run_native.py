#!/usr/bin/env python3
"""Verify original Metal quotes, actual allocations and physical child returns."""

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
    "main_rd_rt",
    "disabled_outside_lists",
    "fixed_case",
    "quote_zero_extent_rejected",
    "quote_over_limit_rejected",
    "quote_issued",
    "quote_observed",
    "quote_descriptor_policy",
    "quote_native_bytes",
    "quote_scope_unknown",
    "quoted_create_verified",
    "actual_depth_within_quote",
    "framebuffer_created",
    "pair_captured",
    "live_original_children",
    "shared_attachment_identity",
    "unknown_overhead_preserved",
    "depth_clear_recorded",
    "original_driver_framebuffer_materialized",
    "retire_framebuffer",
    "framebuffer_enqueued_not_returned",
    "only_framebuffer_driver_returned",
    "half_returned_original_identities",
    "retire_depth",
    "depth_enqueued_not_returned",
    "both_driver_returned",
    "returned_original_identities_bytes",
    "repeat_retire",
    "no_next_draw",
}
SOURCES = (
    "modules/particles_runtime_tests/native_depth_allocation_test_fixture.h",
    "modules/particles_runtime_tests/native_depth_allocation_test_fixture.cpp",
    "modules/particles_runtime_tests/register_types.cpp",
    "modules/particles_runtime_tests/config.py",
    "modules/particles_runtime_tests/SCsub",
    "servers/rendering/native_depth_allocation_projection.h",
    "servers/rendering/native_disposal_witness.h",
    "servers/rendering/rendering_device.h",
    "servers/rendering/rendering_device.cpp",
    "servers/rendering/rendering_device_driver.h",
    "drivers/metal/metal_allocator.h",
    "drivers/metal/metal_allocator.cpp",
    "drivers/metal/rendering_device_driver_metal.h",
    "drivers/metal/rendering_device_driver_metal.cpp",
    "main/main.cpp",
) + tuple(
    f"tests/servers/rendering/fixtures/native_depth_allocation/{name}"
    for name in (
        "project.godot",
        "run.gd",
        "run_native.py",
        "README.md",
    )
)
UNKNOWN = {"knowledge": "UnknownNotExposed"}
STAGES = {
    "live": (1, 1, False),
    "framebuffer_enqueued": (1, 2, False),
    "framebuffer_returned": (1, 3, False),
    "depth_enqueued": (2, 3, False),
    "returned": (3, 3, True),
}


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


def known_bytes(value: object) -> bool:
    return (
        isinstance(value, dict)
        and set(value) == {"knowledge", "bytes"}
        and value["knowledge"] == "Known"
        and positive_int(value["bytes"])
    )


def valid_report(report: object, case_index: int) -> bool:
    if not isinstance(report, dict):
        return False
    if any(
        type(report.get(key)) is not int
        for key in (
            "schema",
            "error",
            "case_index",
            "create_error",
            "create_outcome",
            "maintenance_calls",
            "elapsed_usec",
            "deadline_usec",
        )
    ):
        return False
    if any(
        report.get(key) is not False
        for key in (
            "retained_on_failure",
            "producer_complete",
            "whole_output_released",
            "reservation_refunded",
        )
    ):
        return False
    if not (
        report["schema"] == 1
        and report["error"] == 0
        and report["case_index"] == case_index
        and report["create_error"] == 0
        and report["create_outcome"] == 1
        and report.get("started") is True
        and report.get("complete") is True
        and report.get("physical_prefix_returned") is True
        and report.get("failures") == []
        and report["maintenance_calls"] > 0
        and report["deadline_usec"] == 20_000_000
        and 0 <= report["elapsed_usec"] < report["deadline_usec"]
    ):
        return False
    checks = report.get("checks")
    if not isinstance(checks, list) or len(checks) != len(CHECKS):
        return False
    if any(
        not isinstance(row, dict) or row.get("ok") is not True or not isinstance(row.get("check"), str)
        for row in checks
    ):
        return False
    if {row["check"] for row in checks} != CHECKS:
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
    boundaries = report.get("boundaries")
    if not isinstance(boundaries, dict) or set(boundaries) != {
        "zero_width",
        "zero_height",
        "over_width",
        "over_height",
        "maximum_texture_size_2d",
    }:
        return False
    maximum = boundaries["maximum_texture_size_2d"]
    if not positive_int(maximum) or maximum >= 0xFFFFFFFF:
        return False
    for name, width, height in (
        ("zero_width", 0, 64),
        ("zero_height", 64, 0),
        ("over_width", maximum + 1, 1),
        ("over_height", 1, maximum + 1),
    ):
        row = boundaries[name]
        if (
            not isinstance(row, dict)
            or row.get("width") != width
            or row.get("height") != height
            or type(row.get("error")) is not int
            or row["error"] != 31
            or type(row.get("observe_error")) is not int
            or row["observe_error"] != 4
            or row.get("quote_empty") is not True
            or row.get("original_counters_unchanged") is not True
        ):
            return False
    instances = report.get("native_framebuffer_instances")
    if (
        not isinstance(instances, list)
        or len(instances) != 1
        or not isinstance(instances[0], dict)
        or not positive_int(instances[0].get("framebuffer"))
        or not positive_int(instances[0].get("render_pass"))
    ):
        return False
    quote = report.get("quote")
    if not isinstance(quote, dict):
        return False
    width, height, policy = (64, 64, 1) if case_index == 0 else (8192, 4096, 2)
    if any(
        type(quote.get(key)) is not int
        for key in ("width", "height", "policy", "alignment", "allocations", "parts", "shared_attachment_references")
    ):
        return False
    if not (
        quote["width"] == width
        and quote["height"] == height
        and quote["policy"] == policy
        and quote["allocations"] == 2
        and quote["parts"] == 1
        and quote["shared_attachment_references"] == 1
        and positive_int(quote["alignment"])
        and quote["alignment"] & (quote["alignment"] - 1) == 0
        and quote.get("framebuffer_owner_reservation") == {"knowledge": "Known", "bytes": 0}
        and quote.get("framebuffer_driver_backing") == UNKNOWN
        and quote.get("opaque_driver_overhead") == UNKNOWN
        and known_bytes(quote.get("texture_native_requirement"))
        and known_bytes(quote.get("texture_owner_reservation"))
    ):
        return False
    if quote["texture_native_requirement"]["bytes"] > quote["texture_owner_reservation"]["bytes"]:
        return False
    snapshots = report.get("snapshots")
    if not isinstance(snapshots, dict) or set(snapshots) != set(STAGES):
        return False
    identities: dict[str, tuple[int, int]] = {}
    actual = None
    for name, (depth_state, fb_state, all_returned) in STAGES.items():
        snapshot = snapshots[name]
        if (
            not isinstance(snapshot, dict)
            or snapshot.get("framebuffer_created") is not True
            or snapshot.get("all_created_children_returned") is not all_returned
        ):
            return False
        for child, state in (("depth", depth_state), ("framebuffer", fb_state)):
            witness = snapshot.get(child)
            if (
                not isinstance(witness, dict)
                or type(witness.get("state")) is not int
                or witness["state"] != state
                or not positive_int(witness.get("rid"))
                or not positive_int(witness.get("serial"))
            ):
                return False
            identity = (witness["rid"], witness["serial"])
            if child in identities and identities[child] != identity:
                return False
            identities[child] = identity
        if (
            identities["depth"][0] == identities["framebuffer"][0]
            or identities["depth"][1] == identities["framebuffer"][1]
        ):
            return False
        if snapshot.get("shared_attachment_depth") != snapshot["depth"]:
            return False
        if any(
            snapshot.get(key) != UNKNOWN
            for key in (
                "framebuffer_driver_backing",
                "framebuffer_object",
                "attachment_reference",
                "opaque_driver_overhead",
            )
        ):
            return False
        backing = snapshot.get("depth_driver_backing")
        if (
            not isinstance(backing, dict)
            or not known_bytes(backing)
            or backing["bytes"] > quote["texture_owner_reservation"]["bytes"]
        ):
            return False
        if actual is not None and actual != backing["bytes"]:
            return False
        actual = backing["bytes"]
    return True


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
        matches = re.findall(r"^NATIVE_DEPTH_ALLOCATION_RESULT=(.+)$", text, re.M)
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
            and text.count(f"NATIVE_DEPTH_ALLOCATION_PASS case={case_index}") == 1
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
