/**************************************************************************/
/*  skinned_mesh_capture_test_public_snapshot.h                           */
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

#include "core/math/transform_3d.h"
#include "core/math/vector3.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

#include <array>

// Private, copied Q1 values, not an authenticated native observation. The
// fixture must still match its live owner, epoch, request and bind digest.
struct SkinnedMeshCaptureTestPublicSnapshot {
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
Error skinned_mesh_capture_test_decode_public_snapshot(const Dictionary &p_value, SkinnedMeshCaptureTestPublicSnapshot &r_snapshot);
