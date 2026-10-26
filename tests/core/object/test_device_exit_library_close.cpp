/**************************************************************************/
/*  test_device_exit_library_close.cpp                                       */
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
#include "core/extension/gdextension.h"
#include "core/extension/gdextension_function_loader.h"
#include "core/extension/gdextension_library_loader.h"
#include "core/extension/gdextension_manager.h"
#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

#include <cstdio>
#include <cstdlib>

TEST_FORCE_LINK(test_device_exit_library_close)

namespace TestDeviceExitLibraryClose {
struct Access {
	static bool observation(const GDExtension &extension, Error init, Error close) {
		const auto &ref = extension.library_exit_ref;
		if (!ref.registry) {
			return false;
		}
		MutexLock lock(ref.registry->mutex);
		const auto &record = ref.registry->records[ref.slot];
		return record.attached && record.close_returned && record.close_error == close &&
				record.snapshot.loader_initialize_error == init && record.snapshot.wrapper_association == 1;
	}
	static Error retain_record(GDExtension &extension, GDExtensionDeviceExit::RecordRef &out) {
		return extension.library_exit_ref.registry->retain_private(extension.library_exit_ref, out);
	}
	static bool os_facts(const GDExtensionDeviceExit::RecordRef &ref, bool attempted, bool returned, Error error) {
		MutexLock lock(ref.registry->mutex);
		const auto &record = ref.registry->records[ref.slot];
		return record.os_close_attempted == attempted && record.os_close_returned == returned && record.os_close_error == error;
	}
	static void inject(GDExtensionLibraryLoader &loader, Error error) { loader.test_close_error = error; }
	static void *handle(GDExtensionLibraryLoader &loader) { return loader.library; }
	static uint32_t attempts(GDExtensionLibraryLoader &loader) { return loader.test_close_attempts; }
	static bool transitional(GDExtensionLibraryLoader &loader) {
		loader.opening = true;
		const Error opened = loader.close_library();
		loader.opening = false;
		loader.preparing = true;
		const Error prepared = loader.close_library();
		loader.preparing = false;
		loader.closing = true;
		const Error closing = loader.close_library();
		loader.closing = false;
		return opened == ERR_BUSY && prepared == ERR_BUSY && closing == ERR_BUSY && loader.test_close_attempts == 0;
	}
};

static GDExtension *init_abort_observed = nullptr;
void observe_close_failure() {
	if (init_abort_observed) {
		const bool good = Access::observation(*init_abort_observed, ERR_CANT_CREATE, FAILED);
		std::fprintf(stderr, "CLOSE_FIXTURE_ERRORS init=20 close=1 good=%d\n", int(good));
		std::fflush(stderr);
		if (!good) {
			std::_Exit(94);
		}
	}
}

static void no_op_level(void *, GDExtensionInitializationLevel) {}
static int entry_calls = 0;
static void counted_level(void *, GDExtensionInitializationLevel) {
	entry_calls++;
}

class RefusingLoader : public GDExtensionLoader {
	bool open = false;

public:
	Error initialization_result = OK;
	int closes = 0;
	mutable int queries = 0;
	Error open_library(const String &) override {
		open = true;
		return OK;
	}
	Error initialize(GDExtensionInterfaceGetProcAddress, const Ref<GDExtension> &, GDExtensionInitialization *r_initialization) override {
		*r_initialization = {};
		r_initialization->initialize = no_op_level;
		r_initialization->deinitialize = no_op_level;
		return initialization_result;
	}
	Error close_library() override {
		closes++;
		std::fprintf(stderr, "CLOSE_FIXTURE_REFUSED\n");
		std::fflush(stderr);
		return FAILED;
	}
	bool is_library_open() const override {
		queries++;
		return open;
	}
	bool has_library_changed() const override { return false; }
	bool library_exists() const override { return true; }
};

static void last_ref_child() {
	Ref<RefusingLoader> loader;
	loader.instantiate();
	Ref<GDExtension> extension;
	extension.instantiate();
	if (extension->open_library("custom://close-refusal", loader) != OK) {
		std::fprintf(stderr, "CLOSE_FIXTURE_SETUP_FAILED\n");
		return;
	}
	std::fprintf(stderr, "CLOSE_FIXTURE_BEFORE_LAST_REF\n");
	std::fflush(stderr);
	extension.unref();
	std::fprintf(stderr, "CLOSE_FIXTURE_AFTER_LAST_REF\n");
	std::fflush(stderr);
}
REGISTER_TEST_COMMAND("--device-exit-close-last-ref", last_ref_child);

static void init_abort_child() {
	Ref<RefusingLoader> loader;
	loader.instantiate();
	loader->initialization_result = ERR_CANT_CREATE;
	Ref<GDExtension> extension;
	extension.instantiate();
	init_abort_observed = extension.ptr();
	std::fprintf(stderr, "CLOSE_FIXTURE_INIT_ABORT\n");
	std::fflush(stderr);
	extension->open_library("custom://init-abort", loader);
	std::fprintf(stderr, "CLOSE_FIXTURE_UNEXPECTED_RETURN\n");
}
REGISTER_TEST_COMMAND("--device-exit-close-init-abort", init_abort_child);

static void latched_child() {
	Ref<RefusingLoader> loader;
	loader.instantiate();
	Ref<GDExtension> extension;
	extension.instantiate();
	if (extension->open_library("custom://latched", loader) != OK) {
		std::_Exit(91);
	}
	const Error first = extension->close_library();
	const int queries = loader->queries;
	const Error second = extension->close_library();
	const Error reopen = extension->open_library("custom://retry", loader);
	const bool associated = extension->is_library_open();
	const bool good = first == FAILED && second == FAILED && reopen == FAILED &&
			associated && loader->closes == 1 && loader->queries == queries && Access::observation(*extension.ptr(), OK, FAILED);
	std::fprintf(stderr, "CLOSE_FIXTURE_LATCH good=%d calls=%d\n", int(good), loader->closes);
	std::fflush(stderr);
	if (!good) {
		std::_Exit(92);
	}
	extension.unref();
	std::fprintf(stderr, "CLOSE_FIXTURE_UNEXPECTED_RETURN\n");
}
REGISTER_TEST_COMMAND("--device-exit-close-latched", latched_child);

class EntryRefusingLoader : public RefusingLoader {
public:
	Error initialize(GDExtensionInterfaceGetProcAddress, const Ref<GDExtension> &, GDExtensionInitialization *result) override {
		*result = {};
		result->minimum_initialization_level = GDEXTENSION_INITIALIZATION_SCENE;
		result->initialize = counted_level;
		result->deinitialize = counted_level;
		return OK;
	}
};

static void latched_entries_child() {
	Ref<EntryRefusingLoader> loader;
	loader.instantiate();
	Ref<GDExtension> extension;
	extension.instantiate();
	if (extension->open_library("custom://latched-entries", loader) != OK) {
		std::_Exit(91);
	}
	const Error closed = extension->close_library();
	const int queries = loader->queries;
	const Error initialize_error = extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SERVERS);
	const Error deinitialize_error = extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_CORE);
	const auto minimum = extension->get_minimum_library_initialization_level();
	const bool good = closed == FAILED && initialize_error == closed && deinitialize_error == closed && entry_calls == 0 && loader->queries == queries &&
			minimum == GDExtension::INITIALIZATION_LEVEL_CORE && loader->closes == 1;
	std::fprintf(stderr, "CLOSE_FIXTURE_ENTRIES good=%d calls=%d\n", int(good), entry_calls);
	std::fflush(stderr);
	if (!good) {
		std::_Exit(95);
	}
	extension.unref();
}
REGISTER_TEST_COMMAND("--device-exit-close-latched-entries", latched_entries_child);

