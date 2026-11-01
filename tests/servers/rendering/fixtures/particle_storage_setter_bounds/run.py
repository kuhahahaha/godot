#!/usr/bin/env python3
"""Check RD particle storage setters reject invalid enum/count inputs."""
import argparse
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[5]
SOURCE = "servers/rendering/renderer_rd/storage_rd/particles_storage.cpp"


def body(source: str, signature: str) -> str:
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
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    source = subprocess.check_output(["git", "show", args.source_ref + ":" + SOURCE], cwd=ROOT, text=True) if args.source_ref else (ROOT / SOURCE).read_text()

    enums = (ROOT / "servers/rendering/rendering_server_enums.h").read_text()
    enum_names = ["ParticlesTransformAlign", "ParticlesTransformAlignCustomSrc", "ParticlesTransformAlignAxis", "ParticlesDrawOrder"]
    enum_defs = "\n".join("enum " + name + " " + body(enums, "enum " + name + " {") + ";" for name in enum_names)
    methods = [
        ("transform_align", "ParticlesTransformAlign", "p_transform_align"),
        ("transform_align_channel_filter", "ParticlesTransformAlignCustomSrc", "p_transform_align_channel_filter"),
        ("transform_align_axis", "ParticlesTransformAlignAxis", "p_rotation_axis"),
        ("draw_order", "ParticlesDrawOrder", "p_order"),
        ("draw_passes", "int", "p_passes"),
    ]
    functions = "\n".join("void particles_set_" + name + "(int p_particles, " + ("RSE::" + typ if typ != "int" else typ) + " " + param + ") " + body(source, "void ParticlesStorage::particles_set_" + name + "(") for name, typ, param in methods)
    harness = r"""
#include <cstdio>
#include <vector>
#define ERR_FAIL_NULL(p) if (!(p)) return
#define ERR_FAIL_INDEX(i, max) if ((i) < 0 || (i) >= (max)) return
#define ERR_FAIL_COND(cond) if (cond) return
namespace RSE {
""" + enum_defs + r"""
}
struct Particles {
    RSE::ParticlesTransformAlign transform_align = RSE::PARTICLES_TRANSFORM_ALIGN_DISABLED;
    RSE::ParticlesTransformAlignCustomSrc transform_align_src = RSE::PARTICLES_ALIGN_CHANNEL_FILTER_DISABLED;
    RSE::ParticlesTransformAlignAxis transform_align_axis = RSE::PARTICLES_ALIGN_AXIS_X;
    RSE::ParticlesDrawOrder draw_order = RSE::PARTICLES_DRAW_ORDER_INDEX;
    std::vector<int> draw_passes;
};
struct Owner { Particles value; Particles *get_or_null(int id) { return id == 1 ? &value : nullptr; } } particles_owner;
""" + functions + r"""
int main() {
    int checks = 0, failures = 0;
    auto check = [&](bool result) { checks++; if (!result) failures++; };
    auto &p = particles_owner.value;
    auto exercise = [&](auto setter, auto &field, int maximum) {
        using T = decltype(field + 0);
        (void)sizeof(T);
        for (int valid = 0; valid < maximum; valid++) {
            setter(valid);
            check(int(field) == valid);
        }
        for (int invalid : {-1, maximum, 99}) {
            setter(invalid);
            check(int(field) == maximum - 1);
        }
    };
    exercise([](int value) { particles_set_transform_align(1, static_cast<RSE::ParticlesTransformAlign>(value)); }, p.transform_align, RSE::PARTICLES_TRANSFORM_ALIGN_LOCAL_BILLBOARD + 1);
    exercise([](int value) { particles_set_transform_align_channel_filter(1, static_cast<RSE::ParticlesTransformAlignCustomSrc>(value)); }, p.transform_align_src, RSE::PARTICLES_ALIGN_CHANNEL_FILTER_MAX);
    exercise([](int value) { particles_set_transform_align_axis(1, static_cast<RSE::ParticlesTransformAlignAxis>(value)); }, p.transform_align_axis, RSE::PARTICLES_ALIGN_AXIS_MAX);
    exercise([](int value) { particles_set_draw_order(1, static_cast<RSE::ParticlesDrawOrder>(value)); }, p.draw_order, RSE::PARTICLES_DRAW_ORDER_VIEW_DEPTH + 1);
    particles_set_draw_passes(1, 3);
    check(p.draw_passes.size() == 3);
    particles_set_draw_passes(1, 0);
    check(p.draw_passes.size() == 3);
    try { particles_set_draw_passes(1, -1); } catch (...) { failures++; }
    check(p.draw_passes.size() == 3);
    std::printf("particle storage setter bounds: %d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
"""
    with tempfile.TemporaryDirectory(prefix="particle-setter-bounds-") as temp:
        cpp, binary = Path(temp) / "test.cpp", Path(temp) / "test"
        cpp.write_text(harness)
        subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", str(cpp), "-o", str(binary)], check=True)
        return subprocess.run([str(binary)], timeout=10).returncode


if __name__ == "__main__":
    raise SystemExit(main())
