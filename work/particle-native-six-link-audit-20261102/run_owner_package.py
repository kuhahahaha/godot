"""Run the original owner's three pinned capture specs in dependency order."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess


SCENARIOS = ("success", "failure", "cancel")
FIXED_INPUTS = ("editor", "script", "sdk", "dso", "issuer", "project")
FIXED_VERSIONS = ("godot_revision", "ange_revision", "editor_version",
                  "sdk_version", "dso_version", "renderer", "device")
PAIR_FIELDS = ("core_stamp", "compiler_route", "compiler_stamp", "platform",
               "architecture", "target_kind")


def _digest(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as source:
            for block in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(block)
    except OSError:
        return ""
    return digest.hexdigest()


def _pair_selection(path: Path) -> dict[str, str] | None:
    """Read an adjacent controlled-build identity, when one is supplied."""
    candidates = [
        path.with_name(path.name + ".controlled_build.json"),
        path.with_name(path.name + ".particle-identity.json"),
    ]
    for candidate in candidates:
        if not candidate.is_file():
            continue
        value = json.loads(candidate.read_text())
        selection = value.get("selection", value.get("expected_core_selection"))
        if not isinstance(selection, dict) or any(field not in selection for field in PAIR_FIELDS):
            raise ValueError(f"invalid controlled-build identity: {candidate}")
        return {field: str(selection[field]) for field in PAIR_FIELDS}
    return None


def _validate_pair_inputs(spec: dict) -> None:
    editor = Path(spec["inputs"]["editor"]["path"])
    dso = Path(spec["inputs"]["dso"]["path"])
    editor_selection = _pair_selection(editor)
    dso_selection = _pair_selection(dso)
    if editor_selection is not None and dso_selection is not None and editor_selection != dso_selection:
        raise ValueError(
            "editor/dso controlled-build identity mismatch: "
            f"editor={editor_selection['core_stamp']}/{editor_selection['compiler_stamp']} "
            f"dso={dso_selection['core_stamp']}/{dso_selection['compiler_stamp']}"
        )


def preflight(package: Path, evidence: Path | None = None) -> list[Path]:
    specs = [package / f"{scenario}.json" for scenario in SCENARIOS]
    if any(not path.is_file() for path in specs):
        raise ValueError("owner package must contain success.json, failure.json and cancel.json")
    capture_path = Path(__file__).with_name("capture_run.py")
    module_spec = importlib.util.spec_from_file_location("owner_capture_run", capture_path)
    capture_module = importlib.util.module_from_spec(module_spec)
    module_spec.loader.exec_module(capture_module)
    output_paths: set[Path] = set()
    input_hashes: dict[Path, str] = {}
    fixed_inputs = None
    fixed_versions = None
    for scenario, path in zip(SCENARIOS, specs):
        spec = json.loads(path.read_text())
        if spec.get("scenario") != scenario:
            raise ValueError(f"{path} does not declare scenario {scenario}")
        capture_module._validate(spec)
        _validate_pair_inputs(spec)
        script = Path(spec["inputs"]["script"]["path"]).resolve()
        cwd = Path(spec["cwd"])
        def names_pinned_script(argument: str) -> bool:
            if argument == "{script}":
                return True
            candidate = Path(argument)
            return (candidate if candidate.is_absolute() else cwd / candidate).resolve() == script
        command = spec["command"]
        script_arguments = []
        index = 1
        while index < len(command):
            argument = command[index]
            if argument == "--script" and index + 1 < len(command):
                script_arguments.append(command[index + 1])
                index += 2
                continue
            if argument.startswith("--script="):
                script_arguments.append(argument.partition("=")[2])
            elif argument == "{script}" or (index == 1 and not argument.startswith("-")):
                script_arguments.append(argument)
            index += 1
        if not any(names_pinned_script(argument) for argument in script_arguments):
            raise ValueError(f"{scenario} script must appear in command")
        current_inputs = {name: spec["inputs"][name] for name in FIXED_INPUTS}
        current_versions = {name: spec["declared_versions"][name] for name in FIXED_VERSIONS}
        if fixed_inputs is None:
            fixed_inputs = current_inputs
            fixed_versions = current_versions
        else:
            for name in FIXED_INPUTS:
                if current_inputs[name] != fixed_inputs[name]:
                    raise ValueError(f"{scenario} fixed input mismatch across scenarios: {name}")
            if current_versions != fixed_versions:
                raise ValueError("fixed versions mismatch across scenarios")
        outputs = spec.get("outputs", {})
        if "correlation" not in outputs or (spec.get("expect_readback", True) and "readback" not in outputs):
            raise ValueError(f"{scenario} must declare expected readback and correlation outputs")
        if not spec.get("expect_readback", True):
            if scenario == "success" or "readback" in outputs:
                raise ValueError(f"{scenario} no-readback mode requires a failure or cancel scenario without readback output")
            oracle = Path(spec["inputs"]["oracle"]["path"])
            if not oracle.is_file() or oracle.stat().st_size != 0:
                raise ValueError(f"{scenario} no-readback mode requires an empty oracle")
        for output in spec["outputs"].values():
            destination = Path(output).resolve()
            if evidence is not None and destination.is_relative_to(evidence.resolve()):
                raise ValueError(f"{scenario} output overlaps evidence bundle: {destination}")
            if destination in output_paths:
                raise ValueError(f"output path overlap across scenarios: {destination}")
            output_paths.add(destination)
        for name, item in spec["inputs"].items():
            source = Path(item["path"])
            if not source.is_file() or _digest(source) != item["sha256"]:
                raise ValueError(f"{scenario} input pin mismatch: {name}")
            input_hashes[source.resolve()] = item["sha256"]
    if any(_digest(path) != digest for path, digest in input_hashes.items()):
        raise ValueError("owner package input drift during preflight")
    return specs


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    args = parser.parse_args()
    package = args.package.resolve()
    evidence = args.evidence.resolve()
    if not package.is_dir() or evidence.exists():
        parser.error("owner package must exist and evidence directory must be new")
    try:
        specs = preflight(package, evidence)
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.error(str(error))
    spec_hashes = {path: _digest(path) for path in specs}
    input_hashes = {
        Path(item["path"]).resolve(): item["sha256"]
        for spec in specs
        for item in json.loads(spec.read_text())["inputs"].values()
    }
    evidence.mkdir(parents=True)
    root = Path(__file__).resolve().parent
    for scenario, spec in zip(SCENARIOS, specs):
        if any(not path.is_file() or _digest(path) != spec_hashes[path] for path in specs):
            print(f"OWNER_PACKAGE_SPEC_DRIFT_STOP scenario={scenario} spec drift before launch")
            return 1
        if any(_digest(path) != digest for path, digest in input_hashes.items()):
            print(f"OWNER_PACKAGE_INPUT_DRIFT_STOP scenario={scenario} input drift before launch")
            return 1
        destination = evidence / scenario
        capture = subprocess.run([
            "python3", str(root / "capture_run.py"), "--spec", str(spec),
            "--output", str(destination),
        ], check=False)
        if capture.returncode != 0:
            print(f"OWNER_PACKAGE_CAPTURE_STOP scenario={scenario} rc={capture.returncode}")
            return capture.returncode
        verification = subprocess.run([
            "python3", str(root / "verify_capture.py"), str(destination),
        ], check=False)
        if verification.returncode != 0:
            print(f"OWNER_PACKAGE_VERIFY_STOP scenario={scenario} rc={verification.returncode}")
            return verification.returncode
        if any(not path.is_file() or _digest(path) != spec_hashes[path] for path in specs):
            print(f"OWNER_PACKAGE_SPEC_DRIFT_STOP scenario={scenario} spec drift after capture")
            return 1
        if any(_digest(path) != digest for path, digest in input_hashes.items()):
            print(f"OWNER_PACKAGE_INPUT_DRIFT_STOP scenario={scenario} input drift after capture")
            return 1
    print("OWNER_PACKAGE_EVIDENCE_INTACT production_acceptance=NOT_PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
