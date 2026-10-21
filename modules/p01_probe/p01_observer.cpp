#include "p01_observer.h"
#include "p01_write_census.h"
#include "p01_capture_startup.h"
#include <mutex>
#include <atomic>

// Session capture is serialized; physical operation identity remains separate
// from session metadata. Missing physical serial stays unknown, never invented.
static std::mutex hook_mutex;
static P01HookSnapshot snapshot;
static std::vector<P01HookEvent> events;
static uint64_t next_sequence = 1;
static uint64_t current_epoch = 0;
static uint64_t current_allocation_serial = 0;
static bool capture_poisoned = false;
static constexpr size_t MAX_EVENTS = 4096;
enum : uint8_t { EVENT_WRITE = 1, EVENT_READBACK_QUEUE = 2, EVENT_READBACK_COMPLETE = 3, EVENT_ALLOCATE = 4, EVENT_FREE = 5, EVENT_CLEAR = 6, EVENT_COPY = 7 };

static thread_local uint64_t recording_token = 0;
static std::atomic<uint64_t> next_recording_token{1};
P01RecordingScope::P01RecordingScope() : previous(recording_token) {
 uint64_t candidate = next_recording_token.load();
 while (candidate && !next_recording_token.compare_exchange_weak(candidate, candidate == UINT64_MAX ? 0 : candidate + 1)) {}
 token = candidate;
 recording_token = token;
}
P01RecordingScope::~P01RecordingScope() { recording_token = previous; }

static P01HookEvent append_event(P01HookEvent p_event) {
	if (events.size() >= MAX_EVENTS) {
		capture_poisoned = true;
		return {};
	}
	p_event.epoch = current_epoch;
	p_event.session_serial_hint = current_allocation_serial;
	p_event.sequence = next_sequence++;
	p_event.recording_token = recording_token;
	events.push_back(p_event);
	p01_capture_publish(p_event);
	return p_event;
}

void p01_observe_buffer_write(uint64_t p_rid, uint32_t p_offset, uint32_t p_size, bool p_success, uint64_t p_device, uint64_t p_driver, uint64_t p_serial, bool p_failed_prefix) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	if (current_epoch == 0) { return; }
	snapshot.write_begin_count++;
	if (p_success) {
		snapshot.write_bytes += p_size;
	} else {
		snapshot.write_failure_count++;
	}
	P01HookEvent event{p_rid, 0, 0, 0, p_offset, p_size, p_size, EVENT_WRITE, p_success, !p_success && p_failed_prefix};
	event.device = p_device;
	event.driver = p_driver;
	event.allocation_serial = p_serial;
	append_event(event);
}

void p01_observe_skeleton_setter(uint64_t p_skeleton, uint64_t p_version) {
	std::lock_guard<std::mutex> lock(hook_mutex); if (!current_epoch) return;
	P01HookEvent event; event.rid = p_skeleton; event.kind = 10; event.content_version = p_version; event.success = true; append_event(event);
}
void p01_observe_skeleton_upload(uint64_t p_skeleton, uint64_t p_buffer, uint32_t p_size, int32_t p_error) {
	std::lock_guard<std::mutex> lock(hook_mutex); if (!current_epoch) return;
	P01HookEvent event; event.rid = p_skeleton; event.source_rid = p_buffer; event.kind = 11; event.size = p_size; event.success = p_error == 0; event.operation_error = p_error; append_event(event);
}
void p01_observe_skeleton_version(uint64_t p_skeleton, uint64_t p_version) {
	std::lock_guard<std::mutex> lock(hook_mutex); if (!current_epoch) return;
	P01HookEvent event; event.rid = p_skeleton; event.kind = 12; event.content_version = p_version; event.success = true; append_event(event);
}
void p01_observe_readback_queued(uint64_t p_rid, uint32_t p_size) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	if (current_epoch == 0) { return; }
	snapshot.readback_queued_count++;
	append_event({p_rid, 0, 0, 0, 0, p_size, p_size, EVENT_READBACK_QUEUE, true, false});
}
void p01_observe_readback_completed(uint64_t p_rid, uint32_t p_size) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	if (current_epoch == 0) { return; }
	snapshot.readback_completed_count++;
	append_event({p_rid, 0, 0, 0, 0, p_size, p_size, EVENT_READBACK_COMPLETE, true, false});
}
void p01_observe_buffer_allocation(uint64_t p_rid, uint64_t p_device, uint64_t p_driver, uint64_t p_serial) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	if (current_epoch == 0) { return; }
	P01HookEvent event{p_rid, 0, 0, 0, 0, 0, 0, EVENT_ALLOCATE, true, false};
	event.device = p_device;
	event.driver = p_driver;
	event.allocation_serial = p_serial;
	append_event(event);
}
void p01_observe_buffer_free(uint64_t p_rid, uint64_t p_device, uint64_t p_driver, uint64_t p_serial) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	if (current_epoch == 0) { return; }
	P01HookEvent event{p_rid, 0, 0, 0, 0, 0, 0, EVENT_FREE, true, false};
	event.device = p_device;
	event.driver = p_driver;
	event.allocation_serial = p_serial;
	append_event(event);
}
void p01_observe_buffer_clear(uint64_t p_rid, uint32_t p_offset, uint32_t p_size, uint64_t p_device, uint64_t p_driver, uint64_t p_serial) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	if (current_epoch == 0) { return; }
	P01HookEvent event{p_rid, 0, 0, 0, p_offset, p_size, p_size, EVENT_CLEAR, true, false};
	event.device = p_device;
	event.driver = p_driver;
	event.allocation_serial = p_serial;
	append_event(event);
}
void p01_observe_buffer_copy(uint64_t p_src_rid, uint64_t p_dst_rid, uint32_t p_src_offset, uint32_t p_dst_offset, uint32_t p_size, uint64_t p_device, uint64_t p_source_serial, uint64_t p_destination_serial) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	if (current_epoch == 0) { return; }
	P01HookEvent event{p_dst_rid, 0, 0, 0, p_dst_offset, p_size, p_size, EVENT_COPY, true, false};
	event.source_rid = p_src_rid;
	event.source_offset = p_src_offset;
	event.device = p_device;
	event.source_allocation_serial = p_source_serial;
	event.allocation_serial = p_destination_serial;
	append_event(event);
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

