#!/usr/bin/env python3
"""Exercise rejected RD particle buffers and subsequent visible recovery."""

import argparse
import re
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--renderer", choices=["mobile", "forward_plus"], required=True)
    parser.add_argument(
        "--dimension", choices=["2d", "3d", "motion_retry", "motion_control", "uniform_retry"], required=True
    )
    parser.add_argument("--log", type=Path, required=True)
    args = parser.parse_args()
    if args.dimension in {"motion_retry", "motion_control"} and args.renderer != "forward_plus":
        parser.error("motion profiles require forward_plus")
    project = Path(__file__).resolve().parent
    command = [
        str(args.binary.resolve()),
        "--display-driver",
        "macos",
        "--rendering-driver",
        "metal",
        "--rendering-method",
        args.renderer,
        "--render-thread",
        "safe",
        "--audio-driver",
        "Dummy",
        "--path",
        str(project),
        "--script",
        "res://probe.gd",
        "--",
        args.dimension,
    ]
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=20)
        output = result.stdout + result.stderr
    except subprocess.TimeoutExpired as error:
        output = (error.stdout or b"").decode(errors="replace") + (error.stderr or b"").decode(errors="replace")
        args.log.write_text(output)
        print("Particle buffer rejection did not complete within 20 seconds")
        return 1
    args.log.write_text(output)
    red = re.findall(r"^PARTICLE_BUFFER_FAILURE_RECOVERED (\d+)$", output, re.MULTILINE)
    ready = re.findall(r"^PARTICLE_BUFFER_MOTION_READY (\d+)$", output, re.MULTILINE)
    errors = re.findall(r"^ERROR: (.*)$", output, re.MULTILINE)
    if args.dimension == "uniform_retry":
        recovered = re.findall(r"^PARTICLE_UNIFORM_RECOVERED (\d+)$", output, re.MULTILINE)
        passed = (
            result.returncode == 0
            and "Metal 4.0" in output
            and "Using Device #0: Apple" in output
            and ("Forward Mobile" if args.renderer == "mobile" else "Forward+") in output
            and output.count("PARTICLE_UNIFORM_FAILURE_BEFORE") == 1
            and output.count("PARTICLE_UNIFORM_FAILURE_AFTER") == 1
            and len(recovered) == 1
            and int(recovered[0]) > 0
            and errors == ["Test-only particle material uniform allocation rejection."]
            and "SCRIPT ERROR" not in output
        )
        print(
            f"renderer={args.renderer} dimension=uniform_retry rc={result.returncode} recovered_red={recovered} errors={len(errors)} pass={passed}"
        )
        return 0 if passed else 1
    expected_renderer = "Forward Mobile" if args.renderer == "mobile" else "Forward+"
    motion_retry = args.dimension == "motion_retry"
    motion_control = args.dimension == "motion_control"
    expected_error = (
        "Test-only particle instance buffer allocation rejection."
        if motion_retry
        else "Particle buffer size exceeds the rendering device buffer size limit."
    )
    passed = (
        result.returncode == 0
        and "Metal 4.0" in output
        and "Using Device #0: Apple" in output
        and expected_renderer in output
        and output.count("PARTICLE_BUFFER_FAILURE_BEFORE") == 1
        and output.count("PARTICLE_BUFFER_FAILURE_AFTER") == 1
        and (not (motion_retry or motion_control) or (len(ready) == 1 and int(ready[0]) > 0))
        and ((motion_retry or motion_control) or len(ready) == 0)
        and len(red) == 1
        and int(red[0]) > 0
        and len(errors) == (1 if motion_retry else 0 if motion_control else 2)
        and (motion_control or all(error == expected_error for error in errors))
        and "Failed to create particle material uniform set." not in output
        and 'Parameter "uniform_set" is null.' not in output
        and "SCRIPT ERROR" not in output
        and "WARNING:" not in output
    )
    print(
        f"renderer={args.renderer} dimension={args.dimension} rc={result.returncode} recovered_red={red} expected_errors={len(errors)} pass={passed}"
    )
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
