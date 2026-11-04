#!/usr/bin/env python3
"""Compile all real guard variants and execute its scalar control flow on CPU.

This is a component fixture: sequential CPU lanes do not prove GPU memory
ordering, E09 provenance, material qualification, draw adoption or retirement.
"""
import argparse
from pathlib import Path
import json
import re
import shutil
import sys
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[5]
SHADER = ROOT / 'servers/rendering/renderer_rd/shaders/particles_external_guard.glsl'


def run(command):
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result.stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--shader', type=Path, default=SHADER)
    parser.add_argument('--mutations', action='store_true')
    args = parser.parse_args()
    source = args.shader.read_text()
    glsl = source.replace('#[compute]\n', '').replace('#VERSION_DEFINES', '')
    tools = {name: shutil.which(name) for name in ('glslangValidator', 'spirv-val', 'spirv-dis', 'clang++')}
    if not all(tools.values()):
        raise SystemExit('required component tools missing: ' + str(tools))
    with tempfile.TemporaryDirectory(prefix='particle-external-guard-') as directory:
        temp = Path(directory)
        for mode in ('INIT', 'VALIDATE', 'FINALIZE'):
            shader = temp / (mode + '.comp')
            shader.write_text(glsl.replace('#version 450', '#version 450\n#define MODE_' + mode))
            spv = temp / (mode + '.spv')
            run([tools['glslangValidator'], '-V', str(shader), '-o', str(spv)])
            run([tools['spirv-val'], str(spv)])
            assembly = run([tools['spirv-dis'], str(spv)])
            for member in range(8):
                assert f'OpMemberDecorate %Params {member} Offset {member * 4}' in assembly
            for member, offset in enumerate((0, 16, 32, 48, 64, 80, 96, 144, 192, 208, 224)):
                assert f'OpMemberDecorate %GeometryProof {member} Offset {offset}' in assembly
            for index, name in enumerate(('current_records', 'previous_records', 'output_meta', 'source', 'scratch', 'status', 'commands', 'proof')):
                assert f'OpDecorate %{name} Binding {index}' in assembly
                assert f'OpDecorate %{name} DescriptorSet 0' in assembly
            assert 'ArrayStride 144' in assembly
            if mode == 'VALIDATE':
                assert assembly.count('NoContraction') >= 8
            assert 'OpExecutionMode %main LocalSize 64 1 1' in assembly
            print(mode + ': SPIR-V compile/validate/layout PASS')
        # Translate the production functions, not a reimplementation of their
        # conditions. Vectors and buffer operations supply CPU scalar semantics.
        body = source[source.index('const uint SOURCE_FAULT'):source.index('\nvoid main()')]
        main_body = source[source.index('\nvoid main()') + len('\nvoid main() {'):]
        main_body = main_body[:main_body.rfind('}')]
        def translate(text):
            text = text.replace('inout bool ', 'bool &').replace('out uint ', 'uint &').replace('precise ', '')
            for swizzle in ('xyz', 'yzw', 'zw'):
                text = re.sub(r'\.' + swizzle + r'\b', '.' + swizzle + '()', text)
            return text
        functions = translate(body)
        for mode in ('INIT', 'VALIDATE', 'FINALIZE'):
            selected = re.sub(r'#ifdef MODE_(INIT|VALIDATE|FINALIZE)\n(.*?)#endif',
                              lambda m: m[2] if m[1] == mode else '', main_body, flags=re.S)
            functions += '\nvoid shader_' + mode + '() {' + translate(selected) + '}\n'
        harness = Path(__file__).with_name('harness.cpp').read_text()
        surface = source[source.index('struct SurfaceProof {'):source.index('\nlayout(set = 0, binding = 7')]
        cpp = temp / 'guard.cpp'
        cpp.write_text(harness.replace('// PRODUCTION_SURFACE', surface).replace('// PRODUCTION_FUNCTIONS', functions))
        binary = temp / 'guard'
        run([tools['clang++'], '-std=c++17', '-O0', '-ffp-contract=off', str(cpp), '-o', str(binary)])
        print(run([str(binary)]), end='')
    if args.mutations:
        mutations = (
            ('ignore_source_validity', 'uint raw_validity = source.words[proof.source_accessor.y];', 'uint raw_validity = 1u;', 'invalid source expected'),
            ('ignore_bounds', 'record_fault |= BOUNDS_FAULT;', 'record_fault |= 0u;', 'current bounds escape expected'),
            ('ignore_history', 'if ((flags & 2u) != 0u) {', 'if (false) {', 'valid previous escape expected'),
        )
        with tempfile.TemporaryDirectory(prefix='particle-guard-mutations-') as directory:
            for name, before, after, diagnostic in mutations:
                assert source.count(before) == 1, name
                mutated = Path(directory) / (name + '.glsl')
                mutated.write_text(source.replace(before, after))
                result = subprocess.run([sys.executable, str(Path(__file__).resolve()), '--shader', str(mutated)], text=True, capture_output=True)
                assert result.returncode != 0 and diagnostic in result.stderr, (name, result.stdout, result.stderr)
                print(name + ': expected RED confirmed')
    print(json.dumps({'gpu_execution': False, 'provenance_bridge': False, 'component_only': True}))


if __name__ == '__main__':
    main()
