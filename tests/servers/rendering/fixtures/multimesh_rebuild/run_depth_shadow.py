#!/usr/bin/env python3
"""Run the ordinary native depth/shadow fixture with frozen pixel expectations."""

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
from pathlib import Path


def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as source:
        for part in iter(lambda: source.read(1048576), b""):
            h.update(part)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--build-receipt", required=True, type=Path)
    ap.add_argument("--pack-editor-receipt", type=Path, help="package a main scene for a release template")
    ap.add_argument("--renderer", required=True, choices=["mobile", "forward_plus"])
    ap.add_argument("--control", choices=["normal", "omit-depth", "omit-shadow"], default="normal")
    ap.add_argument("--output", required=True, type=Path)
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
        raise ValueError("a matching successful clean engine build receipt is required")
    out = a.output.resolve()
    project = out / "project"
    project.mkdir(parents=True, exist_ok=False)
    (project / ".godot").mkdir()
    (out / "png").mkdir()
    fixture = Path(__file__).resolve().with_name("native_depth_shadow.gd")
    script = project / fixture.name
    shutil.copy2(fixture, script)
    (project / "project.godot").write_text(
        '[application]\nconfig/name="OrdinaryDepthShadow"\n[display]\nwindow/size/viewport_width=96\nwindow/size/viewport_height=96\n'
        "[rendering]\nshader_compiler/shader_cache/enabled=false\n"
    )
    (project / ".godot/global_script_class_cache.cfg").write_text("list=[]\n")
    executable = binary
    package_inputs = []
    package_record = None
    if a.pack_editor_receipt:
        from pack_native_fixture import prepare_packed_scene

        executable, package_inputs, package_record = prepare_packed_scene(
            project, script, binary, a.pack_editor_receipt, out
        )
    stages = [
        "depth_all",
        "depth_visible_0",
        "depth_restored_resize",
        "shadow_all",
        "shadow_visible_0",
        "shadow_restored",
    ]
    labels = ["mesh", "multimesh", "gpu_particles"]
    # Literal sRGB encoding of .5/.625/.375, fixed before any engine run.
    depth_colors = [0.7353569830524495, 0.8123661451624896, 0.6460766259089328]
    samples = []
    for stage in stages:
        shadow = stage.startswith("shadow_")
        centers = [(25, 48), (57, 48), (89, 48)] if shadow else [(16, 48), (48, 48), (80, 48)]
        for label, center, depth in zip(
            labels + ["lit_receiver" if shadow else "background"], centers + [(5, 5)], depth_colors + [0]
        ):
            value = (1 if label == "lit_receiver" else 0) if shadow else depth
            if stage.endswith("visible_0") and label == "multimesh":
                value = 1 if shadow else 0
            for y in range(-1, 2):
                for x in range(-1, 2):
                    samples.append({
                        "stage": stage,
                        "label": label,
                        "xy": [center[0] + x, center[1] + y],
                        "rgb": [value] * 3,
                    })
    expected = {"stages": stages, "samples": samples, "tolerance": 1 / 255, "checks": 216}
    expected_path = out / "EXPECTED.json"
    expected_path.write_text(json.dumps(expected, indent=2) + "\n")
    inputs = [
        binary,
        a.build_receipt.resolve(),
        fixture,
        script,
        project / "project.godot",
        expected_path,
        Path(__file__).resolve(),
    ]
    inputs.extend(package_inputs)
    before = {str(path): digest(path) for path in inputs}
    cmd = [
        str(binary),
        "--path",
        str(project),
        "--rendering-driver",
        "metal",
        "--rendering-method",
        a.renderer,
        "--render-thread",
        "safe",
        "--audio-driver",
        "Dummy",
        "--script",
        "res://" + fixture.name,
    ]
    if package_record:
        cmd = [
            str(executable),
            "--rendering-driver",
            "metal",
            "--rendering-method",
            a.renderer,
            "--render-thread",
            "safe",
            "--audio-driver",
            "Dummy",
        ]
    if a.control != "normal":
        cmd += ["--", "--omit-multimesh-" + ("depth" if a.control == "omit-depth" else "shadow")]
    env = os.environ.copy()
    env["GODOT_TEST_K02_DEPTH_SHADOW_OUT"] = str(out / "png")
    try:
        run = subprocess.run(cmd, capture_output=True, text=True, env=env, timeout=90)
        log = run.stdout + run.stderr
        rc = run.returncode
    except subprocess.TimeoutExpired as exc:

        def decoded(value):
            return value.decode(errors="replace") if isinstance(value, bytes) else value or ""

        log = decoded(exc.stdout) + decoded(exc.stderr)
        rc = "timeout"
    (out / "RUN.log").write_text(log)
    observed = [json.loads(line) for line in re.findall(r"^K02_DEPTH_SHADOW_SAMPLE (.*)$", log, re.M)]
    results = [json.loads(line) for line in re.findall(r"^K02_DEPTH_SHADOW_RESULT (.*)$", log, re.M)]
    runtime = [json.loads(line) for line in re.findall(r"^K02_DEPTH_SHADOW_RUNTIME (.*)$", log, re.M)]
    observed_stages = re.findall(r"^K02_DEPTH_SHADOW_STAGE (.*)$", log, re.M)
    failures = []
    coherent = len(observed) == len(samples)
    for actual, literal in zip(observed, samples):
        identity = all(actual[key] == literal[key] for key in ["stage", "label", "xy"])
        expected_rgb = len(actual["expected"]) == 3 and all(
            abs(actual["expected"][i] - literal["rgb"][i]) < 0.000001 for i in range(3)
        )
        matches = len(actual["actual"]) == 3 and all(
            abs(actual["actual"][i] - literal["rgb"][i]) <= 1 / 255 for i in range(3)
        )
        coherent = coherent and identity and expected_rgb and matches == actual["pass"]
        if not matches:
            failures.append((literal["stage"], literal["label"], literal["xy"]))
    target_stages = (
        ["depth_all", "depth_restored_resize"] if a.control == "omit-depth" else ["shadow_all", "shadow_restored"]
    )
    expected_failures = (
        []
        if a.control == "normal"
        else [
            (s["stage"], s["label"], s["xy"])
            for s in samples
            if s["label"] == "multimesh" and s["stage"] in target_stages
        ]
    )
    expected_rc = 0 if a.control == "normal" else 1
    bad = bool(re.search(r"ERROR:|WARNING:|UNAVAILABLE|[Ll]eaked", log))
    after = {str(path): digest(path) for path in inputs}
    requested_renderer = "Forward Mobile" if a.renderer == "mobile" else "Forward+"
    passed = (
        rc == expected_rc
        and coherent
        and failures == expected_failures
        and observed_stages == stages
        and len(results) == 1
        and results[0]["checks"] == 216
        and results[0]["stages"] == 6
        and results[0]["failures"] == len(expected_failures)
        and results[0]["pass"] == (a.control == "normal")
        and runtime
        == [
            {
                "main_rd": True,
                "editor_hint": False,
                "cache_enabled": False,
                "omit_depth": a.control == "omit-depth",
                "omit_shadow": a.control == "omit-shadow",
            }
        ]
        and requested_renderer in log
        and "Metal" in log
        and (".double." in log.splitlines()[0]) == (receipt["precision"] == "double")
        and not bad
        and before == after
        and all((out / "png" / (stage + ".png")).is_file() for stage in stages)
    )
    record = {
        "command": cmd,
        "engine_commit": receipt["commit"],
        "precision": receipt["precision"],
        "control": a.control,
        "build_receipt": str(a.build_receipt.resolve()),
        "package": package_record,
        "host_rc": rc,
        "inputs_before": before,
        "inputs_after": after,
        "inputs_stable": before == after,
        "samples_coherent": coherent,
        "failures": failures,
        "results": results,
        "diagnostics": bad,
        "pass": passed,
        "scope": "ordinary native depth/shadow framebuffer constituent",
    }
    (out / "RESULTS.json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps({key: record[key] for key in ["host_rc", "control", "results", "diagnostics", "pass"]}, indent=2))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
