#!/usr/bin/env python3
"""Run this standalone native Metal control only against an explicitly pinned host and its actual linkage."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time

RUNTIME = Path(__file__).resolve().parent

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def require(value, message):
    if not value:
        raise SystemExit(message)

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--binary", required=True, type=Path)
parser.add_argument("--binary-sha256", required=True)
parser.add_argument("--controlled-build", type=Path, help="Defaults to <binary>.controlled_build.json")
parser.add_argument("--controlled-build-sha256", required=True)
parser.add_argument("--core", type=Path, help="Only for an actual libgodot dependency shown by otool")
parser.add_argument("--core-sha256")
parser.add_argument("--action", choices=("generate", "run", "all"), default="all")
parser.add_argument("--case", choices=("positive", "cancel_prefix", "store_destination", "both"), default="both")
parser.add_argument("--include-legacy", action="store_true", help="Also run the current-header V1 ticket/upload native neighbors")
parser.add_argument("--legacy-only", action="store_true", help="Only generate/run those two V1 native neighbors")
args = parser.parse_args()
binary = args.binary.resolve()
controlled = args.controlled_build.resolve() if args.controlled_build else Path(str(binary) + ".controlled_build.json")
require(binary.is_file() and controlled.is_file(), "The pinned executable and its controlled-build metadata must exist.")
require(sha(binary) == args.binary_sha256 and sha(controlled) == args.controlled_build_sha256, "Executable/controlled-build identity differs from the supplied SHA-256 pins.")
metadata = json.loads(controlled.read_text())
require(metadata.get("status") == "available" and metadata.get("selection", {}).get("target_kind") == "Executable", "The native control needs available Executable controlled-build metadata.")
require(metadata.get("artifact", {}).get("sha256") == args.binary_sha256 and metadata["artifact"]["bytes"] == binary.stat().st_size, "Controlled-build metadata does not identify this exact executable.")
linkage = subprocess.check_output(["otool", "-L", str(binary)], text=True)
dependencies = [line.strip().split(" (compatibility version", 1)[0] for line in linkage.splitlines()[1:] if line.strip()]
godot_dependencies = [item for item in dependencies if "libgodot" in Path(item).name.lower()]
require(bool(args.core) == bool(args.core_sha256), "Supply --core and --core-sha256 together only for a loaded dependency.")
core = None
host = {"binary": str(binary), "binary_sha256": args.binary_sha256, "controlled_build": str(controlled), "controlled_build_sha256": args.controlled_build_sha256, "controlled_build_selection": metadata["selection"], "linkage": dependencies, "core_model": "monolithic"}
if godot_dependencies:
    require(args.core, "This executable links libgodot; explicitly pin that actual dependency with --core/--core-sha256.")
    core = args.core.resolve()
    require(core.is_file() and sha(core) == args.core_sha256, "The actual linked core differs from its supplied SHA-256 pin.")
    loads = subprocess.check_output(["otool", "-l", str(binary)], text=True)
    rpaths = re.findall(r"cmd LC_RPATH\n\s+cmdsize \d+\n\s+path (.+?) \(offset", loads)
    def expand(path):
        return path.replace("@executable_path", str(binary.parent)).replace("@loader_path", str(binary.parent))
    resolved = []
    for dependency in godot_dependencies:
        candidates = [Path(expand(rpath)) / dependency[len("@rpath/"):] for rpath in rpaths] if dependency.startswith("@rpath/") else [Path(expand(dependency))]
        existing = [item.resolve() for item in candidates if item.is_file()]
        require(existing, f"Cannot resolve actual executable dependency {dependency}; supply a correctly linked host.")
        resolved.append(existing[0])
    require(core in resolved, "The supplied core is not an actual libgodot dependency resolved from this executable's linkage.")
    host.update({"core_model": "linked", "loaded_core": str(core), "loaded_core_sha256": args.core_sha256})
else:
    require(not args.core, "This host has monolithic core; an unlinked libgodot DSO cannot be pinned as loaded host evidence.")
output = RUNTIME / "results" / time.strftime("%Y%m%d-%H%M%S")
output.mkdir(parents=True, exist_ok=False)
source_hashes = {name: sha(RUNTIME / "containers" / name) for name in ("capture.glsl", "body.glsl", "generate.gd")}
build = json.loads((RUNTIME / "BUILD.json").read_text())
require(build["header_sha256"] == sha(RUNTIME / "gdextension_interface_v2.pinned.h"), "ABI header changed since the small dylib build; rerun prepare.py.")
require(build["source_sha256"] == sha(RUNTIME / "v2_probe.cpp"), "Probe source changed since the small dylib build; rerun prepare.py.")
for entry in build["builds"]:
    require(entry["rc"] == 0 and entry["sha256"] == sha(RUNTIME / f'v2_{entry["case"]}.dylib'), "Probe DSO identity differs from BUILD.json.")
if args.case == "store_destination":
    store_build = json.loads((RUNTIME / "STORE_BUILD.json").read_text())
    require(store_build["header_sha256"] == sha(RUNTIME / "gdextension_interface_v2.pinned.h") and store_build["source_sha256"] == sha(RUNTIME / "v2_store_destination_probe.cpp") and store_build["rc"] == 0 and store_build["sha256"] == sha(RUNTIME / "v2_store_destination.dylib"), "Store destination source/header/DSO changed since its build.")
env = {**os.environ, "ANGE_MACOS_BACKGROUND_RENDER": "1"}
# Use the executable's ordinary linked core resolution, not an inherited test override.
for name in ("DYLD_LIBRARY_PATH", "DYLD_FALLBACK_LIBRARY_PATH", "DYLD_INSERT_LIBRARIES"):
    env.pop(name, None)
env.update({"MULTI_V2_CAPTURE_CONTAINER": str(RUNTIME / "containers/capture.bin"), "MULTI_V2_BODY_CONTAINER": str(RUNTIME / "containers/body.bin"), "MULTI_V2_CAPTURE_SOURCE": str(RUNTIME / "containers/capture.glsl"), "MULTI_V2_BODY_SOURCE": str(RUNTIME / "containers/body.glsl")})
receipt = {"scope": "Godot original SceneReady/guard/QRT/native/physical control; not an ANGE product request, Method verification, or owner acceptance", "host": host, "build": build, "source_hashes": source_hashes, "runs": []}

def save():
    (output / "results.json").write_text(json.dumps(receipt, indent=2) + "\n")

def launch(case, project, script):
    command = [str(binary), "--path", str(project), "--rendering-driver", "metal", "--rendering-method", "mobile", "--render-thread", "safe", "--script", script]
    log = output / f"{case}.log"
    print(f"running {case}: {log}", flush=True)
    with log.open("w") as stream:
        try:
            result = subprocess.run(command, env=env, stdout=stream, stderr=subprocess.STDOUT, timeout=90)
            code = result.returncode
        except subprocess.TimeoutExpired:
            code = "timeout"
    text = log.read_text(errors="replace")
    checks = {"exit_zero": code == 0, "metal": "Metal" in text, "clean": not re.search(r"ERROR:|leaked at exit|Leaked instance|CrashHandler|SIGSEGV|MULTI_V2_ASSERT|FIXTURE_FATAL", text)}
    entry = {"case": case, "command": command, "returncode": code, "log": str(log), "checks": checks}
    receipt["runs"].append(entry)
    return text, checks

if not args.legacy_only and args.action in ("generate", "all"):
    for kind in ("capture", "body"):
        (RUNTIME / "containers" / f"{kind}.bin").unlink(missing_ok=True)
    text, checks = launch("containers", RUNTIME / "containers", "generate.gd")
    checks["both_generated"] = all(re.search(rf"MULTI_V2_CONTAINER kind={kind} bytes=[1-9][0-9]*", text) and (RUNTIME / "containers" / f"{kind}.bin").is_file() for kind in ("capture", "body"))
    save()
    require(all(checks.values()), f"Container generation failed; inspect {output}.")
    generated = {"host": host, "source_hashes": source_hashes, "containers": {kind: sha(RUNTIME / "containers" / f"{kind}.bin") for kind in ("capture", "body")}, "log": receipt["runs"][-1]["log"]}
    (RUNTIME / "containers/GENERATED.json").write_text(json.dumps(generated, indent=2) + "\n")

if not args.legacy_only and args.action in ("run", "all"):
    generated = json.loads((RUNTIME / "containers/GENERATED.json").read_text())
    require(generated["host"] == host and generated["source_hashes"] == source_hashes, "Containers were generated for a different host/linkage or GLSL source; generate them again with the current pins.")
    require(all(generated["containers"][kind] == sha(RUNTIME / "containers" / f"{kind}.bin") for kind in ("capture", "body")), "Container content differs from its generation receipt.")
    for case in (("store_destination",) if args.case == "store_destination" else ("positive", "cancel_prefix")):
        if args.case not in ("both", case):
            continue
        text, checks = launch(case, RUNTIME / case, "probe.gd")
        kinds = {int(n) for n in re.findall(r"QRT_NATIVE_RETURN kind=(\d+)", text)}
        checks["original_kinds"] = kinds == ({1, 2, 3, 4, 5, 8, 10, 11, 15, 16} if case != "cancel_prefix" else {1, 2, 3, 4, 5, 8, 11, 15, 16})
        checks["typed_sets_created"] = len(re.findall(r"QRT_NATIVE_RETURN kind=15 .* error=0 reply_ack=1", text)) == 4
        checks["typed_sets_retired"] = len(re.findall(r"QRT_NATIVE_RETURN kind=16 .* error=0 reply_ack=1", text)) == 4
        checks["loop"] = text.count("MULTI_V2_LOOP_INITIALIZED\n") == 1
        checks["unload"] = text.count("PHS1_DSO_UNLOAD tag=one\n") == 1
        unload = text.find("PHS1_DSO_UNLOAD tag=one\n")
        for level in (2, 3):
            user = text.find(f"PHS1_USER_BODY tag=one level={level}\n")
            binding = text.find(f"PHS1_BINDING_BODY tag=one level={level}\n")
            checks[f"terminal_{level}"] = 0 <= user < binding < unload
        expected = (("mode=3" if case == "store_destination" else "mode=1"), "readbacks=3 actual_async_distinct=1 exact_bytes=44 guards=1") if case != "cancel_prefix" else ("mode=2", "readbacks=0 actual_async_distinct=0 exact_bytes=0 guards=0")
        if case == "store_destination":
            checks["allocator_issuer_negative"] = "STORE_DESTINATION_NEGATIVE owner_kind=E24 e05_backing_nonzero=1 error=4 ticket=0 refused=1" in text
            checks["actual_store_e05_record"] = "STORE_DESTINATION_RECORD range_role=2 upload_role=18 owner_kind=1 e05_backing_nonzero=1 native_error=0 reason=0" in text
        markers = re.findall(r"MULTI_V2_NATIVE [^\n]+", text)
        checks["same_ticket_native_result"] = len(markers) == 1 and all(word in markers[0] for word in (*expected, "actual_main=1 same_ticket=1 set_count=2 upload_plans=4", "native_complete=1", "physical_free=1 ack=1 ange_product_acceptance=0"))
        receipt["runs"][-1]["kinds"] = sorted(kinds)
        save()
        print(f"{case}: failed={[name for name, passed in checks.items() if not passed]}", flush=True)
        require(all(checks.values()), f"Native control failed; inspect {output}.")

if args.include_legacy or args.legacy_only:
    from legacy.control import run_legacy
    receipt["legacy_build"] = json.loads((RUNTIME / "legacy/BUILD.json").read_text())
    run_legacy(RUNTIME, host, env, args.action, launch, save, require)

receipt["host_unchanged"] = sha(binary) == args.binary_sha256 and sha(controlled) == args.controlled_build_sha256 and (core is None or sha(core) == args.core_sha256)
save()
require(receipt["host_unchanged"], "Executable, controlled-build metadata or actual linked core changed during the native control.")
print(f"receipt: {output / 'results.json'}", flush=True)
