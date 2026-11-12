#!/usr/bin/env python3
"""Run a behavior harness against the current SortEffects::sort_buffer body.

The harness compiles the production method body against a tiny fake rendering
 device. It records compute-list dispatches and makes invalid uniform/pipeline
 RIDs observable. This exercises scheduling/fail-closed behavior only; it is
 not a GPU shader or rendering validation.
"""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[5]
SOURCE = ROOT / "servers/rendering/renderer_rd/effects/sort_effects.cpp"


def method_body(source: str) -> tuple[str, bool]:
    marker = "SortEffects::sort_buffer("
    start = source.index(marker)
    sig_start = source.rfind("\n", 0, start) + 1
    brace = source.index("{", start)
    depth = 0
    end = None
    for i in range(brace, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                end = i
                break
    if end is None:
        raise RuntimeError("sort_buffer body is not balanced")
    signature = source[sig_start:brace].strip()
    body = source[brace + 1 : end]
    return body, signature.startswith("bool ")


def generated_source(source: str) -> str:
    body, returns_bool = method_body(source)
    if not returns_bool:
        body += "\nreturn true;\n"
    return f"""#include <algorithm>
#include <array>
#include <cstring>
#define ERR_PRINT(message) do {{ (void)sizeof(message); }} while (0)
#define ERR_FAIL_COND_V_MSG(condition, value, message) do {{ if (condition) {{ ERR_PRINT(message); return value; }} }} while (0)
#define MIN(a, b) std::min((a), (b))
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

struct RID {{
    int id = 0;
    RID() = default;
    RID(int p_id) : id(p_id) {{}}
    bool is_valid() const {{ return id != 0; }}
}};

class RD {{
public:
    using ComputeListID = int;
    static RD *get_singleton() {{ static RD rd; return &rd; }}
    bool uniform_valid = true;
    std::array<bool, 4> pipeline_valid{{ false, true, true, true }};
    int begin_count = 0;
    int end_count = 0;
    int begin_result = 1;
    static constexpr ComputeListID INVALID_ID = -1;
    enum Limit {{ LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X }};
    uint64_t max_groups_x = 65535;
    uint64_t limit_get(Limit) const {{ return max_groups_x; }}
    struct Push {{ uint32_t total_elements; uint32_t pad[3]; int32_t job_params[4]; }};
    struct Dispatch {{ uint32_t groups; int y; int z; int pipeline; int barrier; int push_serial; Push push; }};
    std::vector<Dispatch> dispatches;
    Push push{{}};
    int pipeline = 0;
    int barrier = 0;
    int push_serial = 0;
    int dispatch_count = 0;
    std::vector<int> dispatch_groups;
    void reset() {{ begin_count = end_count = dispatch_count = 0; begin_result = 1; max_groups_x = 65535; dispatch_groups.clear(); dispatches.clear(); push = {{}}; pipeline = barrier = push_serial = 0; }}
    bool uniform_set_is_valid(RID p_rid) const {{ return p_rid.is_valid() && uniform_valid; }}
    bool compute_pipeline_is_valid(RID p_rid) const {{ return p_rid.id >= 0 && p_rid.id < (int)pipeline_valid.size() && pipeline_valid[p_rid.id]; }}
    ComputeListID compute_list_begin() {{ begin_count++; return begin_result; }}
    void compute_list_end() {{ end_count++; }}
    void compute_list_bind_compute_pipeline(ComputeListID, RID p_pipeline) {{ pipeline = p_pipeline.id; }}
    void compute_list_bind_uniform_set(ComputeListID, RID, int) {{}}
    void compute_list_set_push_constant(ComputeListID, const void *p_push, uint32_t p_size) {{ if (p_size == sizeof(push)) std::memcpy(&push, p_push, p_size); push_serial++; }}
    void compute_list_dispatch(ComputeListID, int p_x, int p_y, int p_z) {{ dispatch_count++; dispatch_groups.push_back(p_x); dispatches.push_back({{uint32_t(p_x), p_y, p_z, pipeline, barrier, push_serial, push}}); }}
    void compute_list_add_barrier(ComputeListID) {{ barrier++; }}
}};

namespace RendererRD {{
class SortEffects {{
private:
    enum SortMode {{ SORT_MODE_BLOCK, SORT_MODE_STEP, SORT_MODE_INNER, SORT_MODE_MAX }};
    struct PushConstant {{ uint32_t total_elements; uint32_t pad[3]; int32_t job_params[4]; }};
    std::array<RID, SORT_MODE_MAX> pipelines{{ RID(1), RID(2), RID(3) }};
public:
    bool sort_buffer(RID p_uniform_set, int p_size) {{{body}}}
}};
}} // namespace RendererRD

static bool equal(const std::vector<int> &a, std::initializer_list<int> b) {{
    return a == std::vector<int>(b);
}}

static int check_case(const char *name, RID uniform, int size, bool uniform_valid,
        bool block_valid, bool step_valid, bool inner_valid,
        bool expect_ok, bool expect_begin, std::initializer_list<int> expected_dispatches) {{
    RD *rd = RD::get_singleton();
    rd->reset();
    rd->uniform_valid = uniform_valid;
    rd->pipeline_valid = {{ false, block_valid, step_valid, inner_valid }};
    rd->begin_result = expect_begin ? 1 : RD::INVALID_ID;
    RendererRD::SortEffects effects;
    bool ok = effects.sort_buffer(uniform, size);
    bool dispatch_ok = equal(rd->dispatch_groups, expected_dispatches);
    for (const auto &dispatch : rd->dispatches) dispatch_ok = dispatch_ok && dispatch.y == 1 && dispatch.z == 1;
    bool lifecycle_ok = (rd->begin_count == (expect_ok || !expect_begin ? 1 : 0)) && (rd->end_count == (expect_ok && expect_begin ? 1 : 0));
    bool result_ok = ok == expect_ok;
    if (!dispatch_ok || !lifecycle_ok || !result_ok) {{
        std::fprintf(stderr, "%s: result=%d begin=%d end=%d dispatch=[", name, (int)ok, rd->begin_count, rd->end_count);
        for (size_t i = 0; i < rd->dispatch_groups.size(); ++i) std::fprintf(stderr, "%s%d", i ? "," : "", rd->dispatch_groups[i]);
        std::fprintf(stderr, "] expected_ok=%d\\n", (int)expect_ok);
        return 1;
    }}
    return 0;
}}

// Expected logical stages use a separate merge-width model. Only dispatch metadata
// is allocated, including for the backend boundary case; there is no particle buffer.
struct ExpectedStage {{ int pipeline; uint32_t groups; int32_t distance; int32_t partner; int32_t direction; }};
static int check_capacity(const char *name, int size, uint64_t limit) {{
    RD *rd = RD::get_singleton();
    rd->reset();
    rd->max_groups_x = limit;
    rd->uniform_valid = true;
    rd->pipeline_valid = {{ false, true, true, true }};
    RendererRD::SortEffects effects;
    bool ok = effects.sort_buffer(RID(7), size);
    if (limit == 0) {{
        if (!ok && rd->begin_count == 0 && rd->end_count == 0 && rd->dispatches.empty()) return 0;
        std::fprintf(stderr, "%s: zero capacity must fail before opening a compute list\\n", name);
        return 1;
    }}
    std::vector<ExpectedStage> stages;
    stages.push_back({{1, uint32_t((uint64_t(size) + 511) / 512), 0, 0, 0}});
    uint64_t padded = 512;
    while (padded < uint64_t(size)) padded *= 2;
    for (uint64_t width = 1024; width <= padded; width *= 2) {{
        for (uint64_t distance = width / 2; distance >= 512; distance /= 2) {{
            bool first = distance == width / 2;
            stages.push_back({{2, uint32_t(padded / 512), int32_t(distance), int32_t(first ? 2 * distance - 1 : distance), first ? -1 : 1}});
        }}
        stages.push_back({{3, uint32_t(padded / 512), 0, 0, 0}});
    }}
    bool valid = ok && rd->begin_count == 1 && rd->end_count == 1 && rd->barrier == int(stages.size()) - 1;
    size_t cursor = 0;
    for (size_t stage_index = 0; stage_index < stages.size(); stage_index++) {{
        const auto &stage = stages[stage_index];
        uint32_t covered = 0;
        while (covered < stage.groups) {{
            if (cursor == rd->dispatches.size()) {{ valid = false; break; }}
            const auto &dispatch = rd->dispatches[cursor];
            uint32_t expected_groups = uint32_t(std::min(limit, uint64_t(stage.groups - covered)));
            valid = valid && dispatch.pipeline == stage.pipeline && dispatch.barrier == int(stage_index)
                    && dispatch.groups == expected_groups && dispatch.groups > 0 && dispatch.groups <= limit
                    && dispatch.y == 1 && dispatch.z == 1
                    && dispatch.push.total_elements == uint32_t(size) && dispatch.push.pad[0] == covered
                    && dispatch.push_serial == int(cursor) + 1;
            if (stage.pipeline == 2) {{
                valid = valid && dispatch.push.job_params[0] == stage.distance && dispatch.push.job_params[1] == stage.partner
                        && dispatch.push.job_params[2] == stage.direction && dispatch.push.job_params[3] == 0;
            }}
            covered += expected_groups;
            cursor++;
        }}
    }}
    valid = valid && cursor == rd->dispatches.size() && rd->push_serial == int(cursor);
    if (!valid) {{
        std::fprintf(stderr, "%s: capacity/stage coverage failed (limit=%llu, stages=%zu, dispatches=%zu, barriers=%d, result=%d)\\n",
                name, (unsigned long long)limit, stages.size(), rd->dispatches.size(), rd->barrier, int(ok));
        return 1;
    }}
    std::printf("%s: exact coverage of %zu stages in %zu bounded dispatches\\n", name, stages.size(), cursor);
    return 0;
}}

int main() {{
    int failures = 0;
    failures += check_case("invalid uniform", RID(), 1, false, true, true, true, false, true, {{}});
    failures += check_case("invalid block pipeline", RID(7), 1, true, false, true, true, false, true, {{}});
    failures += check_case("invalid step pipeline", RID(7), 513, true, true, false, true, false, true, {{}});
    failures += check_case("invalid inner pipeline", RID(7), 513, true, true, true, false, false, true, {{}});
    failures += check_case("zero elements", RID(7), 0, true, true, true, true, false, true, {{}});
    failures += check_case("negative elements", RID(7), -1, true, true, true, true, false, true, {{}});
    failures += check_case("expired uniform", RID(7), 1, false, true, true, true, false, true, {{}});
    failures += check_case("invalid compute list", RID(7), 1, true, true, true, true, false, false, {{}});
    failures += check_case("one element", RID(7), 1, true, true, true, true, true, true, {{1}});
    failures += check_case("512 elements", RID(7), 512, true, true, true, true, true, true, {{1}});
    failures += check_case("unused merge pipelines", RID(7), 512, true, true, false, false, true, true, {{1}});
    failures += check_case("513 elements", RID(7), 513, true, true, true, true, true, true, {{2, 2, 2}});
    failures += check_case("1025 elements", RID(7), 1025, true, true, true, true, true, true, {{3, 4, 4, 4, 4, 4}});
    failures += check_capacity("zero dispatch capacity", 1025, 0);
    failures += check_capacity("1025 elements limit 1", 1025, 1);
    failures += check_capacity("1025 elements limit 2", 1025, 2);
    failures += check_capacity("2049 elements limit 1", 2049, 1);
    failures += check_capacity("2049 elements limit 2", 2049, 2);
    failures += check_capacity("16777217 elements backend boundary", 16777217, 65535);
    if (failures) {{
        std::fprintf(stderr, "sort behavior harness: %d case(s) failed\\n", failures);
        return 1;
    }}
    std::puts("sort behavior harness: all cases passed");
    return 0;
}}
"""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--expect-red", action="store_true", help="invert success: a failing harness is expected")
    parser.add_argument("--source-ref", help="read SortEffects from a git revision instead of the worktree")
    args = parser.parse_args()
    source = SOURCE.read_text()
    if args.source_ref:
        source = subprocess.check_output(
            ["git", "show", f"{args.source_ref}:{SOURCE.relative_to(ROOT)}"], cwd=ROOT, text=True
        )
    with tempfile.TemporaryDirectory(prefix="godot-sort-harness-") as td:
        src = pathlib.Path(td) / "harness.cpp"
        exe = pathlib.Path(td) / "harness"
        src.write_text(generated_source(source))
        compile_result = subprocess.run(
            ["c++", "-std=c++17", "-O0", "-Wall", "-Wextra", str(src), "-o", str(exe)], text=True, capture_output=True
        )
        if compile_result.returncode:
            print(compile_result.stdout, end="")
            print(compile_result.stderr, end="")
            return compile_result.returncode
        run_result = subprocess.run([str(exe)], text=True, capture_output=True)
        print(run_result.stdout, end="")
        print(run_result.stderr, end="")
        failed = run_result.returncode != 0
        if args.expect_red:
            return 0 if failed else 1
        return run_result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
