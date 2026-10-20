#include "p01_fixture.h"

#include "p01_kernels.h"

bool P01Fixture::begin(uint64_t p_epoch) {
	if (begun || closed || p_epoch == 0) {
		return false;
	}
	capture.begin(p_epoch);
	if (capture.is_poisoned()) {
		return false;
	}
	result = {};
	result.epoch = p_epoch;
	next_token = 1;
	next_queue_sequence = 1;
	begun = true;
	return true;
}


bool P01Fixture::advance_action(uint32_t p_value) {
	if (!begun || closed || result.action_count == UINT32_MAX) {
		return false;
	}
	const uint64_t token = next_token++;
	const uint64_t queue_sequence = next_queue_sequence++;
	const P01CaptureEvent queued{0, 1, token, result.epoch, 1, 1, 1, 1, queue_sequence, 0, sizeof(uint32_t), 0};
	if (!capture.append(queued)) {
		result.capture_poisoned = capture.is_poisoned();
		return false;
	}
	const uint32_t output = p01_kernel_identity({p_value});
	const P01CaptureEvent completed{0, 2, token, result.epoch, 1, 1, 1, 1, queue_sequence, 0, sizeof(uint32_t), output};
	if (!capture.append(completed)) {
		result.capture_poisoned = capture.is_poisoned();
		return false;
	}
	result.action_count++;
	result.last_value = output;
	return true;
}

P01FixtureResult P01Fixture::take_result() const {
	P01FixtureResult snapshot = result;
	snapshot.capture_poisoned = snapshot.capture_poisoned || capture.is_poisoned();
	snapshot.disposed = snapshot.disposed || capture.is_disposed();
	return snapshot;
}

bool P01Fixture::close() {
	if (!begun || closed || !capture.drain_complete()) {
		return false;
	}
	if (!capture.dispose_physical() || !capture.seal()) {
		result.capture_poisoned = capture.is_poisoned();
		return false;
	}
	closed = true;
	result.disposed = true;
	return true;
}
