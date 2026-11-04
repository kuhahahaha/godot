#!/usr/bin/env python3
"""Run the existing fixed native main-Metal particle retry/retirement fixture."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[4]
EXPECTED_ERRORS = Counter({
    "Failed to reset particle sub-emitter buffer.": 2,
    "Failed to update particle emission buffer.": 2,
    "Failed to update particle frame parameters buffer.": 6,
    "Failed to create particle fallback trail buffer.": 1,
})
EXPECTED_NATIVE_CHECKS = Counter({
    "actual production particle shader replaces sentinel on emitted particle": 1,
    "actual trail shader consumed original retried fallback": 1,
    "all fixed actual Metal workloads accepted": 1,
    "all six original async readbacks physically completed": 1,
    "async completion belongs to original main RD": 6,
    "bounded native resource observation": 22,
    "exact original buffer_update failure hit": 6,
    "exact upload boundary rejects simulation": 6,
    "failed original work remains queued": 4,
    "failed progress restores original speed scale": 4,
    "failed residual suffix restores original emitting state": 1,
    "failed upload records no simulation dispatch": 6,
    "failed upload retains original subemitter state": 6,
    "failed upload retains pending particle state": 6,
    "fixed-step witness includes a nonzero fractional suffix": 1,
    "frame delta committed to original frame": 1,
    "manual progress closes only after original success": 4,
    "only successful prefix decrements original retained progress": 4,
    "original allocation owner authenticated": 26,
    "original buffer allocation identity": 26,
    "original native physical return acknowledged": 22,
    "original native resource identity": 22,
    "original particle owner exists before close": 6,
    "original retained final frame reaches actual device buffer": 4,
    "original retry fixture allocation": 1,
    "original retry queue drains after recovery": 4,
    "original trail fallback allocation failure": 1,
    "original trail retry creates real Metal resources": 1,
    "original uniform owner exists": 20,
    "owner release is not physical driver retirement": 1,
    "owner release removes original buffer RID": 26,
    "particle GPU witness sentinel upload": 1,
    "preprocess ownership survives partial failure": 1,
    "production particle async readback queued": 1,
    "production particle readback exact bytes": 1,
    "progress failure follows one successful original step": 4,
    "progress fixture original buffers": 4,
    "progress upload readback exact bytes": 4,
    "retained progress async readback queued": 4,
    "retry commits original subemitter transition": 3,
    "retry consumes original pending state once": 3,
    "retry consumes retained suffix without replay or duplicate frame delta": 4,
    "retry preserves exact nonzero fractional frame remainder": 1,
    "retry records one original particle dispatch": 3,
    "same original request succeeds after fault removal": 3,
    "shared native observation coverage intact at begin": 1,
    "successful progress restores original speed scale": 4,
    "successful residual suffix closes emission": 1,
    "trail async readback queued": 1,
    "trail compute shader compiles on actual main RD": 1,
    "trail failure stops before uniform creation": 1,
    "trail native compute pipeline exists": 1,
    "trail native shader exists": 1,
    "trail original compute list opens": 1,
    "trail readback exact bytes": 1,
    "trail subsequent use retains original buffer and uniform": 1,
})
SOURCES = (
    "modules/particles_runtime_tests/particles_runtime_test_fixture.h",
    "modules/particles_runtime_tests/particles_runtime_test_fixture.cpp",
    "modules/particles_runtime_tests/config.py",
    "modules/particles_runtime_tests/register_types.cpp",
    "servers/rendering/render_capture_test_tracking.h",
    "servers/rendering/rendering_device.h",
    "servers/rendering/rendering_device.cpp",
    "servers/rendering/renderer_rd/storage_rd/particles_storage.h",
    "servers/rendering/renderer_rd/storage_rd/particles_storage.cpp",
    "tests/servers/rendering/fixtures/particle_runtime_native/probe.gd",
    "tests/servers/rendering/fixtures/particle_runtime_native/project.godot",
    "tests/servers/rendering/fixtures/particle_runtime_native/run_native.py",
    "tests/servers/rendering/fixtures/particle_runtime_native/README.md",
)


def sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def source_manifest() -> dict[str, dict[str, int | str]]:
    return {name: {"bytes": (ROOT / name).stat().st_size, "sha256": sha(ROOT / name)} for name in SOURCES}


def strict_json_loads(text: str) -> object:
    def unique_object(pairs: list[tuple[str, object]]) -> dict[str, object]:
        result: dict[str, object] = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"duplicate JSON object key: {key}")
            result[key] = value
        return result

    value: object = json.loads(text, object_pairs_hook=unique_object)
    return value


def valid_metadata(data: object, host_hash: str, host_bytes: int) -> bool:
    if not isinstance(data, dict):
        return False
    artifact, selection = data.get("artifact"), data.get("selection")
    return (
        type(data.get("schema")) is int
        and data["schema"] == 1
        and data.get("status") == "available"
        and data.get("reason") is None
        and isinstance(artifact, dict)
        and artifact.get("sha256") == host_hash
        and type(artifact.get("bytes")) is int
        and artifact["bytes"] == host_bytes
        and isinstance(selection, dict)
        and type(selection.get("schema")) is int
        and selection["schema"] == 1
        and selection.get("platform") == "macos"
        and selection.get("architecture") == "arm64"
        and selection.get("target_kind") == "Executable"
        and selection.get("compiler_route") == "BuiltinGlslangV1"
        and all(
            isinstance(selection.get(k), str) and re.fullmatch(r"[0-9a-f]{64}", selection[k])
            for k in ("core_stamp", "compiler_stamp")
        )
    )


def valid_report(report: object) -> bool:
    if not isinstance(report, dict):
        return False
    integers = {
        "advance_error": 0,
        "readbacks_queued": 6,
        "readbacks_completed": 6,
        "allocation_count": 26,
        "native_resource_count": 22,
    }
    if any(type(report.get(key)) is not int or report[key] != value for key, value in integers.items()):
        return False
    checks = report.get("checks")
    if not isinstance(checks, list) or len(checks) != 292:
        return False
    if not all(isinstance(row, dict) and isinstance(row.get("check"), str) and row.get("ok") is True for row in checks):
        return False
    return (
        Counter(row["check"] for row in checks) == EXPECTED_NATIVE_CHECKS
        and all(report.get(key) is True for key in ("gpu_pass", "done", "released"))
        and report.get("failures") == []
        and report.get("driver") == "metal"
        and report.get("renderer") == "mobile"
        and all(
            isinstance(report.get(key), str) and bool(report[key])
            for key in ("device_name", "device_vendor", "device_api")
        )
        and type(report.get("device_identity")) is int
        and 0 < report["device_identity"] <= 0xFFFFFFFFFFFFFFFF
        and type(report.get("frame_delay")) is int
        and 1 <= report["frame_delay"] <= 180
        and type(report.get("rendered_frames")) is int
        and 1 <= report["rendered_frames"] <= 180
    )


def evaluate(
    stdout: str, stderr: str, returncode: int, entry_negative: bool
) -> tuple[dict[str, bool], object, Counter[str]]:
    text = stdout + "\n" + stderr
    matches = re.findall(r"^PARTICLES_GPU_RESULT (\{[^\n]+\})$", text, re.M)
    report: object = None
    if len(matches) == 1:
        try:
            report = strict_json_loads(matches[0])
        except ValueError:
            pass
    diagnostics = Counter(re.findall(r"^ERROR: (.*)$", text, re.M))
    checks = {
        "one_result": len(matches) == 1,
        "not_timeout": returncode != 124,
        "no_fatal_or_exit_leaks": not re.search(
            r"Parse Error|SCRIPT ERROR|CrashHandler|Assertion|FATAL|leaked at exit|RID allocations.*leaked|ObjectDB instances leaked",
            text,
        ),
    }
    if entry_negative:
        checks["exact_entry_negative"] = returncode == 41 and report == {
            "unavailable": True,
            "reason": "tests_only_module_missing",
            "gpu_pass": False,
        }
        checks["no_engine_errors"] = not diagnostics and "ERROR:" not in text
    else:
        checks["exact_expected_engine_errors"] = diagnostics == EXPECTED_ERRORS and text.count("ERROR:") == 11
        checks["actual_metal_startup"] = (
            len(re.findall(r"^Metal [0-9.]+ - Forward Mobile - Using Device #[0-9]+: .+$", text, re.M)) == 1
        )
        checks["native_and_outer_success"] = returncode == 0 and valid_report(report)
    return checks, report, diagnostics


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--godot", type=Path, required=True)
    parser.add_argument("--sha256", required=True)
    parser.add_argument("--controlled-metadata", type=Path, required=True)
    parser.add_argument("--metadata-sha256", required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument(
        "--entry-negative", action="store_true", help="headless module-missing control only, never GPU success"
    )
    args = parser.parse_args()
    if not all(path.is_absolute() and path.is_file() for path in (args.godot, args.controlled_metadata)):
        parser.error("host and controlled metadata must be explicit existing absolute file paths")
    if not all(re.fullmatch(r"[0-9a-f]{64}", value) for value in (args.sha256, args.metadata_sha256)):
        parser.error("explicit lowercase SHA256 values are required for both inputs")
    host = args.godot.resolve()
    if host.is_relative_to((ROOT / "bin").resolve()):
        parser.error("use an immutable pinned host outside the in-progress build-tree bin directory")
    if sha(host) != args.sha256 or sha(args.controlled_metadata) != args.metadata_sha256:
        parser.error("pinned host or metadata SHA256 mismatch")
    try:
        metadata = strict_json_loads(args.controlled_metadata.read_text())
    except ValueError as error:
        parser.error(f"invalid controlled metadata JSON: {error}")
    if not valid_metadata(metadata, args.sha256, host.stat().st_size):
        parser.error(
            "controlled metadata must be available and match actual host hash/size and native macOS arm64 executable selection"
        )
    sources = source_manifest()
    args.out.mkdir(parents=True, exist_ok=False)
    (args.out / "controlled_build.json").write_bytes(args.controlled_metadata.read_bytes())
    command = [str(host), "--render-thread", "safe", "--path", str(HERE), "--script", str(HERE / "probe.gd")]
    command += (
        ["--headless"]
        if args.entry_negative
        else [
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
    )
    environment = {key: value for key, value in os.environ.items() if not key.startswith(("GODOT_", "DYLD_"))}
    environment["ANGE_MACOS_BACKGROUND_RENDER"] = "1"
    timeout_seconds = 90
    try:
        process = subprocess.run(command, cwd=ROOT, env=environment, capture_output=True, timeout=timeout_seconds)
        stdout, stderr, returncode = process.stdout, process.stderr, process.returncode
    except subprocess.TimeoutExpired as error:
        stdout, stderr, returncode = error.stdout or b"", error.stderr or b"", 124
    (args.out / "stdout.log").write_bytes(stdout)
    (args.out / "stderr.log").write_bytes(stderr)
    checks, report, diagnostics = evaluate(
        stdout.decode(errors="replace"), stderr.decode(errors="replace"), returncode, args.entry_negative
    )
    checks["host_and_metadata_unchanged"] = (
        sha(host) == args.sha256 and sha(args.controlled_metadata) == args.metadata_sha256
    )
    checks["source_unchanged"] = sources == source_manifest()
    result = {
        "scope": "existing fixed main-Metal native particle retry/readback/physical-return fixture only",
        "entry_negative_only": args.entry_negative,
        "gpu_pass": all(checks.values()) and not args.entry_negative,
        "holder_qualified": False,
        "ange_product_qualified": False,
        "whole_output_released": False,
        "command": command,
        "cwd": str(ROOT),
        "returncode": returncode,
        "timeout_seconds": timeout_seconds,
        "host": str(host),
        "host_sha256": args.sha256,
        "metadata_sha256": args.metadata_sha256,
        "source_manifest_is_not_build_provenance": True,
        "sources": sources,
        "engine_diagnostics": dict(diagnostics),
        "result": report,
        "checks": checks,
        "stdout_sha256": sha(args.out / "stdout.log"),
        "stderr_sha256": sha(args.out / "stderr.log"),
    }
    (args.out / "RESULT.json").write_text(json.dumps(result, indent=2) + "\n")
    files = {p.name: {"bytes": p.stat().st_size, "sha256": sha(p)} for p in args.out.iterdir() if p.is_file()}
    (args.out / "FILES.json").write_text(json.dumps(files, indent=2) + "\n")
    print(json.dumps({"out": str(args.out), "checks": checks, "gpu_pass": result["gpu_pass"]}, indent=2))
    return 0 if all(checks.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
