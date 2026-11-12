/**************************************************************************/
/*  skinned_mesh_capture_test_capture_startup.h                           */
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
#include "skinned_mesh_capture_test_capture_codec.h"

#include <vector>

struct SkinnedMeshCaptureTestCaptureStartup {
	int descriptor = -1;
	SkinnedMeshCaptureTestCaptureIdentity identity;
};
enum class SkinnedMeshCaptureTestStartupParse { Absent,
	Valid,
	Invalid };
SkinnedMeshCaptureTestStartupParse skinned_mesh_capture_test_parse_capture_startup(const std::vector<std::string> &p_args, SkinnedMeshCaptureTestCaptureStartup &r_startup);
// Original engine lifecycle only; not registered to ClassDB or GDExtension.
void skinned_mesh_capture_test_capture_startup();
void skinned_mesh_capture_test_capture_shutdown();
void skinned_mesh_capture_test_capture_publish(const SkinnedMeshCaptureTestHookEvent &p_event);
bool skinned_mesh_capture_test_capture_transport_available();

// Private child-bound one-shot admission; never exposed through ClassDB.
bool skinned_mesh_capture_test_capture_claim_child(const std::string &p_member, const std::string &p_route, uint64_t &r_epoch);

// Engine-only completion publication; no seal or qualification is implied.
bool skinned_mesh_capture_test_capture_blob(const std::string &p_artifact, const uint8_t *p_bytes, size_t p_size);