#ifdef TOOLS_ENABLED
static void reload_child() {
	Engine::get_singleton()->set_extension_reloading_enabled(true);
	Ref<RefusingLoader> loader;
	loader.instantiate();
	auto *manager = GDExtensionManager::get_singleton();
	// Test commands run after editor initialization. Use the real manager's
	// empty bootstrap state so this child reaches close, while initialized
	// reload refusal remains covered by --device-exit-successor-preflight.
	if (!manager->get_loaded_extensions().is_empty()) {
		std::_Exit(90);
	}
	for (int level = GDExtension::INITIALIZATION_LEVEL_EDITOR; level >= GDExtension::INITIALIZATION_LEVEL_CORE; level--) {
		manager->deinitialize_extensions(GDExtension::InitializationLevel(level));
	}
	const String path = "custom://close-reload";
	if (manager->load_extension_with_loader(path, loader) != GDExtensionManager::LOAD_STATUS_OK) {
		std::_Exit(91);
	}
	manager->get_extension(path)->set_reloadable(true);
	std::fprintf(stderr, "CLOSE_FIXTURE_RELOAD\n");
	std::fflush(stderr);
	manager->reload_extension(path);
	std::fprintf(stderr, "CLOSE_FIXTURE_UNEXPECTED_RETURN\n");
}
REGISTER_TEST_COMMAND("--device-exit-close-reload", reload_child);
#endif

