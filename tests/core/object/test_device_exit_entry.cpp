/**************************************************************************/
/*  test_device_exit_entry.cpp                                            */
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

#include "core/config/engine.h"
#include "core/crypto/crypto_core.h"
#include "core/extension/gdextension.h"
#include "core/extension/gdextension_function_loader.h"
#include "core/extension/gdextension_library_loader.h"
#include "core/extension/gdextension_manager.h"
#include "core/io/marshalls.h"
#include "core/object/class_db.h"
#include "core/object/ref_counted.h"
#include "core/object/ref_counted_test_hooks.h"
#include "core/object/script_language.h"
#include "core/os/memory_allocation_test_hooks.h"
#include "core/os/os.h"
#include "tests/test_macros.h"

#include "modules/modules_enabled.gen.h"
#ifdef MODULE_GDSCRIPT_ENABLED
#include "modules/gdscript/gdscript.h"
#endif

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>

TEST_FORCE_LINK(test_device_exit_entry)

namespace TestDeviceExitEntry {
struct Access;
Error register_authenticated_lifetime(Object *object, const GDExtensionDeviceExit::RecordRef &record, uint64_t thread, uint64_t action) {
	GDExtensionDeviceExit::CallbackScope entry_scope(record);
	if (entry_scope.get_error() != OK) {
		return entry_scope.get_error();
	}
	return object->register_release_lifetime(record, thread, action);
}
static GDExtension *owner = nullptr;
static Error nested_close = OK;
static bool close_at_bootstrap = false;
static bool close_at_initialize = false;
static bool close_at_deinitialize = false;
static bool nested_levels = false;
static Error inner_close = OK;

static void initialize(void *, GDExtensionInitializationLevel level) {
	if (nested_levels && level == GDEXTENSION_INITIALIZATION_CORE) {
		owner->initialize_library(GDExtension::INITIALIZATION_LEVEL_SERVERS);
		inner_close = nested_close;
	}
	if (close_at_initialize) {
		nested_close = owner->close_library();
	}
}
static void deinitialize(void *, GDExtensionInitializationLevel level) {
	if (nested_levels && level == GDEXTENSION_INITIALIZATION_SERVERS) {
		owner->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_CORE);
		inner_close = nested_close;
	}
	if (close_at_deinitialize) {
		nested_close = owner->close_library();
	}
}
static GDExtensionBool entry(GDExtensionInterfaceGetProcAddress, GDExtensionClassLibraryPtr library, GDExtensionInitialization *result) {
	owner = static_cast<GDExtension *>(library);
	*result = {};
	result->initialize = initialize;
	result->deinitialize = deinitialize;
	if (close_at_bootstrap) {
		nested_close = owner->close_library();
	}
	return true;
}

static Object *create_real_binding_holder(GDExtension *extension) {
	Object *object = memnew(Object);
	const GDExtensionInstanceBindingCallbacks callbacks = {
		[](void *, void *instance) -> void * { return instance; },
		[](void *, void *, void *) {}, nullptr
	};
	if (object->get_instance_binding(extension, &callbacks) != object) {
		memdelete(object);
		return nullptr;
	}
	return object;
}

// The real FunctionLoader executes resident test code, so the old unsafe close
// is a deterministic logical detach rather than a use-after-dlclose experiment.
TEST_CASE("device_exit::entry/outer-callback-return-precedes-close") {
	for (int route = 0; route < 3; route++) {
		close_at_bootstrap = route == 0;
		close_at_initialize = route == 1;
		close_at_deinitialize = route == 2;
		nested_close = OK;
		Ref<GDExtensionFunctionLoader> loader;
		loader.instantiate();
		loader->set_initialization_function(entry);
		Ref<GDExtension> extension;
		extension.instantiate();
		REQUIRE_EQ(extension->open_library("libgodot://outer-callback-return", loader), OK);
		if (route != 0) {
			extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_CORE);
		}
		if (route == 2) {
			extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_CORE);
		}
		CHECK_EQ(nested_close, route == 1 ? ERR_UNAVAILABLE : ERR_BUSY);
		CHECK(extension->is_library_open());
		CHECK(loader->is_library_open());
		if (route == 1) {
			// Legacy initialized teardown has no typed schedule. Complete the
			// actual terminator before requesting physical close.
			CHECK_EQ(extension->close_library(), ERR_UNAVAILABLE);
			close_at_initialize = false;
			CHECK_EQ(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_CORE), OK);
		}
		if (extension->is_library_open()) {
			CHECK_EQ(extension->close_library(), OK);
		}
		owner = nullptr;
	}
}
TEST_CASE("device_exit::entry/nested-level-return-keeps-outer-frame-active") {
	for (const bool exiting : { false, true }) {
		close_at_bootstrap = false;
		close_at_initialize = false;
		close_at_deinitialize = false;
		nested_levels = false;
		Ref<GDExtensionFunctionLoader> loader;
		loader.instantiate();
		loader->set_initialization_function(entry);
		Ref<GDExtension> extension;
		extension.instantiate();
		REQUIRE_EQ(extension->open_library("libgodot://nested-return", loader), OK);
		if (exiting) {
			extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_CORE);
			extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SERVERS);
		}
		close_at_initialize = !exiting;
		close_at_deinitialize = exiting;
		nested_levels = true;
		inner_close = OK;
		nested_close = OK;
		if (exiting) {
			extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SERVERS);
		} else {
			extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_CORE);
		}
		CHECK_EQ(inner_close, exiting ? ERR_BUSY : ERR_UNAVAILABLE);
		CHECK_EQ(nested_close, exiting ? ERR_BUSY : ERR_UNAVAILABLE);
		CHECK(extension->is_library_open());
		if (!exiting) {
			CHECK_EQ(extension->close_library(), ERR_UNAVAILABLE);
			nested_levels = close_at_initialize = false;
			CHECK_EQ(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SERVERS), OK);
			CHECK_EQ(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_CORE), OK);
		}
		CHECK_EQ(extension->close_library(), OK);
		nested_levels = false;
		owner = nullptr;
	}
}

TEST_CASE("device_exit::entry/binding-owner-is-enrolled-generation-not-arbitrary-token") {
	close_at_bootstrap = false;
	close_at_initialize = false;
	close_at_deinitialize = false;
	nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	GDExtensionDeviceExit::RecordRef old_owner;
	GDExtensionDeviceExit::RecordRef new_owner;
	GDExtensionDeviceExit::RecordRef unknown;
	int arbitrary_token = 0;
	CHECK_EQ(registry.retain_binding_owner(&arbitrary_token, unknown), ERR_DOES_NOT_EXIST);
	CHECK_FALSE(unknown.is_valid());
	REQUIRE_EQ(extension->open_library("libgodot://binding-owner-first", loader), OK);
	CHECK_EQ(registry.retain_binding_owner(extension.ptr(), old_owner), OK);
	CHECK(old_owner.is_valid());
	if (old_owner.is_valid()) {
		GDExtensionDeviceExit::CallbackScope frame(old_owner);
		CHECK_EQ(frame.get_error(), OK);
		CHECK_EQ(extension->close_library(), ERR_BUSY);
	}
	CHECK_EQ(extension->close_library(), OK);
	CHECK_EQ(registry.retain_binding_owner(extension.ptr(), unknown), ERR_UNAVAILABLE);
	Ref<GDExtensionFunctionLoader> second_loader;
	second_loader.instantiate();
	second_loader->set_initialization_function(entry);
	REQUIRE_EQ(extension->open_library("libgodot://binding-owner-second", second_loader), OK);
	CHECK_EQ(registry.retain_binding_owner(extension.ptr(), new_owner), OK);
	CHECK(new_owner.is_valid());
	if (old_owner.is_valid()) {
		GDExtensionDeviceExit::CallbackScope stale(old_owner);
		CHECK_NE(stale.get_error(), OK);
	}
	if (new_owner.is_valid()) {
		GDExtensionDeviceExit::CallbackScope fresh(new_owner);
		CHECK_EQ(fresh.get_error(), OK);
		CHECK_EQ(extension->close_library(), ERR_BUSY);
	}
	CHECK_EQ(extension->close_library(), OK);
	owner = nullptr;
}

TEST_CASE("device_exit::entry/quiesce-blocked-still-seals-normal-admission") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://quiesce-gate", loader), OK);
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	GDExtensionDeviceExit::RecordRef record;
	REQUIRE_EQ(registry.retain_binding_owner(extension.ptr(), record), OK);
	{
		Object *existing_holder = create_real_binding_holder(extension.ptr());
		REQUIRE(existing_holder != nullptr);
		{
			GDExtensionDeviceExit::CallbackScope accepted(record);
			REQUIRE_EQ(accepted.get_error(), OK);
			CHECK_EQ(registry.begin_quiesce(record), ERR_BUSY);
			GDExtensionDeviceExit::CallbackScope late(record);
			CHECK_NE(late.get_error(), OK);
			GDExtensionDeviceExit::BindingLifetime late_holder;
			CHECK_NE(late_holder.acquire(record), OK);
			{
				GDExtensionDeviceExit::BindingPublicationScope publication(record);
				CHECK_NE(publication.get_error(), OK);
			}
			CHECK_EQ(extension->close_library(), ERR_BUSY);
			existing_holder->free_instance_binding(extension.ptr());
			memdelete(existing_holder); // Original core disposal, inside the admitted frame.
		}
		// Returning accepted work retires its exact holder; the seal stays closed.
		CHECK_EQ(registry.begin_quiesce(record), OK);
	}
	CHECK_EQ(registry.begin_quiesce(record), OK);
	CHECK_EQ(registry.begin_quiesce(record), OK);
	{
		GDExtensionDeviceExit::CallbackScope late(record);
		CHECK_NE(late.get_error(), OK);
	}
	CHECK(loader->is_library_open());
	CHECK_EQ(extension->close_library(), OK);
	owner = nullptr;
}

TEST_CASE("device_exit::entry/exact-cleanup-action-after-quiesce") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://exact-cleanup", loader), OK);
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	GDExtensionDeviceExit::RecordRef record;
	REQUIRE_EQ(registry.retain_binding_owner(extension.ptr(), record), OK);
	{
		GDExtensionDeviceExit::CleanupAction action;
		CHECK_EQ(registry.register_cleanup_action(record, 701, Thread::get_caller_id(), 11, action), OK);
		CHECK_EQ(registry.begin_quiesce(record), ERR_BUSY);
		GDExtensionDeviceExit::CleanupAction late;
		CHECK_NE(registry.register_cleanup_action(record, 701, Thread::get_caller_id(), 12, late), OK);
		{
			GDExtensionDeviceExit::CleanupScope exact(action, record, 701, 11);
			CHECK_EQ(exact.get_error(), OK);
			GDExtensionDeviceExit::CallbackScope normal(record);
			CHECK_NE(normal.get_error(), OK);
			GDExtensionDeviceExit::BindingLifetime new_holder;
			CHECK_NE(new_holder.acquire(record), OK);
			GDExtensionDeviceExit::CleanupScope wrong_object(action, record, 702, 11);
			CHECK_NE(wrong_object.get_error(), OK);
			GDExtensionDeviceExit::CleanupScope wrong_action(action, record, 701, 12);
			CHECK_NE(wrong_action.get_error(), OK);
			GDExtensionDeviceExit::RecordRef absent;
			GDExtensionDeviceExit::CleanupScope wrong_generation(action, absent, 701, 11);
			CHECK_NE(wrong_generation.get_error(), OK);
			Error other_thread = OK;
			std::thread wrong_thread([&]() {
				GDExtensionDeviceExit::CleanupScope wrong(action, record, 701, 11);
				other_thread = wrong.get_error();
			});
			wrong_thread.join();
			CHECK_NE(other_thread, OK);
			CHECK_EQ(extension->close_library(), ERR_BUSY);
		}
		CHECK_EQ(extension->close_library(), ERR_BUSY);
	}
	CHECK_EQ(registry.begin_quiesce(record), OK);
	CHECK_EQ(extension->close_library(), OK);
	owner = nullptr;
}

static int binding_creates = 0;
static Error binding_create_close = OK;
static void *binding_create(void *, void *instance) {
	binding_creates++;
	binding_create_close = owner->close_library();
	return instance;
}
TEST_CASE("device_exit::object/binding-create-frame-and-closed-lookup") {
	close_at_bootstrap = false;
	close_at_initialize = false;
	close_at_deinitialize = false;
	nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://binding-create", loader), OK);
	Object *object = memnew(Object);
	Object *absent = memnew(Object);
	const GDExtensionInstanceBindingCallbacks callbacks = { binding_create, nullptr, nullptr };
	binding_creates = 0;
	binding_create_close = OK;
	CHECK_EQ(object->get_instance_binding(extension.ptr(), &callbacks), nullptr);
	CHECK_EQ(binding_creates, 1);
	CHECK_EQ(binding_create_close, ERR_BUSY);
	CHECK_FALSE(object->has_instance_binding(extension.ptr()));
	CHECK_EQ(extension->close_library(), OK);
	CHECK_EQ(object->get_instance_binding(extension.ptr(), nullptr), nullptr);
	CHECK_EQ(absent->get_instance_binding(extension.ptr(), &callbacks), nullptr);
	CHECK_EQ(binding_creates, 1);
	CHECK_FALSE(absent->has_instance_binding(extension.ptr()));
	memdelete(absent);
	memdelete(object);
	owner = nullptr;
}

static int binding_frees = 0;
static Error binding_free_close = OK;
static bool binding_free_lookup = false;
static void *binding_free_lookup_result = nullptr;
static void binding_free(void *token, void *instance, void *) {
	if (binding_free_lookup) {
		binding_free_lookup_result = static_cast<Object *>(instance)->get_instance_binding(token, nullptr);
	}
	binding_frees++;
	binding_free_close = owner->close_library();
}
TEST_CASE("device_exit::object/binding-holder-retires-after-real-free-return") {
	for (bool destructor : { false, true }) {
		close_at_bootstrap = false;
		close_at_initialize = false;
		close_at_deinitialize = false;
		nested_levels = false;
		Ref<GDExtensionFunctionLoader> loader;
		loader.instantiate();
		loader->set_initialization_function(entry);
		Ref<GDExtension> extension;
		extension.instantiate();
		REQUIRE_EQ(extension->open_library("libgodot://binding-holder", loader), OK);
		Object *object = memnew(Object);
		GDExtensionDeviceExit::RecordRef record;
		REQUIRE_EQ(GDExtensionDeviceExit::Registry::get_singleton().retain_binding_owner(extension.ptr(), record), OK);
		REQUIRE_EQ(TestDeviceExitEntry::register_authenticated_lifetime(object, record, Thread::get_caller_id(), 702), OK);
		const GDExtensionInstanceBindingCallbacks callbacks = {
			[](void *, void *instance) -> void * { return instance; }, binding_free, nullptr
		};
		binding_frees = 0;
		binding_free_close = OK;
		binding_free_lookup = true;
		binding_free_lookup_result = object;
		REQUIRE_EQ(object->get_instance_binding(extension.ptr(), &callbacks), object);
		CHECK_EQ(extension->close_library(), ERR_BUSY);
		CHECK(extension->is_library_open());
		if (destructor) {
			memdelete(object);
		} else {
			object->free_instance_binding(extension.ptr());
			CHECK_FALSE(object->has_instance_binding(extension.ptr()));
			object->free_instance_binding(extension.ptr());
			memdelete(object);
		}
		CHECK_EQ(binding_free_lookup_result, nullptr);
		binding_free_lookup = false;
		CHECK_EQ(binding_frees, 1);
		CHECK_EQ(binding_free_close, ERR_BUSY);
		if (extension->is_library_open()) {
			CHECK_EQ(extension->close_library(), OK);
		}
		owner = nullptr;
	}
}

static bool probe_on_stack = false;
static int premature_loader_closes = 0;
static int probes = 0;
static GDExtensionInterfaceGetProcAddress original_get = nullptr;
static void close_probe() {
	probe_on_stack = true;
	const Error result = owner->close_library();
	probe_on_stack = false;
	probes++;
	if (result != ERR_BUSY || premature_loader_closes != 0) {
		// The safety loader below prevented any actual close while DSO code ran.
		std::_Exit(96);
	}
}
static GDExtensionInterfaceFunctionPtr intercepted_get(const char *name) {
	if (std::strcmp(name, "fixture_core_close_probe") == 0) {
		return reinterpret_cast<GDExtensionInterfaceFunctionPtr>(close_probe);
	}
	return original_get(name);
}
class ProbedLoader : public GDExtensionLibraryLoader {
public:
	Error initialize(GDExtensionInterfaceGetProcAddress get, const Ref<GDExtension> &extension, GDExtensionInitialization *result) override {
		owner = extension.ptr();
		original_get = get;
		return GDExtensionLibraryLoader::initialize(intercepted_get, extension, result);
	}
	Error close_library() override {
		if (probe_on_stack) {
			premature_loader_closes++;
			return ERR_BUSY;
		}
		return GDExtensionLibraryLoader::close_library();
	}
};
static void dynamic_entry_child() {
	String path;
	for (const String &arg : OS::get_singleton()->get_cmdline_args()) {
		if (arg.begins_with("--device-exit-path=")) {
			path = arg.trim_prefix("--device-exit-path=");
		}
	}
	if (path.is_empty()) {
		std::_Exit(91);
	}
	Ref<ProbedLoader> loader;
	loader.instantiate();
	Ref<GDExtension> extension;
	extension.instantiate();
	if (extension->open_library(path, loader) != OK) {
		std::_Exit(92);
	}
	extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_CORE);
	extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_CORE);
	const Error closed = extension->close_library();
	const bool good = closed == OK && probes == 3 && premature_loader_closes == 0 && !loader->is_library_open();
	std::fprintf(stderr, "ENTRY_FRAME_NATIVE good=%d probes=%d early_closes=%d\n", int(good), probes, premature_loader_closes);
	if (!good) {
		std::_Exit(93);
	}
	owner = nullptr;
}
REGISTER_TEST_COMMAND("--device-exit-entry-native", dynamic_entry_child);
static String control_route;
static int control_calls = 0;
static Error control_close = OK;
static void control_probe() {
	control_calls++;
	control_close = owner->close_library();
}
static void control_startup() {
	if (control_route == "startup" || control_route == "late-startup") {
		control_probe();
	}
}
static void control_frame() {
	if (control_route == "frame") {
		control_probe();
	}
}
static void control_shutdown() {
	if (control_route == "shutdown" || control_route == "unload-shutdown") {
		control_probe();
	}
}
#ifdef TOOLS_ENABLED
static void control_classes(GDExtensionTypePtr result) {
	static_cast<PackedStringArray *>(result)->push_back("Node");
	control_probe();
}
#endif // TOOLS_ENABLED
static void control_level(void *, GDExtensionInitializationLevel) {}
static GDExtensionBool control_entry(GDExtensionInterfaceGetProcAddress get, GDExtensionClassLibraryPtr library, GDExtensionInitialization *result) {
	owner = static_cast<GDExtension *>(library);
	*result = {};
	result->initialize = control_level;
	result->deinitialize = control_level;
	auto register_loop = reinterpret_cast<GDExtensionInterfaceRegisterMainLoopCallbacks>(get("register_main_loop_callbacks"));
	const GDExtensionMainLoopCallbacks callbacks = { control_startup, control_shutdown, control_frame };
	register_loop(library, &callbacks);
#ifdef TOOLS_ENABLED
	auto register_classes = reinterpret_cast<GDExtensionInterfaceEditorRegisterGetClassesUsedCallback>(get("editor_register_get_classes_used_callback"));
	register_classes(library, control_classes);
#endif
	return true;
}
static void control_entry_child() {
	for (const String &arg : OS::get_singleton()->get_cmdline_args()) {
		if (arg.begins_with("--entry-route=")) {
			control_route = arg.trim_prefix("--entry-route=");
		}
	}
	const String requested = control_route;
#ifdef TOOLS_ENABLED
	if (requested == "editor-empty") {
		Ref<GDExtension> empty;
		empty.instantiate();
		const bool good = empty->get_classes_used().is_empty();
		std::fprintf(stderr, "ENTRY_CONTROL route=editor-empty good=%d calls=0 close=0\n", int(good));
		std::fflush(stderr);
		std::_Exit(good ? 0 : 95);
	}
#endif
	auto *manager = GDExtensionManager::get_singleton();
	if (requested == "late-startup") {
		manager->startup();
	}
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(control_entry);
	const String path = "libgodot://control-frame";
	if (manager->load_extension_with_loader(path, loader) != GDExtensionManager::LOAD_STATUS_OK) {
		std::_Exit(91);
	}
	Ref<GDExtension> extension = manager->get_extension(path);
#ifdef TOOLS_ENABLED
	if (requested == "empty-loop") {
		Engine::get_singleton()->set_extension_reloading_enabled(true);
		extension->set_reloadable(true);
		const auto reloaded = manager->reload_extension(path);
		const bool good = reloaded == GDExtensionManager::LOAD_STATUS_FAILED && manager->is_extension_loaded(path) && !extension->is_library_open();
		manager->frame();
		manager->frame();
		manager->startup();
		manager->shutdown();
		std::fprintf(stderr, "ENTRY_CONTROL route=empty-loop good=%d calls=%d close=%d\n", int(good), control_calls, int(control_close));
		std::fflush(stderr);
		// This child tests failed-reload no-op dispatch, not the separate host
		// teardown behavior of a failed reload retained in the manager map.
		std::_Exit(good ? 0 : 95);
	}
#endif
	bool unloaded = false;
	bool result_valid = true;
	if (requested == "startup") {
		manager->startup();
	} else if (requested == "frame") {
		manager->frame();
	} else if (requested == "shutdown") {
		manager->shutdown();
	} else if (requested == "unload-shutdown") {
		unloaded = manager->unload_extension(path) == GDExtensionManager::LOAD_STATUS_OK;
		result_valid = unloaded;
#ifdef TOOLS_ENABLED
	} else if (requested == "editor") {
		const PackedStringArray classes = extension->get_classes_used();
		result_valid = classes.size() == 1 && classes[0] == "Node";
#endif
	} else if (requested != "late-startup") {
		std::_Exit(92);
	}
	const bool good = result_valid && control_calls == 1 && control_close == ERR_BUSY && extension->is_library_open();
	std::fprintf(stderr, "ENTRY_CONTROL route=%s good=%d calls=%d close=%d\n", requested.utf8().get_data(), int(good), control_calls, int(control_close));
	std::fflush(stderr);
	if (!good) {
		std::_Exit(95);
	}
	control_route = "finished";
	if (!unloaded && manager->unload_extension(path) != GDExtensionManager::LOAD_STATUS_OK) {
		std::_Exit(93);
	}
	if (extension->close_library() != OK) {
		std::_Exit(94);
	}
	owner = nullptr;
}
REGISTER_TEST_COMMAND("--device-exit-entry-control", control_entry_child);
struct Access {
	static bool ready_reservation_retired(GDExtension &extension) {
		auto &ref = extension.library_exit_ref;
		MutexLock lock(ref.registry->mutex);
		const auto &record = ref.registry->records[ref.slot];
		return !record.reserved_scene_ready_frame && !record.scene_ready_view_serial && !record.scene_ready_callback && !record.scene_ready_callback_v2;
	}
	static uint8_t loop_mask(GDExtension &extension) { return extension.main_loop_callback_mask; }
	static void (*loop_frame(GDExtension &extension))() { return extension.frame_callback; }
	static void loop_dispatch(GDExtension &extension) { extension._call_main_loop_callback(GDExtension::MainLoopCallback::FRAME); }
	static Error cancel_ready(GDExtension &extension, GDExtensionDeviceExit::CloseRoute route) {
		return GDExtensionDeviceExit::Registry::get_singleton().cancel_pending_scene_ready(extension.library_exit_ref, route);
	}
	static Error dispatch_ready(GDExtension &extension) {
		return GDExtensionDeviceExit::Registry::get_singleton().dispatch_scene_ready(extension.library_exit_ref);
	}
	static Error dispatch_materialized_ready(GDExtension &extension) { return extension.dispatch_scene_ready_if_materialized(); }
	static Error abort_ready(GDExtension &extension, Error error) { return extension.abort_scene_ready_load(error); }
	static bool ready_seam(bool value) {
		auto *manager = GDExtensionManager::get_singleton();
		const bool old = manager->scene_materialization_seam_reached;
		manager->scene_materialization_seam_reached = value;
		return old;
	}
	static int32_t manager_level() { return GDExtensionManager::get_singleton()->level; }
#ifdef TOOLS_ENABLED
	static void clear_bindings(GDExtension &extension) { extension.clear_instance_bindings(); }
	static void forget_closed_test_extension(const String &path) { GDExtensionManager::get_singleton()->gdextension_map.erase(path); }
#endif
	static bool quiescing(GDExtension &extension) {
		auto &ref = extension.library_exit_ref;
		MutexLock lock(ref.registry->mutex);
		return ref.registry->records[ref.slot].quiescing;
	}
	static uint32_t ready_private_budget(GDExtension &extension, uint32_t extra) {
		auto &ref = extension.library_exit_ref;
		MutexLock lock(ref.registry->mutex);
		const uint32_t old = ref.registry->private_limit;
		ref.registry->private_limit = ref.registry->records[ref.slot].private_refs + extra;
		return old;
	}
	static void session_identity(Object *object, uint64_t &registry, uint64_t &epoch) { GDExtensionDeviceExit::Registry::get_singleton().device_loss_identity(object, registry, epoch); }
	static void session_lost_identity(uint64_t registry, uint64_t epoch) { GDExtensionDeviceExit::Registry::get_singleton().device_lost(registry, epoch); }
	static bool session_live(Object *object) {
		auto &r = GDExtensionDeviceExit::Registry::get_singleton();
		MutexLock lock(r.mutex);
		for (const auto &d : r.device_records) {
			if (d.object == object) {
				return d.live;
			}
		}
		return false;
	}
	static void session_lost(Object *object) { GDExtensionDeviceExit::Registry::get_singleton().device_lost(object); }
	static void session_counter(Object *object, uint64_t next, bool exhausted) {
		auto &r = GDExtensionDeviceExit::Registry::get_singleton();
		MutexLock lock(r.mutex);
		for (auto &d : r.device_records) {
			if (d.object == object) {
				d.sessions.next = next;
				d.sessions.exhausted = exhausted;
			}
		}
	}
	static Error session_create(Object *object) { return GDExtensionDeviceExit::Registry::get_singleton().device_created(object); }
	static void session_close(Object *object) { GDExtensionDeviceExit::Registry::get_singleton().device_closed(object); }
	static void session_destroy(Object *object) { GDExtensionDeviceExit::Registry::get_singleton().device_destroyed(object); }
	static uint32_t session_record_count() {
		auto &r = GDExtensionDeviceExit::Registry::get_singleton();
		MutexLock lock(r.mutex);
		uint32_t count = 0;
		for (const auto &d : r.device_records) {
			count += d.epoch != 0;
		}
		return count;
	}

