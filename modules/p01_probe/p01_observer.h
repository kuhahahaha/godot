#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <limits>

class P01ReadbackRequestBook {
	struct Request { uint64_t epoch = 0, token = 0, rid = 0; uint32_t size = 0; bool live = false; };
	std::array<Request, 128> requests{};
	uint64_t epoch = 0;
	uint64_t next_token = 1;
	uint32_t live_count = 0;
public:
	bool begin_epoch(uint64_t p_epoch) {
		if (p_epoch == 0 || p_epoch <= epoch || live_count != 0) return false;
		epoch = p_epoch;
		return true;
	}
	uint64_t enqueue(uint64_t p_epoch, uint64_t p_rid, uint32_t p_size) {
		if (p_epoch != epoch || p_epoch == 0 || p_rid == 0 || p_size == 0 || next_token == std::numeric_limits<uint64_t>::max()) return 0;
		for (Request &r : requests) if (!r.live) { r = {p_epoch, next_token++, p_rid, p_size, true}; ++live_count; return r.token; }
		return 0;
	}
	bool complete(uint64_t p_epoch, uint64_t p_token, uint64_t p_rid, uint32_t p_size) {
		if (p_epoch != epoch || p_token == 0) return false;
		for (Request &r : requests) if (r.live && r.epoch == p_epoch && r.token == p_token && r.rid == p_rid && r.size == p_size) { r.live = false; --live_count; return true; }
		return false;
	}
	uint32_t pending() const { return live_count; }
};

struct P01HookEvent {
	uint64_t rid = 0, epoch = 0, allocation_serial = 0, sequence = 0;
	uint32_t offset = 0, size = 0, range = 0;
	uint8_t kind = 0;
	bool success = false, failed_prefix = false;
	// Operation identity is distinct from session metadata.
	uint64_t device = 0, driver = 0, source_rid = 0;
	uint32_t source_offset = 0;
	uint64_t session_serial_hint = 0;
	uint64_t source_allocation_serial = 0;
	uint32_t writable_usage = 0, write_operation = 0;
	uint64_t content_version = 0;
	int32_t operation_error = 0;
	uint64_t recording_token = 0; // Private exact synchronous recording scope; not wire authority.
};

// Engine-owned lifetime accounting. Transport/parent verdicts never release GPU
// work. The caller serializes access on the collector's owning thread.
class P01CollectorLifetime {
	uint64_t epoch = 0;
	uint32_t pending = 0;
	uint32_t slot_pending = 0;
	std::array<bool, 64> slots{};
	bool accepted = false;
	bool closing = false;
	bool references_released = false;
	bool capture_poisoned = false;
	uint64_t next_token = 1;
	std::array<uint64_t, 128> generic_tokens{};
	std::array<uint64_t, 64> slot_tokens{};

public:
	bool accept(uint64_t p_epoch) {
		if (accepted || closing || p_epoch == 0) {
			return false;
		}
		epoch = p_epoch;
		accepted = true;
		return true;
	}
	uint64_t record_token(uint64_t p_epoch) {
		if (!accepted || closing || capture_poisoned || p_epoch != epoch || pending == UINT32_MAX) {
			return 0;
		}
		for (uint64_t &token : generic_tokens) {
			if (token == 0) { token = next_token++; ++pending; return token; }
		}
		return 0;
	}
	bool record(uint64_t p_epoch) { return record_token(p_epoch) != 0; }
	bool complete_readback_token(uint64_t p_epoch, uint64_t p_token) {
		if (!accepted || p_epoch != epoch || p_token == 0) return false;
		for (uint64_t &token : generic_tokens) {
			if (token == p_token) { token = 0; --pending; return true; }
		}
		return false;
	}
	bool complete_readback(uint64_t p_epoch) {
		(void)p_epoch;
		// Completion must identify the exact queued request. Never consume an
		// arbitrary pending operation based only on the epoch.
		return false;
	}
	uint64_t record_slot_token(uint64_t p_epoch, uint32_t p_slot) {
		if (p_slot >= slots.size() || !accepted || closing || p_epoch != epoch || slots[p_slot]) return 0;
		if (next_token == std::numeric_limits<uint64_t>::max()) return 0;
		slots[p_slot] = true;
		slot_tokens[p_slot] = next_token++;
		++slot_pending;
		return slot_tokens[p_slot];
	}
	bool record_slot(uint64_t p_epoch, uint32_t p_slot) { return record_slot_token(p_epoch, p_slot) != 0; }
	bool complete_slot_token(uint64_t p_epoch, uint32_t p_slot, uint64_t p_token) {
		if (p_slot >= slots.size() || p_epoch != epoch || !slots[p_slot] || slot_tokens[p_slot] != p_token) return false;
		slots[p_slot] = false;
		slot_tokens[p_slot] = 0;
		--slot_pending;
		return true;
	}
	bool complete_slot(uint64_t p_epoch, uint32_t p_slot) {
		if (p_slot >= slots.size() || p_epoch != epoch || !slots[p_slot]) return false;
		return complete_slot_token(p_epoch, p_slot, slot_tokens[p_slot]);
	}
	void close() { closing = true; }
	void poison_capture() { capture_poisoned = true; }
	void device_failure() { capture_poisoned = true; }
	bool is_capture_poisoned() const { return capture_poisoned; }
	void release_native_references() { references_released = true; }
	bool can_retire() const { return accepted && closing && pending == 0 && slot_pending == 0 && references_released; }
};


