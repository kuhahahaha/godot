#!/usr/bin/env python3
"""Exercise non-negative collision base-size guards in particle nodes/storage."""
from __future__ import annotations

import argparse
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[5]
SCENE_SOURCES = [
    "scene/2d/gpu_particles_2d.cpp",
    "scene/3d/gpu_particles_3d.cpp",
]
STORAGE_SOURCES = [
    "servers/rendering/renderer_rd/storage_rd/particles_storage.cpp",
    "drivers/gles3/storage/particles_storage.cpp",
]


def read_source(path: str, ref: str | None) -> str:
    if ref:
        return subprocess.check_output(["git", "show", f"{ref}:{path}"], cwd=ROOT, text=True)
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


def run_harness(rd_source: str, gles_source: str) -> int:
    rd_helper = "{ return Math::is_finite(p_size); }"
    helper_signature = "bool ParticlesStorage::_particles_collision_base_size_is_valid("
    if helper_signature in rd_source:
        rd_helper = function_body(rd_source, helper_signature)
    rd_body = function_body(rd_source, "void ParticlesStorage::particles_set_collision_base_size(")
    gles_body = function_body(gles_source, "void ParticlesStorage::particles_set_collision_base_size(")
    harness = r'''
#include <cmath>
#include <cstdio>
using real_t = float;
struct RID { int id = 0; };
#define ERR_FAIL_NULL(p) if (!(p)) return
#define ERR_FAIL_COND_MSG(cond, message) if (cond) return
namespace Math { inline bool is_finite(double value) { return std::isfinite(value); } }
struct ExternalOutputOwner {};
struct Particles { real_t collision_base_size = 0.01f; ExternalOutputOwner *external = nullptr; };
struct Owner { Particles value; Particles *get_or_null(RID rid) { return rid.id == 1 ? &value : nullptr; } } particles_owner;
bool _particles_collision_base_size_is_valid(real_t p_size) ''' + rd_helper + r'''
void rd_set(RID p_particles, real_t p_size) ''' + rd_body + r'''
void gles_set(RID p_particles, real_t p_size) ''' + gles_body + r'''
int main() {
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char *name) {
        checks++;
        if (!ok) { failures++; std::fprintf(stderr, "FAIL: %s\n", name); }
    };
    RID rid{1};
    rd_set(rid, 0.5f);
    check(particles_owner.value.collision_base_size == 0.5f, "RD accepts non-negative size");
    rd_set(rid, -1.0f);
    check(particles_owner.value.collision_base_size == 0.5f, "RD rejects negative size");
    rd_set(rid, std::nanf(""));
    check(particles_owner.value.collision_base_size == 0.5f, "RD rejects non-finite size");
    ExternalOutputOwner external;
    particles_owner.value.external = &external;
    rd_set(rid, 0.75f);
    check(particles_owner.value.collision_base_size == 0.5f, "RD rejects mutable updates to external output");
    particles_owner.value.external = nullptr;
    gles_set(rid, 0.25f);
    check(particles_owner.value.collision_base_size == 0.25f, "GLES3 accepts non-negative size");
    gles_set(rid, -1.0f);
    check(particles_owner.value.collision_base_size == 0.25f, "GLES3 rejects negative size");
    gles_set(rid, std::nanf(""));
    check(particles_owner.value.collision_base_size == 0.25f, "GLES3 rejects non-finite size");
    std::printf("particle collision base size: %d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
'''
    with tempfile.TemporaryDirectory(prefix="particle-collision-base-size-") as temp:
        cpp = Path(temp) / "test.cpp"
        binary = Path(temp) / "test"
        cpp.write_text(harness)
        subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", str(cpp), "-o", str(binary)], check=True)
        return subprocess.run([str(binary)], timeout=10).returncode


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    scene_sources = {path: read_source(path, args.source_ref) for path in SCENE_SOURCES}
    storage_sources = {path: read_source(path, args.source_ref) for path in STORAGE_SOURCES}

    checks = 0
    for path, source in scene_sources.items():
        body = function_body(source, "void GPUParticles" + ("2D" if "/2d/" in path else "3D") + "::set_collision_base_size(")
        if "p_size < 0.0" not in body:
            raise AssertionError(f"{path}: node setter must reject negative collision base size")
        checks += 1
    rd_source = storage_sources[STORAGE_SOURCES[0]]
    rd_helper = function_body(rd_source, "bool ParticlesStorage::_particles_collision_base_size_is_valid(")
    if "p_size >= 0.0" not in rd_helper:
        raise AssertionError("RD collision base-size validator must reject negative values")
    checks += 1
    gles_body = function_body(storage_sources[STORAGE_SOURCES[1]], "void ParticlesStorage::particles_set_collision_base_size(")
    if "p_size < 0.0" not in gles_body:
        raise AssertionError("GLES3 collision base-size setter must reject negative values")
    checks += 1
    if args.source_ref is None:
        result = run_harness(rd_source, storage_sources[STORAGE_SOURCES[1]])
        if result:
            return result
    print(f"particle collision base size guards: {checks}/{checks} source checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
