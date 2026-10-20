#pragma once

#include <cstdint>
#include <array>
#include <vector>

struct P01HookEvent {
	uint64_t rid = 0, epoch = 0, allocation_serial = 0, sequence = 0;
	uint32_t offset = 0, size = 0, range = 0;
	bool success = false, failed_prefix = false;
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

public:
	bool accept(uint64_t p_epoch) {
		if (accepted || closing || p_epoch == 0) {
			return false;
		}
		epoch = p_epoch;
		accepted = true;
		return true;
	}
	bool record(uint64_t p_epoch) {
		if (!accepted || closing || capture_poisoned || p_epoch != epoch || pending == UINT32_MAX) {
			return false;
		}
		++pending;
		return true;
	}
	bool complete_readback(uint64_t p_epoch) {
		if (!accepted || p_epoch != epoch || pending == 0) {
			return false;
		}
		--pending;
		return true;
	}
	bool record_slot(uint64_t p_epoch, uint32_t p_slot) {
		if (p_slot >= slots.size() || !accepted || closing || p_epoch != epoch || slots[p_slot]) {
			return false;
		}
		if (!record(p_epoch)) {
			return false;
		}
		--pending;
		++slot_pending;
		slots[p_slot] = true;
		return true;
	}
	bool complete_slot(uint64_t p_epoch, uint32_t p_slot) {
		if (p_slot >= slots.size() || p_epoch != epoch || !slots[p_slot]) return false;
		if (slot_pending == 0) return false;
		slots[p_slot] = false;
		--slot_pending;
		return true;
	}
	void close() { closing = true; }
	void poison_capture() { capture_poisoned = true; }
	void device_failure() { capture_poisoned = true; }
	bool is_capture_poisoned() const { return capture_poisoned; }
	void release_native_references() { references_released = true; }
	bool can_retire() const { return accepted && closing && pending == 0 && slot_pending == 0 && references_released; }
};


class P01AllocationIdentity {
	struct Entry { uint64_t device = 0; uint64_t driver = 0; uint64_t serial = 0; bool live = false; };
	std::array<Entry, 64> entries{};
	uint64_t next_serial = 1;
public:
	uint64_t acquire(uint64_t p_device, uint64_t p_driver) {
		if (p_device == 0 || p_driver == 0) return 0;
		for (const Entry &e : entries) if (e.live && e.device == p_device && e.driver == p_driver) return 0;
		for (Entry &e : entries) if (!e.live) { e = {p_device, p_driver, next_serial++, true}; return e.serial; }
		return 0;
	}
	uint64_t resolve(uint64_t p_device, uint64_t p_driver) const {
		for (const Entry &e : entries) if (e.live && e.device == p_device && e.driver == p_driver) return e.serial;
		return 0;
	}
	bool release(uint64_t p_device, uint64_t p_driver) {
		for (Entry &e : entries) if (e.live && e.device == p_device && e.driver == p_driver) { e.live = false; return true; }
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
void p01_observe_buffer_write(uint64_t p_rid, uint32_t p_offset, uint32_t p_size, bool p_success);
void p01_observe_readback_queued(uint64_t p_rid, uint32_t p_size);
void p01_observe_readback_completed(uint64_t p_rid, uint32_t p_size);
P01HookSnapshot p01_hook_snapshot();
std::vector<P01HookEvent> p01_hook_events();
void p01_hook_reset();
