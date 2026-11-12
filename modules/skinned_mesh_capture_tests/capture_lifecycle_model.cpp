/**************************************************************************/
/*  capture_lifecycle_model.cpp                                           */
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

#include "capture_lifecycle_model.h"

#include "skinned_mesh_capture_test_kernels.h"

bool CaptureLifecycleTestModel::begin(uint64_t p_epoch) {
	if (begun || closed || p_epoch == 0) {
		return false;
	}
	capture.begin(p_epoch);
	if (capture.is_poisoned()) {
		return false;
	}
	result = {};
	result.epoch = p_epoch;
	next_token = 1;
	next_queue_sequence = 1;
	begun = true;
	return true;
}

bool CaptureLifecycleTestModel::advance_action(uint32_t p_value) {
	if (!begun || closed || result.action_count == UINT32_MAX) {
		return false;
	}
	const uint64_t token = next_token++;
	const uint64_t queue_sequence = next_queue_sequence++;
	const SkinnedMeshCaptureTestCaptureEvent queued{ 0, 1, token, result.epoch, 1, 1, 1, 1, queue_sequence, 0, sizeof(uint32_t), 0 };
	if (!capture.append(queued)) {
		result.capture_poisoned = capture.is_poisoned();
		return false;
	}
	const uint32_t output = skinned_mesh_capture_test_kernel_identity({ p_value });
	const SkinnedMeshCaptureTestCaptureEvent completed{ 0, 2, token, result.epoch, 1, 1, 1, 1, queue_sequence, 0, sizeof(uint32_t), output };
	if (!capture.append(completed)) {
		result.capture_poisoned = capture.is_poisoned();
		return false;
	}
	result.action_count++;
	result.last_value = output;
	return true;
}

CaptureLifecycleTestModelResult CaptureLifecycleTestModel::take_result() const {
	CaptureLifecycleTestModelResult snapshot = result;
	snapshot.capture_poisoned = snapshot.capture_poisoned || capture.is_poisoned();
	snapshot.disposed = snapshot.disposed || capture.is_disposed();
	return snapshot;
}

bool CaptureLifecycleTestModel::close() {
	if (!begun || closed || !capture.drain_complete()) {
		return false;
	}
	if (!capture.dispose_physical() || !capture.seal()) {
		result.capture_poisoned = capture.is_poisoned();
		return false;
	}
	closed = true;
	result.disposed = true;
	return true;
}
