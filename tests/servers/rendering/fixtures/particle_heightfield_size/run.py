#!/usr/bin/env python3
"""Execute production heightfield sizing for thin and degenerate extents."""
from __future__ import annotations

import argparse
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[5]
SOURCE_PATH = ROOT / "servers/rendering/renderer_rd/storage_rd/particles_storage.cpp"


def read_source(source_ref: str | None) -> str:
    if source_ref:
        return subprocess.check_output(
            ["git", "show", f"{source_ref}:servers/rendering/renderer_rd/storage_rd/particles_storage.cpp"],
            cwd=ROOT,
            text=True,
        )
    return SOURCE_PATH.read_text()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    source = read_source(args.source_ref)
    start = source.index("RID ParticlesStorage::particles_collision_get_heightfield_framebuffer")
    start = source.index("\t\tconst int resolutions", start)
    end = source.index("\n\t\tRD::TextureFormat tf;", start)
    body = source[start:end]
    harness = r'''
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <limits>
#define MAX(a, b) std::max((a), (b))
#ifdef REAL_T_IS_DOUBLE
using real_t = double;
#else
using real_t = float;
#endif
namespace RSE { enum { PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_MAX = 6 }; }
struct Vector3 { real_t x, y, z; };
struct Size2i { int32_t x = 0, y = 0; };
struct ParticlesCollision { Vector3 extents; int heightfield_resolution; };
Size2i get_size(real_t x, real_t z, int resolution = 2) {
    ParticlesCollision collision{{x, 0, z}, resolution};
    const ParticlesCollision *particles_collision = &collision;
''' + body + r'''
    return size;
}
int main() {
    int checks = 0, failures = 0;
    auto check = [&](real_t x, real_t z, int resolution, int width, int height, const char *name) {
        checks++;
        const Size2i actual = get_size(x, z, resolution);
        if (actual.x != width || actual.y != height) {
            failures++;
            std::fprintf(stderr, "FAIL: %s: got %d x %d, expected %d x %d\n", name, actual.x, actual.y, width, height);
        }
    };
    check(2, 2, 2, 1024, 1024, "equal positive extents remain square");
    check(2, 1, 2, 1024, 512, "X-dominant aspect ratio is preserved");
    check(1, 2, 2, 512, 1024, "Z-dominant aspect ratio is preserved");
    check(1, real_t(1e-9), 2, 1024, 1, "thin Z extent retains one row");
    check(real_t(1e-9), 1, 2, 1, 1024, "thin X extent retains one column");
    check(real_t(1e-9), real_t(1e-9), 2, 1024, 1024, "small equal extents retain full resolution");
    check(std::numeric_limits<real_t>::max(), std::numeric_limits<real_t>::min(), 2, 1024, 1, "underflowed Z ratio retains one row");
    check(std::numeric_limits<real_t>::min(), std::numeric_limits<real_t>::max(), 2, 1, 1024, "underflowed X ratio retains one column");
    check(0, 0, 2, 1024, 1024, "both zero extents use square fallback");
    check(0, 3, 2, 1024, 1024, "zero X uses square fallback");
    check(3, 0, 2, 1024, 1024, "zero Z uses square fallback");
    check(-1, 3, 2, 1024, 1024, "negative X uses square fallback");
    check(3, -1, 2, 1024, 1024, "negative Z uses square fallback");
    check(-1, -1, 2, 1024, 1024, "both negative extents use square fallback");
    check(1, real_t(1e-9), 0, 256, 1, "lowest resolution keeps thin row");
    check(real_t(1e-9), 1, 5, 1, 8192, "highest resolution keeps thin column");
    check(0, 1, 0, 256, 256, "fallback honors lowest resolution");
    check(1, 0, 5, 8192, 8192, "fallback honors highest resolution");
    std::printf("particle heightfield size (%s): %d/%d checks passed\n", sizeof(real_t) == sizeof(double) ? "double" : "float", checks - failures, checks);
    return failures ? 1 : 0;
}
'''
    result = 0
    with tempfile.TemporaryDirectory(prefix="particle-heightfield-size-") as temp:
        cpp = Path(temp) / "test.cpp"
        cpp.write_text(harness)
        for precision in ("float", "double"):
            binary = Path(temp) / precision
            command = ["c++", "-std=c++17", "-Wall", "-Wextra"]
            if precision == "double":
                command.append("-DREAL_T_IS_DOUBLE")
            subprocess.run(command + [str(cpp), "-o", str(binary)], check=True)
            result |= subprocess.run([str(binary)], timeout=10).returncode
    return result


if __name__ == "__main__":
    raise SystemExit(main())
