#!/usr/bin/env python3
"""Check CPU point emission disables a particle when its point array is empty."""
from pathlib import Path
import argparse
import subprocess

ROOT = Path(__file__).resolve().parents[4]

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
    failures = []
    for dimension in (2, 3):
        path = f"scene/{dimension}d/cpu_particles_{dimension}d.cpp"
        source = (subprocess.check_output(["git", "show", f"{args.source_ref}:{path}"], cwd=ROOT, text=True)
                  if args.source_ref else (ROOT / path).read_text())
        section = body(source, f"void CPUParticles{dimension}D::_particles_process(")
        branch = body(section, "if (pc == 0)")
        if "p.active = false;" not in branch or "continue;" not in branch:
            failures.append(f"{path}: empty point emission must deactivate and skip the particle")
    if failures:
        print("\n".join(failures))
        return 1
    print("cpu particle empty points: 2/2 checks passed")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
