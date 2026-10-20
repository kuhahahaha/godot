#pragma once

#include <cstdint>
#include <array>

// Engine-owned lifetime accounting. Transport/parent verdicts never release GPU
// work. The caller serializes access on the collector's owning thread.
class P01CollectorLifetime {
	uint64_t epoch = 0;
	uint32_t pending = 0;
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
		if (!accepted || closing || p_epoch != epoch || pending == UINT32_MAX) {
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
		slots[p_slot] = true;
		return true;
	}
	bool complete_slot(uint64_t p_epoch, uint32_t p_slot) {
		if (p_slot >= slots.size() || p_epoch != epoch || !slots[p_slot]) return false;
		slots[p_slot] = false;
		return complete_readback(p_epoch);
	}
	void close() { closing = true; }
	void poison_capture() { capture_poisoned = true; }
	bool is_capture_poisoned() const { return capture_poisoned; }
	void release_native_references() { references_released = true; }
	bool can_retire() const { return accepted && closing && pending == 0 && references_released; }
};
