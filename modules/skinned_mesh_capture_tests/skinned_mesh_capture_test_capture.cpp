/**************************************************************************/
/*  skinned_mesh_capture_test_capture.cpp                                 */
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

#include "skinned_mesh_capture_test_capture.h"

void SkinnedMeshCaptureTestCapture::begin(uint64_t p_epoch) {
	std::lock_guard<std::mutex> lock(mutex);
	if (active && !disposed && (!pending.empty() || !sealed)) {
		poisoned = true;
		return;
	}
	if (p_epoch == 0 || p_epoch <= last_epoch) {
		poisoned = true;
		return;
	}
	events.clear();
	pending.clear();
	epoch = p_epoch;
	next_sequence = 1;
	poisoned = p_epoch == 0;
	sealed = false;
	disposed = false;
	eof = false;
	transport_failed = false;
	active = !poisoned;
	last_epoch = p_epoch;
}

bool SkinnedMeshCaptureTestCapture::append(const SkinnedMeshCaptureTestCaptureEvent &p_event) {
	std::lock_guard<std::mutex> lock(mutex);
	if (sealed || disposed || epoch == 0 || p_event.epoch != epoch || p_event.token == 0 || p_event.rid == 0 || p_event.allocation_serial == 0 || p_event.owner == 0 || p_event.alias == 0 || (p_event.kind != 1 && p_event.kind != 2) || UINT32_MAX - p_event.offset < p_event.size || (poisoned && p_event.kind != 2) || (events.size() >= MAX_EVENTS && p_event.kind != 2) || next_sequence == UINT64_MAX) {
		poisoned = true;
		return false;
	}
	if (p_event.kind == 1) {
		if (p_event.queue_sequence == 0 || pending.size() >= MAX_PENDING) {
			poisoned = true;
			return false;
		}
		for (const SkinnedMeshCaptureTestCaptureEvent &prior : events) {
			if (prior.kind == 1 && prior.token == p_event.token && prior.epoch == p_event.epoch && prior.rid == p_event.rid && prior.allocation_serial == p_event.allocation_serial && prior.owner == p_event.owner && prior.alias == p_event.alias && prior.queue_sequence == p_event.queue_sequence) {
				poisoned = true;
				return false;
			}
		}
		pending.push_back({ p_event.token, p_event.epoch, p_event.rid, p_event.allocation_serial, p_event.owner, p_event.alias, p_event.queue_sequence });
	} else if (p_event.kind == 2) {
		bool found = false;
		for (auto it = pending.begin(); it != pending.end(); ++it) {
			if (it->token == p_event.token && it->epoch == p_event.epoch && it->rid == p_event.rid && it->serial == p_event.allocation_serial && it->owner == p_event.owner && it->alias == p_event.alias && it->queue == p_event.queue_sequence) {
				pending.erase(it);
				found = true;
				break;
			}
		}
		if (!found) {
			poisoned = true;
			return false;
		}
	}
	SkinnedMeshCaptureTestCaptureEvent event = p_event;
	event.sequence = next_sequence++;
	if (events.size() < MAX_EVENTS) {
		events.push_back(event);
	} else {
		// Completion must still release its exact pending tuple, but missing
		// evidence makes publication invalid even when retirement succeeds.
		poisoned = true;
	}
	return true;
}

bool SkinnedMeshCaptureTestCapture::seal() {
	std::lock_guard<std::mutex> lock(mutex);
	// Physical retirement is an engine-owned prerequisite for publishing the
	// capture.  The parent receiver still validates seal/EOF independently.
	if (poisoned || sealed || epoch == 0 || !pending.empty() || !disposed) {
		return false;
	}
	sealed = true;
	eof = true;
	active = false;
	return true;
}

bool SkinnedMeshCaptureTestCapture::write_transport(const uint8_t *p_data, size_t p_size, size_t p_written) {
	std::lock_guard<std::mutex> lock(mutex);
	if (p_data == nullptr || p_written != p_size || !sealed || !eof || transport_failed) {
		transport_failed = true;
		return false;
	}
	return true;
}

bool SkinnedMeshCaptureTestCapture::dispose_physical() {
	std::lock_guard<std::mutex> lock(mutex);
	// Poison invalidates evidence publication but must never block engine-owned
	// retirement once all queued GPU work has drained.
	if (epoch == 0 || pending.size() != 0 || disposed || sealed) {
		return false;
	}
	disposed = true;
	return true;
}

bool SkinnedMeshCaptureTestCapture::drain_complete() const {
	std::lock_guard<std::mutex> lock(mutex);
	return pending.empty();
}

bool SkinnedMeshCaptureTestCapture::is_poisoned() const {
	std::lock_guard<std::mutex> lock(mutex);
	return poisoned;
}

bool SkinnedMeshCaptureTestCapture::is_sealed() const {
	std::lock_guard<std::mutex> lock(mutex);
	return sealed;
}

bool SkinnedMeshCaptureTestCapture::is_disposed() const {
	std::lock_guard<std::mutex> lock(mutex);
	return disposed;
}

bool SkinnedMeshCaptureTestCapture::has_transport_failure() const {
	std::lock_guard<std::mutex> lock(mutex);
	return transport_failed;
}

std::vector<SkinnedMeshCaptureTestCaptureEvent> SkinnedMeshCaptureTestCapture::get_events() const {
	std::lock_guard<std::mutex> lock(mutex);
	return events;
}
