/**************************************************************************/
/*  skinned_mesh_capture_test_image_collector.cpp                         */
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

#include "skinned_mesh_capture_test_image_collector.h"

struct SkinnedMeshCaptureTestImageCollector::State {
	struct Entry {
		bool reserved = false, complete = false, rejected = false;
		PackedByteArray bytes;
	};
	std::array<Entry, 18> entries;
	std::mutex mutex;
	uint32_t holders = 0;
	bool closed = false, poisoned = false;
};
class SkinnedMeshCaptureTestImageCollector::Callback : public CallableCustom {
	std::shared_ptr<State> state;
	uint32_t slot;
	static bool equal(const CallableCustom *p_a, const CallableCustom *p_b) { return p_a == p_b; }
	static bool less(const CallableCustom *p_a, const CallableCustom *p_b) { return std::less<const CallableCustom *>()(p_a, p_b); }

public:
	Callback(std::shared_ptr<State> p_state, uint32_t p_slot) : state(p_state), slot(p_slot) {}
	~Callback() {
		std::lock_guard<std::mutex> lock(state->mutex);
		--state->holders;
	}
	uint32_t hash() const override { return slot; }
	String get_as_text() const override { return "P01 engine full image"; }
	CompareEqualFunc get_compare_equal_func() const override { return equal; }
	CompareLessFunc get_compare_less_func() const override { return less; }
	ObjectID get_object() const override { return ObjectID(); }
	bool is_valid() const override { return true; }
	void call(const Variant **p_arguments, int p_argcount, Variant &, Callable::CallError &r_error) const override {
		std::lock_guard<std::mutex> lock(state->mutex);
		r_error.error = Callable::CallError::CALL_OK;
		auto &entry = state->entries[slot];
		if (!entry.reserved || entry.complete || entry.rejected) {
			state->poisoned = true;
			return;
		}
		entry.complete = true;
		if (p_argcount != 1 || p_arguments[0]->get_type() != Variant::PACKED_BYTE_ARRAY) {
			state->poisoned = true;
			return;
		}
		PackedByteArray bytes = *p_arguments[0];
		if (bytes.size() != 128 * 64 * 4) {
			state->poisoned = true;
			return;
		}
		entry.bytes = bytes.duplicate();
	}
};
SkinnedMeshCaptureTestImageCollector::SkinnedMeshCaptureTestImageCollector() : state(std::make_shared<State>()) {}
Error SkinnedMeshCaptureTestImageCollector::reserve(uint32_t p_slot, Callable &r_callback) {
	if (p_slot >= 18 || !r_callback.is_null()) {
		return ERR_INVALID_PARAMETER;
	}
	std::lock_guard<std::mutex> lock(state->mutex);
	if (state->closed || state->poisoned) {
		return ERR_UNAVAILABLE;
	}
	if (state->entries[p_slot].reserved) {
		return ERR_ALREADY_IN_USE;
	}
	state->entries[p_slot].reserved = true;
	++state->holders;
	r_callback = Callable(memnew(Callback(state, p_slot)));
	return OK;
}
Error SkinnedMeshCaptureTestImageCollector::reject(uint32_t p_slot) {
	std::lock_guard<std::mutex> lock(state->mutex);
	if (p_slot >= 18 || !state->entries[p_slot].reserved || state->entries[p_slot].complete || state->entries[p_slot].rejected) {
		state->poisoned = true;
		return ERR_INVALID_PARAMETER;
	}
	state->entries[p_slot].rejected = true;
	return OK;
}
Error SkinnedMeshCaptureTestImageCollector::copy(uint32_t p_slot, PackedByteArray &r_bytes) const {
	std::lock_guard<std::mutex> lock(state->mutex);
	if (state->poisoned) {
		return ERR_INVALID_DATA;
	}
	if (p_slot >= 18 || !state->entries[p_slot].reserved) {
		return ERR_INVALID_PARAMETER;
	}
	const auto &entry = state->entries[p_slot];
	if (entry.rejected) {
		return ERR_UNAVAILABLE;
	}
	if (!entry.complete) {
		return ERR_BUSY;
	}
	r_bytes = entry.bytes.duplicate();
	return OK;
}
void SkinnedMeshCaptureTestImageCollector::close() {
	std::lock_guard<std::mutex> lock(state->mutex);
	state->closed = true;
}
bool SkinnedMeshCaptureTestImageCollector::drained() const {
	std::lock_guard<std::mutex> lock(state->mutex);
	if (!state->closed || state->holders) {
		return false;
	}
	for (const auto &entry : state->entries) {
		if (entry.reserved && !entry.complete && !entry.rejected) {
			return false;
		}
	}
	return true;
}
bool SkinnedMeshCaptureTestImageCollector::poisoned() const {
	std::lock_guard<std::mutex> lock(state->mutex);
	return state->poisoned;
}
