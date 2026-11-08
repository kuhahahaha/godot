#!/usr/bin/env python3
"""Compile all real guard variants and execute its scalar control flow on CPU.

This is a component fixture: sequential CPU lanes do not prove GPU memory
ordering, E09 provenance, material qualification, draw adoption or retirement.
"""

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[5]
SHADER = ROOT / "servers/rendering/renderer_rd/shaders/particles_external_guard.glsl"


def run(command):
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result.stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--shader", type=Path, default=SHADER)
    parser.add_argument("--mutations", action="store_true")
    parser.add_argument("--shader-only", action="store_true", help="SPIR-V/layout checks only, no CPU executable")
    parser.add_argument("--syntax-only", action="store_true", help="SPIR-V/layout plus CPU harness syntax, no executable")
    parser.add_argument("--native-source-root", type=Path, default=ROOT)
    args = parser.parse_args()
    source = args.shader.read_text()
    # Derive the same bound as the original Core owner; no mesh-RID limit is
    # substituted for the flattened surface count in either translation.
    config = (args.native_source_root / "servers/rendering/renderer_rd/storage_rd/external_output_config.h").read_text()
    enums = (args.native_source_root / "servers/rendering/rendering_server_enums.h").read_text()
    mesh_rids = int(re.search(r"MAX_MESH_RIDS = (\d+)", config)[1])
    mesh_surfaces = int(re.search(r"MAX_MESH_SURFACES = (\d+)", enums)[1])
    surface_limit = mesh_rids * mesh_surfaces
    glsl = source.replace("#[compute]\n", "").replace("#VERSION_DEFINES", "")
    tools = {name: shutil.which(name) for name in ("glslangValidator", "spirv-val", "spirv-dis", "clang++")}
    if not all(tools.values()):
        raise SystemExit("required component tools missing: " + str(tools))
    with tempfile.TemporaryDirectory(prefix="particle-external-guard-") as directory:
        temp = Path(directory)
        for mode in ("INIT", "VALIDATE", "FINALIZE", "INIT_EMPTY", "FINALIZE_EMPTY"):
            shader = temp / (mode + ".comp")
            defines = (
                "#define EXTERNAL_MAX_SURFACES "
                + str(surface_limit)
                + "u\n#define MODE_"
                + mode.removesuffix("_EMPTY")
                + "\n"
            )
            if mode.endswith("_EMPTY"):
                defines += "#define MODE_EMPTY\n"
            shader.write_text(glsl.replace("#version 450", "#version 450\n" + defines))
            spv = temp / (mode + ".spv")
            run([tools["glslangValidator"], "-V", str(shader), "-o", str(spv)])
            run([tools["spirv-val"], str(spv)])
            assembly = run([tools["spirv-dis"], str(spv)])
            for member in range(8):
                assert f"OpMemberDecorate %Params {member} Offset {member * 4}" in assembly
            for member, offset in enumerate((0, 16, 32, 48, 64, 80, 96, 144, 192, 208, 224, 240)):
                assert f"OpMemberDecorate %GeometryProof {member} Offset {offset}" in assembly, (mode, "GeometryProof member", member, "expected offset", offset)
            for index, name in enumerate((
                "current_records",
                "previous_records",
                "output_meta",
                "source_count",
                "scratch",
                "status",
                "commands",
                "proof",
                "source_count_validity",
                "source_attributes_validity",
                "source_dense_ids_validity",
                "source_instance_gate",
            )):
                if mode.endswith("_EMPTY") and index < 2:
                    assert f"OpDecorate %{name} Binding {index}" not in assembly, (
                        "empty guard must not require record descriptors"
                    )
                else:
                    assert f"OpDecorate %{name} Binding {index}" in assembly
                    assert f"OpDecorate %{name} DescriptorSet 0" in assembly
            for name in ("SourceCount", "SourceCountValidity", "SourceAttributesValidity", "SourceDenseIdsValidity", "SourceInstanceGate"):
                assert f"OpMemberDecorate %{name} 0 Restrict" not in assembly, "source READ aliases must remain legal"
                assert f"OpMemberDecorate %{name} 0 NonWritable" in assembly
            assert "ArrayStride 144" in assembly
            if mode == "VALIDATE":
                assert assembly.count("NoContraction") >= 8
            assert "OpExecutionMode %main LocalSize 64 1 1" in assembly
            print(mode + ": SPIR-V compile/validate/layout PASS")
        if args.shader_only:
            print(json.dumps({"gpu_execution": False, "cpu_execution": False, "shader_layout_only": True}))
            return
        # Translate the production functions, not a reimplementation of their
        # conditions. Vectors and buffer operations supply CPU scalar semantics.
        body = source[source.index("const uint SOURCE_FAULT") : source.index("\nvoid main()")]
        main_body = source[source.index("\nvoid main()") + len("\nvoid main() {") :]
        main_body = main_body[: main_body.rfind("}")]

        def translate(text):
            text = text.replace("inout bool ", "bool &").replace("out uint ", "uint &").replace("precise ", "")
            for swizzle in ("xyz", "yzw", "zw"):
                text = re.sub(r"\." + swizzle + r"\b", "." + swizzle + "()", text)
            return text

        functions = "#define EXTERNAL_MAX_SURFACES " + str(surface_limit) + "u\n" + translate(body)
        for mode in ("INIT", "VALIDATE", "FINALIZE"):
            selected = re.sub(
                r"#ifdef MODE_(INIT|VALIDATE|FINALIZE)\n(.*?)#endif",
                lambda m: m[2] if m[1] == mode else "",
                main_body,
                flags=re.S,
            )
            functions += "\nvoid shader_" + mode + "() {" + translate(selected) + "}\n"
        harness = Path(__file__).with_name("harness.cpp.in").read_text()
        surface = source[source.index("struct SurfaceProof {") : source.index("\nlayout(set = 0, binding = 7")]
        cpp = temp / "guard.cpp"
        cpp.write_text(harness.replace("// PRODUCTION_SURFACE", surface).replace("// PRODUCTION_FUNCTIONS", functions))
        if args.syntax_only:
            run([tools["clang++"], "-std=c++17", "-ffp-contract=off", "-fsyntax-only", str(cpp)])
            print("CPU translated harness syntax PASS (not executed)")
            return
        binary = temp / "guard"
        run([tools["clang++"], "-std=c++17", "-O0", "-ffp-contract=off", str(cpp), "-o", str(binary)])
        print(run([str(binary)]), end="")
    if args.mutations:
        mutations = (
            (
                "init_first_eight_only",
                "i < min(EXTERNAL_MAX_SURFACES * 5u, uint(commands.words.length()))",
                "i < min(8u * 5u, uint(commands.words.length()))",
                "ninth initialized command expected",
            ),
            (
                "check_first_eight_only",
                "min(min(params.surface_count, EXTERNAL_MAX_SURFACES), uint(proof.surfaces.length()))",
                "min(min(params.surface_count, 8u), uint(proof.surfaces.length()))",
                "invalid ninth surface expected",
            ),
            (
                "ignore_source_validity",
                "source_count_validity.words[proof.source_gate_words.x] != 0u",
                "false",
                "invalid source expected",
            ),
            ("ignore_attribute_gate", "source_attributes_validity.words[proof.source_gate_words.y] != 0u", "false", "each independent source gate expected"),
            ("ignore_identity_gate", "source_dense_ids_validity.words[proof.source_gate_words.z] != 0u", "false", "each independent source gate expected"),
            ("ignore_instance_gate", "source_instance_gate.words[proof.source_gate_words.w] != 0u", "false", "each independent source gate expected"),
            ("ignore_selected_count", "source_count.words[proof.source_accessor.x] != scratch.words[0]", "false", "source count mismatch expected"),
            ("omit_empty_count_gate", "source_count_validity.words[proof.source_gate_words.x] != 0u", "params.capacity != 0u && source_count_validity.words[proof.source_gate_words.x] != 0u", "N0 real source gate fault expected"),
            ("omit_empty_count", "source_count.words[proof.source_accessor.x] != scratch.words[0]", "params.capacity != 0u && source_count.words[proof.source_accessor.x] != scratch.words[0]", "N0 real selected count mismatch expected"),
            ("ignore_bounds", "record_fault |= BOUNDS_FAULT;", "record_fault |= 0u;", "current bounds escape expected"),
            ("ignore_history", "if ((flags & 2u) != 0u) {", "if (false) {", "valid previous escape expected"),
        )
        with tempfile.TemporaryDirectory(prefix="particle-guard-mutations-") as directory:
            for name, before, after, diagnostic in mutations:
                assert source.count(before) == 1, name
                mutated = Path(directory) / (name + ".glsl")
                mutated.write_text(source.replace(before, after))
                result = subprocess.run(
                    [
                        sys.executable,
                        str(Path(__file__).resolve()),
                        "--shader",
                        str(mutated),
                        "--native-source-root",
                        str(args.native_source_root),
                    ],
                    text=True,
                    capture_output=True,
                )
                assert result.returncode != 0 and diagnostic in result.stderr, (name, result.stdout, result.stderr)
                print(name + ": expected RED confirmed")
    print(json.dumps({"gpu_execution": False, "provenance_bridge": False, "component_only": True}))


if __name__ == "__main__":
    main()
