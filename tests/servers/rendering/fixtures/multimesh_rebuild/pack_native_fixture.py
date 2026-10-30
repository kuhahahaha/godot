"""Package the standalone SceneTree fixture as a normal release main scene."""

import hashlib
import json
import shutil
import subprocess
from pathlib import Path


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def prepare_packed_scene(project, script, binary, editor_receipt_path, out):
    editor_receipt_path = editor_receipt_path.resolve()
    editor_receipt = json.loads(editor_receipt_path.read_text())
    editor = Path(editor_receipt["binary"])
    if (
        editor_receipt["build_rc"] != 0
        or not editor_receipt["source_stable"]
        or editor_receipt["source_before"] != editor_receipt["source_after"]
        or editor_receipt["git_status_before"]
        or editor_receipt["git_status_after"]
        or "target=editor" not in editor_receipt["command"]
        or digest(editor) != editor_receipt["binary_sha256"]
    ):
        raise ValueError("packing requires a matching successful clean Editor receipt")
    original = script.read_text()
    replacements = {
        "extends SceneTree\n": "extends Node\n",
        "func _initialize()": "func _ready()",
        "\troot.add_child(": "\tget_tree().root.add_child(",
        "\t\tquit(": "\t\tget_tree().quit(",
        "\tquit(": "\tget_tree().quit(",
        "await process_frame": "await get_tree().process_frame",
    }
    adapted = original
    for old, new in replacements.items():
        if adapted.count(old) != 1:
            raise ValueError("fixture does not match the explicit scene entry adapter: " + old)
        adapted = adapted.replace(old, new, 1)
    script.write_text(adapted)
    project_file = project / "project.godot"
    settings = project_file.read_text()
    if settings.count("[application]\n") != 1:
        raise ValueError("expected one project application section")
    project_file.write_text(settings.replace("[application]\n", '[application]\nrun/main_scene="res://main.tscn"\n', 1))
    scene = project / "main.tscn"
    scene.write_text(
        '[gd_scene load_steps=2 format=3]\n[ext_resource type="Script" path="res://' + script.name + '" id="1"]\n'
        '[node name="NativeFixture" type="Node"]\nscript = ExtResource("1")\n'
    )
    app = out / "app"
    app.mkdir()
    executable = app / "godot"
    shutil.copy2(binary, executable)
    if digest(executable) != digest(binary):
        raise ValueError("packaged executable differs from the recorded template")
    pack = app / "godot.pck"
    packed_files = ["project.godot", script.name, "main.tscn", ".godot/global_script_class_cache.cfg"]
    packer = out / "pack.gd"
    packer.write_text(
        "extends SceneTree\nfunc _initialize():\n\tvar pack = PCKPacker.new()\n\tvar error = pack.pck_start("
        + json.dumps(str(pack))
        + ")\n\tif error != OK: quit(1); return\n\tfor path in "
        + json.dumps(packed_files)
        + ':\n\t\terror = pack.add_file("res://" + path, '
        + json.dumps(str(project))
        + ' + "/" + path)\n\t\tif error != OK: quit(2); return\n\terror = pack.flush()\n\tquit(0 if error == OK else 3)\n'
    )
    sources = [
        editor,
        editor_receipt_path,
        *[project / name for name in packed_files],
        packer,
        Path(__file__).resolve(),
        binary,
        executable,
    ]
    before = {str(path): digest(path) for path in sources}
    cmd = [str(editor), "--headless", "--path", str(project), "--script", str(packer)]
    with (out / "PACK.log").open("w") as log:
        try:
            run = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT, timeout=90)
            rc = run.returncode
        except subprocess.TimeoutExpired:
            rc = "timeout"
    after = {str(path): digest(path) for path in sources}
    log_text = (out / "PACK.log").read_text()
    passed = (
        rc == 0
        and before == after
        and pack.is_file()
        and not any(marker in log_text for marker in ["ERROR:", "WARNING:", "leaked", "Leaked"])
    )
    record = {
        "command": cmd,
        "rc": rc,
        "editor_commit": editor_receipt["commit"],
        "sources_before": before,
        "sources_after": after,
        "original_script_sha256": hashlib.sha256(original.encode()).hexdigest(),
        "entry_replacements": replacements,
        "pck": str(pack),
        "pck_sha256": digest(pack) if pack.is_file() else None,
        "pass": passed,
        "scope": "ordinary PCK main-scene entry; body uses the same public rendering APIs",
    }
    receipt = out / "PACKAGE.json"
    receipt.write_text(json.dumps(record, indent=2) + "\n")
    if not passed:
        raise RuntimeError("PCK packaging failed; see PACK.log and PACKAGE.json")
    return executable, [*sources, pack, receipt], record