static void dynamic_failure_child() {
	String path;
	for (const String &arg : OS::get_singleton()->get_cmdline_args()) {
		if (arg.begins_with("--device-exit-path=")) {
			path = arg.trim_prefix("--device-exit-path=");
		}
	}
	if (path.is_empty()) {
		std::_Exit(91);
	}
	Ref<GDExtensionLibraryLoader> loader;
	loader.instantiate();
	Ref<GDExtension> extension;
	extension.instantiate();
	if (extension->open_library(path, loader) != OK) {
		std::_Exit(92);
	}
	GDExtensionDeviceExit::RecordRef record;
	if (Access::retain_record(*extension.ptr(), record) != OK) { std::_Exit(90); }
	void *const handle = Access::handle(*loader.ptr());
	Access::inject(*loader.ptr(), ERR_CANT_CREATE);
	const Error first = extension->close_library();
	const Error second = extension->close_library();
	const Error direct = loader->close_library();
	const Error prepare = loader->prepare_open(path);
	const Error reopen = loader->open_library(path);
	GDExtensionInitialization initialization = {};
	const Error initialize = loader->initialize(nullptr, extension, &initialization);
	const bool good = handle && Access::handle(*loader.ptr()) == handle &&
			Access::attempts(*loader.ptr()) == 1 && first == ERR_CANT_CREATE && second == first && direct == first &&
			prepare == first && reopen == first && initialize == first && Access::observation(*extension.ptr(), OK, first) &&
			Access::os_facts(record, false, false, OK);
	std::fprintf(stderr, "CLOSE_FIXTURE_DYNAMIC good=%d attempts=%u\n", int(good), Access::attempts(*loader.ptr()));
	std::fflush(stderr);
	if (!good) {
		std::_Exit(93);
	}
	extension.unref();
	std::fprintf(stderr, "CLOSE_FIXTURE_UNEXPECTED_RETURN\n");
}
REGISTER_TEST_COMMAND("--device-exit-close-dynamic-failure", dynamic_failure_child);

TEST_CASE("device_exit::library-close/dynamic-empty-and-transitional") {
	Ref<GDExtensionLibraryLoader> loader;
	loader.instantiate();
	CHECK_EQ(loader->close_library(), ERR_UNCONFIGURED);
	CHECK_EQ(Access::attempts(*loader.ptr()), 0);
	CHECK(Access::transitional(*loader.ptr()));
	CHECK_EQ(loader->close_library(), ERR_UNCONFIGURED);
	CHECK_EQ(Access::attempts(*loader.ptr()), 0);
}

TEST_CASE("device_exit::library-close/function-empty-logical-detach") {
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	CHECK_EQ(loader->close_library(), OK);
	CHECK_FALSE(loader->is_library_open());
	CHECK_EQ(loader->close_library(), OK);
}

