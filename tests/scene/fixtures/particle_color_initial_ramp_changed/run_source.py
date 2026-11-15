#!/usr/bin/env python3
"""Supporting verbatim-method harness; not an engine/GPU acceptance test."""
import argparse
from pathlib import Path
import subprocess
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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path)
    args = parser.parse_args()
    source = args.source.read_text()
    methods = function(source, 'void ParticleProcessMaterial::set_color_initial_ramp(')
    if 'void ParticleProcessMaterial::_color_initial_ramp_changed()' in source:
        methods = function(source, 'void ParticleProcessMaterial::_color_initial_ramp_changed()') + '\n' + methods
    harness = Path(__file__).with_name('source_harness.cpp.in').read_text()
    with tempfile.TemporaryDirectory(prefix='color-initial-ramp-source-') as tmp:
        cpp, binary = Path(tmp) / 'test.cpp', Path(tmp) / 'test'
        cpp.write_text(harness.replace('// EXTRACTED_METHODS', methods))
        subprocess.run(['clang++', '-std=c++17', str(cpp), '-o', str(binary)], check=True)
        return subprocess.run([str(binary)], timeout=10).returncode


if __name__ == '__main__':
    raise SystemExit(main())
