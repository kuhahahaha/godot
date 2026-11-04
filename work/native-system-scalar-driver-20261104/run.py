#!/usr/bin/env python3
"""Run original System scalar native record/readback/retirement on a pinned real GPU."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
RUNTIME = HERE / "runtime"
HOST = ROOT / "work/particle-render-exit-seal-audit-20261105/seal-wait-clean-pair/editor.arm64"
HOST_SHA = "887bff69ab84ab6f35a88f2f2bb9cd46d5970bb7e4ca7a230ea5c94761130392"
META = Path(str(HOST) + ".controlled_build.json")
META_SHA = "c7355cb7415dcc2584522d9a2f2bbbf663fe591aec46c24a3fd655eea297293a"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--driver", choices=("metal", "vulkan"), default="metal")
    args = parser.parse_args()
    assert sha(HOST) == HOST_SHA and sha(META) == META_SHA
    metadata = json.loads(META.read_text())
    assert metadata["status"] == "available" and metadata["artifact"]["sha256"] == HOST_SHA
    assert metadata["artifact"]["bytes"] == HOST.stat().st_size
    linkage = subprocess.check_output(["otool", "-L", str(HOST)], text=True)
    assert "libgodot" not in linkage.lower(), "Pin an actual linked core separately before using a linked host."
    out = HERE / "results" / (time.strftime("%Y%m%d-%H%M%S") + "-" + args.driver)
    out.mkdir(parents=True, exist_ok=False)
    names = ("system_probe.cpp", "gdextension_interface_v2.pinned.h", "scalar.glsl", "generate.gd")
    before = {"runtime/" + name: sha(RUNTIME / name) for name in names}
    before["run.py"] = sha(HERE / "run.py")
    snapshot = out / "inputs"
    for name in before:
        target = snapshot / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(HERE / name, target)
    result = {"driver": args.driver, "host": {"path": str(HOST), "sha256": HOST_SHA,
              "metadata_sha256": META_SHA, "metadata": metadata, "linkage": linkage},
              "inputs": before, "builds": [], "runs": []}
    env = dict(os.environ, ANGE_MACOS_BACKGROUND_RENDER="1")
    for name in ("DYLD_LIBRARY_PATH", "DYLD_FALLBACK_LIBRARY_PATH", "DYLD_INSERT_LIBRARIES"):
        env.pop(name, None)
    generator = out / "containers"
    generator.mkdir()
    for name in ("scalar.glsl", "generate.gd"):
        shutil.copyfile(RUNTIME / name, generator / name)
    project_text = 'config_version=5\n[application]\nconfig/name="Original System scalar control"\n[rendering]\nrenderer/rendering_method="mobile"\n'
    (generator / "project.godot").write_text(project_text)
    env["SYSTEM_SCALAR_CONTAINER"] = str(generator / "scalar.bin")

    def save():
        (out / "results.json").write_text(json.dumps(result, indent=2) + "\n")

    def launch(case, project, script):
        cmd = [str(HOST), "--path", str(project), "--rendering-driver", args.driver,
               "--rendering-method", "mobile", "--render-thread", "safe", "--script", script]
        try:
            proc = subprocess.run(cmd, cwd=ROOT, env=env, capture_output=True, text=True, timeout=60)
            text, code = proc.stdout + proc.stderr, proc.returncode
        except subprocess.TimeoutExpired as exc:
            text = (exc.stdout or b"").decode(errors="replace") + (exc.stderr or b"").decode(errors="replace")
            code = "timeout"
        log = out / (case + ".log")
        log.write_text(text)
        row = {"case": case, "command": cmd, "cwd": str(ROOT), "returncode": code,
               "timeout_seconds": 60, "log_sha256": sha(log), "checks": {}}
        result["runs"].append(row)
        save()
        return text, row

    text, row = launch("containers", generator, "generate.gd")
    row["checks"] = {"exit_zero": row["returncode"] == 0,
                     "native_container": bool(re.search(r"SYSTEM_SCALAR_CONTAINER bytes=[1-9][0-9]*", text)) and (generator / "scalar.bin").is_file(),
                     "actual_backend": ("Metal" if args.driver == "metal" else "Vulkan") in text and "Forward Mobile" in text,
                     "clean": "ERROR:" not in text}
    save()
    assert all(row["checks"].values()), row
    result["container_sha256"] = sha(generator / "scalar.bin")
    for case, word in (("wrong_expected_word", "0x40400001"), ("positive", "0x40400000")):
        dso = out / (case + ".dylib")
        cmd = ["clang++", "-std=c++17", "-dynamiclib", "-arch", "arm64", "-mmacosx-version-min=11.0",
               "-O2", "-Wall", "-Wextra", "-Wno-unused-parameter", "-Wno-deprecated-declarations",
               "-DSCALAR_EXPECTED_WORD=" + word, "-I", str(ROOT), str(RUNTIME / "system_probe.cpp"), "-o", str(dso)]
        build = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
        (out / (case + "-build.log")).write_text(build.stdout + build.stderr)
        result["builds"].append({"case": case, "command": cmd, "returncode": build.returncode,
                                 "dso_sha256": sha(dso) if build.returncode == 0 else None})
        save()
        assert build.returncode == 0, build.stderr
        project = out / case
        (project / ".godot").mkdir(parents=True)
        (project / "project.godot").write_text(project_text)
        (project / "probe.gd").write_text('extends SceneTree\nfunc _initialize():\n print("SYSTEM_SCALAR_LOOP_INITIALIZED")\n quit(0)\n')
        (project / "probe.gdextension").write_text('[configuration]\nentry_symbol="phs1_probe_init"\ncompatibility_minimum="4.7"\ndevice_exit_guard_version=1\n[libraries]\nmacos="' + str(dso) + '"\n')
        (project / ".godot/extension_list.cfg").write_text('res://probe.gdextension\n')
        text, row = launch(case, project, "probe.gd")
        if case == "wrong_expected_word":
            row["checks"] = {"target_assertion": row["returncode"] == 180 and "where=scalar_exact_output" in text}
        else:
            row["checks"] = {"exit_zero": row["returncode"] == 0,
                             "native_scalar": "SYSTEM_SCALAR_COMPLETE exact_word=40400000 bytes=4 native_complete=1 physical_free=1 ack=1" in text,
                             "actual_backend": ("Metal" if args.driver == "metal" else "Vulkan") in text and "Forward Mobile" in text,
                             "unload": text.count("PHS1_DSO_UNLOAD tag=one") == 1,
                             "clean": not re.search(r"ERROR:|ASSERT|CrashHandler|leaked at exit|FIXTURE_FATAL", text)}
        save()
        print(case, row["returncode"], row["checks"], flush=True)
        assert all(row["checks"].values()), row
    result["inputs_unchanged"] = all(sha(HERE / name) == digest for name, digest in before.items())
    result["host_unchanged"] = sha(HOST) == HOST_SHA and sha(META) == META_SHA
    save()
    assert result["inputs_unchanged"] and result["host_unchanged"]
    print(out / "results.json", flush=True)


if __name__ == "__main__":
    main()
