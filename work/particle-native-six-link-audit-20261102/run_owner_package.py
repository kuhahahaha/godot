"""Run the original owner's three pinned capture specs in dependency order."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess


SCENARIOS = ("success", "failure", "cancel")


def _digest(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def preflight(package: Path) -> list[Path]:
    specs = [package / f"{scenario}.json" for scenario in SCENARIOS]
    if any(not path.is_file() for path in specs):
        raise ValueError("owner package must contain success.json, failure.json and cancel.json")
    capture_path = Path(__file__).with_name("capture_run.py")
    module_spec = importlib.util.spec_from_file_location("owner_capture_run", capture_path)
    capture_module = importlib.util.module_from_spec(module_spec)
    module_spec.loader.exec_module(capture_module)
    for scenario, path in zip(SCENARIOS, specs):
        spec = json.loads(path.read_text())
        if spec.get("scenario") != scenario:
            raise ValueError(f"{path} does not declare scenario {scenario}")
        capture_module._validate(spec)
        if not {"readback", "correlation"}.issubset(spec.get("outputs", {})):
            raise ValueError(f"{scenario} must declare readback and correlation outputs")
        for name, item in spec["inputs"].items():
            source = Path(item["path"])
            if not source.is_file() or _digest(source) != item["sha256"]:
                raise ValueError(f"{scenario} input pin mismatch: {name}")
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
        specs = preflight(package)
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.error(str(error))
    evidence.mkdir(parents=True)
    root = Path(__file__).resolve().parent
    for scenario, spec in zip(SCENARIOS, specs):
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
    print("OWNER_PACKAGE_EVIDENCE_INTACT production_acceptance=NOT_PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
