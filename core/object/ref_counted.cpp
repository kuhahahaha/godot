/**************************************************************************/
/*  ref_counted.cpp                                                       */
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

#include "ref_counted.h"

#include "core/object/class_db.h"
#include "core/object/script_instance.h"
#ifdef TESTS_ENABLED
#include "core/object/ref_counted_test_hooks.h"
#endif

bool RefCounted::init_ref() {
	ReferenceCohort cohort;
	bool result = false;
	const Error admitted = cohort.execute(this, false, [](void *context, ReferenceCohort &p_cohort) {
		GDExtensionDeviceExit::record_binding_entry_result(OK);
		return static_cast<RefCounted *>(context)->_init_ref(p_cohort);
	}, this, result);
	if (admitted != OK) {
		GDExtensionDeviceExit::record_binding_entry_result(admitted);
	}
	return admitted == OK && result;
}

bool RefCounted::_init_ref(ReferenceCohort &cohort) {
#ifdef TESTS_ENABLED
	const auto observer = RefCountedReleaseTest::channel;
	RefCountedReleaseTest::Observation observation{ cohort.get_operation_id() };
	{
		MutexLock lock(_instance_binding_mutex);
		observation.object_read_pins = _binding_read_cohorts;
	}
	observer.emit(RefCountedReleaseTest::Phase::ADMITTED, observation);
#endif
	// The outer fence spans the original compensation. Nested operations use
	// the same exact callback vector and keep its Object pin until this epilogue.
	if (!dereference_count.try_increment(2)) {
		GDExtensionDeviceExit::record_binding_entry_result(ERR_PARAMETER_RANGE_ERROR);
		return false;
	}
	const bool success = _reference(cohort);
	if (success && !is_referenced() && refcount_init.unref()) {
		_unreference(cohort, true);
	}
	cohort.retire_object_pin();
#ifdef TESTS_ENABLED
	{
		MutexLock lock(_instance_binding_mutex);
		observation.object_read_pins = _binding_read_cohorts;
	}
	observation.pin_retired = true;
	observation.completion_count = dereference_count.get();
	observer.emit(RefCountedReleaseTest::Phase::INIT_PIN_RETIRED, observation);
#endif
#ifdef TESTS_ENABLED
	observation.completion_count = dereference_count.decrement(2);
#else
	dereference_count.decrement(2);
#endif
#ifdef TESTS_ENABLED
	observer.emit(RefCountedReleaseTest::Phase::INIT_COMPLETE, observation);
#endif
	return success;
}

void RefCounted::deinit_ref() {
	// Somewhat unsafe since we're doing tests in sync, but it's probably fine
	// since this function is called from the creation thread, so nobody else should have access.

	// If this succeeds, refcount_init is 2 (or more).
	// This would be unexpected, since callers should already have consumed it, or never established it.
	// It's a little unsafe to bring it above 1, since it means init_ref must be called more than once,
	// but the alternative would be decrementing refcount instead, which is also unsafe since the object
	// might unexpectedly destruct early. Better to risk zombying than to risk a crash.
	if (!refcount_init.ref()) {
		// refcount_init already dead, must re-establish (expected).
		refcount_init.init(1);
	}
}

void RefCounted::_bind_methods() {
	ClassDB::bind_method(D_METHOD("init_ref"), &RefCounted::init_ref);
	ClassDB::bind_method(D_METHOD("reference"), &RefCounted::reference);
	ClassDB::bind_method(D_METHOD("unreference"), &RefCounted::unreference);
	ClassDB::bind_method(D_METHOD("get_reference_count"), &RefCounted::get_reference_count);
}

int RefCounted::get_reference_count() const {
	return refcount.get();
}

bool RefCounted::reference() {
	ReferenceCohort cohort;
	bool result = false;
	const Error admitted = cohort.execute(this, false, [](void *context, ReferenceCohort &p_cohort) {
		return static_cast<RefCounted *>(context)->_reference(p_cohort);
	}, this, result);
	GDExtensionDeviceExit::record_binding_entry_result(admitted);
	return admitted == OK && result;
}

bool RefCounted::_reference(ReferenceCohort &p_cohort) {
	const uint32_t rc_val = refcount.refval();
	const bool success = rc_val != 0;
	if (success && rc_val <= 2) {
		p_cohort.invoke(true);
	}
	return success;
}

bool RefCounted::unreference() {
	ReferenceCohort cohort;
	bool result = false;
	const Error admitted = cohort.execute(this, true, [](void *context, ReferenceCohort &p_cohort) {
		return static_cast<RefCounted *>(context)->_unreference(p_cohort, false);
	}, this, result);
	if (admitted != OK) {
		GDExtensionDeviceExit::stop_lifetime_entry(admitted == ERR_PARAMETER_RANGE_ERROR ? GDExtensionDeviceExit::LifetimeFailure::LIMIT : (admitted == ERR_BUSY ? GDExtensionDeviceExit::LifetimeFailure::ASSOCIATION_BUSY : GDExtensionDeviceExit::LifetimeFailure::UNREGISTERED));
	}
	return result;
}

