"""Check reproducible force-link generation from filesystem source discovery."""

import importlib.util
import itertools
import pathlib
import re
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
SPEC = importlib.util.spec_from_file_location("test_builders", ROOT / "tests/test_builders.py")
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("Cannot load the test source generator")
builder = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(builder)


class SourceValue:
    def __init__(self, paths):
        self.paths = paths

    def read(self):
        return self.paths


class ForceLinkBuilderTests(unittest.TestCase):
    def test_discovery_permutations_produce_identical_complete_header(self):
        paths = ["core/test_beta.cpp", "scene/test_gamma.cpp", "core/test_alpha.cpp"]
        with tempfile.TemporaryDirectory() as directory:
            target = pathlib.Path(directory) / "force_link.gen.h"
            results = []
            for permutation in itertools.permutations(paths):
                discovered = list(permutation)
                builder.force_link_builder([target], [SourceValue(discovered)], None)
                text = target.read_text()
                calls = re.findall(r"^\t(force_link_\w+)\(\);$", text, re.M)
                self.assertCountEqual(calls, ["force_link_test_alpha", "force_link_test_beta", "force_link_test_gamma"])
                self.assertEqual(discovered, list(permutation))
                results.append(target.read_bytes())
            for result in results[1:]:
                self.assertEqual(result, results[0])

    def test_empty_source_set_generates_an_empty_link_body(self):
        with tempfile.TemporaryDirectory() as directory:
            target = pathlib.Path(directory) / "force_link.gen.h"
            builder.force_link_builder([target], [SourceValue([])], None)
            self.assertIn("void ForceLink::force_link_tests()", target.read_text())
            self.assertEqual(re.findall(r"^\t(force_link_\w+)\(\);$", target.read_text(), re.M), [])


if __name__ == "__main__":
    unittest.main()
