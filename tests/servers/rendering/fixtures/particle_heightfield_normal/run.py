#!/usr/bin/env python3
"""Execute the production heightfield collision branch on a local GPU device.

This exercises the branch's numeric GLSL result, not the whole particle runtime.
"""

import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[5]
HERE = Path(__file__).resolve().parent
SHADER = ROOT / "servers/rendering/renderer_rd/shaders/particles.glsl"
source = SHADER.read_text()
start = source.index("case COLLIDER_TYPE_HEIGHT_FIELD: {")
end = source.index("} break;", start)
branch = source[start:end + len("} break;")]
compute = """#version 450
layout(local_size_x = 4) in;
#define COLLIDER_TYPE_HEIGHT_FIELD 3
struct Collider { mat4 transform; vec3 extents; uint type; };
struct Frame { Collider colliders[1]; };
layout(set = 0, binding = 0, std430) buffer Results { vec4 values[4]; } results;
layout(set = 0, binding = 1) uniform sampler SAMPLER_LINEAR_CLAMP;
layout(set = 0, binding = 2) uniform texture2D height_field_texture;
void main() {
    uint index = gl_GlobalInvocationID.x;
    if (index >= 4u) { return; }
    Frame FRAME;
    FRAME.colliders[0].type = COLLIDER_TYPE_HEIGHT_FIELD;
    FRAME.colliders[0].extents = index == 2u ? vec3(2.0, 3.0, 4.0) : vec3(1.0);
    FRAME.colliders[0].transform = index == 1u ?
        mat4(vec4(0, 1, 0, 0), vec4(-1, 0, 0, 0), vec4(0, 0, 1, 0), vec4(0, 0, 0, 1)) : mat4(1.0);
    vec3 rel_vec = mat3(FRAME.colliders[0].transform) * vec3(0.0, index == 3u ? -0.05 : -0.1, index == 3u ? 0.5 : index == 2u ? -3.5 : -0.75);
    vec3 local_pos = rel_vec * mat3(FRAME.colliders[0].transform);
    float particle_size = 0.1;
    vec3 normal = vec3(0.0);
    float depth = 0.0;
    bool col = false;
    const float EPSILON = 0.001;
    for (uint i = 0u; i < 1u; i++) {
        switch (FRAME.colliders[i].type) {
""" + branch + """
        }
    }
    results.values[index] = col ? vec4(normal, depth) : vec4(99.0);
}
"""

with tempfile.TemporaryDirectory(prefix="ange-heightfield-normal-") as tmp:
    shader_path = Path(tmp) / "compute.glsl"
    shader_path.write_text(compute)
    env = os.environ.copy()
    env["ANGE_HEIGHTFIELD_TEST_SHADER"] = str(shader_path)
    command = [str(ROOT / "bin/godot.macos.editor.arm64"), "--display-driver", "macos", "--rendering-driver", "metal", "--rendering-method", "mobile", "--path", str(HERE), "--script", str(HERE / "probe.gd")]
    result = subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
    print(result.stdout)
    if "Metal 4.0 - Forward Mobile" not in result.stdout:
        sys.exit("Expected local Metal/Mobile GPU evidence")
    if "HEIGHTFIELD_NORMAL_GPU 3" not in result.stdout:
        sys.exit("Missing slope sample")
    sys.exit(result.returncode)
