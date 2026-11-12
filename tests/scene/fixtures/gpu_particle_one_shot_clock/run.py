#!/usr/bin/env python3
"""Compile literal scene lifecycle bodies in a small shell (no engine/GPU execution)."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def body(source, marker):
    start = source.index('{', source.index(marker))
    depth = 0
    for end in range(start, len(source)):
        depth += (source[end] == '{') - (source[end] == '}')
        if depth == 0:
            return source[start + 1:end]
    raise ValueError(f'Unclosed source body: {marker}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-ref', help='Read production files from a Git revision')
    parser.add_argument('--mutate-unscaled', choices=['2d', '3d', 'both'])
    parser.add_argument('--output', type=Path, help='Save commands, source hashes, exit codes and output')
    args = parser.parse_args()
    receipt = {'scope': __doc__, 'results': []}
    passed = True
    for dimension in ('2d', '3d'):
        path = f'scene/{dimension}/gpu_particles_{dimension}.cpp'
        source = subprocess.check_output(['git', 'show', f'{args.source_ref}:{path}'], cwd=ROOT, text=True) if args.source_ref else (ROOT / path).read_text()
        original_hash = hashlib.sha256(source.encode()).hexdigest()
        if args.mutate_unscaled in (dimension, 'both'):
            old = 'time += get_process_delta_time() * speed_scale;'
            if source.count(old) != 1:
                raise ValueError(f'Expected exactly one scaled clock in {path}')
            source = source.replace(old, 'time += get_process_delta_time();')
        klass = f'GPUParticles{dimension.upper()}'
        harness = (HERE / 'harness.cpp.in').read_text()
        for token, marker in [('PROCESS', 'case NOTIFICATION_INTERNAL_PROCESS:'), ('RESTART', f'void {klass}::restart('), ('EMITTING', f'void {klass}::set_emitting(')]:
            harness = harness.replace(f'@{token}@', body(source, marker))
        with tempfile.TemporaryDirectory(prefix='gpu-one-shot-clock-') as temporary:
            cpp, binary = Path(temporary) / 'test.cpp', Path(temporary) / 'test'
            cpp.write_text(harness)
            command = ['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', str(cpp), '-o', str(binary)]
            compile_result = subprocess.run(command, capture_output=True, text=True, timeout=30)
            result = {'dimension': dimension, 'source_sha256': original_hash, 'tested_source_sha256': hashlib.sha256(source.encode()).hexdigest(), 'harness_sha256': hashlib.sha256(harness.encode()).hexdigest(), 'compile_command': command, 'compile_exit': compile_result.returncode, 'compile_stdout': compile_result.stdout, 'compile_stderr': compile_result.stderr}
            if compile_result.returncode == 0:
                run = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
                result.update(command=[str(binary)], exit=run.returncode, stdout=run.stdout, stderr=run.stderr)
            else:
                result.update(exit=None, stdout='', stderr='Compilation failed')
            receipt['results'].append(result)
            passed &= result['exit'] == 0
            print(dimension + ': ' + result['stdout'], end='')
            print(result['stderr'], end='')
    receipt['passed'] = passed
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(receipt, indent=2) + '\n')
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
