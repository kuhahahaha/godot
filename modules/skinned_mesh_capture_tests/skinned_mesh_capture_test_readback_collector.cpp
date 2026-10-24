/**************************************************************************/
/*  skinned_mesh_capture_test_readback_collector.cpp                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "skinned_mesh_capture_test_readback_collector.h"

#include "core/io/marshalls.h"

bool SkinnedMeshCaptureTestReadbackIdentity::operator==(const SkinnedMeshCaptureTestReadbackIdentity &p_other) const {
	return run == p_other.run && epoch == p_other.epoch && token == p_other.token && queue == p_other.queue && device == p_other.device && allocation == p_other.allocation && rid == p_other.rid && owner == p_other.owner && alias == p_other.alias && slot == p_other.slot && consumer == p_other.consumer;
}

struct SkinnedMeshCaptureTestReadbackCollector::State {
	struct Entry {
		SkinnedMeshCaptureTestReadbackIdentity identity;
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
	uint32_t holders = 0;
	bool closing = false, poisoned = false;
};

class SkinnedMeshCaptureTestReadbackCollector::Callback : public CallableCustom {
	std::shared_ptr<State> state;
	SkinnedMeshCaptureTestReadbackIdentity identity;
	static bool equal(const CallableCustom *p_a, const CallableCustom *p_b) { return p_a == p_b; }
	static bool less(const CallableCustom *p_a, const CallableCustom *p_b) { return std::less<const CallableCustom *>()(p_a, p_b); }

public:
	Callback(std::shared_ptr<State> p_state, const SkinnedMeshCaptureTestReadbackIdentity &p_identity) : state(p_state), identity(p_identity) {}
	~Callback() {
		std::lock_guard<std::mutex> lock(state->mutex);
		--state->holders;
	}
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

SkinnedMeshCaptureTestReadbackCollector::SkinnedMeshCaptureTestReadbackCollector(uint64_t p_run, uint64_t p_epoch, bool p_dual_view) : state(std::make_shared<State>()) {
	state->run = p_run;
	state->epoch = p_epoch;
	state->capacity = p_dual_view ? 18 : 8;
	state->poisoned = p_run == 0 || p_epoch == 0;
}

Error SkinnedMeshCaptureTestReadbackCollector::reserve(const SkinnedMeshCaptureTestReadbackIdentity &p_identity, Callable &r_callback) {
	// The output is an out-parameter. Rejecting a live Callable before taking
	// state->mutex prevents its destructor from re-entering the same mutex when
	// a caller accidentally reuses an occupied output variable.
	if (!r_callback.is_null()) {
		return ERR_INVALID_PARAMETER;
	}
	std::lock_guard<std::mutex> lock(state->mutex);
	if (state->closing || state->poisoned) {
		return ERR_UNAVAILABLE;
	}
	if (p_identity.run != state->run || p_identity.epoch != state->epoch || p_identity.slot >= state->capacity || !p_identity.token || !p_identity.queue || !p_identity.device || !p_identity.allocation || !p_identity.rid || !p_identity.owner || !p_identity.alias) {
		return ERR_INVALID_PARAMETER;
	}
	for (const State::Entry &entry : state->entries) {
		if (entry.reserved && (entry.identity == p_identity)) {
			return ERR_ALREADY_IN_USE;
		}
	}
	State::Entry &entry = state->entries[p_identity.slot * 2 + uint32_t(p_identity.consumer)];
	if (entry.reserved) {
		return ERR_ALREADY_IN_USE;
	}
	entry.identity = p_identity;
	entry.reserved = true;
	++state->pending;
	++state->holders;
	r_callback = Callable(memnew(Callback(state, p_identity)));
	return OK;
}

Error SkinnedMeshCaptureTestReadbackCollector::copy_completed(const SkinnedMeshCaptureTestReadbackIdentity &p_identity, PackedByteArray &r_bytes) const {
	std::lock_guard<std::mutex> lock(state->mutex);
	return copy_completed_locked(p_identity, r_bytes);
}
Error SkinnedMeshCaptureTestReadbackCollector::copy_completed_locked(const SkinnedMeshCaptureTestReadbackIdentity &p_identity, PackedByteArray &r_bytes) const {
	if (state->poisoned) {
		return ERR_INVALID_DATA;
	}
	if (p_identity.slot >= state->capacity) {
		return ERR_INVALID_PARAMETER;
	}
	const State::Entry &entry = state->entries[p_identity.slot * 2 + uint32_t(p_identity.consumer)];
	if (!entry.reserved || !(entry.identity == p_identity)) {
		return ERR_INVALID_PARAMETER;
	}
	if (entry.canceled) {
		return ERR_INVALID_DATA;
	}
	if (!entry.completed) {
		return ERR_BUSY;
	}
	if (!entry.enqueued) {
		return ERR_INVALID_DATA;
	}
	if (entry.bytes.size() != 64) {
		return ERR_INVALID_DATA;
	}
	r_bytes = entry.bytes.duplicate();
	return OK;
}
void SkinnedMeshCaptureTestReadbackCollector::close() {
	std::lock_guard<std::mutex> lock(state->mutex);
	state->closing = true;
}
bool SkinnedMeshCaptureTestReadbackCollector::drained() const {
	std::lock_guard<std::mutex> lock(state->mutex);
	return state->closing && state->pending == 0 && state->holders == 0;
}
bool SkinnedMeshCaptureTestReadbackCollector::poisoned() const {
	std::lock_guard<std::mutex> lock(state->mutex);
	return state->poisoned;
}

Error SkinnedMeshCaptureTestReadbackCollector::cancel_unqueued(const SkinnedMeshCaptureTestReadbackIdentity &p_identity) {
	std::lock_guard<std::mutex> lock(state->mutex);
	if (p_identity.slot >= state->capacity) {
		return ERR_INVALID_PARAMETER;
	}
	State::Entry &entry = state->entries[p_identity.slot * 2 + uint32_t(p_identity.consumer)];
	if (!entry.reserved || !(entry.identity == p_identity) || entry.completed || entry.enqueued || entry.canceled) {
		return ERR_INVALID_PARAMETER;
	}
	entry.canceled = true;
	--state->pending;
	state->poisoned = true;
	return OK;
}

Error SkinnedMeshCaptureTestReadbackCollector::mark_enqueued(const SkinnedMeshCaptureTestReadbackIdentity &p_identity) {
	std::lock_guard<std::mutex> lock(state->mutex);
	if (p_identity.slot >= state->capacity) {
		return ERR_INVALID_PARAMETER;
	}
	State::Entry &entry = state->entries[p_identity.slot * 2 + uint32_t(p_identity.consumer)];
	if (!entry.reserved || !(entry.identity == p_identity) || entry.canceled) {
		return ERR_INVALID_PARAMETER;
	}
	if (entry.enqueued) {
		state->poisoned = true;
		return ERR_INVALID_PARAMETER;
	}
	entry.enqueued = true;
	return OK;
}

Error SkinnedMeshCaptureTestReadbackCollector::validate_pair(const SkinnedMeshCaptureTestReadbackIdentity &p_snapshot, const SkinnedMeshCaptureTestReadbackIdentity &p_consumer) const {
	std::lock_guard<std::mutex> lock(state->mutex);
	if (p_snapshot.consumer || !p_consumer.consumer || p_snapshot.slot != p_consumer.slot || p_snapshot.run != p_consumer.run || p_snapshot.epoch != p_consumer.epoch) {
		return ERR_INVALID_PARAMETER;
	}
	PackedByteArray snapshot, consumer;
	Error error = copy_completed_locked(p_snapshot, snapshot);
	if (error != OK) {
		return error;
	}
	error = copy_completed_locked(p_consumer, consumer);
	if (error != OK) {
		return error;
	}
	for (int word = 0; word < 16; word++) {
		const uint32_t a = decode_uint32(snapshot.ptr() + word * 4);
		const uint32_t b = decode_uint32(consumer.ptr() + word * 4);
		if (a != b || (word >= 9 && a != 0x7fc12345u)) {
			return ERR_INVALID_DATA;
		}
	}
	return OK;
}
