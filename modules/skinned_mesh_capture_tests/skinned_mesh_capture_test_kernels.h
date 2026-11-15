/**************************************************************************/
/*  skinned_mesh_capture_test_kernels.h                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include <cstdint>

struct SkinnedMeshCaptureTestKernelInput {
	uint32_t value = 0;
};

inline uint32_t skinned_mesh_capture_test_kernel_identity(SkinnedMeshCaptureTestKernelInput p_input) {
	return p_input.value;
}

// The only two compute sources permitted by the fixed native fixture.
// std430 input is exactly 228 bytes: 9 position floats, 24 row-major affine
// matrix floats, 12 uint bind slots, 12 uint UNORM16 weights. No world transform
// is applied; the output is local-space positions, matching native skin output.
inline const char *skinned_mesh_capture_test_public_deformation_shader_source() {
	return R"GLSL(#version 450
layout(local_size_x = 3, local_size_y = 1, local_size_z = 1) in;
layout(set = 0, binding = 0, std430) readonly buffer Source {
    float positions[9];
    float matrices[24];
    uint bind_slots[12];
    uint weights[12];
} source_data;
layout(set = 0, binding = 1, std430) writeonly buffer Snapshot {
    float values[16];
} snapshot;
void main() {
    uint vertex = gl_GlobalInvocationID.x;
    if (vertex >= 3u) { return; }
    vec4 point = vec4(source_data.positions[vertex * 3u],
                      source_data.positions[vertex * 3u + 1u],
                      source_data.positions[vertex * 3u + 2u], 1.0);
    vec3 position = vec3(0.0);
    for (uint lane = 0u; lane < 4u; lane++) {
        uint input_index = vertex * 4u + lane;
        uint base = source_data.bind_slots[input_index] * 12u;
        vec3 transformed;
        for (uint row = 0u; row < 3u; row++) {
            uint offset = base + row * 4u;
            vec4 matrix_row = vec4(source_data.matrices[offset],
                                    source_data.matrices[offset + 1u],
                                    source_data.matrices[offset + 2u],
                                    source_data.matrices[offset + 3u]);
            transformed[row] = dot(matrix_row, point);
        }
        position += transformed * (float(source_data.weights[input_index]) / 65535.0);
    }
    snapshot.values[vertex * 3u] = position.x;
    snapshot.values[vertex * 3u + 1u] = position.y;
    snapshot.values[vertex * 3u + 2u] = position.z;
}
)GLSL";
}

// Copy only the nine payload words; both independent 64-byte buffers retain
// their seven initialized guard words. Float interpretation would destroy the
// bitwise transport check for NaN sentinels and signed zero.
inline const char *skinned_mesh_capture_test_transport_shader_source() {
	return R"GLSL(#version 450
layout(local_size_x = 9, local_size_y = 1, local_size_z = 1) in;
layout(set = 0, binding = 0, std430) readonly buffer Snapshot {
    uint words[16];
} snapshot;
layout(set = 0, binding = 1, std430) writeonly buffer Consumer {
    uint words[16];
} consumer;
void main() {
    uint word = gl_GlobalInvocationID.x;
    if (word < 9u) { consumer.words[word] = snapshot.words[word]; }
}
)GLSL";
}
