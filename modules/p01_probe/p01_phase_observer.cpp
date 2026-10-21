#include "p01_phase_observer.h"
#include <mutex>
#include <thread>

static std::mutex phase_mutex;
static std::vector<P01PhaseEvent> phase_events;
static uint64_t phase_epoch = 0, last_epoch = 0, last_frame = 0;
static bool active = false, awaiting_views = false, poisoned = false;
static std::thread::id phase_thread;

bool p01_phase_begin(uint64_t p_epoch) {
	std::lock_guard<std::mutex> lock(phase_mutex);
	if (active || !p_epoch || p_epoch <= last_epoch) { return false; }
	phase_epoch = last_epoch = p_epoch;
	last_frame = 0;
	active = true;
	awaiting_views = poisoned = false;
	phase_thread = {};
	phase_events.clear();
	return true;
}
void p01_phase_end() {
	std::lock_guard<std::mutex> lock(phase_mutex);
	if (active && awaiting_views) { poisoned = true; }
	active = false;
}
void p01_observe_phase(uint64_t p_frame, bool p_after_views) {
	std::lock_guard<std::mutex> lock(phase_mutex);
	if (!active) { return; }
	if (phase_thread == std::thread::id()) { phase_thread = std::this_thread::get_id(); }
	if (poisoned || phase_thread != std::this_thread::get_id() || phase_events.size() >= 128 || p_after_views != awaiting_views || (p_after_views ? p_frame != last_frame : (!phase_events.empty() && p_frame <= last_frame))) {
		poisoned = true;
		return;
	}
	phase_events.push_back({ phase_events.size() + 1, phase_epoch, p_frame, p_after_views });
	last_frame = p_frame;
	awaiting_views = !p_after_views;
}
std::vector<P01PhaseEvent> p01_phase_events() { std::lock_guard<std::mutex> lock(phase_mutex); return phase_events; }
bool p01_phase_poisoned() { std::lock_guard<std::mutex> lock(phase_mutex); return poisoned; }