class P01AllocationIdentity {
	struct Owner { uint64_t owner = 0; bool live = false; };
	struct Entry { uint64_t device = 0; uint64_t driver = 0; uint64_t serial = 0; uint32_t references = 0; bool physically_live = false; std::array<Owner, 8> owners{}; };
	std::array<Entry, 64> entries{};
	uint64_t next_serial = 1;
public:
	uint64_t acquire(uint64_t p_device, uint64_t p_driver) {
		if (p_device == 0 || p_driver == 0) return 0;
		for (Entry &e : entries) if (e.references != 0 && e.device == p_device && e.driver == p_driver) { ++e.references; return e.serial; }
		for (Entry &e : entries) if (e.references == 0) { e = {p_device, p_driver, next_serial++, 1}; return e.serial; }
		return 0;
	}
	uint64_t acquire_alias(uint64_t p_device, uint64_t p_driver) { return acquire(p_device, p_driver); }
	uint64_t acquire_owner(uint64_t p_device, uint64_t p_driver, uint64_t p_owner) {
		if (p_owner == 0) return 0;
		uint64_t serial = acquire(p_device, p_driver);
		for (Entry &e : entries) if (e.physically_live && e.device == p_device && e.driver == p_driver) for (Owner &o : e.owners) if (!o.live) { o = {p_owner, true}; return serial; }
		return serial;
	}
	uint64_t resolve_owner(uint64_t p_device, uint64_t p_driver, uint64_t p_owner) const {
		for (const Entry &e : entries) if (e.physically_live && e.device == p_device && e.driver == p_driver) for (const Owner &o : e.owners) if (o.live && o.owner == p_owner) return e.serial;
		return 0;
	}
	bool logical_free_owner(uint64_t p_device, uint64_t p_driver, uint64_t p_owner) {
		for (Entry &e : entries) if (e.physically_live && e.device == p_device && e.driver == p_driver) for (Owner &o : e.owners) if (o.live && o.owner == p_owner) { o.live = false; return true; }
		return false;
	}
	bool physical_dispose(uint64_t p_device, uint64_t p_driver) {
		for (Entry &e : entries) if (e.physically_live && e.device == p_device && e.driver == p_driver) { e.physically_live = false; e.references = 0; e.device = e.driver = e.serial = 0; for (Owner &o : e.owners) o.live = false; return true; }
		return false;
	}
	uint64_t resolve(uint64_t p_device, uint64_t p_driver) const {
		for (const Entry &e : entries) if (e.references != 0 && e.device == p_device && e.driver == p_driver) return e.serial;
		return 0;
	}
	bool resolve_serial(uint64_t p_device, uint64_t p_driver, uint64_t p_serial) const {
		for (const Entry &e : entries) if (e.references != 0 && e.device == p_device && e.driver == p_driver && e.serial == p_serial) return true;
		return false;
	}
	bool release(uint64_t p_device, uint64_t p_driver) {
		for (Entry &e : entries) if (e.references != 0 && e.device == p_device && e.driver == p_driver) { if (--e.references == 0) { e.device = e.driver = e.serial = 0; } return true; }
		return false;
	}
	bool release_alias(uint64_t p_device, uint64_t p_driver) { return release(p_device, p_driver); }
	bool release_serial(uint64_t p_device, uint64_t p_driver, uint64_t p_serial) {
		for (Entry &e : entries) if (e.references != 0 && e.device == p_device && e.driver == p_driver && e.serial == p_serial) {
			if (--e.references == 0) e.device = e.driver = e.serial = 0;
			return true;
		}
		return false;
	}
};


