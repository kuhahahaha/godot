#!/usr/bin/env python3
"""Check CPUParticles3D maintains its Mesh changed connection lifecycle."""

import argparse
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]


def load_source(source_ref: str | None) -> str:
    path = "scene/3d/cpu_particles_3d.cpp"
    if source_ref:
        return subprocess.check_output(["git", "show", f"{source_ref}:{path}"], cwd=ROOT, text=True)
    return (ROOT / path).read_text()


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace : index + 1]
    raise ValueError(signature)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    body = function_body(load_source(args.source_ref), "void CPUParticles3D::set_mesh(")
    checks = [
        "mesh->disconnect_changed(callable_mp((Node *)this, &Node::update_configuration_warnings));",
        "mesh->connect_changed(callable_mp((Node *)this, &Node::update_configuration_warnings), CONNECT_DEFERRED | CONNECT_REFERENCE_COUNTED);",
    ]
    for check in checks:
        if check not in body:
            raise AssertionError(f"missing mesh changed lifecycle contract: {check}")
    print(f"CPU particle mesh sync: {len(checks)}/{len(checks)} checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