	static void issuer_terminal_sequences(int missing) {
		auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
		MutexLock lock(registry.mutex);
		// SCENE+EDITOR: control + four level contexts + token, four actions.
		registry.serials.next = UINT64_MAX - 5 + (missing == 1);
		registry.frame_serials.next = UINT64_MAX - 4 + (missing == 2);
		registry.action_serials.next = UINT64_MAX - 3 + (missing == 3);
	}

	static void collide_cleanup_holder_serial(GDExtension &extension, GDExtensionDeviceExit::CleanupAction &action) {
		auto &ref = extension.library_exit_ref;
		MutexLock lock(ref.registry->mutex);
		// The private holder and frame sequences are distinct namespaces. Equal
		// numeric serials are valid and must not revoke another frame's views.
		auto *holder = ref.registry->records[ref.slot].holders;
		CRASH_COND(!holder || !holder->cleanup);
		holder->serial = ref.registry->records[ref.slot].levels[2].initialize_frame;
	}

	static uint32_t issuer_limit(int which, uint32_t limit) {
		auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
		MutexLock lock(registry.mutex);
		uint32_t &target = which == 0 ? registry.action_limit : which == 1 ? registry.view_limit
																		   : registry.private_limit;
		const uint32_t old = target;
		target = limit;
		return old;
	}
	static void corrupt_typed_holder(GDExtension &extension, int field, uint64_t handle = 0) {
		auto &ref = extension.library_exit_ref;
		MutexLock lock(ref.registry->mutex);
		auto *holder = ref.registry->records[ref.slot].holders;
		while (holder && handle && holder->handle_serial != handle) {
			holder = holder->record_next;
		}
		CRASH_COND(!holder);
		if (field == 0) {
			holder->generation ^= uint64_t(1) << 60;
		} else {
			holder->terminal_action ^= uint64_t(1) << 60;
		}
	}

	static void exhaust_issuer_sequences(bool value) {
		auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
		MutexLock lock(registry.mutex);
		registry.serials.exhausted = value;
		registry.frame_serials.exhausted = value;
	}
	static bool guard_pointers_clear(GDExtension &extension) {
		auto &ref = extension.library_exit_ref;
		if (!ref.registry) {
			return true;
		}
		MutexLock lock(ref.registry->mutex);
		const auto &record = ref.registry->records[ref.slot];
		if (record.guard.preflight || record.guard.user_initializer || record.guard.user_terminator) {
			return false;
		}
		for (const auto &level : record.levels) {
			if (level.user_action.wrapper || level.binding_action.wrapper || level.user_action.terminator || level.user_action.userdata || level.binding_action.userdata) {
				return false;
			}
		}
		return true;
	}

	static int32_t level(GDExtension &extension) { return extension.level_initialized; }
	static void set_close_error(GDExtension &extension, Error error) { extension.library_close_error = error; }
	static void clear_initialize(GDExtension &extension) { extension.initialization.initialize = nullptr; }
	static void clear_deinitialize(GDExtension &extension) { extension.initialization.deinitialize = nullptr; }
	static void restore_deinitialize(GDExtension &extension, void (*callback)(void *, GDExtensionInitializationLevel)) { extension.initialization.deinitialize = callback; }
	static uint32_t record_capacity() { return GDExtensionDeviceExit::Registry::RECORD_CAPACITY; }
	static void set_quiescing(GDExtension &extension, bool closed) {
		auto &reference = extension.library_exit_ref;
		MutexLock lock(reference.registry->mutex);
		reference.registry->records[reference.slot].quiescing = closed;
	}
	static void set_completion_count(RefCounted &object, uint32_t count) { object.dereference_count.set(count); }
	static uint32_t completion_count(RefCounted &object) { return object.dereference_count.get(); }
	static void set_resident_gate_closed(GDExtension &extension, bool closed) {
		auto &reference = extension.library_exit_ref;
		MutexLock lock(reference.registry->mutex);
		reference.registry->records[reference.slot].close_started = closed;
	}
	static uint64_t generation(GDExtension &extension) { return extension.library_exit_ref.get_generation(); }
	static uint32_t private_refs(GDExtension &extension) {
		auto &reference = extension.library_exit_ref;
		MutexLock lock(reference.registry->mutex);
		return reference.registry->records[reference.slot].private_refs;
	}
	static uint32_t active_frames(GDExtension &extension) {
		auto &reference = extension.library_exit_ref;
		MutexLock lock(reference.registry->mutex);
		return reference.registry->records[reference.slot].active_callbacks;
	}
	static uint32_t binding_holders(GDExtension &extension) {
		auto &reference = extension.library_exit_ref;
		MutexLock lock(reference.registry->mutex);
		return reference.registry->records[reference.slot].binding_holders;
	}
	static uint64_t first_holder_serial(GDExtension &extension) {
		auto &reference = extension.library_exit_ref;
		MutexLock lock(reference.registry->mutex);
		return reference.registry->records[reference.slot].holders ? reference.registry->records[reference.slot].holders->serial : 0;
	}
	static void corrupt_pending_holder_frame(GDExtension &extension) {
		auto &reference = extension.library_exit_ref;
		MutexLock lock(reference.registry->mutex);
		auto *holder = reference.registry->records[reference.slot].holders;
		CRASH_COND(!holder || !holder->pending);
		holder->return_frame++;
	}
	static uint64_t first_holder_retirement_attempt(GDExtension &extension) {
		auto &reference = extension.library_exit_ref;
		MutexLock lock(reference.registry->mutex);
		return reference.registry->records[reference.slot].holders ? reference.registry->records[reference.slot].holders->retirement_attempt : 0;
	}
	static uint64_t first_holder_acquired_thread(GDExtension &extension) {
		auto &reference = extension.library_exit_ref;
		MutexLock lock(reference.registry->mutex);
		return reference.registry->records[reference.slot].holders ? reference.registry->records[reference.slot].holders->acquired_thread : 0;
	}
	static void set_active_frames(GDExtension &extension, uint32_t count) {
		auto &reference = extension.library_exit_ref;
		MutexLock lock(reference.registry->mutex);
		reference.registry->records[reference.slot].active_callbacks = count;
	}
	static uint32_t cleanup_reservations(GDExtension &extension) {
		auto &reference = extension.library_exit_ref;
		MutexLock lock(reference.registry->mutex);
		return reference.registry->records[reference.slot].cleanup_reservations;
	}
	static GDExtensionDeviceExit::CleanupPhase cleanup_phase(const GDExtensionDeviceExit::CleanupAction &action) {
		return action.get_phase();
	}
	static bool physical_can_close(GDExtension &extension) {
		auto &reference = extension.library_exit_ref;
		MutexLock lock(reference.registry->mutex);
		return reference.registry->physical_can_close_locked(reference.slot);
	}
	static uint64_t attempt(GDExtension &extension) {
		auto &reference = extension.library_exit_ref;
		MutexLock lock(reference.registry->mutex);
		return reference.registry->records[reference.slot].exit_attempt;
	}
	static uint64_t evidence_serial(const GDExtensionDeviceExit::RecordRef &reference) {
		MutexLock lock(reference.registry->mutex);
		return reference.registry->records[reference.slot].evidence_serial;
	}
	static bool evidence_sealed(const GDExtensionDeviceExit::RecordRef &reference) {
		MutexLock lock(reference.registry->mutex);
		const auto &record = reference.registry->records[reference.slot];
		return record.evidence_committed && record.evidence_ack_received;
	}
	static PackedByteArray evidence_bytes(const GDExtensionDeviceExit::RecordRef &reference) {
		MutexLock lock(reference.registry->mutex);
		const auto &record = reference.registry->records[reference.slot];
		PackedByteArray result;
		result.resize(record.evidence.size);
		if (record.evidence.size) {
			memcpy(result.ptrw(), record.evidence.bytes, record.evidence.size);
		}
		return result;
	}
	static PackedByteArray evidence_digest(const GDExtensionDeviceExit::RecordRef &reference) {
		MutexLock lock(reference.registry->mutex);
		PackedByteArray result;
		result.resize(32);
		memcpy(result.ptrw(), reference.registry->records[reference.slot].evidence_digest, 32);
		return result;
	}
	static bool evidence_committed(const GDExtensionDeviceExit::RecordRef &reference) {
		MutexLock lock(reference.registry->mutex);
		return reference.registry->records[reference.slot].evidence_committed;
	}
	static Error prepare_evidence(const GDExtensionDeviceExit::RecordRef &reference, GDExtensionDeviceExit::CloseRoute route, GDExtensionDeviceExit::CoreEvidenceAdmission &admission) {
		return reference.registry->prepare_core_evidence(reference, route, admission);
	}
	static Error commit_evidence(const GDExtensionDeviceExit::RecordRef &reference, const GDExtensionDeviceExit::CoreEvidenceAdmission &admission, GDExtensionDeviceExit::PublicationAck &ack) {
		return reference.registry->commit_core_evidence(admission, ack);
	}
	static Error consume_evidence(const GDExtensionDeviceExit::RecordRef &reference, const GDExtensionDeviceExit::PublicationAck &ack) {
		return reference.registry->consume_core_ack(reference, ack);
	}
	static Error close_route(GDExtension &extension, GDExtensionDeviceExit::CloseRoute route) { return extension._close_library(route); }
#ifdef TOOLS_ENABLED
	static bool reloading(GDExtension &extension) { return extension.is_reloading; }
#endif
	static Error retain_record(GDExtension &extension, GDExtensionDeviceExit::RecordRef &r_ref) {
		return GDExtensionDeviceExit::Registry::get_singleton().retain_private(extension.library_exit_ref, r_ref);
	}
	static uint32_t exhaust_callback_refs(GDExtension &extension, uint32_t reserve = 0) {
		auto &reference = extension.library_exit_ref;
		MutexLock lock(reference.registry->mutex);
		const uint32_t old_limit = reference.registry->private_limit;
		reference.registry->private_limit = reference.registry->records[reference.slot].private_refs + reserve;
		return old_limit;
	}
	static void restore_callback_refs(GDExtension &extension, uint32_t limit) {
		auto &reference = extension.library_exit_ref;
		MutexLock lock(reference.registry->mutex);
		reference.registry->private_limit = limit;
	}
};
TEST_CASE("device_exit::entry/binding-holder-retirement-waits-for-outer-frame-return") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://retiring-at-return", loader), OK);
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	GDExtensionDeviceExit::RecordRef record;
	REQUIRE_EQ(Access::retain_record(*extension.ptr(), record), OK);
	Object *holder = create_real_binding_holder(extension.ptr());
	REQUIRE(holder != nullptr);
	{
		GDExtensionDeviceExit::CallbackScope outer(record);
		REQUIRE_EQ(outer.get_error(), OK);
		CHECK_EQ(Access::binding_holders(*extension.ptr()), 2);
		CHECK_NE(Access::first_holder_serial(*extension.ptr()), 0);
		CHECK_EQ(Access::first_holder_acquired_thread(*extension.ptr()), Thread::get_caller_id());
		const uint32_t retained = Access::private_refs(*extension.ptr());
		{
			GDExtensionDeviceExit::CallbackScope nested(record);
			REQUIRE_EQ(nested.get_error(), OK);
			holder->free_instance_binding(extension.ptr());
			memdelete(holder);
			CHECK_EQ(registry.begin_quiesce(record), ERR_BUSY);
			CHECK_EQ(Access::binding_holders(*extension.ptr()), 2);
		}
		CHECK_EQ(Access::binding_holders(*extension.ptr()), 2);
		// The Object's SharedRecord metadata reference has gone; both code
		// holder references remain pending until the outer actual return.
		CHECK_EQ(Access::private_refs(*extension.ptr()), retained - 1);
		CHECK_EQ(extension->close_library(), ERR_BUSY);
	}
	CHECK_EQ(Access::binding_holders(*extension.ptr()), 0);
	CHECK_EQ(registry.begin_quiesce(record), OK);
	CHECK_EQ(extension->close_library(), OK);
	owner = nullptr;
}

TEST_CASE("device_exit::host-lifetime/cleanup-action-final-return") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://cleanup-retiring-at-return", loader), OK);
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	GDExtensionDeviceExit::RecordRef record;
	REQUIRE_EQ(Access::retain_record(*extension.ptr(), record), OK);
	auto *action = memnew(GDExtensionDeviceExit::CleanupAction);
	REQUIRE_EQ(registry.register_cleanup_action(record, 77, Thread::get_caller_id(), 88, *action), OK);
	{
		GDExtensionDeviceExit::CallbackScope outer(record);
		REQUIRE_EQ(outer.get_error(), OK);
		REQUIRE_EQ(registry.begin_quiesce(record), ERR_BUSY);
		{
			GDExtensionDeviceExit::CleanupScope cleanup(*action, record, 77, 88);
			REQUIRE_EQ(cleanup.get_error(), OK);
			memdelete(action);
		}
		CHECK_EQ(Access::cleanup_reservations(*extension.ptr()), 0);
		CHECK_EQ(Access::binding_holders(*extension.ptr()), 1);
		CHECK_EQ(extension->close_library(), ERR_BUSY);
	}
	CHECK_EQ(Access::binding_holders(*extension.ptr()), 0);
	CHECK_EQ(extension->close_library(), OK);
	owner = nullptr;
}

TEST_CASE("device_exit::host-lifetime/production-preclose-commit") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://preclose-production", loader), OK);
	GDExtensionDeviceExit::RecordRef evidence;
	REQUIRE_EQ(Access::retain_record(*extension.ptr(), evidence), OK);
	const uint64_t before = Access::evidence_serial(evidence);
	CHECK_EQ(before, 0);
	CHECK_EQ(extension->close_library(), OK);
	CHECK_NE(before, Access::evidence_serial(evidence));
	CHECK(Access::evidence_sealed(evidence));
	CHECK_FALSE(extension->is_library_open());
	owner = nullptr;
}

TEST_CASE("device_exit::successor/capsule-bytes-survive-close-and-lost-ack") {
	using namespace GDExtensionDeviceExit;
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://successor-capsule", loader), OK);
	RecordRef record;
	REQUIRE_EQ(Access::retain_record(*extension.ptr(), record), OK);
	const uint64_t generation = record.get_generation();
	REQUIRE_EQ(extension->preflight_exit(CloseRoute::EXPLICIT), OK);
	CoreEvidenceAdmission admission;
	REQUIRE_EQ(Access::prepare_evidence(record, CloseRoute::EXPLICIT, admission), OK);
	PublicationAck empty;
	CHECK_EQ(Access::consume_evidence(record, empty), ERR_INVALID_DATA);
	CHECK_EQ(Registry::get_singleton().begin_close(record), ERR_BUSY);
	{
		PublicationAck lost;
		REQUIRE_EQ(Access::commit_evidence(record, admission, lost), OK);
		CHECK(Access::evidence_committed(record));
		CHECK_FALSE(Access::evidence_sealed(record));
		CHECK_EQ(Registry::get_singleton().begin_close(record), ERR_BUSY);
	} // Commit exists but no ack was delivered to the close controller.
	const uint64_t serial = Access::evidence_serial(record);
	const PackedByteArray bytes = Access::evidence_bytes(record);
	const PackedByteArray digest = Access::evidence_digest(record);
	REQUIRE_EQ(bytes.size(), 224);
	CHECK_EQ(decode_uint32(bytes.ptr()), 1);
	CHECK_EQ(decode_uint64(bytes.ptr() + 12), generation);
	CHECK_EQ(decode_uint64(bytes.ptr() + 28), serial);
	CHECK_EQ(decode_uint32(bytes.ptr() + 36), uint32_t(CloseRoute::EXPLICIT));
	uint8_t actual_digest[32];
	REQUIRE_EQ(CryptoCore::sha256(bytes.ptr(), bytes.size(), actual_digest), OK);
	CHECK_EQ(memcmp(actual_digest, digest.ptr(), 32), 0);
	REQUIRE_EQ(extension->close_library(), OK); // Exact retry obtains the original ack.
	CHECK_EQ(Access::evidence_serial(record), serial);
	CHECK(Access::evidence_sealed(record));
	CHECK(Access::evidence_bytes(record) == bytes);
	CHECK(Access::evidence_digest(record) == digest);
	CHECK_FALSE(extension->is_library_open());
	extension.unref();
	CHECK(Access::evidence_bytes(record) == bytes); // Only independent core storage remains.
	owner = nullptr;
}

TEST_CASE("device_exit::successor/preclose-rejects-foreign-and-wrong-route") {
	using namespace GDExtensionDeviceExit;
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loaders[2];
	Ref<GDExtension> extensions[2];
	RecordRef records[2];
	for (int i = 0; i < 2; i++) {
		loaders[i].instantiate();
		loaders[i]->set_initialization_function(entry);
		extensions[i].instantiate();
		REQUIRE_EQ(extensions[i]->open_library("libgodot://successor-identity", loaders[i]), OK);
		REQUIRE_EQ(Access::retain_record(*extensions[i].ptr(), records[i]), OK);
		REQUIRE_EQ(extensions[i]->preflight_exit(CloseRoute::RELOAD), OK);
	}
	CoreEvidenceAdmission admission;
	CHECK_EQ(Access::prepare_evidence(records[0], CloseRoute::EXPLICIT, admission), ERR_INVALID_DATA);
	REQUIRE_EQ(Access::prepare_evidence(records[0], CloseRoute::RELOAD, admission), OK);
	PublicationAck ack;
	REQUIRE_EQ(Access::commit_evidence(records[0], admission, ack), OK);
	CHECK_EQ(Access::consume_evidence(records[1], ack), ERR_INVALID_DATA);
	CHECK_FALSE(Access::evidence_sealed(records[1]));
	CHECK_EQ(extensions[0]->close_library(), ERR_INVALID_DATA);
	CHECK(extensions[0]->is_library_open());
	for (int i = 0; i < 2; i++) {
		REQUIRE_EQ(Access::close_route(*extensions[i].ptr(), CloseRoute::RELOAD), OK);
		CHECK_EQ(decode_uint32(Access::evidence_bytes(records[i]).ptr() + 36), uint32_t(CloseRoute::RELOAD));
	}
	owner = nullptr;
}

static void holder_frame_mismatch_child() {
	using namespace GDExtensionDeviceExit;
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	if (extension->open_library("libgodot://holder-frame-mismatch", loader) != OK) {
		std::_Exit(91);
	}
	RecordRef record;
	if (Access::retain_record(*extension.ptr(), record) != OK) {
		std::_Exit(92);
	}
	Object *holder = create_real_binding_holder(extension.ptr());
	if (!holder) {
		std::_Exit(93);
	}
	{
		CallbackScope outer(record);
		if (outer.get_error() != OK) {
			std::_Exit(94);
		}
		holder->free_instance_binding(extension.ptr());
		memdelete(holder);
		Access::corrupt_pending_holder_frame(*extension.ptr());
		std::fprintf(stderr, "HOLDER_FRAME_MISMATCH_BEFORE_RETURN\n");
		std::fflush(stderr);
	}
	std::fprintf(stderr, "HOLDER_FRAME_MISMATCH_UNEXPECTED_RETIREMENT\n");
	std::fflush(stderr);
	std::_Exit(95);
}
REGISTER_TEST_COMMAND("--device-exit-holder-frame-mismatch", holder_frame_mismatch_child);

