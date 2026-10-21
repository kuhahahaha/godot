#pragma once

#include "p01_capture.h"
#include <deque>

class P01TransportQueue {
	std::deque<P01CaptureEvent> queue;
	bool poisoned = false;
	static constexpr size_t MAX_QUEUE = 128;
	public:
	bool push(const P01CaptureEvent &p_event) {
		if (poisoned || queue.size() >= MAX_QUEUE) { poisoned = true; return false; }
		queue.push_back(p_event); return true;
	}
	bool pop(P01CaptureEvent &r_event) {
		if (queue.empty()) return false;
		r_event = queue.front(); queue.pop_front(); return true;
	}
	bool is_poisoned() const { return poisoned; }
	size_t size() const { return queue.size(); }
};
