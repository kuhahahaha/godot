#!/usr/bin/env python3
"""Verify preserved height-range evidence offline without invoking an engine."""

from __future__ import annotations

import gzip
import hashlib
import json
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any

HERE = Path(__file__).resolve().parent
FINAL_HOST = "1e4b08bf515d6c40e3b8a106c3d535bb8564e467c99a9a4bbdf2d3a5c06f739b"
THIRD_HOST = "dc470ec119af6c28eaaf7a36a554fd5d218d33f9d8a159e9376e2b8015c22e83"
ERROR_FIELDS = ("append_error", "mapping_error", "issue_error", "retention_error", "completion_error", "return_error")


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def raw(name: str) -> bytes:
    data = (HERE / name).read_bytes()
    return gzip.decompress(data) if name.endswith(".gz") else data


def doc(name: str) -> Any:
    return json.loads(raw(name))


def xml(name: str) -> ET.Element:
    return ET.fromstring(raw(name))


def totals(tree: ET.Element) -> tuple[dict[str, str], dict[str, str]]:
    cases = tree.find("OverallResultsTestCases")
    asserts = tree.find("OverallResultsAsserts")
    assert cases is not None and asserts is not None
    return cases.attrib, asserts.attrib


def verify_height_gpu(folder: str, host: str) -> dict[str, Any]:
    gpu = doc(f"parent/{folder}/result.json")
    assert gpu["passed"] is True and gpu["host_sha256"] == host and gpu["host_and_sources_unchanged"] is True
    assert len(gpu["cases"]) == 2
    summaries = []
    for index, case in enumerate(gpu["cases"]):
        report = case["report"]
        assert case["case_index"] == report["case_index"] == index
        assert case["passed"] is True and case["returncode"] == report["error"] == 0
        checks = report["checks"]
        assert len(checks) == len({r["check"] for r in checks}) == 34 and all(r["ok"] is True for r in checks)
        assert (
            report["failures"] == []
            and report["physical_prefix_returned"] is True
            and report["retained_on_failure"] is False
        )
        assert report["producer_complete"] is report["whole_output_released"] is report["reservation_refunded"] is False
        assert report["cleanup_continuation_active"] is False and report["cleanup_recording_error"] == 0
        assert report["device"]["driver"] == "metal" and report["device"]["renderer"] == "mobile"
        assert report["independent_ranges"] is True and report["pending_clear_before_range"] is False
        before = report["draws_before_range_issue"]
        complete = report["draws_at_native_complete"]
        returned = report["draws_at_physical_return"]
        if index == 0:
            assert report["callbacks"] == 1 and before == complete == returned and report["range_maintenance_calls"] > 0
        else:
            assert report["callbacks"] >= 3 and before < complete <= returned and report["range_maintenance_calls"] == 0
        stages = report["ranges"]
        assert set(stages) == {"closed", "poll", "completed", "first_returned", "both_returned", "physical_returned"}
        for stage, rows in stages.items():
            assert len(rows) == 2
            for i, row in enumerate(rows):
                assert all(row[k] == 0 for k in ERROR_FIELDS)
                assert row["recording_closed"] is row["has_recorded_effect"] is True
                assert row["graph_members"] == stages["closed"][i]["graph_members"] > 0
                if stage == "closed":
                    assert row["native_complete"] is row["observers_returned"] is False
                    assert row["issued_sources"] == row["sealed_members"] == 0
                elif stage != "poll":
                    assert row["mapping_closed"] is row["issue_closed"] is row["native_complete"] is True
                    assert row["graph_members"] == row["sealed_members"]
                    assert row["issued_sources"] == row["retained_sources"] == row["completed_sources"] > 0
                    expected = stage in {"both_returned", "physical_returned"} or (stage == "first_returned" and i == 0)
                    assert row["observers_returned"] is expected
        states = {
            "recorded": (1, 1, False),
            "native_complete": (1, 1, False),
            "observers_returned": (1, 1, False),
            "framebuffer_enqueued": (1, 2, False),
            "framebuffer_returned": (1, 3, False),
            "depth_enqueued": (2, 3, False),
            "returned": (3, 3, True),
        }
        assert set(report["physical"]) == set(states)
        identities = None
        for stage, expected in states.items():
            row = report["physical"][stage]
            assert row["observe_error"] == 0
            assert (
                row["depth"]["state"],
                row["framebuffer"]["state"],
                row["all_created_children_returned"],
            ) == expected
            current = tuple((row[k]["rid"], row[k]["serial"]) for k in ("depth", "framebuffer"))
            assert all(rid > 0 and serial > 0 for rid, serial in current)
            assert current[0][0] != current[1][0] and current[0][1] != current[1][1]
            assert identities is None or identities == current
            identities = current
        for stream in ("stdout", "stderr"):
            assert digest(raw(f"parent/{folder}/{index}/{stream}.log.gz")) == case[stream + "_sha256"]
        assert raw(f"parent/{folder}/{index}/stderr.log.gz") == b""
        summaries.append({
            "case": index,
            "checks": 34,
            "callbacks": report["callbacks"],
            "draws_before_complete_return": [before, complete, returned],
        })
    return {"host_sha256": host, "cases": summaries}