// Private production resolver invoked from the actual loader entry. No public
// proc-address registration or metadata capability grants guard authority.
static void (*issuer_init_hook)(GDExtensionDeviceExit::InitializationLevelContextRef) = nullptr;
static void (*issuer_term_hook)(GDExtensionDeviceExit::DeinitLevelContextRef, int) = nullptr;
static void (*issuer_entry_hook)(GDExtensionDeviceExit::GuardRegistrationV1 &, GDExtensionInitialization *) = nullptr;
static void (*issuer_before_register)(GDExtensionDeviceExit::GuardRegistrationV1 &) = nullptr;
static bool issuer_entry_success = true;
static bool issuer_child_mode = false;
static bool issuer_skip_init_ack = false;
static bool issuer_skip_deinit_ack = false;
static Error issuer_preflight_result = OK;
static bool issuer_preflight_publish = true;
static bool issuer_preflight_mismatch = false;
static GDExtensionDeviceExit::DeinitPreflightContextRef issuer_stale_preflight;
static GDExtensionDeviceExit::GuardToken issuer_token;
static Error issuer_registration = FAILED;
static Error issuer_ack = FAILED;
static GDExtensionDeviceExit::InitializationLevelContextRef issuer_stale_context;
static void issuer_initialize(void *, GDExtensionInitializationLevel level) {
	using namespace GDExtensionDeviceExit;
	auto &registry = Registry::get_singleton();
	const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
	InitializationLevelContextRef context;
	issuer_ack = registry.borrow_initialize_level(&abi, owner, uint32_t(level), &context);
	if (issuer_ack == OK) {
		issuer_stale_context = context;
		if (issuer_init_hook) {
			issuer_init_hook(context);
		}
		if (!issuer_skip_init_ack) {
			issuer_ack = registry.after_binding_initialize(&abi, context);
		}
	}
}
static void issuer_terminate(void *, GDExtensionInitializationLevel level) {
	using namespace GDExtensionDeviceExit;
	auto &registry = Registry::get_singleton();
	const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
	DeinitLevelContextRef context;
	issuer_ack = registry.borrow_deinit_level(&abi, owner, uint32_t(level), &context);
	if (issuer_ack == OK) {
		issuer_ack = registry.begin_user_terminator(&abi, issuer_token, context);
	}
	if (issuer_ack == OK && issuer_term_hook) {
		issuer_term_hook(context, 1);
	}
	if (issuer_ack == OK) {
		issuer_ack = registry.after_user_terminator(&abi, context);
	}
	if (issuer_ack == OK && issuer_term_hook) {
		issuer_term_hook(context, 2);
	}
	if (issuer_ack == OK && !issuer_skip_deinit_ack) {
		issuer_ack = registry.after_binding_deinit(&abi, context);
	}
}
static int32_t issuer_preflight(GDExtensionDeviceExit::DeinitPreflightContextRef context, void *) {
	using namespace GDExtensionDeviceExit;
	const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
	ExitDiagnosticV1 diagnostic;
	diagnostic.abi = { 1, sizeof(ExitDiagnosticV1), 0 };
	diagnostic.disposition = issuer_preflight_result;
	issuer_stale_preflight = context;
	if (issuer_preflight_publish) {
		const Error published = Registry::get_singleton().publish_exit_result(&abi, issuer_token, context, &diagnostic);
		if (issuer_child_mode) {
			if (published != OK) {
				std::_Exit(94);
			}
		} else {
			CHECK_EQ(published, OK);
			CHECK_EQ(Registry::get_singleton().publish_exit_result(&abi, issuer_token, context, &diagnostic), ERR_INVALID_DATA);
		}
	}
	return issuer_preflight_mismatch ? ERR_UNAVAILABLE : issuer_preflight_result;
}
static GDExtensionBool issuer_entry(GDExtensionInterfaceGetProcAddress, GDExtensionClassLibraryPtr library, GDExtensionInitialization *result) {
	using namespace GDExtensionDeviceExit;
	owner = static_cast<GDExtension *>(library);
	*result = {};
	result->minimum_initialization_level = GDEXTENSION_INITIALIZATION_SCENE;
	result->initialize = issuer_initialize;
	result->deinitialize = issuer_terminate;
	GuardRegistrationV1 registration{};
	registration.abi = { 1, sizeof(GuardRegistrationV1), 0 };
	registration.binding_protocol = 1;
	registration.level_bitmap = 12;
	registration.user_cleanup_bitmap = 12;
	registration.binding_cleanup_bitmap = 12;
	registration.expected_initialization = *result;
	registration.user_initializer = issuer_initialize;
	registration.user_terminator = issuer_terminate;
	registration.preflight = issuer_preflight;
	issuer_token = {};
	if (issuer_before_register) {
		issuer_before_register(registration);
	}
	issuer_registration = Registry::get_singleton().register_guard(library, &registration, &issuer_token);
	if (issuer_entry_hook) {
		issuer_entry_hook(registration, result);
	}
	return issuer_registration == OK && issuer_entry_success;
}
struct IssuerFixture {
	Ref<GDExtensionFunctionLoader> loader;
	Ref<GDExtension> extension;
	IssuerFixture() {
		issuer_entry_success = true;
		issuer_child_mode = false;
		issuer_init_hook = nullptr;
		issuer_term_hook = nullptr;
		issuer_entry_hook = nullptr;
		issuer_before_register = nullptr;
		issuer_skip_init_ack = issuer_skip_deinit_ack = false;
		issuer_preflight_result = OK;
		issuer_preflight_publish = true;
		issuer_preflight_mismatch = false;
		loader.instantiate();
		loader->set_initialization_function(issuer_entry);
		extension.instantiate();
	}
	Error open() { return extension->open_library("libgodot://issuer", loader); }
	void finish() {
		CHECK_EQ(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT), OK);
		if (Access::level(*extension.ptr()) >= 2) {
			CHECK_EQ(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
		}
		CHECK(Access::guard_pointers_clear(*extension.ptr()));
		CHECK_EQ(extension->close_library(), OK);
	}
};
static int loop_entry_calls = 0;
static void loop_entry_frame() {
	++loop_entry_calls;
}
TEST_CASE("device_exit::issuer/main-loop-bit1-is-required-before-pointer-publication") {
	using namespace GDExtensionDeviceExit;
	for (bool declared : { false, true }) {
		IssuerFixture fixture;
		loop_entry_calls = 0;
		if (declared) {
			issuer_before_register = [](GuardRegistrationV1 &r) { r.entry_classes |= uint64_t(1) << 1; };
		}
		REQUIRE(fixture.open() == OK);
		const auto install = reinterpret_cast<GDExtensionInterfaceRegisterMainLoopCallbacks>(GDExtension::get_interface_function("register_main_loop_callbacks"));
		REQUIRE(install);
		GDExtensionMainLoopCallbacks callbacks{ nullptr, nullptr, loop_entry_frame };
		ERR_PRINT_OFF;
		install(fixture.extension.ptr(), &callbacks);
		ERR_PRINT_ON;
		CHECK(Access::loop_mask(*fixture.extension.ptr()) == (declared ? 4 : 0));
		CHECK(Access::loop_frame(*fixture.extension.ptr()) == (declared ? loop_entry_frame : nullptr));
		Access::loop_dispatch(*fixture.extension.ptr());
		CHECK(loop_entry_calls == (declared ? 1 : 0));
		REQUIRE(fixture.extension->preflight_exit(CloseRoute::EXPLICIT) == OK);
		callbacks.frame_func = nullptr;
		ERR_PRINT_OFF;
		install(fixture.extension.ptr(), &callbacks);
		Access::loop_dispatch(*fixture.extension.ptr());
		ERR_PRINT_ON;
		CHECK(Access::loop_mask(*fixture.extension.ptr()) == (declared ? 4 : 0));
		CHECK(Access::loop_frame(*fixture.extension.ptr()) == (declared ? loop_entry_frame : nullptr));
		CHECK(loop_entry_calls == (declared ? 1 : 0));
		fixture.finish();
	}
}
static bool v2_registration_v1_first = false;
static int v2_registration_bodies = 0;
static int32_t registration_body_v1(void *, GDExtensionDeviceExitSceneReadyContextRef) {
	++v2_registration_bodies;
	return OK;
}
static int32_t registration_body_v2(void *, GDExtensionDeviceExitSceneReadyContextRef, const GDExtensionSceneInstallPlanV2 *) {
	++v2_registration_bodies;
	return OK;
}
// Shared original FunctionLoader receiver for TypedABI's SI03 subcase.
void test_v2_single_registration() {
	using namespace GDExtensionDeviceExit;
	const auto v2 = reinterpret_cast<GDExtensionInterfaceDeviceExitRegisterSceneReadyV2>(GDExtension::get_interface_function("device_exit_register_scene_ready_v2"));
	REQUIRE(v2 != nullptr);
	if (!v2) {
		return;
	}
	for (bool first : { false, true }) {
		IssuerFixture fixture;
		v2_registration_v1_first = first;
		v2_registration_bodies = 0;
		issuer_before_register = [](GuardRegistrationV1 &r) { r.entry_classes |= GDEXTENSION_DEVICE_EXIT_ENTRY_SCENE_READY_V1; };
		issuer_init_hook = [](InitializationLevelContextRef frame) {
			const auto install = reinterpret_cast<GDExtensionInterfaceDeviceExitRegisterSceneReadyV2>(GDExtension::get_interface_function("device_exit_register_scene_ready_v2"));
			auto &registry = Registry::get_singleton();
			const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
			const GDExtensionSceneInstallRequirementsV2 q{ { 1, 32, 0 }, 1, 2, 0 };
			const auto refs = Access::private_refs(*owner);
			const GDExtensionDeviceExitInitializationLevelContextRef c{ frame.registry_instance, frame.handle_serial };
			int wrong_userdata = 0;
			CHECK(install(&q, c, registration_body_v2, &wrong_userdata) == ERR_INVALID_DATA);
			CHECK(Access::private_refs(*owner) == refs);
#ifdef THREADS_ENABLED
			int32_t wrong = OK;
			std::thread worker([&] { wrong = install(&q, c, registration_body_v2, nullptr); });
			worker.join();
			CHECK(wrong != OK);
			CHECK(Access::private_refs(*owner) == refs);
#endif
			if (v2_registration_v1_first) {
				REQUIRE(registry.register_scene_ready(&abi, frame, registration_body_v1, nullptr) == OK);
				CHECK(install(&q, c, registration_body_v2, nullptr) == ERR_INVALID_DATA);
			} else {
				REQUIRE(install(&q, c, registration_body_v2, nullptr) == OK);
				CHECK(registry.register_scene_ready(&abi, frame, registration_body_v1, nullptr) == ERR_INVALID_DATA);
				CHECK(install(&q, c, registration_body_v2, nullptr) == ERR_INVALID_DATA);
			}
			CHECK(Access::private_refs(*owner) == refs);
			CHECK(v2_registration_bodies == 0);
			REQUIRE(registry.after_binding_initialize(&abi, frame) == OK);
			issuer_skip_init_ack = true;
			CHECK(install(&q, c, registration_body_v2, nullptr) == ERR_UNAUTHORIZED);
			CHECK(Access::private_refs(*owner) == refs);
		};
		REQUIRE(fixture.open() == OK);
		REQUIRE(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
		CHECK(v2_registration_bodies == 0);
		const auto pending_refs = Access::private_refs(*fixture.extension.ptr());
		CHECK(Access::cancel_ready(*fixture.extension.ptr(), CloseRoute::EXPLICIT) == ERR_INVALID_DATA);
		CHECK(Access::private_refs(*fixture.extension.ptr()) == pending_refs);
		REQUIRE(Access::cancel_ready(*fixture.extension.ptr(), CloseRoute::SETUP_ABORT) == OK);
		CHECK(Access::private_refs(*fixture.extension.ptr()) + 1 == pending_refs);
		CHECK(Access::ready_reservation_retired(*fixture.extension.ptr()));
		CHECK(Access::cancel_ready(*fixture.extension.ptr(), CloseRoute::SETUP_ABORT) == OK);
		CHECK(Access::private_refs(*fixture.extension.ptr()) + 1 == pending_refs);
		fixture.finish();
		CHECK(v2_registration_bodies == 0);
	}
}
static int32_t ready_no_effects(void *, GDExtensionDeviceExitSceneReadyContextRef) {
	return OK;
}
class SceneReadyCountingLoader : public GDExtensionFunctionLoader {
public:
	int close_count = 0;
	Error close_library() override {
		close_count++;
		return GDExtensionFunctionLoader::close_library();
	}
};
static int ready_abort_user_returns = 0;
static int ready_abort_binding_returns = 0;
TEST_CASE("device_exit::issuer/scene-ready-init-abort-keeps-original-error-and-single-close") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	Ref<SceneReadyCountingLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(issuer_entry);
	loader->set_device_exit_guard_version(1);
	fixture.loader = loader;
	ready_abort_user_returns = ready_abort_binding_returns = 0;
	issuer_before_register = [](GuardRegistrationV1 &registration) { registration.entry_classes |= GDEXTENSION_DEVICE_EXIT_ENTRY_SCENE_READY_V1; };
	issuer_term_hook = [](DeinitLevelContextRef, int phase) {
		if (phase == 1) {
			ready_abort_user_returns++;
		}
		if (phase == 2) {
			ready_abort_binding_returns++;
		}
	};
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), ERR_UNAVAILABLE);
	RecordRef original;
	REQUIRE_EQ(Access::retain_record(*fixture.extension.ptr(), original), OK);
	CHECK_EQ(Access::abort_ready(*fixture.extension.ptr(), ERR_UNAVAILABLE), ERR_UNAVAILABLE);
	CHECK_FALSE(fixture.extension->is_library_open());
	CHECK_EQ(loader->close_count, 1);
	CHECK_EQ(ready_abort_user_returns, 1);
	CHECK_EQ(ready_abort_binding_returns, 1);
	CoreCloseReturnFact fact;
	REQUIRE_EQ(Registry::get_singleton().resolve_core_close_return(original, fact), OK);
	CHECK(fact.route == CloseRoute::INIT_ABORT);
	CHECK(fact.close_returned);
	fixture.extension.unref();
	CHECK_EQ(loader->close_count, 1); // No destructor LAST_REF retry.
}
static int materialized_ready_calls = 0;
static int32_t count_materialized_ready(void *, GDExtensionDeviceExitSceneReadyContextRef) {
	materialized_ready_calls++;
	return OK;
}
TEST_CASE("device_exit::issuer/scene-ready-missing-registration-reports-after-real-return") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	issuer_before_register = [](GuardRegistrationV1 &registration) { registration.entry_classes |= GDEXTENSION_DEVICE_EXIT_ENTRY_SCENE_READY_V1; };
	REQUIRE_EQ(fixture.open(), OK);
	CHECK_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), ERR_UNAVAILABLE);
	CHECK_EQ(issuer_ack, OK); // The real early wrapper actually acknowledged/returned.
	CHECK_EQ(Access::level(*fixture.extension.ptr()), 2);
	fixture.finish(); // Early USER/BINDING remain reachable, no Ready body ran.
}
TEST_CASE("device_exit::issuer/scene-ready-requires-the-real-materialization-seam") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	issuer_before_register = [](GuardRegistrationV1 &registration) { registration.entry_classes |= GDEXTENSION_DEVICE_EXIT_ENTRY_SCENE_READY_V1; };
	issuer_init_hook = [](InitializationLevelContextRef frame) {
		const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
		REQUIRE_EQ(Registry::get_singleton().register_scene_ready(&abi, frame, ready_no_effects, nullptr), OK);
	};
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	const bool old = Access::ready_seam(false);
	CHECK_EQ(Access::dispatch_materialized_ready(*fixture.extension.ptr()), OK);
	CHECK_EQ(fixture.extension->preflight_exit(CloseRoute::EXPLICIT), ERR_BUSY);
	Access::ready_seam(old);
	REQUIRE_EQ(Access::cancel_ready(*fixture.extension.ptr(), CloseRoute::SETUP_ABORT), OK);
	fixture.finish();
}
TEST_CASE("device_exit::issuer/scene-ready-no-selected-tree-rejects-before-effects") {
	using namespace GDExtensionDeviceExit;
	REQUIRE(OS::get_singleton()->get_main_loop() == nullptr); // Actual --test harness, before Main::start.
	IssuerFixture fixture;
	materialized_ready_calls = 0;
	issuer_before_register = [](GuardRegistrationV1 &registration) { registration.entry_classes |= GDEXTENSION_DEVICE_EXIT_ENTRY_SCENE_READY_V1; };
	issuer_init_hook = [](InitializationLevelContextRef frame) {
		const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
		REQUIRE_EQ(Registry::get_singleton().register_scene_ready(&abi, frame, count_materialized_ready, nullptr), OK);
	};
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	struct Restore {
		bool seam;
		~Restore() { Access::ready_seam(seam); }
	} restore{ Access::ready_seam(true) };
	CHECK_EQ(Access::dispatch_materialized_ready(*fixture.extension.ptr()), ERR_UNAVAILABLE);
	CHECK_EQ(materialized_ready_calls, 0);
	CHECK_FALSE(Access::quiescing(*fixture.extension.ptr()));
	fixture.finish();
}
TEST_CASE("device_exit::issuer/scene-ready-late-load-failure-closes-before-publication") {
	using namespace GDExtensionDeviceExit;
	REQUIRE(OS::get_singleton()->get_main_loop() == nullptr);
	REQUIRE(Access::manager_level() >= GDExtension::INITIALIZATION_LEVEL_SCENE);
	IssuerFixture fixture;
	Ref<SceneReadyCountingLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(issuer_entry);
	loader->set_device_exit_guard_version(1);
	materialized_ready_calls = 0;
	ready_abort_user_returns = ready_abort_binding_returns = 0;
	issuer_before_register = [](GuardRegistrationV1 &registration) { registration.entry_classes |= GDEXTENSION_DEVICE_EXIT_ENTRY_SCENE_READY_V1; };
	issuer_init_hook = [](InitializationLevelContextRef) {
		const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
		InitializationLevelContextRef scene;
		auto &registry = Registry::get_singleton();
		if (registry.borrow_initialize_level(&abi, owner, 2, &scene) == OK) {
			REQUIRE_EQ(registry.register_scene_ready(&abi, scene, count_materialized_ready, nullptr), OK);
		}
	};
	issuer_term_hook = [](DeinitLevelContextRef, int phase) {
		if (phase == 1) {
			ready_abort_user_returns++;
		}
		if (phase == 2) {
			ready_abort_binding_returns++;
		}
	};
	struct Restore {
		bool seam;
		~Restore() { Access::ready_seam(seam); }
	} restore{ Access::ready_seam(true) };
	const String path = "libgodot://scene-ready-late-failure";
	auto *manager = GDExtensionManager::get_singleton();
	CHECK(manager->load_extension_with_loader(path, loader) == GDExtensionManager::LOAD_STATUS_FAILED);
	CHECK_FALSE(manager->is_extension_loaded(path));
	CHECK_EQ(materialized_ready_calls, 0);
	CHECK_EQ(loader->close_count, 1);
	CHECK_EQ(ready_abort_user_returns, Access::manager_level() - 1);
	CHECK_EQ(ready_abort_binding_returns, Access::manager_level() - 1);
}
TEST_CASE("device_exit::issuer/scene-ready-extra-reservation-is-prevalidated") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	issuer_before_register = [](GuardRegistrationV1 &registration) {
		registration.entry_classes |= GDEXTENSION_DEVICE_EXIT_ENTRY_SCENE_READY_V1;
		const uint32_t old = Access::ready_private_budget(*owner, 5); // Old control + four level frames only.
		GuardToken token{};
		const Error error = Registry::get_singleton().register_guard(owner, &registration, &token);
		Access::issuer_limit(2, old);
		CHECK_EQ(error, ERR_PARAMETER_RANGE_ERROR);
		CHECK_EQ(token.registry_instance, 0);
		CHECK_EQ(token.handle_serial, 0);
	};
	REQUIRE_EQ(fixture.open(), OK);
	// No SCENE body ran; the unused reservation is retired by the original close.
	fixture.finish();
}
TEST_CASE("device_exit::issuer/scene-ready-requires-original-opt-in") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	issuer_init_hook = [](InitializationLevelContextRef frame) {
		const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
		CHECK_EQ(Registry::get_singleton().register_scene_ready(&abi, frame, ready_no_effects, nullptr), ERR_UNAVAILABLE);
	};
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	fixture.finish();
}
static Object *ready_created_object = nullptr;
static int32_t ready_create_and_schedule(void *, GDExtensionDeviceExitSceneReadyContextRef ready) {
	using namespace GDExtensionDeviceExit;
	auto &registry = Registry::get_singleton();
	const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
	const uint32_t before_binding = Access::binding_holders(*owner);
	const uint32_t before_cleanup = Access::cleanup_reservations(*owner);
	ready_created_object = create_real_binding_holder(owner);
	if (!ready_created_object) {
		return ERR_UNAVAILABLE;
	}
	CHECK_EQ(Access::binding_holders(*owner), before_binding + 2); // Original cleanup + binding holders.
	CHECK_EQ(Access::cleanup_reservations(*owner), before_cleanup + 1);
	const uint64_t id = uint64_t(ready_created_object->get_instance_id());
	CHECK_EQ(registry.schedule_scene_ready_object_cleanup(&abi, ready, 2, id), ERR_INVALID_DATA);
	// An intervening rejected attempt must invalidate that old fatal receipt.
	CHECK_EQ(registry.schedule_scene_ready_object_cleanup(nullptr, ready, 0, id), ERR_INVALID_PARAMETER);
	CHECK_EQ(registry.scene_ready_abort_unscheduled_pair(&abi, ready, ERR_INVALID_DATA, 1, id), ERR_INVALID_DATA);
	CHECK_EQ(registry.schedule_scene_ready_object_cleanup(&abi, ready, 0, id), OK);
	CHECK_EQ(registry.schedule_scene_ready_object_cleanup(&abi, ready, 0, id), ERR_INVALID_DATA);
	CHECK_EQ(registry.scene_ready_abort_unscheduled_pair(&abi, ready, ERR_INVALID_DATA, 1, id), ERR_INVALID_DATA);
	return OK;
}
TEST_CASE("device_exit::issuer/scene-ready-materializes-original-pair-before-scheduling") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	ready_created_object = nullptr;
	issuer_before_register = [](GuardRegistrationV1 &registration) { registration.entry_classes |= GDEXTENSION_DEVICE_EXIT_ENTRY_SCENE_READY_V1 | (1u << 6); };
	issuer_init_hook = [](InitializationLevelContextRef frame) {
		const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
		REQUIRE_EQ(Registry::get_singleton().register_scene_ready(&abi, frame, ready_create_and_schedule, nullptr), OK);
	};
	issuer_term_hook = [](DeinitLevelContextRef, int phase) {
		if (phase == 1 && ready_created_object) {
			memdelete(ready_created_object);
			ready_created_object = nullptr;
		}
	};
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	REQUIRE_EQ(Access::dispatch_ready(*fixture.extension.ptr()), OK);
	fixture.finish();
	CHECK(ready_created_object == nullptr);
}
class SceneReadyAbortObject : public Object {
public:
	~SceneReadyAbortObject() { std::fputs("SCENE_READY_UNEXPECTED_OBJECT_DESTRUCTOR\n", stderr); }
};
static int32_t ready_abort_real_pair(void *, GDExtensionDeviceExitSceneReadyContextRef ready) {
	const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
	auto *object = memnew(SceneReadyAbortObject);
	const GDExtensionInstanceBindingCallbacks callbacks = {
		[](void *, void *instance) -> void * { return instance; },
		[](void *, void *, void *) { std::fputs("SCENE_READY_UNEXPECTED_BINDING_FREE\n", stderr); }, nullptr
	};
	if (object->get_instance_binding(owner, &callbacks) != object) {
		std::_Exit(91);
	}
	const auto schedule = reinterpret_cast<GDExtensionInterfaceDeviceExitSceneReadyScheduleObjectCleanupV1>(GDExtension::get_interface_function("device_exit_scene_ready_schedule_object_cleanup_v1"));
	const auto abort_pair = reinterpret_cast<GDExtensionInterfaceDeviceExitSceneReadyAbortV1>(GDExtension::get_interface_function("device_exit_scene_ready_abort_v1"));
	if (!schedule || !abort_pair) {
		std::_Exit(92);
	}
	const uint64_t id = uint64_t(object->get_instance_id());
	const int32_t error = schedule(&abi, ready, 2, id);
	if (error != ERR_INVALID_DATA) {
		std::_Exit(93);
	}
	abort_pair(&abi, ready, error, GDEXTENSION_DEVICE_EXIT_SCENE_READY_ABORT_UNSCHEDULED_PAIR_V1, id);
	std::fputs("SCENE_READY_UNEXPECTED_ABORT_RETURN\n", stderr);
	std::_Exit(94);
}
static void scene_ready_unscheduled_pair_child() {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	issuer_child_mode = true;
	issuer_before_register = [](GuardRegistrationV1 &registration) { registration.entry_classes |= GDEXTENSION_DEVICE_EXIT_ENTRY_SCENE_READY_V1 | (1u << 6); };
	issuer_init_hook = [](InitializationLevelContextRef frame) {
		const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
		if (Registry::get_singleton().register_scene_ready(&abi, frame, ready_abort_real_pair, nullptr) != OK) {
			std::_Exit(95);
		}
	};
	if (fixture.open() != OK || fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) != OK) {
		std::_Exit(96);
	}
	Access::dispatch_ready(*fixture.extension.ptr());
	std::_Exit(97);
}
REGISTER_TEST_COMMAND("--device-exit-scene-ready-unscheduled-pair", scene_ready_unscheduled_pair_child);
#ifdef TOOLS_ENABLED
static Object *binding_cleanup_objects[3] = {};
static int binding_cleanup_frees[3] = {};
TEST_CASE("device_exit::issuer/clear-bindings-survives-callback-untrack-and-object-delete") {
	auto *manager = GDExtensionManager::get_singleton();
	auto *engine = Engine::get_singleton();
	struct Restore {
		bool reloading;
		~Restore() { Engine::get_singleton()->set_extension_reloading_enabled(reloading); }
	} restore{ engine->is_extension_reloading_enabled() };
	engine->set_extension_reloading_enabled(true);
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function([](GDExtensionInterfaceGetProcAddress, GDExtensionClassLibraryPtr, GDExtensionInitialization *initialization) -> GDExtensionBool {
		*initialization = {};
		initialization->initialize = [](void *, GDExtensionInitializationLevel) {};
		initialization->deinitialize = [](void *, GDExtensionInitializationLevel) {};
		return true;
	});
	const String path = "libgodot://binding-clear-reentrant";
	REQUIRE(manager->load_extension_with_loader(path, loader) == GDExtensionManager::LOAD_STATUS_OK);
	Ref<GDExtension> extension = manager->get_extension(path);
	extension->set_reloadable(true);
	const GDExtensionInstanceBindingCallbacks callbacks = {
		[](void *, void *instance) -> void * { return instance; },
		[](void *token, void *instance, void *) {
			for (int i = 0; i < 3; i++) {
				if (instance == binding_cleanup_objects[i]) {
					binding_cleanup_frees[i]++;
				}
			}
			if (instance == binding_cleanup_objects[0]) {
				GDExtensionManager::get_singleton()->untrack_instance_binding(token, binding_cleanup_objects[0]);
				// The real Object predelete also untracks the later owner from this same
				// manager vector; the middle live owner must still be visited exactly once.
				memdelete(binding_cleanup_objects[2]);
				binding_cleanup_objects[2] = nullptr;
			}
		},
		nullptr
	};
	for (int i = 0; i < 3; i++) {
		binding_cleanup_frees[i] = 0;
		binding_cleanup_objects[i] = memnew(Object);
		REQUIRE(binding_cleanup_objects[i]->get_instance_binding(extension.ptr(), &callbacks) == binding_cleanup_objects[i]);
	}
	Access::clear_bindings(*extension.ptr());
	CHECK_EQ(binding_cleanup_frees[0], 1);
	CHECK_EQ(binding_cleanup_frees[1], 1);
	CHECK_EQ(binding_cleanup_frees[2], 1);
	for (int i = 0; i < 3; i++) {
		if (binding_cleanup_objects[i]) {
			memdelete(binding_cleanup_objects[i]);
			binding_cleanup_objects[i] = nullptr;
		}
	}
	for (int32_t i = Access::level(*extension.ptr()); i >= 0; i--) {
		REQUIRE_EQ(extension->deinitialize_library(GDExtension::InitializationLevel(i)), OK);
	}
	REQUIRE_EQ(extension->close_library(), OK);
	Access::forget_closed_test_extension(path);
}
#endif
static GDExtensionDeviceExitSceneReadyContextRef last_ready_frame{};
static int ready_calls = 0;
static int32_t ready_check_frame(void *, GDExtensionDeviceExitSceneReadyContextRef ready) {
	using namespace GDExtensionDeviceExit;
	ready_calls++;
	last_ready_frame = ready;
	const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
	InitializationLevelContextRef initializer{};
	CHECK_EQ(Registry::get_singleton().borrow_initialize_level(&abi, owner, 2, &initializer), ERR_UNAUTHORIZED);
	CHECK_EQ(owner->preflight_exit(CloseRoute::EXPLICIT), ERR_BUSY);
	CHECK_EQ(Registry::get_singleton().schedule_scene_ready_object_cleanup(&abi, ready, 2, 0), ERR_INVALID_DATA);
	return INT32_MAX; // Undefined callback Error must never become success.
}
TEST_CASE("device_exit::issuer/scene-ready-once-fresh-frame-and-unknown-error") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	ready_calls = 0;
	issuer_before_register = [](GuardRegistrationV1 &registration) { registration.entry_classes |= GDEXTENSION_DEVICE_EXIT_ENTRY_SCENE_READY_V1; };
	issuer_init_hook = [](InitializationLevelContextRef frame) {
		const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
		REQUIRE_EQ(Registry::get_singleton().register_scene_ready(&abi, frame, ready_check_frame, nullptr), OK);
	};
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	CHECK_EQ(Access::dispatch_ready(*fixture.extension.ptr()), ERR_INVALID_DATA);
	CHECK_EQ(ready_calls, 1);
	CHECK_EQ(Access::dispatch_ready(*fixture.extension.ptr()), ERR_INVALID_DATA);
	CHECK_EQ(ready_calls, 1);
	const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
	CHECK_NE(Registry::get_singleton().schedule_scene_ready_object_cleanup(&abi, last_ready_frame, 0, 1), OK);
	fixture.finish();
}
TEST_CASE("device_exit::issuer/scene-ready-registration-pending-is-not-quiesced") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	issuer_before_register = [](GuardRegistrationV1 &registration) { registration.entry_classes |= GDEXTENSION_DEVICE_EXIT_ENTRY_SCENE_READY_V1; };
	issuer_init_hook = [](InitializationLevelContextRef frame) {
		const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
		auto &registry = Registry::get_singleton();
		CHECK_EQ(registry.register_scene_ready(&abi, frame, ready_no_effects, reinterpret_cast<void *>(1)), ERR_INVALID_DATA);
		CHECK_EQ(registry.register_scene_ready(&abi, frame, ready_no_effects, nullptr), OK);
		CHECK_EQ(registry.register_scene_ready(&abi, frame, ready_no_effects, nullptr), ERR_INVALID_DATA);
	};
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
	CHECK_NE(Registry::get_singleton().register_scene_ready(&abi, issuer_stale_context, ready_no_effects, nullptr), OK);
	CHECK_EQ(fixture.extension->preflight_exit(CloseRoute::EXPLICIT), ERR_BUSY);
	CHECK_FALSE(Access::quiescing(*fixture.extension.ptr()));
	{
		RecordRef retained;
		REQUIRE_EQ(Access::retain_record(*fixture.extension.ptr(), retained), OK);
		CallbackScope callback(retained);
		REQUIRE_EQ(callback.get_error(), OK);
		CHECK_EQ(Access::cancel_ready(*fixture.extension.ptr(), CloseRoute::SETUP_ABORT), ERR_UNAUTHORIZED);
	}
	CHECK_EQ(Access::cancel_ready(*fixture.extension.ptr(), CloseRoute::EXPLICIT), ERR_INVALID_DATA);
	CHECK_EQ(Access::cancel_ready(*fixture.extension.ptr(), CloseRoute::SETUP_ABORT), OK);
	CHECK_EQ(Access::cancel_ready(*fixture.extension.ptr(), CloseRoute::SETUP_ABORT), OK);
	CHECK_EQ(Access::cancel_ready(*fixture.extension.ptr(), CloseRoute::SHUTDOWN), ERR_INVALID_DATA);
	fixture.finish();
}
// Uses the existing real FunctionLoader guard enrollment and init-frame fixture.
static Object *session_device = nullptr;
static GDExtensionDeviceRegistryContextRef session_context{};
static GDExtensionOriginalDeviceSessionRef original_session{};
TEST_CASE("device_exit::issuer/device-session-original-context") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	session_device = memnew(Object);
	REQUIRE_EQ(Access::session_create(session_device), OK);
	session_context = {};
	original_session = {};
	issuer_init_hook = [](InitializationLevelContextRef frame) {
		auto &r = Registry::get_singleton();
		const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
		REQUIRE_EQ(r.bind_device_session(&abi, frame, session_device, &session_context, &original_session).native_error, OK);
		GDExtensionDeviceSessionViewV1 view{};
		REQUIRE_EQ(r.copy_device_session(session_context, original_session, true, &view).native_error, OK);
		CHECK_EQ(view.session, 1);
		CHECK_NE(view.device_epoch, 0);
		CHECK_EQ(view.new_work, 1);
		GDExtensionDeviceRegistryContextRef duplicate_context{};
		GDExtensionOriginalDeviceSessionRef duplicate_session{};
		REQUIRE_EQ(r.bind_device_session(&abi, frame, session_device, &duplicate_context, &duplicate_session).native_error, OK);
		REQUIRE_EQ(r.copy_device_session(duplicate_context, original_session, true, &view).native_error, OK);
		CHECK_EQ(view.session, 1);
		CHECK_EQ(r.release_device_reference(duplicate_session.registry_instance, duplicate_session.handle_serial, true).native_error, OK);
		CHECK_EQ(r.release_device_reference(duplicate_context.registry_instance, duplicate_context.handle_serial, false).native_error, OK);
		GDExtensionOriginalDeviceSessionRef resolved{};
		CHECK_EQ(r.resolve_device_session(session_context, view.device_epoch + 1, 1, &resolved).reason, 6);
		CHECK_EQ(r.resolve_device_session(session_context, view.device_epoch, 0, &resolved).reason, 3);
		CHECK_EQ(r.resolve_device_session(session_context, view.device_epoch, 2, &resolved).reason, 7);
		auto foreign = session_context;
		foreign.registry_instance ^= uint64_t(1) << 63;
		CHECK_EQ(r.resolve_device_session(foreign, view.device_epoch, 1, &resolved).reason, 5);
		CHECK_EQ(r.resolve_device_session(session_context, view.device_epoch, 1, &resolved).native_error, OK);
		CHECK_EQ(r.release_device_reference(resolved.registry_instance, resolved.handle_serial, true).native_error, OK);
	};
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	auto &r = Registry::get_singleton();
	const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
	GDExtensionDeviceRegistryContextRef denied_context{};
	GDExtensionOriginalDeviceSessionRef denied_session{};
	CHECK_NE(r.bind_device_session(&abi, issuer_stale_context, session_device, &denied_context, &denied_session).native_error, OK);
	Access::session_lost(session_device);
	Access::session_close(session_device);
	GDExtensionDeviceSessionViewV1 view{};
	CHECK_EQ(r.copy_device_session(session_context, original_session, true, &view).reason, 10);
	REQUIRE_EQ(r.copy_device_session(session_context, original_session, false, &view).native_error, OK);
	CHECK_EQ(view.state, 2);
	CHECK_EQ(view.new_work, 0);
	Access::session_destroy(session_device);
	memdelete(session_device);
	session_device = nullptr;
	REQUIRE_EQ(r.copy_device_session(session_context, original_session, false, &view).native_error, OK);
	CHECK_EQ(view.state, 3);
	CHECK_EQ(view.session, 1);
	CHECK_EQ(r.release_device_reference(original_session.registry_instance, original_session.handle_serial, true).native_error, OK);
	CHECK_EQ(r.copy_device_session(session_context, original_session, false, &view).reason, 8);
	CHECK_EQ(r.release_device_reference(session_context.registry_instance, session_context.handle_serial, false).native_error, OK);
	CHECK_EQ(Access::session_record_count(), 0);
	fixture.finish();
}

