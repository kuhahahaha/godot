#!/usr/bin/env python3
"""Check particle update failures stay local to the current node."""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[5]
SOURCE = ROOT / "servers/rendering/renderer_rd/storage_rd/particles_storage.cpp"


def read_source(ref: str | None) -> str:
    if ref:
        return subprocess.check_output(
            ["git", "show", f"{ref}:servers/rendering/renderer_rd/storage_rd/particles_storage.cpp"],
            cwd=ROOT,
            text=True,
        )
    return SOURCE.read_text()


def update_body(source: str) -> str:
    start = source.index("void ParticlesStorage::update_particles()")
    end = source.index("\n}\n", start) + 2
    return source[start:end]


def main() -> int:
    ref = sys.argv[sys.argv.index("--source-ref") + 1] if "--source-ref" in sys.argv else None
    body = update_body(read_source(ref))
    failures = []

    # Allocation and upload failures inside the per-node loop must advance to
    # the next node. The only function-level return is the empty-queue fast path.
    if re.search(r"if \(!_particles_get_buffer_size\([^\n]+\)\) \{[^}]*\breturn;", body, re.S):
        failures.append("frame/trail allocation failure returns from batch")
    if re.search(r"if \(!_particles_ensure_trail_uniform\([^\n]+\)\) \{[^}]*\breturn;", body, re.S):
        failures.append("trail uniform failure returns from batch")
    if body.count("\t\t\t\t\treturn;") > 0:
        failures.append("per-node preparation still contains a return")

    upload = body[
        body.index("if (particles->trail_bind_pose_buffer.is_valid() && particles->trail_bind_poses_dirty)") :
    ]
    if "buffer_update" not in upload or "particles->trail_bind_poses_dirty = false;" not in upload:
        failures.append("successful trail bind pose upload does not clear dirty state")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return 1
    print("particle update queue guard: 3/3 checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
