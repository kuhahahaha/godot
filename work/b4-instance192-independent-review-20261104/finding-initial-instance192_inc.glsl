// Private Instance192 raster binding. G cannot publish this set directly: K
// retains the guarded current/previous records and exact packet read lease.
#ifdef USE_DOUBLE_PRECISION
#error Instance192 raster requires a separately qualified double/origin path
#endif
#ifdef USE_INDEXED144_VERTEX_INPUT
#error Instance192 and Indexed144 are distinct raster profiles
#endif
#ifdef USE_PARTICLE_TRAILS
#error Instance192 does not admit native particle trails
#endif

layout(set = 2, binding = 1, std430) restrict readonly buffer Instance192Previous {
	vec4 data[];
} instance192_previous;

// Match RD PolygonCullMode (disabled=0/front=1/back=2). Pipeline hardware cull
// is disabled; this is the original material/pass cull request, never a bucket.
layout(constant_id = 4) const uint instance192_cull_mode = 0u;

struct Instance192Record {
	mat4 model;
	mat3 normal;
	mat4 inverse_model;
	vec4 color;
	vec4 custom;
	uint flags;
};

uint instance192_flags(uint index) {
	return floatBitsToUint(transforms.data[index * 12u + 11u].z);
}

bool instance192_has_history(uint flags) {
	return (flags & 2u) != 0u;
}

vec4 instance192_row(uint index, uint row, bool previous) {
	uint offset = index * 12u + row;
	if (previous) {
		return instance192_previous.data[offset];
	}
	return transforms.data[offset];
}

Instance192Record instance192_read(uint index, bool previous) {
	// Invalid history uses current geometry and never fetches stale previous.
	previous = previous && instance192_has_history(instance192_flags(index));
	Instance192Record record;
	record.model = transpose(mat4(instance192_row(index, 0u, previous), instance192_row(index, 1u, previous), instance192_row(index, 2u, previous), vec4(0.0, 0.0, 0.0, 1.0)));
	record.normal = transpose(mat3(instance192_row(index, 3u, previous).xyz, instance192_row(index, 4u, previous).xyz, instance192_row(index, 5u, previous).xyz));
	record.inverse_model = transpose(mat4(instance192_row(index, 6u, previous), instance192_row(index, 7u, previous), instance192_row(index, 8u, previous), vec4(0.0, 0.0, 0.0, 1.0)));
	record.color = instance192_row(index, 9u, previous);
	record.custom = instance192_row(index, 10u, previous);
	record.flags = floatBitsToUint(instance192_row(index, 11u, previous).z);
	return record;
}

bool instance192_front_facing(bool raster_front, uint flags) {
	return raster_front != ((flags & 4u) != 0u);
}

bool instance192_discard_face(bool logical_front) {
	return (instance192_cull_mode == 1u && logical_front) || (instance192_cull_mode == 2u && !logical_front);
}
