#pragma once
#include <cstddef>
#include <cstdint>

struct P01ReceiverState {
	uint64_t bytes = 0;
	bool saw_seal = false;
	bool saw_eof = false;
	bool poisoned = false;
};

class P01ReceiverContract {
	P01ReceiverState state;
public:
	bool accept_bytes(const uint8_t *p_data, size_t p_size);
	bool accept_seal();
	bool accept_eof();
	bool finish(bool p_process_ok);
	const P01ReceiverState &get_state() const { return state; }
};
