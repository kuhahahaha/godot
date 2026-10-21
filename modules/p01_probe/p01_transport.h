#pragma once

#include "p01_capture.h"
#include <deque>
#include <mutex>
#include <thread>

class P01TransportQueue {
	std::deque<P01CaptureEvent> queue;
	bool poisoned = false;
	std::thread::id owner;
	mutable std::mutex mutex;
	static constexpr size_t MAX_QUEUE = 128;
	public:
	bool push(const P01CaptureEvent &p_event) {
		std::lock_guard<std::mutex> lock(mutex);
		if (owner == std::thread::id()) owner = std::this_thread::get_id();
		if (owner != std::this_thread::get_id()) { poisoned = true; return false; }
		if (poisoned || queue.size() >= MAX_QUEUE) { poisoned = true; return false; }
		queue.push_back(p_event); return true;
	}
	bool pop(P01CaptureEvent &r_event) {
		std::lock_guard<std::mutex> lock(mutex);
		if (owner == std::thread::id()) owner = std::this_thread::get_id();
		if (owner != std::this_thread::get_id()) { poisoned = true; return false; }
		if (queue.empty()) return false;
		r_event = queue.front(); queue.pop_front(); return true;
	}
	bool is_poisoned() const { std::lock_guard<std::mutex> lock(mutex); return poisoned; }
	size_t size() const { std::lock_guard<std::mutex> lock(mutex); return queue.size(); }
};
