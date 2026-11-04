#!/usr/bin/env python3
"""Bind an explicit new host/SDK pair and run two original System native cases."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
SELECTION_FIELDS = frozenset(('schema', 'core_stamp', 'compiler_route', 'compiler_stamp', 'platform', 'architecture', 'target_kind'))
PROJECT = 'config_version=5\n[application]\nconfig/name="Original System scalar correctness"\n[rendering]\nrenderer/rendering_method="mobile"\n'
CASES = (('positive', 'positive.cpp'), ('post_submit_close', 'post_submit_close.cpp'))
GLOBAL_FATAL_PATTERNS = (r'ERROR:|ASSERT|CrashHandler|leaked at exit|FIXTURE_FATAL',)
CONTROL_NAMES = frozenset(('native_unknown', 'native_fact_capacity', 'set_retire_error'))
CONTROL_ENVIRONMENT = frozenset(('GODOT_SYSTEM_SCALAR_TEST_APPEND_UNKNOWN', 'GODOT_SYSTEM_SCALAR_TEST_SET_RETURN_ERROR', 'GODOT_QUALIFICATION_CAPACITY_TRACE'))


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def file_identity(path):
    path = Path(path)
    return {'path': str(path.resolve()), 'sha256': sha(path), 'bytes': path.stat().st_size}


def load_json(path):
    def unique_pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError('duplicate JSON key: ' + key)
            result[key] = value
        return result
    return json.loads(Path(path).read_text(), object_pairs_hook=unique_pairs)


def validate_selection(selection):
    if not isinstance(selection, dict) or set(selection) != SELECTION_FIELDS:
        raise ValueError('selection must have exactly the original seven fields')
    if type(selection['schema']) is not int or selection['schema'] != 1:
        raise ValueError('selection schema must be integer 1')
    for key in ('core_stamp', 'compiler_stamp'):
        if not isinstance(selection[key], str) or re.fullmatch('[0-9a-f]{64}', selection[key]) is None:
            raise ValueError('invalid ' + key)
    if any(not isinstance(selection[key], str) or not selection[key] for key in SELECTION_FIELDS - {'schema', 'core_stamp', 'compiler_stamp'}):
        raise ValueError('selection identity fields must be nonempty strings')
    if (selection['platform'], selection['architecture'], selection['target_kind']) != ('macos', 'arm64', 'Executable'):
        raise ValueError('this standalone runner requires macos/arm64/Executable')
    return selection


def actual_backend(text, driver):
    clean = re.sub(r'\x1b\[[0-9;]*m', '', text)
    name = 'Metal' if driver == 'metal' else 'Vulkan'
    startup = re.findall(r'^(Metal|Vulkan) [0-9]+(?:\.[0-9]+)* - Forward Mobile - Using Device #[0-9]+: .+$', clean, re.M)
    return startup == [name]


def actual_identity(text, selection):
    matches = re.findall(r'^SYSTEM_SCALAR_HOST_IDENTITY=(.+)$', text, re.M)
    if len(matches) != 1:
        return False
    try:
        return validate_selection(json.loads(matches[0])) == selection
    except (ValueError, TypeError):
        return False


def load_negative_cases(path):
    configuration = load_json(path)
    if set(configuration) != {'schema', 'cases'} or type(configuration['schema']) is not int or configuration['schema'] != 1 or not isinstance(configuration['cases'], list):
        raise ValueError('negative cases require closed schema1/cases object')
    seen = set()
    required = {'name', 'defines', 'environment', 'expected_returncode', 'required_patterns', 'forbidden_patterns'}
    optional = {'requires_driver', 'expected_process_termination', 'retirement_claim'}
    for case in configuration['cases']:
        if not isinstance(case, dict) or not required <= set(case) or set(case) - required - optional:
            raise ValueError('negative case has missing or unknown fields')
        name = case['name']
        if name not in CONTROL_NAMES or name in seen:
            raise ValueError('unknown or duplicate negative case: ' + str(name))
        seen.add(name)
        if type(case['expected_returncode']) is not int:
            raise ValueError('negative expected returncode must be integer')
        if not isinstance(case['defines'], list) or not all(isinstance(value, str) and re.fullmatch(r'[A-Z_][A-Z0-9_]*(?:=[A-Za-z0-9_]+)?', value) for value in case['defines']):
            raise ValueError('invalid explicit negative preprocessor definition')
        environment = case['environment']
        if not isinstance(environment, dict) or set(environment) - CONTROL_ENVIRONMENT or any(value != '1' for value in environment.values()):
            raise ValueError('unknown negative environment or value')
        fault_keys = set(environment) - {'GODOT_QUALIFICATION_CAPACITY_TRACE'}
        expected = {'native_unknown': {'GODOT_SYSTEM_SCALAR_TEST_APPEND_UNKNOWN'}, 'set_retire_error': {'GODOT_SYSTEM_SCALAR_TEST_SET_RETURN_ERROR'}, 'native_fact_capacity': set()}[name]
        if fault_keys != expected:
            raise ValueError('negative environment does not match its original case')
        if case.get('requires_driver') not in (None, 'metal', 'vulkan'):
            raise ValueError('invalid negative driver requirement')
        for field in ('expected_process_termination', 'retirement_claim'):
            if field in case and type(case[field]) is not bool:
                raise ValueError('negative ' + field + ' must be bool')
        if case.get('expected_process_termination', False) and case.get('retirement_claim', False):
            raise ValueError('process-isolated negative cannot claim physical retirement')
        for field in ('required_patterns', 'forbidden_patterns'):
            if not isinstance(case[field], list) or not all(isinstance(value, str) for value in case[field]):
                raise ValueError('negative patterns must be string arrays')
            for pattern in case[field]:
                re.compile(pattern)
        if not case['required_patterns']:
            raise ValueError('negative case requires actual observation patterns')
    if seen != CONTROL_NAMES:
        raise ValueError('negative configuration must declare exactly the three original controls')
    return configuration['cases']


def clean_environment(inherited):
    environment = dict(inherited)
    for key in tuple(environment):
        if key.startswith('GODOT_SYSTEM_SCALAR_TEST_') or key == 'GODOT_QUALIFICATION_CAPACITY_TRACE' or key in ('DYLD_LIBRARY_PATH', 'DYLD_FALLBACK_LIBRARY_PATH', 'DYLD_INSERT_LIBRARIES'):
            environment.pop(key, None)
    return environment


def actual_capacity_evidence(text):
    # This finite fixture targets the shared pre-driver source-slot gate. On
    # this Mac the actual transfer pool is 11; do not assume that from a name.
    rows = re.findall(r'^SYS_SCALAR_CAPACITY current=([0-9]+) workers=([0-9]+) bytes=16384 graphs=([0-9]+) error=([0-9]+)$', text, re.M)
    if len(rows) != 1:
        return {'status': 'missing_or_ambiguous_actual_capacity_trace', 'matching_rows': len(rows)}
    current, workers, graphs, error = map(int, rows[0])
    frames = graphs + 1
    capacity = 4096
    slots = current + frames * (workers + 1)
    evidence = {'current_commands': current, 'workers': workers, 'bytes': 16384,
                'graph_bound': graphs, 'frame_bound': frames, 'required_slots': slots,
                'source_slot_capacity': capacity, 'actual_error': error, 'expected_error': 5, 'expected_reason': 19}
    if current > 0xffffffff or workers > 0xffffffff or graphs != 513:
        evidence['status'] = 'unexpected_actual_capacity_inputs'
    elif slots <= capacity:
        evidence['status'] = 'fixture_precondition_not_met'
    elif error != 5:
        evidence['status'] = 'unexpected_actual_capacity_error'
    else:
        evidence['status'] = 'satisfied_shared_pre_driver_slot_refusal'
    return evidence


def validate_probe_constructor(script):
    functions = dict(re.findall(r'^func (_init|_initialize)\(\):\n((?:[ \t].*|\n)*)', script, re.M))
    constructor = functions.get('_init', '')
    initialize = functions.get('_initialize', '')
    required = ('Engine.get_version_info()', 'SYSTEM_SCALAR_HOST_IDENTITY=', 'SYSTEM_SCALAR_SELECTION_JSON', 'if identity != expected:')
    if any(marker not in constructor for marker in required) or script.count('SYSTEM_SCALAR_HOST_IDENTITY=') != 1:
        raise ValueError('probe actual identity must be printed and checked in _init before synchronous SceneReady')
    if any(marker in initialize for marker in required) or 'quit(0)' not in initialize:
        raise ValueError('probe _initialize must keep only ordinary loop initialization/quit')


def forbidden_patterns_for(control=None):
    return list(GLOBAL_FATAL_PATTERNS) + (list(control['forbidden_patterns']) if control else [])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', required=True, type=Path)
    parser.add_argument('--sdk-header', required=True, type=Path)
    parser.add_argument('--selection', required=True, type=Path)
    parser.add_argument('--driver', required=True, choices=('metal', 'vulkan'))
    parser.add_argument('--metadata', type=Path, help='defaults to HOST.controlled_build.json')
    parser.add_argument('--output', type=Path, help='fresh output directory; existing directory is refused')
    parser.add_argument('--cases', choices=('all', 'original'), default='all')
    parser.add_argument('--prepare-only', action='store_true', help='bind and snapshot inputs; no host execution, GPU or DSO compilation')
    args = parser.parse_args()
    validate_probe_constructor((HERE / 'probe.gd').read_text())
    host, sdk, selected = (p.resolve(strict=True) for p in (args.host, args.sdk_header, args.selection))
    metadata_path = (args.metadata or Path(str(host) + '.controlled_build.json')).resolve(strict=True)
    selection = validate_selection(load_json(selected))
    negative_path = HERE / 'negative_cases.json'
    negative_cases = load_negative_cases(negative_path) if args.cases == 'all' and negative_path.is_file() else []
    metadata = load_json(metadata_path)
    identities = {key: file_identity(path) for key, path in (('host', host), ('metadata', metadata_path), ('sdk_header', sdk), ('selection', selected))}
    if metadata.get('schema') != 1 or metadata.get('status') != 'available' or validate_selection(metadata.get('selection')) != selection:
        raise ValueError('host metadata must be available and match the explicit closed selection')
    if metadata.get('artifact', {}).get('sha256') != identities['host']['sha256'] or metadata['artifact'].get('bytes') != identities['host']['bytes']:
        raise ValueError('host SHA/size does not match actual adjacent controlled metadata')
    header = sdk.read_text()
    for symbol in ('GDExtensionInterfaceDeviceSessionSystemScalarRecordV1', 'GDExtensionInterfaceDeviceSessionSystemScalarObserveV1'):
        if symbol not in header:
            raise ValueError('explicit SDK lacks required System ABI: ' + symbol)
    out = (args.output or HERE.parent / 'results' / (datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f') + '-' + args.driver)).resolve()
    out.mkdir(parents=True, exist_ok=False)
    result = {'schema': 1, 'status': 'preparing', 'driver': args.driver, 'selection': selection,
              'requested_cases': args.cases, 'bound_inputs': identities, 'controlled_metadata': metadata, 'builds': [], 'runs': [],
              'pending_controls': {key: {'status': 'unsupported_not_run', 'reason': 'No original-owner controlled injection adapter was supplied.'}
                                   for key in ('native_unknown', 'native_fact_capacity', 'set_retire_error')},
              'scope': 'Original System scalar native positive/post-submit bridge close plus explicitly observed original-owner controls; not ANGE Host/U1/B4 acceptance.'}
    if args.cases == 'original':
        result['pending_controls'] = {key: {'status': 'not_requested'} for key in CONTROL_NAMES}
    for control in negative_cases:
        result['pending_controls'][control['name']] = {'status': 'prepared_not_run' if control.get('requires_driver') in (None, args.driver) else 'unsupported_driver_not_run', 'requires_driver': control.get('requires_driver')}

    def save():
        result['artifacts'] = {str(p.relative_to(out)): {'sha256': sha(p), 'bytes': p.stat().st_size}
                               for p in sorted(out.rglob('*')) if p.is_file() and p.name != 'results.json'}
        (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')

    def unchanged():
        for identity in identities.values():
            if file_identity(Path(identity['path'])) != identity:
                raise ValueError('bound input changed during run: ' + identity['path'])
        for name, identity in result['source_inputs'].items():
            if file_identity(HERE / name) != identity:
                raise ValueError('runner source input changed during run: ' + name)

    try:
        snapshot = out / 'inputs'
        snapshot.mkdir()
        names = ('run.py', 'positive.cpp', 'post_submit_close.cpp', 'post_submit_close.original.cpp', 'generate.gd', 'probe.gd', 'scalar.glsl', 'SOURCE_PROVENANCE.json', 'include/core/error/error_list.h')
        result['source_inputs'] = {name: file_identity(HERE / name) for name in names}
        if negative_cases:
            names += ('negative_probe.cpp', 'negative_cases.json')
            result['source_inputs'].update({name: file_identity(HERE / name) for name in names[-2:]})
        for name in names:
            target = snapshot / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(HERE / name, target)
        shutil.copyfile(sdk, snapshot / 'gdextension_interface_v2.pinned.h')
        shutil.copyfile(selected, snapshot / 'CoreBuildSelection.schema1.json')
        shutil.copyfile(metadata_path, snapshot / 'host.controlled_build.json')
        linkage = subprocess.run(['otool', '-L', str(host)], capture_output=True, text=True, check=True)
        (out / 'host-linkage.log').write_text(linkage.stdout + linkage.stderr)
        if 'libgodot' in linkage.stdout.lower():
            raise ValueError('linked core host is unsupported without separately binding actual core')
        result['linkage_command'] = ['otool', '-L', str(host)]
        unchanged()
        if args.prepare_only:
            result['status'] = 'prepared_gpu_not_run'
            result['host_executed'] = False
            result['dso_compiled'] = False
            save()
            print(out / 'results.json')
            return 0
        if args.cases == 'all' and not negative_cases:
            raise RuntimeError('all requested but original-owner negative adapter is absent; no GPU execution started')
        env = dict(os.environ, ANGE_MACOS_BACKGROUND_RENDER='1', SYSTEM_SCALAR_SELECTION_JSON=json.dumps(selection, sort_keys=True))
        env = clean_environment(env)
        generator = out / 'containers'
        generator.mkdir()
        for name in ('scalar.glsl', 'generate.gd'):
            shutil.copyfile(snapshot / name, generator / name)
        (generator / 'project.godot').write_text(PROJECT)
        env['SYSTEM_SCALAR_CONTAINER'] = str(generator / 'scalar.bin')

        def launch(case, project, script, control=None):
            unchanged()
            command = [str(host), '--path', str(project), '--rendering-driver', args.driver, '--rendering-method', 'mobile', '--render-thread', 'safe', '--script', script]
            child_env = dict(env)
            if control:
                child_env.update(control['environment'])
            row = {'case': case, 'controlled_environment': control['environment'] if control else {}, 'command': command, 'cwd': str(project), 'timeout_seconds': 60, 'checks': {}}
            try:
                proc = subprocess.run(command, cwd=project, env=child_env, capture_output=True, text=True, timeout=60)
                stdout, stderr, code = proc.stdout, proc.stderr, proc.returncode
            except subprocess.TimeoutExpired as exc:
                def decode(data):
                    return data.decode(errors='replace') if isinstance(data, bytes) else (data or '')
                stdout, stderr, code = decode(exc.stdout), decode(exc.stderr), 'timeout'
            (out / (case + '.stdout.log')).write_text(stdout)
            (out / (case + '.stderr.log')).write_text(stderr)
            text = stdout + stderr
            row['returncode'] = code
            forbidden = forbidden_patterns_for(control)
            row['checks'] = {'expected_exit': code == (control['expected_returncode'] if control else 0), 'actual_backend': actual_backend(text, args.driver),
                             'actual_selection': actual_identity(text, selection),
                             'forbidden_patterns_absent': not any(re.search(pattern, text, re.M) for pattern in forbidden)}
            if control and case == 'native_fact_capacity':
                row['actual_capacity'] = actual_capacity_evidence(text)
                row['checks']['actual_source_slot_precondition'] = row['actual_capacity']['status'] == 'satisfied_shared_pre_driver_slot_refusal'
            if control:
                row['checks']['required_observations'] = all(re.search(pattern, text, re.M) for pattern in control['required_patterns'])
                row['expected_process_termination'] = control.get('expected_process_termination', False)
                row['retirement_claim'] = control.get('retirement_claim', False)
            result['runs'].append(row)
            save()
            unchanged()
            return text, row

        text, row = launch('containers', generator, 'generate.gd')
        row['checks']['native_container'] = bool(re.search(r'^SYSTEM_SCALAR_CONTAINER bytes=[1-9][0-9]*$', text, re.M)) and (generator / 'scalar.bin').is_file()
        save()
        if not all(row['checks'].values()):
            raise RuntimeError('container generation checks failed: ' + repr(row['checks']))
        cases = [(name, source, None) for name, source in CASES]
        for control in negative_cases:
            if control.get('requires_driver') not in (None, args.driver):
                result['pending_controls'][control['name']] = {'status': 'unsupported_driver_not_run', 'requires_driver': control['requires_driver']}
                continue
            cases.append((control['name'], 'negative_probe.cpp', control))
        for case, source_name, control in cases:
            unchanged()
            dso = out / (case + '.dylib')
            command = ['clang++', '-std=c++17', '-dynamiclib', '-arch', 'arm64', '-mmacosx-version-min=11.0', '-O2', '-Wall', '-Wextra', '-Wno-unused-parameter', '-Wno-deprecated-declarations', '-DSCALAR_EXPECTED_WORD=0x40400000', '-I', str(snapshot), '-I', str(snapshot / 'include'), str(snapshot / source_name), '-o', str(dso)]
            if control:
                command[1:1] = ['-D' + define for define in control['defines']]
            build = subprocess.run(command, cwd=out, capture_output=True, text=True)
            (out / (case + '-build.stdout.log')).write_text(build.stdout)
            (out / (case + '-build.stderr.log')).write_text(build.stderr)
            result['builds'].append({'case': case, 'command': command, 'cwd': str(out), 'returncode': build.returncode,
                                     'dso': file_identity(dso) if build.returncode == 0 else None})
            save()
            if build.returncode:
                raise RuntimeError('DSO compile failed: ' + case)
            project = out / case
            (project / '.godot').mkdir(parents=True)
            (project / 'project.godot').write_text(PROJECT)
            shutil.copyfile(snapshot / 'probe.gd', project / 'probe.gd')
            (project / 'probe.gdextension').write_text('[configuration]\nentry_symbol="phs1_probe_init"\ncompatibility_minimum="4.7"\ndevice_exit_guard_version=1\n[libraries]\nmacos=' + json.dumps(str(dso)) + '\n')
            (project / '.godot/extension_list.cfg').write_text('res://probe.gdextension\n')
            text, row = launch(case, project, 'probe.gd', control)
            if control:
                if control.get('expected_process_termination', False):
                    row['checks']['no_false_unload'] = 'PHS1_DSO_UNLOAD' not in text
                elif control.get('retirement_claim', False):
                    row['checks']['unload'] = text.count('PHS1_DSO_UNLOAD tag=one') == 1
                result['pending_controls'][case] = {'status': 'observed_passed' if all(row['checks'].values()) else 'failed', 'retirement_claim': control.get('retirement_claim', False)}
            else:
                row['checks']['unload'] = text.count('PHS1_DSO_UNLOAD tag=one') == 1
            if control:
                pass
            elif case == 'positive':
                row['checks']['native_scalar'] = 'SYSTEM_SCALAR_COMPLETE exact_word=40400000 bytes=4 native_complete=1 physical_free=1 ack=1' in text
            else:
                row['checks']['native_close'] = 'SYSTEM_SCALAR_CLOSE_COMPLETE post_submit=1 admission_refused=1 native_complete=1 physical_free=1 ack=1' in text
                row['checks']['actual_issue_before_close'] = bool(re.search(r'SYSTEM_SCALAR_POST_SUBMIT_OBSERVED semantic=[45] issue=1 native=[1-9][0-9]* completion=[1-9][0-9]* queue=[1-9][0-9]* ticket=[1-9][0-9]* state=[0-9]+', text)) and text.find('SYSTEM_SCALAR_POST_SUBMIT_OBSERVED') < text.find('SYSTEM_SCALAR_POST_SUBMIT_CLOSE')
                row['checks']['original_ticket_drain'] = bool(re.search(r'SYSTEM_SCALAR_POST_SUBMIT_DRAIN state=(?:7 phase=6|8 phase=5) facts=[1-9][0-9]* returned_bytes=4 owned=[04] copied=[04] cancel_error=(?:0|44) result=(?:completed|cancelled)', text))
            save()
            print(case, row['returncode'], row['checks'], flush=True)
            if not all(row['checks'].values()):
                raise RuntimeError('native case checks failed: ' + case)
        unchanged()
        result['bound_inputs_unchanged'] = True
        result['status'] = 'passed_original_native_component' if args.cases == 'original' else ('passed_available_native_controls_driver_limits_recorded' if any(value['status'] != 'observed_passed' for value in result['pending_controls'].values()) else 'passed_original_native_component_and_three_controls')
        save()
        print(out / 'results.json', flush=True)
        return 0
    except Exception as exc:
        result['status'] = 'failed'
        result['failure'] = {'type': type(exc).__name__, 'message': str(exc)}
        save()
        raise


if __name__ == '__main__':
    sys.exit(main())
