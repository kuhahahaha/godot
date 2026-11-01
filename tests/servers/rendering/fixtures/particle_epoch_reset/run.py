#!/usr/bin/env python3
"""Verify resource teardown invalidates all particle simulation epoch state."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[5]
SOURCE = ROOT / "servers/rendering/renderer_rd/storage_rd/particles_storage.cpp"


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for i in range(brace, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[brace:i + 1]
    raise ValueError(signature)


def main() -> int:
    source = SOURCE.read_text()
    body = function_body(source, "void ParticlesStorage::_particles_free_data(Particles *particles)")
    checks = {
        "frame remainder reset": r"particles->frame_remainder\s*=\s*0\.0",
        "retry marker reset": r"particles->retry_pending\s*=\s*false",
        "delta commit reset": r"particles->frame_delta_committed\s*=\s*false",
        "delta frame reset": r"particles->frame_delta_committed_frame\s*=\s*UINT32_MAX",
        "pre-process remainder reset": r"particles->pre_process_time_remaining\s*=\s*0\.0",
        "pre-process pending reset": r"particles->pre_process_pending\s*=\s*false",
    }
    update_start = source.index("void ParticlesStorage::update_particles()")
    update_body = source[update_start:]
    failures = [name for name, pattern in checks.items() if re.search(pattern, body) is None]
    update_checks = {
        "retry marker rechecked after buffer preparation": r"bool retrying = retry_candidate && particles->frame_delta_committed",
        "restart invalidates retry candidate": r"retrying\s*=\s*false",
    }
    failures.extend(name for name, pattern in update_checks.items() if re.search(pattern, update_body) is None)
    total = len(checks) + len(update_checks)
    print(f"particle epoch reset: {total - len(failures)}/{total} checks passed")
    if failures:
        print("missing: " + ", ".join(failures))
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