TEST_CASE("device_exit::issuer/device-session-retained-through-user-then-release") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	session_device = memnew(Object);
	REQUIRE_EQ(Access::session_create(session_device), OK);
	session_context = {};
	original_session = {};
	issuer_init_hook = [](InitializationLevelContextRef frame) {
		const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
		REQUIRE_EQ(Registry::get_singleton().bind_device_session(&abi, frame, session_device, &session_context, &original_session).native_error, OK);
	};
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	auto &r = Registry::get_singleton();
	// The device references are core owned metadata; they do not grant normal
	// callbacks after G1. They retain the original association through USER.
	REQUIRE_EQ(fixture.extension->preflight_exit(CloseRoute::EXPLICIT), OK);
	GDExtensionDeviceSessionViewV1 view{};
	CHECK_EQ(r.copy_device_session(session_context, original_session, true, &view).native_error, ERR_UNAUTHORIZED);
	CHECK_EQ(r.copy_device_session(session_context, original_session, false, &view).native_error, OK);
	REQUIRE_EQ(fixture.extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	CHECK_EQ(fixture.extension->close_library(), ERR_BUSY);
	CHECK_EQ(r.release_device_reference(original_session.registry_instance, original_session.handle_serial, true).native_error, OK);
	CHECK_EQ(fixture.extension->close_library(), ERR_BUSY);
	CHECK_EQ(r.release_device_reference(session_context.registry_instance, session_context.handle_serial, false).native_error, OK);
	Access::session_destroy(session_device);
	memdelete(session_device);
	session_device = nullptr;
	CHECK_EQ(fixture.extension->close_library(), OK);
}

TEST_CASE("device_exit::issuer/device-session-quota-and-terminal-counter") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	session_device = memnew(Object);
	REQUIRE_EQ(Access::session_create(session_device), OK);
	Access::session_counter(session_device, UINT64_MAX, true);
	session_context = {};
	original_session = {};
	issuer_init_hook = [](InitializationLevelContextRef frame) {
		auto &r = Registry::get_singleton();
		const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
		auto version = abi;
		version.abi_version = 2;
		CHECK_EQ(r.bind_device_session(&version, frame, session_device, &session_context, &original_session).reason, 1);
		CHECK_EQ(r.bind_device_session(&abi, {}, session_device, &session_context, &original_session).reason, 4);
		CHECK_EQ(r.bind_device_session(&abi, frame, session_device, &session_context, &original_session).reason, 12);
		CHECK_EQ(session_context.handle_serial, 0);
		CHECK_EQ(original_session.handle_serial, 0);
		Access::session_counter(session_device, UINT64_MAX, false);
		const uint32_t quota = Access::issuer_limit(2, 0);
		auto denied = r.bind_device_session(&abi, frame, session_device, &session_context, &original_session);
		CHECK_EQ(denied.reason, 11);
		CHECK_EQ(denied.expected, 0);
		CHECK_GT(denied.actual, 0);
		Access::issuer_limit(2, quota);
		REQUIRE_EQ(r.bind_device_session(&abi, frame, session_device, &session_context, &original_session).native_error, OK);
		GDExtensionDeviceSessionViewV1 view{};
		REQUIRE_EQ(r.copy_device_session(session_context, original_session, true, &view).native_error, OK);
		CHECK_EQ(view.session, UINT64_MAX);
		GDExtensionDeviceRegistryContextRef repeat{};
		GDExtensionOriginalDeviceSessionRef repeated{};
		REQUIRE_EQ(r.bind_device_session(&abi, frame, session_device, &repeat, &repeated).native_error, OK);
		REQUIRE_EQ(r.copy_device_session(repeat, repeated, true, &view).native_error, OK);
		CHECK_EQ(view.session, UINT64_MAX);
		CHECK_EQ(r.release_device_reference(repeated.registry_instance, repeated.handle_serial, true).native_error, OK);
		CHECK_EQ(r.release_device_reference(repeat.registry_instance, repeat.handle_serial, false).native_error, OK);
	};
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	auto &r = Registry::get_singleton();
	CHECK_EQ(r.release_device_reference(original_session.registry_instance, original_session.handle_serial, true).native_error, OK);
	CHECK_EQ(r.release_device_reference(session_context.registry_instance, session_context.handle_serial, false).native_error, OK);
	// Releasing every attachment must not reclaim a live RD or reset its counter.
	CHECK_EQ(Access::session_record_count(), 1);
	Access::session_destroy(session_device);
	memdelete(session_device);
	session_device = nullptr;
	CHECK_EQ(Access::session_record_count(), 0);
	fixture.finish();
}

TEST_CASE("device_exit::issuer/device-session-reload-keeps-live-epoch") {
	using namespace GDExtensionDeviceExit;
	session_device = memnew(Object);
	REQUIRE_EQ(Access::session_create(session_device), OK);
	uint64_t epoch = 0;
	for (int attachment = 0; attachment < 2; ++attachment) {
		IssuerFixture fixture;
		session_context = {};
		original_session = {};
		issuer_init_hook = [](InitializationLevelContextRef frame) {
			const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
			REQUIRE_EQ(Registry::get_singleton().bind_device_session(&abi, frame, session_device, &session_context, &original_session).native_error, OK);
		};
		REQUIRE_EQ(fixture.open(), OK);
		REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
		auto &r = Registry::get_singleton();
		GDExtensionDeviceSessionViewV1 view{};
		REQUIRE_EQ(r.copy_device_session(session_context, original_session, true, &view).native_error, OK);
		if (attachment == 0) {
			epoch = view.device_epoch;
		} else {
			CHECK_EQ(view.device_epoch, epoch);
		}
		CHECK_EQ(view.session, 1);
		CHECK_EQ(r.release_device_reference(original_session.registry_instance, original_session.handle_serial, true).native_error, OK);
		CHECK_EQ(r.release_device_reference(session_context.registry_instance, session_context.handle_serial, false).native_error, OK);
		fixture.finish();
		CHECK_EQ(Access::session_record_count(), 1);
	}
	Access::session_destroy(session_device);
	memdelete(session_device);
	session_device = nullptr;
	CHECK_EQ(Access::session_record_count(), 0);
}

TEST_CASE("device_exit::issuer/device-session-rebuild-rejects-late-loss") {
	auto *object = memnew(Object);
	REQUIRE_EQ(Access::session_create(object), OK);
	uint64_t registry = 0, epoch = 0;
	Access::session_identity(object, registry, epoch);
	REQUIRE_NE(epoch, 0);
	CHECK(Access::session_live(object));
	Access::session_lost_identity(registry, epoch);
	CHECK_FALSE(Access::session_live(object));
	Access::session_destroy(object);
	CHECK_EQ(Access::session_record_count(), 0);
	// Same address models allocation reuse; only original core creates the epoch.
	REQUIRE_EQ(Access::session_create(object), OK);
	uint64_t next_registry = 0, next_epoch = 0;
	Access::session_identity(object, next_registry, next_epoch);
	CHECK_EQ(next_registry, registry);
	CHECK_GT(next_epoch, epoch);
	Access::session_lost_identity(registry, epoch);
	CHECK(Access::session_live(object));
	Access::session_lost_identity(registry + 1, next_epoch);
	CHECK(Access::session_live(object));
	Access::session_destroy(object);
	memdelete(object);
	CHECK_EQ(Access::session_record_count(), 0);
}

