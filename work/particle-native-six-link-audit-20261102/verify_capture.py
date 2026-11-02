"""Verify captured process evidence bytes without claiming production acceptance."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def _digest(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def verify(root: Path) -> dict:
    root = root.resolve()
    capture = json.loads((root / "capture.json").read_text())
    checks: dict[str, bool] = {}
    checks["capture_status"] = capture.get("status") == "PROCESS_CAPTURED"
    checks["acceptance_unclaimed"] = capture.get("production_acceptance") == "NOT_PASSED"
    checks["inputs_pinned"] = all(
        item.get("sha256") == capture.get("spec", {}).get("inputs", {}).get(name, {}).get("sha256")
        for name, item in capture.get("inputs_before", {}).items()
    ) and bool(capture.get("inputs_before"))
    checks["inputs_unchanged"] = capture.get("inputs_before") == capture.get("inputs_after")
    for name in ("stdout.bin", "stderr.bin"):
        path = root / name
        checks[name] = path.is_file() and _digest(path) == capture.get("logs", {}).get(name)
    declared = capture.get("spec", {}).get("outputs", {})
    recorded = capture.get("output_artifacts", {})
    checks["output_inventory"] = set(declared) == set(recorded)
    checks["required_raw_outputs"] = {"readback", "correlation"}.issubset(declared)
    for name, item in recorded.items():
        path = root / "artifacts" / name
        checks[f"artifact:{name}"] = (
            name in declared
            and item.get("source_path") == declared[name]
            and Path(item.get("capture_path", "")).resolve() == path
            and path.is_file()
            and not path.is_symlink()
            and path.stat().st_size == item.get("size")
            and _digest(path) == item.get("sha256")
        )
    return {
        "status": "EVIDENCE_INTACT" if all(checks.values()) else "EVIDENCE_INCOMPLETE",
        "checks": checks,
        "production_acceptance": "NOT_PASSED",
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture_dir", type=Path)
    args = parser.parse_args()
    try:
        result = verify(args.capture_dir)
    except (OSError, ValueError, KeyError, TypeError) as error:
        result = {"status": "EVIDENCE_INCOMPLETE", "error": str(error), "production_acceptance": "NOT_PASSED"}
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result["status"] == "EVIDENCE_INTACT" else 1


if __name__ == "__main__":
    raise SystemExit(main())
