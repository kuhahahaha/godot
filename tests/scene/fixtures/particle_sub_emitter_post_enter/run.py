#!/usr/bin/env python3
"""Check GPU particle sub-emitters retry binding after the full tree enters."""

import argparse
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]


def load_source(path: str, source_ref: str | None) -> str:
    if source_ref:
        return subprocess.check_output(["git", "show", f"{source_ref}:{path}"], cwd=ROOT, text=True)
    return (ROOT / path).read_text()


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening : index + 1]
    raise ValueError(signature)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    checks = 0
    failures = []
    for path, class_name in {
        "scene/2d/gpu_particles_2d.cpp": "GPUParticles2D",
        "scene/3d/gpu_particles_3d.cpp": "GPUParticles3D",
    }.items():
        source = load_source(path, args.source_ref)
        marker = "case NOTIFICATION_POST_ENTER_TREE:"
        notification = function_body(source, f"void {class_name}::_notification(")
        checks += 1
        if marker not in notification:
            failures.append(f"{path}: missing post-enter-tree sub-emitter rebind")
        else:
            post_enter = notification.split(marker, 1)[1].split("break;", 1)[0]
            if "_attach_sub_emitter();" not in post_enter:
                failures.append(f"{path}: missing post-enter-tree sub-emitter rebind")
        checks += 1
        body = function_body(source, f"void {class_name}::_attach_sub_emitter()")
        if "RID sub_emitter_rid;" not in body or "particles_set_subemitter(particles, sub_emitter_rid);" not in body:
            failures.append(f"{path}: invalid sub-emitter targets must clear the backend link")
    if failures:
        for failure in failures:
            print("FAIL:", failure)
        print(f"particle sub-emitter post-enter binding: {checks - len(failures)}/{checks} checks passed")
        return 1
    print(f"particle sub-emitter post-enter binding: {checks}/{checks} checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
