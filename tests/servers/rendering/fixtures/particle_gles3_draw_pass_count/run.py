#!/usr/bin/env python3
"""Compile and exercise the GLES3 draw-pass count setter with a small owner model."""
import argparse
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[5]
SOURCE = "drivers/gles3/storage/particles_storage.cpp"


def body(source: str) -> str:
    signature = "void ParticlesStorage::particles_set_draw_passes("
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        depth += (source[pos] == "{") - (source[pos] == "}")
        if depth == 0:
            return source[opening : pos + 1]
    raise ValueError(signature)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    source = subprocess.check_output(["git", "show", f"{args.source_ref}:{SOURCE}"], cwd=ROOT, text=True) if args.source_ref else (ROOT / SOURCE).read_text()
    cpp = r'''
#include <vector>
#include <cstdio>
#define ERR_FAIL_NULL(v) if (!(v)) return
#define ERR_FAIL_COND_MSG(c, m) if (c) return
using RID = int;
struct Dependency { enum { DEPENDENCY_CHANGED_PARTICLES }; int changes = 0; void changed_notify(int) { changes++; } };
struct Particles { std::vector<int> draw_passes; Dependency dependency; };
struct Owner { Particles object; Particles *get_or_null(int rid) { return rid == 1 ? &object : nullptr; } } particles_owner;
void particles_set_draw_passes(RID p_particles, int p_passes) ''' + body(source) + r'''
int main() {
    auto &p = particles_owner.object;
    particles_set_draw_passes(1, 2);
    bool valid = p.draw_passes.size() == 2 && p.dependency.changes == 1;
    bool invalid_threw = false;
    particles_set_draw_passes(1, 0);
    try { particles_set_draw_passes(1, -1); } catch (...) { invalid_threw = true; }
    particles_set_draw_passes(1, 5);
    bool invalid = !invalid_threw && p.draw_passes.size() == 2 && p.dependency.changes == 1;
    particles_set_draw_passes(1, 4);
    bool upper = p.draw_passes.size() == 4 && p.dependency.changes == 2;
    particles_set_draw_passes(0, 3);
    bool rid = p.draw_passes.size() == 4 && p.dependency.changes == 2;
    std::printf("GLES3 draw-pass count: %d/4 checks passed\n", int(valid)+int(invalid)+int(upper)+int(rid));
    return valid && invalid && upper && rid ? 0 : 1;
}
'''
    with tempfile.TemporaryDirectory(prefix="particle-gles3-draw-passes-") as temp:
        src, exe = Path(temp) / "test.cpp", Path(temp) / "test"
        src.write_text(cpp)
        subprocess.run(["c++", "-std=c++17", str(src), "-o", str(exe)], check=True)
        return subprocess.run([str(exe)]).returncode


if __name__ == "__main__":
    raise SystemExit(main())
