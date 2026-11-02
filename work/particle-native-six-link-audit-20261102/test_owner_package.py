"""Controls for preflighting a real owner package before any launch."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import subprocess
import tempfile
import unittest


RUNNER = Path(__file__).with_name("run_owner_package.py")


class OwnerPackageTests(unittest.TestCase):
    def test_success_cannot_rewrite_later_spec_before_launch(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            package = root / "owner"
            package.mkdir()
            marker = root / "failure-launched"
            script = root / "program.py"
            script.write_text(
                "from pathlib import Path\n"
                "import json, sys\n"
                "name = sys.argv[1]\n"
                "Path(name + '.bin').write_bytes(b'raw')\n"
                "Path(name + '.out.json').write_text('{}')\n"
                "if name == 'success':\n"
                "    path = Path('owner/failure.json')\n"
                "    spec = json.loads(path.read_text())\n"
                "    spec['command'].append('changed')\n"
                "    path.write_text(json.dumps(spec))\n"
                "if name == 'failure':\n"
                "    Path('failure-launched').write_text('yes')\n"
            )
            paths = {key: root / key for key in
                     ("sdk", "dso", "request", "oracle", "issuer", "project")}
            for path in paths.values():
                path.write_text("input")
            paths.update(editor=Path(sys.executable).resolve(), script=script)
            inputs = {key: {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                      for key, path in paths.items()}
            versions = {key: "control" for key in
                        ("godot_revision", "ange_revision", "editor_version", "sdk_version",
                         "dso_version", "renderer", "device")}
            for scenario in ("success", "failure", "cancel"):
                (package / f"{scenario}.json").write_text(json.dumps({
                    "scenario": scenario, "cwd": str(root), "timeout_seconds": 5,
                    "declared_versions": versions, "inputs": inputs,
                    "command": ["{editor}", "{script}", scenario],
                    "outputs": {"readback": str(root / f"{scenario}.bin"),
                                "correlation": str(root / f"{scenario}.out.json")},
                }))
            run = subprocess.run([
                sys.executable, str(RUNNER), "--package", str(package),
                "--evidence", str(root / "evidence"),
            ], capture_output=True, text=True)
            self.assertNotEqual(run.returncode, 0)
            self.assertIn("spec drift", run.stdout + run.stderr)
            self.assertFalse(marker.exists())

    def test_output_paths_must_not_overlap_across_scenarios(self):
        spec = importlib.util.spec_from_file_location("run_owner_package", RUNNER)
        runner = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(runner)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            package = root / "owner"
            package.mkdir()
            script = root / "script.py"
            script.write_text("print('no launch')\n")
            paths = {key: root / key for key in
                     ("sdk", "dso", "request", "oracle", "issuer", "project")}
            for path in paths.values():
                path.write_text("input")
            paths.update(editor=Path(sys.executable).resolve(), script=script)
            inputs = {key: {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                      for key, path in paths.items()}
            versions = {key: "control" for key in
                        ("godot_revision", "ange_revision", "editor_version", "sdk_version",
                         "dso_version", "renderer", "device")}
            for scenario in runner.SCENARIOS:
                (package / f"{scenario}.json").write_text(json.dumps({
                    "scenario": scenario, "cwd": str(root), "timeout_seconds": 5,
                    "declared_versions": versions, "inputs": inputs,
                    "command": ["{editor}", "{script}"],
                    "outputs": {"readback": str(root / "shared.bin"),
                                "correlation": str(root / f"{scenario}.json")},
                }))
            with self.assertRaisesRegex(ValueError, "output.*overlap"):
                runner.preflight(package)

    def test_later_spec_pin_failure_prevents_success_launch(self):
        spec = importlib.util.spec_from_file_location("run_owner_package", RUNNER)
        runner = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(runner)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            package = root / "owner"
            package.mkdir()
            marker = root / "launched"
            script = root / "script.py"
            script.write_text(f"from pathlib import Path\nPath({str(marker)!r}).write_text('yes')\n")
            paths = {key: root / key for key in
                     ("sdk", "dso", "request", "oracle", "issuer", "project")}
            for path in paths.values():
                path.write_text("input")
            paths.update(editor=Path(sys.executable).resolve(), script=script)
            inputs = {key: {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                      for key, path in paths.items()}
            versions = {key: "control" for key in
                        ("godot_revision", "ange_revision", "editor_version", "sdk_version",
                         "dso_version", "renderer", "device")}
            for scenario in runner.SCENARIOS:
                scenario_inputs = json.loads(json.dumps(inputs))
                if scenario == "failure":
                    scenario_inputs["dso"]["sha256"] = "0" * 64
                (package / f"{scenario}.json").write_text(json.dumps({
                    "scenario": scenario, "cwd": str(root), "timeout_seconds": 5,
                    "declared_versions": versions, "inputs": scenario_inputs,
                    "command": ["{editor}", "{script}"],
                    "outputs": {"readback": str(root / f"{scenario}.bin"),
                                "correlation": str(root / f"{scenario}.out.json")},
                }))
            with self.assertRaisesRegex(ValueError, "failure.*dso"):
                runner.preflight(package)
            self.assertFalse(marker.exists())


if __name__ == "__main__":
    unittest.main()
