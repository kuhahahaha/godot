#!/usr/bin/env python3
"""Offline R2 archive verifier; never runs or reads the current engine/source."""

import gzip
import hashlib
import json
import xml.etree.ElementTree as ET
from pathlib import Path

HERE = Path(__file__).resolve().parent
HOST = "c5e4bc1951643a6968c69a73d15b19abd0479de174bfa8e5898a4a689a83f939"
R1_HOST = "7e2c5796c813a48b7587fbec6356c0ae9f90f735abf621cf0c6ea8e17b317774"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def raw(name, base=HERE):
    data = (base / name).read_bytes()
    return gzip.decompress(data) if name.endswith(".gz") else data


def doc(name, base=HERE):
    return json.loads(raw(name, base))


def verify():
    manifest = doc("MANIFEST.json")
    actual = {str(p.relative_to(HERE)) for p in HERE.rglob("*") if p.is_file()}
    assert actual == set(manifest["files"]) | {"MANIFEST.json", "ARCHIVE_VERIFICATION.json"}
    for name, pin in manifest["files"].items():
        data = (HERE / name).read_bytes()
        assert {"bytes": len(data), "sha256": digest(data)} == pin, name
    entries = doc("ORIGINAL_FILE_MAP.json")["entries"]
    for e in entries:
        stored = (HERE / e["path"]).read_bytes()
        content = raw(e["path"])
        assert len(stored) == e["stored_bytes"] and digest(stored) == e["stored_sha256"], e["path"]
        assert len(content) == e["original_bytes"] and digest(content) == e["original_sha256"], e["path"]
    pins = doc("parent/FINAL_R2_SOURCE_PINS.json")
    snapshots = doc("SOURCE_SNAPSHOT_MAP.json")
    assert len(pins) == len(snapshots) == 19
    for e in snapshots:
        content = raw(e["path"])
        assert pins[e["source"]] == {"bytes": len(content), "sha256": digest(content)}
    gpu = doc("parent/green_r2_gpu/result.json")
    assert gpu["passed"] is True and gpu["host_sha256"] == HOST and gpu["host_and_sources_unchanged"] is True
    for name, pin in pins.items():
        if name in gpu["sources_before"]:
            assert pin == gpu["sources_before"][name], name
    for label in ["FINAL_BUILD", "FULL", "GPU"]:
        result = doc("parent/GREEN_R2_" + label + "_RESULT.json")
        assert result["exit"] == 0 and result["host_sha256"] == HOST
    assert len(gpu["cases"]) == 2
    stages = {
        "live": (1, 1, False),
        "framebuffer_enqueued": (1, 2, False),
        "framebuffer_returned": (1, 3, False),
        "depth_enqueued": (2, 3, False),
        "returned": (3, 3, True),
    }
    values = []
    for index, case in enumerate(gpu["cases"]):
        r = case["report"]
        assert case["case_index"] == index and case["passed"] is True and case["returncode"] == r["error"] == 0
        assert len(r["checks"]) == len({c["check"] for c in r["checks"]}) == 29 and all(
            c["ok"] is True for c in r["checks"]
        )
        assert r["failures"] == [] and r["physical_prefix_returned"] is True and r["retained_on_failure"] is False
        assert r["producer_complete"] is r["whole_output_released"] is r["reservation_refunded"] is False
        assert r["device"]["driver"] == "metal" and r["device"]["renderer"] == "mobile"
        q = r["quote"]
        assert q["framebuffer_owner_reservation"] == {"knowledge": "Known", "bytes": 0}
        assert q["framebuffer_driver_backing"] == q["opaque_driver_overhead"] == {"knowledge": "UnknownNotExposed"}
        identities = None
        actual_bytes = None
        assert set(r["snapshots"]) == set(stages)
        for name, expected in stages.items():
            s = r["snapshots"][name]
            assert (s["depth"]["state"], s["framebuffer"]["state"], s["all_created_children_returned"]) == expected
            current = tuple((s[k]["rid"], s[k]["serial"]) for k in ["depth", "framebuffer"])
            assert all(rid > 0 and serial > 0 for rid, serial in current)
            assert current[0][0] != current[1][0] and current[0][1] != current[1][1]
            assert identities is None or identities == current
            identities = current
            assert s["shared_attachment_depth"] == s["depth"]
            assert s["depth_driver_backing"]["knowledge"] == "Known"
            size = s["depth_driver_backing"]["bytes"]
            assert size > 0 and (actual_bytes is None or size == actual_bytes)
            actual_bytes = size
            for key in [
                "framebuffer_driver_backing",
                "framebuffer_object",
                "attachment_reference",
                "opaque_driver_overhead",
            ]:
                assert s[key] == {"knowledge": "UnknownNotExposed"}
        assert (
            actual_bytes <= q["texture_owner_reservation"]["bytes"]
            and q["texture_native_requirement"]["bytes"] <= q["texture_owner_reservation"]["bytes"]
        )
        for stream in ["stdout", "stderr"]:
            assert digest(raw(f"parent/green_r2_gpu/{index}/{stream}.log.gz")) == case[stream + "_sha256"]
        assert raw(f"parent/green_r2_gpu/{index}/stderr.log.gz") == b""
        values.append({
            "case": index,
            "checks": 29,
            "actual_depth_bytes": actual_bytes,
            "texture_credit_bytes": q["texture_owner_reservation"]["bytes"],
            "framebuffer_credit_bytes": 0,
        })
    tree = ET.fromstring(raw("parent/GREEN_R2_FULL.xml.gz"))
    cases = tree.find("OverallResultsTestCases").attrib
    assertions = tree.find("OverallResultsAsserts").attrib
    assert int(cases["successes"]) == 2269 and int(cases["failures"]) == 0 and int(cases["skipped"]) == 3
    assert int(assertions["successes"]) == 585464 and int(assertions["failures"]) == 0
    hooks = doc("parent/FINAL_HOOKS_R3.json")
    assert len(hooks) == 10 and all(h["exit"] == 0 for h in hooks)
    for label in ["VIEWPORT", "LEGACY", "NEIGHBORS"]:
        result = doc("historical_neighbor/GREEN_FINAL_" + label + "_RESULT.json")
        assert result["exit"] == 0 and result["host_sha256"] == R1_HOST
    # Verify versioned encoding changes against the preserved original maps.
    for scope in [HERE.parent, HERE]:
        receipt = doc("packaging_v2/RECEIPT.json", scope)
        for entry in receipt["diffs"]:
            content = raw(entry["path"], scope)
            stored = (scope / entry["path"]).read_bytes()
            assert len(content) == entry["original_bytes"] and digest(content) == entry["original_sha256"]
            assert len(stored) == entry["stored_bytes"] and digest(stored) == entry["stored_sha256"]
        before = doc("packaging_v2/ORIGINAL_FILE_MAP.json.gz", scope)["entries"]
        after = doc("ORIGINAL_FILE_MAP.json", scope)["entries"]
        assert len(before) == len(after)
        for left, right in zip(before, after):
            assert left["original_path"] == right["original_path"]
            assert left["original_bytes"] == right["original_bytes"]
            assert left["original_sha256"] == right["original_sha256"]
    old = HERE.parent
    prior = doc("MANIFEST.json", old)
    for name, pin in prior["files"].items():
        content = (old / name).read_bytes()
        assert {"bytes": len(content), "sha256": digest(content)} == pin, name
    receipt = doc("parent/RED_R4_CONTROLLED_MUTATION_RECEIPT.json")
    preimage = raw("final_source/servers/rendering/rendering_device.cpp.gz", old)
    assert digest(preimage) == receipt["final_sha256"]
    assert preimage.count(receipt["replace_from"].encode()) == 1
    mutant = preimage.replace(receipt["replace_from"].encode(), receipt["replace_to"].encode())
    assert digest(mutant) == receipt["negative_sha256_reconstructed"]
    red = doc("parent/red_r4_gpu/result.json", old)
    assert red["passed"] is False and len(red["cases"]) == 2
    for case in red["cases"]:
        assert case["report"]["failures"] == ["only_framebuffer_driver_returned"] and case["returncode"] == 1
    return {
        "status": "PASS_ARCHIVE_R2_FINAL",
        "payload_files": len(manifest["files"]),
        "copied_original_entries": len(entries),
        "source_snapshots": 19,
        "host_sha256": HOST,
        "cpu_cases": 2269,
        "cpu_assertions": 585464,
        "cpu_skipped": 3,
        "gpu": values,
        "hooks_passed": 10,
        "r1_raw_evidence_preserved_with_versioned_packaging": True,
        "historical_neighbor_host_sha256": R1_HOST,
        "red_negative_hash_is_later_reconstruction": True,
        "independent_review_is_prior_revision": True,
        "production_cross_dso_complete": False,
    }


if __name__ == "__main__":
    print(json.dumps(verify(), indent=2))
