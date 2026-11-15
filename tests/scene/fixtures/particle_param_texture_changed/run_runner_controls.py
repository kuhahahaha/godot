#!/usr/bin/env python3
"""Run the indexed parameter texture source harness and focused mutations."""
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parents[3] / "scene/resources/particle_process_material.cpp"
RUN_SOURCE = HERE / "run_source.py"


def run(source: Path) -> int:
    return subprocess.run(["python3", str(RUN_SOURCE), "--source", str(source)], timeout=10).returncode


def main() -> int:
    original = SOURCE.read_text()
    cases = {
        "baseline": (original, 0),
        "non_deferred": (original.replace("CONNECT_DEFERRED", "0", 1), 1),
        "missing_disconnect": (original.replace("tex_parameters[p_param]->disconnect_changed(changed);", "(void)changed;", 1), 1),
        "missing_callback_refresh": (original.replace(
            "RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->angular_velocity_texture, tex_rid);",
            "(void)tex_rid;", 1), 1),
        "shared_method_identity": (original.replace(
            "_param_texture_changed_slot<PARAM_DAMPING>",
            "_param_texture_changed_slot<PARAM_ANGULAR_VELOCITY>", 1), 1),
        "compile_error_is_not_behavioral_red": (original.replace(
            "_get_param_texture_changed(p_param);", "missing_factory(p_param);", 1), 2),
    }
    failures = 0
    with tempfile.TemporaryDirectory(prefix="particle-param-texture-controls-") as tmp:
        root = Path(tmp)
        for label, (source, expected) in cases.items():
            path = root / f"{label}.cpp"
            path.write_text(source)
            actual = run(path)
            if actual != expected:
                print(f"FAIL {label}: expected {expected}, got {actual}")
                failures += 1
    print(f"param-texture runner controls: {len(cases) - failures}/{len(cases)} passed")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
