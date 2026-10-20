#include "p01_observer.h"
#include <mutex>

// Hook observations are scoped to the owning thread. RenderingDevice instances
// may issue work concurrently; a process-global mutable snapshot would lose
// events and introduce a data race between instances.
static std::mutex hook_mutex;
static P01HookSnapshot snapshot;
static std::vector<P01HookEvent> events;
static uint64_t next_sequence = 1;

void p01_observe_buffer_write(uint64_t p_rid, uint32_t p_offset, uint32_t p_size, bool p_success) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	snapshot.write_begin_count++;
	if (p_success) {
		snapshot.write_bytes += p_size;
	} else {
		snapshot.write_failure_count++;
	}
	events.push_back({p_rid, 0, 0, next_sequence++, p_offset, p_size, p_size, p_success, !p_success && p_offset != 0});
}
void p01_observe_readback_queued(uint64_t p_rid, uint32_t p_size) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	snapshot.readback_queued_count++;
	events.push_back({p_rid, 0, 0, next_sequence++, 0, p_size, p_size, true, false});
}
void p01_observe_readback_completed(uint64_t p_rid, uint32_t p_size) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	snapshot.readback_completed_count++;
	events.push_back({p_rid, 0, 0, next_sequence++, 0, p_size, p_size, true, false});
}
P01HookSnapshot p01_hook_snapshot() { std::lock_guard<std::mutex> lock(hook_mutex); return snapshot; }
std::vector<P01HookEvent> p01_hook_events() { std::lock_guard<std::mutex> lock(hook_mutex); return events; }
void p01_hook_reset() { std::lock_guard<std::mutex> lock(hook_mutex); snapshot = {}; events.clear(); next_sequence = 1; }
