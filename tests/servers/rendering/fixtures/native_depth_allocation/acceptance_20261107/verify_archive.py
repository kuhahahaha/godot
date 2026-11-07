#!/usr/bin/env python3
"""Verify archived original native-depth evidence without running the engine."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
from pathlib import Path
from typing import Any
import xml.etree.ElementTree as ET

HERE = Path(__file__).resolve().parent
FINAL_HOST = "7e2c5796c813a48b7587fbec6356c0ae9f90f735abf621cf0c6ea8e17b317774"
RED_HOST = "dafe816067c21a2857274f3eb1daf3885470e403704938876d777755b53a5be9"


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def raw(path: str) -> bytes:
    data = (HERE / path).read_bytes()
    return gzip.decompress(data) if path.endswith(".gz") else data


def document(path: str) -> Any:
    return json.loads(raw(path))


def verify(compare_originals: bool) -> dict[str, Any]:
    manifest = document("MANIFEST.json")
    actual = {str(p.relative_to(HERE)) for p in HERE.rglob("*") if p.is_file()}
    assert actual == set(manifest["files"]) | {"MANIFEST.json", "ARCHIVE_VERIFICATION.json"}, (
        "archive inventory differs"
    )
    for name, expected in manifest["files"].items():
        content = (HERE / name).read_bytes()
        assert len(content) == expected["bytes"] and digest(content) == expected["sha256"], name
    mapping = document("ORIGINAL_FILE_MAP.json")["entries"]
    for entry in mapping:
        stored = (HERE / entry["path"]).read_bytes()
        content = raw(entry["path"])
        assert len(stored) == entry["stored_bytes"] and digest(stored) == entry["stored_sha256"], entry["path"]
        assert len(content) == entry["original_bytes"] and digest(content) == entry["original_sha256"], entry["path"]
        if compare_originals:
            assert Path(entry["original_path"]).read_bytes() == content, entry["original_path"]
    pins = document("parent/FINAL_SOURCE_PINS.json")
    for name, expected in pins.items():
        suffix = ".gz" if Path(name).suffix in {".cpp", ".h"} else ""
        content = raw("final_source/" + name + suffix)
        assert len(content) == expected["bytes"] and digest(content) == expected["sha256"], name
    identity = document("independent_review/QUOTE_TEST_TEARDOWN_IDENTITY.json")
    assert identity["sha256"] == pins[identity["path"]]["sha256"]
    assert document("parent/GREEN_FINAL_BUILD_RESULT.json")["exit"] == 0
    assert document("parent/GREEN_FINAL_BUILD_RESULT.json")["host_sha256"] == FINAL_HOST
    assert document("parent/RED_R4_BUILD_RESULT.json")["exit"] == 0
    green = document("parent/green_final_gpu/result.json")
    red = document("parent/red_r4_gpu/result.json")
    assert (
        green["passed"] is True and green["host_sha256"] == FINAL_HOST and green["host_and_sources_unchanged"] is True
    )
    assert red["passed"] is False and red["host_sha256"] == RED_HOST and red["host_and_sources_unchanged"] is True
    assert len(green["cases"]) == len(red["cases"]) == 2
    # Runtime source pins were captured after restoration, not from the mutant build.
    assert red["sources_before"]["servers/rendering/rendering_device.cpp"]["sha256"] == pins["servers/rendering/rendering_device.cpp"]["sha256"]
    assert document("parent/SOURCE_RESTORATION_R4.json")["restored"] is True
    assert (
        document("parent/SOURCE_RESTORATION_R4.json")["sha256"]
        == pins["servers/rendering/rendering_device.cpp"]["sha256"]
    )
    stages = {
        "live": (1, 1, False),
        "framebuffer_enqueued": (1, 2, False),
        "framebuffer_returned": (1, 3, False),
        "depth_enqueued": (2, 3, False),
        "returned": (3, 3, True),
    }
    values = []
    for index, (good, bad) in enumerate(zip(green["cases"], red["cases"])):
        assert good["case_index"] == bad["case_index"] == index
        report = good["report"]
        assert good["passed"] is True and good["returncode"] == report["error"] == 0
        checks = report["checks"]
        assert len(checks) == len({row["check"] for row in checks}) == 29 and all(row["ok"] is True for row in checks)
        assert (
            report["failures"] == []
            and report["physical_prefix_returned"] is True
            and report["retained_on_failure"] is False
        )
        assert (
            report["device"]["driver"] == "metal"
            and report["device"]["renderer"] == "mobile"
            and report["device"]["api"] == "4.0"
        )
        assert report["producer_complete"] is report["whole_output_released"] is report["reservation_refunded"] is False
        assert set(report["snapshots"]) == set(stages)
        identities = None
        actual_bytes = None
        for name, expected in stages.items():
            snapshot = report["snapshots"][name]
            assert (
                snapshot["depth"]["state"],
                snapshot["framebuffer"]["state"],
                snapshot["all_created_children_returned"],
            ) == expected
            current = tuple((snapshot[k]["rid"], snapshot[k]["serial"]) for k in ("depth", "framebuffer"))
            assert all(rid > 0 and serial > 0 for rid, serial in current)
            assert current[0][0] != current[1][0] and current[0][1] != current[1][1]
            if identities is not None:
                assert identities == current
            identities = current
            assert snapshot["shared_attachment_depth"] == snapshot["depth"]
            backing = snapshot["depth_driver_backing"]
            assert backing["knowledge"] == "Known" and backing["bytes"] > 0
            if actual_bytes is not None:
                assert actual_bytes == backing["bytes"]
            actual_bytes = backing["bytes"]
            for key in (
                "framebuffer_driver_backing",
                "framebuffer_object",
                "attachment_reference",
                "opaque_driver_overhead",
            ):
                assert snapshot[key] == {"knowledge": "UnknownNotExposed"}
        quote = report["quote"]
        assert (
            quote["policy"] == index + 1
            and quote["allocations"] == 2
            and quote["parts"] == 1
            and quote["shared_attachment_references"] == 1
        )
        assert quote["texture_native_requirement"]["bytes"] <= quote["texture_owner_reservation"]["bytes"]
        assert actual_bytes is not None and actual_bytes <= quote["texture_owner_reservation"]["bytes"]
        assert bad["returncode"] == 1 and bad["passed"] is False
        assert bad["report"]["error"] == 47 and bad["report"]["failures"] == ["only_framebuffer_driver_returned"]
        assert len(bad["report"]["checks"]) == 22 and [
            row["check"] for row in bad["report"]["checks"] if not row["ok"]
        ] == ["only_framebuffer_driver_returned"]
        for folder, case in (("green_final_gpu", good), ("red_r4_gpu", bad)):
            for stream in ("stdout", "stderr"):
                content = raw(f"parent/{folder}/{index}/{stream}.log.gz")
                assert digest(content) == case[f"{stream}_sha256"]
        assert raw(f"parent/green_final_gpu/{index}/stderr.log.gz") == b""
        values.append({
            "case_index": index,
            "actual_bytes": actual_bytes,
            "reservation_bytes": quote["texture_owner_reservation"]["bytes"],
            "checks": 29,
            "target_red_failure": "only_framebuffer_driver_returned",
        })
    cpu = []
    for label, cases, assertions in (("FOCUSED", 55, 6841), ("FULL", 2269, 585450)):
        content = raw(f"parent/GREEN_FINAL_{label}.xml.gz")
        lines = content.decode().splitlines()
        derived = "\n".join(
            line for line in lines if not line.startswith("[doctest]") and not (line and set(line) == {"="})
        )
        tree = ET.fromstring(derived)
        found_cases = tree.find("OverallResultsTestCases")
        found_asserts = tree.find("OverallResultsAsserts")
        assert found_cases is not None and found_asserts is not None
        assert int(found_cases.attrib["successes"]) == cases and int(found_cases.attrib["failures"]) == 0
        assert int(found_asserts.attrib["successes"]) == assertions and int(found_asserts.attrib["failures"]) == 0
        assert document(f"parent/GREEN_FINAL_{label}_RESULT.json")["exit"] == 0
        cpu.append({
            "suite": label,
            "cases": cases,
            "assertions": assertions,
            "failures": 0,
            "raw_sha256": digest(content),
        })
    for name in ("CAMERA", "HEIGHT"):
        result = document(f"parent/GREEN_FINAL_{name}_RESULT.json")
        assert result["exit"] == 0 and result["host_sha256"] == FINAL_HOST
    return {
        "status": "PASS_ARCHIVE_R1_HISTORICAL_NOT_FINAL",
        "copied_original_entries": len(mapping),
        "payload_files": len(manifest["files"]),
        "original_bytes": sum(entry["original_bytes"] for entry in mapping),
        "stored_original_bytes": sum(entry["stored_bytes"] for entry in mapping),
        "compared_original_workspace_files": compare_originals,
        "final_source_pins": len(pins),
        "final_host_sha256": FINAL_HOST,
        "cpu": cpu,
        "gpu": values,
        "old_cpu_abort_and_r2_are_not_target_red": True,
        "red_runtime_source_pins_are_restored_green_not_mutant_build_provenance": True,
        "production_cross_dso_complete": False,
        "pending_parent_neighbors_and_hooks": True,
        "pending_r2_framebuffer_owner_reservation_acceptance": True,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compare-originals", action="store_true")
    args = parser.parse_args()
    print(json.dumps(verify(args.compare_originals), indent=2))


if __name__ == "__main__":
    main()