// Writable uses share copy/write/free sequencing and the same bounded event log.
bool p01_write_census_active() { std::lock_guard<std::mutex> lock(hook_mutex); return current_epoch != 0; }
void p01_write_census_poison() { std::lock_guard<std::mutex> lock(hook_mutex); if (current_epoch) capture_poisoned = true; }
bool p01_write_census_is_poisoned() { return p01_hook_is_poisoned(); }
void p01_write_census_record(uint64_t p_driver, uint64_t p_device, uint64_t p_serial, uint32_t p_usage, uint32_t p_operation) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	if (!current_epoch || capture_poisoned) return;
	if (!p_driver || !p_device || !p_serial || !p_usage || p_operation < P01_DRAW || p_operation > P01_CALLBACK) { capture_poisoned = true; return; }
	P01HookEvent event;
	event.kind = 8;
	event.driver = p_driver;
	event.device = p_device;
	event.allocation_serial = p_serial;
	event.writable_usage = p_usage;
	event.write_operation = p_operation;
	event.success = true;
	append_event(event);
}
std::vector<P01WritableUse> p01_write_census_events() {
	std::lock_guard<std::mutex> lock(hook_mutex);
	std::vector<P01WritableUse> result;
	for (const auto &event : events) {
		if (event.kind == 8) result.push_back({ event.driver, event.device, event.allocation_serial, event.sequence, event.epoch, event.writable_usage, event.write_operation });
	}
	return result;
}

void p01_observe_buffer_dispose(uint64_t p_device, uint64_t p_driver, uint64_t p_serial) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	if (!current_epoch) return;
	P01HookEvent event;
	event.kind = 9;
	event.device = p_device;
	event.driver = p_driver;
	event.allocation_serial = p_serial;
	event.success = true;
	append_event(event);
}

void p01_observe_skeleton_signal(uint64_t p_object, bool p_after) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	if (!current_epoch) { return; }
	P01HookEvent event;
	event.rid = p_object;
	event.kind = p_after ? 14 : 13;
	event.success = true;
	append_event(event);
}
void p01_observe_skeleton_palette_request(uint64_t p_object, uint64_t p_skeleton, uint32_t p_bind) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	if (!current_epoch) { return; }
	P01HookEvent event;
	event.rid = p_skeleton;
	event.source_rid = p_object;
	event.offset = p_bind;
	event.kind = 15;
	event.success = true;
	append_event(event);
}

bool p01_hook_buffer_unchanged(uint64_t p_device, uint64_t p_allocation, uint64_t p_after_sequence, uint64_t p_epoch) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	if (!current_epoch || current_epoch != p_epoch || capture_poisoned || !p_device || !p_allocation || !p_after_sequence || p_after_sequence >= next_sequence) { return false; }
	bool anchor = false;
	for (const auto &event : events) {
		if (event.sequence == p_after_sequence) { anchor = event.epoch == current_epoch; }
		if (event.sequence <= p_after_sequence || event.device != p_device || event.allocation_serial != p_allocation) { continue; }
		switch (event.kind) {
			case 1: if (event.success || event.failed_prefix) { return false; } break;
			case 5: case 6: case 7: case 8: case 9: return false;
			default: break;
		}
	}
	return anchor;
}

P01HookEvent p01_observe_draw_record(uint64_t p_device, uint64_t p_mesh_instance, uint64_t p_vertex_array) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	if (!current_epoch || capture_poisoned) { return {}; }
	P01HookEvent event;
	event.device = p_device; event.rid = p_mesh_instance; event.source_rid = p_vertex_array;
	event.kind = 16; event.success = true;
	return append_event(event);
}

void p01_observe_driver_result(uint64_t p_device, uint32_t p_operation, uint64_t p_fence, int32_t p_error) {
	std::lock_guard<std::mutex> lock(hook_mutex);
	if (!current_epoch) { return; }
	P01HookEvent event;
	event.device = p_device; event.driver = p_fence; event.write_operation = p_operation;
	event.kind = 17; event.operation_error = p_error; event.success = p_error == 0;
	append_event(event);
	if (p_error != 0) { capture_poisoned = true; }
}
