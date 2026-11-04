/**************************************************************************/
/*  external_output_config.h                                              */
/**************************************************************************/

#ifndef EXTERNAL_OUTPUT_CONFIG_H
#define EXTERNAL_OUTPUT_CONFIG_H

#include "core/error/error_list.h"
#include "core/math/aabb.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

namespace RendererRD {

// CPU syntax and size projection only. Parsing does not validate mesh/material
// ownership, qualify pipelines or mint a source/publication capability.
struct ExternalOutputConfig {
	enum Profile : uint32_t {
		INSTANCE_192 = 1,
		INDEXED_144 = 2,
		SCENE_ONLY = 3,
	};
	enum Space : uint32_t {
		WORLD = 1,
		LOCAL = 2,
	};
	enum Motion : uint32_t {
		DISABLED = 0,
		REQUIRED = 1,
	};

	// External schema1 wire domain. Never cast a renderer list/pass enum here.
	enum Pass : uint32_t {
		PASS_OPAQUE = 0x01,
		PASS_DEPTH = 0x02,
		PASS_SHADOW = 0x04,
		PASS_TRANSPARENT = 0x08,
		PASS_PROBE = 0x10,
		PASS_KNOWN_MASK = 0x1f,
	};

	Profile profile = SCENE_ONLY;
	uint32_t capacity = 0;
	uint32_t max_views = 0;
	uint32_t max_inflight = 0;
	Space space = WORLD;
	uint32_t pass_mask = 0;
	Motion motion = DISABLED;
	Vector<RID> surfaces;
	Vector<RID> surface_materials;
	AABB bounds;
	AABB geometry_bounds;
	bool query_enabled = false;
	AABB query_bounds;
	PackedByteArray producer_tag;
	PackedByteArray device_tag;
	PackedByteArray layout_digest;
	PackedByteArray mapping_digest;
	PackedByteArray numeric_profile_digest;

	static Error parse(const Dictionary &p_config, ExternalOutputConfig &r_config, StringName &r_detail);

	// A size accepted here still needs the actual RD/device limit check.
	// Outputs remain unchanged on failure, including u64 multiplication overflow.
	static Error checked_buffer_bytes(uint64_t p_count, uint64_t p_stride, uint64_t p_device_limit, uint32_t &r_bytes);
	static Error decode_u64(const Variant &p_value, uint64_t &r_value);
};

} // namespace RendererRD

#endif // EXTERNAL_OUTPUT_CONFIG_H