class P01WritePrefix {
	uint64_t device = 0;
	uint64_t driver = 0;
	uint32_t bytes = 0;
	bool active = false;
	bool failed = false;
public:
	bool begin(uint64_t p_device, uint64_t p_driver) {
		if (active || p_device == 0 || p_driver == 0) return false;
		device = p_device; driver = p_driver; bytes = 0; failed = false; active = true; return true;
	}
	bool record(uint32_t p_offset, uint32_t p_size) {
		if (!active || failed || p_offset != bytes || p_size == 0 || UINT32_MAX - bytes < p_size) return false;
		bytes += p_size; return true;
	}
	bool record_failure(uint32_t p_offset) {
		if (!active || failed || p_offset != bytes) return false;
		failed = true; return false;
	}
	uint32_t recorded_bytes() const { return bytes; }
	bool has_failed_prefix() const { return failed && bytes != 0; }
};


struct P01HookSnapshot {
	uint64_t write_begin_count = 0;
	uint64_t write_bytes = 0;
	uint64_t write_failure_count = 0;
	uint64_t readback_queued_count = 0;
	uint64_t readback_completed_count = 0;
};
void p01_observe_buffer_write(uint64_t p_rid, uint32_t p_offset, uint32_t p_size, bool p_success, uint64_t p_device = 0, uint64_t p_driver = 0, uint64_t p_serial = 0, bool p_failed_prefix = false);
void p01_observe_buffer_allocation(uint64_t p_rid, uint64_t p_device, uint64_t p_driver, uint64_t p_serial = 0);
void p01_observe_buffer_free(uint64_t p_rid, uint64_t p_device = 0, uint64_t p_driver = 0, uint64_t p_serial = 0);
void p01_observe_buffer_dispose(uint64_t p_device, uint64_t p_driver, uint64_t p_serial);
void p01_observe_buffer_clear(uint64_t p_rid, uint32_t p_offset, uint32_t p_size, uint64_t p_device = 0, uint64_t p_driver = 0, uint64_t p_serial = 0);
void p01_observe_buffer_copy(uint64_t p_src_rid, uint64_t p_dst_rid, uint32_t p_src_offset, uint32_t p_dst_offset, uint32_t p_size, uint64_t p_device = 0, uint64_t p_source_serial = 0, uint64_t p_destination_serial = 0);
void p01_observe_readback_queued(uint64_t p_rid, uint32_t p_size);
void p01_observe_readback_completed(uint64_t p_rid, uint32_t p_size);
// Main publication only; never an RT upload or GPU completion.
void p01_observe_skeleton_signal(uint64_t p_object, bool p_after);
void p01_observe_skeleton_palette_request(uint64_t p_object, uint64_t p_skeleton, uint32_t p_bind);
void p01_observe_skeleton_setter(uint64_t p_skeleton, uint64_t p_version);
void p01_observe_skeleton_upload(uint64_t p_skeleton, uint64_t p_buffer, uint32_t p_size, int32_t p_error);
void p01_observe_skeleton_version(uint64_t p_skeleton, uint64_t p_version);
P01HookSnapshot p01_hook_snapshot();
std::vector<P01HookEvent> p01_hook_events();
void p01_hook_begin(uint64_t p_epoch, uint64_t p_allocation_serial = 0);
bool p01_hook_is_poisoned();
void p01_hook_reset();

// Conservative whole-allocation hazard scan; sequence is publication, not GPU completion.
bool p01_hook_buffer_unchanged(uint64_t p_device, uint64_t p_allocation, uint64_t p_after_sequence, uint64_t p_epoch);

// Same-thread scope around one original RD operation. Foreign publishers never
// inherit it; nested scopes get distinct tokens and cannot satisfy their parent.
class P01RecordingScope {
 uint64_t previous = 0, token = 0;
public:
 P01RecordingScope();
 ~P01RecordingScope();
 P01RecordingScope(const P01RecordingScope &) = delete;
 P01RecordingScope &operator=(const P01RecordingScope &) = delete;
 uint64_t identity() const { return token; }
};
P01HookEvent p01_observe_draw_record(uint64_t p_device, uint64_t p_mesh_instance, uint64_t p_vertex_array);

void p01_observe_driver_result(uint64_t p_device, uint32_t p_operation, uint64_t p_fence, int32_t p_error);