class ReentrantLoader : public GDExtensionLoader {
public:
	GDExtension *extension = nullptr;
	bool open = false;
	int closes = 0;
	Error nested = OK;
	Error open_library(const String &) override {
		open = true;
		return OK;
	}
	Error initialize(GDExtensionInterfaceGetProcAddress, const Ref<GDExtension> &owner, GDExtensionInitialization *result) override {
		extension = owner.ptr();
		*result = {};
		return OK;
	}
	Error close_library() override {
		closes++;
		if (closes == 1) {
			nested = extension->close_library();
		}
		open = false;
		return OK;
	}
	bool is_library_open() const override { return open; }
	bool has_library_changed() const override { return false; }
	bool library_exists() const override { return true; }
};

TEST_CASE("device_exit::library-close/wrapper-reentrant-close") {
	Ref<ReentrantLoader> loader;
	loader.instantiate();
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("custom://reentrant-close", loader), OK);
	CHECK_EQ(extension->close_library(), OK);
	CHECK_EQ(loader->nested, ERR_BUSY);
	CHECK_EQ(loader->closes, 1);
}

static uint64_t captured_generation = 0;
class GenerationLoader : public GDExtensionLibraryLoader {
public:
	Error initialize(GDExtensionInterfaceGetProcAddress get, const Ref<GDExtension> &extension, GDExtensionInitialization *result) override {
		auto bootstrap = (GDExtensionInterfaceDeviceExitBootstrapLibraryV1)get("device_exit_bootstrap_library_v1");
		auto retain = (GDExtensionInterfaceDeviceExitRetainLibraryViewV1)get("device_exit_retain_library_view_v1");
		auto copy = (GDExtensionInterfaceDeviceExitCopyLibrarySnapshotV1)get("device_exit_copy_library_snapshot_v1");
		auto release = (GDExtensionInterfaceDeviceExitReleaseLibraryViewV1)get("device_exit_release_library_view_v1");
		const GDExtensionDeviceExitAbi abi = { 1, 16, 0 };
		GDExtensionDeviceExitLibraryCapability capability = {};
		GDExtensionDeviceExitLibraryViewRef view = {};
		GDExtensionDeviceExitLibrarySnapshot snapshot = { { 1, 48, 0 } };
		if (bootstrap(&abi, extension.ptr(), &capability) != OK || retain(&abi, capability, &view) != OK) {
			return FAILED;
		}
		const bool good = copy(view, &snapshot) == OK;
		const bool released = release(&abi, view) == OK;
		if (!good || !released) {
			return FAILED;
		}
		captured_generation = snapshot.library_generation;
		return GDExtensionLibraryLoader::initialize(get, extension, result);
	}
};

static void dynamic_success_child() {
	String path;
	for (const String &arg : OS::get_singleton()->get_cmdline_args()) {
		if (arg.begins_with("--device-exit-path=")) {
			path = arg.trim_prefix("--device-exit-path=");
		}
	}
	if (path.is_empty()) {
		std::_Exit(91);
	}
	Ref<GenerationLoader> loader;
	loader.instantiate();
	Ref<GDExtension> extension;
	extension.instantiate();
	if (extension->open_library(path, loader) != OK) {
		std::_Exit(92);
	}
	const uint64_t first = captured_generation;
	GDExtensionDeviceExit::RecordRef first_record;
	if (Access::retain_record(*extension.ptr(), first_record) != OK || !Access::os_facts(first_record, false, false, OK)) { std::_Exit(90); }
	const Error closed = extension->close_library();
	const bool absent = Access::handle(*loader.ptr()) == nullptr && !extension->is_library_open();
	if (extension->open_library(path, loader) != OK) {
		std::_Exit(93);
	}
	const uint64_t second = captured_generation;
	GDExtensionDeviceExit::RecordRef second_record;
	if (Access::retain_record(*extension.ptr(), second_record) != OK || !Access::os_facts(second_record, false, false, OK)) { std::_Exit(90); }
	const Error closed_again = extension->close_library();
	const bool good = closed == OK && closed_again == OK && absent && first != 0 && second > first &&
			Access::handle(*loader.ptr()) == nullptr && Access::attempts(*loader.ptr()) == 2 &&
			Access::os_facts(first_record, true, true, OK) && Access::os_facts(second_record, true, true, OK);
	std::fprintf(stderr, "CLOSE_FIXTURE_REOPEN good=%d closes=%u\n", int(good), Access::attempts(*loader.ptr()));
	if (!good) {
		std::_Exit(94);
	}
}
REGISTER_TEST_COMMAND("--device-exit-close-dynamic-success", dynamic_success_child);

