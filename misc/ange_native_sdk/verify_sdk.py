#!/usr/bin/env python3
"""Read-only SDK verification. Regeneration and syntax run only in a temporary copy."""
import hashlib
import importlib.util
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
root = pathlib.Path(__file__).resolve().parent
def pin(p):
    raw = p.read_bytes()
    return {'sha256': hashlib.sha256(raw).hexdigest(), 'bytes': len(raw)}
manifest = json.loads((root / 'MANIFEST.json').read_text())
for rel, expected in manifest['files'].items():
    assert pin(root / rel) == expected, rel
spec = importlib.util.spec_from_file_location('original_cohort_identity', root / 'cohort/engine_build_identity.py')
identity = importlib.util.module_from_spec(spec)
spec.loader.exec_module(identity)
snapshot = json.loads((root / 'cohort/engine_build_identity.gen.json').read_text())['snapshot']
core, compiler = identity.derive_stamps(snapshot)
selection = identity.selection_record(snapshot, core, compiler)
assert selection == json.loads((root / 'cohort/engine_build_selection.gen.json').read_text())
assert identity.render_source(selection).encode() == (root / 'cohort/engine_build_identity.gen.cpp').read_bytes()
assert selection == json.loads((root / 'COHORT.json').read_text())['selection']
assert selection == json.loads((root / 'cohort/godot.macos.template_debug.arm64.controlled_build.json').read_text())['selection']
with tempfile.TemporaryDirectory(prefix='native-sdk-readonly-') as temp:
    copy = pathlib.Path(temp) / 'sdk'
    shutil.copytree(root, copy, ignore=shutil.ignore_patterns('verification', 'MANIFEST.json', '__pycache__', '*.pyc'))
    result = subprocess.run([sys.executable, '-B', str(copy / 'replay_sdk.py')], capture_output=True)
    assert result.returncode == 0, result.stderr.decode(errors='replace')
    replay = json.loads((copy / 'verification/SDK_REPLAY.json').read_text())
    assert replay['formal_products_exact'] and replay['double_generation_exact'] and replay['inputs_unchanged']
    assert all(row['exit'] == 0 and row['stderr_bytes'] == 0 for row in replay['results'])
for rel, expected in manifest['files'].items():
    assert pin(root / rel) == expected, rel
print(json.dumps({'status': 'READ_ONLY_MANIFEST_COHORT_AND_PRIVATE_REGENERATION_VERIFIED',
    'manifest': pin(root / 'MANIFEST.json'), 'files': len(manifest['files']), 'selection': selection,
    'formal_products': replay['products'], 'generation_passes': 2, 'syntax_probes': 3,
    'all_probe_exit_zero_empty_stderr': True, 'delivery_unchanged': True, 'engine_run': False}, indent=2))
