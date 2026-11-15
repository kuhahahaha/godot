/**************************************************************************/
/*  gdscript_test_runner_suite.h                                          */
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

#include "../gdscript_cache.h"
#include "active_reload_cache_test.h"
#include "gdscript_test_runner.h"
#include "script_lifecycle_test.h"

#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

#ifdef TOOLS_ENABLED
#include "core/os/os.h"
#endif

namespace GDScriptTests {

class TestGDScriptFunctionStateAccessor {
public:
	static int stack_size(const GDScriptFunctionState *p_state) { return p_state->state.stack_size; }
	static const uint8_t *stack_data(const GDScriptFunctionState *p_state) { return p_state->state.stack.ptr(); }
	static int64_t stack_bytes(const GDScriptFunctionState *p_state) { return p_state->state.stack.size(); }
};

class StackCleanupObserver : public RefCounted {
public:
	GDScriptFunctionState *state = nullptr;
	int destroyed = 0;
	int reentered = 0;
	int completed = 0;
	int active_callbacks = 0;
	bool armed = false;
	bool extent_claimed = true;
	bool storage_detached = true;
	bool active_storage_preserved = false;
	Variant completed_result;

	void on_completed(const Variant &p_result) {
		completed++;
		completed_result = p_result;
	}

	void on_active() {
		active_callbacks++;
		const uint8_t *data = TestGDScriptFunctionStateAccessor::stack_data(state);
		const int64_t bytes = TestGDScriptFunctionStateAccessor::stack_bytes(state);
		const bool vm_owns_stack = TestGDScriptFunctionStateAccessor::stack_size(state) == 0;
		state->_clear_stack();
		active_storage_preserved = vm_owns_stack && data != nullptr && bytes > 0 &&
				data == TestGDScriptFunctionStateAccessor::stack_data(state) &&
				bytes == TestGDScriptFunctionStateAccessor::stack_bytes(state);
	}
};

class StackCleanupProbe : public RefCounted {
public:
	Ref<StackCleanupObserver> observer;

	~StackCleanupProbe() {
		if (observer.is_null()) {
			return;
		}
		observer->destroyed++;
		if (!observer->armed || !observer->state) {
			return; // Fixture setup can fail before a suspended state is available.
		}
		const bool claimed = TestGDScriptFunctionStateAccessor::stack_size(observer->state) == 0;
		observer->extent_claimed &= claimed;
		observer->storage_detached &= TestGDScriptFunctionStateAccessor::stack_bytes(observer->state) == 0;
		// The old implementation must fail an assertion, not recursively double-destroy slots.
		if (claimed) {
			observer->state->_clear_stack();
			observer->reentered++;
		}
	}
};

class ScopedStackCleanupObservation {
	Ref<StackCleanupObserver> observer;

public:
	explicit ScopedStackCleanupObservation(const Ref<StackCleanupObserver> &p_observer) :
			observer(p_observer) {
		observer->armed = true;
	}
	~ScopedStackCleanupObservation() { observer->armed = false; }
};

TEST_CASE("[Modules][GDScript] Suspended stack cleanup claims extent and detaches storage before destructors") {
	GDScriptLanguage::get_singleton()->init();
	Ref<GDScript> script = memnew(GDScript);
	script->set_source_code(R"(
extends RefCounted
signal wake
func suspend(probe, tail):
	await wake
	return 42
)");
	REQUIRE(script->reload() == OK);
	Ref<RefCounted> owner = memnew(RefCounted);
	owner->set_script(script);
	REQUIRE(owner->get_script_instance() != nullptr);
	Ref<StackCleanupObserver> observer = memnew(StackCleanupObserver);
	Ref<StackCleanupProbe> probe = memnew(StackCleanupProbe);
	Ref<StackCleanupProbe> tail = memnew(StackCleanupProbe);
	probe->observer = observer;
	tail->observer = observer;
	Variant first_arg = probe;
	Variant second_arg = tail;
	const Variant *args[] = { &first_arg, &second_arg };
	Callable::CallError error;
	Ref<GDScriptFunctionState> state = owner->get_script_instance()->callp("suspend", args, 2, error);
	REQUIRE(error.error == Callable::CallError::CALL_OK);
	REQUIRE(state.is_valid());
	observer->state = state.ptr();
	REQUIRE(state->connect("completed", callable_mp(observer.ptr(), &StackCleanupObserver::on_completed)) == OK);
	first_arg = Variant();
	second_arg = Variant();
	probe.unref();
	tail.unref();
	REQUIRE(observer->destroyed == 0);
	REQUIRE(TestGDScriptFunctionStateAccessor::stack_size(state.ptr()) > 0);
	// Detach delivery first; a manually cleared pending state must never be resumed.
	state->_clear_connections();
	{
		ScopedStackCleanupObservation observation(observer);
		state->_clear_stack();
	}
	CHECK(observer->destroyed == 2);
	CHECK(observer->extent_claimed);
	CHECK(observer->storage_detached);
	CHECK(observer->reentered == 2);
	CHECK(observer->completed == 0);
	state->_clear_stack();
	CHECK(observer->destroyed == 2);
	CHECK(TestGDScriptFunctionStateAccessor::stack_size(state.ptr()) == 0);
}

TEST_CASE("[Modules][GDScript] Suspended stack cleanup preserves active VM storage during manual resume") {
	GDScriptLanguage::get_singleton()->init();
	Ref<GDScript> script = memnew(GDScript);
	script->set_source_code(R"(
extends RefCounted
signal wake
func suspend(check_active: Callable):
	await wake
	check_active.call()
	return 42
)");
	REQUIRE(script->reload() == OK);
	Ref<RefCounted> owner = memnew(RefCounted);
	owner->set_script(script);
	REQUIRE(owner->get_script_instance() != nullptr);
	Ref<StackCleanupObserver> observer = memnew(StackCleanupObserver);
	Variant callback = callable_mp(observer.ptr(), &StackCleanupObserver::on_active);
	const Variant *args[] = { &callback };
	Callable::CallError error;
	Ref<GDScriptFunctionState> state = owner->get_script_instance()->callp("suspend", args, 1, error);
	REQUIRE(error.error == Callable::CallError::CALL_OK);
	REQUIRE(state.is_valid());
	observer->state = state.ptr();
	REQUIRE(state->connect("completed", callable_mp(observer.ptr(), &StackCleanupObserver::on_completed)) == OK);
	state->_clear_connections();
	const Variant result = state->resume();
	CHECK(result == Variant(42));
	CHECK(observer->active_callbacks == 1);
	CHECK(observer->active_storage_preserved);
	CHECK(observer->completed == 1);
	CHECK(observer->completed_result == Variant(42));
	CHECK_FALSE(state->is_valid());
}

class TestGDScriptCacheAccessor {
public:
	static bool has_shallow(String p_path) {
		return GDScriptCache::singleton->shallow_gdscript_cache.has(p_path);
	}

