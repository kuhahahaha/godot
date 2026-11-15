/**************************************************************************/
/*  script_lifecycle_test.h                                                   */
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

#pragma once

#include "../gdscript.h"
#include "../gdscript_function.h"

#include "core/object/callable_mp.h"
#include "tests/test_macros.h"

#include <thread>

namespace GDScriptLifecycleTests {

class LanguageLockAccessor {
public:
	static Mutex &get() { return GDScriptLanguage::get_singleton()->mutex; }
};

TEST_CASE("[Modules][GDScript][Lifecycle] detached await slot releases state despite copied connection") {
	GDScriptLanguage::get_singleton()->init();
	Ref<GDScript> script = memnew(GDScript);
	script->set_source_code("extends RefCounted\nsignal wake\nfunc suspend():\n\tawait wake\n\treturn 42\n");
	REQUIRE(script->reload() == OK);
	Ref<RefCounted> owner = memnew(RefCounted);
	owner->set_script(script);
	REQUIRE(owner->get_script_instance() != nullptr);
	Callable::CallError error;
	Ref<GDScriptFunctionState> state = owner->get_script_instance()->callp("suspend", nullptr, 0, error);
	REQUIRE(error.error == Callable::CallError::CALL_OK);
	REQUIRE(state.is_valid());
	const ObjectID state_id = state->get_instance_id();
	List<Object::Connection> copied;
	owner->get_signal_connection_list("wake", &copied);
	REQUIRE(copied.size() == 1);
	state.unref();
	REQUIRE(ObjectDB::get_instance(state_id) != nullptr);
	owner->disconnect("wake", copied.front()->get().callable);
	CHECK_FALSE(owner->is_connected("wake", copied.front()->get().callable));
	// The copied connection contains the custom callable that owns the pending
	// state. Drop that inspection copy before asserting final object disposal.
	copied.clear();
	CHECK(ObjectDB::get_instance(state_id) == nullptr);
	CHECK(ObjectDB::get_instance(state_id) == nullptr);
}

TEST_CASE("[Modules][GDScript][Lifecycle] manual resume detaches only its await slot") {
	GDScriptLanguage::get_singleton()->init();
	Ref<GDScript> script = memnew(GDScript);
	script->set_source_code("extends RefCounted\nsignal wake\nfunc suspend():\n\tawait wake\n\treturn 42\n");
	REQUIRE(script->reload() == OK);
	Ref<RefCounted> owner = memnew(RefCounted);
	owner->set_script(script);
	REQUIRE(owner->get_script_instance() != nullptr);
	Callable::CallError error;
	Ref<GDScriptFunctionState> state = owner->get_script_instance()->callp("suspend", nullptr, 0, error);
	REQUIRE(error.error == Callable::CallError::CALL_OK);
	REQUIRE(state.is_valid());
	struct Probe : RefCounted {
		int calls = 0;
		void wake() { calls++; }
	};
	Ref<Probe> probe = memnew(Probe);
	const Callable ordinary = callable_mp(probe.ptr(), &Probe::wake);
	REQUIRE(owner->connect("wake", ordinary) == OK);
	CHECK(state->resume() == Variant(42));
	CHECK_FALSE(state->is_valid());
	List<Object::Connection> remaining;
	owner->get_signal_connection_list("wake", &remaining);
	CHECK(remaining.size() == 1);
	CHECK(owner->is_connected("wake", ordinary));
	// Do not emit while the obsolete await slot is still present on the RED path.
	if (remaining.size() == 1) {
		CHECK(owner->emit_signal("wake") == OK);
		CHECK(probe->calls == 1);
	}
}

TEST_CASE("[Modules][GDScript][Lifecycle] failed reload preserves callable committed image") {
	GDScriptLanguage::get_singleton()->init();
	Ref<GDScript> script = memnew(GDScript);
	script->set_source_code("extends RefCounted\nstatic func value():\n\treturn 7\n");
	REQUIRE(script->reload() == OK);
	Callable::CallError error;
	CHECK(static_cast<Object *>(script.ptr())->callp("value", nullptr, 0, error) == Variant(7));
	REQUIRE(error.error == Callable::CallError::CALL_OK);
	script->set_source_code("extends RefCounted\nstatic func value(\n");
	ERR_PRINT_OFF;
	const Error failed = script->reload(true);
	ERR_PRINT_ON;
	CHECK(failed == ERR_PARSE_ERROR);
	CHECK(script->is_valid());
	const Variant old_value = static_cast<Object *>(script.ptr())->callp("value", nullptr, 0, error);
	CHECK(error.error == Callable::CallError::CALL_OK);
	CHECK(old_value == Variant(7));
	// A real retry is a negative control: refusal must not permanently poison the script.
	script->set_source_code("extends RefCounted\nstatic func value():\n\treturn 9\n");
	REQUIRE(script->reload(true) == OK);
	CHECK(static_cast<Object *>(script.ptr())->callp("value", nullptr, 0, error) == Variant(9));
	CHECK(error.error == Callable::CallError::CALL_OK);
}

#ifdef THREADS_ENABLED
TEST_CASE("[Modules][GDScript][Lifecycle] state destruction releases captured values outside language lock") {
	GDScriptLanguage::get_singleton()->init();
	Ref<GDScript> script = memnew(GDScript);
	script->set_source_code("extends RefCounted\nsignal wake\nfunc suspend(captured):\n\tawait wake\n\treturn captured\n");
	REQUIRE(script->reload() == OK);
	Ref<RefCounted> owner = memnew(RefCounted);
	owner->set_script(script);
	REQUIRE(owner->get_script_instance() != nullptr);
	struct Capture : RefCounted {
		bool *destroyed = nullptr;
		bool *language_available = nullptr;
		~Capture() {
			*destroyed = true;
			// A different native thread distinguishes this recursive mutex being
			// unlocked from a destructor merely reentering it on the same thread.
			std::thread contender([&]() {
				Mutex &mutex = LanguageLockAccessor::get();
				for (int attempt = 0; attempt < 64; attempt++) {
					if (mutex.try_lock()) {
						*language_available = true;
						mutex.unlock();
						break;
					}
					std::this_thread::yield();
				}
			});
			contender.join();
		}
	};
	bool destroyed = false;
	bool language_available = false;
	Ref<Capture> capture = memnew(Capture);
	capture->destroyed = &destroyed;
	capture->language_available = &language_available;
	Variant argument = capture;
	const Variant *args[] = { &argument };
	Callable::CallError error;
	Ref<GDScriptFunctionState> state = owner->get_script_instance()->callp("suspend", args, 1, error);
	REQUIRE(error.error == Callable::CallError::CALL_OK);
	REQUIRE(state.is_valid());
	argument = Variant();
	capture.unref();
	REQUIRE_FALSE(destroyed);
	state->_clear_connections();
	REQUIRE_FALSE(destroyed);
	state.unref();
	CHECK(destroyed);
	CHECK(language_available);
}

TEST_CASE("[Modules][GDScript][Lifecycle] unregistered native thread cannot consume pending state") {
	GDScriptLanguage::get_singleton()->init();
	Ref<GDScript> script = memnew(GDScript);
	script->set_source_code("extends RefCounted\nsignal wake\nfunc suspend():\n\tawait wake\n\treturn 42\n");
	REQUIRE(script->reload() == OK);
	Ref<RefCounted> owner = memnew(RefCounted);
	owner->set_script(script);
	REQUIRE(owner->get_script_instance() != nullptr);
	Callable::CallError error;
	Ref<GDScriptFunctionState> state = owner->get_script_instance()->callp("suspend", nullptr, 0, error);
	REQUIRE(error.error == Callable::CallError::CALL_OK);
	REQUIRE(state.is_valid());
	Variant foreign_result;
	// A raw native thread deliberately has no ScriptServer entry registration.
	// Join makes the observation deterministic without racing another VM entry.
	std::thread foreign([&]() { foreign_result = state->resume(); });
	foreign.join();
	CHECK(foreign_result.get_type() == Variant::NIL);
	CHECK(state->is_valid(true));
	if (state->is_valid(true)) {
		CHECK(state->resume() == Variant(42));
	}
}
#endif

} // namespace GDScriptLifecycleTests
