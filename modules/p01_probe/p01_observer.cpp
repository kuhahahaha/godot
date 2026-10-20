#include "p01_observer.h"
#include <mutex>

// Hook observations are scoped to the owning thread. RenderingDevice instances
// may issue work concurrently; a process-global mutable snapshot would lose
// events and introduce a data race between instances.
static std::mutex hook_mutex;
static P01HookSnapshot snapshot;
static std::vector<P01HookEvent> events;
static uint64_t next_sequence = 1;
static uint64_t current_epoch = 0;
static uint64_t current_allocation_serial = 0;
static bool capture_poisoned = false;
static constexpr size_t MAX_EVENTS = 4096;
enum : uint8_t { EVENT_WRITE = 1, EVENT_READBACK_QUEUE = 2, EVENT_READBACK_COMPLETE = 3, EVENT_ALLOCATE = 4, EVENT_FREE = 5, EVENT_CLEAR = 6, EVENT_COPY = 7 };

static void append_event(P01HookEvent p_event) {
	if (events.size() >= MAX_EVENTS) {
		capture_poisoned = true;
		return;
	}
	p_event.epoch = current_epoch;
	p_event.allocation_serial = current_allocation_serial;
	p_event.sequence = next_sequence++;
	events.push_back(p_event);
}

void p01_observe_buffer_write(uint64_t p_rid, uint32_t p_offset, uint32_t p_size, bool p_success) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	snapshot.write_begin_count++;
	if (p_success) {
		snapshot.write_bytes += p_size;
	} else {
		snapshot.write_failure_count++;
	}
	append_event({p_rid, 0, 0, 0, p_offset, p_size, p_size, EVENT_WRITE, p_success, !p_success && p_offset != 0});
}
void p01_observe_readback_queued(uint64_t p_rid, uint32_t p_size) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	snapshot.readback_queued_count++;
	append_event({p_rid, 0, 0, 0, 0, p_size, p_size, EVENT_READBACK_QUEUE, true, false});
}
void p01_observe_readback_completed(uint64_t p_rid, uint32_t p_size) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	snapshot.readback_completed_count++;
	append_event({p_rid, 0, 0, 0, 0, p_size, p_size, EVENT_READBACK_COMPLETE, true, false});
}
void p01_observe_buffer_allocation(uint64_t p_rid, uint64_t p_device, uint64_t p_driver) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	append_event({p_rid, p_device, p_driver, 0, 0, 0, 0, EVENT_ALLOCATE, true, false});
}
void p01_observe_buffer_free(uint64_t p_rid) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	append_event({p_rid, 0, 0, 0, 0, 0, 0, EVENT_FREE, true, false});
}
void p01_observe_buffer_clear(uint64_t p_rid, uint32_t p_offset, uint32_t p_size) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	append_event({p_rid, 0, 0, 0, p_offset, p_size, p_size, EVENT_CLEAR, true, false});
}
void p01_observe_buffer_copy(uint64_t p_src_rid, uint64_t p_dst_rid, uint32_t p_src_offset, uint32_t p_dst_offset, uint32_t p_size) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	append_event({p_dst_rid, p_src_rid, p_dst_rid, 0, p_dst_offset, p_size, p_size, EVENT_COPY, true, false});
}
P01HookSnapshot p01_hook_snapshot() { std::lock_guard<std::mutex> lock(hook_mutex); return snapshot; }
std::vector<P01HookEvent> p01_hook_events() { std::lock_guard<std::mutex> lock(hook_mutex); return events; }
void p01_hook_begin(uint64_t p_epoch, uint64_t p_allocation_serial) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	if (p_epoch == 0 || (current_epoch != 0 && p_epoch <= current_epoch)) { capture_poisoned = true; return; }
	current_epoch = p_epoch;
	current_allocation_serial = p_allocation_serial;
}
bool p01_hook_is_poisoned() { std::lock_guard<std::mutex> lock(hook_mutex); return capture_poisoned; }
void p01_hook_reset() {
	std::lock_guard<std::mutex> lock(hook_mutex);
	snapshot = {};
	events.clear();
	next_sequence = 1;
	current_epoch = 0;
	current_allocation_serial = 0;
	capture_poisoned = false;
}
