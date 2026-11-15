#!/usr/bin/env python3
"""Exercise extracted D3D12 ordinary indirect methods against a bounded mock."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[4]
SOURCE = "drivers/d3d12/rendering_device_driver_d3d12.cpp"
METHODS = ("command_render_draw_indirect", "command_render_draw_indexed_indirect")


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract(source: str, method: str) -> str:
    signature = f"void RenderingDeviceDriverD3D12::{method}("
    start = source.index(signature)
    # These production definitions end with a brace at column zero. Retain
    # their entire signature/body verbatim, including original barrier prefix.
    end = source.index("\n}", start) + 2
    body = source[start:end]
    if body.count("{") != body.count("}"):
        raise RuntimeError(f"unbalanced complete production method: {method}")
    return body


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--source-ref", help="exact old revision for an old-implementation regression control")
    parser.add_argument("--cxx", default="clang++")
    args = parser.parse_args()
    raw = (
        subprocess.check_output(["git", "show", f"{args.source_ref}:{SOURCE}"], cwd=ROOT)
        if args.source_ref
        else (ROOT / SOURCE).read_bytes()
    )
    methods = {name: extract(raw.decode(), name) for name in METHODS}
    template = (HERE / "harness.cpp.in").read_text()
    if template.count("PRODUCTION_METHODS") != 1:
        raise RuntimeError("expected exactly one extraction marker")
    harness = template.replace("PRODUCTION_METHODS", "\n\n".join(methods.values()))
    args.out.mkdir(parents=True, exist_ok=False)
    (args.out / "production-methods.cpp.txt").write_text("\n\n".join(methods.values()) + "\n")
    result: dict[str, object] = {
        "source": SOURCE,
        "source_ref": args.source_ref,
        "source_sha256": digest(raw),
        "method_sha256": {name: digest(body.encode()) for name, body in methods.items()},
        "harness_sha256": digest(harness.encode()),
        "mock_only": True,
        "windows_gpu_executed": False,
        "count_buffer_methods_tested": False,
    }
    # Generated .cpp and executable never enter tests/SCsub recursive discovery.
    with tempfile.TemporaryDirectory(prefix="godot-d3d12-stride-") as temporary:
        generated = Path(temporary) / "harness.cpp"
        binary = Path(temporary) / "harness"
        generated.write_text(harness)
        command = [
            args.cxx,
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Wno-unused-parameter",
            str(generated),
            "-o",
            str(binary),
        ]
        build = subprocess.run(command, capture_output=True, timeout=60)
        (args.out / "compile.stdout.log").write_bytes(build.stdout)
        (args.out / "compile.stderr.log").write_bytes(build.stderr)
        result["compile_command"] = command
        result["compile_rc"] = build.returncode
        if build.returncode == 0:
            run = subprocess.run([str(binary)], capture_output=True, timeout=30)
            (args.out / "run.stdout.log").write_bytes(run.stdout)
            (args.out / "run.stderr.log").write_bytes(run.stderr)
            result["run_rc"] = run.returncode
            result["passed"] = run.returncode == 0 and b"CASES=28 FAILURES=0\n" in run.stdout and not run.stderr
        else:
            result["passed"] = False
    if not args.source_ref:
        result["source_unchanged"] = raw == (ROOT / SOURCE).read_bytes()
        result["passed"] = result["passed"] is True and result["source_unchanged"] is True
    (args.out / "RESULT.json").write_text(json.dumps(result, indent=2) + "\n")
    files = {
        p.name: {"bytes": p.stat().st_size, "sha256": digest(p.read_bytes())} for p in args.out.iterdir() if p.is_file()
    }
    (args.out / "FILES.json").write_text(json.dumps(files, indent=2) + "\n")
    print(
        json.dumps({
            "out": str(args.out),
            "passed": result["passed"],
            "compile_rc": build.returncode,
            "run_rc": result.get("run_rc"),
        })
    )
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
