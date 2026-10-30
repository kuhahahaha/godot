#!/usr/bin/env python3
"""Run the controlled real-renderer admission fixture on macOS Metal."""

import argparse
import hashlib
import platform
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

FIXTURE = Path(__file__).resolve().parent
REPOSITORY = FIXTURE.parents[4]
REQUIRED = (
    "REAL_MATERIALS",
    "COLOR_ATOMIC",
    "SHADOW_APPEND",
    "RECOVERY",
    "ORDINARY",
    "FIRST_MISMATCH",
    "RETIRED",
    "EMPTY",
    "ALL",
)


def invalid_markers(lines):
    expected = {f"B4_LIST_{name}": "1" for name in REQUIRED}
    expected.update({"B4_LIST_LOAD": "0", "B4_LIST_UNLOAD": "0"})
    return {
        name: matches
        for name, value in expected.items()
        if (matches := [line for line in lines if line.startswith(f"{name}=")]) != [f"{name}={value}"]
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--engine", required=True, type=Path, help="tests=yes editor containing test_b4_render_list.cpp"
    )
    parser.add_argument(
        "--generated-root",
        type=Path,
        default=REPOSITORY,
        help="root containing core/extension/gdextension_interface.gen.h",
    )
    parser.add_argument("--compiler", default="clang")
    parser.add_argument("--timeout", type=int, default=90)
    args = parser.parse_args()
    engine = args.engine.resolve(strict=True)
    if sys.platform != "darwin" or platform.machine() != "arm64":
        parser.error(
            "this fixture currently supports macOS arm64; other platforms require their own tested driver/extension build"
        )
    header = args.generated_root / "core/extension/gdextension_interface.gen.h"
    if not header.is_file():
        parser.error(f"missing generated header: {header}")
    print(f"engine_sha256={hashlib.sha256(engine.read_bytes()).hexdigest()}", flush=True)
    with tempfile.TemporaryDirectory(prefix="godot-b4-render-list-") as temp:
        project = Path(temp)
        for name in ("project.godot", "probe.gd", "probe.gdextension"):
            shutil.copy2(FIXTURE / name, project / name)
        compile_cmd = [
            args.compiler,
            "-dynamiclib",
            "-arch",
            "arm64",
            "-mmacosx-version-min=11.0",
            "-I",
            str(args.generated_root.resolve()),
            str(FIXTURE / "client.c"),
            "-o",
            str(project / "probe.dylib"),
        ]
        print("compile:", compile_cmd, flush=True)
        if subprocess.run(compile_cmd, check=False).returncode:
            return 2
        failed = False
        for method in ("mobile", "forward_plus"):
            command = [
                str(engine),
                "--path",
                str(project),
                "--rendering-driver",
                "metal",
                "--rendering-method",
                method,
                "--render-thread",
                "safe",
                "--script",
                "probe.gd",
            ]
            print("run:", command, flush=True)
            try:
                result = subprocess.run(command, capture_output=True, text=True, timeout=args.timeout, check=False)
            except subprocess.TimeoutExpired:
                print(f"{method}: timeout", file=sys.stderr)
                failed = True
                continue
            output = result.stdout + result.stderr
            print(output, end="", flush=True)
            lines = output.splitlines()
            invalid = invalid_markers(lines)
            renderer = "Forward Mobile" if method == "mobile" else "Forward+"
            backend_present = any("Metal " in line and renderer in line for line in lines)
            diagnostics = [line for line in lines if "ERROR:" in line or "FATAL" in line or "leaked" in line.lower()]
            if result.returncode or invalid or not backend_present or diagnostics:
                print(
                    f"{method}: failed rc={result.returncode}, invalid markers={invalid}, backend_present={backend_present}, diagnostics={diagnostics}",
                    file=sys.stderr,
                    flush=True,
                )
                failed = True
        return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
