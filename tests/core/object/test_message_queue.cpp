/**************************************************************************/
/*  test_message_queue.cpp                                                */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_message_queue)

#include "core/object/callable_mp.h"
#include "core/object/message_queue.h"
#include "core/object/ref_counted.h"

namespace TestMessageQueue {

class QueueAccessor {
public:
	static int page_argument_capacity() { return (CallQueue::PAGE_SIZE_BYTES - sizeof(CallQueue::Message)) / sizeof(Variant); }
	static void discard_destroyed_records(CallQueue &p_queue) {
		// Failure-only recovery for the intentionally destructive baseline.
		// A passing implementation never calls this helper.
		for (uint32_t i = 0; i < p_queue.page_bytes.size(); i++) {
			p_queue.page_bytes[i] = 0;
		}
		p_queue.pages_used = p_queue.pages.is_empty() ? 0 : 1;
	}
};

class DestructionProbe : public RefCounted {
	int *destructions = nullptr;

public:
	explicit DestructionProbe(int *p_destructions) :
			destructions(p_destructions) {}
	~DestructionProbe() { ++*destructions; }
};

class CountingTarget : public Object {
public:
	int calls = 0;
	void receive(Variant p_value) {
		calls++;
		set_meta("queued_probe", p_value);
	}
};

static ObjectID enqueue_probe(CallQueue &p_queue, CountingTarget &p_owner, int &r_destructions) {
	Ref<DestructionProbe> probe = memnew(DestructionProbe(&r_destructions));
	ObjectID id = probe->get_instance_id();
	CHECK(p_queue.push_callable(callable_mp(&p_owner, &CountingTarget::receive), probe) == OK);
	return id;
}

TEST_CASE("[CallQueue] Statistics preserves queued payload ownership") {
	int destructions = 0;
	CountingTarget owner;
	CallQueue queue;
	const ObjectID id = enqueue_probe(queue, owner, destructions);
	REQUIRE(destructions == 0);
	for (int i = 0; i < 2; i++) {
		queue.statistics();
		CHECK(destructions == 0);
		if (destructions != 0) {
			QueueAccessor::discard_destroyed_records(queue);
			return;
		}
		CHECK(queue.has_messages());
		CHECK_FALSE(owner.has_meta("queued_probe"));
	}
	CHECK(queue.flush() == OK);
	CHECK_FALSE(queue.has_messages());
	CHECK(owner.calls == 1);
	CHECK(destructions == 0);
	CHECK(ObjectID(owner.get_meta("queued_probe")) == id);
	owner.remove_meta("queued_probe");
	CHECK(destructions == 1);
	queue.statistics();
	CHECK(queue.flush() == OK);
	CHECK(destructions == 1);
}

class CloseProbe : public Object {
	CallQueue *queue = nullptr;

public:
	Error during_close = ERR_INVALID_PARAMETER;
	Error enqueue_after_busy = ERR_INVALID_PARAMETER;
	bool clear_left_work = false;
	int notifications = 0;

	void receive() {
		during_close = queue->prepare_close();
		enqueue_after_busy = queue->push_callable(callable_mp(this, &CloseProbe::following));
		queue->clear();
		clear_left_work = queue->has_messages();
	}
	void following() { notifications++; }
	void set_queue(CallQueue *p_queue) { queue = p_queue; }
};

TEST_CASE("[CallQueue] Prepare close rejects public work and protects active flush") {
	CallQueue queue;
	CHECK(queue.prepare_close() == OK);
	CHECK(queue.get_max_buffer_usage() == 0);
	CHECK(queue.push_notification(ObjectID(), 7) == ERR_UNAVAILABLE);
	CHECK(queue.push_set(ObjectID(), "closed", 7) == ERR_UNAVAILABLE);
	CHECK(queue.push_callable(Callable(), 7) == ERR_UNAVAILABLE);
	CHECK(queue.flush() == ERR_UNAVAILABLE);
	CHECK(queue.get_max_buffer_usage() == 0);
	CHECK(queue.prepare_close() == OK);
	queue.clear();

	CallQueue active_queue;
	CloseProbe probe;
	probe.set_queue(&active_queue);
	CHECK(active_queue.push_callable(callable_mp(&probe, &CloseProbe::receive)) == OK);
	CHECK(active_queue.flush() == OK);
	CHECK(probe.during_close == ERR_BUSY);
	CHECK(probe.enqueue_after_busy == OK);
	CHECK(probe.clear_left_work == true);
	CHECK(probe.notifications == 1);
	CHECK(active_queue.prepare_close() == OK);
	CHECK(active_queue.push_notification(ObjectID(), 8) == ERR_UNAVAILABLE);
	CHECK(active_queue.flush() == ERR_UNAVAILABLE);
	active_queue.clear();
}

TEST_CASE("[CallQueue] Closed queue rejects preconstructed payloads") {
	CallQueue queue;
	Object owner;
	int destructions = 0;
	queue.prepare_close();
	Ref<DestructionProbe> probe = memnew(DestructionProbe(&destructions));
	Variant payload = probe;
	const Variant *args[] = { &payload };
	CHECK(queue.push_callablep(Callable(&owner, "set_meta"), args, 1) == ERR_UNAVAILABLE);
	CHECK(queue.push_set(owner.get_instance_id(), "closed_payload", payload) == ERR_UNAVAILABLE);
	CHECK(destructions == 0);
	payload = Variant();
	probe.unref();
	CHECK(destructions == 1);
	CHECK(queue.get_max_buffer_usage() == 0);
}

TEST_CASE("[CallQueue] Closed queue clears accepted work") {
	CallQueue queue;
	Object owner;
	int destructions = 0;
	Ref<DestructionProbe> probe = memnew(DestructionProbe(&destructions));
	CHECK(queue.push_callable(Callable(&owner, "set_meta"), StringName("closed_payload"), probe) == OK);
	CHECK(queue.prepare_close() == OK);
	probe.unref();
	CHECK(destructions == 0);
	queue.clear();
	CHECK(destructions == 1);
	CHECK_FALSE(queue.has_messages());
	CHECK(queue.flush() == ERR_UNAVAILABLE);
}

struct HookCounts {
	int object = 0;
	int hash = 0;
	int text = 0;
	int call = 0;
};

class MetadataCallable : public CallableCustom {
	ObjectID owner;
	HookCounts *counts;
	bool store_last;

public:
	MetadataCallable(ObjectID p_owner, HookCounts *p_counts, bool p_store_last = false) :
			owner(p_owner), counts(p_counts), store_last(p_store_last) {}
	uint32_t hash() const override {
		++counts->hash;
		return 42;
	}
	String get_as_text() const override {
		++counts->text;
		return "queue statistics hook witness";
	}
	CompareEqualFunc get_compare_equal_func() const override { return nullptr; }
	CompareLessFunc get_compare_less_func() const override { return nullptr; }
	ObjectID get_object() const override {
		++counts->object;
		return owner;
	}
	void call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, Callable::CallError &r_error) const override {
		++counts->call;
		if (store_last && p_argcount > 0) {
			ObjectDB::get_instance(owner)->set_meta("queued_probe", *p_arguments[p_argcount - 1]);
		}
		r_error.error = Callable::CallError::CALL_OK;
	}
};

