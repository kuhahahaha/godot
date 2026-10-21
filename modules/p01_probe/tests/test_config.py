import importlib.util
import pathlib
import unittest


class ModuleEligibilityTests(unittest.TestCase):
    def test_requires_tests_and_3d(self):
        path = pathlib.Path(__file__).parents[1] / "config.py"
        spec = importlib.util.spec_from_file_location("p01_config", path)
        config = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(config)
        for tests in (False, True):
            for disable_3d in (False, True):
                with self.subTest(tests=tests, disable_3d=disable_3d):
                    self.assertEqual(
                        config.can_build({"tests": tests, "disable_3d": disable_3d}, "macos"),
                        tests and not disable_3d,
                    )


if __name__ == "__main__":
    unittest.main()
