#!/usr/bin/env python3
"""Pure runner validation tests. No host, GPU or DSO compiler is invoked."""
import copy
import json
import re
from pathlib import Path
import tempfile
import unittest
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

    def test_duplicate_json_keys(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'selection.json'
            path.write_text('{"schema": 1, "schema": 2}')
            with self.assertRaises(ValueError):
                run.load_json(path)


if __name__ == '__main__':
    unittest.main()
