#!/usr/bin/env python3
"""Check CPU particle birth animation offsets using production initialization code."""

import argparse
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]


def initialization(source):
    start = source.index("\t\t\treal_t tex_anim_offset = 1.0;")
    end = source.index("\n\t\t\t}", start) + len("\n\t\t\t}")
    assignment_start = source.index("\t\t\tp.custom[2] =", end)
    assignment_end = source.index(";", assignment_start) + 1
    return source[start:end] + "\n" + source[assignment_start:assignment_end]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref")
    args = parser.parse_args()
    functions = []
    for dimension in (2, 3):
        path = f"scene/{dimension}d/cpu_particles_{dimension}d.cpp"
        source = (
            subprocess.check_output(["git", "show", args.source_ref + ":" + path], cwd=ROOT, text=True)
            if args.source_ref
            else (ROOT / path).read_text()
        )
        functions.append(
            f"real_t initialize_{dimension}d(const CurveRef *curve_parameters) {{\n"
            + r"""
    const real_t tv = 0.0;
    real_t parameters_min[PARAM_MAX] = {}, parameters_max[PARAM_MAX] = {};
    parameters_min[PARAM_ANIM_OFFSET] = 0.25;
    parameters_max[PARAM_ANIM_OFFSET] = 0.75;
    struct { real_t custom[4] = {}; real_t anim_offset_rand = 0.5; } p;
"""
            + initialization(source)
            + "\nreturn p.custom[2];\n}"
        )
    harness = (
        r"""
#include <cmath>
#include <cstdio>
using real_t = double;
enum { PARAM_ANGLE, PARAM_ANIM_OFFSET, PARAM_MAX };
struct Curve { real_t value; real_t sample(real_t) const { return value; } };
struct CurveRef {
    const Curve *curve = nullptr;
    bool is_valid() const { return curve != nullptr; }
    const Curve *operator->() const { return curve; }
};
namespace Math { real_t lerp(real_t a, real_t b, real_t t) { return a + (b - a) * t; } }
"""
        + "\n".join(functions)
        + r"""
int main() {
    const Curve angle{0.8}, offset{0.2}, zero{0.0};
    const CurveRef inputs[][PARAM_MAX] = {
        { {}, {} },
        { {}, {&offset} },
        { {&angle}, {} },
        { {&angle}, {&offset} },
        { {&angle}, {&zero} },
    };
    const real_t expected[] = {0.5, 0.1, 0.5, 0.1, 0.0};
    int checks = 0, failures = 0;
    for (auto initialize : {initialize_2d, initialize_3d}) {
        for (int i = 0; i < 5; i++) {
            checks++;
            const real_t result = initialize(inputs[i]);
            if (std::abs(result - expected[i]) > 0.000001) {
                failures++;
                std::fprintf(stderr, "FAIL: case %d expected %.3f, got %.3f\n", checks, expected[i], result);
            }
        }
    }
    std::printf("CPU particle birth animation offset: %d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
"""
    )
    harness = "#include <initializer_list>\n" + harness
    with tempfile.TemporaryDirectory(prefix="cpu-particle-animation-offset-") as temp:
        cpp, binary = Path(temp) / "test.cpp", Path(temp) / "test"
        cpp.write_text(harness)
        subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", str(cpp), "-o", str(binary)], check=True)
        return subprocess.run([str(binary)], timeout=10).returncode


if __name__ == "__main__":
    raise SystemExit(main())
