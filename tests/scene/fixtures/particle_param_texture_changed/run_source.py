#!/usr/bin/env python3
"""Compile the indexed parameter texture methods against a focused source harness."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile


def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for pos in range(brace, len(source)):
        depth += source[pos] == '{'
        depth -= source[pos] == '}'
        if depth == 0:
            return source[start:pos + 1]
    raise ValueError(signature)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', required=True, type=Path)
    args = parser.parse_args()
    source = args.source.read_text()
    selector = ('Callable ParticleProcessMaterial::_get_param_texture_changed('
                if 'Callable ParticleProcessMaterial::_get_param_texture_changed(' in source
                else 'static bool _param_texture_has_sampler(')
    methods = '\n'.join(function(source, signature) for signature in (
        selector,
        'void ParticleProcessMaterial::_param_texture_changed(',
        'void ParticleProcessMaterial::set_param_texture(',
    ))
    harness = Path(__file__).with_name('source_harness.cpp.in').read_text()
    with tempfile.TemporaryDirectory(prefix='particle-param-texture-source-') as tmp:
        cpp, binary = Path(tmp) / 'test.cpp', Path(tmp) / 'test'
        cpp.write_text(harness.replace('// EXTRACTED_METHODS', methods))
        compile_result = subprocess.run(['clang++', '-std=c++17', str(cpp), '-o', str(binary)])
        if compile_result.returncode != 0:
            return 2  # Compilation failure is never a behavioral mutation result.
        return subprocess.run([str(binary)], timeout=10).returncode


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (ValueError, OSError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        raise SystemExit(2)