static GDExtensionDeviceExit::LibraryCodeHoldRef issuer_user_hold;
static GDExtensionDeviceExit::LibraryCodeHoldRef issuer_binding_hold;
static GDExtensionDeviceExit::LibraryCodeHoldRef issuer_clone_hold;
static void issuer_acquire_holds(GDExtensionDeviceExit::InitializationLevelContextRef context) {
	using namespace GDExtensionDeviceExit;
	const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
	auto &registry = Registry::get_singleton();
	GuardedLibraryRef library;
	RegisteredCleanupRef user, binding;
	CHECK_EQ(registry.borrow_guarded_library(&abi, owner, &library), OK);
	CHECK_EQ(registry.borrow_level_cleanup(&abi, context, 0, &user), OK);
	CHECK_EQ(registry.borrow_level_cleanup(&abi, context, 1, &binding), OK);
	issuer_user_hold = {};
	issuer_binding_hold = {};
	issuer_clone_hold = {};
	CHECK_EQ(registry.acquire_code_hold(&abi, library, user, &issuer_user_hold), OK);
	CHECK_EQ(registry.acquire_code_hold(&abi, library, binding, &issuer_binding_hold), OK);
	CHECK_EQ(registry.clone_code_hold(&abi, issuer_user_hold, &issuer_clone_hold), OK);
	CHECK_NE(issuer_user_hold.handle_serial, issuer_clone_hold.handle_serial);
	CHECK_EQ(Access::binding_holders(*owner), 3);
}
TEST_CASE("device_exit::issuer/holds-phase-order-and-real-return") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	issuer_init_hook = issuer_acquire_holds;
	issuer_term_hook = [](DeinitLevelContextRef context, int stage) {
		const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
		auto &registry = Registry::get_singleton();
		if (stage == 1) {
			CHECK_EQ(registry.after_binding_deinit(&abi, context), ERR_INVALID_DATA);
			CHECK_EQ(registry.after_user_terminator(&abi, context), ERR_BUSY);
			CHECK_EQ(registry.release_code_hold(&abi, issuer_binding_hold), ERR_UNAUTHORIZED);
			CHECK_EQ(registry.release_code_hold(&abi, issuer_user_hold), OK);
			CHECK_EQ(registry.release_code_hold(&abi, issuer_clone_hold), OK);
			CHECK_EQ(registry.release_code_hold(&abi, issuer_user_hold), ERR_INVALID_DATA);
		} else {
			CHECK_EQ(registry.after_binding_deinit(&abi, context), ERR_BUSY);
			CHECK_EQ(registry.release_code_hold(&abi, issuer_binding_hold), OK);
		}
		CHECK_EQ(Access::binding_holders(*owner), 3);
		CHECK_FALSE(Access::physical_can_close(*owner));
	};
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	CHECK_EQ(Access::binding_holders(*owner), 3);
	const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
	CHECK_EQ(Registry::get_singleton().release_code_hold(&abi, issuer_user_hold), ERR_INVALID_DATA);
	fixture.finish();
	CHECK_EQ(Registry::get_singleton().release_code_hold(&abi, issuer_user_hold), ERR_INVALID_DATA);
}
TEST_CASE("device_exit::issuer/nested-same-generation-release-waits-for-initializer") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	issuer_init_hook = [](InitializationLevelContextRef context) {
		issuer_acquire_holds(context);
		RecordRef record;
		CHECK_EQ(Access::retain_record(*owner, record), OK);
		const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
		{
			CallbackScope nested(record);
			CHECK_EQ(nested.get_error(), OK);
			CHECK_EQ(Registry::get_singleton().release_code_hold(&abi, issuer_user_hold), OK);
			CHECK_EQ(Registry::get_singleton().release_code_hold(&abi, issuer_clone_hold), OK);
			CHECK_EQ(Registry::get_singleton().release_code_hold(&abi, issuer_binding_hold), OK);
		}
		CHECK_EQ(Access::binding_holders(*owner), 3);
	};
	// Clean up still-live cells on the RED implementation so a failed assertion
	// cannot turn the intended failure into an unrelated last-ref fatal stop.
	issuer_term_hook = [](DeinitLevelContextRef, int stage) {
		const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
		if (!Access::binding_holders(*owner)) {
			return;
		}
		auto &registry = Registry::get_singleton();
		if (stage == 1) {
			registry.release_code_hold(&abi, issuer_user_hold);
			registry.release_code_hold(&abi, issuer_clone_hold);
		} else {
			registry.release_code_hold(&abi, issuer_binding_hold);
		}
	};
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	CHECK_EQ(Access::binding_holders(*owner), 0);
	fixture.finish();
}
TEST_CASE("device_exit::issuer/busy-seal-and-control-retry") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	RecordRef record;
	REQUIRE_EQ(Access::retain_record(*owner, record), OK);
	uint64_t attempt;
	{
		CallbackScope active(record);
		CHECK_EQ(active.get_error(), OK);
		CHECK_EQ(fixture.extension->preflight_exit(CloseRoute::EXPLICIT), ERR_BUSY);
		attempt = Access::attempt(*owner);
		CHECK_NE(attempt, 0);
		CallbackScope late(record);
		CHECK_EQ(late.get_error(), ERR_BUSY);
	}
	issuer_preflight_result = ERR_BUSY;
	CHECK_EQ(fixture.extension->preflight_exit(CloseRoute::EXPLICIT), ERR_BUSY);
	CHECK_EQ(Access::attempt(*owner), attempt);
	CHECK_EQ(fixture.extension->preflight_exit(CloseRoute::RELOAD), ERR_INVALID_DATA);
	issuer_preflight_result = OK;
	issuer_preflight_publish = false;
	CHECK_EQ(fixture.extension->preflight_exit(CloseRoute::EXPLICIT), ERR_UNAVAILABLE);
	issuer_preflight_publish = true;
	issuer_preflight_mismatch = true;
	CHECK_EQ(fixture.extension->preflight_exit(CloseRoute::EXPLICIT), ERR_UNAVAILABLE);
	issuer_preflight_mismatch = false;
	const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
	ExitDiagnosticV1 diagnostic;
	diagnostic.abi = { 1, sizeof(ExitDiagnosticV1), 0 };
	CHECK_EQ(Registry::get_singleton().publish_exit_result(&abi, issuer_token, issuer_stale_preflight, &diagnostic), ERR_INVALID_DATA);
	fixture.finish();
}
TEST_CASE("device_exit::issuer/mandatory-deinit-capacity-is-reserved-before-teardown") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	REQUIRE_EQ(fixture.extension->preflight_exit(CloseRoute::EXPLICIT), OK);
	const uint32_t limit = Access::exhaust_callback_refs(*owner);
	Access::exhaust_issuer_sequences(true);
	const Error result = fixture.extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE);
	Access::exhaust_issuer_sequences(false);
	Access::restore_callback_refs(*owner, limit);
	CHECK_EQ(result, OK);
	fixture.finish();
	CHECK(Access::guard_pointers_clear(*fixture.extension.ptr()));
}
TEST_CASE("device_exit::issuer/registration-validation-is-atomic") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	issuer_before_register = [](GuardRegistrationV1 &original) {
		auto &registry = Registry::get_singleton();
		for (int test = 0; test < 9; test++) {
			auto registration = original;
			GuardToken token;
			Error expected = ERR_INVALID_DATA;
			switch (test) {
				case 0:
					registration.abi.struct_size--;
					break;
				case 1:
					registration.abi.reserved = 1;
					break;
				case 2:
					registration.binding_protocol = 2;
					expected = ERR_UNAVAILABLE;
					break;
				case 3:
					registration.entry_classes = uint64_t(1) << 12;
					expected = ERR_UNAVAILABLE;
					break;
				case 4:
					registration.level_bitmap = 4;
					break;
				case 5:
					registration.user_terminator = nullptr;
					break;
				case 6:
					registration.needs[0].user_services = 1;
					break;
				case 7:
					registration.needs[2].user_services = 256;
					expected = ERR_UNAVAILABLE;
					break;
				case 8:
					registration.binding_cleanup_bitmap = 0;
					break;
			}
			CHECK_EQ(registry.register_guard(owner, &registration, &token), expected);
			CHECK_EQ(token.registry_instance, 0);
			CHECK_EQ(token.handle_serial, 0);
		}
		GuardToken filled{ 123, 456 };
		CHECK_EQ(registry.register_guard(owner, &original, &filled), ERR_INVALID_DATA);
		CHECK_EQ(filled.registry_instance, 123);
		CHECK_EQ(filled.handle_serial, 456);
		GuardToken token;
		Error thread_result = OK;
		std::thread worker([&] { thread_result = registry.register_guard(owner, &original, &token); });
		worker.join();
		CHECK_EQ(thread_result, ERR_UNAUTHORIZED);
		RecordRef record;
		CHECK_EQ(Access::retain_record(*owner, record), OK);
		{
			CallbackScope nested(record);
			CHECK_EQ(nested.get_error(), OK);
			CHECK_EQ(registry.register_guard(owner, &original, &token), ERR_UNAUTHORIZED);
		}
	};
	issuer_entry_hook = [](GuardRegistrationV1 &registration, GDExtensionInitialization *) {
		GuardToken duplicate;
		CHECK_EQ(Registry::get_singleton().register_guard(owner, &registration, &duplicate), ERR_ALREADY_IN_USE);
		CHECK_EQ(duplicate.handle_serial, 0);
		RecordRef record;
		CHECK_EQ(Access::retain_record(*owner, record), OK);
		CallbackScope pending(record);
		CHECK_EQ(pending.get_error(), ERR_UNAVAILABLE);
	};
	REQUIRE_EQ(fixture.open(), OK);
	fixture.finish();
	CHECK(Access::guard_pointers_clear(*fixture.extension.ptr()));
}
TEST_CASE("device_exit::issuer/entry-tuple-mismatch-revokes-without-initialization") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	issuer_entry_hook = [](GuardRegistrationV1 &, GDExtensionInitialization *actual) { actual->userdata = owner; };
	ERR_PRINT_OFF;
	const Error result = fixture.open();
	ERR_PRINT_ON;
	CHECK_EQ(result, ERR_INVALID_DATA);
	CHECK_FALSE(fixture.extension->is_library_open());
	CHECK_EQ(Access::level(*fixture.extension.ptr()), -1);
}
TEST_CASE("device_exit::issuer/holder-domains-foreign-top-thread-and-generation") {
	using namespace GDExtensionDeviceExit;
	Ref<GDExtensionFunctionLoader> foreign_loader;
	foreign_loader.instantiate();
	foreign_loader->set_initialization_function(entry);
	Ref<GDExtension> foreign;
	foreign.instantiate();
	REQUIRE_EQ(foreign->open_library("libgodot://foreign-issuer-neighbor", foreign_loader), OK);
	RecordRef foreign_record;
	REQUIRE_EQ(Access::retain_record(*foreign.ptr(), foreign_record), OK);
	static RecordRef *foreign_frame_record;
	foreign_frame_record = &foreign_record;
	IssuerFixture fixture;
	issuer_init_hook = [](InitializationLevelContextRef context) {
		issuer_acquire_holds(context);
		auto &registry = Registry::get_singleton();
		const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
		CHECK_EQ(registry.after_binding_deinit(&abi, { context.registry_instance, context.handle_serial }), ERR_INVALID_DATA);
		CHECK_EQ(registry.release(&abi, { context.registry_instance, context.handle_serial }), ERR_INVALID_DATA);
		RecordRef record;
		CHECK_EQ(Access::retain_record(*owner, record), OK);
		{
			CallbackScope foreign_top(*foreign_frame_record);
			CHECK_EQ(foreign_top.get_error(), OK);
			CHECK_EQ(registry.release_code_hold(&abi, issuer_clone_hold), ERR_INVALID_DATA);
			LibraryCodeHoldRef copy;
			CHECK_EQ(registry.clone_code_hold(&abi, issuer_clone_hold, &copy), ERR_UNAUTHORIZED);
		}
		Error thread_result = OK;
		std::thread worker([&] {
			CallbackScope wrong_thread(record);
			thread_result = registry.release_code_hold(&abi, issuer_clone_hold);
		});
		worker.join();
		CHECK_EQ(thread_result, ERR_UNAUTHORIZED);
		for (int field = 0; field < 2; field++) {
			Access::corrupt_typed_holder(*owner, field);
			CHECK_EQ(registry.release_code_hold(&abi, issuer_clone_hold), ERR_INVALID_DATA);
			Access::corrupt_typed_holder(*owner, field);
		}
		CHECK_EQ(Access::binding_holders(*owner), 3);
		CHECK_EQ(registry.release_code_hold(&abi, issuer_user_hold), OK);
		CHECK_EQ(registry.release_code_hold(&abi, issuer_binding_hold), OK);
		CHECK_EQ(registry.release_code_hold(&abi, issuer_clone_hold), OK);
	};
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	CHECK_EQ(Access::binding_holders(*owner), 0);
	fixture.finish();
	CHECK_EQ(foreign->close_library(), OK);
}
TEST_CASE("device_exit::issuer/enrollment-capacity-refuses-without-publication") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	issuer_before_register = [](GuardRegistrationV1 &registration) {
		auto &registry = Registry::get_singleton();
		for (int which = 0; which < 3; which++) {
			const uint32_t old = Access::issuer_limit(which, 0);
			GuardToken token;
			const Error result = registry.register_guard(owner, &registration, &token);
			Access::issuer_limit(which, old);
			CHECK_EQ(result, ERR_PARAMETER_RANGE_ERROR);
			CHECK_EQ(token.registry_instance, 0);
			CHECK_EQ(token.handle_serial, 0);
		}
		GuardToken token;
		Access::exhaust_issuer_sequences(true);
		const Error result = registry.register_guard(owner, &registration, &token);
		Access::exhaust_issuer_sequences(false);
		CHECK_EQ(result, ERR_PARAMETER_RANGE_ERROR);
		CHECK_EQ(token.handle_serial, 0);
	};
	REQUIRE_EQ(fixture.open(), OK);
	fixture.finish();
}
static void issuer_typed_fatal_child() {
	using namespace GDExtensionDeviceExit;
	bool init = false;
	for (const String &arg : OS::get_singleton()->get_cmdline_args()) {
		init |= arg == "--issuer-init";
	}
	if (init) {
		Registry::get_singleton().terminate_initialize_blocked({}, ERR_BUSY);
	} else {
		Registry::get_singleton().terminate_blocked({}, ERR_BUSY);
	}
}
REGISTER_TEST_COMMAND("--device-exit-issuer-typed-fatal", issuer_typed_fatal_child);
static void issuer_terminal_serial_child() {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	issuer_child_mode = true;
	static int missing = 0;
	for (const String &arg : OS::get_singleton()->get_cmdline_args()) {
		if (arg == "--issuer-handle-short") {
			missing = 1;
		}
		if (arg == "--issuer-frame-short") {
			missing = 2;
		}
		if (arg == "--issuer-action-short") {
			missing = 3;
		}
	}
	issuer_before_register = [](GuardRegistrationV1 &) { Access::issuer_terminal_sequences(missing); };
	const Error opened = fixture.open();
	bool good = false;
	if (missing) {
		good = opened != OK && issuer_registration == ERR_PARAMETER_RANGE_ERROR && !issuer_token.registry_instance && !issuer_token.handle_serial;
	} else {
		good = opened == OK && fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK &&
				fixture.extension->preflight_exit(CloseRoute::EXPLICIT) == OK &&
				fixture.extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK && fixture.extension->close_library() == OK;
	}
	std::fprintf(stderr, "ISSUER_TERMINAL_SERIAL missing=%d good=%d\n", missing, int(good));
	std::_Exit(good ? 0 : 95);
}
REGISTER_TEST_COMMAND("--device-exit-issuer-terminal-serial", issuer_terminal_serial_child);
static void issuer_missing_ack_child() {
	IssuerFixture fixture;
	issuer_child_mode = true;
	bool deinit = false;
	for (const String &arg : OS::get_singleton()->get_cmdline_args()) {
		deinit |= arg == "--issuer-deinit";
	}
	if (fixture.open() != OK) {
		std::_Exit(91);
	}
	issuer_skip_init_ack = !deinit;
	fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE);
	if (deinit) {
		if (fixture.extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) != OK) {
			std::_Exit(92);
		}
		issuer_skip_deinit_ack = true;
		fixture.extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE);
	}
	std::fputs("UNEXPECTED_ISSUER_RETURN\n", stderr);
	std::_Exit(93);
}
REGISTER_TEST_COMMAND("--device-exit-issuer-missing-ack", issuer_missing_ack_child);
static void issuer_core_action_identity_child() {
	using namespace GDExtensionDeviceExit;
	Ref<GDExtension> extension;
	extension.instantiate();
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	if (extension->open_library("libgodot://core-action-identity", loader) != OK) {
		std::_Exit(91);
	}
	RecordRef record;
	Access::retain_record(*extension.ptr(), record);
	bool cleanup_only = false;
	bool cleanup_active = false;
	for (const String &arg : OS::get_singleton()->get_cmdline_args()) {
		cleanup_only |= arg == "--issuer-cleanup-action";
		cleanup_active |= arg == "--issuer-cleanup-active";
	}
	if (cleanup_active) {
		auto *action = memnew(CleanupAction);
		if (Registry::get_singleton().register_cleanup_action(record, 71, Thread::get_caller_id(), 81, *action) != OK) {
			std::_Exit(92);
		}
		CleanupScope scope(*action, record, 71, 81);
		if (scope.get_error() != OK) {
			std::_Exit(92);
		}
		memdelete(action); // Scope retains the last storage reference.
		Access::corrupt_typed_holder(*extension.ptr(), 1);
	} else if (cleanup_only) {
		CleanupAction action;
		if (Registry::get_singleton().register_cleanup_action(record, 71, Thread::get_caller_id(), 81, action) != OK) {
			std::_Exit(92);
		}
		Access::corrupt_typed_holder(*extension.ptr(), 1);
	} else {
		Object *object = memnew(Object);
		const GDExtensionInstanceBindingCallbacks callbacks = {
			[](void *, void *instance) -> void * { return instance; },
			[](void *, void *, void *) {}, nullptr
		};
		if (object->get_instance_binding(extension.ptr(), &callbacks) != object) {
			std::_Exit(92);
		}
		Access::corrupt_typed_holder(*extension.ptr(), 1);
		object->free_instance_binding(extension.ptr());
		memdelete(object);
	}
	std::fputs("UNEXPECTED_CORE_ACTION_RETIREMENT\n", stderr);
	std::_Exit(93);
}
REGISTER_TEST_COMMAND("--device-exit-issuer-core-action-identity", issuer_core_action_identity_child);
static void issuer_pending_abort_child() {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	issuer_child_mode = true;
	static bool mismatch = false;
	for (const String &arg : OS::get_singleton()->get_cmdline_args()) {
		mismatch |= arg == "--issuer-tuple-mismatch";
	}
	issuer_entry_success = mismatch;
	issuer_entry_hook = [](GuardRegistrationV1 &, GDExtensionInitialization *actual) {
		RecordRef record;
		if (Access::retain_record(*owner, record) != OK) {
			std::_Exit(91);
		}
		// Keep an actual bootstrap obligation live across the failed return.
		auto *holder = memnew(BindingLifetime);
		if (holder->acquire(record) != OK) {
			std::_Exit(92);
		}
		if (mismatch) {
			actual->userdata = owner;
		}
		std::fputs("ISSUER_PENDING_OBLIGATION_BEFORE_RETURN\n", stderr);
	};
	fixture.open();
	std::fputs("UNEXPECTED_PENDING_ABORT_RETURN\n", stderr);
	std::_Exit(93);
}
REGISTER_TEST_COMMAND("--device-exit-issuer-pending-abort", issuer_pending_abort_child);
static void issuer_generic_holder_child() {
	using namespace GDExtensionDeviceExit;
	Ref<GDExtension> extension;
	extension.instantiate();
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	if (extension->open_library("libgodot://generic-holder-source", loader) != OK) {
		std::_Exit(91);
	}
	RecordRef record;
	Access::retain_record(*extension.ptr(), record);
	BindingLifetime holder;
	if (holder.acquire(record) != OK) {
		std::_Exit(92);
	}
	bool wrong_thread = false;
	bool same_frame = false;
	for (const String &arg : OS::get_singleton()->get_cmdline_args()) {
		wrong_thread |= arg == "--issuer-wrong-thread";
		same_frame |= arg == "--issuer-same-frame";
	}
	if (wrong_thread) {
		std::thread worker([h = std::move(holder)]() mutable { h = {}; });
		worker.join();
	} else if (same_frame) {
		CallbackScope frame(record);
		if (frame.get_error() != OK) {
			std::_Exit(92);
		}
		holder = {};
	} else {
		holder = {};
	}
	std::fputs("UNEXPECTED_GENERIC_RETIREMENT\n", stderr);
	std::_Exit(93);
}
REGISTER_TEST_COMMAND("--device-exit-issuer-generic-holder", issuer_generic_holder_child);
static void issuer_foreign_holder_child() {
	using namespace GDExtensionDeviceExit;
	Ref<GDExtension> first, second;
	first.instantiate();
	second.instantiate();
	Ref<GDExtensionFunctionLoader> a, b;
	a.instantiate();
	b.instantiate();
	a->set_initialization_function(entry);
	b->set_initialization_function(entry);
	if (first->open_library("libgodot://foreign-holder-a", a) != OK || second->open_library("libgodot://foreign-holder-b", b) != OK) {
		std::_Exit(91);
	}
	RecordRef first_record, second_record;
	Access::retain_record(*first.ptr(), first_record);
	Access::retain_record(*second.ptr(), second_record);
	BindingLifetime holder;
	if (holder.acquire(first_record) != OK) {
		std::_Exit(92);
	}
	{
		CallbackScope outer(first_record);
		CallbackScope foreign(second_record);
		holder = {};
	}
	std::fputs("UNEXPECTED_FOREIGN_RETIREMENT\n", stderr);
	std::_Exit(93);
}
REGISTER_TEST_COMMAND("--device-exit-issuer-foreign-holder", issuer_foreign_holder_child);
TEST_CASE("device_exit::issuer/ack-closes-initialization-publication") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	issuer_init_hook = [](InitializationLevelContextRef context) {
		auto &registry = Registry::get_singleton();
		const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
		GuardedLibraryRef library;
		RegisteredCleanupRef action;
		CHECK_EQ(registry.borrow_guarded_library(&abi, owner, &library), OK);
		CHECK_EQ(registry.borrow_level_cleanup(&abi, context, 0, &action), OK);
		CHECK_EQ(registry.after_binding_initialize(&abi, context), OK);
		CHECK_EQ(registry.after_binding_initialize(&abi, context), ERR_INVALID_DATA);
		LibraryCodeHoldRef hold;
		const Error result = registry.acquire_code_hold(&abi, library, action, &hold);
		CHECK_EQ(result, ERR_UNAUTHORIZED);
		if (result == OK) {
			registry.release_code_hold(&abi, hold);
		}
		RegisteredCleanupRef late;
		CHECK_EQ(registry.borrow_level_cleanup(&abi, context, 0, &late), ERR_UNAUTHORIZED);
	};
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	fixture.finish();
}
TEST_CASE("device_exit::issuer/cleanup-holder-serial-is-not-frame-identity") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	issuer_init_hook = [](InitializationLevelContextRef context) {
		RecordRef record;
		CHECK_EQ(Access::retain_record(*owner, record), OK);
		auto &registry = Registry::get_singleton();
		CleanupAction cleanup;
		CHECK_EQ(registry.register_cleanup_action(record, 17, Thread::get_caller_id(), 19, cleanup), OK);
		Access::collide_cleanup_holder_serial(*owner, cleanup);
		{
			CleanupScope nested(cleanup, record, 17, 19);
			CHECK_EQ(nested.get_error(), OK);
		}
		const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
		CHECK_EQ(registry.after_binding_initialize(&abi, context), OK);
	};
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	fixture.finish();
}
TEST_CASE("device_exit::issuer/null-user-and-descending-real-levels") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	issuer_before_register = [](GuardRegistrationV1 &registration) {
		registration.user_cleanup_bitmap = 0;
		registration.user_terminator = nullptr;
	};
	issuer_init_hook = [](InitializationLevelContextRef context) {
		const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
		RegisteredCleanupRef action;
		CHECK_EQ(Registry::get_singleton().borrow_level_cleanup(&abi, context, 0, &action), ERR_UNAVAILABLE);
		CHECK_EQ(action.handle_serial, 0);
	};
	REQUIRE_EQ(fixture.open(), OK);
	CHECK_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	CHECK_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_EDITOR), OK);
	CHECK_EQ(fixture.extension->preflight_exit(CloseRoute::EXPLICIT), OK);
	ERR_PRINT_OFF;
	CHECK_EQ(fixture.extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), ERR_INVALID_DATA);
	ERR_PRINT_ON;
	CHECK_EQ(Access::level(*owner), 3);
	CHECK_EQ(fixture.extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_EDITOR), OK);
	CHECK_FALSE(Access::guard_pointers_clear(*owner));
	fixture.finish();
}
TEST_CASE("device_exit::issuer/service-descriptors-rollback-on-tuple-mismatch") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	issuer_before_register = [](GuardRegistrationV1 &registration) { registration.needs[2].user_services = 1; };
	// Never initialize the service-dependent action: rollback through real entry
	// failure verifies reservation cleanup without inventing service completion.
	issuer_entry_hook = [](GuardRegistrationV1 &, GDExtensionInitialization *actual) { actual->userdata = owner; };
	ERR_PRINT_OFF;
	CHECK_EQ(fixture.open(), ERR_INVALID_DATA);
	ERR_PRINT_ON;
	CHECK_FALSE(fixture.extension->is_library_open());
}
TEST_CASE("device_exit::issuer/preseal-typed-pending-binds-one-attempt") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	issuer_init_hook = [](InitializationLevelContextRef context) {
		issuer_acquire_holds(context);
		const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
		auto &registry = Registry::get_singleton();
		CHECK_EQ(registry.release_code_hold(&abi, issuer_user_hold), OK);
		CHECK_EQ(registry.release_code_hold(&abi, issuer_binding_hold), OK);
		CHECK_EQ(registry.release_code_hold(&abi, issuer_clone_hold), OK);
		CHECK_EQ(Access::first_holder_retirement_attempt(*owner), 0);
		CHECK_EQ(owner->preflight_exit(CloseRoute::EXPLICIT), ERR_BUSY);
		CHECK_NE(Access::attempt(*owner), 0);
		CHECK_EQ(Access::first_holder_retirement_attempt(*owner), Access::attempt(*owner));
		LibraryCodeHoldRef late;
		CHECK_EQ(registry.clone_code_hold(&abi, issuer_user_hold, &late), ERR_INVALID_DATA);
	};
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	CHECK_EQ(Access::binding_holders(*owner), 0);
	fixture.finish();
}
static void issuer_services_child() {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	issuer_child_mode = true;
	issuer_before_register = [](GuardRegistrationV1 &registration) { registration.needs[2].user_services = 1; };
	if (fixture.open() != OK || fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) != OK) {
		std::_Exit(91);
	}
	const Error result = fixture.extension->preflight_exit(CloseRoute::EXPLICIT);
	const bool good = result == ERR_UNAVAILABLE && Access::attempt(*owner) == 0 && Access::level(*owner) == 2 && fixture.extension->is_library_open();
	std::fprintf(stderr, "ISSUER_SERVICE_UNAVAILABLE good=%d\n", int(good));
	std::_Exit(good ? 0 : 92); // Retained image; never fabricate service teardown.
}
REGISTER_TEST_COMMAND("--device-exit-issuer-services", issuer_services_child);
TEST_CASE("device_exit::issuer/clone-revalidates-live-action-and-ack") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	issuer_init_hook = [](InitializationLevelContextRef context) {
		issuer_acquire_holds(context);
		auto &registry = Registry::get_singleton();
		const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
		for (int field = 0; field < 2; field++) {
			Access::corrupt_typed_holder(*owner, field, issuer_clone_hold.handle_serial);
			LibraryCodeHoldRef copy;
			const Error result = registry.clone_code_hold(&abi, issuer_clone_hold, &copy);
			Access::corrupt_typed_holder(*owner, field, issuer_clone_hold.handle_serial);
			CHECK_EQ(result, ERR_INVALID_DATA);
			if (result == OK) { // Repair only the mutant test copy for safe cleanup.
				Access::corrupt_typed_holder(*owner, field, copy.handle_serial);
				registry.release_code_hold(&abi, copy);
			}
		}
		CHECK_EQ(registry.after_binding_initialize(&abi, context), OK);
		LibraryCodeHoldRef copy;
		const Error result = registry.clone_code_hold(&abi, issuer_clone_hold, &copy);
		CHECK_EQ(result, ERR_UNAUTHORIZED);
		if (result == OK) {
			registry.release_code_hold(&abi, copy);
		}
		registry.release_code_hold(&abi, issuer_user_hold);
		registry.release_code_hold(&abi, issuer_binding_hold);
		registry.release_code_hold(&abi, issuer_clone_hold);
	};
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	fixture.finish();
}
TEST_CASE("device_exit::issuer/failed-entry-retires-inactive-actions-without-terminator") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	issuer_entry_success = false;
	issuer_ack = FAILED;
	ERR_PRINT_OFF;
	const Error result = fixture.open();
	ERR_PRINT_ON;
	CHECK_NE(result, OK);
	CHECK_EQ(issuer_registration, OK);
	CHECK_EQ(issuer_ack, FAILED);
	CHECK_FALSE(fixture.extension->is_library_open());
	CHECK_EQ(Access::level(*fixture.extension.ptr()), -1);
}
TEST_CASE("device_exit::issuer/route-preflight-keeps-existing-quiesce-attempt") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	RecordRef record;
	REQUIRE_EQ(Access::retain_record(*owner, record), OK);
	CHECK_EQ(Registry::get_singleton().begin_quiesce(record), OK);
	const uint64_t attempt = Access::attempt(*owner);
	CHECK_NE(attempt, 0);
	CHECK_EQ(fixture.extension->preflight_exit(CloseRoute::EXPLICIT), OK);
	CHECK_EQ(Access::attempt(*owner), attempt);
	fixture.finish();
}
TEST_CASE("device_exit::issuer/first-control-capacity-reserved-at-enrollment") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	const uint32_t limit = Access::exhaust_callback_refs(*owner);
	Access::exhaust_issuer_sequences(true);
	const Error result = fixture.extension->preflight_exit(CloseRoute::EXPLICIT);
	Access::exhaust_issuer_sequences(false);
	Access::restore_callback_refs(*owner, limit);
	CHECK_EQ(result, OK);
	fixture.finish();
}
TEST_CASE("device_exit::issuer/core-binding-free-from-foreign-callback") {
	using namespace GDExtensionDeviceExit;
	Ref<GDExtensionFunctionLoader> a, b;
	a.instantiate();
	b.instantiate();
	a->set_initialization_function(entry);
	b->set_initialization_function(entry);
	Ref<GDExtension> first, second;
	first.instantiate();
	second.instantiate();
	REQUIRE_EQ(first->open_library("libgodot://disposal-first", a), OK);
	REQUIRE_EQ(second->open_library("libgodot://disposal-second", b), OK);
	Object *object = memnew(Object);
	const GDExtensionInstanceBindingCallbacks callbacks = {
		[](void *, void *instance) -> void * { return instance; },
		[](void *, void *, void *) {}, nullptr
	};
	CHECK_EQ(object->get_instance_binding(second.ptr(), &callbacks), object);
	RecordRef first_record;
	REQUIRE_EQ(Access::retain_record(*first.ptr(), first_record), OK);
	{
		CallbackScope outer(first_record);
		CHECK_EQ(outer.get_error(), OK);
		object->free_instance_binding(second.ptr());
		CHECK_EQ(Access::binding_holders(*second.ptr()), 0);
	}
	memdelete(object);
	CHECK_EQ(first->close_library(), OK);
	CHECK_EQ(second->close_library(), OK);
}
TEST_CASE("device_exit::issuer/control-busy-retry-retains-quota-and-expires-serial") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	issuer_preflight_result = ERR_BUSY;
	CHECK_EQ(fixture.extension->preflight_exit(CloseRoute::EXPLICIT), ERR_BUSY);
	const auto stale = issuer_stale_preflight;
	const uint64_t attempt = Access::attempt(*owner);
	const uint32_t old_refs = Access::exhaust_callback_refs(*owner);
	issuer_preflight_result = OK;
	CHECK_EQ(fixture.extension->preflight_exit(CloseRoute::EXPLICIT), OK);
	Access::restore_callback_refs(*owner, old_refs);
	CHECK_EQ(Access::attempt(*owner), attempt);
	CHECK_NE(issuer_stale_preflight.handle_serial, stale.handle_serial);
	const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
	ExitDiagnosticV1 diagnostic;
	diagnostic.abi = { 1, sizeof(ExitDiagnosticV1), 0 };
	CHECK_EQ(Registry::get_singleton().publish_exit_result(&abi, issuer_token, stale, &diagnostic), ERR_INVALID_DATA);
	fixture.finish();
}
TEST_CASE("device_exit::issuer/typed-clone-quota-preserves-output-and-holder") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture fixture;
	issuer_init_hook = [](InitializationLevelContextRef context) {
		issuer_acquire_holds(context);
		const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
		auto &registry = Registry::get_singleton();
		const uint32_t limit = Access::exhaust_callback_refs(*owner);
		LibraryCodeHoldRef copy;
		CHECK_EQ(registry.clone_code_hold(&abi, issuer_clone_hold, &copy), ERR_PARAMETER_RANGE_ERROR);
		CHECK_EQ(copy.handle_serial, 0);
		CHECK_EQ(Access::binding_holders(*owner), 3);
		Access::restore_callback_refs(*owner, limit);
		CHECK_EQ(registry.release_code_hold(&abi, issuer_user_hold), OK);
		CHECK_EQ(registry.release_code_hold(&abi, issuer_binding_hold), OK);
		CHECK_EQ(registry.release_code_hold(&abi, issuer_clone_hold), OK);
	};
	REQUIRE_EQ(fixture.open(), OK);
	REQUIRE_EQ(fixture.extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	fixture.finish();
}
TEST_CASE("device_exit::issuer/actual-entry-qualifies-and-frame-ack-expires") {
	using namespace GDExtensionDeviceExit;
	IssuerFixture reset_fixture;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(issuer_entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://issuer-bootstrap", loader), OK);
	CHECK_EQ(issuer_registration, OK);
	CHECK_NE(issuer_token.handle_serial, 0);
	REQUIRE_EQ(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	CHECK_EQ(issuer_ack, OK);
	const GDExtensionDeviceExitAbi abi{ 1, 16, 0 };
	CHECK_EQ(Registry::get_singleton().after_binding_initialize(&abi, issuer_stale_context), ERR_INVALID_DATA);
	REQUIRE_EQ(extension->preflight_exit(CloseRoute::EXPLICIT), OK);
	REQUIRE_EQ(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
	CHECK_EQ(issuer_ack, OK);
	CHECK_EQ(extension->close_library(), OK);
	owner = nullptr;
}

TEST_CASE("device_exit::holder-frame/preseal-pending-cell-binds-exact-attempt") {
	using namespace GDExtensionDeviceExit;
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://holder-preseal-attempt", loader), OK);
	RecordRef record;
	REQUIRE_EQ(Access::retain_record(*extension.ptr(), record), OK);
	Object *holder = create_real_binding_holder(extension.ptr());
	REQUIRE(holder != nullptr);
	{
		CallbackScope outer(record);
		REQUIRE_EQ(outer.get_error(), OK);
		{
			CallbackScope inner(record);
			REQUIRE_EQ(inner.get_error(), OK);
			holder->free_instance_binding(extension.ptr());
			memdelete(holder);
			CHECK_EQ(Access::first_holder_retirement_attempt(*extension.ptr()), 0);
			CHECK_EQ(extension->close_library(), ERR_BUSY);
			CHECK_NE(Access::attempt(*extension.ptr()), 0);
			CHECK_EQ(Access::first_holder_retirement_attempt(*extension.ptr()), Access::attempt(*extension.ptr()));
		}
		CHECK_EQ(Access::binding_holders(*extension.ptr()), 2);
	}
	CHECK_EQ(Access::binding_holders(*extension.ptr()), 0);
	CHECK_EQ(extension->close_library(), OK);
	owner = nullptr;
}

TEST_CASE("device_exit::successor/busy-attempt-seals-ordinary-admission") {
	using namespace GDExtensionDeviceExit;
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://successor-busy-seal", loader), OK);
	RecordRef record;
	REQUIRE_EQ(Access::retain_record(*extension.ptr(), record), OK);
	uint64_t attempt = 0;
	{
		CallbackScope outer(record);
		REQUIRE_EQ(outer.get_error(), OK);
		CHECK_EQ(extension->close_library(), ERR_BUSY);
		attempt = Access::attempt(*extension.ptr());
		CHECK_NE(attempt, 0);
		CallbackScope late(record);
		CHECK_EQ(late.get_error(), ERR_BUSY);
		CHECK_EQ(Access::evidence_serial(record), 0);
	}
	CHECK_EQ(extension->close_library(), OK);
	CHECK_EQ(decode_uint64(Access::evidence_bytes(record).ptr() + 20), attempt);
	owner = nullptr;
}

TEST_CASE("device_exit::successor/direct-close-cannot-bypass-unissued-teardown") {
	using namespace GDExtensionDeviceExit;
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://successor-direct-close", loader), OK);
	REQUIRE_EQ(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_CORE), OK);
	CHECK_EQ(extension->close_library(), ERR_UNAVAILABLE);
	const bool still_open = extension->is_library_open();
	CHECK(still_open);
	if (still_open) {
		CHECK_EQ(Access::attempt(*extension.ptr()), 0);
		CHECK_EQ(Access::level(*extension.ptr()), 0);
		REQUIRE_EQ(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_CORE), OK);
		CHECK_EQ(extension->close_library(), OK);
	}
	owner = nullptr;
}

TEST_CASE("device_exit::successor/preflight-capacity-refuses-without-publication") {
	using namespace GDExtensionDeviceExit;
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://successor-capacity", loader), OK);
	RecordRef record;
	REQUIRE_EQ(Access::retain_record(*extension.ptr(), record), OK);
	const uint32_t old_limit = Access::exhaust_callback_refs(*extension.ptr(), 1);
	CHECK_EQ(extension->preflight_exit(CloseRoute::EXPLICIT), ERR_PARAMETER_RANGE_ERROR);
	CHECK_EQ(Access::attempt(*extension.ptr()), 0);
	CHECK_EQ(Access::evidence_serial(record), 0);
	CHECK_FALSE(Access::evidence_committed(record));
	Access::restore_callback_refs(*extension.ptr(), old_limit);
	CHECK_EQ(extension->close_library(), OK);
	owner = nullptr;
}

// Isolated command: the real manager is already initialized by the engine.
// Exit with the retained image intact after proving a non-destructive refusal.
#ifdef TOOLS_ENABLED
static int successor_deinitializations = 0;
static void successor_deinitialize(void *, GDExtensionInitializationLevel) {
	successor_deinitializations++;
}
static GDExtensionBool successor_entry(GDExtensionInterfaceGetProcAddress get, GDExtensionClassLibraryPtr library, GDExtensionInitialization *result) {
	const GDExtensionBool accepted = entry(get, library, result);
	result->deinitialize = successor_deinitialize;
	return accepted;
}
#endif // TOOLS_ENABLED
static void successor_manager_preflight_child() {
#ifdef TOOLS_ENABLED
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	auto *manager = GDExtensionManager::get_singleton();
	Engine::get_singleton()->set_extension_reloading_enabled(true);
	bool good = true;
	for (bool reload : { false, true }) {
		Ref<GDExtensionFunctionLoader> loader;
		loader.instantiate();
		loader->set_initialization_function(successor_entry);
		const String path = reload ? "libgodot://successor-manager-reload" : "libgodot://successor-manager-unload";
		if (manager->load_extension_with_loader(path, loader) != GDExtensionManager::LOAD_STATUS_OK) {
			std::_Exit(91);
		}
		Ref<GDExtension> extension = manager->get_extension(path);
		extension->set_reloadable(true);
		const uint64_t generation = Access::generation(*extension.ptr());
		const int32_t initialized_level = Access::level(*extension.ptr());
		const auto result = reload ? manager->reload_extension(path) : manager->unload_extension(path);
		const bool intact = result == GDExtensionManager::LOAD_STATUS_FAILED && manager->is_extension_loaded(path) &&
				extension->is_library_open() && !Access::reloading(*extension.ptr()) && initialized_level >= 0 &&
				Access::level(*extension.ptr()) == initialized_level && Access::generation(*extension.ptr()) == generation &&
				Access::attempt(*extension.ptr()) == 0 && successor_deinitializations == 0;
		good = good && intact;
		std::fprintf(stderr, "SUCCESSOR_PREFLIGHT route=%s intact=%d deinit=%d level=%d\n", reload ? "reload" : "unload", int(intact), successor_deinitializations, initialized_level);
		std::fflush(stderr);
	}
	std::_Exit(good ? 0 : 95);
#else
	std::_Exit(96);
#endif
}
REGISTER_TEST_COMMAND("--device-exit-successor-preflight", successor_manager_preflight_child);

TEST_CASE("device_exit::host-lifetime/cleanup-phase-rejects-unissued-authority") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://cleanup-phase", loader), OK);
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	GDExtensionDeviceExit::RecordRef record;
	REQUIRE_EQ(Access::retain_record(*extension.ptr(), record), OK);
	{
		GDExtensionDeviceExit::CleanupAction unscheduled;
		REQUIRE_EQ(registry.register_cleanup_action(record, 501, Thread::get_caller_id(), 601, unscheduled), OK);
		CHECK(Access::cleanup_phase(unscheduled) == GDExtensionDeviceExit::CleanupPhase::UNSCHEDULED);
		for (auto phase : { GDExtensionDeviceExit::CleanupPhase::USER, GDExtensionDeviceExit::CleanupPhase::BINDING, GDExtensionDeviceExit::CleanupPhase::FUTURE_LEVEL }) {
			GDExtensionDeviceExit::CleanupAction unissued;
			CHECK_EQ(registry.register_cleanup_action(record, 502, Thread::get_caller_id(), 602, unissued, phase), ERR_UNAUTHORIZED);
		}
		GDExtensionDeviceExit::CleanupAction invalid;
		CHECK_EQ(registry.register_cleanup_action(record, 503, Thread::get_caller_id(), 603, invalid, static_cast<GDExtensionDeviceExit::CleanupPhase>(255)), ERR_INVALID_PARAMETER);
		CHECK_EQ(registry.begin_quiesce(record), ERR_BUSY);
	}
	CHECK_EQ(extension->close_library(), OK);
	owner = nullptr;
}

