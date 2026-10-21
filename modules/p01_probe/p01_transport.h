#pragma once

#include "p01_capture.h"
#include <deque>
#include <thread>

class P01TransportQueue {
	std::deque<P01CaptureEvent> queue;
	bool poisoned = false;
	std::thread::id owner;
	static constexpr size_t MAX_QUEUE = 128;
	public:
	bool push(const P01CaptureEvent &p_event) {
		if (owner == std::thread::id()) owner = std::this_thread::get_id();
		if (owner != std::this_thread::get_id()) { poisoned = true; return false; }
		if (poisoned || queue.size() >= MAX_QUEUE) { poisoned = true; return false; }
		queue.push_back(p_event); return true;
	}
	bool pop(P01CaptureEvent &r_event) {
		if (owner == std::thread::id()) owner = std::this_thread::get_id();
		if (owner != std::this_thread::get_id()) { poisoned = true; return false; }
		if (queue.empty()) return false;
		r_event = queue.front(); queue.pop_front(); return true;
	}
	bool is_poisoned() const { return poisoned; }
	size_t size() const { return queue.size(); }
};
