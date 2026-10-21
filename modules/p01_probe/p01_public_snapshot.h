#pragma once

#include "core/variant/dictionary.h"
#include "core/math/transform_3d.h"
#include "core/math/vector3.h"
#include "core/string/ustring.h"

#include <array>

// Private, copied Q1 values, not an authenticated native observation. The
// fixture must still match its live owner, epoch, request and bind digest.
struct P01PublicSnapshot {
	int64_t epoch = 0;
	int64_t request_id = 0;
	std::array<uint64_t, 6> source_object_ids{};
	String source_content_digest;
	String bind_digest;
	std::array<float, 24> pose_matrices{};
	Transform3D source_to_world;
	std::array<Vector3, 3> positions{};
	std::array<int32_t, 12> bind_indices{};
	std::array<int32_t, 12> weights_u16{};
	bool mesh_refresh = false;
	int64_t pose_signal_ordinal = 0;
	int64_t observed_signal_ordinal = 0;
};

// Reject without modifying r_snapshot. This performs Q1 value validation and
// Q2 source-content digest verification; it does not admit work or issue IDs.
Error p01_decode_public_snapshot(const Dictionary &p_value, P01PublicSnapshot &r_snapshot);
