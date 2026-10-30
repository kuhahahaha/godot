#!/usr/bin/env python3
"""Check controlled ordinary GPUParticles simulation against frozen pixel masks."""

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


def witness_literals(names):
    colors = [[1, 0, 1], [0, 1, 1], [1, 1, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1]]
    return [
        {"stage": name, "ordinal": ordinal, "xy": [4 + x, 90 + y], "rgb": colors[ordinal]}
        for ordinal, name in enumerate(names)
        for y in [-1, 0, 1]
        for x in [-1, 0, 1]
    ]


def verify_witnesses(log, prefix, names, expected_commit, precision, target):
    samples = [json.loads(line) for line in re.findall(r"^" + prefix + r"_WITNESS (.*)$", log, re.M)]
    frames = [json.loads(line) for line in re.findall(r"^" + prefix + r"_WITNESS_FRAME (.*)$", log, re.M)]
    results = [json.loads(line) for line in re.findall(r"^" + prefix + r"_WITNESS_RESULT (.*)$", log, re.M)]
    hosts = [json.loads(line) for line in re.findall(r"^" + prefix + r"_HOST (.*)$", log, re.M)]
    wanted = witness_literals(names)
    coherent = len(samples) == len(wanted)
    for actual, literal in zip(samples, wanted):
        coherent = coherent and all(actual[key] == literal[key] for key in ["stage", "ordinal", "xy"])
        coherent = (
            coherent
            and actual["pass"] is True
            and len(actual["actual"]) == 3
            and all(abs(a - b) <= 1 / 255 for a, b in zip(actual["actual"], literal["rgb"]))
        )
    coherent = coherent and len(frames) == len(names)
    for ordinal, (frame, name) in enumerate(zip(frames, names)):
        coherent = (
            coherent
            and frame["stage"] == name
            and frame["ordinal"] == ordinal
            and frame["fresh"] is True
            and 1 <= frame["frame_polls"] <= 12
        )
    coherent = coherent and results == [{"stages": len(names), "checks": len(wanted), "failures": 0, "pass": True}]
    expected_host = {
        "editor_feature": target == "editor",
        "release_feature": target == "template_release",
        "double_precision": precision == "double",
        "cache_enabled": False,
        "editor_hint": False,
    }
    host_ok = hosts == [expected_host]
    banner = log.splitlines()[0] if log else ""
    revision = re.search(r"\.([0-9a-f]{9,40})(?:\s|$)", banner)
    revision_ok = revision is not None and expected_commit.startswith(revision.group(1))
    return {
        "pass": coherent and host_ok and revision_ok,
        "sample_count": len(samples),
        "frames": frames,
        "results": results,
        "runtime_host": hosts,
        "host_valid": host_ok,
        "banner": banner,
        "revision_valid": revision_ok,
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--build-receipt", required=True, type=Path)
    ap.add_argument("--expected-commit", help="full source commit; defaults to the validated build receipt")
    ap.add_argument("--pack-editor-receipt", type=Path, help="package a main scene for a release template")
    ap.add_argument("--renderer", required=True, choices=["mobile", "forward_plus"])
    ap.add_argument(
        "--profile",
        choices=["normal", "fractional", "preprocess", "zero-velocity", "omit-second-request"],
        default="normal",
    )
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
    target = "template_release" if a.pack_editor_receipt else "editor"
    expected_commit = a.expected_commit or receipt.get("commit", "")
    if (
        not re.fullmatch(r"[0-9a-f]{40}", expected_commit)
        or receipt.get("commit") != expected_commit
        or not re.fullmatch(r"[0-9a-f]{40}", receipt.get("tree", ""))
        or receipt.get("precision") not in ["single", "double"]
        or f"precision={receipt['precision']}" not in receipt["command"]
        or f"target={target}" not in receipt["command"]
    ):
        raise ValueError("explicit matching source, precision and target build identity required")
    if a.pack_editor_receipt:
        editor = json.loads(a.pack_editor_receipt.read_text())
        if (
            editor.get("commit") != expected_commit
            or editor.get("tree") != receipt["tree"]
            or editor.get("precision") != receipt["precision"]
            or f"precision={receipt['precision']}" not in editor["command"]
        ):
            raise ValueError("packing Editor source and precision must match the tested template")
    out = a.output.resolve()
    project = out / "project"
    project.mkdir(parents=True, exist_ok=False)
    (project / ".godot").mkdir()
    (out / "png").mkdir()
    fixture = Path(__file__).resolve().with_name("native_simulation.gd")
    script = project / fixture.name
    shutil.copy2(fixture, script)
    source_copy = out / "SOURCE_FIXTURE.gd"
    runner_copy = out / "RUNNER_SOURCE.py"
    shutil.copy2(fixture, source_copy)
    shutil.copy2(Path(__file__), runner_copy)
    (project / "project.godot").write_text(
        '[application]\nconfig/name="OrdinaryNativeSimulation"\n[display]\nwindow/size/viewport_width=96\nwindow/size/viewport_height=96\n'
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
    names = ["initial", "paused_hold", "requested_1", "requested_2_resize"]
    centers = (
        [24] * 4
        if a.profile == "zero-velocity"
        else [40, 40, 48, 56]
        if a.profile == "preprocess"
        else [32, 32, 40, 48]
    )
    stages = [{"stage": stage, "expected_center_x": center} for stage, center in zip(names, centers)]
    samples = []
    for stage, center in zip(names, centers):
        masks = [("particle_" + str(x), (x, 24), [0, 0, 1] if x == center else [0, 0, 0]) for x in [24, 32, 40, 48, 56]]
        masks += [("mesh", (16, 48), [1, 0, 0]), ("multimesh", (48, 48), [0, 1, 0]), ("background", (5, 5), [0, 0, 0])]
        for label, point, rgb in masks:
            for y in range(-1, 2):
                for x in range(-1, 2):
                    samples.append({"stage": stage, "label": label, "xy": [point[0] + x, point[1] + y], "rgb": rgb})
    expected = {"profile": a.profile, "stages": stages, "samples": samples, "tolerance": 1 / 255, "checks": 288}
    expected["witnesses"] = witness_literals(names)
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
        source_copy,
        runner_copy,
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
    if a.profile != "normal":
        cmd += ["--", "--" + a.profile]
    env = os.environ.copy()
    env["GODOT_TEST_K02_SIMULATION_OUT"] = str(out / "png")
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
    observed = [json.loads(line) for line in re.findall(r"^K02_SIMULATION_SAMPLE (.*)$", log, re.M)]
    results = [json.loads(line) for line in re.findall(r"^K02_SIMULATION_RESULT (.*)$", log, re.M)]
    runtime = [json.loads(line) for line in re.findall(r"^K02_SIMULATION_RUNTIME (.*)$", log, re.M)]
    observed_stages = [json.loads(line) for line in re.findall(r"^K02_SIMULATION_STAGE (.*)$", log, re.M)]
    failures = []
    coherent = len(observed) == len(samples)
    for actual, literal in zip(observed, samples):
        identity = all(actual[key] == literal[key] for key in ["stage", "label", "xy"])
        expected_rgb = actual["expected"] == literal["rgb"]
        matches = len(actual["actual"]) == 3 and all(
            abs(actual["actual"][i] - literal["rgb"][i]) <= 1 / 255 for i in range(3)
        )
        coherent = coherent and identity and expected_rgb and matches == actual["pass"]
        if not matches:
            failures.append((literal["stage"], literal["label"], literal["xy"]))
        actual_rgb = literal["rgb"]
        if a.profile == "omit-second-request" and literal["stage"] == "requested_2_resize":
            if literal["label"] == "particle_40":
                actual_rgb = [0, 0, 1]
            if literal["label"] == "particle_48":
                actual_rgb = [0, 0, 0]
        coherent = (
            coherent
            and len(actual["actual"]) == 3
            and all(abs(a - b) <= 1 / 255 for a, b in zip(actual["actual"], actual_rgb))
        )
    negative = a.profile == "omit-second-request"
    expected_failures = [
        (s["stage"], s["label"], s["xy"])
        for s in samples
        if negative and s["stage"] == "requested_2_resize" and s["label"] in ["particle_40", "particle_48"]
    ]
    bad = bool(re.search(r"ERROR:|WARNING:|UNAVAILABLE|[Ll]eaked", log))
    after = {str(path): digest(path) for path in inputs}
    requested_renderer = "Forward Mobile" if a.renderer == "mobile" else "Forward+"
    witness = verify_witnesses(log, "K02_SIMULATION", names, expected_commit, receipt["precision"], target)
    passed = (
        rc == (1 if negative else 0)
        and coherent
        and witness["pass"]
        and failures == expected_failures
        and observed_stages == stages
        and len(results) == 1
        and results[0]
        == {
            "profile": a.profile,
            "stages": 4,
            "checks": 288,
            "failures": len(expected_failures),
            "pass": not negative,
            "scope": "ordinary_planar_native_simulation_pixels",
        }
        and runtime == [{"profile": a.profile, "main_rd": True, "editor_hint": False, "cache_enabled": False}]
        and requested_renderer in log
        and "Metal" in log
        and (".double." in witness["banner"]) == (receipt["precision"] == "double")
        and not bad
        and before == after
        and all((out / "png" / (stage + ".png")).is_file() for stage in names)
    )
    record = {
        "command": cmd,
        "target": target,
        "expected_commit": expected_commit,
        "witness": witness,
        "engine_commit": receipt["commit"],
        "precision": receipt["precision"],
        "profile": a.profile,
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
        "scope": "ordinary planar native GPUParticles simulation pixels",
    }
    (out / "RESULTS.json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps({key: record[key] for key in ["host_rc", "profile", "results", "diagnostics", "pass"]}, indent=2))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
