/**************************************************************************/
/*  skinned_mesh_capture_test_capture_codec.h                             */
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
#include "skinned_mesh_capture_test_observer.h"

#include <map>
#include <string>

struct SkinnedMeshCaptureTestCaptureIdentity {
	std::string run, child, manifest_sha256, source_build;
};
// Private lossless JSONL codec for original hook records. Publication order is
// explicitly not a main/RT causal edge or a GPU completion certificate.
class SkinnedMeshCaptureTestCaptureCodec {
	SkinnedMeshCaptureTestCaptureIdentity identity;
	uint64_t frame_sequence = 0, event_count = 0, last_hook_sequence = 0, metadata_bytes = 0;
	bool started = false, sealed = false, poisoned = false;
	struct Blob {
		std::string digest;
		size_t size = 0;
	};
	std::map<std::string, Blob> blobs;
	std::string envelope(const char *p_kind) const;
	bool finish(std::string p_line, std::string &r_line);

public:
	bool start(const SkinnedMeshCaptureTestCaptureIdentity &p_identity, std::string &r_line);
	bool event(const SkinnedMeshCaptureTestHookEvent &p_event, const std::string &p_action, std::string &r_line);
	bool blob(const std::string &p_artifact, const uint8_t *p_bytes, size_t p_size, std::string &r_line);
	bool seal(std::string &r_line);
	bool failed() const { return poisoned; }
};
