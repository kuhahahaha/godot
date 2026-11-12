#!/usr/bin/env python3
"""Exercise the real runner with controlled engine output; never start Godot."""
import contextlib
import copy
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('param_texture_engine_runner', HERE / 'run.py')
RUNNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUNNER)
MARKER = 'PARTICLE_PARAM_TEXTURE_RESULT '
STARTUP = 'Metal 4.0 - Forward Mobile - Using Device #0: Apple M3 Pro\n'


def complete_payload():
    # Hand-derived from the scenario, independently of run.py's validator.
    pairs = [
        ('initial A', 'RID(11)'),
        ('shared initial A', 'RID(11)'),
        ('A to B', 'RID(22)'),
        ('shared A to B', 'RID(22)'),
        ('same-ref immediate refresh', 'RID(11)'),
        ('shared notification refresh', 'RID(11)'),
        ('empty wrapper retains RID Variant', 'RID(0)'),
        ('empty wrapper recovers', 'RID(22)'),
        ('render-thread changed first', 'RID(11)'),
        ('render-thread changed shared', 'RID(11)'),
        ('queued old callback reads current member', 'RID(11)'),
        ('old later mutation cannot restore old RID', 'RID(11)'),
        ('old wrapper still updates shared second', 'RID(22)'),
        ('clear immediate NIL before queued drain', '<null>'),
        ('detached later mutation stays NIL', '<null>'),
        ('wrapper alive and mutated after destruction', 'RID(22)'),
    ]
    slots = (
        'angular_velocity_texture', 'orbit_velocity_curve', 'linear_accel_texture',
        'radial_accel_texture', 'tangent_accel_texture', 'damping_texture',
        'angle_texture', 'scale_curve', 'hue_rot_curve', 'animation_speed_curve',
        'animation_offset_curve', 'turbulence_influence_over_life',
        'radial_velocity_curve', 'scale_over_velocity_curve', 'directional_velocity_curve',
    )
    pairs = [(slot + '/' + label, value) for slot in slots for label, value in pairs] + [
        ('angular_velocity_texture/cross-slot initial', 'RID(11)'),
        ('damping_texture/cross-slot initial', 'RID(11)'),
        ('angular_velocity_texture/cross-slot shared change', 'RID(22)'),
        ('damping_texture/cross-slot shared change', 'RID(22)'),
        ('angular_velocity_texture/cross-slot detached first', 'RID(11)'),
        ('damping_texture/cross-slot retained second', 'RID(22)'),
        ('damping_texture/cross-slot second survives clear', 'RID(11)'),
        ('angular_velocity_texture/cross-slot first stays NIL', '<null>'),
    ]
    rows = []
    for label, value in pairs:
        kind = 0 if value == '<null>' else 23
        rows.append({'label': label, 'type': kind, 'value': value,
                     'immediate_type': kind, 'immediate': value,
                     'expected_type': kind, 'expected': value})
    return {'checks': 973, 'failures': [], 'observations': rows}


