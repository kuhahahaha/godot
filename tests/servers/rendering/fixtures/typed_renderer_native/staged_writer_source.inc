// Fixture-only source; compiled by the actual main RD before original qualification.
static constexpr const char *STAGED_WRITER_SHADER = R"GLSL(#version 450

// Fixture-only single-record writer. Public qualification/import and the
// original output plan retain all resource and recording authority.
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0, std430) readonly buffer Source {
	uint words[8];
} source;
layout(set = 0, binding = 1, std430) readonly buffer CameraControl {
	uint words[3];
} camera_control;
layout(set = 0, binding = 2, std430) writeonly buffer Current {
	uint words[48];
} current;
layout(set = 0, binding = 3, std430) writeonly buffer Previous {
	uint words[48];
} previous;
layout(set = 0, binding = 4, std430) writeonly buffer OutputMetadata {
	uint words[8];
} output_metadata;

layout(push_constant, std430) uniform Host {
	vec4 affine[3];
	vec4 inverse_affine[3];
} host;

void write_both(uint word, uint value) {
	current.words[word] = value;
	previous.words[word] = value;
}

void main() {
	if (any(notEqual(gl_GlobalInvocationID, uvec3(0u)))) {
		return;
	}
	for (uint word = 0u; word < 48u; ++word) {
		write_both(word, 0u);
	}
	for (uint row = 0u; row < 3u; ++row) {
		for (uint column = 0u; column < 4u; ++column) {
			write_both(row * 4u + column, floatBitsToUint(host.affine[row][column]));
			write_both(24u + row * 4u + column, floatBitsToUint(host.inverse_affine[row][column]));
		}
		for (uint column = 0u; column < 3u; ++column) {
			// Original inverse's transposed linear part, not the model's scale.
			write_both(12u + row * 4u + column, floatBitsToUint(host.inverse_affine[column][row]));
		}
	}
	const vec4 color = vec4(0.75, 0.25, 0.125, 1.0);
	for (uint component = 0u; component < 4u; ++component) {
		write_both(36u + component, floatBitsToUint(color[component]));
	}
	// The single fixture entity is stable across both records. History remains
	// disabled, and padding words 15/19/23/47 and custom words remain exact zero.
	write_both(44u, 1u);
	write_both(45u, 0u);
	write_both(46u, 1u); // VALID; no history, reflection, or producer cull flags.

	output_metadata.words[0] = source.words[0];
	output_metadata.words[1] = 0u;
	output_metadata.words[2] = 0u;
	output_metadata.words[3] = source.words[1] | source.words[2] | source.words[3] | source.words[4];
	output_metadata.words[4] = 1u; // Actual immutable fixture capacity.
	output_metadata.words[5] = 1u; // Metadata codec revision.
	// Codec control3=[needs_inverse,present low,present high] is filled by the
	// original Core camera invocation, rather than a CPU fixture sequence.
	output_metadata.words[6] = camera_control.words[1];
	output_metadata.words[7] = camera_control.words[2];
}
)GLSL";