	static bool has_full(String p_path) {
		return GDScriptCache::singleton->full_gdscript_cache.has(p_path);
	}
};

// TODO: Handle some cases failing on release builds. See: https://github.com/godotengine/godot/pull/88452
#ifdef TOOLS_ENABLED
TEST_SUITE("[Modules][GDScript]") {
	TEST_CASE("Script compilation and runtime") {
		bool print_filenames = OS::get_singleton()->get_cmdline_args().find("--print-filenames") != nullptr;
		bool use_binary_tokens = OS::get_singleton()->get_cmdline_args().find("--use-binary-tokens") != nullptr;
		GDScriptTestRunner runner("modules/gdscript/tests/scripts", true, print_filenames, use_binary_tokens);
		int fail_count = runner.run_tests();
		INFO("Make sure `*.out` files have expected results.");
		REQUIRE_MESSAGE(fail_count == 0, "All GDScript tests should pass.");
	}
}
#endif // TOOLS_ENABLED

TEST_CASE("[Modules][GDScript] Load source code dynamically and run it") {
	GDScriptLanguage::get_singleton()->init();
	Ref<GDScript> gdscript = memnew(GDScript);
	gdscript->set_source_code(R"(
extends RefCounted

func _init():
	set_meta("result", 42)
)");
	// A spurious `Condition "err" is true` message is printed (despite parsing being successful and returning `OK`).
	// Silence it.
	ERR_PRINT_OFF;
	const Error error = gdscript->reload();
	ERR_PRINT_ON;
	CHECK_MESSAGE(error == OK, "The script should parse successfully.");

	// Run the script by assigning it to a reference-counted object.
	Ref<RefCounted> ref_counted = memnew(RefCounted);
	ref_counted->set_script(gdscript);
	CHECK_MESSAGE(int(ref_counted->get_meta("result")) == 42, "The script should assign object metadata successfully.");
}

TEST_CASE("[Modules][GDScript] Loading keeps ResourceCache and GDScriptCache in sync") {
	GDScriptLanguage::get_singleton()->init();
	const String path = TestUtils::get_temp_path("gdscript_load_test.gd");

	{
		Ref<FileAccess> fa = FileAccess::open(path, FileAccess::ModeFlags::WRITE);
		fa->store_string("extends RefCounted\n");
		fa->close();
	}

	CHECK(!ResourceCache::has(path));
	CHECK(!TestGDScriptCacheAccessor::has_shallow(path));
	CHECK(!TestGDScriptCacheAccessor::has_full(path));

	Ref<GDScript> loaded = ResourceLoader::load(path);

	CHECK(ResourceCache::has(path));
	CHECK(!TestGDScriptCacheAccessor::has_shallow(path));
	CHECK(TestGDScriptCacheAccessor::has_full(path));
}

TEST_CASE("[Modules][GDScript] Validate built-in API") {
	GDScriptLanguage *lang = GDScriptLanguage::get_singleton();

	// Validate methods.
	List<MethodInfo> builtin_methods;
	lang->get_public_functions(&builtin_methods);

	SUBCASE("[Modules][GDScript] Validate built-in methods") {
		for (const MethodInfo &mi : builtin_methods) {
			for (int64_t i = 0; i < mi.arguments.size(); ++i) {
				TEST_COND((mi.arguments[i].name.is_empty() || mi.arguments[i].name.begins_with("_unnamed_arg")),
						vformat("Unnamed argument in position %d of built-in method '%s'.", i, mi.name));
			}
		}
	}

	// Validate annotations.
	List<MethodInfo> builtin_annotations;
	lang->get_public_annotations(&builtin_annotations);

	SUBCASE("[Modules][GDScript] Validate built-in annotations") {
		for (const MethodInfo &ai : builtin_annotations) {
			for (int64_t i = 0; i < ai.arguments.size(); ++i) {
				TEST_COND((ai.arguments[i].name.is_empty() || ai.arguments[i].name.begins_with("_unnamed_arg")),
						vformat("Unnamed argument in position %d of built-in annotation '%s'.", i, ai.name));
			}
		}
	}
}

} // namespace GDScriptTests

