#include "p01_capture.h"

void P01Capture::begin(uint64_t p_epoch) {
	std::lock_guard<std::mutex> lock(mutex);
	if (!pending.empty() || !events.empty() && !sealed) { poisoned = true; return; }
	events.clear();
	pending.clear();
	epoch = p_epoch;
	next_sequence = 1;
	poisoned = p_epoch == 0;
	sealed = false;
	disposed = false;
}

bool P01Capture::append(const P01CaptureEvent &p_event) {
	std::lock_guard<std::mutex> lock(mutex);
	if (sealed || disposed || epoch == 0 || p_event.epoch != epoch || p_event.token == 0 || p_event.rid == 0 || p_event.allocation_serial == 0 || UINT32_MAX - p_event.offset < p_event.size || (poisoned && p_event.kind != 2) || (events.size() >= MAX_EVENTS && p_event.kind != 2)) {
		poisoned = true;
		return false;
	}
	if (p_event.kind == 1) {
		if (p_event.queue_sequence == 0 || pending.size() >= MAX_PENDING) { poisoned = true; return false; }
		pending.push_back({p_event.token, p_event.epoch, p_event.rid, p_event.allocation_serial, p_event.queue_sequence});
	} else if (p_event.kind == 2) {
		bool found = false;
		for (auto it = pending.begin(); it != pending.end(); ++it) if (it->token == p_event.token && it->epoch == p_event.epoch && it->rid == p_event.rid && it->serial == p_event.allocation_serial && it->queue == p_event.queue_sequence) { pending.erase(it); found = true; break; }
		if (!found) { poisoned = true; return false; }
	}
	P01CaptureEvent event = p_event;
	event.sequence = next_sequence++;
	if (events.size() < MAX_EVENTS) events.push_back(event);
	return true;
}

bool P01Capture::seal() {
	std::lock_guard<std::mutex> lock(mutex);
	if (poisoned || sealed || epoch == 0 || !pending.empty()) return false;
	sealed = true;
	return true;
}

bool P01Capture::dispose_physical() {
	std::lock_guard<std::mutex> lock(mutex);
	// Poison invalidates evidence publication but must never block engine-owned
	// retirement once all queued GPU work has drained.
	if (!pending.empty() || disposed || (!sealed && !poisoned)) return false;
	disposed = true;
	return true;
}
