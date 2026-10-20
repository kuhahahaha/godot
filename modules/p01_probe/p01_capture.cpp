#include "p01_capture.h"

void P01Capture::begin(uint64_t p_epoch) {
	events.clear();
	tokens.clear();
	epoch = p_epoch;
	next_sequence = 1;
	poisoned = p_epoch == 0;
	sealed = false;
}

bool P01Capture::append(const P01CaptureEvent &p_event) {
	if (poisoned || sealed || epoch == 0 || p_event.epoch != epoch || p_event.token == 0 || p_event.queue_sequence != p_event.token || p_event.rid == 0 || p_event.allocation_serial == 0 || UINT32_MAX - p_event.offset < p_event.size) {
		poisoned = true;
		return false;
	}
	if (p_event.kind == 2) {
		bool found = false;
		for (uint64_t &token : tokens) if (token == p_event.token) { token = 0; found = true; break; }
		if (!found) { poisoned = true; return false; }
	}
	P01CaptureEvent event = p_event;
	event.sequence = next_sequence++;
	events.push_back(event);
	if (p_event.kind == 1) tokens.push_back(p_event.token);
	return true;
}

bool P01Capture::seal() {
	if (poisoned || sealed || epoch == 0) return false;
	sealed = true;
	return true;
}
