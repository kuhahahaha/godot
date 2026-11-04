#!/usr/bin/env python3
"""Exercise cancellation after original native record admission on a pinned Metal host."""
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
BINARY = ROOT / "work/particle-render-exit-seal-audit-20261105/seal-wait-clean-pair/editor.arm64"
PIN = "887bff69ab84ab6f35a88f2f2bb9cd46d5970bb7e4ca7a230ea5c94761130392"
META = Path(str(BINARY) + ".controlled_build.json")
META_PIN = "c7355cb7415dcc2584522d9a2f2bbbf663fe591aec46c24a3fd655eea297293a"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    assert sha(BINARY) == PIN and sha(META) == META_PIN
    metadata = json.loads(META.read_text())
    assert metadata["status"] == "available" and metadata["artifact"]["sha256"] == PIN
    assert metadata["artifact"]["bytes"] == BINARY.stat().st_size
    linkage = subprocess.check_output(["otool", "-L", str(BINARY)], text=True)
    assert "libgodot" not in linkage.lower(), "This control pins a monolithic host."
    out = HERE / "results" / time.strftime("%Y%m%d-%H%M%S")
    out.mkdir(parents=True, exist_ok=False)
    inputs = [HERE / "run.py", RUNTIME / "typed_probe.cpp", RUNTIME / "typed_setup.inc",
              RUNTIME / "typed_retire.inc", RUNTIME / "gdextension_interface_v2.pinned.h"]
    inputs += sorted((RUNTIME / "containers").glob("*.glsl"))
    inputs += [RUNTIME / "containers/generate.gd"]
    before = {str(p.relative_to(HERE)): sha(p) for p in inputs}
    env = dict(os.environ, ANGE_MACOS_BACKGROUND_RENDER="1")
    for key in ("DYLD_LIBRARY_PATH", "DYLD_FALLBACK_LIBRARY_PATH", "DYLD_INSERT_LIBRARIES"):
        env.pop(key, None)
    for kind in ("capture", "body"):
        env[f"MULTI_V2_{kind.upper()}_CONTAINER"] = str(RUNTIME / f"containers/{kind}.bin")
        env[f"MULTI_V2_{kind.upper()}_SOURCE"] = str(RUNTIME / f"containers/{kind}.glsl")
        (RUNTIME / f"containers/{kind}.bin").unlink(missing_ok=True)
    result = {"host": {"binary": str(BINARY), "sha256": PIN, "metadata_sha256": META_PIN,
                       "selection": metadata["selection"], "linkage": linkage},
              "inputs": before, "builds": [], "runs": []}

    def save():
        (out / "results.json").write_text(json.dumps(result, indent=2) + "\n")

    def launch(name, project, script):
        cmd = [str(BINARY), "--path", str(project), "--rendering-driver", "metal",
               "--rendering-method", "mobile", "--render-thread", "safe", "--script", script]
        try:
            run = subprocess.run(cmd, cwd=ROOT, env=env, capture_output=True, text=True, timeout=60)
            text, rc = run.stdout + run.stderr, run.returncode
        except subprocess.TimeoutExpired as exc:
            text = (exc.stdout or b"").decode(errors="replace") + (exc.stderr or b"").decode(errors="replace")
            rc = "timeout"
        log = out / f"{name}.log"
        log.write_text(text)
        row = {"case": name, "command": cmd, "cwd": str(ROOT), "timeout_seconds": 60,
               "returncode": rc, "log_sha256": sha(log), "checks": {}}
        result["runs"].append(row)
        save()
        return text, row

    text, row = launch("containers", RUNTIME / "containers", "generate.gd")
    row["checks"] = {"exit_zero": row["returncode"] == 0,
                     "generated": all((RUNTIME / f"containers/{kind}.bin").is_file() and
                                      f"MULTI_V2_CONTAINER kind={kind}" in text for kind in ("capture", "body")),
                     "real_metal": "Metal 4.0 - Forward Mobile" in text,
                     "clean": "ERROR:" not in text}
    save()
    assert all(row["checks"].values()), row
    result["containers"] = {kind: sha(RUNTIME / f"containers/{kind}.bin") for kind in ("capture", "body")}

    for case, omit in (("omit_cancel_control", True), ("after_record_cancel", False)):
        project = RUNTIME / case
        shutil.copytree(RUNTIME / "positive", project, dirs_exist_ok=True)
        dso = RUNTIME / f"{case}.dylib"
        (project / "probe.gdextension").write_text(
            '[configuration]\nentry_symbol="phs1_probe_init"\ncompatibility_minimum="4.7"\n'
            'device_exit_guard_version=1\n[libraries]\nmacos="../' + dso.name + '"\n')
        cmd = ["clang++", "-std=c++17", "-dynamiclib", "-arch", "arm64", "-mmacosx-version-min=11.0",
               "-O2", "-Wall", "-Wextra", "-Wno-unused-parameter", "-Wno-deprecated-declarations",
               "-DTICKET_MODE=3", "-I", str(ROOT), str(RUNTIME / "typed_probe.cpp"), "-o", str(dso)]
        if omit:
            cmd.insert(1, "-DOMIT_AFTER_RECORD_CANCEL")
        build = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
        (out / f"{case}-build.log").write_text(build.stdout + build.stderr)
        result["builds"].append({"case": case, "command": cmd, "returncode": build.returncode,
                                 "dso_sha256": sha(dso) if build.returncode == 0 else None})
        save()
        assert build.returncode == 0, build.stderr
        text, row = launch(case, project, "probe.gd")
        if omit:
            row["checks"] = {"target_assertion": row["returncode"] == 180 and
                             "MULTI_V2_ASSERT where=after_record_cancel_closes_upload mode=3" in text}
        else:
            row["checks"] = {
                "exit_zero": row["returncode"] == 0,
                "real_metal": "Metal 4.0 - Forward Mobile" in text,
                "record_before_result": bool(re.search(r"AFTER_RECORD_CANCEL before_state=[0-7] before_phase=\d+ owned=0 readbacks=3", text)),
                "cancelled_drain": "AFTER_RECORD_DRAIN state=7 phase=6 admitted_readbacks=3 returned_bytes=44 owned=0" in text,
                "physical_events": len(re.findall(r"TYPED_PHYSICAL kind=[123] .*exact_ack=1", text)) == 6,
                "terminal": bool(re.search(r"TYPED_NATIVE mode=3 .*same_ticket=1 .*actual_async_distinct=1 .*native_complete=1 .*physical_free=1 ack=1 .*typed_physical_acks=6", text)),
                "unload": text.count("PHS1_DSO_UNLOAD tag=one") == 1,
                "clean": not re.search(r"ERROR:|MULTI_V2_ASSERT|CrashHandler|leaked at exit", text),
            }
        save()
        print(case, row["returncode"], row["checks"], flush=True)
        assert all(row["checks"].values()), row
    result["inputs_unchanged"] = all(sha(HERE / p) == digest for p, digest in before.items())
    result["host_unchanged"] = sha(BINARY) == PIN and sha(META) == META_PIN
    save()
    assert result["inputs_unchanged"] and result["host_unchanged"]
    print(out / "results.json")


if __name__ == "__main__":
    main()
