/**************************************************************************/
/*  external_output_config.h                                              */
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

#include "core/error/error_list.h"
#include "core/math/aabb.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"
#include "servers/rendering/rendering_server_enums.h"

namespace RendererRD {

// CPU syntax and size projection only. Parsing does not validate mesh/material
// ownership, qualify pipelines or mint a source/publication capability.
struct ExternalOutputConfig {
	// Configured mesh RID count and the actual flattened native surface table
	// are different bounds. Every original MeshStorage surface owner is finite.
	static constexpr uint32_t MAX_MESH_RIDS = 8;
	static constexpr uint32_t MAX_FLATTENED_SURFACES = MAX_MESH_RIDS * RenderingServerEnums::MAX_MESH_SURFACES;

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
