#!/usr/bin/env python3
"""Check GLES3 particle free entry points guard invalid/uninitialized RIDs."""
import argparse
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = "drivers/gles3/storage/particles_storage.cpp"

def body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for i in range(brace, len(source)):
        if source[i] == "{": depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0: return source[brace:i + 1]
    raise ValueError(signature)

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    source = (subprocess.check_output(["git", "show", f"{args.source_ref}:{SOURCE}"], cwd=ROOT, text=True)
              if args.source_ref else (ROOT / SOURCE).read_text())
    failures = []
    for name, owner_type in (("particles_free", "particles_owner"), ("particles_collision_free", "particles_collision_owner")):
        section = body(source, f"void ParticlesStorage::{name}(")
        lookup = section.index(f"{owner_type}.get_or_null")
        lookup_end = section.index(";", lookup)
        if not section[lookup_end + 1:].lstrip().startswith("ERR_FAIL_NULL("):
            failures.append(f"{name}: missing ERR_FAIL_NULL after get_or_null")
    if failures:
        print("\n".join(failures))
        return 1
    print("GLES3 particle cleanup guards: 2/2 checks passed")
    return 0
if __name__ == "__main__":
    raise SystemExit(main())