static GDExtensionBool function_entry(GDExtensionInterfaceGetProcAddress, GDExtensionClassLibraryPtr, GDExtensionInitialization *result) {
	*result = {};
	return true;
}
TEST_CASE("device_exit::library-close/function-success-return") {
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(function_entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://typed-close", loader), OK);
	GDExtensionDeviceExit::RecordRef record;
	REQUIRE_EQ(Access::retain_record(*extension.ptr(), record), OK);
	GDExtensionDeviceExit::CoreCloseReturnFact close_fact;
	close_fact.library_generation = 77;
	CHECK_EQ(GDExtensionDeviceExit::Registry::get_singleton().resolve_core_close_return(record, close_fact), ERR_BUSY);
	CHECK_EQ(close_fact.library_generation, 77);
	CHECK(Access::os_facts(record, false, false, OK));
	CHECK_EQ(extension->close_library(), OK);
	CHECK_EQ(GDExtensionDeviceExit::Registry::get_singleton().resolve_core_close_return(record, close_fact), OK);
	CHECK_EQ(close_fact.library_generation, record.get_generation());
	CHECK(close_fact.close_returned);
	CHECK_EQ(close_fact.close_error, OK);
	CHECK_FALSE(close_fact.os_close_attempted);
	CHECK_FALSE(close_fact.os_close_returned);
	CHECK(Access::os_facts(record, false, false, OK));
	CHECK_FALSE(extension->is_library_open());
	CHECK_FALSE(loader->is_library_open());
}

TEST_CASE("device_exit::library-close/manager-real-dynamic-close-fact") {
	const String image = DirAccess::create(DirAccess::ACCESS_FILESYSTEM)->get_current_dir().path_join("work/typed-lifecycle-abi-v1/native-dynamic/libtyped_dynamic.dylib");
	if (!FileAccess::exists(image)) { return; }
	const String path = TestUtils::get_temp_path("typed-manager-close-fact.gdextension");
	Ref<ConfigFile> config; config.instantiate();
	config->set_value("configuration", "entry_symbol", "typed_dynamic_entry");
	config->set_value("configuration", "compatibility_minimum", "4.1");
	config->set_value("configuration", "device_exit_guard_version", 1);
	config->set_value("libraries", OS::get_singleton()->get_name().to_lower(), image);
	REQUIRE(config->save(path) == OK);
	GDExtensionManager *manager = GDExtensionManager::get_singleton();
	REQUIRE(manager->load_extension(path) == GDExtensionManager::LOAD_STATUS_OK);
	Ref<GDExtension> extension = manager->get_extension(path);
	REQUIRE(extension.is_valid());
	Ref<GDExtensionLibraryLoader> loader = extension->get_loader();
	REQUIRE(loader.is_valid());
	GDExtensionDeviceExit::RecordRef before_close;
	REQUIRE(Access::retain_record(*extension.ptr(), before_close) == OK);
	CHECK(Access::os_facts(before_close, false, false, OK));
	CHECK(manager->unload_extension(path) == GDExtensionManager::LOAD_STATUS_OK);
	CHECK(Access::os_facts(before_close, true, true, OK));
	GDExtensionDeviceExit::CoreCloseReturnFact close_fact;
	REQUIRE_EQ(GDExtensionDeviceExit::Registry::get_singleton().resolve_core_close_return(before_close, close_fact), OK);
	CHECK_EQ(close_fact.library_generation, before_close.get_generation());
	CHECK(close_fact.close_returned);
	CHECK_EQ(close_fact.close_error, OK);
	CHECK(close_fact.os_close_attempted);
	CHECK(close_fact.os_close_returned);
	CHECK_EQ(close_fact.os_close_error, OK);
	CHECK(Access::attempts(*loader.ptr()) == 1);
	CHECK_FALSE(extension->is_library_open());
	extension.unref();
	CHECK(DirAccess::remove_absolute(path) == OK);
}

} //namespace TestDeviceExitLibraryClose
