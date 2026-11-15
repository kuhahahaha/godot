// The sealed Indexed144 packet retains this exact vertex_capacity*4 backing.
// Binding 1 shares the profile namespace with Instance192 previous records;
// those profiles are mutually exclusive. Float144 vertex bytes are unchanged.
layout(set = 2, binding = 1, std430) restrict readonly buffer Indexed144HistoryValidity {
	uint data[];
}
indexed144_history;

bool indexed144_history_valid(int vertex_index) {
	if (vertex_index < 0 || uint(vertex_index) >= uint(indexed144_history.data.length())) {
		return false;
	}
	return indexed144_history.data[vertex_index] == 1u;
}
