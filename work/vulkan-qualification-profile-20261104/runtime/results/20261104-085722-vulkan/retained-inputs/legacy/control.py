"""V1 neighbor stages, invoked through the parent runner's actual host/linkage pins."""
import hashlib
import json
from pathlib import Path
import re

def run_legacy(runtime, host, env, action, launch, save, require):
    here = runtime / "legacy"
    def sha(path):
        return hashlib.sha256(path.read_bytes()).hexdigest()
    build = json.loads((here / "BUILD.json").read_text())
    require(build["header_sha256"] == sha(runtime / "gdextension_interface_v2.pinned.h"), "V1 neighbor header changed; run legacy/prepare.py again.")
    for entry in build["builds"]:
        require(entry["rc"] == 0 and entry["sha256"] == sha(here / f'{entry["case"]}_probe.dylib') and entry["source_sha256"] == sha(here / f'{entry["case"]}_probe.cpp'), "V1 neighbor source or DSO changed since its current-header build.")
    sources = {name: sha(here / "ticket_containers" / name) for name in ("generate.gd", "project.godot")}
    if action in ("generate", "all"):
        for kind in ("capture", "body"):
            (here / "ticket_containers" / f"{kind}.bin").unlink(missing_ok=True)
        text, checks = launch("legacy-containers", here / "ticket_containers", "generate.gd")
        checks["both_generated"] = all(re.search(rf"QB1_TICKET_CONTAINER kind={kind} bytes=[1-9][0-9]*", text) and (here / "ticket_containers" / f"{kind}.bin").is_file() for kind in ("capture", "body"))
        save()
        require(all(checks.values()), "V1 neighbor container generation failed.")
        generated = {"host": host, "source_hashes": sources, "containers": {kind: sha(here / "ticket_containers" / f"{kind}.bin") for kind in ("capture", "body")}}
        (here / "ticket_containers/GENERATED.json").write_text(json.dumps(generated, indent=2) + "\n")
    if action in ("run", "all"):
        generated = json.loads((here / "ticket_containers/GENERATED.json").read_text())
        require(generated["host"] == host and generated["source_hashes"] == sources and all(generated["containers"][kind] == sha(here / "ticket_containers" / f"{kind}.bin") for kind in ("capture", "body")), "V1 neighbor containers differ from this exact host/source generation receipt.")
        env.update({"QB1_CAPTURE_CONTAINER": str(here / "ticket_containers/capture.bin"), "QB1_BODY_CONTAINER": str(here / "ticket_containers/body.bin")})
        for case in ("qrt_ticket", "qrt_upload"):
            text, checks = launch(f"legacy-{case}", here / case, "probe.gd")
            kinds = {int(value) for value in re.findall(r"QRT_NATIVE_RETURN kind=(\d+)", text)}
            checks["original_v1_kinds"] = kinds == ({1, 2, 3, 4, 5, 6} if case == "qrt_ticket" else {1, 2, 3, 4, 5, 7, 8, 9})
            checks["loop"] = text.count("QB1_TICKET_LOOP_INITIALIZED\n") == 1
            checks["unload"] = text.count("PHS1_DSO_UNLOAD tag=one\n") == 1
            unload = text.find("PHS1_DSO_UNLOAD tag=one\n")
            for level in (2, 3):
                user = text.find(f"PHS1_USER_BODY tag=one level={level}\n")
                binding = text.find(f"PHS1_BINDING_BODY tag=one level={level}\n")
                checks[f"terminal_{level}"] = 0 <= user < binding < unload
            if case == "qrt_ticket":
                checks["exact_original_result"] = bool(re.search(r"QB1_TICKET_NATIVE_COMPLETE actual_main=1 same_ticket=1 facts=\d+ readback=7,8,9,10 exact_bytes=16 native_sources=1 physical_free=1 ack=1", text))
            else:
                checks["original_uploaded_ticket"] = bool(re.search(r"U1_TICKET_NATIVE mode=1 actual_main=1 same_ticket=1 facts=\d+ prefix_settled=1 physical_free=1 ack=1", text))
            save()
            print(f"legacy/{case}: failed={[name for name, passed in checks.items() if not passed]}", flush=True)
            require(all(checks.values()), f"V1 native neighbor {case} failed.")
