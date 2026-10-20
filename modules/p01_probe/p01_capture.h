#pragma once

#include <cstdint>
#include <vector>

struct P01CaptureEvent {
	uint64_t sequence = 0;
	uint64_t epoch = 0;
	uint64_t token = 0;
	uint64_t rid = 0;
	uint64_t allocation_serial = 0;
	uint64_t queue_sequence = 0;
	uint32_t offset = 0;
	uint32_t size = 0;
	uint8_t kind = 0;
};

class P01Capture {
	std::vector<P01CaptureEvent> events;
	struct Pending { uint64_t token, epoch, rid, serial, queue; };
	std::vector<Pending> pending;
	uint64_t epoch = 0;
	uint64_t next_sequence = 1;
	bool poisoned = false;
	bool sealed = false;
	bool disposed = false;
	static constexpr size_t MAX_EVENTS = 4096;
	static constexpr size_t MAX_PENDING = 64;
	public:
	P01Capture() = default;
	void begin(uint64_t p_epoch);
	bool append(const P01CaptureEvent &p_event);
	bool seal();
	bool dispose_physical();
	bool is_poisoned() const { return poisoned; }
	bool is_sealed() const { return sealed; }
	bool is_disposed() const { return disposed; }
	const std::vector<P01CaptureEvent> &get_events() const { return events; }
};