TEST_CASE("[CallQueue] Capacity diagnostics preserve accepted work") {
	int operation = 0;
	SUBCASE("call") {
		operation = 0;
	}
	SUBCASE("set") {
		operation = 1;
	}
	SUBCASE("notification") {
		operation = 2;
	}
	int destructions = 0;
	HookCounts counts;
	Object owner;
	CallQueue queue(nullptr, 1);
	Callable callable(memnew(MetadataCallable(owner.get_instance_id(), &counts, true)));
	ObjectID id;
	{
		const int count = QueueAccessor::page_argument_capacity();
		Vector<Variant> arguments;
		Vector<const Variant *> pointers;
		arguments.resize(count);
		pointers.resize(count);
		for (int i = 0; i < count; i++) {
			arguments.write[i] = 17;
			pointers.write[i] = &arguments[i];
		}
		Ref<DestructionProbe> probe = memnew(DestructionProbe(&destructions));
		id = probe->get_instance_id();
		arguments.write[count - 1] = probe;
		REQUIRE(queue.push_callablep(callable, pointers.ptrw(), count) == OK);
	}
	REQUIRE(destructions == 0);
	counts = HookCounts();
	Error result;
	if (operation == 0) {
		result = queue.push_callable(callable);
	} else if (operation == 1) {
		result = queue.push_set(&owner, "metadata/filler", 17);
	} else {
		result = queue.push_notification(&owner, 12345);
	}
	CHECK(result == ERR_OUT_OF_MEMORY);
	CHECK(counts.object == 0);
	CHECK(counts.hash == 0);
	CHECK(counts.text == 0);
	CHECK(destructions == 0);
	if (destructions != 0) {
		// Only one accepted Message; its last argument is this witness.
		// The baseline returned after destroying that argument and header.
		QueueAccessor::discard_destroyed_records(queue);
		return;
	}
	CHECK(queue.flush() == OK);
	CHECK(counts.call == 1);
	CHECK(ObjectID(owner.get_meta("queued_probe")) == id);
	CHECK(destructions == 0);
	owner.remove_meta("queued_probe");
	CHECK(destructions == 1);
}

TEST_CASE("[CallQueue] Statistics does not invoke callable metadata hooks") {
	int destructions = 0;
	HookCounts counts;
	Object owner;
	CallQueue queue;
	Callable callable(memnew(MetadataCallable(owner.get_instance_id(), &counts)));
	{
		Ref<DestructionProbe> probe = memnew(DestructionProbe(&destructions));
		CHECK(queue.push_callable(callable, probe) == OK);
	}
	counts = HookCounts();
	queue.statistics();
	CHECK(counts.object == 0);
	CHECK(counts.hash == 0);
	CHECK(counts.text == 0);
	CHECK(counts.call == 0);
	CHECK(destructions == 0);
	if (destructions != 0) {
		QueueAccessor::discard_destroyed_records(queue);
		return;
	}
	CHECK(queue.flush() == OK);
	CHECK(counts.call == 1);
	CHECK(destructions == 1);
}

} // namespace TestMessageQueue
