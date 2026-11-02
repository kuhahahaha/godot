"""Process-capture controls only; these tests do not issue native tickets."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import subprocess
import time
import tempfile
import unittest

MODULE_PATH = Path(__file__).with_name("capture_run.py")
VERIFY_PATH = Path(__file__).with_name("verify_capture.py")


class CaptureRunTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location("capture_run", MODULE_PATH)
        cls.runner = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.runner)
        verifier_spec = importlib.util.spec_from_file_location("verify_capture", VERIFY_PATH)
        cls.verifier = importlib.util.module_from_spec(verifier_spec)
        verifier_spec.loader.exec_module(cls.verifier)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.script = self.root / "program.py"
        self.script.write_text("print('capture control')\n")
        paths = {key: self.root / key for key in
                 ("sdk", "dso", "request", "oracle", "issuer", "project")}
        for key, path in paths.items():
            path.write_text("unit test input: " + key)
        paths.update(editor=Path(sys.executable).resolve(), script=self.script)
        self.spec = {
            "scenario": "success",
            "cwd": str(self.root),
            "timeout_seconds": 5,
            "declared_versions": {key: "unit-test-only" for key in
                                  ("godot_revision", "ange_revision", "editor_version",
                                   "sdk_version", "dso_version", "renderer", "device")},
            "inputs": {key: {"path": str(path),
                             "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                       for key, path in paths.items()},
            "command": ["{editor}", "{script}"],
        }
        self.out = self.root / "capture"

    def run_capture(self):
        return self.runner.capture(self.spec, self.out)

    def test_success_keeps_raw_logs_and_never_qualifies_production(self):
        result = self.run_capture()
        self.assertEqual(result["status"], "PROCESS_CAPTURED")
        self.assertEqual(result["returncode"], 0)
        self.assertEqual((self.out / "stdout.bin").read_bytes(), b"capture control\n")
        self.assertEqual((self.out / "stderr.bin").read_bytes(), b"")
        self.assertEqual(result["production_acceptance"], "NOT_PASSED")
        self.assertEqual(result["native_cleanup"], "REQUIRES_SAME_OWNER_EVIDENCE")
        self.assertEqual(result, json.loads((self.out / "capture.json").read_text()))

    def test_pin_mismatch_prevents_launch(self):
        self.spec["inputs"]["dso"]["sha256"] = "0" * 64
        result = self.run_capture()
        self.assertEqual(result["status"], "INPUT_PIN_MISMATCH")
        self.assertIsNone(result["returncode"])
        self.assertFalse((self.out / "stdout.bin").exists())

    def test_missing_required_input_prevents_launch(self):
        del self.spec["inputs"]["issuer"]
        with self.assertRaisesRegex(ValueError, "issuer"):
            self.run_capture()
        self.assertFalse(self.out.exists())

    def test_another_executable_cannot_bypass_editor_pin(self):
        self.spec["command"][0] = "/bin/echo"
        with self.assertRaisesRegex(ValueError, "editor"):
            self.run_capture()

    def test_process_failure_preserves_return_code_and_stderr(self):
        self.spec["command"] = ["{editor}", "-c",
                                "import sys; sys.stderr.buffer.write(b'failure\\x00'); sys.exit(7)"]
        result = self.run_capture()
        self.assertEqual(result["status"], "PROCESS_FAILED")
        self.assertEqual(result["returncode"], 7)
        self.assertEqual((self.out / "stderr.bin").read_bytes(), b"failure\x00")

    def test_mutated_pinned_input_is_reported(self):
        self.spec["command"] = ["{editor}", "-c",
                                "from pathlib import Path; Path('request').write_text('changed')"]
        result = self.run_capture()
        self.assertEqual(result["status"], "INPUT_DRIFT")
        self.assertNotEqual(result["inputs_before"], result["inputs_after"])

    def test_timeout_is_reaped_without_claiming_native_cleanup(self):
        self.spec["timeout_seconds"] = 0.1
        self.spec["command"] = ["{editor}", "-c", "import time; time.sleep(10)"]
        result = self.run_capture()
        self.assertEqual(result["status"], "PROCESS_TIMEOUT")
        self.assertTrue(result["timed_out"])
        self.assertIsNotNone(result["returncode"])
        self.assertEqual(result["native_cleanup"], "NOT_ESTABLISHED_AFTER_FORCED_STOP")

    def test_capture_directory_cannot_overwrite_inputs(self):
        self.out.mkdir()
        (self.out / "capture.json").write_text("prior receipt")
        with self.assertRaises(FileExistsError):
            self.run_capture()
        self.assertEqual((self.out / "capture.json").read_text(), "prior receipt")

    def test_missing_artifact_is_not_a_successful_capture(self):
        Path(self.spec["inputs"]["dso"]["path"]).unlink()
        result = self.run_capture()
        self.assertEqual(result["status"], "INPUT_PIN_MISMATCH")
        self.assertIsNone(result["inputs_before"]["dso"]["sha256"])

    def test_literal_braces_in_command_are_preserved(self):
        self.spec["command"] = ["{editor}", "-c", "print({'same': 'request'})"]
        result = self.run_capture()
        self.assertEqual(result["status"], "PROCESS_CAPTURED")
        self.assertEqual((self.out / "stdout.bin").read_bytes(), b"{'same': 'request'}\n")
        self.assertEqual(result["command"][2], self.spec["command"][2])

    def test_invalid_timeout_is_rejected_before_launch(self):
        for value in (0, -1, float("nan"), float("inf")):
            with self.subTest(timeout=value):
                self.spec["timeout_seconds"] = value
                with self.assertRaisesRegex(ValueError, "timeout"):
                    self.run_capture()
        self.assertFalse(self.out.exists())

    def test_relative_input_path_is_rejected(self):
        self.spec["inputs"]["request"]["path"] = "request"
        with self.assertRaisesRegex(ValueError, "absolute"):
            self.run_capture()

    def test_scenario_is_explicit(self):
        self.spec["scenario"] = "fixture-success-is-production"
        with self.assertRaisesRegex(ValueError, "scenario"):
            self.run_capture()

    def test_launch_failure_is_retained(self):
        self.spec["inputs"]["editor"] = self.spec["inputs"]["request"].copy()
        result = self.run_capture()
        self.assertEqual(result["status"], "LAUNCH_FAILED")
        self.assertIsNone(result["returncode"])
        self.assertTrue((self.out / "stderr.bin").read_bytes())

    def test_full_launch_spec_and_log_hashes_are_retained(self):
        result = self.run_capture()
        self.assertEqual(result["spec"], self.spec)
        self.assertEqual(result["cwd"], str(self.root))
        for name in ("stdout.bin", "stderr.bin"):
            self.assertEqual(result["logs"][name],
                             hashlib.sha256((self.out / name).read_bytes()).hexdigest())

    def test_declared_raw_output_is_copied_with_exact_bytes(self):
        raw = self.root / "readback.bin"
        self.spec["outputs"] = {"readback": str(raw)}
        self.spec["command"] = ["{editor}", "-c",
                                "from pathlib import Path; Path('readback.bin').write_bytes(bytes([0,7,8,9,255]))"]
        result = self.run_capture()
        self.assertEqual(result["status"], "PROCESS_CAPTURED")
        artifact = result["output_artifacts"]["readback"]
        self.assertEqual((self.out / "artifacts/readback").read_bytes(), bytes([0,7,8,9,255]))
        self.assertEqual(artifact["size"], 5)
        self.assertEqual(artifact["sha256"], hashlib.sha256(bytes([0,7,8,9,255])).hexdigest())
        self.assertEqual(result["production_acceptance"], "NOT_PASSED")

    def test_missing_declared_output_is_recorded_as_failed_capture(self):
        self.spec["outputs"] = {"readback": str(self.root / "absent.bin")}
        result = self.run_capture()
        self.assertEqual(result["status"], "OUTPUT_INCOMPLETE")
        self.assertEqual(result["output_artifacts"]["readback"]["status"], "MISSING")

    def test_preexisting_output_is_rejected_before_launch(self):
        self.spec["outputs"] = {"readback": self.spec["inputs"]["request"]["path"]}
        with self.assertRaisesRegex(ValueError, "output paths must not exist"):
            self.run_capture()
        self.assertFalse(self.out.exists())

    def test_nonzero_process_still_preserves_raw_output(self):
        self.spec["outputs"] = {"readback": str(self.root / "failure.bin")}
        self.spec["command"] = ["{editor}", "-c",
                                "from pathlib import Path; import sys; Path('failure.bin').write_bytes(b'partial'); sys.exit(7)"]
        result = self.run_capture()
        self.assertEqual(result["status"], "PROCESS_FAILED")
        self.assertEqual(result["returncode"], 7)
        self.assertEqual((self.out / "artifacts/readback").read_bytes(), b"partial")

    def test_verifier_checks_captured_raw_output(self):
        self.spec["outputs"] = {"readback": str(self.root / "bytes.bin")}
        self.spec["outputs"]["correlation"] = str(self.root / "correlation.json")
        self.spec["command"] = ["{editor}", "-c",
                                "from pathlib import Path; Path('bytes.bin').write_bytes(bytes([0,1,255])); Path('correlation.json').write_text('{}')"]
        self.run_capture()
        result = self.verifier.verify(self.out)
        self.assertEqual(result["status"], "EVIDENCE_INTACT")
        self.assertEqual(result["production_acceptance"], "NOT_PASSED")
        self.assertTrue(result["checks"]["artifact:readback"])

    def test_verifier_rejects_mutated_captured_bytes(self):
        self.spec["outputs"] = {"readback": str(self.root / "bytes.bin")}
        self.spec["outputs"]["correlation"] = str(self.root / "correlation.json")
        self.spec["command"] = ["{editor}", "-c",
                                "from pathlib import Path; Path('bytes.bin').write_bytes(b'original'); Path('correlation.json').write_text('{}')"]
        self.run_capture()
        (self.out / "artifacts/readback").write_bytes(b"replaced")
        result = self.verifier.verify(self.out)
        self.assertEqual(result["status"], "EVIDENCE_INCOMPLETE")
        self.assertFalse(result["checks"]["artifact:readback"])

    def test_verifier_rejects_missing_required_output_declarations(self):
        self.run_capture()
        result = self.verifier.verify(self.out)
        self.assertEqual(result["status"], "EVIDENCE_INCOMPLETE")
        self.assertFalse(result["checks"]["required_raw_outputs"])

    def test_timeout_cannot_wait_for_child_inheriting_logs(self):
        self.spec["timeout_seconds"] = 0.2
        self.spec["command"] = ["{editor}", "-c",
                                "import subprocess, sys, time; "
                                "subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(4)']); "
                                "time.sleep(4)"]
        started = time.monotonic()
        result = self.run_capture()
        self.assertLess(time.monotonic() - started, 2)
        self.assertEqual(result["status"], "PROCESS_TIMEOUT")

    def test_cli_captures_spec_and_returns_nonzero_for_pin_failure(self):
        self.spec["inputs"]["dso"]["sha256"] = "0" * 64
        spec_path = self.root / "spec.json"
        spec_path.write_text(json.dumps(self.spec))
        process = subprocess.run([sys.executable, str(MODULE_PATH), "--spec", str(spec_path),
                                  "--output", str(self.out)], capture_output=True)
        self.assertNotEqual(process.returncode, 0)
        result = json.loads((self.out / "capture.json").read_text())
        self.assertEqual(result["status"], "INPUT_PIN_MISMATCH")
        self.assertEqual(result["spec"], self.spec)


if __name__ == "__main__":
    unittest.main()
