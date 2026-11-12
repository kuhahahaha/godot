/**************************************************************************/
/*  test_object_lifetime.cpp                                              */
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

TEST_FORCE_LINK(test_object_lifetime)

#include "core/object/callable_mp.h"
#include "core/object/object.h"

namespace TestObjectLifetime {

struct DeletionObservations {
	int predelete_calls = 0;
	int cleanup_calls = 0;
	int destructor_calls = 0;
	bool nested_accepted = false;
};

class DeletionProbe : public Object {
	GDCLASS(DeletionProbe, Object);

	DeletionObservations *observations = nullptr;
	bool entered_nested = false;

protected:
	static void _bind_methods() {}
	void _notification(int p_what) {
		if (p_what == NOTIFICATION_PREDELETE) {
			observations->predelete_calls++;
			if (cancel_deletion && !entered_nested) {
				cancel_free();
			}
			if (nest_from_predelete && !entered_nested) {
				entered_nested = true;
				observations->nested_accepted = predelete_handler(this);
			}
		} else if (p_what == NOTIFICATION_PREDELETE_CLEANUP) {
			observations->cleanup_calls++;
			if (nest_from_cleanup && !entered_nested) {
				entered_nested = true;
				observations->nested_accepted = predelete_handler(this);
			}
		}
	}

public:
	bool cancel_deletion = false;
	bool nest_from_predelete = false;
	bool nest_from_cleanup = false;

	explicit DeletionProbe(DeletionObservations *p_observations) : observations(p_observations) {}
	~DeletionProbe() { observations->destructor_calls++; }
};

TEST_CASE("[Object][Native lifetime] Nested predelete keeps one accepted deletion owner") {
	DeletionObservations observations;
	DeletionProbe *object = memnew(DeletionProbe(&observations));
	ObjectID id = object->get_instance_id();
	object->nest_from_predelete = true;
	memdelete(object);
	CHECK_FALSE(observations.nested_accepted);
	CHECK_EQ(observations.predelete_calls, 1);
	CHECK_EQ(observations.cleanup_calls, 1);
	CHECK_EQ(observations.destructor_calls, 1);
	CHECK(ObjectDB::get_instance(id) == nullptr);
}

TEST_CASE("[Object][Native lifetime] Nested predelete preserves outer cancellation") {
	DeletionObservations observations;
	DeletionProbe *object = memnew(DeletionProbe(&observations));
	ObjectID id = object->get_instance_id();
	object->cancel_deletion = true;
	object->nest_from_predelete = true;
	memdelete(object);
	CHECK_FALSE(observations.nested_accepted);
	CHECK_EQ(observations.predelete_calls, 1);
	CHECK_EQ(observations.cleanup_calls, 0);
	CHECK_EQ(observations.destructor_calls, 0);
	CHECK(ObjectDB::get_instance(id) == object);
	if (ObjectDB::get_instance(id) != object) {
		return; // Observe a failed cancellation without touching the deleted probe.
	}
	object->cancel_deletion = false;
	memdelete(object);
	CHECK_EQ(observations.predelete_calls, 2);
	CHECK_EQ(observations.cleanup_calls, 1);
	CHECK_EQ(observations.destructor_calls, 1);
	CHECK(ObjectDB::get_instance(id) == nullptr);
}

TEST_CASE("[Object][Native lifetime] Accepted cleanup cannot create a second deletion owner") {
	DeletionObservations observations;
	DeletionProbe *object = memnew(DeletionProbe(&observations));
	ObjectID id = object->get_instance_id();
	object->nest_from_cleanup = true;
	memdelete(object);
	CHECK_FALSE(observations.nested_accepted);
	CHECK_EQ(observations.predelete_calls, 1);
	CHECK_EQ(observations.cleanup_calls, 1);
	CHECK_EQ(observations.destructor_calls, 1);
	CHECK(ObjectDB::get_instance(id) == nullptr);
}

TEST_CASE("[Object][Native lifetime] Canceled predelete releases the deletion gate for retry") {
	DeletionObservations observations;
	DeletionProbe *object = memnew(DeletionProbe(&observations));
	ObjectID id = object->get_instance_id();
	object->cancel_deletion = true;
	memdelete(object);
	CHECK_EQ(observations.predelete_calls, 1);
	CHECK_EQ(observations.cleanup_calls, 0);
	CHECK_EQ(observations.destructor_calls, 0);
	CHECK(ObjectDB::get_instance(id) == object);
	if (ObjectDB::get_instance(id) != object) {
		return; // Observe a failed cancellation without touching the deleted probe.
	}
	object->cancel_deletion = false;
	memdelete(object);
	CHECK_EQ(observations.predelete_calls, 2);
	CHECK_EQ(observations.cleanup_calls, 1);
	CHECK_EQ(observations.destructor_calls, 1);
	CHECK(ObjectDB::get_instance(id) == nullptr);
}

} // namespace TestObjectLifetime

class RegistrationProbe : public Object {
	GDCLASS(RegistrationProbe, Object);

protected:
	static void _bind_methods() {}
	void _notification(int p_what) {
		if (p_what == NOTIFICATION_PREDELETE) {
			valid_in_predelete = registration->is_valid();
			if (cancel_deletion) {
				cancel_free();
			}
		} else if (p_what == NOTIFICATION_PREDELETE_CLEANUP) {
			*valid_in_cleanup = registration->is_valid();
			Object::NativeContinuation late(this);
			*late_valid = late.is_valid();
		}
	}

public:
	Object::NativeContinuation *registration = nullptr;
	bool cancel_deletion = false;
	bool valid_in_predelete = false;
	bool *valid_in_cleanup = nullptr;
	bool *late_valid = nullptr;
};

