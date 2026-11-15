#!/usr/bin/env python3
"""Ensure manual particle emission preserves active emission when allocation fails."""
import argparse, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[5]
SOURCE = "servers/rendering/renderer_rd/storage_rd/particles_storage.cpp"

def body(source):
    sig = "void ParticlesStorage::particles_emit("
    start = source.index(sig); brace = source.index("{", start); depth = 0
    for i in range(brace, len(source)):
        if source[i] == "{": depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0: return source[brace:i+1]
    raise ValueError(sig)

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--source-ref"); a = ap.parse_args()
    source = subprocess.check_output(["git", "show", f"{a.source_ref}:{SOURCE}"], cwd=ROOT, text=True) if a.source_ref else (ROOT/SOURCE).read_text()
    b = body(source)
    alloc = b.index("if (particles->emission_buffer == nullptr)")
    alloc_fail = b.index('if (particles->emission_buffer == nullptr || particles->emission_storage_buffer.is_null())', alloc)
    fail_return = b.index("return;", alloc_fail)
    state = b.index("if (particles->emitting)")
    inactive = b.index("particles->inactive = false;")
    checks = [
        (state > fail_return, "emitting state changes only after allocation failure can return"),
        (state < inactive, "emitting state changes before inactive reset"),
        ("Failed to allocate particle emission buffer." in b, "allocation failure remains explicit"),
        (b.index("particles->clear = true;", state) < b.index("particles->emitting = false;", state), "clear precedes emitting reset"),
    ]
    bad = [name for ok,name in checks if not ok]
    if bad: raise AssertionError("; ".join(bad))
    print(f"particle emit allocation retry: {len(checks)}/{len(checks)} checks passed")
if __name__ == "__main__": main()
