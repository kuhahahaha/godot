#!/usr/bin/env python3
"""Validate ordinary MultiMesh cross-frame velocity after same-mesh resize."""

import argparse
import hashlib
import json
import math
import os
import re
import shutil
import subprocess
from pathlib import Path


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--build-receipt", type=Path, required=True)
    ap.add_argument("--pack-editor-receipt", type=Path)
    ap.add_argument("--omit-second-move", action="store_true")
    update = ap.add_mutually_exclusive_group()
    update.add_argument("--cached-transform", action="store_true")
    update.add_argument("--cached-hidden-first", action="store_true")
    update.add_argument("--cached-start-hidden", action="store_true")
    update.add_argument("--cached-late-motion", action="store_true")
    ap.add_argument("--output", type=Path, required=True)
    a = ap.parse_args()
    receipt = json.loads(a.build_receipt.read_text())
    binary = Path(receipt["binary"])
    if (
        receipt["build_rc"] != 0
        or not receipt["source_stable"]
        or receipt["source_before"] != receipt["source_after"]
        or receipt["git_status_before"]
        or receipt["git_status_after"]
        or digest(binary) != receipt["binary_sha256"]
    ):
        raise ValueError("a matching successful clean build receipt is required")
    out = a.output.resolve()
    project = out / "project"
    project.mkdir(parents=True, exist_ok=False)
    (project / ".godot").mkdir()
    (project / ".godot/global_script_class_cache.cfg").write_text("list=[]\n")
    (out / "observed").mkdir()
    (project / "project.godot").write_text(
        '[application]\nconfig/name="NativeMotionHistory"\n[rendering]\nshader_compiler/shader_cache/enabled=false\n'
    )
    fixture = Path(__file__).with_name("native_motion_history.gd").resolve()
    script = project / fixture.name
    shutil.copy2(fixture, script)
    executable = binary
    package_inputs = []
    package_record = None
    if a.pack_editor_receipt:
        from pack_native_fixture import prepare_packed_scene

        executable, package_inputs, package_record = prepare_packed_scene(
            project, script, binary, a.pack_editor_receipt, out
        )
    stages = [
        ("initial", 24, -0.125 if a.cached_late_motion else 0.0),
        ("move_1", 32, -0.125),
        ("move_2", 40, -0.125),
        ("resize", 40, 0.0),
        ("move_after_resize", 48, -0.125),
        ("hold", 48, 0.0),
    ]
    expected = []
    for stage, center, velocity in stages:
        for label, point, value in [("multimesh", (center, 32), velocity), ("background", (5, 5), 0.0)]:
            for y in range(-1, 2):
                for x in range(-1, 2):
                    expected.append({
                        "stage": stage,
                        "label": label,
                        "xy": [point[0] + x, point[1] + y],
                        "velocity": [value, 0.0],
                    })
    expected_path = out / "EXPECTED.json"
    expected_path.write_text(json.dumps({"stages": stages, "velocity": expected, "tolerance": 2**-12}, indent=2) + "\n")
    inputs = [
        binary,
        a.build_receipt.resolve(),
        fixture,
        script,
        project / "project.godot",
        expected_path,
        Path(__file__).resolve(),
        *package_inputs,
    ]
    before = {str(p): digest(p) for p in inputs}
    cmd = [
        str(executable),
        "--rendering-driver",
        "metal",
        "--rendering-method",
        "forward_plus",
        "--render-thread",
        "safe",
        "--audio-driver",
        "Dummy",
    ]
    if package_record is None:
        cmd += ["--path", str(project), "--script", "res://" + script.name]
    user_args = []
    if a.omit_second_move:
        user_args.append("--omit-second-move")
    if a.cached_transform:
        user_args.append("--cached-transform")
    if a.cached_hidden_first:
        user_args.append("--cached-hidden-first")
    if a.cached_start_hidden:
        user_args.append("--cached-start-hidden")
    if a.cached_late_motion:
        user_args.append("--cached-late-motion")
    if user_args:
        cmd += ["--", *user_args]
    env = os.environ.copy()
    env["GODOT_TEST_K02_MOTION_OUT"] = str(out / "observed")
    try:
        run = subprocess.run(cmd, capture_output=True, text=True, env=env, timeout=90)
        rc = run.returncode
        log = run.stdout + run.stderr
    except subprocess.TimeoutExpired as exc:

        def decoded(value):
            return value.decode(errors="replace") if isinstance(value, bytes) else value or ""

        rc = "timeout"
        log = decoded(exc.stdout) + decoded(exc.stderr)
    (out / "RUN.log").write_text(log)

    def records(label):
        return [json.loads(x) for x in re.findall("^K02_MOTION_" + label + r" (.*)$", log, re.M)]

    observed = records("SAMPLE")
    coherent = len(observed) == len(expected)
    failures = []
    for row, literal in zip(observed, expected):
        matches = len(row["actual"]) == 2 and all(
            math.isfinite(row["actual"][i]) and abs(row["actual"][i] - literal["velocity"][i]) <= 2**-12
            for i in range(2)
        )
        coherent = (
            coherent
            and all(row[k] == literal[k] for k in ["stage", "label", "xy"])
            and row["expected"] == literal["velocity"]
            and row["pass"] == matches
        )
        if not matches:
            failures.append((literal["stage"], literal["label"], literal["xy"]))
    expected_failures = [
        (x["stage"], x["label"], x["xy"])
        for x in expected
        if a.omit_second_move and x["stage"] == "move_2" and x["label"] == "multimesh"
    ]
    colors = records("COLOR")
    colors_ok = len(colors) == 6
    for row, (stage, center, _) in zip(colors, stages):
        rgb = [0, 0, 0] if a.omit_second_move and stage == "move_2" else [0, 1, 0]
        colors_ok = (
            colors_ok
            and row["stage"] == stage
            and row["xy"] == [center, 32]
            and len(row["actual"]) == 3
            and all(math.isfinite(x) and abs(x - y) <= 1 / 255 for x, y in zip(row["actual"], rgb))
            and row["pass"] == (rgb == [0, 1, 0])
        )
    observed_stages = records("STAGE")
    stages_ok = len(observed_stages) == 6
    for row, (stage, center, velocity) in zip(observed_stages, stages):
        stages_ok = stages_ok and row == {
            "stage": stage,
            "center_x": center,
            "expected_velocity": [velocity, 0.0],
            "callback_delta": 1,
            "bytes": 32768,
        }
    result = records("RESULT")
    expected_result = {
        "stages": 6,
        "checks": 114,
        "failures": 10 if a.omit_second_move else 0,
        "pass": not a.omit_second_move,
        "scope": "ordinary_multimesh_velocity_texture",
    }
    diagnostics = bool(re.search(r"ERROR:|WARNING:|UNAVAILABLE|[Ll]eaked|INVALID|SHADER_ERROR", log))
    after = {str(p): digest(p) for p in inputs}
    passed = (
        rc == (1 if a.omit_second_move else 0)
        and coherent
        and failures == expected_failures
        and colors_ok
        and stages_ok
        and result == [expected_result]
        and records("SOURCE") == [{"format": "R16G16_SFLOAT", "width": 64, "height": 64, "main_rd": True}] * 6
        and records("RUNTIME")
        == [
            {
                "omit_second_move": a.omit_second_move,
                "cached_transform": a.cached_transform
                or a.cached_hidden_first
                or a.cached_start_hidden
                or a.cached_late_motion,
                "cached_hidden_first": a.cached_hidden_first,
                "cached_start_hidden": a.cached_start_hidden,
                "cached_late_motion": a.cached_late_motion,
                "cache_enabled": False,
                "editor_hint": False,
                "renderer": "forward_plus",
            }
        ]
        and "Metal" in log
        and "Forward+" in log
        and (".double." in log.splitlines()[0]) == (receipt["precision"] == "double")
        and not diagnostics
        and before == after
        and all(
            (out / "observed" / (stage + suffix)).is_file() for stage, _, _ in stages for suffix in [".rgf", ".png"]
        )
    )
    record = {
        "command": cmd,
        "host_rc": rc,
        "engine_commit": receipt["commit"],
        "precision": receipt["precision"],
        "cached_transform": a.cached_transform
        or a.cached_hidden_first
        or a.cached_start_hidden
        or a.cached_late_motion,
        "cached_hidden_first": a.cached_hidden_first,
        "cached_start_hidden": a.cached_start_hidden,
        "cached_late_motion": a.cached_late_motion,
        "package": package_record,
        "inputs_before": before,
        "inputs_after": after,
        "inputs_stable": before == after,
        "velocity_failures": failures,
        "colors_coherent": colors_ok,
        "result": result,
        "diagnostics": diagnostics,
        "pass": passed,
    }
    (out / "RESULTS.json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps({k: record[k] for k in ["host_rc", "result", "diagnostics", "pass"]}, indent=2))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
