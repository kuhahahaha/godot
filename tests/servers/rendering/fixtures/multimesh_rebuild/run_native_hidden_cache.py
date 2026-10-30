#!/usr/bin/env python3
"""Check hidden MultiMesh edits survive cross-frame motion-cache changes."""

import argparse
import hashlib
import json
import math
import os
import re
import shutil
import struct
import subprocess
from pathlib import Path


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--build-receipt", type=Path, required=True)
    ap.add_argument("--pack-editor-receipt", type=Path)
    ap.add_argument("--with-fields", action="store_true")
    controls = ap.add_mutually_exclusive_group()
    controls.add_argument("--no-motion", action="store_true")
    controls.add_argument("--omit-second-edit", action="store_true")
    ap.add_argument("--output", type=Path, required=True)
    a = ap.parse_args()
    build = json.loads(a.build_receipt.read_text())
    binary = Path(build["binary"])
    if (
        build["build_rc"] != 0
        or not build["source_stable"]
        or build["source_before"] != build["source_after"]
        or build["git_status_before"]
        or build["git_status_after"]
        or digest(binary) != build["binary_sha256"]
    ):
        raise ValueError("matching successful clean build receipt required")
    out = a.output.resolve()
    project = out / "project"
    project.mkdir(parents=True, exist_ok=False)
    (project / ".godot").mkdir()
    (project / ".godot/global_script_class_cache.cfg").write_text("list=[]\n")
    (project / "project.godot").write_text(
        '[application]\nconfig/name="HiddenCachedCarry"\n[rendering]\nshader_compiler/shader_cache/enabled=false\n'
    )
    (out / "observed").mkdir()
    fixture = Path(__file__).with_name("native_hidden_cache.gd").resolve()
    script = project / fixture.name
    shutil.copy2(fixture, script)
    executable = binary
    package = None
    package_inputs = []
    if a.pack_editor_receipt:
        from pack_native_fixture import prepare_packed_scene

        executable, package_inputs, package = prepare_packed_scene(project, script, binary, a.pack_editor_receipt, out)
    stages = [
        ("initial", 16, False),
        ("hidden_a", 16, False),
        ("hidden_b", 16, False),
        ("visible_move", 24, False),
        ("reveal", 24, True),
        ("hold", 24, True),
    ]
    samples = []
    for stage, first, reveal in stages:
        masks = [
            ("first_old", (16, 24), [0, 1, 0] if first == 16 else [0, 0, 0]),
            ("first_new", (24, 24), [0, 1, 0] if first == 24 else [0, 0, 0]),
            ("hidden_a", (40, 32), ([1, 0, 0] if a.with_fields else [0, 1, 0]) if reveal else [0, 0, 0]),
            ("hidden_b", (48, 40), ([0, 0, 1] if a.with_fields else [0, 1, 0]) if reveal else [0, 0, 0]),
            ("background", (5, 5), [0, 0, 0]),
        ]
        for label, (x, y), rgb in masks:
            for dy in [-1, 0, 1]:
                for dx in [-1, 0, 1]:
                    samples.append({"stage": stage, "label": label, "xy": [x + dx, y + dy], "rgb": rgb})
    expected_path = out / "EXPECTED.json"
    expected_path.write_text(
        json.dumps(
            {
                "stages": stages,
                "samples": samples,
                "tolerance": 1 / 255,
                "count": 1025,
                "stride": 20 if a.with_fields else 12,
            },
            indent=2,
        )
        + "\n"
    )
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
    if package is None:
        cmd += ["--path", str(project), "--script", "res://" + fixture.name]
    args = [
        flag
        for flag, enabled in [
            ("--with-fields", a.with_fields),
            ("--no-motion", a.no_motion),
            ("--omit-second-edit", a.omit_second_edit),
        ]
        if enabled
    ]
    if args:
        cmd += ["--", *args]
    env = os.environ.copy()
    env["GODOT_TEST_K02_HIDDEN_OUT"] = str(out / "observed")
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
        return [json.loads(x) for x in re.findall("^K02_HIDDEN_" + label + r" (.*)$", log, re.M)]

    observed = records("SAMPLE")
    coherent = len(observed) == 270
    failed = []
    for row, literal in zip(observed, samples):
        matches = len(row["actual"]) == 3 and all(
            math.isfinite(x) and abs(x - y) <= 1 / 255 for x, y in zip(row["actual"], literal["rgb"])
        )
        coherent = (
            coherent
            and all(row[k] == literal[k] for k in ["stage", "label", "xy"])
            and row["expected"] == literal["rgb"]
            and row["pass"] == matches
        )
        if not matches:
            failed.append((literal["stage"], literal["label"], literal["xy"]))
    expected_failed = [
        (x["stage"], x["label"], x["xy"])
        for x in samples
        if a.omit_second_edit and x["stage"] in ["reveal", "hold"] and x["label"] == "hidden_b"
    ]
    # Independent public input records: exact float32 CPU cache every stage,
    # and a complete matching native GPU half once all instances are visible.
    stride = 20 if a.with_fields else 12
    data = []
    for i in range(1025):
        row = [1, 0, 0, -2 if i == 0 else 20 + i, 0, 1, 0, 1 if i == 0 else 0, 0, 0, 1, 0]
        if a.with_fields:
            row += ([0, 1, 0, 1] if i == 0 else [1, 1, 1, 1]) + [0.25] * 4
        data += row
    raw_checks = []
    stages_coherent = len(records("STAGE")) == 6
    for index, (stage, first, reveal) in enumerate(stages):
        if index == 1:
            data[700 * stride + 3] = 1
            if a.with_fields:
                data[700 * stride + 12 : 700 * stride + 20] = [1, 0, 0, 1, 0.5, 0.25, 0.75, 1]
        if index == 2 and not a.omit_second_edit:
            data[1024 * stride + 3] = 2
            data[1024 * stride + 7] = -1
            if a.with_fields:
                data[1024 * stride + 12 : 1024 * stride + 20] = [0, 0, 1, 1, 0.75, 0.5, 0.25, 1]
        if index == 3:
            data[3] = -1
        literal_bytes = struct.pack("<" + "f" * len(data), *data)
        cpu_path = out / "observed" / (stage + ".cpu")
        gpu_path = out / "observed" / (stage + ".gpu")
        cpu = cpu_path.read_bytes() if cpu_path.exists() else b""
        gpu = gpu_path.read_bytes() if gpu_path.exists() else b""
        size = len(literal_bytes)
        gpu_size_ok = len(gpu) == size * (1 if a.no_motion else 2)
        matched = [
            half for half in range(1 if a.no_motion else 2) if gpu[half * size : (half + 1) * size] == literal_bytes
        ]
        raw_checks.append({
            "stage": stage,
            "cpu_exact": cpu == literal_bytes,
            "gpu_size_ok": gpu_size_ok,
            "gpu_matching_halves": matched,
            "gpu_exact_required": reveal,
        })
        actual_stages = records("STAGE")
        if len(actual_stages) == 6:
            stages_coherent = stages_coherent and actual_stages[index] == {
                "stage": stage,
                "first_x": first,
                "reveal": reveal,
                "callback_delta": 1,
                "cpu_bytes": size,
                "gpu_bytes": size * (1 if a.no_motion else 2),
                "cached_a_x": data[700 * stride + 3],
                "cached_b_x": data[1024 * stride + 3],
            }
    result = {
        "stages": 6,
        "checks": 270,
        "failures": 18 if a.omit_second_edit else 0,
        "pass": not a.omit_second_edit,
        "scope": "ordinary_hidden_cached_instance_carry",
    }
    diagnostics = bool(re.search(r"ERROR:|WARNING:|UNAVAILABLE|INVALID|[Ll]eaked", log))
    after = {str(p): digest(p) for p in inputs}
    passed = (
        rc == (1 if a.omit_second_edit else 0)
        and coherent
        and failed == expected_failed
        and records("RESULT") == [result]
        and stages_coherent
        and all(
            x["cpu_exact"] and x["gpu_size_ok"] and (x["gpu_matching_halves"] or not x["gpu_exact_required"])
            for x in raw_checks
        )
        and records("RUNTIME")
        == [
            {
                "no_motion": a.no_motion,
                "with_fields": a.with_fields,
                "omit_second": a.omit_second_edit,
                "cache_enabled": False,
                "editor_hint": False,
            }
        ]
        and "Metal" in log
        and "Forward+" in log
        and (".double." in log.splitlines()[0]) == (build["precision"] == "double")
        and not diagnostics
        and before == after
        and all((out / "observed" / (stage + ".png")).is_file() for stage, _, _ in stages)
    )
    record = {
        "command": cmd,
        "host_rc": rc,
        "engine_commit": build["commit"],
        "precision": build["precision"],
        "with_fields": a.with_fields,
        "no_motion": a.no_motion,
        "omit_second": a.omit_second_edit,
        "package": package,
        "inputs_before": before,
        "inputs_after": after,
        "inputs_stable": before == after,
        "sample_failures": failed,
        "raw_checks": raw_checks,
        "stages_coherent": stages_coherent,
        "results": records("RESULT"),
        "diagnostics": diagnostics,
        "pass": passed,
    }
    (out / "RESULTS.json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps({k: record[k] for k in ["host_rc", "results", "diagnostics", "pass"]}, indent=2))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