TEST_CASE("[Modules][GDScript][Cancellation] completed callback cannot re-enter the same state") {
	GDScriptLanguage::get_singleton()->init();
	Ref<GDScript> script = memnew(GDScript);
	script->set_source_code(R"(
extends RefCounted
signal wake
func suspend():
	await wake
	return 42
)");
	REQUIRE(script->reload() == OK);
	Ref<RefCounted> owner = memnew(RefCounted);
	owner->set_script(script);
	REQUIRE(owner->get_script_instance() != nullptr);
	Callable::CallError error;
	Ref<GDScriptFunctionState> state = owner->get_script_instance()->callp("suspend", nullptr, 0, error);
	REQUIRE(error.error == Callable::CallError::CALL_OK);
	REQUIRE(state.is_valid());

	struct ReentrantProbe : RefCounted {
		Ref<GDScriptFunctionState> state;
		int callback_count = 0;
		bool valid_during_callback = true;
		Variant reentrant_result;
		void on_completed(const Variant &) {
			callback_count++;
			valid_during_callback = state->is_valid();
			if (callback_count == 1) {
				reentrant_result = state->resume();
			}
		}
	};
	Ref<ReentrantProbe> probe = memnew(ReentrantProbe);
	probe->state = state;
	REQUIRE(state->connect("completed", callable_mp(probe.ptr(), &ReentrantProbe::on_completed)) == OK);
	const Variant result = state->resume();
	CHECK_EQ(result, Variant(42));
	CHECK_EQ(probe->callback_count, 1);
	CHECK_FALSE(probe->valid_during_callback);
	CHECK(probe->reentrant_result.get_type() == Variant::NIL);
}

TEST_CASE("[Modules][GDScript][Cancellation] canceled pending state cannot appear resumable") {
	GDScriptLanguage::get_singleton()->init();
	Ref<GDScript> script = memnew(GDScript);
	script->set_source_code(R"(
extends RefCounted
signal wake
func suspend():
	await wake
	return 42
)"
	);
	REQUIRE(script->reload() == OK);
	Ref<RefCounted> owner = memnew(RefCounted);
	owner->set_script(script);
	REQUIRE(owner->get_script_instance() != nullptr);
	Callable::CallError error;
	Ref<GDScriptFunctionState> state = owner->get_script_instance()->callp("suspend", nullptr, 0, error);
	REQUIRE(error.error == Callable::CallError::CALL_OK);
	REQUIRE(state.is_valid());
	REQUIRE(state->is_valid());

	script->cancel_pending_functions(false);

	CHECK_FALSE(state->is_valid());
	ERR_PRINT_OFF;
	const Variant result = state->resume();
	ERR_PRINT_ON;
	CHECK(result.get_type() == Variant::NIL);
}