class RunnerControls(unittest.TestCase):
    def run_output(self, payload, *, code=0, startup=STARTUP, suffix='', stderr='', mutate_binary=False):
        encoded = payload if isinstance(payload, str) else json.dumps(payload)
        stdout = startup + MARKER + encoded + '\n' + suffix
        with tempfile.TemporaryDirectory(prefix='param-texture-runner-control-') as tmp:
            root = Path(tmp)
            binary = root / 'unused-godot'
            binary.write_bytes(b'runner control: not executed')
            output = root / 'receipt'
            result = subprocess.CompletedProcess([], code, stdout, stderr)
            def engine_result(*args, **kwargs):
                if mutate_binary:
                    binary.write_bytes(b'changed while the engine was running')
                return result
            with patch.object(sys, 'argv', ['run.py', '--godot', str(binary), '--output', str(output)]), \
                    patch.object(RUNNER.subprocess, 'run', side_effect=engine_result), \
                    contextlib.redirect_stdout(io.StringIO()):
                status = RUNNER.main()
            receipt = json.loads((output / 'result.json').read_text())
            self.assertEqual(bool(receipt['errors']), status != 0)
            return status

    def test_complete_result_passes(self):
        self.assertEqual(self.run_output(complete_payload()), 0)

    def test_malformed_or_incomplete_results_fail(self):
        valid = complete_payload()
        cases = {'vacuous': {}, 'truncated_json': '{"checks":63,', 'wrong_payload_type': [],
                 'null_rows': {**valid, 'observations': [None] * 248},
                 'empty_rows': {**valid, 'observations': [{}] * 248},
                 'string_rows': {**valid, 'observations': ['bad'] * 248}}
        for name, field, value in [
            ('checks_too_low', 'checks', 972), ('checks_too_high', 'checks', 974),
            ('checks_bool', 'checks', True), ('checks_float', 'checks', 63.0),
            ('wrong_failures_type', 'failures', None), ('fixture_failure', 'failures', ['failed']),
            ('missing_rows', 'observations', valid['observations'][:-1]),
        ]:
            cases[name] = {**valid, field: value}
        for name, field, value in [
            ('bool_variant_type', 'type', True), ('float_variant_type', 'type', 23.0),
            ('wrong_variant_type', 'type', 0), ('wrong_immediate_type', 'immediate_type', 0),
            ('wrong_expected_type', 'expected_type', 0), ('wrong_value_type', 'value', 11),
            ('mismatched_value', 'value', 'RID(22)'),
            ('mismatched_immediate', 'immediate', 'RID(22)'),
            ('mismatched_expected', 'expected', 'RID(22)'),
            ('unknown_label', 'label', 'unknown'),
        ]:
            payload = copy.deepcopy(valid)
            payload['observations'][0][field] = value
            cases[name] = payload
        for name, mutate in [
            ('duplicate_label', lambda rows: rows[1].update(label=rows[0]['label'])),
            ('reordered_labels', lambda rows: rows.reverse()),
            ('missing_field', lambda rows: rows[0].pop('value')),
            ('false_nil', lambda rows: rows[13].update(type=23, immediate_type=23, expected_type=23,
                                                      value='RID(0)', immediate='RID(0)', expected='RID(0)')),
            ('wrong_association', lambda rows: rows[10].update(value='RID(22)', immediate='RID(22)', expected='RID(22)')),
            ('wrong_last_slot', lambda rows: rows[238].update(value='RID(22)', immediate='RID(22)', expected='RID(22)')),
            ('wrong_cross_slot', lambda rows: rows[244].update(value='RID(22)', immediate='RID(22)', expected='RID(22)')),
            ('invalid_a', lambda rows: [row.update(value='RID(0)', immediate='RID(0)', expected='RID(0)')
                                        for row in rows if row['value'] == 'RID(11)']),
            ('same_a_b', lambda rows: [row.update(value='RID(11)', immediate='RID(11)', expected='RID(11)')
                                       for row in rows if row['value'] == 'RID(22)']),
        ]:
            payload = copy.deepcopy(valid)
            mutate(payload['observations'])
            cases[name] = payload
        for name, payload in cases.items():
            with self.subTest(name=name):
                self.assertNotEqual(self.run_output(payload), 0)

    def test_process_and_marker_controls_fail(self):
        payload = complete_payload()
        for name, options in [
            ('nonzero_exit', {'code': 1}), ('missing_metal', {'startup': ''}),
            ('engine_error', {'stderr': 'ERROR: test control\n'}),
            ('leak_warning', {'stderr': 'WARNING: ObjectDB instances leaked at exit\n'}),
            ('binary_changed', {'mutate_binary': True}),
            ('duplicate_marker', {'suffix': MARKER + json.dumps(payload) + '\n'}),
            ('trailing_json', {'suffix': ''}),
        ]:
            with self.subTest(name=name):
                value = json.dumps(payload) + ' junk' if name == 'trailing_json' else payload
                self.assertNotEqual(self.run_output(value, **options), 0)


if __name__ == '__main__':
    unittest.main()