bool RefCounted::_unreference(ReferenceCohort &p_cohort, bool p_borrowed) {
#ifdef TESTS_ENABLED
	const auto observer = RefCountedReleaseTest::channel;
	RefCountedReleaseTest::Observation observation{ p_cohort.get_operation_id(), 0, false, p_borrowed };
	if (!p_borrowed) {
		observer.emit(RefCountedReleaseTest::Phase::ADMITTED, observation);
	}
#endif
	if (!(p_borrowed ? dereference_count.increment_borrowed() : dereference_count.try_increment(1))) {
		GDExtensionDeviceExit::stop_lifetime_entry(GDExtensionDeviceExit::LifetimeFailure::LIMIT);
	}
#ifdef TESTS_ENABLED
	{
		MutexLock lock(_instance_binding_mutex);
		observation.object_read_pins = _binding_read_cohorts;
	}
	observation.completion_count = dereference_count.get();
	observer.emit(RefCountedReleaseTest::Phase::BEFORE_MUTATION, observation);
#endif
	const uint32_t rc_val = refcount.unrefval();
	bool die = rc_val == 0;
	if (rc_val <= 1) {
		const bool callback_ret = p_cohort.invoke(false);
		die = die && callback_ret;
	}
#ifdef TESTS_ENABLED
	observation.refcount_result = rc_val;
	observation.terminal = die;
	observer.emit(RefCountedReleaseTest::Phase::CALLBACK_RESULTS, observation);
#endif
	const uint64_t release_id = p_cohort.get_operation_id();
	if (die) {
		if (p_borrowed) {
			// Valid first-reference compensation always retains its initialized
			// native owner; it cannot become the terminal releaser.
			GDExtensionDeviceExit::stop_lifetime_entry(GDExtensionDeviceExit::LifetimeFailure::UNREGISTERED);
		}
		_reserve_terminal_release(release_id);
	}
	if (!p_borrowed) {
		p_cohort.retire_object_pin();
#ifdef TESTS_ENABLED
		{
			MutexLock lock(_instance_binding_mutex);
			observation.object_read_pins = _binding_read_cohorts;
		}
		observation.pin_retired = true;
		observation.completion_count = dereference_count.get();
		observer.emit(RefCountedReleaseTest::Phase::PIN_RETIRED, observation);
#endif
	}
// A nonterminal completion is the last Object access. The cohort's
// remaining destructor owns only core generation frames.
#ifdef TESTS_ENABLED
	observation.completion_count = dereference_count.decrement(p_borrowed ? 0 : 1);
#else
	dereference_count.decrement(p_borrowed ? 0 : 1);
#endif
#ifdef TESTS_ENABLED
	observer.emit(RefCountedReleaseTest::Phase::COMPLETE, observation);
#endif
	if (die) {
#ifdef TESTS_ENABLED
		// Observe pending once; retain the original completion-counter join.
		if (dereference_count.get()) {
			observer.emit(RefCountedReleaseTest::Phase::JOIN_PENDING, observation);
		}
#endif
		while (dereference_count.get()) {
			// Original terminal completion join; no association reader wait.
		}
#ifdef TESTS_ENABLED
		observation.completion_count = 0; // The original acquire loop observed zero.
		observer.emit(RefCountedReleaseTest::Phase::JOIN_ZERO, observation);
#endif
		_complete_terminal_release(release_id);
#ifdef TESTS_ENABLED
		observer.emit(RefCountedReleaseTest::Phase::TERMINAL_READY, observation);
#endif
	}
	return die;
}

RefCounted::RefCounted() :
		Object(true) {
	_define_ancestry(AncestralClass::REF_COUNTED);
	refcount.init();
	refcount_init.init();
	dereference_count.set(0);
}

Variant WeakRef::get_ref() const {
	if (ref.is_null()) {
		return Variant();
	}

	Object *obj = ObjectDB::get_instance(ref);
	if (!obj) {
		return Variant();
	}
	RefCounted *r = cast_to<RefCounted>(obj);
	if (r) {
		return Ref<RefCounted>(r);
	}

	return obj;
}

void WeakRef::set_obj(Object *p_object) {
	ref = p_object ? p_object->get_instance_id() : ObjectID();
}

void WeakRef::set_ref(const Ref<RefCounted> &p_ref) {
	ref = p_ref.is_valid() ? p_ref->get_instance_id() : ObjectID();
}

void WeakRef::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_ref"), &WeakRef::get_ref);
}
