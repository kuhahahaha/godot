#include "p01_readback_collector.h"
#include "core/io/marshalls.h"

bool P01ReadbackIdentity::operator==(const P01ReadbackIdentity &p_other) const {
	return run == p_other.run && epoch == p_other.epoch && token == p_other.token && queue == p_other.queue && device == p_other.device && allocation == p_other.allocation && rid == p_other.rid && owner == p_other.owner && alias == p_other.alias && slot == p_other.slot && consumer == p_other.consumer;
}

struct P01ReadbackCollector::State {
	struct Entry {
		P01ReadbackIdentity identity;
		PackedByteArray bytes;
		bool reserved = false;
		bool completed = false;
		bool enqueued = false;
		bool canceled = false;
	};
	std::mutex mutex;
	std::array<Entry, 36> entries;
	uint64_t run = 0, epoch = 0;
	uint32_t capacity = 0, pending = 0;
	bool closing = false, poisoned = false;
};

class P01ReadbackCollector::Callback : public CallableCustom {
	std::shared_ptr<State> state;
	P01ReadbackIdentity identity;
	static bool equal(const CallableCustom *p_a, const CallableCustom *p_b) { return p_a == p_b; }
	static bool less(const CallableCustom *p_a, const CallableCustom *p_b) { return std::less<const CallableCustom *>()(p_a, p_b); }

public:
	Callback(std::shared_ptr<State> p_state, const P01ReadbackIdentity &p_identity) : state(p_state), identity(p_identity) {}
	uint32_t hash() const override { return uint32_t(identity.token ^ (identity.token >> 32)); }
	String get_as_text() const override { return "P01 engine readback"; }
	CompareEqualFunc get_compare_equal_func() const override { return equal; }
	CompareLessFunc get_compare_less_func() const override { return less; }
	ObjectID get_object() const override { return ObjectID(); }
	bool is_valid() const override { return true; }
	void call(const Variant **p_arguments, int p_argcount, Variant &, Callable::CallError &r_error) const override {
		std::lock_guard<std::mutex> lock(state->mutex);
		r_error.error = Callable::CallError::CALL_OK;
		State::Entry &entry = state->entries[identity.slot * 2 + uint32_t(identity.consumer)];
		if (!entry.reserved || !(entry.identity == identity) || entry.completed || entry.canceled) {
			state->poisoned = true;
			return;
		}
		entry.completed = true;
		--state->pending;
		if (p_argcount != 1 || p_arguments[0]->get_type() != Variant::PACKED_BYTE_ARRAY) {
			state->poisoned = true;
			return;
		}
		PackedByteArray bytes = *p_arguments[0];
		// Receipt of malformed data still ends this callback, but invalidates
		// evidence. No timeout or transport verdict can synthesize completion.
		if (bytes.size() != 64) {
			state->poisoned = true;
			return;
		}
		entry.bytes = bytes.duplicate();
	}
};

P01ReadbackCollector::P01ReadbackCollector(uint64_t p_run, uint64_t p_epoch, bool p_dual_view) : state(std::make_shared<State>()) {
	state->run = p_run;
	state->epoch = p_epoch;
	state->capacity = p_dual_view ? 18 : 8;
	state->poisoned = p_run == 0 || p_epoch == 0;
}

Error P01ReadbackCollector::reserve(const P01ReadbackIdentity &p_identity, Callable &r_callback) {
	std::lock_guard<std::mutex> lock(state->mutex);
	if (state->closing || state->poisoned) { return ERR_UNAVAILABLE; }
	if (p_identity.run != state->run || p_identity.epoch != state->epoch || p_identity.slot >= state->capacity || !p_identity.token || !p_identity.queue || !p_identity.device || !p_identity.allocation || !p_identity.rid || !p_identity.owner || !p_identity.alias) { return ERR_INVALID_PARAMETER; }
	for (const State::Entry &entry : state->entries) {
		if (entry.reserved && (entry.identity == p_identity)) { return ERR_ALREADY_IN_USE; }
	}
	State::Entry &entry = state->entries[p_identity.slot * 2 + uint32_t(p_identity.consumer)];
	if (entry.reserved) { return ERR_ALREADY_IN_USE; }
	entry.identity = p_identity;
	entry.reserved = true;
	++state->pending;
	r_callback = Callable(memnew(Callback(state, p_identity)));
	return OK;
}

