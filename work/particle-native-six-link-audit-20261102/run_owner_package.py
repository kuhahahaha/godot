"""Run the original owner's three pinned capture specs in dependency order."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess


SCENARIOS = ("success", "failure", "cancel")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    args = parser.parse_args()
    package = args.package.resolve()
    evidence = args.evidence.resolve()
    if not package.is_dir() or evidence.exists():
        parser.error("owner package must exist and evidence directory must be new")
    specs = [package / f"{scenario}.json" for scenario in SCENARIOS]
    if any(not path.is_file() for path in specs):
        parser.error("owner package must contain success.json, failure.json and cancel.json")
    for scenario, path in zip(SCENARIOS, specs):
        spec = json.loads(path.read_text())
        if spec.get("scenario") != scenario:
            parser.error(f"{path} does not declare scenario {scenario}")
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
