import pathlib
import subprocess
import sys
import tempfile
import unittest


SCRIPT = pathlib.Path(__file__).with_name('check_plan.py')


class PlanCheckTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        self.plan = self.root / 'FR_sample_PLAN.md'
        (self.root / 'FR_sample.md').write_text('# Ask\n[PLAN](FR_sample_PLAN.md)\n', encoding='utf-8')
        (self.root / 'source.cpp').write_text('first\nsecond\n', encoding='utf-8')
        self.body = '''# Sample
[FR](FR_sample.md)
## 0. Decisions
## Sources
[source](source.cpp:2)
## Implementation
Success check: independent result.
## Tests
| ID | Trigger | Expected | Test / command | RED |
| --- | --- | --- | --- | --- |
| T01 | valid | accept | tests/a.py / case | reject valid |
## Example Impact
none: not-applicable — tooling only
## Build and acceptance
## File-change checklist
## Review
## Self-review
'''

    def run_check(self, body=None):
        self.plan.write_text(self.body if body is None else body, encoding='utf-8')
        return subprocess.run([sys.executable, str(SCRIPT), str(self.plan)],
                              text=True, capture_output=True)

    def test_valid_plan_and_escaped_pipe(self):
        result = self.run_check(self.body.replace('valid |', r'valid a\|b |'))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_missing_source_is_error(self):
        result = self.run_check(self.body.replace('source.cpp:2', 'absent.cpp:2'))
        self.assertEqual(result.returncode, 1)
        self.assertIn('missing target', result.stdout)

    def test_invalid_source_line_is_error(self):
        result = self.run_check(self.body.replace('source.cpp:2', 'source.cpp:3'))
        self.assertEqual(result.returncode, 1)
        self.assertIn('line out of range', result.stdout)

    def test_absolute_source_path_is_checked(self):
        result = self.run_check(self.body.replace('source.cpp:2', str(self.root / 'source.cpp') + ':0'))
        self.assertEqual(result.returncode, 1)

    def test_code_examples_are_not_links_or_sections(self):
        result = self.run_check(self.body + '\n```md\n[example](missing.cpp:7)\n```\n')
        self.assertEqual(result.returncode, 0, result.stdout)
        result = self.run_check(self.body.replace('## Review\n', '') + '\n```md\n## Review\n```\n')
        self.assertEqual(result.returncode, 1)

    def test_duplicate_test_id_is_error(self):
        result = self.run_check(self.body.replace('## Example Impact', '| T01 | bad | reject | x / bad | accept bad |\n## Example Impact'))
        self.assertEqual(result.returncode, 1)
        self.assertIn('duplicate test ID', result.stdout)

    def test_unescaped_pipe_and_empty_red_are_errors(self):
        result = self.run_check(self.body.replace('valid |', 'valid a|b |'))
        self.assertEqual(result.returncode, 1)
        result = self.run_check(self.body.replace('reject valid |', ' |'))
        self.assertEqual(result.returncode, 1)

    def test_missing_pair_and_unlinked_pair_are_errors(self):
        result = self.run_check(self.body.replace('[FR](FR_sample.md)', 'unlinked'))
        self.assertEqual(result.returncode, 1)
        (self.root / 'FR_sample.md').unlink()
        result = self.run_check()
        self.assertEqual(result.returncode, 1)

    def test_no_arguments_and_missing_plan_are_tool_errors(self):
        for args in [[], [str(self.root / 'absent_PLAN.md')]]:
            result = subprocess.run([sys.executable, str(SCRIPT), *args], capture_output=True)
            self.assertEqual(result.returncode, 2)

    def test_matrix_requires_meaningful_columns(self):
        result = self.run_check(self.body.replace(
            '| ID | Trigger | Expected | Test / command | RED |\n| --- | --- | --- | --- | --- |\n| T01 | valid | accept | tests/a.py / case | reject valid |',
            '| ID |\n| --- |\n| T01 |'))
        self.assertEqual(result.returncode, 1)

    def test_pair_must_link_back(self):
        (self.root / 'FR_sample.md').write_text('# Ask\n', encoding='utf-8')
        self.assertEqual(self.run_check().returncode, 1)

    def test_inline_code_is_not_a_live_link(self):
        result = self.run_check(self.body + '\n`[example](absent.cpp:1)`\n')
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_link_title_and_reference_style(self):
        result = self.run_check(self.body.replace('source.cpp:2)', 'source.cpp:2 "location")'))
        self.assertEqual(result.returncode, 0, result.stdout)
        result = self.run_check(self.body.replace('[FR](FR_sample.md)', '[FR][ask]\n\n[ask]: FR_sample.md'))
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_fence_closing_requires_no_info(self):
        result = self.run_check(self.body + '\n```md\n```fake\n[example](missing.cpp:1)\n```\n')
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_non_test_id_table_is_not_a_test_matrix(self):
        body = self.body.replace('## Sources\n', '## Sources\n| ID | Source |\n| --- | --- |\n| S01 | example |\n')
        result = self.run_check(body)
        self.assertEqual(result.returncode, 0, result.stdout)


if __name__ == '__main__':
    unittest.main()