TEST_CASE("device_exit::entry/cleanup-capacity-and-detached-scope") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://cleanup-capacity", loader), OK);
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	GDExtensionDeviceExit::RecordRef record;
	REQUIRE_EQ(registry.retain_binding_owner(extension.ptr(), record), OK);
	const uint32_t baseline = Access::private_refs(*extension.ptr());
	GDExtensionDeviceExit::CleanupScope *detached = nullptr;
	{
		GDExtensionDeviceExit::CleanupAction action;
		const uint32_t limit = Access::exhaust_callback_refs(*extension.ptr());
		CHECK_EQ(registry.register_cleanup_action(record, 901, Thread::get_caller_id(), 31, action), ERR_PARAMETER_RANGE_ERROR);
		CHECK_EQ(Access::private_refs(*extension.ptr()), baseline);
		Access::restore_callback_refs(*extension.ptr(), limit);
		CHECK_EQ(registry.register_cleanup_action(record, 901, Thread::get_caller_id(), 31, action), OK);
		CHECK_EQ(registry.register_cleanup_action(record, 901, Thread::get_caller_id(), 31, action), ERR_INVALID_DATA);
		CHECK_EQ(registry.begin_quiesce(record), ERR_BUSY);
		const uint32_t full = Access::exhaust_callback_refs(*extension.ptr());
		{
			GDExtensionDeviceExit::CleanupScope reserved(action, record, 901, 31);
			CHECK_EQ(reserved.get_error(), OK);
			CHECK_EQ(Access::active_frames(*extension.ptr()), 1);
			GDExtensionDeviceExit::CleanupScope duplicate(action, record, 901, 31);
			CHECK_EQ(duplicate.get_error(), ERR_BUSY);
		}
		Access::restore_callback_refs(*extension.ptr(), full);
		detached = new GDExtensionDeviceExit::CleanupScope(action, record, 901, 31);
		CHECK_EQ(detached->get_error(), OK);
		CHECK_EQ(Access::active_frames(*extension.ptr()), 1);
	}
	// The callback acknowledgment owns its own lease even after registration
	// storage retires; final close must not race its actual return.
	CHECK_EQ(Access::private_refs(*extension.ptr()), baseline + 1);
	CHECK_EQ(registry.begin_quiesce(record), ERR_BUSY);
	CHECK_EQ(extension->close_library(), ERR_BUSY);
	delete detached;
	CHECK_EQ(Access::active_frames(*extension.ptr()), 0);
	CHECK_EQ(Access::private_refs(*extension.ptr()), baseline);
	CHECK_EQ(registry.begin_quiesce(record), OK);
	CHECK_EQ(extension->close_library(), OK);
	owner = nullptr;
}

TEST_CASE("device_exit::entry/cleanup-active-capacity-no-wrap") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://cleanup-active-capacity", loader), OK);
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	GDExtensionDeviceExit::RecordRef record;
	REQUIRE_EQ(registry.retain_binding_owner(extension.ptr(), record), OK);
	const uint32_t baseline = Access::private_refs(*extension.ptr());
	{
		GDExtensionDeviceExit::CleanupAction action;
		Access::set_active_frames(*extension.ptr(), UINT32_MAX);
		CHECK_EQ(registry.register_cleanup_action(record, 902, Thread::get_caller_id(), 32, action), ERR_PARAMETER_RANGE_ERROR);
		CHECK_EQ(Access::private_refs(*extension.ptr()), baseline);
		CHECK_EQ(Access::cleanup_reservations(*extension.ptr()), 0);
		Access::set_active_frames(*extension.ptr(), UINT32_MAX - 1);
		CHECK_EQ(registry.register_cleanup_action(record, 902, Thread::get_caller_id(), 32, action), OK);
		CHECK_EQ(Access::cleanup_reservations(*extension.ptr()), 1);
		{
			// Synthetic boundary state never enters DSO code. A refused normal
			// frame cannot steal the final slot from registered cleanup.
			GDExtensionDeviceExit::CallbackScope normal(record);
			CHECK_EQ(normal.get_error(), ERR_PARAMETER_RANGE_ERROR);
		}
		CHECK_EQ(Access::active_frames(*extension.ptr()), UINT32_MAX - 1);
		{
			GDExtensionDeviceExit::CleanupScope cleanup(action, record, 902, 32);
			CHECK_EQ(cleanup.get_error(), OK);
			CHECK_EQ(Access::active_frames(*extension.ptr()), UINT32_MAX);
			CHECK_EQ(Access::cleanup_reservations(*extension.ptr()), 0);
		}
		CHECK_EQ(Access::active_frames(*extension.ptr()), UINT32_MAX - 1);
		CHECK_EQ(Access::cleanup_reservations(*extension.ptr()), 1);
		Access::set_active_frames(*extension.ptr(), 0);
	}
	CHECK_EQ(Access::private_refs(*extension.ptr()), baseline);
	CHECK_EQ(Access::cleanup_reservations(*extension.ptr()), 0);
	CHECK_EQ(extension->close_library(), OK);
	owner = nullptr;
}

// Resident-only admission-state injection: no loader detach, no holder removal,
// and never used as evidence that a live direct instance permits OS close.
void set_resident_method_gate_closed(GDExtension &extension, bool closed) {
	Access::set_resident_gate_closed(extension, closed);
}
void set_resident_method_quiescing(GDExtension &extension, bool closed) {
	Access::set_quiescing(extension, closed);
}
uint32_t exhaust_method_refs(GDExtension &extension, uint32_t reserve) {
	return Access::exhaust_callback_refs(extension, reserve);
}
void restore_method_refs(GDExtension &extension, uint32_t limit) {
	Access::restore_callback_refs(extension, limit);
}
static bool binding_null_create = false;
static int binding_reserved_creates = 0;
static int binding_reserved_frees = 0;
static void *binding_reserved_create(void *token, void *instance) {
	binding_reserved_creates++;
	Object *object = static_cast<Object *>(instance);
	const GDExtensionInstanceBindingCallbacks recursive = { binding_reserved_create, nullptr, nullptr };
	CHECK_EQ(object->get_instance_binding(token, &recursive), nullptr);
	CHECK_EQ(String(GDExtensionDeviceExit::get_binding_entry_diagnostic()), "BINDING_ASSOCIATION_BUSY");
	return binding_null_create ? nullptr : instance;
}
static void binding_reserved_free(void *, void *, void *) {
	binding_reserved_frees++;
}
TEST_CASE("device_exit::object/binding-null-reentrant-quota-and-duplicate") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://binding-reservation", loader), OK);
	Object *object = memnew(Object);
	const GDExtensionInstanceBindingCallbacks callbacks = { binding_reserved_create, binding_reserved_free, nullptr };
	binding_reserved_creates = binding_reserved_frees = 0;
	for (uint32_t spare : { 0, 1, 2 }) {
		const uint32_t old_limit = Access::exhaust_callback_refs(*extension.ptr(), spare);
		CHECK_EQ(object->get_instance_binding(extension.ptr(), &callbacks), nullptr);
		CHECK_EQ(String(GDExtensionDeviceExit::get_binding_entry_diagnostic()), "BINDING_ASSOCIATION_LIMIT");
		Access::restore_callback_refs(*extension.ptr(), old_limit);
		CHECK_EQ(binding_reserved_creates, 0);
		CHECK_FALSE(object->has_instance_binding(extension.ptr()));
	}
	binding_null_create = true;
	CHECK_EQ(object->get_instance_binding(extension.ptr(), &callbacks), nullptr);
	CHECK_EQ(GDExtensionDeviceExit::get_binding_entry_diagnostic(), nullptr);
	CHECK_EQ(binding_reserved_creates, 1);
	CHECK_EQ(binding_reserved_frees, 0);
	CHECK_FALSE(object->has_instance_binding(extension.ptr()));
	binding_null_create = false;
	CHECK_EQ(object->get_instance_binding(extension.ptr(), &callbacks), object);
	CHECK_EQ(object->get_instance_binding(extension.ptr(), &callbacks), object);
	CHECK_EQ(GDExtensionDeviceExit::get_binding_entry_diagnostic(), nullptr);
	CHECK_EQ(binding_reserved_creates, 2);
	object->free_instance_binding(extension.ptr());
	CHECK_EQ(binding_reserved_frees, 1);
	CHECK_EQ(extension->close_library(), OK);
	CHECK_EQ(object->get_instance_binding(extension.ptr(), &callbacks), nullptr);
	CHECK_EQ(String(GDExtensionDeviceExit::get_binding_entry_diagnostic()), "ENTRY_CLOSED");
	CHECK_EQ(binding_reserved_creates, 2);
	CHECK_EQ(object->get_instance_binding(extension.ptr(), nullptr), nullptr);
	CHECK_EQ(GDExtensionDeviceExit::get_binding_entry_diagnostic(), nullptr);
	memdelete(object);
	owner = nullptr;
}

TEST_CASE("device_exit::object/binding-create-failure-rolls-back-cleanup-ticket") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://binding-create-rollback", loader), OK);
	Object *object = memnew(Object);
	const GDExtensionInstanceBindingCallbacks callbacks = { binding_reserved_create, binding_reserved_free, nullptr };
	binding_null_create = true;
	binding_reserved_creates = binding_reserved_frees = 0;
	CHECK_EQ(object->get_instance_binding(extension.ptr(), &callbacks), nullptr);
	CHECK_EQ(binding_reserved_creates, 1);
	CHECK_EQ(binding_reserved_frees, 0);
	CHECK_EQ(extension->close_library(), OK);
	Ref<GDExtension> reopened;
	reopened.instantiate();
	Ref<GDExtensionFunctionLoader> reopened_loader;
	reopened_loader.instantiate();
	reopened_loader->set_initialization_function(entry);
	REQUIRE_EQ(reopened->open_library("libgodot://binding-create-rollback-reuse", reopened_loader), OK);
	binding_null_create = false;
	CHECK_EQ(object->get_instance_binding(reopened.ptr(), &callbacks), object);
	object->free_instance_binding(reopened.ptr());
	CHECK_EQ(reopened->close_library(), OK);
	memdelete(object);
	owner = nullptr;
	binding_null_create = false;
}

// A failed binding transaction must not consume an earlier owner's obligation.
TEST_CASE("device_exit::object/binding-rollback-preserves-authenticated-registration") {
	for (bool with_free : { false, true }) {
		close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
		Ref<GDExtensionFunctionLoader> loader;
		loader.instantiate();
		loader->set_initialization_function(entry);
		Ref<GDExtension> extension;
		extension.instantiate();
		REQUIRE_EQ(extension->open_library("libgodot://binding-preserved-registration", loader), OK);
		GDExtensionDeviceExit::RecordRef record;
		REQUIRE_EQ(Access::retain_record(*extension.ptr(), record), OK);
		Object *object = memnew(Object);
		REQUIRE_EQ(register_authenticated_lifetime(object, record, Thread::get_caller_id(), 991), OK);
		const uint32_t reservations = Access::cleanup_reservations(*extension.ptr());
		const uint32_t refs = Access::private_refs(*extension.ptr());
		const GDExtensionInstanceBindingCallbacks callbacks = { binding_reserved_create, with_free ? binding_reserved_free : nullptr, nullptr };
		binding_null_create = true;
		binding_reserved_creates = binding_reserved_frees = 0;
		CHECK_EQ(object->get_instance_binding(extension.ptr(), &callbacks), nullptr);
		CHECK_FALSE(object->has_instance_binding(extension.ptr()));
		CHECK_EQ(Access::cleanup_reservations(*extension.ptr()), reservations);
		CHECK_EQ(Access::private_refs(*extension.ptr()), refs);
		CHECK_EQ(register_authenticated_lifetime(object, record, Thread::get_caller_id(), 992), ERR_ALREADY_EXISTS);
		CHECK_EQ(binding_reserved_frees, 0);
		CHECK_EQ(extension->close_library(), ERR_BUSY);
		memdelete(object);
		CHECK_EQ(extension->close_library(), OK);
	}
	binding_null_create = false;
	owner = nullptr;
}

static void *binding_seal_create(void *, void *instance) {
	binding_reserved_creates++;
	CHECK_EQ(owner->close_library(), ERR_BUSY);
	return instance;
}
TEST_CASE("device_exit::object/binding-publication-rollback-owns-only-new-ticket") {
	for (bool registered : { false, true }) {
		close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
		Ref<GDExtensionFunctionLoader> loader;
		loader.instantiate();
		loader->set_initialization_function(entry);
		Ref<GDExtension> extension;
		extension.instantiate();
		REQUIRE_EQ(extension->open_library("libgodot://binding-sealed-publication", loader), OK);
		GDExtensionDeviceExit::RecordRef record;
		REQUIRE_EQ(Access::retain_record(*extension.ptr(), record), OK);
		Object *object = memnew(Object);
		if (registered) {
			REQUIRE_EQ(register_authenticated_lifetime(object, record, Thread::get_caller_id(), 993), OK);
		}
		const uint32_t reservations = Access::cleanup_reservations(*extension.ptr());
		const uint32_t refs = Access::private_refs(*extension.ptr());
		const GDExtensionInstanceBindingCallbacks callbacks = { binding_seal_create, binding_reserved_free, nullptr };
		binding_reserved_creates = binding_reserved_frees = 0;
		CHECK_EQ(object->get_instance_binding(extension.ptr(), &callbacks), nullptr);
		CHECK_EQ(String(GDExtensionDeviceExit::get_binding_entry_diagnostic()), "ENTRY_CLOSED");
		CHECK_FALSE(object->has_instance_binding(extension.ptr()));
		CHECK_EQ(binding_reserved_creates, 1);
		CHECK_EQ(binding_reserved_frees, 1);
		CHECK_EQ(Access::cleanup_reservations(*extension.ptr()), reservations);
		CHECK_EQ(Access::private_refs(*extension.ptr()), refs);
		CHECK_EQ(extension->close_library(), registered ? ERR_BUSY : OK);
		memdelete(object);
		if (registered) {
			CHECK_EQ(extension->close_library(), OK);
		}
	}
	owner = nullptr;
}

TEST_CASE("device_exit::object/binding-primary-and-reference-only-auto-cleanup") {
	for (bool primary : { false, true }) {
		close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
		Ref<GDExtensionFunctionLoader> loader;
		loader.instantiate();
		loader->set_initialization_function(entry);
		Ref<GDExtension> extension;
		extension.instantiate();
		REQUIRE_EQ(extension->open_library("libgodot://binding-auto-cleanup", loader), OK);
		Object *object = memnew(Object);
		binding_null_create = false;
		binding_reserved_creates = binding_reserved_frees = 0;
		const GDExtensionInstanceBindingCallbacks callbacks = { binding_reserved_create, primary ? binding_reserved_free : nullptr, nullptr };
		if (primary) {
			object->set_instance_binding(extension.ptr(), object, &callbacks);
		} else {
			CHECK_EQ(object->get_instance_binding(extension.ptr(), &callbacks), object);
		}
		CHECK_EQ(Access::cleanup_reservations(*extension.ptr()), 1);
		CHECK_EQ(object->get_instance_binding(extension.ptr(), &callbacks), object);
		CHECK_EQ(Access::cleanup_reservations(*extension.ptr()), 1);
		CHECK_EQ(extension->close_library(), ERR_BUSY);
		object->free_instance_binding(extension.ptr());
		CHECK_EQ(binding_reserved_frees, primary ? 1 : 0);
		CHECK_EQ(Access::cleanup_reservations(*extension.ptr()), 0);
		CHECK_EQ(extension->close_library(), OK);
		memdelete(object);
	}
	owner = nullptr;
}

TEST_CASE("device_exit::object/enrolled-secondary-holder-survives-primary-insertion") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://enrolled-secondary", loader), OK);
	Object *object = memnew(Object);
	GDExtensionDeviceExit::RecordRef record;
	REQUIRE_EQ(Access::retain_record(*extension.ptr(), record), OK);
	REQUIRE_EQ(TestDeviceExitEntry::register_authenticated_lifetime(object, record, Thread::get_caller_id(), 704), OK);
	const GDExtensionInstanceBindingCallbacks callbacks = { binding_reserved_create, binding_reserved_free, nullptr };
	binding_null_create = false;
	binding_reserved_creates = binding_reserved_frees = 0;
	REQUIRE_EQ(object->get_instance_binding(extension.ptr(), &callbacks), object);
	CHECK_EQ(extension->close_library(), ERR_BUSY);
	int primary = 1;
	const GDExtensionInstanceBindingCallbacks primary_callbacks = {};
	object->set_instance_binding(&primary, &primary, &primary_callbacks);
	CHECK_EQ(object->get_instance_binding(extension.ptr(), nullptr), nullptr);
	CHECK(object->has_instance_binding(extension.ptr()));
	CHECK_EQ(String(GDExtensionDeviceExit::get_binding_entry_diagnostic()), "ENTRY_CLOSED");
	CHECK_EQ(extension->close_library(), ERR_BUSY);
	object->free_instance_binding(&primary);
	CHECK_EQ(object->get_instance_binding(extension.ptr(), nullptr), nullptr);
	CHECK(object->has_instance_binding(extension.ptr()));
	CHECK_EQ(String(GDExtensionDeviceExit::get_binding_entry_diagnostic()), "ENTRY_CLOSED");
	CHECK_EQ(extension->close_library(), ERR_BUSY);
	CHECK_EQ(binding_reserved_frees, 0);
	object->free_instance_binding(extension.ptr());
	CHECK_EQ(binding_reserved_frees, 1);
	CHECK_EQ(extension->close_library(), OK);
	memdelete(object);
	owner = nullptr;
}
static int reference_cohort_calls = 0;
static GDExtensionBool reference_cohort_callback(void *, void *, GDExtensionBool) {
	reference_cohort_calls++;
	return true;
}
TEST_CASE("device_exit::object/release-registration-requires-current-owner-entry") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtension> extensions[2];
	GDExtensionDeviceExit::RecordRef records[2];
	for (int i = 0; i < 2; i++) {
		Ref<GDExtensionFunctionLoader> loader;
		loader.instantiate();
		loader->set_initialization_function(entry);
		extensions[i].instantiate();
		REQUIRE_EQ(extensions[i]->open_library("libgodot://registration-auth", loader), OK);
		REQUIRE_EQ(Access::retain_record(*extensions[i].ptr(), records[i]), OK);
	}
	auto attempt = [&](int index) {
		Object *object = memnew(Object);
		const Error result = object->register_release_lifetime(records[index], Thread::get_caller_id(), 71);
		memdelete(object);
		return result;
	};
	CHECK_EQ(attempt(0), ERR_UNAUTHORIZED);
	{
		GDExtensionDeviceExit::CallbackScope outer(records[0]);
		REQUIRE_EQ(outer.get_error(), OK);
		CHECK_EQ(attempt(0), OK);
		Error foreign = OK;
		std::thread worker([&]() { foreign = attempt(0); });
		worker.join();
		CHECK_EQ(foreign, ERR_UNAUTHORIZED);
		{
			GDExtensionDeviceExit::CallbackScope inner(records[1]);
			REQUIRE_EQ(inner.get_error(), OK);
			CHECK_EQ(attempt(0), ERR_UNAUTHORIZED);
			CHECK_EQ(attempt(1), OK);
		}
		CHECK_EQ(attempt(0), OK);
	}
	CHECK_EQ(attempt(0), ERR_UNAUTHORIZED);
	for (auto &extension : extensions) {
		CHECK_EQ(extension->close_library(), OK);
	}
	owner = nullptr;
}

