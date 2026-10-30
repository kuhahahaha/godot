#!/usr/bin/env python3
"""Build a standalone godot-cpp oracle and run real main-RD texture readback."""

import argparse
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path


def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as f:
        for part in iter(lambda: f.read(1048576), b""):
            h.update(part)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--godot", required=True, type=Path)
    ap.add_argument("--sdk", required=True, type=Path)
    ap.add_argument("--output", required=True, type=Path)
    ap.add_argument("--engine-receipt", required=True, type=Path)
    ap.add_argument("--precision", choices=["single", "double"], default="single")
    ap.add_argument("--sdk-receipt", required=True, type=Path)
    ap.add_argument("--api-receipt", required=True, type=Path)
    ap.add_argument("--core", required=True, type=Path, help="Godot source worktree matching the engine receipt")
    a = ap.parse_args()
    fixture = Path(__file__).resolve().parent
    core = a.core.resolve() / "scene/resources/resource_snapshot_bridge.h"
    sdk = a.sdk.resolve()
    archive = sdk / (
        "bin/libgodot-cpp.macos.template_debug.double.arm64.a"
        if a.precision == "double"
        else "bin/libgodot-cpp.macos.template_debug.arm64.a"
    )
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    project = out / "project"
    (project / ".godot").mkdir(parents=True)
    for name in ("image_probe.cc", "image_probe.gd"):
        shutil.copy2(fixture / name, project / name)
    shutil.copy2(core, project / core.name)
    (project / "project.godot").write_text(
        '[application]\nconfig/name="E22NativeImageOracle"\n[display]\nwindow/size/viewport_width=96\nwindow/size/viewport_height=96\n'
    )
    (project / ".godot/global_script_class_cache.cfg").write_text("list=[]\n")
    (project / "probe.gdextension").write_text(
        '[configuration]\nentry_symbol="texture_snapshot_image_init"\ncompatibility_minimum="4.7"\n[libraries]\nmacos.arm64="res://probe.dylib"\n'
    )
    expected = {
        "real_bytes": 8 if a.precision == "double" else 4,
        "cases": 270,
        "checks": 372870,
        "failures": 0,
        "renderers": ["mobile", "forward_plus"],
        "scope": "real native GradientTexture1D upload/readback comparison; not ANGE owner integration or Ready",
    }
    (out / "EXPECTED.json").write_text(json.dumps(expected, indent=2) + "\n")
    engine_receipt = json.loads(a.engine_receipt.read_text())
    if (
        engine_receipt["binary_sha256"] != digest(a.godot)
        or not engine_receipt["source_stable"]
        or engine_receipt["source_before"] != engine_receipt["source_after"]
        or engine_receipt["build_rc"] != 0
    ):
        raise ValueError("engine receipt does not match a stable successful build")
    for path, value in engine_receipt["source_after"].items():
        if digest(path) != value:
            raise ValueError("engine receipt source changed: " + path)
    inputs = [
        a.godot.resolve(),
        a.engine_receipt.resolve(),
        archive,
        fixture / "image_probe.cc",
        fixture / "image_probe.gd",
        Path(__file__).resolve(),
        core,
        out / "EXPECTED.json",
    ]
    if a.sdk_receipt and a.api_receipt:
        sdk_receipt = json.loads(a.sdk_receipt.read_text())
        api_receipt = json.loads(a.api_receipt.read_text())
        if (
            sdk_receipt["rc"] != 0
            or sdk_receipt["archive_sha256"] != digest(archive)
            or "precision=" + a.precision not in sdk_receipt["command"]
        ):
            raise ValueError("SDK archive does not match its successful build receipt")
        if (
            api_receipt["rc"] != 0
            or not api_receipt["stable"]
            or api_receipt["engine_binary_sha256_before"] != digest(a.godot)
            or api_receipt["engine_binary_sha256_after"] != digest(a.godot)
            or api_receipt["header"]["precision"] != a.precision
            or api_receipt["api_sha256"] != sdk_receipt["api_sha256"]
            or api_receipt["api_sha256"] != digest(Path(api_receipt["api"]))
        ):
            raise ValueError("SDK API dump does not match this engine precision")
        inputs.extend([a.sdk_receipt.resolve(), a.api_receipt.resolve(), Path(api_receipt["api"])])
    before = {str(p): digest(p) for p in inputs}
    sdk_headers = sorted([
        *(sdk / "include").rglob("*.h"),
        *(sdk / "include").rglob("*.hpp"),
        *(sdk / "gen/include").rglob("*.h"),
        *(sdk / "gen/include").rglob("*.hpp"),
    ])
    headers_before = {str(p): digest(p) for p in sdk_headers}
    cmd = [
        "xcrun",
        "clang++",
        "-std=c++17",
        "-arch",
        "arm64",
        "-dynamiclib",
        "-fPIC",
        "-O1",
        "-DDEBUG_ENABLED",
        "-DMACOS_ENABLED",
        "-DUNIX_ENABLED",
        "-MMD",
        "-MF",
        str(out / "compile.d"),
        "-I" + str(project),
        "-I" + str(sdk / "gen/include"),
        "-I" + str(sdk / "include"),
        str(project / "image_probe.cc"),
        str(archive),
        "-o",
        str(project / "probe.dylib"),
    ]
    if a.precision == "double":
        cmd.insert(2, "-DREAL_T_IS_DOUBLE")
    try:
        built = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        build_rc, build_log = built.returncode, built.stdout + built.stderr
    except subprocess.TimeoutExpired as e:
        build_rc = "timeout"
        build_log = (e.stdout or b"").decode() + (e.stderr or b"").decode()
    (out / "COMPILE.log").write_text(build_log)
    result = {
        "expected": expected,
        "engine_build_receipt": engine_receipt,
        "inputs_before": before,
        "compile_command": cmd,
        "compile_rc": build_rc,
        "runs": [],
        "sdk_headers_before": headers_before,
    }
    if build_rc == 0:
        result["extension_sha256"] = digest(project / "probe.dylib")
        deps = (out / "compile.d").read_text().replace("\\n", " ").split(":", 1)[1].split()
        result["compiled_header_inputs"] = {p: digest(p) for p in deps if Path(p).is_file()}
        result["compiler_version"] = subprocess.run(
            ["xcrun", "clang++", "--version"], capture_output=True, text=True
        ).stdout
        for renderer in expected["renderers"]:
            cmd = [
                str(a.godot.resolve()),
                "--path",
                str(project),
                "--rendering-driver",
                "metal",
                "--rendering-method",
                renderer,
                "--render-thread",
                "safe",
                "--audio-driver",
                "Dummy",
                "--script",
                "res://image_probe.gd",
            ]
            try:
                run = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
                log = run.stdout + run.stderr
                rc = run.returncode
            except subprocess.TimeoutExpired as e:
                log = (e.stdout or b"").decode() + (e.stderr or b"").decode()
                rc = "timeout"
            (out / (renderer + ".log")).write_text(log)
            markers = re.findall(r"^E22_GRADIENT_RESULT cases=(\d+) checks=(\d+) failures=(\d+)$", log, re.M)
            observed = tuple(map(int, markers[0])) if len(markers) == 1 else None
            errors = any(
                word in log
                for word in [
                    "ERROR:",
                    "SCRIPT ERROR:",
                    "WARNING:",
                    "E22_GRADIENT_FAILURE",
                    "UNAVAILABLE",
                    "Leaked",
                    "leaked",
                ]
            )
            case_lines = re.findall(
                r"^E22_GRADIENT_CASE width=(\d+) profile=(\d+) mode=(\d+) space=(\d+) hdr=(\d+) points=(\d+) status=(\d+)$",
                log,
                re.M,
            )
            actual_cases = {tuple(map(int, case)) for case in case_lines}
            expected_cases = {
                (width, profile, mode, space, hdr, (0, 1, 3, 6, 4096)[profile], 0)
                for width in (2, 33, 4096)
                for profile in range(5)
                for mode in range(3)
                for space in range(3)
                for hdr in range(2)
            }
            actual_renderer = "Forward Mobile" if renderer == "mobile" else "Forward+"
            passed = (
                rc == 0
                and observed == (270, 372870, 0)
                and len(case_lines) == 270
                and actual_cases == expected_cases
                and "Metal" in log
                and actual_renderer in log
                and "E22_GRADIENT_RUNTIME editor_hint=false" in log
                and not errors
            )
            result["runs"].append({
                "command": cmd,
                "rc": rc,
                "observed": observed,
                "error_markers": errors,
                "unique_cases": len(actual_cases),
                "case_matrix_exact": actual_cases == expected_cases,
                "pass": passed,
            })
    result["inputs_after"] = {str(p): digest(p) for p in inputs}
    result["sdk_headers_after"] = {str(p): digest(p) for p in sdk_headers}
    result["inputs_stable"] = (
        result["inputs_before"] == result["inputs_after"]
        and result["sdk_headers_before"] == result["sdk_headers_after"]
    )
    result["engine_sources_stable_after_runs"] = all(
        digest(path) == value for path, value in engine_receipt["source_after"].items()
    )
    result["pass"] = (
        result["inputs_stable"]
        and result["engine_sources_stable_after_runs"]
        and len(result["runs"]) == 2
        and all(run["pass"] for run in result["runs"])
    )
    (out / "RESULTS.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"pass": result["pass"], "compile_rc": build_rc, "runs": result["runs"]}, indent=2))
    return 0 if result["pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
