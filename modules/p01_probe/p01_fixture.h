#pragma once

#include <cstdint>

#include "p01_capture.h"

// Tests-only deterministic native fixture.  It exercises the private capture
// and kernel primitives without exposing a ClassDB or script-facing evidence
// API.  Real RenderingDevice acquisition is owned by the production hooks and
// remains a separate acceptance layer.
struct P01FixtureResult {
	uint64_t epoch = 0;
	uint32_t action_count = 0;
	uint32_t last_value = 0;
	bool capture_poisoned = false;
	bool disposed = false;
};

class P01Fixture {
	P01Capture capture;
	P01FixtureResult result;
	uint64_t next_token = 1;
	uint64_t next_queue_sequence = 1;
	bool begun = false;
	bool closed = false;

public:
	bool begin(uint64_t p_epoch);
	bool advance_action(uint32_t p_value);
	P01FixtureResult take_result() const;
	bool close();
};
