#!/usr/bin/env python3
"""Compile a real SDK-header GDExtension and verify original guarded/legacy lookup."""

import argparse
import hashlib
import json
import platform
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

FIXTURE = Path(__file__).resolve().parent


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def execute(command: list[str], output: Path, name: str) -> str:
    record = output / f"{name}.json"
    record.write_text(json.dumps({"command": command}, indent=2) + "\n")
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
    except subprocess.TimeoutExpired as error:
        (output / f"{name}.stdout.log").write_bytes(error.stdout or b"")
        (output / f"{name}.stderr.log").write_bytes(error.stderr or b"")
        record.write_text(json.dumps({"command": command, "timeout_seconds": 60}, indent=2) + "\n")
        raise
    (output / f"{name}.stdout.log").write_text(result.stdout)
    (output / f"{name}.stderr.log").write_text(result.stderr)
    record.write_text(json.dumps({"command": command, "exit": result.returncode}, indent=2) + "\n")
    require(result.returncode == 0, f"{name}: process exit {result.returncode}")
    require(not result.stderr, f"{name}: unexpected stderr (see saved log)")
    return result.stdout


def validate(output: str, mode: str, expected_names: set[str]) -> None:
    lookups = re.findall(r"^ORIGINAL_LOOKUP (\S+)=(\S+)$", output, re.MULTILINE)
    require(len(lookups) == 41, f"{mode}: expected exactly 41 original lookups")
    require({name for name, _ in lookups} == expected_names, f"{mode}: lookup name set differs from SDK62")
    require("MANAGER_LOAD=0" in output, f"{mode}: manager did not load the real extension")
    require(f"SDK62_PROBE_DONE {mode}" in output, f"{mode}: missing script completion")
    initializations = re.findall(r"^INITIALIZE level=(\d+) legacy=(\d)$", output, re.MULTILINE)
    terminations = re.findall(r"^TERMINATE level=(\d+) legacy=(\d)$", output, re.MULTILINE)
    calls = re.findall(r"^ABI_CALL (\w+)=(-?\d+)$", output, re.MULTILINE)
    if mode == "guarded":
        require(all(value not in ("0x0", "(nil)") for _, value in lookups), "guarded: missing original proc")
        require(
            "SDK62_LOAD=1 LEGACY=0 VISIBLE=41 EXPECTED=41 V2_NULL=1" in output,
            "guarded: actual SDK load failed or unexpected SceneReady V2 member",
        )
        levels = [level for level, legacy in initializations if legacy == "0"]
        require(levels in (["2"], ["2", "3"]), "guarded: unexpected initialization levels")
        require(initializations == [(level, "0") for level in levels], "guarded: unexpected legacy initialization")
        require(terminations == [(level, "0") for level in reversed(levels)], "guarded: missing termination")
        expected_calls = [("register_guard", "0")]
        for _ in levels:
            expected_calls += [("borrow_initialize_level", "0"), ("after_binding_initialize", "0")]
        expected_calls += [("publish_exit_result", "0")]
        for _ in levels:
            expected_calls += [
                ("borrow_deinit_level", "0"),
                ("begin_user_terminator", "0"),
                ("after_user_terminator", "0"),
                ("after_binding_deinit", "0"),
            ]
        require(calls == expected_calls, "guarded: lifecycle call sequence/results differ")
        require("MANAGER_UNLOAD=0" in output, "guarded: real explicit unload failed")
    else:
        require(all(value in ("0x0", "(nil)") for _, value in lookups), "legacy: typed proc leaked")
        require(
            "SDK62_LOAD=0 LEGACY=1 VISIBLE=0 EXPECTED=41 V2_NULL=1" in output,
            "legacy: SDK load was not rejected",
        )
        levels = [level for level, legacy in initializations if legacy == "1"]
        require(levels in (["0", "1", "2"], ["0", "1", "2", "3"]), "legacy: missing initialization")
        require(initializations == [(level, "1") for level in levels], "legacy: unexpected guarded initialization")
        require(terminations == [(level, "1") for level in reversed(levels)], "legacy: missing shutdown")
        require(not calls, "legacy: invoked guarded lifecycle API")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", type=Path, required=True, help="actual SDK62 godot-cpp checkout")
    parser.add_argument("--binary", type=Path, required=True, help="existing native Godot executable")
    parser.add_argument("--output", type=Path, help="new artifact directory (default: temporary directory)")
    args = parser.parse_args()
    if platform.system() != "Darwin":
        parser.error("this fixture currently supports macOS only; no Linux/Windows execution is claimed")
    output = args.output.resolve() if args.output else Path(tempfile.mkdtemp(prefix="godot-sdk62-load-"))
    if args.output:
        output.mkdir(parents=True, exist_ok=False)
    print(f"Artifacts: {output}", flush=True)
    input_hashes: dict[str, str] = {}
    receipt: dict[str, object] = {"status": "FAIL", "platform": platform.platform(), "inputs": input_hashes}
    try:
        sdk = args.sdk_root.resolve()
        binary = args.binary.resolve()
        inputs = [
            binary,
            sdk / "gen/include/gdextension_interface.h",
            sdk / "include/godot_cpp/core/device_exit_api.hpp",
            sdk / "gdextension/gdextension_interface.json",
            FIXTURE / "probe.cc",
            Path(__file__).resolve(),
        ]
        for path in inputs:
            input_hashes[str(path)] = sha256(path)
        api = (sdk / "include/godot_cpp/core/device_exit_api.hpp").read_text()
        names = set(re.findall(r'lookup\("([^"]+)"\)', api)) - {"device_exit_register_scene_ready_v2"}
        require(len(names) == 41, "expected frozen SDK62 41-proc load surface")
        # Freeze the supplied host before running so parallel engine builds cannot replace it.
        snapshot = output / "host"
        shutil.copy2(binary, snapshot)
        require(sha256(snapshot) == input_hashes[str(binary)], "host changed while snapshotting")
        receipt["host_version"] = execute([str(snapshot), "--version"], output, "version").strip()
        execute(["clang++", "--version"], output, "compiler-version")
        library = output / "libprobe.dylib"
        execute(
            [
                "clang++",
                "-std=c++17",
                "-dynamiclib",
                "-Wall",
                "-Wextra",
                "-Werror",
                f"-I{sdk / 'gen/include'}",
                f"-I{sdk / 'include'}",
                str(FIXTURE / "probe.cc"),
                "-o",
                str(library),
            ],
            output,
            "compile",
        )
        for mode, guard_version in (("guarded", 1), ("legacy", 0)):
            project = output / mode
            project.mkdir()
            (project / "project.godot").write_text(
                '[application]\nconfig/name="SDK62 load probe"\n'
                '[rendering]\nrenderer/rendering_method="gl_compatibility"\n'
            )
            (project / "probe.gdextension").write_text(
                '[configuration]\nentry_symbol="sdk62_entry"\ncompatibility_minimum="4.7"\n'
                f'device_exit_guard_version={guard_version}\n[libraries]\nmacos="../libprobe.dylib"\n'
            )
            script = (
                "extends SceneTree\nfunc _initialize():\n"
                '    var result := GDExtensionManager.load_extension("res://probe.gdextension")\n'
                '    print("MANAGER_LOAD=", result)\n'
                "    if result != GDExtensionManager.LOAD_STATUS_OK:\n"
                "        quit(1)\n        return\n"
            )
            if mode == "guarded":
                script += (
                    '    var closed := GDExtensionManager.unload_extension("res://probe.gdextension")\n'
                    '    print("MANAGER_UNLOAD=", closed)\n'
                    '    print("SDK62_PROBE_DONE guarded")\n'
                    "    quit(0 if closed == GDExtensionManager.LOAD_STATUS_OK else 2)\n"
                )
            else:
                script += '    print("SDK62_PROBE_DONE legacy")\n    quit(0)\n'
            (project / "probe.gd").write_text(script)
            text = execute(
                [str(snapshot), "--headless", "--path", str(project), "--script", "res://probe.gd"], output, mode
            )
            validate(text, mode, names)
            require(sha256(snapshot) == input_hashes[str(binary)], "frozen runtime host changed")
        for path in inputs[1:]:
            require(sha256(path) == input_hashes[str(path)], f"input changed during probe: {path}")
        receipt["status"] = "PASS"
        receipt["scope"] = "41 raw lookups + SDK load; eight distinct lifecycle APIs, no qualification/GPU calls"
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        receipt["error"] = str(error)
    finally:
        receipt["artifact_sha256"] = {
            str(path.relative_to(output)): sha256(path)
            for path in output.rglob("*")
            if path.is_file() and path.name != "RESULT.json"
        }
        (output / "RESULT.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(receipt["status"], receipt.get("error", "guarded and legacy checks passed"))
    return 0 if receipt["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
