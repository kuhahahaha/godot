#!/usr/bin/env python3
"""Run the real Metal main-RD oracle in an isolated temporary project."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

MARKER = 'PARTICLE_ROTATION_VELOCITY_3D_CURVE_RESULT '
EXPECTED_CHECKS = 63
EXPECTED_OBSERVATIONS = (
    ('initial A', 'a'),
    ('shared initial A', 'a'),
    ('A to B', 'b'),
    ('shared A to B', 'b'),
    ('same-ref immediate refresh', 'a'),
    ('shared notification refresh', 'a'),
    ('empty wrapper retains RID Variant', 'empty'),
    ('empty wrapper recovers', 'b'),
    ('render-thread changed first', 'a'),
    ('render-thread changed shared', 'a'),
    ('queued old callback reads current member', 'a'),
    ('old later mutation cannot restore old RID', 'a'),
    ('old wrapper still updates shared second', 'b'),
    ('clear immediate NIL before queued drain', 'nil'),
    ('detached later mutation stays NIL', 'nil'),
    ('wrapper alive and mutated after destruction', 'b'),
)


def validate_payload(payload):
    if type(payload) is not dict:
        return ['invalid result object']
    errors = []
    if type(payload.get('failures')) is not list or payload['failures']:
        errors.append('fixture failures')
    if type(payload.get('checks')) is not int or payload['checks'] != EXPECTED_CHECKS:
        errors.append('incorrect fixture check count')
    rows = payload.get('observations')
    if type(rows) is not list or len(rows) != len(EXPECTED_OBSERVATIONS):
        return errors + ['incomplete fixture observations']
    fields = {'label', 'type', 'value', 'immediate_type', 'immediate', 'expected_type', 'expected'}
    for row, (label, _) in zip(rows, EXPECTED_OBSERVATIONS):
        if (type(row) is not dict or set(row) != fields
                or any(type(row[key]) is not int for key in ('type', 'immediate_type', 'expected_type'))
                or any(type(row[key]) is not str for key in ('label', 'value', 'immediate', 'expected'))):
            return errors + ['invalid observation fields/types']
        if row['label'] != label:
            return errors + ['incorrect observation labels/order']
    # A and B originate from two independently allocated live textures. Derive them
    # once, then enforce every scenario's association rather than trusting expected.
    a, b = rows[0]['expected'], rows[2]['expected']
    if (not re.fullmatch(r'RID\([1-9][0-9]*\)', a)
            or not re.fullmatch(r'RID\([1-9][0-9]*\)', b) or a == b):
        return errors + ['invalid or non-distinct A/B texture RIDs']
    values = {'a': (23, a), 'b': (23, b), 'empty': (23, 'RID(0)'), 'nil': (0, '<null>')}
    for row, (label, association) in zip(rows, EXPECTED_OBSERVATIONS):
        kind, value = values[association]
        if (any(row[key] != kind for key in ('type', 'immediate_type', 'expected_type'))
                or any(row[key] != value for key in ('value', 'immediate', 'expected'))):
            errors.append(f'incorrect raw Variant association: {label}')
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--godot', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    binary = args.godot.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    here = Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory(prefix='particle-rotation-velocity-3d-curve-') as tmp:
        project = Path(tmp)
        for name in ('project.godot', 'run.gd'):
            shutil.copyfile(here / name, project / name)
        (project / '.godot').mkdir()
        (project / '.godot/global_script_class_cache.cfg').write_text('list=[]\n')
        command = [str(binary), '--display-driver', 'macos', '--rendering-method', 'mobile',
                   '--rendering-driver', 'metal', '--audio-driver', 'Dummy',
                   '--path', str(project), '--script', 'run.gd']
        timed_out = False
        try:
            result = subprocess.run(command, capture_output=True, text=True, timeout=45)
            stdout, stderr, code = result.stdout, result.stderr, result.returncode
        except subprocess.TimeoutExpired as error:
            timed_out = True
            stdout = error.stdout or b''
            stderr = error.stderr or b''
            stdout = stdout.decode(errors='replace') if isinstance(stdout, bytes) else stdout
            stderr = stderr.decode(errors='replace') if isinstance(stderr, bytes) else stderr
            code = None
    (args.output / 'stdout.log').write_text(stdout)
    (args.output / 'stderr.log').write_text(stderr)
    lines = [line for line in (stdout + '\n' + stderr).splitlines() if MARKER in line]
    errors = []
    payload = None
    if code != 0:
        errors.append(f'engine return code {code}')
    if timed_out:
        errors.append('external watchdog expired')
    if not re.search(r'^Metal [^\n]+ - Forward Mobile - Using Device #\d+: .+', stdout, re.M):
        errors.append('Metal Forward Mobile startup not verified')
    # No ERROR whitelist: any engine/script error makes this run fail.
    if re.search(r'ERROR|SCRIPT ERROR|Parse Error|TIMEOUT', stdout + stderr, re.I):
        errors.append('engine/script error or timeout output')
    if len(lines) != 1 or not lines[0].startswith(MARKER):
        errors.append('expected exactly one standalone result marker')
    else:
        try:
            payload = json.loads(lines[0][len(MARKER):])
            errors.extend(validate_payload(payload))
        except ValueError:
            errors.append('invalid JSON result')
    receipt = {'command': command, 'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
               'returncode': code, 'timed_out': timed_out, 'errors': errors, 'result': payload}
    (args.output / 'result.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps({'ok': not errors, 'errors': errors, 'output': str(args.output)}))
    return 0 if not errors else 1


if __name__ == '__main__':
    raise SystemExit(main())
