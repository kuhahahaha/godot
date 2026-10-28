#!/usr/bin/env python3
"""One positive control on the new canonical host; no production authority."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

P = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument('--case', choices=('positive',))
parser.add_argument('--build', action='store_true', help='Only the saved private DSO compiler command; never an SDK builder')
args = parser.parse_args()

def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b''):
            digest.update(block)
    return {'bytes': path.stat().st_size, 'sha256': digest.hexdigest()}

if args.build:
    with (P / 'PROBE_BUILD.log').open('w') as log:
        result = subprocess.run(json.loads((P / 'PROBE_BUILD_COMMAND.json').read_text()), stdout=log, stderr=subprocess.STDOUT)
    (P / 'PROBE_BUILD.rc').write_text(str(result.returncode) + '\n')
    if result.returncode:
        raise SystemExit(result.returncode)

command = json.loads((P / 'POSITIVE_COMMAND.json').read_text())
files = [Path(command[0]), P / 'runtime/probe.dylib', Path(__file__)]
files += sorted(q for q in (P / 'input').iterdir() if q.suffix in ('.comp', '.spv', '.bin', '.gd', '.godot', '.json') and q.name != 'owned_readback.bin')
files += [P / x for x in ('probe.cpp', 'original_unit.h', 'expected_unit.h', 'ORIGINAL_COMPILER_PAIR.json', 'ORIGINAL_REFLECTION_GRANTS.json', 'NATIVE_GENERATION_RECEIPT.json', 'expected_artifacts.h', 'generate_native.py', 'runtime/probe.gd', 'runtime/probe.gdextension')]
files += [P/'input'/f'role{i}.bin' for i in range(8)]
files += [Path(x) for x in json.loads((P / 'PROBE_BUILD_COMMAND.json').read_text()) if x.endswith(('.o', '.a'))]
files = list(dict.fromkeys(files))
before = {str(q): sha(q) for q in files}
(P / 'RUN_INPUTS_BEFORE.json').write_text(json.dumps(before, indent=2) + '\n')
metadata = json.loads((P / 'input/unit.json').read_text())
generation=json.loads((P/'NATIVE_GENERATION_RECEIPT.json').read_text())
assert sha(Path(command[0]))==generation['core'],'Current core differs from actual native compilation core'
for i,kind in enumerate(('capture','body')):
    for part,extension in [('source','comp'),('spirv','spv'),('container','bin')]:
        assert sha(P/'input'/f'{kind}.{extension}')==generation['physical'][i][part],f'{kind}/{part} differs from generation receipt'

rows = []
for case in ([args.case] if args.case else ['positive']):
    output = P / ('result_' + case)
    output.mkdir(exist_ok=True)
    # A prior result is never accepted as this run's async result.
    for name in ('owned_readback.bin', 'report.bin', 'facts.bin'):
        q = output / name
        if q.exists():
            q.unlink()
    env = {**os.environ, 'ANGE_MACOS_BACKGROUND_RENDER': '1', 'NUMERIC_CONTROL_INPUT': str(P / 'input'), 'NUMERIC_CONTROL_OUTPUT': str(output)}
    if case == 'cancel_prefix':
        env['NUMERIC_CONTROL_CANCEL_PREFIX'] = '1'
    else:
        env.pop('NUMERIC_CONTROL_CANCEL_PREFIX', None)
    log_path = P / (case.upper() + '.log')
    with log_path.open('w') as log:
        try:
            run = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=90)
            rc, timeout = run.returncode, False
        except subprocess.TimeoutExpired:
            rc, timeout = None, True
    (P / (case.upper() + '.rc')).write_text(str(rc) + '\n')
    text = log_path.read_text()
    qrt = re.findall(r'QRT_NATIVE_RETURN kind=(\d+) operation=(\d+) slot=(\d+) result=(\d+) segments=(\d+) error=(-?\d+) reply_ack=1', text)
    completion = re.search(r'ORIGINAL_NATIVE_COMPLETION ticket=(\d+) revision=(\d+) readback_request=(\d+) facts=(\d+) known=(\d+) possible=(\d+) mode=' + case, text)
    facts = re.findall(r'ORIGINAL_NATIVE_FACT index=(\d+) semantic=(\d+) graph=([^ ]+) slot=(\d+) queue=(\d+) issue=(\d+) completion=(\d+) error=(\d+) native=(\d+) completion_source=(\d+) backing=(\d+) offset=(\d+) bytes=(\d+)', text)
    checks = {
        'exit': rc == 0 and not timeout,
        'real_metal': 'Metal ' in text and 'Apple M3 Pro' in text,
        'actual_retained_unit': f'unit={metadata["unit_digest"]} flags={metadata["required_compile_flags"]} same_exported_identity=1' in text,
        'both_original_interfaces': len(re.findall(r'ORIGINAL_INTERFACE_MATCH physical=[01] ', text)) == 2,
        'qrt_original_returns': bool(qrt) and all(int(x[-1]) == 0 for x in qrt if not (case == 'cancel_prefix' and int(x[0]) == 9)),
        'completion': bool(completion) and int(completion[6]) == 0 and int(completion[4]) == len(facts),
        'actual_fact_fields': bool(facts) and all(int(f[4]) and int(f[5]) == 1 and int(f[6]) == 2 and int(f[7]) == 0 and int(f[8]) and int(f[9]) for f in facts),
        'physical_unit_returns': len(re.findall(r'ORIGINAL_UNIT_PHYSICAL_RETURN index=[01] shader_event=[1-9]\d* pipeline_event=[1-9]\d*', text)) == 2,
        'physical_storage_returns': {int(x) for x in re.findall(r'ORIGINAL_STORAGE_PHYSICAL_RETURN role=(\d+) backing=[1-9]\d* event=[1-9]\d*', text)} == set(range(8)),
        'bridge_close': f'ORIGINAL_NATIVE_CONTROL_DONE same_unit=1 same_ticket=1 rt_slots_released=1 physical_returns=1 bridge_closed=1 mode={case}' in text,
        'real_binding_terminal': text.count('ORIGINAL_CODE_HOLD_BINDING_RETURN stage=2\n') == 1,
        'sdk_user_levels': text.count('ORIGINAL_SDK_USER_TERMINATOR level=2\n') == 1 and text.count('ORIGINAL_SDK_USER_TERMINATOR level=3\n') == 1,
        'manager_close': text.count('ORIGINAL_NUMERIC_LOAD=0\n') == 1 and text.count('ORIGINAL_NUMERIC_UNLOAD=0\n') == 1,
        'image_finalizer_before_return': text.count('ORIGINAL_NUMERIC_DSO_UNLOAD\n') == 1 and 0 <= text.find('ORIGINAL_NUMERIC_DSO_UNLOAD\n') < text.find('ORIGINAL_NUMERIC_UNLOAD=0\n'),
        'clean': not any(x in text for x in ('ERROR:', 'FATAL', 'leaked at exit', 'Leaked instance', 'SIGSEGV', 'SIGABRT')),
    }
    owned = output / 'owned_readback.bin'
    if case == 'positive':
        checks['owned_readback_present'] = owned.exists() and owned.stat().st_size == metadata['readback_bytes']
        checks['original_exact_readback'] = f'ORIGINAL_EXACT_READBACK bytes={metadata["readback_bytes"]} samples={metadata["sample_count"]} recipe_header=1 guard=1 original_fixed_expected=1 repeated_owned_copy=1' in text
        checks['request_issued'] = bool(completion) and int(completion[3]) != 0
    else:
        checks['no_readback_or_fake_completion'] = not owned.exists() and 'ORIGINAL_EXACT_READBACK' not in text and bool(completion) and int(completion[3]) == 0
        checks['cancel_refuses_record'] = bool(re.search(r'ORIGINAL_CANCEL_REFUSAL error=[1-9]\d* reason=16 no_new_record=1',text))
        checks['only_one_original_upload_prefix'] = sum(int(x[0]) == 8 for x in qrt) == 1
    row = {'case': case, 'command': command, 'env': {k: env[k] for k in ('NUMERIC_CONTROL_INPUT', 'NUMERIC_CONTROL_OUTPUT')}, 'rc': rc, 'timeout': timeout, 'checks': checks, 'qrt_returns': qrt, 'completion': completion.groups() if completion else None, 'facts': facts, 'outputs': {q.name: sha(q) for q in output.iterdir()}}
    rows.append(row)
    (P / 'RESULTS.json').write_text(json.dumps({'scope': 'original native/GPU substrate only; no real ANGE request/two-stage Verified/Ready/G1 claim', 'runs': rows}, indent=2) + '\n')
    print(case, 'rc=', rc, 'failed=', [k for k, v in checks.items() if not v], flush=True)
    if not all(checks.values()):
        raise SystemExit(1)
after = {str(q): sha(q) for q in files}
(P / 'RUN_INPUTS_AFTER.json').write_text(json.dumps(after, indent=2) + '\n')
if before != after:
    raise SystemExit('Native inputs changed during run; evidence invalid')
print('Exact inputs unchanged; requested positive control validated.', flush=True)
