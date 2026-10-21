#pragma once
#include <cstdint>
#include <vector>

struct P01PhaseEvent {
	uint64_t sequence = 0, epoch = 0, frame = 0;
	bool after_views = false;
};
// Private acquisition control. No script binding or external callback.
bool p01_phase_begin(uint64_t p_epoch);
void p01_phase_end();
void p01_observe_phase(uint64_t p_frame, bool p_after_views);
std::vector<P01PhaseEvent> p01_phase_events();
bool p01_phase_poisoned();