class CohortScriptInstance : public PlaceHolderScriptInstance {
	bool *destroyed;

public:
	CohortScriptInstance(Object *p_owner, bool *p_destroyed) : PlaceHolderScriptInstance(nullptr, Ref<Script>(), p_owner), destroyed(p_destroyed) {}
	~CohortScriptInstance() { *destroyed = true; }
};
static bool cohort_script_destroyed = false;
static bool cohort_script_retained = false;
static GDExtensionBool replace_cohort_script(void *, void *p_instance, GDExtensionBool p_reference) {
	if (p_reference) {
		Object *object = static_cast<Object *>(p_instance);
		object->set_script_instance(nullptr);
		cohort_script_retained = !cohort_script_destroyed && object->get_script_instance() != nullptr;
	}
	return true;
}
TEST_CASE("device_exit::object/cohort-script-target-survives-replacement-attempt") {
	cohort_script_destroyed = cohort_script_retained = false;
	RefCounted *object = ::new (DefaultAllocator{}) RefCounted;
	postinitialize_handler(static_cast<Object *>(object));
	object->set_script_instance(memnew(CohortScriptInstance(object, &cohort_script_destroyed)));
	int token;
	GDExtensionInstanceBindingCallbacks callbacks = {
		[](void *, void *instance) -> void * { return instance; }, nullptr, replace_cohort_script
	};
	CHECK_EQ(object->get_instance_binding(&token, &callbacks), object);
	ERR_PRINT_OFF;
	CHECK(object->reference());
	ERR_PRINT_ON;
	CHECK(cohort_script_retained);
	CHECK_FALSE(cohort_script_destroyed);
	object->set_script_instance(nullptr);
	CHECK(cohort_script_destroyed);
	CHECK_FALSE(object->unreference());
	CHECK(object->unreference());
	memdelete(object);
}

#ifdef MODULE_GDSCRIPT_ENABLED
TEST_CASE("device_exit::object/script-constructor-reference-remains-admissible") {
	GDScriptLanguage::get_singleton()->init();
	Ref<GDScript> script = memnew(GDScript);
	script->set_source_code("extends RefCounted\nvar construction_ok = false\nfunc _init():\n\tvar retained = self\n\tconstruction_ok = retained == self\n");
	REQUIRE_EQ(script->reload(), OK);
	Ref<RefCounted> object = memnew(RefCounted);
	object->set_script(script);
	REQUIRE(object->get_script_instance() != nullptr);
	CHECK_EQ(object->get("construction_ok"), Variant(true));
	object->set_script(Variant());
	CHECK(object->get_script_instance() == nullptr);
}

static GDExtensionBool clear_cohort_script(void *, void *p_instance, GDExtensionBool p_reference) {
	if (p_reference) {
		Object *object = static_cast<Object *>(p_instance);
		object->set_script(Variant());
		cohort_script_retained = object->get_script_instance() != nullptr;
	}
	return true;
}
TEST_CASE("device_exit::object/cohort-script-target-survives-set-script-attempt") {
	GDScriptLanguage::get_singleton()->init();
	Ref<GDScript> script = memnew(GDScript);
	script->set_source_code("extends RefCounted\n");
	REQUIRE_EQ(script->reload(), OK);
	Ref<RefCounted> object = memnew(RefCounted);
	object->set_script(script);
	REQUIRE(object->get_script_instance() != nullptr);
	int token;
	GDExtensionInstanceBindingCallbacks callbacks = {
		[](void *, void *instance) -> void * { return instance; }, nullptr, clear_cohort_script
	};
	CHECK_EQ(object->get_instance_binding(&token, &callbacks), object.ptr());
	cohort_script_retained = false;
	ERR_PRINT_OFF;
	CHECK(object->reference());
	ERR_PRINT_ON;
	CHECK(cohort_script_retained);
	object->set_script(Variant());
	CHECK(object->get_script_instance() == nullptr);
	CHECK_FALSE(object->unreference());
}
#endif

static const GDExtensionDeviceExit::RecordRef *body_auth_records[2] = {};
static Error body_auth_own[2] = {};
static Error body_auth_foreign[2] = {};
static GDExtensionBool body_auth_callback(void *token, void *, GDExtensionBool) {
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	const int index = token == owner ? 1 : 0;
	body_auth_own[index] = registry.authenticate_registration(*body_auth_records[index]);
	body_auth_foreign[index] = registry.authenticate_registration(*body_auth_records[1 - index]);
	return true;
}
TEST_CASE("device_exit::object/cohort-registration-identity-follows-actual-callback") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtension> extensions[2];
	GDExtensionDeviceExit::RecordRef records[2];
	for (int i = 0; i < 2; i++) {
		Ref<GDExtensionFunctionLoader> loader;
		loader.instantiate();
		loader->set_initialization_function(entry);
		extensions[i].instantiate();
		REQUIRE_EQ(extensions[i]->open_library("libgodot://cohort-registration-identity", loader), OK);
		REQUIRE_EQ(Access::retain_record(*extensions[i].ptr(), records[i]), OK);
		body_auth_records[i] = &records[i];
	}
	RefCounted *object = ::new (DefaultAllocator{}) RefCounted;
	postinitialize_handler(static_cast<Object *>(object));
	GDExtensionInstanceBindingCallbacks callbacks = {
		[](void *, void *instance) -> void * { return instance; }, nullptr, body_auth_callback
	};
	for (auto &extension : extensions) {
		CHECK_EQ(object->get_instance_binding(extension.ptr(), &callbacks), object);
	}
	CHECK(object->reference());
	for (int i = 0; i < 2; i++) {
		CHECK_EQ(body_auth_own[i], OK);
		CHECK_EQ(body_auth_foreign[i], ERR_UNAUTHORIZED);
	}
	CHECK_FALSE(object->unreference());
	CHECK(object->unreference());
	memdelete(object);
	for (auto &extension : extensions) {
		CHECK_EQ(extension->close_library(), OK);
	}
	body_auth_records[0] = body_auth_records[1] = nullptr;
	owner = nullptr;
}

TEST_CASE("device_exit::object/release-lifetime-registration-before-association") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://object-release-registration", loader), OK);
	GDExtensionDeviceExit::RecordRef record;
	REQUIRE_EQ(Access::retain_record(*extension.ptr(), record), OK);
	Object *object = memnew(Object);
	CHECK_EQ(TestDeviceExitEntry::register_authenticated_lifetime(object, record, Thread::get_caller_id(), 41), OK);
	CHECK_EQ(TestDeviceExitEntry::register_authenticated_lifetime(object, record, Thread::get_caller_id(), 42), ERR_ALREADY_EXISTS);
	CHECK_EQ(extension->close_library(), ERR_BUSY);
	memdelete(object);
	CHECK_EQ(extension->close_library(), OK);
	owner = nullptr;
}

TEST_CASE("device_exit::object/registered-closed-unreference-keeps-normal-reference-sealed") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://registered-closed-release", loader), OK);
	GDExtensionDeviceExit::RecordRef record;
	REQUIRE_EQ(Access::retain_record(*extension.ptr(), record), OK);
	RefCounted *object = ::new (DefaultAllocator{}) RefCounted;
	postinitialize_handler(static_cast<Object *>(object));
	REQUIRE_EQ(TestDeviceExitEntry::register_authenticated_lifetime(object, record, Thread::get_caller_id(), 43), OK);
	const GDExtensionInstanceBindingCallbacks callbacks = {
		[](void *, void *instance) -> void * { return instance; }, nullptr, reference_cohort_callback
	};
	REQUIRE_EQ(object->get_instance_binding(extension.ptr(), &callbacks), object);
	REQUIRE(object->reference());
	reference_cohort_calls = 0;
	CHECK_EQ(GDExtensionDeviceExit::Registry::get_singleton().begin_quiesce(record), ERR_BUSY);
	CHECK_FALSE(object->reference());
	CHECK_EQ(object->get_reference_count(), 2);
	CHECK_EQ(reference_cohort_calls, 0);
	CHECK_FALSE(object->unreference());
	CHECK_EQ(object->get_reference_count(), 1);
	CHECK_EQ(reference_cohort_calls, 1);
	CHECK_EQ(Access::active_frames(*extension.ptr()), 0);
	// This case qualifies only decrement admission; full closed destruction is
	// covered separately once PREDELETE and binding retirement are integrated.
	Access::set_quiescing(*extension.ptr(), false);
	memdelete(object);
	CHECK_EQ(extension->close_library(), OK);
	owner = nullptr;
}

TEST_CASE("device_exit::object/registered-closed-release-does-not-allocate") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://allocation-free-release", loader), OK);
	GDExtensionDeviceExit::RecordRef record;
	REQUIRE_EQ(Access::retain_record(*extension.ptr(), record), OK);
	RefCounted *object = ::new (DefaultAllocator{}) RefCounted;
	postinitialize_handler(static_cast<Object *>(object));
	REQUIRE_EQ(TestDeviceExitEntry::register_authenticated_lifetime(object, record, Thread::get_caller_id(), 48), OK);
	const GDExtensionInstanceBindingCallbacks callbacks = {
		[](void *, void *instance) -> void * { return instance; }, nullptr, reference_cohort_callback
	};
	REQUIRE_EQ(object->get_instance_binding(extension.ptr(), &callbacks), object);
	REQUIRE(object->reference());
	CHECK_EQ(GDExtensionDeviceExit::Registry::get_singleton().begin_quiesce(record), ERR_BUSY);
	reference_cohort_calls = 0;
	MemoryAllocationTest::attempts = 0;
	MemoryAllocationTest::enabled = true;
	const bool terminal = object->unreference();
	MemoryAllocationTest::enabled = false;
	CHECK_FALSE(terminal);
	CHECK_EQ(MemoryAllocationTest::attempts, 0);
	CHECK_EQ(object->get_reference_count(), 1);
	CHECK_EQ(reference_cohort_calls, 1);
	memdelete(object);
	CHECK_EQ(extension->close_library(), OK);
	owner = nullptr;
}

// Stop before exhausting the native stack, so excessive per-entry reservation
// produces an ordinary assertion failure rather than a guard-page crash.
static uintptr_t nested_reference_stack_start = 0;
static uint32_t nested_reference_depth = 0;
static uint32_t nested_reference_reached = 0;
static bool nested_reference_good = true;
static GDExtensionBool nested_reference_callback(void *, void *instance, GDExtensionBool reference) {
	if (!reference) {
		return true;
	}
	volatile char stack_position = 0;
	const uintptr_t position = reinterpret_cast<uintptr_t>(&stack_position);
	if (!nested_reference_depth) {
		nested_reference_stack_start = position;
	}
	const uintptr_t used = position > nested_reference_stack_start ? position - nested_reference_stack_start : nested_reference_stack_start - position;
	if (used > 128 * 1024) {
		return true;
	}
	nested_reference_depth++;
	nested_reference_reached = MAX(nested_reference_reached, nested_reference_depth);
	if (nested_reference_depth < 16) {
		auto *object = static_cast<RefCounted *>(instance);
		nested_reference_good = object->reference() && nested_reference_good;
		nested_reference_good = !object->unreference() && nested_reference_good;
	}
	nested_reference_depth--;
	return true;
}
TEST_CASE("device_exit::object/nested-single-owner-reference-on-native-thread") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	nested_reference_depth = nested_reference_reached = 0;
	nested_reference_good = true;
	std::thread worker([]() {
		Ref<GDExtensionFunctionLoader> loader;
		loader.instantiate();
		loader->set_initialization_function(entry);
		Ref<GDExtension> extension;
		extension.instantiate();
		if (extension->open_library("libgodot://nested-owner-cohort", loader) != OK) {
			nested_reference_good = false;
			return;
		}
		RefCounted *object = ::new (DefaultAllocator{}) RefCounted;
		postinitialize_handler(static_cast<Object *>(object));
		const GDExtensionInstanceBindingCallbacks callbacks = {
			[](void *, void *instance) -> void * { return instance; }, nullptr, nested_reference_callback
		};
		nested_reference_good = object->get_instance_binding(extension.ptr(), &callbacks) == object;
		nested_reference_good = object->reference() && nested_reference_good;
		nested_reference_good = object->get_reference_count() == 2 && nested_reference_good;
		memdelete(object);
		nested_reference_good = extension->close_library() == OK && nested_reference_good;
		owner = nullptr;
	});
	worker.join();
	CHECK(nested_reference_good);
	// The outer reference raises the count to 2 and enters the callback.
	// Its nested 2 -> 3 -> 2 transitions are outside the callback thresholds.
	CHECK_EQ(nested_reference_reached, 1);
}

// Exercise the Registry's complete owner bound on a real default native-thread
// stack. Registration/setup may allocate; the closed release itself may not.
TEST_CASE("device_exit::object/maximum-owner-cohort-on-native-thread") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	bool good = true;
	uint32_t enrolled = 0;
	uint64_t allocations = UINT64_MAX;
	int callbacks = 0;
	std::thread worker([&]() {
		LocalVector<Ref<GDExtension>> extensions;
		RefCounted *object = ::new (DefaultAllocator{}) RefCounted;
		postinitialize_handler(static_cast<Object *>(object));
		const GDExtensionInstanceBindingCallbacks binding_callbacks = {
			[](void *, void *instance) -> void * { return instance; }, nullptr, reference_cohort_callback
		};
		for (uint32_t i = 0; i < Access::record_capacity(); i++) {
			Ref<GDExtensionFunctionLoader> loader;
			loader.instantiate();
			loader->set_initialization_function(entry);
			Ref<GDExtension> extension;
			extension.instantiate();
			if (extension->open_library("libgodot://maximum-owner-cohort", loader) != OK) {
				good = false;
				break;
			}
			extensions.push_back(extension);
			GDExtensionDeviceExit::RecordRef record;
			if (Access::retain_record(*extension.ptr(), record) != OK ||
					TestDeviceExitEntry::register_authenticated_lifetime(object, record, Thread::get_caller_id(), 1000 + i) != OK ||
					object->get_instance_binding(extension.ptr(), &binding_callbacks) != object) {
				good = false;
				break;
			}
			enrolled++;
		}
		if (good) {
			good = object->reference();
			for (const Ref<GDExtension> &extension : extensions) {
				GDExtensionDeviceExit::RecordRef record;
				good = (Access::retain_record(*extension.ptr(), record) == OK) && good;
				good = (GDExtensionDeviceExit::Registry::get_singleton().begin_quiesce(record) == ERR_BUSY) && good;
			}
			reference_cohort_calls = 0;
			MemoryAllocationTest::attempts = 0;
			MemoryAllocationTest::enabled = true;
			const bool terminal = object->unreference();
			MemoryAllocationTest::enabled = false;
			allocations = MemoryAllocationTest::attempts;
			callbacks = reference_cohort_calls;
			good = !terminal && object->get_reference_count() == 1 && good;
			for (const Ref<GDExtension> &extension : extensions) {
				good = Access::active_frames(*extension.ptr()) == 0 && good;
			}
		}
		memdelete(object);
		for (const Ref<GDExtension> &extension : extensions) {
			good = extension->close_library() == OK && good;
		}
		owner = nullptr;
	});
	worker.join();
	CHECK(good);
	CHECK_EQ(enrolled, Access::record_capacity());
	CHECK_EQ(callbacks, int(Access::record_capacity()));
	CHECK_EQ(allocations, 0);
}

static int cleanup_veto_calls = 0;
static GDExtensionBool cleanup_veto_reference(void *, void *instance, GDExtensionBool reference) {
	CHECK_FALSE(reference);
	auto *object = static_cast<RefCounted *>(instance);
	const int before = object->get_reference_count();
	CHECK_FALSE(object->reference());
	CHECK_EQ(object->get_reference_count(), before);
	CHECK_EQ(owner->close_library(), ERR_PARAMETER_RANGE_ERROR);
	cleanup_veto_calls++;
	return false;
}
TEST_CASE("device_exit::object/registered-veto-under-quota-keeps-unresolved-holder") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://registered-veto", loader), OK);
	GDExtensionDeviceExit::RecordRef record;
	REQUIRE_EQ(Access::retain_record(*extension.ptr(), record), OK);
	RefCounted *object = ::new (DefaultAllocator{}) RefCounted;
	postinitialize_handler(static_cast<Object *>(object));
	REQUIRE_EQ(TestDeviceExitEntry::register_authenticated_lifetime(object, record, Thread::get_caller_id(), 47), OK);
	const GDExtensionInstanceBindingCallbacks callbacks = {
		[](void *, void *instance) -> void * { return instance; }, nullptr, cleanup_veto_reference
	};
	REQUIRE_EQ(object->get_instance_binding(extension.ptr(), &callbacks), object);
	CHECK_EQ(GDExtensionDeviceExit::Registry::get_singleton().begin_quiesce(record), ERR_BUSY);
	const uint32_t original_limit = Access::exhaust_callback_refs(*extension.ptr());
	cleanup_veto_calls = 0;
	CHECK_FALSE(object->unreference());
	CHECK_EQ(object->get_reference_count(), 0);
	CHECK_EQ(cleanup_veto_calls, 1);
	CHECK_EQ(Access::active_frames(*extension.ptr()), 0);
	CHECK_EQ(extension->close_library(), ERR_PARAMETER_RANGE_ERROR);
	CHECK(extension->is_library_open());
	Access::restore_callback_refs(*extension.ptr(), original_limit);
	// Existing owner explicitly retires a vetoed zero-count object; no second
	// decrement, resurrection or fabricated terminal-ready handoff occurs.
	memdelete(object);
	CHECK_EQ(extension->close_library(), OK);
	owner = nullptr;
}

static int registered_terminal_frees = 0;
static void registered_terminal_free(void *, void *, void *) {
	registered_terminal_frees++;
	CHECK_EQ(owner->close_library(), ERR_BUSY);
}
TEST_CASE("device_exit::object/registered-explicit-binding-free-after-quiesce") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://registered-explicit-release", loader), OK);
	GDExtensionDeviceExit::RecordRef record;
	REQUIRE_EQ(Access::retain_record(*extension.ptr(), record), OK);
	Object *object = memnew(Object);
	REQUIRE_EQ(TestDeviceExitEntry::register_authenticated_lifetime(object, record, Thread::get_caller_id(), 45), OK);
	const GDExtensionInstanceBindingCallbacks callbacks = {
		[](void *, void *instance) -> void * { return instance; }, registered_terminal_free, nullptr
	};
	REQUIRE_EQ(object->get_instance_binding(extension.ptr(), &callbacks), object);
	registered_terminal_frees = 0;
	CHECK_EQ(GDExtensionDeviceExit::Registry::get_singleton().begin_quiesce(record), ERR_BUSY);
	object->free_instance_binding(extension.ptr());
	CHECK_EQ(registered_terminal_frees, 1);
	CHECK_EQ(Access::active_frames(*extension.ptr()), 0);
	CHECK_EQ(extension->close_library(), OK);
	memdelete(object);
	owner = nullptr;
}

TEST_CASE("device_exit::object/registered-terminal-binding-free-after-quiesce") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://registered-terminal-release", loader), OK);
	GDExtensionDeviceExit::RecordRef record;
	REQUIRE_EQ(Access::retain_record(*extension.ptr(), record), OK);
	RefCounted *object = ::new (DefaultAllocator{}) RefCounted;
	postinitialize_handler(static_cast<Object *>(object));
	REQUIRE_EQ(TestDeviceExitEntry::register_authenticated_lifetime(object, record, Thread::get_caller_id(), 44), OK);
	const GDExtensionInstanceBindingCallbacks callbacks = {
		[](void *, void *instance) -> void * { return instance; }, registered_terminal_free, reference_cohort_callback
	};
	REQUIRE_EQ(object->get_instance_binding(extension.ptr(), &callbacks), object);
	registered_terminal_frees = 0;
	CHECK_EQ(GDExtensionDeviceExit::Registry::get_singleton().begin_quiesce(record), ERR_BUSY);
	REQUIRE(object->unreference());
	CHECK_EQ(extension->close_library(), ERR_BUSY);
	memdelete(object);
	CHECK_EQ(registered_terminal_frees, 1);
	CHECK_EQ(Access::active_frames(*extension.ptr()), 0);
	CHECK_EQ(extension->close_library(), OK);
	owner = nullptr;
}

