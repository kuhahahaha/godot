/**************************************************************************/
/*  gdextension_scene_phase.h                                             */
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
#include "core/extension/gdextension_interface.gen.h"
#include "core/object/ref_counted.h"
#include "core/math/aabb.h"
#include "core/math/transform_3d.h"

namespace GDExtensionDeviceExit {
inline GDExtensionSceneOpaque32V1 scene_phase_opaque(uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
	GDExtensionSceneOpaque32V1 out{};
	const uint64_t words[] = { a, b, c, d };
	for (uint32_t word = 0; word < 4; ++word) {
		for (uint32_t byte = 0; byte < 8; ++byte) {
			out.bytes[word * 8 + byte] = uint8_t(words[word] >> (byte * 8));
		}
	}
	return out;
}
inline uint64_t scene_phase_word(const GDExtensionSceneOpaque32V1 &token, uint32_t word) {
	uint64_t value = 0;
	for (uint32_t byte = 0; byte < 8; ++byte) {
		value |= uint64_t(token.bytes[word * 8 + byte]) << (8 * byte);
	}
	return value;
}
inline constexpr uint64_t SCENE_RELAY_KIND = 0x4b303652454c4159ull;
inline constexpr uint64_t SCENE_REGISTRATION_KIND = 0x4b303653494e4b31ull;
inline bool scene_phase_config_valid(const GDExtensionScenePhaseSinkConfigV1 &c) {
	return c.abi.abi_version == 1 && c.abi.struct_size == sizeof(c) && !c.abi.reserved && c.phase == 1 && c.max_operations >= 1 && c.max_operations <= 64;
}
struct SceneConnectionRow {
	uint64_t serial, receiver, base, generation;
	uint8_t owner_tag[32], device_tag[32];
	uint32_t kind, max_sources, max_resolution;
	bool released;
};
struct SceneConnectionObservation {
	bool released = false;
	uint32_t owned = 0;
};
// Native commit receipt, borrowed facts only: no source-pair readiness or DSO
// transport. Returned only after the actual receiver commits atomically.
struct SceneConnectionQueryObservation {
	uint64_t receiver = 0, base = 0, generation = 0;
	uint64_t bounds_revision = 0, pose_revision = 0, pair_revision = 0;
	Transform3D transform;
	AABB local_envelope, world_query;
};
inline constexpr uint64_t SCENE_CONNECTION_KIND = 0x4b3036434f4e4e31ull;

// Source-specific native Scene owner implements this retained-content facet.
// It is not a DSO callback or another token registry. Returning the lease is
// explicit on original RT RELEASE after the original accepted readers close.
class SceneReadContent : public RefCounted {
public:
	virtual Error validate(const GDExtensionSceneOpaque32V1 &device,
			const GDExtensionSceneOpaque32V1 &sample) const = 0;
	virtual Error return_after_reads() = 0;
};
struct ScenePhaseAttempt {
	Ref<SceneReadContent> content;
	GDExtensionSceneOpaque32V1 sample = {};
	uint64_t occurrence = 0, physical_frame = 0;
	bool consumed = false;
};
} // namespace GDExtensionDeviceExit
