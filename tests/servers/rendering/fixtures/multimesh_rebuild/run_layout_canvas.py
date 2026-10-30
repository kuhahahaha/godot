#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path


def digest(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-receipt", type=Path, required=True)
    ap.add_argument("--expected-commit", required=True)
    ap.add_argument("--renderer", choices=["mobile", "forward_plus"], required=True)
    ap.add_argument("--output", type=Path, required=True)
    ap.add_argument("--omit-move", action="store_true")
    ap.add_argument("--pack-editor-receipt", type=Path)
    a = ap.parse_args()
    build = json.loads(a.build_receipt.read_text())
    binary = Path(build["binary"])
    if not (
        build["build_rc"] == 0
        and build["source_stable"]
        and build["source_before"] == build["source_after"]
        and not build["git_status_before"]
        and not build["git_status_after"]
        and digest(binary) == build["binary_sha256"]
    ):
        raise ValueError("matching successful clean build receipt required")
    precision = build.get("precision")
    target = "template_release" if a.pack_editor_receipt else "editor"
    if (
        precision not in ["single", "double"]
        or f"precision={precision}" not in build["command"]
        or f"target={target}" not in build["command"]
        or not build.get("commit")
        or not build.get("tree")
        or not re.fullmatch(r"[0-9a-f]{40}", a.expected_commit)
        or build["commit"] != a.expected_commit
    ):
        raise ValueError("explicit matching target, precision and source identity required")
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    project = out / "project"
    (project / ".godot").mkdir(parents=True)
    (out / "observed").mkdir()
    (project / ".godot/global_script_class_cache.cfg").write_text("list=[]\n")
    (project / "project.godot").write_text(
        '[application]\nconfig/name="LayoutCanvas"\n[display]\nwindow/size/viewport_width=96\nwindow/size/viewport_height=64\n[rendering]\nshader_compiler/shader_cache/enabled=false\nenvironment/defaults/default_clear_color=Color(0, 0, 0, 1)\n'
    )
    fixture = Path(__file__).with_name("native_layout_canvas.gd")
    script = project / fixture.name
    shutil.copy2(fixture, script)
    original_fixture = out / "SOURCE_FIXTURE.gd"
    original_runner = out / "RUNNER_SOURCE.py"
    shutil.copy2(fixture, original_fixture)
    shutil.copy2(Path(__file__), original_runner)
    executable = binary
    package = None
    package_inputs = []
    if a.pack_editor_receipt:
        editor = json.loads(a.pack_editor_receipt.read_text())
        if (
            editor.get("precision") != precision
            or editor.get("commit") != build["commit"]
            or editor.get("tree") != build["tree"]
            or f"precision={precision}" not in editor["command"]
        ):
            raise ValueError("packing Editor must match template precision and source identity")
        helper = Path(__file__).with_name("pack_native_fixture.py")
        sys.path.insert(0, str(helper.parent))
        from pack_native_fixture import prepare_packed_scene

        executable, package_inputs, package = prepare_packed_scene(project, script, binary, a.pack_editor_receipt, out)
    stages = [
        ("both", 2, False),
        ("one", 1, False),
        ("zero", 0, False),
        ("restored", 2, False),
        ("move", 2, True),
        ("hold", 2, True),
    ]
    centers = {"old": (24, 32), "new": (48, 32), "second": (72, 32), "background_a": (8, 8), "background_b": (88, 56)}
    expected = {}
    for stage, count, moved in stages:
        red = {
            "old": count > 0 and not moved,
            "new": count > 0 and moved,
            "second": count == 2,
            "background_a": False,
            "background_b": False,
        }
        for label, (cx, cy) in centers.items():
            for dy in [-1, 0, 1]:
                for dx in [-1, 0, 1]:
                    expected[(stage, label, cx + dx, cy + dy)] = red[label]
    (out / "EXPECTED.json").write_text(
        json.dumps(
            {
                "stages": stages,
                "centers": centers,
                "samples": 270,
                "omission_failure_samples": 36,
                "structural_failures": 0,
                "source_engine_commit": build["commit"],
                "scope": "ordinary canvas layout/data/color; no external packet owner or completion",
            },
            indent=2,
        )
        + "\n"
    )
    inputs = [
        binary,
        a.build_receipt,
        fixture,
        script,
        project / "project.godot",
        out / "EXPECTED.json",
        Path(__file__).resolve(),
        original_fixture,
        original_runner,
        *package_inputs,
    ]
    before = {str(p.resolve()): digest(p) for p in inputs}
    cmd = [
        str(executable),
        "--display-driver",
        "macos",
        "--rendering-driver",
        "metal",
        "--rendering-method",
        a.renderer,
        "--render-thread",
        "safe",
        "--audio-driver",
        "Dummy",
    ]
    if not package:
        cmd += ["--path", str(project), "--script", "res://" + script.name]
    if a.omit_move:
        cmd += ["--", "--omit-move"]
    env = os.environ.copy()
    env["GODOT_TEST_LAYOUT_OUT"] = str(out / "observed")
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, env=env, timeout=90)
        log = r.stdout + r.stderr
        rc = r.returncode
    except subprocess.TimeoutExpired as e:
        log = (e.stdout or b"").decode() + (e.stderr or b"").decode()
        rc = "timeout"
    (out / "RUN.log").write_text(log)
    samples = [json.loads(x) for x in re.findall(r"^K02_LAYOUT_SAMPLE (.*)$", log, re.M)]
    states = [json.loads(x) for x in re.findall(r"^K02_LAYOUT_STAGE (.*)$", log, re.M)]
    results = [json.loads(x) for x in re.findall(r"^K02_LAYOUT_RESULT (.*)$", log, re.M)]
    hosts = [json.loads(x) for x in re.findall(r"^K02_LAYOUT_HOST (.*)$", log, re.M)]
    witness_colors = [[1, 0, 1], [0, 1, 1], [1, 1, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1]]
    witnesses = [json.loads(x) for x in re.findall(r"^K02_LAYOUT_WITNESS (.*)$", log, re.M)]
    witness_keys = {
        (stage, i, 4 + x, 60 + y) for i, (stage, _, _) in enumerate(stages) for x in [-1, 0, 1] for y in [-1, 0, 1]
    }
    seen_witnesses = set()
    witness_valid = True
    for sample in witnesses:
        key = (sample["stage"], sample["ordinal"], *sample["xy"])
        if key not in witness_keys or key in seen_witnesses:
            witness_valid = False
            continue
        seen_witnesses.add(key)
        witness_valid &= (
            sample["pass"] is True
            and len(sample["actual"]) == 3
            and all(abs(a - b) <= 1 / 255 for a, b in zip(sample["actual"], witness_colors[sample["ordinal"]]))
        )
    witness_valid &= seen_witnesses == witness_keys and len(witnesses) == 54
    seen = set()
    bad = []
    sample_valid = True
    for s in samples:
        key = (s["stage"], s["label"], *s["xy"])
        if key not in expected or key in seen or s["red"] != expected.get(key):
            sample_valid = False
            continue
        seen.add(key)
        wanted = [1 if expected[key] else 0, 0, 0]
        passed = all(abs(x - y) <= 1 / 255 for x, y in zip(s["actual"], wanted))
        sample_valid &= len(s["actual"]) == 3 and passed == s["pass"]
        if not passed:
            bad.append(key)
        actual_red = expected[key]
        if a.omit_move and s["stage"] in ["move", "hold"] and s["label"] in ["old", "new"]:
            actual_red = s["label"] == "old"
        actual_wanted = [1 if actual_red else 0, 0, 0]
        sample_valid &= all(abs(x - y) <= 1 / 255 for x, y in zip(s["actual"], actual_wanted))
    wanted_bad = (
        {
            (stage, label, cx + dx, 32 + dy)
            for stage in ["move", "hold"]
            for label, cx in [("old", 24), ("new", 48)]
            for dx in [-1, 0, 1]
            for dy in [-1, 0, 1]
        }
        if a.omit_move
        else set()
    )
    data = []
    initial = [0.0] * 60
    for i in range(3):
        for offset in [0, 5, 10, 14, 15]:
            initial[i * 20 + offset] = 1.0
        initial[i * 20 + 16] = 0.25
    original = struct.pack("<60f", *initial)
    old_ok = all(
        (out / "observed" / f"initial_3d.{suffix}").is_file()
        and (out / "observed" / f"initial_3d.{suffix}").read_bytes() == original
        for suffix in ["cpu", "gpu"]
    )
    old_ok &= (out / "observed/initial_3d.command").is_file() and (
        out / "observed/initial_3d.command"
    ).read_bytes() == struct.pack("<5I", 6, 3, 0, 0, 0)
    initial_rows = [json.loads(x) for x in re.findall(r"^K02_LAYOUT_INITIAL (.*)$", log, re.M)]
    old_ok &= len(initial_rows) == 1 and initial_rows[0] == {
        "pass": True,
        "cpu_bytes": 240,
        "gpu_bytes": 240,
        "command": [6, 3, 0, 0, 0],
        "same_mesh": True,
    }
    data.append({"stage": "initial_3d", "pass": old_ok})
    for stage, count, moved in stages:
        x = 48 if moved and not a.omit_move else 24
        want = struct.pack("<16f", 1, 0, 0, x, 0, 1, 0, 32, 1, 0, 0, 72, 0, 1, 0, 32)
        observed = out / "observed"
        ok = all(
            (observed / f"{stage}.{suffix}").is_file() and (observed / f"{stage}.{suffix}").read_bytes() == want
            for suffix in ["cpu", "gpu"]
        )
        ok &= (observed / f"{stage}.command").is_file() and (observed / f"{stage}.command").read_bytes() == struct.pack(
            "<5I", 6, count, 0, 0, 0
        )
        data.append({"stage": stage, "pass": ok})
    after = {str(p.resolve()): digest(p) for p in inputs}
    diagnostics = [
        line
        for line in log.splitlines()
        if any(t in line for t in ["ERROR:", "WARNING:", "SCRIPT ERROR", "Leaked", "leaked"])
    ]
    banner = log.splitlines()[0] if log else ""
    revision = re.search(r"\.([0-9a-f]{9,40})(?:\s|$)", banner)
    renderer = "Forward Mobile" if a.renderer == "mobile" else "Forward+"
    passed = (
        rc == (1 if a.omit_move else 0)
        and sample_valid
        and seen == set(expected)
        and len(samples) == 270
        and set(bad) == wanted_bad
        and len(bad) == len(wanted_bad)
        and len(states) == 6
        and [s["stage"] for s in states] == [s[0] for s in stages]
        and all(
            s["data_pass"]
            and s["image_valid"]
            and s["witness_fresh"]
            and s["witness_ordinal"] == i
            and 1 <= s["frame_polls"] <= 12
            for i, s in enumerate(states)
        )
        and witness_valid
        and hosts
        == [
            {
                "editor_feature": target == "editor",
                "release_feature": target == "template_release",
                "double_precision": precision == "double",
                "cache_enabled": False,
                "editor_hint": False,
            }
        ]
        and revision is not None
        and a.expected_commit.startswith(revision.group(1))
        and len(results) == 1
        and results[0]
        == {
            "checks": 270,
            "witness_checks": 54,
            "pixel_failures": len(wanted_bad),
            "structural_failures": 0,
            "omit_move": a.omit_move,
        }
        and all(d["pass"] for d in data)
        and before == after
        and renderer in log
        and "Metal" in log
        and (".double." in banner) == (build.get("precision", "single") == "double")
        and not diagnostics
    )
    record = {
        "command": cmd,
        "environment": {"GODOT_TEST_LAYOUT_OUT": env["GODOT_TEST_LAYOUT_OUT"]},
        "build_receipt": build,
        "input_hashes_before": before,
        "input_hashes_after": after,
        "inputs_stable": before == after,
        "host_rc": rc,
        "precision": build.get("precision", "single"),
        "renderer": a.renderer,
        "omission": a.omit_move,
        "sample_count": len(samples),
        "witness_count": len(witnesses),
        "witness_pass": witness_valid,
        "package": package,
        "target": target,
        "expected_commit": a.expected_commit,
        "runtime_host": hosts,
        "banner": banner,
        "sample_failure_keys": bad,
        "stages": states,
        "results": results,
        "data_checks": data,
        "diagnostics": diagnostics,
        "pass": passed,
        "scope": "actual ordinary native2D canvas layout/visible/move control; no physical/operation/completion/owner/Ready qualification",
    }
    (out / "RESULTS.json").write_text(json.dumps(record, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: record[k]
                for k in [
                    "host_rc",
                    "precision",
                    "renderer",
                    "omission",
                    "sample_count",
                    "results",
                    "data_checks",
                    "diagnostics",
                    "pass",
                ]
            },
            indent=2,
        )
    )
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