static void completion_capacity_child() {
	RefCounted *object = ::new (DefaultAllocator{}) RefCounted;
	postinitialize_handler(static_cast<Object *>(object));
	if (!object->reference() || object->get_reference_count() != 2) {
		std::_Exit(96);
	}
	Access::set_completion_count(*object, UINT32_MAX);
	object->unreference();
	std::fputs("COMPLETION_CAPACITY_UNGUARDED\n", stderr);
	std::_Exit(0);
}
REGISTER_TEST_COMMAND("--device-exit-completion-capacity", completion_capacity_child);
TEST_CASE("device_exit::object/init-completion-capacity-refuses-before-native-mutation") {
	RefCounted *object = ::new (DefaultAllocator{}) RefCounted;
	postinitialize_handler(static_cast<Object *>(object));
	for (uint32_t count : { UINT32_MAX, UINT32_MAX - 1 }) {
		Access::set_completion_count(*object, count);
		CHECK_FALSE(object->init_ref());
		CHECK_EQ(object->get_reference_count(), 1);
		CHECK_FALSE(object->is_referenced());
		CHECK_EQ(Access::completion_count(*object), count);
		CHECK_EQ(String(GDExtensionDeviceExit::get_binding_entry_diagnostic()), "BINDING_ASSOCIATION_LIMIT");
	}
	Access::set_completion_count(*object, 0);
	memdelete(object);
}
TEST_CASE("device_exit::object/completion-last-capacity-remains-usable") {
	RefCounted *object = ::new (DefaultAllocator{}) RefCounted;
	postinitialize_handler(static_cast<Object *>(object));
	REQUIRE(object->reference());
	Access::set_completion_count(*object, UINT32_MAX - 1);
	CHECK_FALSE(object->unreference());
	CHECK_EQ(object->get_reference_count(), 1);
	CHECK_EQ(Access::completion_count(*object), UINT32_MAX - 1);
	Access::set_completion_count(*object, UINT32_MAX - 2);
	CHECK(object->init_ref());
	CHECK_EQ(object->get_reference_count(), 1);
	CHECK_EQ(Access::completion_count(*object), UINT32_MAX - 2);
	Access::set_completion_count(*object, 0);
	memdelete(object);
}
TEST_CASE("device_exit::object/reference-admission-before-native-count-mutation") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://reference-cohort", loader), OK);
	RefCounted *object = ::new (DefaultAllocator{}) RefCounted;
	postinitialize_handler(static_cast<Object *>(object));
	const GDExtensionInstanceBindingCallbacks callbacks = {
		[](void *, void *instance) -> void * { return instance; }, nullptr, reference_cohort_callback
	};
	REQUIRE_EQ(object->get_instance_binding(extension.ptr(), &callbacks), object);
	for (bool initialize : { false, true }) {
		reference_cohort_calls = 0;
		const uint32_t old_limit = Access::exhaust_callback_refs(*extension.ptr(), 0);
		const bool accepted = initialize ? object->init_ref() : object->reference();
		CHECK_FALSE(accepted);
		CHECK_EQ(reference_cohort_calls, 0);
		CHECK_EQ(object->get_reference_count(), 1);
		CHECK_FALSE(object->is_referenced());
		Access::restore_callback_refs(*extension.ptr(), old_limit);
		// Repair only the unsafe resident RED path; no extra fixture Ref.
		if (object->get_reference_count() == 2) {
			object->unreference();
		}
	}
	object->free_instance_binding(extension.ptr());
	memdelete(object);
	CHECK_EQ(extension->close_library(), OK);
	owner = nullptr;
}
TEST_CASE("device_exit::object/terminal-release-reserves-return-to-memdelete") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://terminal-release", loader), OK);
	RefCounted *object = ::new (DefaultAllocator{}) RefCounted;
	postinitialize_handler(static_cast<Object *>(object));
	GDExtensionDeviceExit::RecordRef record;
	REQUIRE_EQ(Access::retain_record(*extension.ptr(), record), OK);
	REQUIRE_EQ(TestDeviceExitEntry::register_authenticated_lifetime(object, record, Thread::get_caller_id(), 705), OK);
	REQUIRE(object->init_ref());
	const GDExtensionInstanceBindingCallbacks callbacks = {
		[](void *, void *instance) -> void * { return instance; }, binding_reserved_free, reference_cohort_callback
	};
	binding_reserved_frees = 0;
	REQUIRE_EQ(object->get_instance_binding(extension.ptr(), &callbacks), object);
	REQUIRE(object->unreference());
	CHECK_EQ(object->get_instance_binding(extension.ptr(), nullptr), nullptr);
	CHECK_EQ(String(GDExtensionDeviceExit::get_binding_entry_diagnostic()), "BINDING_ASSOCIATION_BUSY");
	CHECK_EQ(extension->close_library(), ERR_BUSY);
	memdelete(object);
	CHECK_EQ(binding_reserved_frees, 1);
	CHECK_EQ(extension->close_library(), OK);
	owner = nullptr;
}
TEST_CASE("device_exit::object/two-generation-cohort-admission-rolls-back-all-frames") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtension> first, second;
	first.instantiate();
	second.instantiate();
	for (GDExtension *extension : { first.ptr(), second.ptr() }) {
		Ref<GDExtensionFunctionLoader> loader;
		loader.instantiate();
		loader->set_initialization_function(entry);
		REQUIRE_EQ(extension->open_library("libgodot://two-generation-cohort", loader), OK);
	}
	RefCounted *object = ::new (DefaultAllocator{}) RefCounted;
	postinitialize_handler(static_cast<Object *>(object));
	const GDExtensionInstanceBindingCallbacks callbacks = {
		[](void *, void *instance) -> void * { return instance; }, nullptr, reference_cohort_callback
	};
	// Reverse membership order; generation admission order must stay stable.
	REQUIRE_EQ(object->get_instance_binding(second.ptr(), &callbacks), object);
	REQUIRE_EQ(object->get_instance_binding(first.ptr(), &callbacks), object);
	GDExtensionDeviceExit::RecordRef extra;
	REQUIRE_EQ(Access::retain_record(*second.ptr(), extra), OK);
	const uint32_t first_refs = Access::private_refs(*first.ptr());
	const uint32_t second_refs = Access::private_refs(*second.ptr());
	REQUIRE_EQ(second_refs, first_refs + 1);
	const uint32_t old_limit = Access::exhaust_callback_refs(*second.ptr());
	reference_cohort_calls = 0;
	CHECK_FALSE(object->reference());
	CHECK_EQ(reference_cohort_calls, 0);
	CHECK_EQ(object->get_reference_count(), 1);
	CHECK_EQ(Access::private_refs(*first.ptr()), first_refs);
	CHECK_EQ(Access::private_refs(*second.ptr()), second_refs);
	CHECK_EQ(Access::active_frames(*first.ptr()), 0);
	CHECK_EQ(Access::active_frames(*second.ptr()), 0);
	Access::restore_callback_refs(*second.ptr(), old_limit);
	REQUIRE(object->reference());
	CHECK_EQ(reference_cohort_calls, 2);
	CHECK_FALSE(object->unreference());
	CHECK_EQ(reference_cohort_calls, 4);
	memdelete(object);
	extra.reset();
	CHECK_EQ(first->close_library(), OK);
	CHECK_EQ(second->close_library(), OK);
	owner = nullptr;
}
TEST_CASE("device_exit::object/init-compensation-retirement-observation") {
	using namespace RefCountedReleaseTest;
	struct Trace {
		LocalVector<Phase> phases;
		LocalVector<Observation> observations;
	} trace;
	RefCounted *object = ::new (DefaultAllocator{}) RefCounted;
	postinitialize_handler(static_cast<Object *>(object));
	channel = { [](void *context, Phase phase, const Observation &observation) {
				   Trace &trace = *static_cast<Trace *>(context);
				   trace.phases.push_back(phase);
				   trace.observations.push_back(observation);
			   },
		&trace };
	const bool initialized = object->init_ref();
	channel = {};
	CHECK(initialized);
	CHECK_EQ(object->get_reference_count(), 1);
	CHECK(object->is_referenced());
	const Phase expected[] = { Phase::ADMITTED, Phase::BEFORE_MUTATION, Phase::CALLBACK_RESULTS, Phase::COMPLETE, Phase::INIT_PIN_RETIRED, Phase::INIT_COMPLETE };
	CHECK_EQ(trace.phases.size(), 6);
	for (uint32_t i = 0; i < MIN(trace.phases.size(), 6u); i++) {
		CHECK(trace.phases[i] == expected[i]);
		CHECK_EQ(trace.observations[i].operation_id, trace.observations[0].operation_id);
	}
	if (trace.observations.size() >= 3) {
		CHECK(trace.observations[1].borrowed);
		CHECK_FALSE(trace.observations[2].terminal);
		CHECK_EQ(trace.observations[2].refcount_result, 1);
		CHECK_EQ(trace.observations[1].completion_count, 2);
		CHECK_EQ(trace.observations[1].object_read_pins, 1);
	}
	if (trace.observations.size() == 6) {
		CHECK_EQ(trace.observations[3].completion_count, 1);
		CHECK_FALSE(trace.observations[3].pin_retired);
		CHECK(trace.observations[4].pin_retired);
		CHECK_EQ(trace.observations[4].object_read_pins, 0);
		CHECK_EQ(trace.observations[4].completion_count, 1);
		CHECK_EQ(trace.observations[5].completion_count, 0);
	}
	memdelete(object);
}
#ifdef THREADS_ENABLED
namespace TwoRelease {
struct Latch {
	std::mutex mutex;
	std::condition_variable condition;
	bool ready = false;
	void signal() {
		std::lock_guard<std::mutex> lock(mutex);
		ready = true;
		condition.notify_all();
	}
	void wait() {
		std::unique_lock<std::mutex> lock(mutex);
		if (!condition.wait_for(lock, std::chrono::seconds(10), [&] { return ready; })) {
			// A scheduling timeout is a failed fixture, never completion evidence.
			std::_Exit(97);
		}
	}
};
struct State {
	Latch b_admitted, a_results, a_paused, release_a, b_pending, b_done, a_completed;
	bool pause_before_completion = false;
	std::atomic<int> direct_frees{ 0 }, direct_references{ 0 };
	bool has_direct = false;
	std::atomic<int> frees{ 0 };
	std::atomic<int> references{ 0 };
	std::atomic<uint32_t> a_result{ 99 }, b_result{ 99 };
	std::atomic<bool> a_terminal{ true }, b_terminal{ false };
	std::atomic<uint32_t> sequence{ 0 }, pending_seq{ 0 }, release_seq{ 0 }, a_complete_seq{ 0 }, zero_seq{ 0 }, free_seq{ 0 };
	std::atomic<bool> snapshots_valid{ true };
};
static State *state = nullptr;
struct Worker {
	State *state;
	bool first;
};
static void observe(void *context, RefCountedReleaseTest::Phase phase, const RefCountedReleaseTest::Observation &value) {
	using RefCountedReleaseTest::Phase;
	Worker &worker = *static_cast<Worker *>(context);
	State &s = *worker.state;
	if (phase == Phase::BEFORE_MUTATION) {
		if (worker.first) {
			s.b_admitted.wait();
		} else {
			s.b_admitted.signal();
			s.a_results.wait();
		}
	}
	if (phase == Phase::CALLBACK_RESULTS) {
		if (worker.first) {
			s.a_result = value.refcount_result;
			s.a_terminal = value.terminal;
			s.a_results.signal();
		} else {
			s.b_result = value.refcount_result;
			s.b_terminal = value.terminal;
		}
	}
	if (phase == Phase::COMPLETE) {
		if (!value.pin_retired) {
			s.snapshots_valid = false;
		}
		if (worker.first) {
			s.a_complete_seq = ++s.sequence;
			s.a_completed.signal();
		}
	}
	if (worker.first && phase == (s.pause_before_completion ? Phase::PIN_RETIRED : Phase::COMPLETE)) {
		s.a_paused.signal();
		s.release_a.wait();
	}
	if (!worker.first && phase == Phase::JOIN_PENDING) {
		s.pending_seq = ++s.sequence;
		s.b_pending.signal();
	}
	if (!worker.first && phase == Phase::JOIN_ZERO) {
		// Synchronize observation delivery after A's actual completion receipt;
		// this never changes either native decrement or the original zero join.
		s.a_completed.wait();
		s.zero_seq = ++s.sequence;
	}
	if (!worker.first && phase == Phase::TERMINAL_READY && (value.object_read_pins != 0 || value.completion_count != 0)) {
		s.snapshots_valid = false;
	}
}
static GDExtensionBool reference(void *, void *, GDExtensionBool) {
	state->references++;
	return true;
}
static void free(void *, void *, void *) {
	if (state->frees.fetch_add(1) == 0) {
		state->free_seq = ++state->sequence;
	}
}
static bool exercise(Ref<RefCounted> a, Ref<RefCounted> b, GDExtension *first, GDExtension *second, State &s) {
	bool good = true;
	const bool before = s.pause_before_completion;
	Worker wa{ &s, true }, wb{ &s, false };
	std::thread worker_a([ref = std::move(a), &wa]() mutable {
		RefCountedReleaseTest::channel = { observe, &wa };
		ref.unref();
		RefCountedReleaseTest::channel = {};
	});
	std::thread worker_b([ref = std::move(b), &wb]() mutable {
		RefCountedReleaseTest::channel = { observe, &wb };
		ref.unref();
		RefCountedReleaseTest::channel = {};
		wb.state->b_done.signal();
	});
	s.a_paused.wait();
	if (before) {
		// Release A only after B authenticates its pending original join.
		s.b_pending.wait();
		good = (s.frees.load() == 0) && good;
		s.release_seq = ++s.sequence;
		s.release_a.signal();
	}
	s.b_done.wait();
	good = (s.frees.load() == 2) && good;
	good = (s.snapshots_valid.load()) && good;
	if (before) {
		good = (s.pending_seq.load() < s.release_seq.load()) && good;
		good = (s.release_seq.load() < s.a_complete_seq.load()) && good;
		good = (s.a_complete_seq.load() < s.zero_seq.load()) && good;
		good = (s.zero_seq.load() < s.free_seq.load()) && good;
	}
	good = (s.references.load() == 4) && good;
	good = (s.direct_references.load() == (s.has_direct ? 2 : 0)) && good;
	good = (s.direct_frees.load() == (s.has_direct ? 1 : 0)) && good;
	good = (s.a_result.load() == 1) && good;
	good = (s.b_result.load() == 0) && good;
	good = !(s.a_terminal.load()) && good;
	good = (s.b_terminal.load()) && good;
	if (!before) {
		// Object already destroyed; A still owns only independent generation tails.
		good = (first->close_library() == ERR_BUSY) && good;
		good = (second->close_library() == ERR_BUSY) && good;
		good = (Access::active_frames(*first) == 1) && good;
		good = (Access::active_frames(*second) == 1) && good;
		s.release_a.signal();
	}
	worker_a.join();
	worker_b.join();
	good = (Access::active_frames(*first) == 0) && good;
	good = (Access::active_frames(*second) == 0) && good;
	good = (first->close_library() == OK) && good;
	good = (second->close_library() == OK) && good;
	return good;
}
} // namespace TwoRelease
TEST_CASE("device_exit::object/two-real-refs-completion-boundary-resident-control") {
	using namespace TwoRelease;
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	for (bool before : { false, true }) {
		State s;
		s.pause_before_completion = before;
		state = &s;
		Ref<GDExtension> first, second;
		first.instantiate();
		second.instantiate();
		for (GDExtension *extension : { first.ptr(), second.ptr() }) {
			Ref<GDExtensionFunctionLoader> loader;
			loader.instantiate();
			loader->set_initialization_function(entry);
			REQUIRE_EQ(extension->open_library("libgodot://two-real-refs", loader), OK);
		}
		Ref<RefCounted> a = memnew(RefCounted);
		Ref<RefCounted> b = a;
		CHECK_EQ(a->get_reference_count(), 2);
		const GDExtensionInstanceBindingCallbacks callbacks = {
			[](void *, void *instance) -> void * { return instance; }, TwoRelease::free, TwoRelease::reference
		};
		REQUIRE_EQ(a->get_instance_binding(first.ptr(), &callbacks), a.ptr());
		REQUIRE_EQ(a->get_instance_binding(second.ptr(), &callbacks), a.ptr());
		CHECK(TwoRelease::exercise(std::move(a), std::move(b), first.ptr(), second.ptr(), s));
		state = nullptr;
		owner = nullptr;
	}
}
namespace NativeRelease {
static GDExtensionStringNamePtr name(int index) {
	static StringName names[] = { "DeviceExitNativeRelease", "RefCounted" };
	return &names[index == 1 ? 1 : 0];
}
static GDExtension *cleanup_owners[2] = {};
static bool cleanup_close_valid = true;
static GDExtensionInterfaceGetProcAddress original_get = nullptr;
static GDExtensionInstanceBindingCallbacks *publishing = nullptr;
static void publish(const GDExtensionInstanceBindingCallbacks *callbacks) {
	*publishing = *callbacks;
}
static void event(int, char phase) {
	if (cleanup_owners[0] && (phase == 'D' || phase == 'F')) {
		for (GDExtension *extension : cleanup_owners) {
			cleanup_close_valid = (extension->close_library() == ERR_BUSY) && cleanup_close_valid;
		}
	}
	if (phase == 'U') {
		TwoRelease::state->direct_references++;
	}
	if (phase == 'D') {
		TwoRelease::state->direct_frees++;
		TwoRelease::state->free_seq = ++TwoRelease::state->sequence;
	}
	if (phase == 'R') {
		TwoRelease::state->references++;
	}
	if (phase == 'F') {
		TwoRelease::free(nullptr, nullptr, nullptr);
	}
}
static GDExtensionInterfaceFunctionPtr release_get(const char *name) {
	if (std::strcmp(name, "fixture_release_name") == 0) {
		return reinterpret_cast<GDExtensionInterfaceFunctionPtr>(NativeRelease::name);
	}
	if (std::strcmp(name, "fixture_release_publish") == 0) {
		return reinterpret_cast<GDExtensionInterfaceFunctionPtr>(publish);
	}
	if (std::strcmp(name, "fixture_release_event") == 0) {
		return reinterpret_cast<GDExtensionInterfaceFunctionPtr>(event);
	}
	return original_get(name);
}
class Loader : public GDExtensionLibraryLoader {
public:
	GDExtensionInstanceBindingCallbacks callbacks = {};
	Error initialize(GDExtensionInterfaceGetProcAddress p_get, const Ref<GDExtension> &extension, GDExtensionInitialization *result) override {
		original_get = p_get;
		publishing = &callbacks;
		Error error = GDExtensionLibraryLoader::initialize(release_get, extension, result);
		publishing = nullptr;
		return error;
	}
};
static void child() {
	String paths[2];
	bool before = false;
	bool cleanup = false;
	int cleanup_negative = 0;
	for (const String &arg : OS::get_singleton()->get_cmdline_args()) {
		if (arg.begins_with("--release-first=")) {
			paths[0] = arg.trim_prefix("--release-first=");
		}
		if (arg.begins_with("--release-second=")) {
			paths[1] = arg.trim_prefix("--release-second=");
		}
		if (arg == "--release-cleanup-wrong-thread") {
			cleanup = true;
			cleanup_negative = 1;
		}
		if (arg == "--release-cleanup-missing-owner") {
			cleanup = true;
			cleanup_negative = 2;
		}
		if (arg == "--release-cleanup") {
			cleanup = true;
		}
		if (arg == "--release-before") {
			before = true;
		}
	}
	if (paths[0].is_empty() || paths[1].is_empty()) {
		std::_Exit(91);
	}
	Ref<GDExtension> extensions[2];
	Ref<Loader> loaders[2];
	for (int i = 0; i < 2; i++) {
		extensions[i].instantiate();
		loaders[i].instantiate();
		if (extensions[i]->open_library(paths[i], loaders[i]) != OK || !loaders[i]->callbacks.reference_callback) {
			std::_Exit(92);
		}
	}
	const uint64_t first_generation = Access::generation(*extensions[0].ptr());
	const uint64_t second_generation = Access::generation(*extensions[1].ptr());
	if (!first_generation || !second_generation || first_generation == second_generation) {
		std::_Exit(93);
	}
	TwoRelease::State s;
	s.has_direct = true;
	s.pause_before_completion = before;
	TwoRelease::state = &s;
	Ref<RefCounted> a = memnew(RefCounted);
	Ref<RefCounted> b = a;
	if (a->get_reference_count() != 2) {
		std::_Exit(93);
	}
	GDExtensionDeviceExit::RecordRef cleanup_records[2];
	if (cleanup) {
		for (int i = 0; i < 2; i++) {
			if (Access::retain_record(*extensions[i].ptr(), cleanup_records[i]) != OK) {
				std::_Exit(96);
			}
			if (!(cleanup_negative == 2 && i == 1) &&
					TestDeviceExitEntry::register_authenticated_lifetime(a.ptr(), cleanup_records[i], Thread::get_caller_id() + (cleanup_negative == 1 ? 1 : 0), 101 + i) != OK) {
				std::_Exit(96);
			}
			cleanup_owners[i] = extensions[i].ptr();
		}
	}
	ClassDB::set_object_extension_instance(a.ptr(), "DeviceExitNativeRelease", a.ptr());
	for (int i = 0; i < 2; i++) {
		if (a->get_instance_binding(extensions[i].ptr(), &loaders[i]->callbacks) != a.ptr()) {
			std::_Exit(94);
		}
	}
	if (cleanup) {
		for (int i = 0; i < 2; i++) {
			if (GDExtensionDeviceExit::Registry::get_singleton().begin_quiesce(cleanup_records[i]) != ERR_BUSY) {
				std::_Exit(96);
			}
		}
		if (a->reference() || a->get_reference_count() != 2) {
			std::_Exit(96);
		}
		if (cleanup_negative) {
			RefCountedReleaseTest::channel = { [](void *, RefCountedReleaseTest::Phase phase, const RefCountedReleaseTest::Observation &) {
												  if (phase == RefCountedReleaseTest::Phase::ADMITTED) {
													  std::fputs("CLEANUP_NEGATIVE_ADMITTED_BEFORE_MUTATION\n", stderr);
													  std::_Exit(97);
												  }
											  },
				nullptr };
		}
		a.unref();
		if (b->get_reference_count() != 1 || s.references != 2 || s.frees != 0) {
			std::_Exit(96);
		}
		for (GDExtension *extension : cleanup_owners) {
			if (extension->close_library() != ERR_BUSY) {
				std::_Exit(96);
			}
		}
		b.unref();
		if (!cleanup_close_valid || s.references != 4 || s.frees != 2 || s.direct_references != 2 || s.direct_frees != 1) {
			std::_Exit(96);
		}
		for (GDExtension *extension : cleanup_owners) {
			if (Access::active_frames(*extension) != 0 || extension->close_library() != OK) {
				std::_Exit(96);
			}
		}
		cleanup_owners[0] = cleanup_owners[1] = nullptr;
	} else if (!TwoRelease::exercise(std::move(a), std::move(b), extensions[0].ptr(), extensions[1].ptr(), s)) {
		std::_Exit(95);
	}
	auto unregister = reinterpret_cast<GDExtensionInterfaceClassdbUnregisterExtensionClass>(original_get("classdb_unregister_extension_class"));
	unregister(extensions[0].ptr(), name(0));
	TwoRelease::state = nullptr;
	std::fprintf(stderr, "RELEASE_NATIVE good=1 first_generation=%llu second_generation=%llu before=%d cleanup=%d callbacks=%d frees=%d direct_callbacks=%d direct_frees=%d\n", (unsigned long long)first_generation, (unsigned long long)second_generation, int(before), int(cleanup), s.references.load(), s.frees.load(), s.direct_references.load(), s.direct_frees.load());
}
} // namespace NativeRelease
REGISTER_TEST_COMMAND("--device-exit-release-native", NativeRelease::child);

#endif
static int quota_initializes = 0;
static int quota_deinitializes = 0;
static void quota_initialize(void *, GDExtensionInitializationLevel) {
	quota_initializes++;
}
static void quota_deinitialize(void *, GDExtensionInitializationLevel) {
	quota_deinitializes++;
}
static GDExtensionBool quota_entry(GDExtensionInterfaceGetProcAddress, GDExtensionClassLibraryPtr, GDExtensionInitialization *result) {
	*result = {};
	result->initialize = quota_initialize;
	result->deinitialize = quota_deinitialize;
	return true;
}
TEST_CASE("device_exit::entry/typed-level-dispatch-refusal") {
	Ref<GDExtension> extension;
	extension.instantiate();
	ERR_PRINT_OFF;
	CHECK_EQ(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_CORE), ERR_UNCONFIGURED);
	CHECK_EQ(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_CORE), ERR_UNCONFIGURED);
	ERR_PRINT_ON;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(quota_entry);
	REQUIRE_EQ(extension->open_library("libgodot://typed-level-dispatch", loader), OK);
	const int initializations = quota_initializes;
	const int deinitializations = quota_deinitializes;
	ERR_PRINT_OFF;
	CHECK_EQ(extension->initialize_library(GDExtension::InitializationLevel(-1)), ERR_INVALID_PARAMETER);
	CHECK_EQ(extension->initialize_library(GDExtension::InitializationLevel(4)), ERR_INVALID_PARAMETER);
	Access::set_quiescing(*extension.ptr(), true);
	CHECK_EQ(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_CORE), ERR_BUSY);
	CHECK_EQ(Access::level(*extension.ptr()), -1);
	Access::set_quiescing(*extension.ptr(), false);
	Access::set_close_error(*extension.ptr(), ERR_CANT_OPEN);
	CHECK_EQ(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_CORE), ERR_CANT_OPEN);
	CHECK_EQ(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_CORE), ERR_CANT_OPEN);
	Access::set_close_error(*extension.ptr(), OK);
	ERR_PRINT_ON;
	CHECK_EQ(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_CORE), OK);
	CHECK_EQ(quota_initializes, initializations + 1);
	ERR_PRINT_OFF;
	CHECK_EQ(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_CORE), ERR_INVALID_PARAMETER);
	CHECK_EQ(extension->deinitialize_library(GDExtension::InitializationLevel(4)), ERR_INVALID_PARAMETER);
	Access::set_quiescing(*extension.ptr(), true);
	CHECK_EQ(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_CORE), ERR_BUSY);
	CHECK_EQ(Access::level(*extension.ptr()), 0);
	Access::set_quiescing(*extension.ptr(), false);
	Access::clear_deinitialize(*extension.ptr());
	CHECK_EQ(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_CORE), ERR_UNCONFIGURED);
	CHECK_EQ(Access::level(*extension.ptr()), 0);
	ERR_PRINT_ON;
	CHECK_EQ(quota_deinitializes, deinitializations);
	CHECK_EQ(extension->close_library(), ERR_UNAVAILABLE);
	Access::restore_deinitialize(*extension.ptr(), quota_deinitialize);
	CHECK_EQ(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_CORE), OK);
	CHECK_EQ(quota_deinitializes, deinitializations + 1);
	CHECK_EQ(extension->close_library(), OK);
	// The successful close retires the old record; reopen through a fresh
	// loader keeps the test independent of loader-specific prepared state.
	// FunctionLoader close intentionally retires its entry pointer; a reopen
	// uses a freshly configured loader while preserving the new generation.
	Ref<GDExtensionFunctionLoader> null_loader;
	null_loader.instantiate();
	null_loader->set_initialization_function(quota_entry);
	REQUIRE_EQ(extension->open_library("libgodot://typed-level-null", null_loader), OK);
	Access::clear_initialize(*extension.ptr());
	ERR_PRINT_OFF;
	CHECK_EQ(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_CORE), ERR_UNCONFIGURED);
	ERR_PRINT_ON;
	CHECK_EQ(Access::level(*extension.ptr()), -1);
	CHECK_EQ(extension->close_library(), OK);
}

TEST_CASE("device_exit::entry/close-gate-requires-quiesce") {
	close_at_bootstrap = close_at_initialize = close_at_deinitialize = nested_levels = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(quota_entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://close-gate-quiesce", loader), OK);
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	GDExtensionDeviceExit::RecordRef record;
	REQUIRE_EQ(Access::retain_record(*extension.ptr(), record), OK);
	// A caller may not bypass admission sealing even with no active callback.
	CHECK_EQ(registry.begin_close(record), ERR_BUSY);
	CHECK(extension->is_library_open());
	record.reset();
	CHECK_EQ(extension->close_library(), OK);
	owner = nullptr;
}

static void quota_child() {
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(quota_entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	if (extension->open_library("libgodot://callback-quota", loader) != OK) {
		std::_Exit(91);
	}
	// Isolated child: the global quota is restored before every retry and close.
	const uint32_t original = Access::exhaust_callback_refs(*extension.ptr());
	const Error init_error = extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_CORE);
	const bool init_refused = init_error == ERR_PARAMETER_RANGE_ERROR && quota_initializes == 0 && Access::level(*extension.ptr()) == -1;
	Access::restore_callback_refs(*extension.ptr(), original);
	extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_CORE);
	Access::exhaust_callback_refs(*extension.ptr());
	const Error deinit_error = extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_CORE);
	const bool deinit_refused = deinit_error == ERR_PARAMETER_RANGE_ERROR && quota_deinitializes == 0 && Access::level(*extension.ptr()) == 0;
	Access::restore_callback_refs(*extension.ptr(), original);
	extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_CORE);
	const bool good = init_refused && deinit_refused && quota_initializes == 1 && quota_deinitializes == 1 && extension->close_library() == OK;
	std::fprintf(stderr, "ENTRY_QUOTA good=%d initialized=%d deinitialized=%d\n", int(good), quota_initializes, quota_deinitializes);
	if (!good) {
		std::_Exit(94);
	}
}
REGISTER_TEST_COMMAND("--device-exit-entry-quota", quota_child);

static GDExtensionBool null_level_entry(GDExtensionInterfaceGetProcAddress, GDExtensionClassLibraryPtr, GDExtensionInitialization *result) {
	*result = {};
	return true;
}
static void typed_level_manager_child() {
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(null_level_entry);
	std::fprintf(stderr, "LEVEL_MANAGER_BEFORE\n");
	std::fflush(stderr);
	GDExtensionManager::get_singleton()->load_extension_with_loader("libgodot://typed-level-manager", loader);
	std::fprintf(stderr, "LEVEL_MANAGER_UNEXPECTED_RETURN\n");
	std::fflush(stderr);
	std::_Exit(97);
}
REGISTER_TEST_COMMAND("--device-exit-level-manager-refusal", typed_level_manager_child);

static Ref<GDExtension> *last_reference = nullptr;
static void last_ref_initialize(void *, GDExtensionInitializationLevel) {
	std::fprintf(stderr, "ENTRY_LAST_REF_BEFORE_DROP\n");
	std::fflush(stderr);
	last_reference->unref();
	std::fprintf(stderr, "ENTRY_LAST_REF_UNEXPECTED_RETURN\n");
}
static GDExtensionBool last_ref_entry(GDExtensionInterfaceGetProcAddress, GDExtensionClassLibraryPtr, GDExtensionInitialization *result) {
	*result = {};
	result->initialize = last_ref_initialize;
	return true;
}
static void last_ref_child() {
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(last_ref_entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	if (extension->open_library("libgodot://callback-last-ref", loader) != OK) {
		std::_Exit(91);
	}
	last_reference = &extension;
	extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_CORE);
}
REGISTER_TEST_COMMAND("--device-exit-entry-last-ref", last_ref_child);
} // namespace TestDeviceExitEntry
