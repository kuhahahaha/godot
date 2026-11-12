#!/usr/bin/env python3
"""Check failed particle collision-set rebuilds force a retry rebuild."""

import argparse
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[5]
SOURCE = ROOT / "servers/rendering/renderer_rd/storage_rd/particles_storage.cpp"


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
    source = (
        subprocess.check_output(
            ["git", "show", f"{args.source_ref}:servers/rendering/renderer_rd/storage_rd/particles_storage.cpp"],
            cwd=ROOT,
            text=True,
        )
        if args.source_ref
        else SOURCE.read_text()
    )
    body = function_body(source, "bool ParticlesStorage::_particles_process(")

    rebuilt = "bool collision_set_rebuilt = false;"
    cleanup = "if (collision_set_rebuilt)"
    invalidate = "p_particles->collision_textures_uniform_set = RID();"
    mark = "collision_set_rebuilt = true;"
    rebuild_branch = "if (different || !uniform_set_valid)"

    for needle in (rebuilt, cleanup, invalidate, mark, rebuild_branch):
        if needle not in body:
            raise AssertionError(f"missing collision retry guard: {needle}")

    fail_start = body.index("auto fail_process")
    fail_end = body.index("};", fail_start) + 2
    fail_body = body[fail_start:fail_end]
    if fail_body.index(cleanup) > fail_body.index("p_particles->phase = previous_phase"):
        raise AssertionError("collision descriptor cleanup must precede CPU rollback")

    branch_start = body.index(rebuild_branch)
    branch_end = body.index("\n\t\t}", branch_start)
    branch_body = body[branch_start:branch_end]
    if branch_body.index(mark) > branch_body.index("if (uniform_set_valid)"):
        raise AssertionError("collision rebuild marker must be set before freeing/replacing the old set")

    print("particle collision uniform retry: 7/7 checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