def verify() -> dict[str, Any]:
    manifest = doc("MANIFEST.json")
    actual = {str(p.relative_to(HERE)) for p in HERE.rglob("*") if p.is_file()}
    assert actual == set(manifest["files"]) | {"MANIFEST.json", "ARCHIVE_VERIFICATION.json"}
    for name, expected in manifest["files"].items():
        content = (HERE / name).read_bytes()
        assert expected == {"bytes": len(content), "sha256": digest(content)}, name
    entries = doc("ORIGINAL_FILE_MAP.json")["entries"]
    for entry in entries:
        stored = (HERE / entry["path"]).read_bytes()
        content = raw(entry["path"])
        assert len(stored) == entry["stored_bytes"] and digest(stored) == entry["stored_sha256"], entry["path"]
        assert len(content) == entry["original_bytes"] and digest(content) == entry["original_sha256"], entry["path"]
    pins = doc("parent/FINAL_SOURCE_PINS.json")
    snapshots = doc("SOURCE_SNAPSHOT_MAP.json")
    assert len(pins) == 11 and len(snapshots) == 23
    snapshot_pins = {}
    for entry in snapshots:
        content = raw(entry["path"])
        pin = {"bytes": len(content), "sha256": digest(content)}
        assert pin == {"bytes": entry["bytes"], "sha256": entry["sha256"]}
        snapshot_pins[entry["source"]] = pin
    assert all(snapshot_pins[name] == pin for name, pin in pins.items())
    for folder in ("third_gpu", "final_gpu"):
        for name, pin in doc(f"parent/{folder}/result.json")["sources_before"].items():
            assert snapshot_pins[name] == pin, name
    assert doc("parent/RESTORED_BUILD_RESULT.json")["exit"] == 0
    assert doc("parent/RESTORED_BUILD_RESULT.json")["host_sha256"] == FINAL_HOST
    cpu = []
    for label, case_count, assert_count, skipped, host in (
        ("THIRD_CPU", 11, 344, 2272, THIRD_HOST),
        ("FINAL_FOCUSED", 60, 6902, 2223, FINAL_HOST),
        ("FINAL_FULL_CPU", 2280, 585821, 3, FINAL_HOST),
    ):
        case_totals, assert_totals = totals(xml(f"parent/{label}.xml.gz"))
        assert case_totals == {"successes": str(case_count), "failures": "0", "skipped": str(skipped)}
        assert assert_totals == {"successes": str(assert_count), "failures": "0"}
        result = doc(f"parent/{label}_RESULT.json")
        assert result["exit"] == 0 and result["host_sha256"] == host
        cpu.append({
            "suite": label,
            "cases": case_count,
            "assertions": assert_count,
            "skipped": skipped,
            "host_sha256": host,
        })
    for audit in doc("parent/FINAL_CPU_XML_AUDIT.json"):
        name = audit["name"]
        assert digest(raw(f"parent/{name}.xml.gz")) == audit["raw_sha256"]
        cases, asserts = totals(xml(f"parent/{name}.xml.gz"))
        assert cases == audit["cases"] and asserts == audit["asserts"]
    final_gpu = verify_height_gpu("final_gpu", FINAL_HOST)
    verify_height_gpu("third_gpu", THIRD_HOST)
    quote = doc("parent/final_quote_gpu/result.json")
    assert (
        quote["passed"] is True and quote["host_sha256"] == FINAL_HOST and quote["host_and_sources_unchanged"] is True
    )
    assert len(quote["cases"]) == 2
    for index, case in enumerate(quote["cases"]):
        assert case["returncode"] == 0 and case["passed"] is True
        assert len(case["report"]["checks"]) == 29 and all(r["ok"] is True for r in case["report"]["checks"])
        assert case["report"]["quote"]["framebuffer_owner_reservation"] == {"knowledge": "Known", "bytes": 0}
        assert case["report"]["quote"]["framebuffer_driver_backing"] == {"knowledge": "UnknownNotExposed"}
        for stream in ("stdout", "stderr"):
            assert digest(raw(f"parent/final_quote_gpu/{index}/{stream}.log.gz")) == case[stream + "_sha256"]
        assert raw(f"parent/final_quote_gpu/{index}/stderr.log.gz") == b""
    hooks = doc("parent/FINAL_HOOKS.json")
    assert len(hooks) == 10 and all(row["exit"] == 0 for row in hooks)
    first, second = (doc(f"parent/{folder}/result.json") for folder in ("first_gpu", "second_gpu"))
    assert first["passed"] is second["passed"] is False
    assert first["cases"][0]["report"]["failures"] == ["correct_issue_path"]
    assert second["cases"][0]["passed"] is True and second["cases"][1]["passed"] is False
    assert b"Failed to resume native disposal frame advance" in raw("parent/second_gpu/1/stderr.log.gz")
    preimage = raw("parent/red/FINAL_RD.cpp.gz")
    assert digest(preimage) == pins["servers/rendering/rendering_device.cpp"]["sha256"]
    expected_failures = {
        "slot_buffer": 5,
        "ordinary_issue": 7,
        "bounded_issue": 7,
        "ordinary_retain": 1,
        "bounded_retain": 1,
    }
    audits = {r["name"]: r for r in doc("parent/red/XML_AUDIT.json")}
    reds = doc("parent/red/RESULTS.json")
    assert {r["name"] for r in reds} == set(expected_failures)
    for row in reds:
        name = row["name"]
        assert row == doc(f"parent/red/{name}/RESULT.json")
        assert row["build_exit"] == 0 and row["test_exit"] == 1 and row["final_sha256"] == digest(preimage)
        before, after = row["mutation_from"].encode(), row["mutation_to"].encode()
        positions = row["function_position"]
        if positions:
            assert len(positions) == 1
            pos = positions[0]
            assert preimage[pos : pos + len(before)] == before
            mutated = preimage[:pos] + after + preimage[pos + len(before) :]
        else:
            assert preimage.count(before) == 1
            mutated = preimage.replace(before, after)
        assert digest(mutated) == row["negative_sha256"]
        tree = xml(f"parent/red/{name}/CPU.xml.gz")
        cases, asserts = totals(tree)
        failures = [
            str(e.findtext("Original")).strip() for e in tree.iter("Expression") if e.attrib.get("success") == "false"
        ]
        assert cases["failures"] == "1" and int(asserts["failures"]) == expected_failures[name]
        assert (
            cases == audits[name]["cases"]
            and asserts == audits[name]["asserts"]
            and failures == audits[name]["failed_expressions"]
        )
        if name.endswith("retain"):
            assert failures == ["f.driver.polls == 0"]
    restoration = doc("parent/red/SOURCE_RESTORATION.json")
    assert restoration["restored"] is True
    assert restoration["sha256"] == digest(preimage)
    for label in ("CAMERA", "VIEWPORT", "HEIGHT"):
        neighbor = doc(f"parent/FINAL_{label}_RESULT.json")
        assert neighbor["exit"] == 0 and neighbor["host_sha256"] == FINAL_HOST
    pngs = [e for e in entries if e["category"] == "final_same_host_neighbor_png"]
    assert len(pngs) == 14
    for image in pngs:
        assert raw(image["path"]).startswith(b"\x89PNG\r\n\x1a\n")
    return {
        "status": "PASS_ARCHIVE_STANDALONE_D32_CLEAR",
        "payload_files": len(manifest["files"]),
        "copied_original_entries": len(entries),
        "source_snapshots": len(snapshots),
        "cpu": cpu,
        "final_height_gpu": final_gpu,
        "final_quote_gpu_checks": [29, 29],
        "controlled_red_assertion_failures": expected_failures,
        "hooks_passed": 10,
        "historical_failed_fixture_runs_preserved": True,
        "production_scene_public_abi_worker_hidden_height_draw_reader_refund_complete": False,
        "actual_finalize_lastref_child_run": False,
    }


if __name__ == "__main__":
    print(json.dumps(verify(), indent=2))