TEST_CASE("[Object][Native lifetime] Registration survives cancellation and expires before cleanup") {
	bool valid_in_cleanup = true;
	bool late_valid = true;
	RegistrationProbe *object = memnew(RegistrationProbe);
	Object::NativeContinuation registration(object);
	object->registration = &registration;
	object->valid_in_cleanup = &valid_in_cleanup;
	object->late_valid = &late_valid;
	object->cancel_deletion = true;
	memdelete(object);
	CHECK(object->valid_in_predelete);
	CHECK(registration.is_valid());
	object->cancel_deletion = false;
	memdelete(object);
	CHECK_FALSE(registration.is_valid());
	CHECK_FALSE(valid_in_cleanup);
	CHECK_FALSE(late_valid);
}

TEST_CASE("[Object][Native lifetime] Registration removal repairs both list directions") {
	Object *object = memnew(Object);
	Object::NativeContinuation *first = memnew(Object::NativeContinuation(object));
	Object::NativeContinuation *second = memnew(Object::NativeContinuation(object));
	SUBCASE("Remove oldest first") {
		memdelete(first);
		CHECK(second->is_valid());
		memdelete(object);
		CHECK_FALSE(second->is_valid());
		memdelete(second);
	}
	SUBCASE("Remove newest first") {
		memdelete(second);
		CHECK(first->is_valid());
		memdelete(object);
		CHECK_FALSE(first->is_valid());
		memdelete(first);
	}
}

TEST_CASE("[Object][Native lifetime] Stack Object base destruction invalidates surviving registrations") {
	Object::NativeContinuation *registration = nullptr;
	{
		Object object;
		registration = memnew(Object::NativeContinuation(&object));
		CHECK(registration->is_valid());
	}
	CHECK_FALSE(registration->is_valid());
	// If invalidation is mutated out, do not dereference the expired stack Object
	// in the registration destructor. The assertion above is the semantic probe.
	if (!registration->is_valid()) {
		memdelete(registration);
	}
}

struct EmissionDiagnostics {
	int freed_during_emission = 0;
	ErrorHandlerList handler;

	static void capture(void *p_data, const char *, const char *, int, const char *p_error, const char *p_message, bool, ErrorHandlerType) {
		if (String(p_error).contains("was freed or unreferenced while a signal is being emitted") || String(p_message).contains("was freed or unreferenced while a signal is being emitted")) {
			static_cast<EmissionDiagnostics *>(p_data)->freed_during_emission++;
		}
	}
	EmissionDiagnostics() {
		handler.errfunc = capture;
		handler.userdata = this;
		add_error_handler(&handler);
	}
	~EmissionDiagnostics() { remove_error_handler(&handler); }
};

class EmitterReceiver : public Object {
	GDCLASS(EmitterReceiver, Object);

protected:
	static void _bind_methods() {}

public:
	Object *emitter = nullptr;
	int later_calls = 0;
	int inner_calls = 0;
	ObjectID emitter_id;
	bool later_saw_deleted = false;
	Error nested_result = FAILED;
	bool nest = false;

	void delete_emitter() {
		if (nest) {
			nested_result = emitter->emit_signal("inner");
		}
		memdelete(emitter);
		emitter = nullptr;
	}
	void later() {
		later_calls++;
		later_saw_deleted = ObjectDB::get_instance(emitter_id) == nullptr;
	}
	void inner() { inner_calls++; }
};

TEST_CASE("[Object][Native lifetime] Owned signal snapshot completes after emitter deletion") {
	EmitterReceiver receiver;
	receiver.emitter = memnew(Object);
	ObjectID id = receiver.emitter->get_instance_id();
	receiver.emitter_id = id;
	receiver.emitter->add_user_signal(MethodInfo("outer"));
	CHECK_EQ(receiver.emitter->connect("outer", callable_mp(&receiver, &EmitterReceiver::delete_emitter)), OK);
	CHECK_EQ(receiver.emitter->connect("outer", callable_mp(&receiver, &EmitterReceiver::later)), OK);
	EmissionDiagnostics diagnostics;
	CHECK_EQ(receiver.emitter->emit_signal("outer"), OK);
	CHECK_EQ(receiver.later_calls, 1);
	CHECK(receiver.later_saw_deleted);
	CHECK(ObjectDB::get_instance(id) == nullptr);
	CHECK_EQ(diagnostics.freed_during_emission, 1);
}

TEST_CASE("[Object][Native lifetime] Nested emission restores outer diagnostic state") {
	EmitterReceiver receiver;
	receiver.emitter = memnew(Object);
	receiver.nest = true;
	receiver.emitter->add_user_signal(MethodInfo("outer"));
	receiver.emitter->add_user_signal(MethodInfo("inner"));
	CHECK_EQ(receiver.emitter->connect("outer", callable_mp(&receiver, &EmitterReceiver::delete_emitter)), OK);
	CHECK_EQ(receiver.emitter->connect("inner", callable_mp(&receiver, &EmitterReceiver::inner)), OK);
	EmissionDiagnostics diagnostics;
	CHECK_EQ(receiver.emitter->emit_signal("outer"), OK);
	CHECK_EQ(receiver.nested_result, OK);
	CHECK_EQ(receiver.inner_calls, 1);
	CHECK_EQ(diagnostics.freed_during_emission, 1);
}
