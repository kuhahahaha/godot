/**************************************************************************/
/*  capture_lifecycle_model.h                                             */
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

#include "skinned_mesh_capture_test_capture.h"

#include <cstdint>

// Tests-only deterministic native fixture.  It exercises the private capture
// and kernel primitives without exposing a ClassDB or script-facing evidence
// API.  Real RenderingDevice acquisition is owned by the production hooks and
// remains a separate acceptance layer.
struct CaptureLifecycleTestModelResult {
	uint64_t epoch = 0;
	uint32_t action_count = 0;
	uint32_t last_value = 0;
	bool capture_poisoned = false;
	bool disposed = false;
};

class CaptureLifecycleTestModel {
	SkinnedMeshCaptureTestCapture capture;
	CaptureLifecycleTestModelResult result;
	uint64_t next_token = 1;
	uint64_t next_queue_sequence = 1;
	bool begun = false;
	bool closed = false;

public:
	bool begin(uint64_t p_epoch);
	bool advance_action(uint32_t p_value);
	CaptureLifecycleTestModelResult take_result() const;
	bool close();
};
