#!/usr/bin/env python3
"""Pure runner validation tests. No host, GPU or DSO compiler is invoked."""
import copy
import json
import re
from pathlib import Path
import tempfile
import unittest
import subprocess
import sys
sys.dont_write_bytecode = True
import run


class RunnerValidation(unittest.TestCase):
    def selection(self):
        return dict(schema=1, core_stamp='0' * 64, compiler_route='BuiltinGlslangV1',
                    compiler_stamp='1' * 64, platform='macos', architecture='arm64', target_kind='Executable')

    def test_closed_selection(self):
        self.assertEqual(run.validate_selection(self.selection()), self.selection())
        for key, value in (('schema', True), ('core_stamp', 'wrong'), ('compiler_stamp', 1),
                           ('platform', 'linux'), ('architecture', 'x86_64'), ('target_kind', 'LinkedCore'),
                           ('compiler_route', ''), ('extra', 1)):
            selection = self.selection()
            selection[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                run.validate_selection(selection)
        for key in self.selection():
            selection = self.selection()
            del selection[key]
            with self.subTest(missing=key), self.assertRaises(ValueError):
                run.validate_selection(selection)

    def test_real_startup_line(self):
        for driver, line in (('metal', 'Metal 4.0'), ('vulkan', 'Vulkan 1.2.357')):
            text = line + ' - Forward Mobile - Using Device #0: Apple - actual device\n'
            self.assertTrue(run.actual_backend(text, driver))
            self.assertFalse(run.actual_backend('fixture says ' + driver + '\nForward Mobile\n', driver))
            self.assertFalse(run.actual_backend(text + text, driver))
            self.assertFalse(run.actual_backend(text.replace('Forward Mobile', 'Compatibility'), driver))
            other = 'vulkan' if driver == 'metal' else 'metal'
            self.assertFalse(run.actual_backend(text, other))

    def test_actual_engine_identity(self):
        text = 'SYSTEM_SCALAR_HOST_IDENTITY=' + json.dumps(self.selection()) + '\n'
        self.assertTrue(run.actual_identity(text, self.selection()))
        self.assertFalse(run.actual_identity(text + text, self.selection()))
        self.assertFalse(run.actual_identity('SYSTEM_SCALAR_HOST_IDENTITY={}\n', self.selection()))
        stale = copy.deepcopy(self.selection())
        stale['core_stamp'] = '2' * 64
        self.assertFalse(run.actual_identity(text, stale))

    def test_explicit_case_adapter(self):
        cases = run.load_negative_cases(Path(__file__).with_name('negative_cases.json'))
        self.assertEqual({case['name'] for case in cases}, run.CONTROL_NAMES)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'cases.json'
            malformed = {'schema': 1, 'cases': copy.deepcopy(cases)}
            malformed['cases'][0]['environment']['DYLD_INSERT_LIBRARIES'] = '1'
            path.write_text(json.dumps(malformed))
            with self.assertRaises(ValueError):
                run.load_negative_cases(path)
            malformed = {'schema': 1, 'cases': copy.deepcopy(cases)}
            malformed['cases'][0]['retirement_claim'] = True
            path.write_text(json.dumps(malformed))
            with self.assertRaises(ValueError):
                run.load_negative_cases(path)

    def test_actual_shared_capacity_gate(self):
        text = 'SYS_SCALAR_CAPACITY current=0 workers=11 bytes=16384 graphs=513 error=5\n'
        evidence = run.actual_capacity_evidence(text)
        self.assertEqual(evidence['status'], 'satisfied_shared_pre_driver_slot_refusal')
        self.assertEqual(evidence['frame_bound'], 514)
        self.assertEqual(evidence['required_slots'], 6168)
        self.assertEqual(run.actual_capacity_evidence(text.replace('workers=11', 'workers=6'))['status'], 'fixture_precondition_not_met')
        self.assertEqual(run.actual_capacity_evidence(text.replace('error=5', 'error=2'))['status'], 'unexpected_actual_capacity_error')
        self.assertEqual(run.actual_capacity_evidence(text + text)['status'], 'missing_or_ambiguous_actual_capacity_trace')
        self.assertEqual(run.actual_capacity_evidence('SYS_SCALAR_CAPACITY graphs=513 unsupported') ['status'], 'missing_or_ambiguous_actual_capacity_trace')
        # A nonzero preexisting main-command count contributes to the same bound.
        evidence = run.actual_capacity_evidence(text.replace('current=0', 'current=256'))
        self.assertEqual(evidence['required_slots'], 6424)
        self.assertEqual(evidence['status'], 'satisfied_shared_pre_driver_slot_refusal')

    def test_case_environment_isolation(self):
        inherited = {'PATH': '/example', 'GODOT_SYSTEM_SCALAR_TEST_APPEND_UNKNOWN': '1',
                     'GODOT_SYSTEM_SCALAR_TEST_SET_RETURN_ERROR': '1',
                     'GODOT_SYSTEM_SCALAR_TEST_FUTURE_HOOK': '1',
                     'GODOT_QUALIFICATION_CAPACITY_TRACE': '1',
                     'DYLD_LIBRARY_PATH': '/wrong', 'DYLD_FALLBACK_LIBRARY_PATH': '/wrong',
                     'DYLD_INSERT_LIBRARIES': '/wrong'}
        original = dict(inherited)
        positive = run.clean_environment(inherited)
        self.assertEqual(positive, {'PATH': '/example'})
        self.assertEqual(inherited, original)
        unknown = dict(positive, GODOT_SYSTEM_SCALAR_TEST_APPEND_UNKNOWN='1')
        self.assertNotIn('GODOT_SYSTEM_SCALAR_TEST_SET_RETURN_ERROR', unknown)
        self.assertEqual(positive, {'PATH': '/example'})

    def test_probe_identity_precedes_synchronous_ready(self):
        script = Path(__file__).with_name('probe.gd').read_text()
        run.validate_probe_constructor(script)
        with self.assertRaises(ValueError):
            run.validate_probe_constructor(script.replace('func _init():', 'func _initialize():'))
        with self.assertRaises(ValueError):
            run.validate_probe_constructor(script.replace('Engine.get_version_info()', 'Engine.get_main_loop()'))

    def test_negative_config_cannot_disable_global_fatal_checks(self):
        for control in ({'forbidden_patterns': []}, {'forbidden_patterns': ['CASE_SPECIFIC_FATAL']}):
            patterns = run.forbidden_patterns_for(control)
            for diagnostic in ('ERROR: native failure', 'SYSTEM_SCALAR_ASSERT', 'CrashHandler', 'leaked at exit', 'PHS1_FIXTURE_FATAL'):
                with self.subTest(control=control, diagnostic=diagnostic):
                    self.assertTrue(any(re.search(pattern, diagnostic) for pattern in patterns))
        self.assertIn('CASE_SPECIFIC_FATAL', run.forbidden_patterns_for({'forbidden_patterns': ['CASE_SPECIFIC_FATAL']}))

    def test_generator_and_probe_share_closed_selection_comparison(self):
        for name in ('generate.gd', 'probe.gd'):
            script = Path(__file__).with_name(name).read_text()
            with self.subTest(script=name):
                self.assertIn('preload("res://selection_identity.gd")', script)
                self.assertIn('if not SelectionIdentity.matches(identity, expected):', script)
                self.assertNotIn('if identity != expected:', script)
        helper = Path(__file__).with_name('selection_identity.gd')
        self.assertTrue(helper.is_file())
        self.assertTrue(Path(__file__).with_name('selection_identity_self_test.gd').is_file())

    def test_only_case_selector_keeps_default_complete_suite(self):
        names = ['positive', 'post_submit_close', 'native_unknown', 'native_fact_capacity', 'set_retire_error']
        self.assertEqual(run.select_case_names('all'), names)
        for name in names:
            self.assertEqual(run.select_case_names('all', name), [name])
        self.assertEqual(run.select_case_names('original'), names[:2])
        with self.assertRaises(ValueError):
            run.select_case_names('original', 'native_unknown')

    def test_exact_original_pending_and_body_tuple_cpp(self):
        root = Path(__file__).resolve().parent
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / 'post-submit-observation'
            subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-pedantic', str(root / 'post_submit_observation_test.cpp'), '-o', str(binary)], check=True)
            result = subprocess.run([str(binary)], check=True, text=True, capture_output=True)
            self.assertIn('post-submit original-source/Body tuple cases PASS (CPU only)', result.stdout)

    def test_actual_body_tuple_cannot_be_hidden_copy(self):
        text = ('SYSTEM_SCALAR_POST_SUBMIT_BODY graph=1 frame=3 kind=1 index=0 offset=0 command_slot=4 issue=1 completion=2 native=21 completion_serial=21 queue=1 native_row=2 native_semantic=5 native_issue=1 native_completion=2 state=8 native_code=4 body_native_code=0\n'
                'SYSTEM_SCALAR_POST_SUBMIT_OBSERVED semantic=5 issue=1 native=21 completion=21 queue=1 ticket=12 state=8 command_slot=4 body_native=21 body_completion=21 body_queue=1\n'
                'SYSTEM_SCALAR_POST_SUBMIT_CLOSE error=44 reason=16 original_ticket=12\n'
                'SYSTEM_SCALAR_POST_SUBMIT_ORIGINAL_RETURN sources=1 body_native=21 completion_serial=21 queue=1 command_slot=4 graph=1 frame=3\n')
        evidence = run.actual_body_submission(text)
        self.assertEqual(evidence['status'], 'matched_actual_body_native_tuple')
        self.assertEqual(evidence['body']['state'], 8)
        self.assertFalse(evidence['inflight_claim'])
        self.assertEqual(evidence['body']['native_code'], 4)
        self.assertEqual(evidence['body']['body_native_code'], 0)
        for before, after in (('command_slot=4', 'command_slot=4294967295'),
                              ('body_native=21', 'body_native=22'), ('native_issue=1', 'native_issue=2'),
                              ('native_semantic=5', 'native_semantic=2'),
                              ('sources=1 body_native=21', 'sources=1 body_native=22')):
            self.assertEqual(run.actual_body_submission(text.replace(before, after))['status'], 'body_native_tuple_mismatch')
        self.assertEqual(run.actual_body_submission(text + text)['status'], 'missing_or_ambiguous_body_submission')

    def test_duplicate_json_keys(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'selection.json'
            path.write_text('{"schema": 1, "schema": 2}')
            with self.assertRaises(ValueError):
                run.load_json(path)


if __name__ == '__main__':
    unittest.main()
