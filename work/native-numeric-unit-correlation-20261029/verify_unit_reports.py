#!/usr/bin/env python3
"""Check the actual V1 binary reports; this grants no production authority."""
import hashlib
import json
from pathlib import Path

p = Path(__file__).resolve().parent
expected = json.loads((p / "input/unit.json").read_text())["unit_digest"]
assert expected != "00" * 32
results = json.loads((p / "RESULTS.json").read_text())
rows = []
for case in ("positive", "cancel_prefix"):
    report = (p / ("result_" + case) / "report.bin").read_bytes()
    # Frozen V1 report: 184 bytes; final unit_digest[32] starts at 152.
    assert len(report) == 184
    digest = hashlib.sha256(report).hexdigest()
    run = next(r for r in results["runs"] if r["case"] == case)
    assert digest == run["outputs"]["report.bin"]["sha256"]
    assert report[152:184].hex() == expected
    rows.append({"case": case, "report_sha256": digest, "unit_digest": expected})
(p / "UNIT_REPORT_CHECK.json").write_text(json.dumps({
    "scope": "Passive binary report correlation only; no owner/operation issuer",
    "checks": rows,
}, indent=2) + "\n")
print("Both actual binary reports retain the nonzero original unit digest.")
