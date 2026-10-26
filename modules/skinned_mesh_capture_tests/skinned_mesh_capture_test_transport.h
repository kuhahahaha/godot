/**************************************************************************/
/*  skinned_mesh_capture_test_transport.h                                 */
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

#include <deque>
#include <mutex>
#include <thread>

class SkinnedMeshCaptureTestTransportQueue {
	std::deque<SkinnedMeshCaptureTestCaptureEvent> queue;
	bool poisoned = false;
	std::thread::id owner;
	mutable std::mutex mutex;
	static constexpr size_t MAX_QUEUE = 128;

public:
	bool push(const SkinnedMeshCaptureTestCaptureEvent &p_event) {
		std::lock_guard<std::mutex> lock(mutex);
		if (owner == std::thread::id()) {
			owner = std::this_thread::get_id();
		}
		if (owner != std::this_thread::get_id()) {
			poisoned = true;
			return false;
		}
		if (poisoned || queue.size() >= MAX_QUEUE) {
			poisoned = true;
			return false;
		}
		queue.push_back(p_event);
		return true;
	}
	bool pop(SkinnedMeshCaptureTestCaptureEvent &r_event) {
		std::lock_guard<std::mutex> lock(mutex);
		if (owner == std::thread::id()) {
			owner = std::this_thread::get_id();
		}
		if (owner != std::this_thread::get_id()) {
			poisoned = true;
			return false;
		}
		if (queue.empty()) {
			return false;
		}
		r_event = queue.front();
		queue.pop_front();
		return true;
	}
	bool is_poisoned() const {
		std::lock_guard<std::mutex> lock(mutex);
		return poisoned;
	}
	size_t size() const {
		std::lock_guard<std::mutex> lock(mutex);
		return queue.size();
	}
};
