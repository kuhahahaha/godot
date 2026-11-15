#!/usr/bin/env python3
"""Observe a retained native SceneOnly receiver through two real Metal views.

This test-only control preserves missing public Scene connection and pair
membership evidence as explicit unverified scope. Legacy getter errors fail.
PNG dimensions are checked here; raw RGBA comparison/hashes come from the
fixture's actual Image data, rather than compressed PNG bytes.
"""

import argparse
import json
import os
import re
import struct
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
PREFIX = "SCENE_VIEWPORT_INVARIANCE_NATIVE "
REQUIRED_CHECKS = {
    "managed_phase_original_occurrence",
    "managed_phase_original_device",
    "managed_phase_live_original_frame",
    "managed_original_record",
    "managed_phase_no_draw_admission_closed",
    "managed_phase_waiting_return_without_draw",
    "managed_phase_typed_only",
    "managed_phase_original_expiry",
    "managed_phase_close_waits_holder",
    "managed_phase_original_free",
    "managed_phase_original_close",
    "managed_original_root_return",
    "managed_original_request_release",
    "managed_enrollment_cleanup",
    "viewport_receiver_query_committed",
    "viewport_receiver_snapshot",
    "viewport_original_root_live",
    "viewport_receiver_retained_until_observation",
    "viewport_post_draw_snapshot",
    "viewport_post_draw_source_unchanged",
    "viewport_post_draw_query_unchanged",
    "viewport_post_draw_revisions_unchanged",
}


def valid_png(value, capture_dir):
    if not isinstance(value, str):
        return False
    path = Path(value)
    if not path.is_absolute() or capture_dir not in path.resolve().parents or not path.is_file():
        return False
    with path.open("rb") as stream:
        header = stream.read(24)
    return (
        len(header) == 24
        and header[:8] == b"\x89PNG\r\n\x1a\n"
        and header[12:16] == b"IHDR"
        and struct.unpack(">II", header[16:24]) == (128, 128)
    )


def valid_report(report, capture_dir):
    if not isinstance(report, dict) or report.get("pass") is not True or report.get("mode") != "managed":
        return False
    checks = report.get("checks")
    views = report.get("views")
    if not (
        isinstance(checks, dict)
        and REQUIRED_CHECKS <= checks.keys()
        and all(value is True for value in checks.values())
        and isinstance(views, dict)
        and set(views) == {"top", "side"}
        and report.get("pair_membership") == "not-observed"
        and report.get("external_scene_connection") == "not-present"
    ):
        return False
    paths = set()
    for view in views.values():
        if not isinstance(view, dict):
            return False
        before_hash = view.get("before_sha256")
        if not (
            type(view.get("before_draw_calls")) is int
            and view["before_draw_calls"] == 2
            and type(view.get("after_draw_calls")) is int
            and view["after_draw_calls"] == 2
            and type(view.get("width")) is int
            and view["width"] == 128
            and type(view.get("height")) is int
            and view["height"] == 128
            and type(view.get("before_nonuniform_pixels")) is int
            and 0 < view["before_nonuniform_pixels"] < 128 * 128
            and type(view.get("after_nonuniform_pixels")) is int
            and 0 < view["after_nonuniform_pixels"] < 128 * 128
            and view.get("rgba_equal") is True
            and isinstance(before_hash, str)
            and re.fullmatch(r"[0-9a-f]{64}", before_hash)
            and view.get("after_sha256") == before_hash
            and view.get("before_path") != view.get("after_path")
            and valid_png(view.get("before_path"), capture_dir)
            and valid_png(view.get("after_path"), capture_dir)
        ):
            return False
        paths.update((view["before_path"], view["after_path"]))
    return len(paths) == 4


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--capture-dir", type=Path, required=True)
    parser.add_argument("--display-driver", default="macos")
    args = parser.parse_args()
    failures = []
    for method, renderer in (("mobile", "Forward Mobile"), ("forward_plus", "Forward+")):
        capture_dir = args.capture_dir.resolve() / method
        capture_dir.mkdir(parents=True, exist_ok=True)
        command = [
            str(args.binary.resolve()),
            "--display-driver",
            args.display_driver,
            "--rendering-driver",
            "metal",
            "--rendering-method",
            method,
            "--path",
            str(HERE),
            "--script",
            "res://run.gd",
        ]
        env = os.environ.copy()
        env.pop("ANGE_SCENE_VIEWPORT_BASELINE_ONLY", None)
        env["ANGE_SCENE_VIEWPORT_CAPTURE_DIR"] = str(capture_dir)
        print(f"SCENE_VIEWPORT_INVARIANCE_CASE renderer={method}", flush=True)
        print("COMMAND " + json.dumps(command), flush=True)
        result = subprocess.run(
            command,
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=90,
        )
        print(result.stdout, flush=True)
        failure = None
        if result.returncode != 0:
            failure = f"Viewport control exited {result.returncode} on {renderer}"
        elif "ERROR:" in result.stdout or "WARNING:" in result.stdout:
            failure = f"Viewport control emitted an error or warning on {renderer}"
        startup = re.findall(
            rf"^Metal [0-9.]+ - {re.escape(renderer)} - Using Device #[0-9]+: .+$",
            result.stdout,
            re.MULTILINE,
        )
        if len(startup) != 1 and failure is None:
            failure = f"Missing real Metal/{renderer} renderer evidence"
        reports = [json.loads(line[len(PREFIX) :]) for line in result.stdout.splitlines() if line.startswith(PREFIX)]
        if (len(reports) != 1 or not valid_report(reports[0], capture_dir)) and failure is None:
            failure = f"Receiver lifetime/ordinary viewport control failed on {renderer}"
        if failure is not None:
            failures.append(failure)
            print("SCENE_VIEWPORT_INVARIANCE_CASE_FAIL " + failure, flush=True)
    if failures:
        raise SystemExit("\n".join(failures))
    print(
        "SCENE_VIEWPORT_INVARIANCE_MATRIX_PASS 4 real Metal view comparisons; public Scene connection and pair membership unverified"
    )


if __name__ == "__main__":
    main()
