#include "receiver_contract.h"

bool P01ReceiverContract::accept_bytes(const uint8_t *p_data, size_t p_size) {
	if (state.saw_seal || state.poisoned || (p_size && !p_data)) { state.poisoned = true; return false; }
	state.bytes += p_size;
	return true;
}

bool P01ReceiverContract::accept_seal() {
	if (state.poisoned || state.saw_seal) { state.poisoned = true; return false; }
	state.saw_seal = true;
	return true;
}

bool P01ReceiverContract::accept_eof() {
	if (state.poisoned || !state.saw_seal || state.saw_eof) { state.poisoned = true; return false; }
	state.saw_eof = true;
	return true;
}

bool P01ReceiverContract::finish(bool p_process_ok) {
	if (state.poisoned || !p_process_ok || !state.saw_seal || !state.saw_eof) return false;
	return true;
}
