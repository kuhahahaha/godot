#!/usr/bin/env python3
"""The converter must reject invalid normal texels without invoking CPU setters."""
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[4]
result = subprocess.run(
    [str(root / "bin/godot.macos.editor.arm64"), "--headless", "--path", str(root),
     "--script", str(Path(__file__).with_name("run.gd"))],
    capture_output=True, text=True,
)
output = result.stdout + result.stderr
assert result.returncode == 0, output
assert "Particle emission normals must be finite" not in output, output
print("invalid normal textures: 2D/3D valid positions retained without setter errors")
