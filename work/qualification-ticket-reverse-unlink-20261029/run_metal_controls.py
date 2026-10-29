#!/usr/bin/env python3
"""Run existing native controls against this editor, with isolated outputs."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

here = Path(__file__).resolve().parent
repo = here.parents[1]
fixture = repo / 'work/native-numeric-unit-correlation-20261029'
editor = repo / 'bin/godot.macos.editor.dev.arm64'
inputs = [editor, fixture / 'runtime/probe.dylib', fixture / 'runtime/probe.gd',
          fixture / 'runtime/probe.gdextension', fixture / 'expected_unit.h',
          fixture / 'expected_artifacts.h', fixture / 'original_unit.h',
          fixture / 'NATIVE_GENERATION_RECEIPT.json']
inputs.extend(sorted(p for p in (fixture / 'input').iterdir()
                     if p.is_file() and p.suffix in ('.comp', '.spv', '.bin', '.gd', '.godot', '.json')
                     and p.name != 'owned_readback.bin'))
def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(4 * 1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()
before = {str(p): sha(p) for p in inputs}
metadata = json.loads((fixture / 'input/unit.json').read_text())
generation = json.loads((fixture / 'NATIVE_GENERATION_RECEIPT.json').read_text())
command = [str(editor), '--disable-crash-handler', '--path', str(fixture / 'runtime'),
           '--rendering-driver', 'metal', '--rendering-method', 'mobile',
           '--render-thread', 'safe', '--script', 'probe.gd']
rows = []
with tempfile.TemporaryDirectory(prefix='retire-metal-controls-') as scratch:
    for case in ('positive', 'cancel_prefix'):
        output = Path(scratch) / case
        output.mkdir()
        env = dict(os.environ, ANGE_MACOS_BACKGROUND_RENDER='1',
                   NUMERIC_CONTROL_INPUT=str(fixture / 'input'), NUMERIC_CONTROL_OUTPUT=str(output))
        if case == 'cancel_prefix':
            env['NUMERIC_CONTROL_CANCEL_PREFIX'] = '1'
        else:
            env.pop('NUMERIC_CONTROL_CANCEL_PREFIX', None)
        run = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
        log = run.stdout.decode('utf-8', errors='replace')
        (here / ('metal-' + case + '.log')).write_text(log)
        completion = re.search(r'ORIGINAL_NATIVE_COMPLETION ticket=(\d+) revision=(\d+) readback_request=(\d+) facts=(\d+) known=(\d+) possible=(\d+) mode=' + case, log)
        facts = re.findall(r'ORIGINAL_NATIVE_FACT index=(\d+) semantic=(\d+) graph=([^ ]+) slot=(\d+) queue=(\d+) issue=(\d+) completion=(\d+) error=(\d+) native=(\d+) completion_source=(\d+) backing=(\d+) offset=(\d+) bytes=(\d+)', log)
        qrt = re.findall(r'QRT_NATIVE_RETURN kind=(\d+) operation=(\d+) slot=(\d+) result=(\d+) segments=(\d+) error=(-?\d+) reply_ack=1', log)
        checks = {
            'exit': run.returncode == 0,
            'metal': 'Metal ' in log and 'Apple M3 Pro' in log,
            'same_unit': f'unit={metadata["unit_digest"]} flags={metadata["required_compile_flags"]} same_exported_identity=1' in log,
            'interfaces': len(re.findall(r'ORIGINAL_INTERFACE_MATCH physical=[01] ', log)) == 2,
            'qrt': bool(qrt) and all(int(x[-1]) == 0 for x in qrt if not (case == 'cancel_prefix' and int(x[0]) == 9)),
            'completion': bool(completion) and int(completion[6]) == 0 and int(completion[4]) == len(facts),
            'facts': bool(facts) and all(int(f[4]) and int(f[5]) == 1 and int(f[6]) == 2 and int(f[7]) == 0 and int(f[8]) and int(f[9]) for f in facts),
            'unit_returns': len(re.findall(r'ORIGINAL_UNIT_PHYSICAL_RETURN index=[01] shader_event=[1-9]\d* pipeline_event=[1-9]\d*', log)) == 2,
            'storage_returns': {int(x) for x in re.findall(r'ORIGINAL_STORAGE_PHYSICAL_RETURN role=(\d+) backing=[1-9]\d* event=[1-9]\d*', log)} == set(range(8)),
            'bridge_close': f'ORIGINAL_NATIVE_CONTROL_DONE same_unit=1 same_ticket=1 rt_slots_released=1 physical_returns=1 bridge_closed=1 mode={case}' in log,
            'code_hold': log.count('ORIGINAL_CODE_HOLD_BINDING_RETURN stage=2\n') == 1,
            'manager': log.count('ORIGINAL_NUMERIC_LOAD=0\n') == 1 and log.count('ORIGINAL_NUMERIC_UNLOAD=0\n') == 1,
            'finalizer': 0 <= log.find('ORIGINAL_NUMERIC_DSO_UNLOAD\n') < log.find('ORIGINAL_NUMERIC_UNLOAD=0\n'),
            'clean': not any(x in log for x in ('ERROR:', 'FATAL', 'leaked at exit', 'Leaked instance', 'SIGSEGV', 'SIGABRT')),
        }
        owned = output / 'owned_readback.bin'
        if case == 'positive':
            checks['owned_readback'] = owned.exists() and owned.stat().st_size == metadata['readback_bytes']
            checks['exact_readback'] = f'ORIGINAL_EXACT_READBACK bytes={metadata["readback_bytes"]} samples={metadata["sample_count"]} recipe_header=1 guard=1 original_fixed_expected=1 repeated_owned_copy=1' in log
            checks['request'] = bool(completion) and int(completion[3]) != 0
        else:
            checks['no_readback'] = not owned.exists() and 'ORIGINAL_EXACT_READBACK' not in log and bool(completion) and int(completion[3]) == 0
            checks['cancel'] = bool(re.search(r'ORIGINAL_CANCEL_REFUSAL error=[1-9]\d* reason=16 no_new_record=1', log))
            checks['one_upload'] = sum(int(x[0]) == 8 for x in qrt) == 1
        outputs = {p.name: sha(p) for p in output.iterdir() if p.is_file()}
        rows.append({'case': case, 'rc': run.returncode, 'checks': checks, 'outputs': outputs})
        print(case, 'rc', run.returncode, 'failed', [k for k, v in checks.items() if not v], flush=True)
        if not all(checks.values()):
            break
after = {str(p): sha(p) for p in inputs}
result = {'scope': 'existing substrate controls only; prior generated inputs and current editor; no product qualification or GPU failure claim',
          'command': command, 'generation_editor_sha256': generation['core']['sha256'],
          'inputs_before': before, 'inputs_after': after, 'runs': rows}
(here / 'metal-results.json').write_text(json.dumps(result, indent=2) + '\n')
if before != after or len(rows) != 2 or not all(all(row['checks'].values()) for row in rows):
    raise SystemExit(1)
