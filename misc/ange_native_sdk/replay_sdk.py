#!/usr/bin/env python3
"""Regenerate and syntax-check the pinned custom native interface SDK."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parent
PROOFS = ROOT / 'verification'
PROOFS.mkdir(exist_ok=True)
PRODUCTS = ['include/gdextension_interface.h', 'include/godot_cpp/core/gdextension_interface_loader.hpp', 'src/gdextension_interface_loader.cpp']

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def run(command, label):
    started = time.monotonic()
    result = subprocess.run(command, cwd=ROOT, capture_output=True, env={**os.environ, 'PYTHONDONTWRITEBYTECODE': '1'})
    for stream in ['stdout', 'stderr']:
        (PROOFS / (label + '.' + stream)).write_bytes(getattr(result, stream))
    record = {'command': command, 'cwd': str(ROOT), 'exit': result.returncode, 'seconds': time.monotonic() - started,
              'stdout_bytes': len(result.stdout), 'stderr_bytes': len(result.stderr),
              'stdout_sha256': hashlib.sha256(result.stdout).hexdigest(), 'stderr_sha256': hashlib.sha256(result.stderr).hexdigest()}
    if result.returncode:
        raise RuntimeError(json.dumps(record) + '\n' + result.stderr.decode())
    return record

pins = json.loads((ROOT / 'INPUT_PINS.json').read_text())
assert all(sha(ROOT / rel) == expected for rel, expected in pins['files'].items()), 'Pinned SDK input changed'
expected = pins['expected_products']
generation_code = ('import sys;from pathlib import Path;sys.path.insert(0,sys.argv[1]);import binding_generator as g;'
                   'g.generate_gdextension_interface_header(sys.argv[3]+"/include/gdextension_interface.h",sys.argv[2]);'
                   'g.generate_gdextension_interface_loader(sys.argv[2],Path(sys.argv[3]))')

def command(output):
    return [sys.executable, '-B', '-c', generation_code, str(ROOT / 'tools/godot-cpp'), str(ROOT / 'schema/gdextension_interface.json'), str(output)]

results = [run(command(ROOT), 'generate')]
assert all(sha(ROOT / rel) == expected[rel] for rel in PRODUCTS), 'Generation differs from authenticated formal products'
with tempfile.TemporaryDirectory(prefix='native-interface-sdk-replay-') as temporary:
    replay = Path(temporary)
    (replay / 'include/godot_cpp/core').mkdir(parents=True)
    (replay / 'src').mkdir()
    results.append(run(command(replay), 'generate-repeat'))
    assert all((replay / rel).read_bytes() == (ROOT / rel).read_bytes() for rel in PRODUCTS), 'Repeat generation differs'

include = '-I' + str(ROOT / 'include')
support = ROOT / 'support/godot-cpp'
probes = [
    ['clang', '-std=c11', include, str(ROOT / 'probes/interface_include.c'), '-fsyntax-only'],
    ['clang++', '-std=c++17', include, str(ROOT / 'probes/interface_include.cpp'), '-fsyntax-only'],
    ['clang++', '-std=c++17', include, '-I' + str(support / 'include'), '-I' + str(support / 'gen/include'), str(ROOT / PRODUCTS[2]), '-fsyntax-only'],
]
for index, cmd in enumerate(probes):
    results.append(run(cmd, 'syntax-' + str(index)))
results.append(run(['clang++', '--version'], 'compiler-version'))
assert all(sha(ROOT / rel) == expected_hash for rel, expected_hash in pins['files'].items()), 'SDK input drifted during verification'
receipt = {'schema_sha256': sha(ROOT / 'schema/gdextension_interface.json'), 'products': {rel: {'sha256': sha(ROOT / rel), 'bytes': (ROOT / rel).stat().st_size} for rel in PRODUCTS},
           'results': results, 'double_generation_exact': True, 'formal_products_exact': True, 'inputs_unchanged': True,
           'scope': 'Custom native interface header plus loader declaration/implementation. Include and loader syntax only; no ANGE/RmlUi libraries, executable link or runtime validation.'}
(PROOFS / 'SDK_REPLAY.json').write_text(json.dumps(receipt, indent=2) + '\n')
print(json.dumps({'products': receipt['products'], 'schema_sha256': receipt['schema_sha256'], 'generation_and_probes': 'passed'}, indent=2))