Error P01ReadbackCollector::copy_completed(const P01ReadbackIdentity &p_identity, PackedByteArray &r_bytes) const {
	std::lock_guard<std::mutex> lock(state->mutex);
	if (p_identity.slot >= state->capacity) { return ERR_INVALID_PARAMETER; }
	const State::Entry &entry = state->entries[p_identity.slot * 2 + uint32_t(p_identity.consumer)];
	if (!entry.reserved || !(entry.identity == p_identity)) { return ERR_INVALID_PARAMETER; }
	if (entry.canceled) { return ERR_INVALID_DATA; }
	if (!entry.completed) { return ERR_BUSY; }
	if (entry.bytes.size() != 64) { return ERR_INVALID_DATA; }
	r_bytes = entry.bytes.duplicate();
	return OK;
}
void P01ReadbackCollector::close() { std::lock_guard<std::mutex> lock(state->mutex); state->closing = true; }
bool P01ReadbackCollector::drained() const { std::lock_guard<std::mutex> lock(state->mutex); return state->closing && state->pending == 0; }
bool P01ReadbackCollector::poisoned() const { std::lock_guard<std::mutex> lock(state->mutex); return state->poisoned; }

Error P01ReadbackCollector::cancel_unqueued(const P01ReadbackIdentity &p_identity) {
	std::lock_guard<std::mutex> lock(state->mutex);
	if (p_identity.slot >= state->capacity) { return ERR_INVALID_PARAMETER; }
	State::Entry &entry = state->entries[p_identity.slot * 2 + uint32_t(p_identity.consumer)];
	if (!entry.reserved || !(entry.identity == p_identity) || entry.completed || entry.enqueued || entry.canceled) { return ERR_INVALID_PARAMETER; }
	entry.canceled = true;
	--state->pending;
	state->poisoned = true;
	return OK;
}

Error P01ReadbackCollector::mark_enqueued(const P01ReadbackIdentity &p_identity) {
	std::lock_guard<std::mutex> lock(state->mutex);
	if (p_identity.slot >= state->capacity) { return ERR_INVALID_PARAMETER; }
	State::Entry &entry = state->entries[p_identity.slot * 2 + uint32_t(p_identity.consumer)];
	if (!entry.reserved || !(entry.identity == p_identity) || entry.canceled || entry.enqueued) { return ERR_INVALID_PARAMETER; }
	entry.enqueued = true;
	return OK;
}

Error P01ReadbackCollector::validate_pair(const P01ReadbackIdentity &p_snapshot, const P01ReadbackIdentity &p_consumer) const {
	if (p_snapshot.consumer || !p_consumer.consumer || p_snapshot.slot != p_consumer.slot || p_snapshot.run != p_consumer.run || p_snapshot.epoch != p_consumer.epoch) { return ERR_INVALID_PARAMETER; }
	PackedByteArray snapshot, consumer;
	Error error = copy_completed(p_snapshot, snapshot);
	if (error != OK) { return error; }
	error = copy_completed(p_consumer, consumer);
	if (error != OK) { return error; }
	for (int word = 0; word < 16; word++) {
		const uint32_t a = decode_uint32(snapshot.ptr() + word * 4);
		const uint32_t b = decode_uint32(consumer.ptr() + word * 4);
		if (a != b || (word >= 9 && a != 0x7fc12345u)) { return ERR_INVALID_DATA; }
	}
	return OK;
}
