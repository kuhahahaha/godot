/**************************************************************************/
/*  test_device_exit_contract.cpp                                         */
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

TEST_FORCE_LINK(test_device_exit_contract)

#include "core/extension/gdextension.h"
#include "core/extension/gdextension_function_loader.h"
#include "core/extension/gdextension_library_loader.h"
#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/os/os.h"
#include "tests/test_utils.h"

#include <cstdio>
#include <cstring>
#include <memory>
#ifdef THREADS_ENABLED
#include <atomic>
#include <thread>
#endif

namespace TestDeviceExitContract {

class ObservingLibraryLoader : public GDExtensionLibraryLoader {
public:
	GDExtensionDeviceExitLibraryViewRef view = {};
	GDExtensionInterfaceDeviceExitCopyLibrarySnapshotV1 copy = nullptr;
	GDExtensionInterfaceDeviceExitReleaseLibraryViewV1 release = nullptr;
	Error observation_error = ERR_UNCONFIGURED;
	bool observe = true;

	Error initialize(GDExtensionInterfaceGetProcAddress p_get, const Ref<GDExtension> &p_extension, GDExtensionInitialization *r_initialization) override {
		if (observe) {
			auto bootstrap = (GDExtensionInterfaceDeviceExitBootstrapLibraryV1)p_get("device_exit_bootstrap_library_v1");
			auto retain = (GDExtensionInterfaceDeviceExitRetainLibraryViewV1)p_get("device_exit_retain_library_view_v1");
			copy = (GDExtensionInterfaceDeviceExitCopyLibrarySnapshotV1)p_get("device_exit_copy_library_snapshot_v1");
			release = (GDExtensionInterfaceDeviceExitReleaseLibraryViewV1)p_get("device_exit_release_library_view_v1");
			const GDExtensionDeviceExitAbi abi = { 1, 16, 0 };
			GDExtensionDeviceExitLibraryCapability authority = {};
			if (bootstrap && retain && copy && release) {
				observation_error = Error(bootstrap(&abi, p_extension.ptr(), &authority));
				if (observation_error == OK) {
					observation_error = Error(retain(&abi, authority, &view));
				}
			}
		}
		return GDExtensionLibraryLoader::initialize(p_get, p_extension, r_initialization);
	}
};

static Error reserved_dynamic_open(const String &p_path, const String &p_operation, const Ref<GDExtension> &p_extension, const Ref<GDExtensionLibraryLoader> &p_loader);

// An isolated child command, not a synthetic loader. The parent checks actual
// return values and native constructor/entry bytes from a preopened descriptor.
static void dynamic_child() {
	String path;
	String operation;
	for (const String &arg : OS::get_singleton()->get_cmdline_args()) {
		if (arg.begins_with("--device-exit-path=")) {
			path = arg.trim_prefix("--device-exit-path=");
		} else if (arg.begins_with("--device-exit-operation=")) {
			operation = arg.trim_prefix("--device-exit-operation=");
		}
	}
	if (path.is_empty() || (operation != "direct" && operation != "prepared-config" && !operation.begins_with("reservation-failure-"))) {
		std::printf("DEVICE_EXIT_DYNAMIC_BAD_ARGUMENTS\n");
		return;
	}
	Ref<ObservingLibraryLoader> loader;
	loader.instantiate();
	Ref<GDExtension> extension;
	extension.instantiate();
	Error prepared = OK;
	Error rewritten = OK;
	Error initialized = ERR_UNCONFIGURED;
	Error retry = ERR_UNCONFIGURED;
	Error opened;
	if (operation == "prepared-config") {
		loader->observe = false;
		prepared = loader->prepare_open(path);
		Ref<ConfigFile> config;
		config.instantiate();
		rewritten = config->load(path);
		if (rewritten == OK) {
			config->set_value("configuration", "device_exit_guard_version", 1);
			config->set_value("configuration", "entry_symbol", "must_not_reparse_this_symbol");
			rewritten = config->save(path);
		}
		opened = loader->open_library(path);
		if (opened == OK) {
			GDExtensionInitialization initialization = {};
			initialized = loader->initialize([](const char *p_name) { return GDExtension::get_interface_function(p_name); }, extension, &initialization);
			if (loader->close_library() != OK) {
				std::_Exit(96);
			}
		}
	} else if (operation.begins_with("reservation-failure-")) {
		opened = reserved_dynamic_open(path, operation, extension, loader);
		retry = extension->open_library(path, loader);
		if (extension->is_library_open()) {
			if (extension->close_library() != OK) {
				std::_Exit(96);
			}
		}
	} else {
		opened = extension->open_library(path, loader);
		if (extension->is_library_open()) {
			if (extension->close_library() != OK) {
				std::_Exit(96);
			}
		}
	}
	GDExtensionDeviceExitLibrarySnapshot snapshot = { { 1, 48, 0 }, 0, 0, 0, 0, 0, 0, 0 };
	Error copied = ERR_UNCONFIGURED;
	Error released = ERR_UNCONFIGURED;
	if (loader->view.handle_serial != 0 && loader->copy && loader->release) {
		copied = Error(loader->copy(loader->view, &snapshot));
		const GDExtensionDeviceExitAbi abi = { 1, 16, 0 };
		released = Error(loader->release(&abi, loader->view));
	}
	std::printf("DEVICE_EXIT_DYNAMIC_RESULT prepare=%d rewrite=%d open=%d initialize=%d retry=%d\n", int(prepared), int(rewritten), int(opened), int(initialized), int(retry));
	std::printf("DEVICE_EXIT_DYNAMIC_METADATA observe=%d copy=%d release=%d kind=%u returned=%u error=%d association=%u qualified=%u\n",
			int(loader->observation_error), int(copied), int(released), snapshot.loader_kind,
			snapshot.loader_initialize_returned, snapshot.loader_initialize_error, snapshot.wrapper_association, snapshot.guarded_qualified);
}
REGISTER_TEST_COMMAND("--device-exit-dynamic", dynamic_child);

static int original_entries = 0;
static int replacement_entries = 0;

static void no_op_level(void *, GDExtensionInitializationLevel) {}

static GDExtensionBool original_entry(GDExtensionInterfaceGetProcAddress, GDExtensionClassLibraryPtr, GDExtensionInitialization *r_initialization) {
	original_entries++;
	*r_initialization = {};
	r_initialization->minimum_initialization_level = GDEXTENSION_INITIALIZATION_CORE;
	r_initialization->initialize = no_op_level;
	r_initialization->deinitialize = no_op_level;
	return true;
}

static GDExtensionBool replacement_entry(GDExtensionInterfaceGetProcAddress, GDExtensionClassLibraryPtr, GDExtensionInitialization *r_initialization) {
	replacement_entries++;
	*r_initialization = {};
	r_initialization->minimum_initialization_level = GDEXTENSION_INITIALIZATION_SERVERS;
	r_initialization->initialize = no_op_level;
	r_initialization->deinitialize = no_op_level;
	return true;
}

TEST_CASE("device_exit::foundation/generation/function-loader-control") {
	original_entries = 0;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(original_entry);
	Ref<GDExtension> extension;
	extension.instantiate();

	REQUIRE_EQ(extension->open_library("libgodot://foundation-control", loader), OK);
	CHECK_EQ(original_entries, 1);
	CHECK(extension->is_library_open());
	CHECK_EQ(extension->get_minimum_library_initialization_level(), GDExtension::INITIALIZATION_LEVEL_CORE);
	CHECK_EQ(extension->close_library(), OK);
	CHECK_FALSE(extension->is_library_open());
	CHECK_FALSE(loader->is_library_open());
}

// T11: replacing the loader before rejecting a repeated open must fail this test.
// Both loaders and callbacks are real; only the extension payload is a minimal fixture.
TEST_CASE("device_exit::foundation/generation/reopen-preserves-original-association") {
	original_entries = 0;
	replacement_entries = 0;
	Ref<GDExtensionFunctionLoader> original;
	original.instantiate();
	original->set_initialization_function(original_entry);
	Ref<GDExtensionFunctionLoader> replacement;
	replacement.instantiate();
	replacement->set_initialization_function(replacement_entry);
	Ref<GDExtension> extension;
	extension.instantiate();

	REQUIRE_EQ(extension->open_library("libgodot://foundation-original", original), OK);
	REQUIRE_EQ(original_entries, 1);

	ERR_PRINT_OFF;
	const Error result = extension->open_library("libgodot://foundation-replacement", replacement);
	ERR_PRINT_ON;
	CHECK_EQ(result, ERR_BUSY);
	CHECK_EQ(replacement_entries, 0);
	CHECK_FALSE(replacement->is_library_open());
	CHECK(original->is_library_open());
	CHECK(extension->is_library_open());
	CHECK_EQ(extension->get_minimum_library_initialization_level(), GDExtension::INITIALIZATION_LEVEL_CORE);

	CHECK_EQ(extension->close_library(), OK);
	CHECK_FALSE(original->is_library_open());
	CHECK_FALSE(replacement->is_library_open());
	// A broken implementation strands the original loader. Clean it after the
	// assertions so RED does not turn into a resource leak or shutdown failure.
	if (original->is_library_open()) {
		CHECK_EQ(original->close_library(), OK);
	}
	if (replacement->is_library_open()) {
		CHECK_EQ(replacement->close_library(), OK);
	}
}

TEST_CASE("device_exit::foundation/prepared/function-abort-and-wrong-path") {
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(original_entry);
	CHECK_EQ(loader->prepare_open("libgodot://prepared-original"), OK);
	CHECK_EQ(loader->prepare_open("libgodot://prepared-other"), ERR_BUSY);
	CHECK_EQ(loader->open_library("libgodot://wrong-path"), ERR_INVALID_PARAMETER);
	CHECK_FALSE(loader->is_library_open());
	// Recover the real baseline's erroneous open before testing another attempt.
	if (loader->is_library_open()) {
		CHECK_EQ(loader->close_library(), OK);
		loader->set_initialization_function(original_entry);
	}
	CHECK_EQ(loader->prepare_open("libgodot://prepared-retry"), OK);
	loader->abort_prepared_open();
	CHECK_EQ(loader->prepare_open("libgodot://prepared-retry"), OK);
	CHECK_EQ(loader->open_library("libgodot://prepared-retry"), OK);
	CHECK(loader->is_library_open());
	loader->abort_prepared_open();
	CHECK(loader->is_library_open());
	CHECK_EQ(loader->close_library(), OK);
}

TEST_CASE("device_exit::foundation/prepared/unavailable-can-retry-same-loader") {
	original_entries = 0;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(original_entry);
	loader->set_device_exit_guard_version(1);
	Ref<GDExtension> extension;
	extension.instantiate();
	ERR_PRINT_OFF;
	const Error first = extension->open_library("libgodot://guarded", loader);
	ERR_PRINT_ON;
	CHECK_EQ(first, ERR_UNAVAILABLE);
	CHECK_EQ(original_entries, 1);
	CHECK_FALSE(loader->is_library_open());
	if (extension->is_library_open()) {
		CHECK_EQ(extension->close_library(), OK);
		loader->set_initialization_function(original_entry);
	}
	loader->set_initialization_function(original_entry);
	ERR_PRINT_OFF;
	const Error retry = extension->open_library("libgodot://guarded", loader);
	ERR_PRINT_ON;
	CHECK_EQ(retry, ERR_UNAVAILABLE);
	CHECK_EQ(original_entries, 2);
	if (extension->is_library_open()) {
		CHECK_EQ(extension->close_library(), OK);
	}
	loader->set_device_exit_guard_version(0);
	loader->set_initialization_function(original_entry);
	const int before = original_entries;
	REQUIRE_EQ(extension->open_library("libgodot://guarded", loader), OK);
	CHECK_EQ(original_entries, before + 1);
	CHECK_EQ(extension->close_library(), OK);
}

TEST_CASE("device_exit::foundation/prepared/dynamic-config-version-and-abort") {
	const String path = TestUtils::get_temp_path("foundation-guard.gdextension");
	Ref<ConfigFile> config;
	config.instantiate();
	config->set_value("configuration", "entry_symbol", "foundation_test_entry");
	config->set_value("configuration", "compatibility_minimum", "4.1");
	config->set_value("configuration", "device_exit_guard_version", 1);
	config->set_value("libraries", OS::get_singleton()->get_name().to_lower(), TestUtils::get_temp_path("foundation-absent.dylib"));
	REQUIRE_EQ(config->save(path), OK);
	Ref<GDExtensionLibraryLoader> loader;
	loader.instantiate();
	REQUIRE_EQ(loader->prepare_open(path), OK);
	CHECK_EQ(loader->get_device_exit_guard_version(), 1);
	CHECK_EQ(loader->prepare_open(path), ERR_BUSY);
	config->set_value("configuration", "device_exit_guard_version", 0);
	REQUIRE_EQ(config->save(path), OK);
	// The prepared image stays version1 despite the changed file.
	ERR_PRINT_OFF;
	const Error denied = loader->open_library(path);
	ERR_PRINT_ON;
	CHECK((denied == ERR_CANT_OPEN || denied == ERR_FILE_NOT_FOUND));
	// Version one is now admitted to the dynamic loader; this fixture's
	// selected library is absent, so the prepared path fails to open it.
	CHECK_FALSE(loader->is_library_open());
	REQUIRE_EQ(loader->prepare_open(path), OK);
	CHECK_EQ(loader->get_device_exit_guard_version(), 0);
	loader->abort_prepared_open();
	config->set_value("configuration", "device_exit_guard_version", "1");
	REQUIRE_EQ(config->save(path), OK);
	ERR_PRINT_OFF;
	const Error malformed = loader->prepare_open(path);
	ERR_PRINT_ON;
	CHECK_EQ(malformed, ERR_INVALID_PARAMETER);
	CHECK_EQ(DirAccess::remove_absolute(path), OK);
}

struct MetadataCapture {
	GDExtensionInterfaceDeviceExitBootstrapLibraryV1 bootstrap = nullptr;
	GDExtensionInterfaceDeviceExitRetainLibraryViewV1 retain = nullptr;
	GDExtensionInterfaceDeviceExitCopyLibrarySnapshotV1 copy = nullptr;
	GDExtensionInterfaceDeviceExitReleaseLibraryViewV1 release = nullptr;
	GDExtensionInterfaceDeviceExitCloneLibraryViewV1 clone = nullptr;
	GDExtensionDeviceExitLibraryCapability authority = {};
	GDExtensionDeviceExitLibraryViewRef view = {};
	GDExtensionDeviceExitLibrarySnapshot during_entry = { { 1, 48, 0 }, 0, 0, 0, 0, 0, 0, 0 };
	int32_t boot_error = -1;
	int32_t retain_error = -1;
	int32_t copy_error = -1;
	bool return_success = true;
	void (*during_callback)(GDExtensionClassLibraryPtr) = nullptr;
	bool available() const { return bootstrap && retain && copy && release && clone; }
};
static MetadataCapture *capture = nullptr;
static const GDExtensionDeviceExitAbi request = { 1, 16, 0 };

static GDExtensionBool metadata_entry(GDExtensionInterfaceGetProcAddress p_get_proc, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_init) {
	*r_init = {};
	r_init->initialize = no_op_level;
	r_init->deinitialize = no_op_level;
	capture->bootstrap = (GDExtensionInterfaceDeviceExitBootstrapLibraryV1)p_get_proc("device_exit_bootstrap_library_v1");
	capture->retain = (GDExtensionInterfaceDeviceExitRetainLibraryViewV1)p_get_proc("device_exit_retain_library_view_v1");
	capture->copy = (GDExtensionInterfaceDeviceExitCopyLibrarySnapshotV1)p_get_proc("device_exit_copy_library_snapshot_v1");
	capture->release = (GDExtensionInterfaceDeviceExitReleaseLibraryViewV1)p_get_proc("device_exit_release_library_view_v1");
	capture->clone = (GDExtensionInterfaceDeviceExitCloneLibraryViewV1)p_get_proc("device_exit_clone_library_view_v1");
	if (capture->available()) {
		capture->boot_error = capture->bootstrap(&request, p_library, &capture->authority);
		capture->retain_error = capture->retain(&request, capture->authority, &capture->view);
		capture->copy_error = capture->copy(capture->view, &capture->during_entry);
		if (capture->during_callback) {
			capture->during_callback(p_library);
		}
	}
	return capture->return_success;
}

TEST_CASE("device_exit::foundation/bootstrap/retained-observation-after-wrapper-destruction") {
	MetadataCapture observations;
	capture = &observations;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(metadata_entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	ERR_PRINT_OFF;
	const Error opened = extension->open_library("libgodot://metadata", loader);
	ERR_PRINT_ON;
	REQUIRE_EQ(opened, OK);
	REQUIRE(observations.available());
	if (!observations.available()) {
		capture = nullptr;
		return;
	}
	CHECK_EQ(observations.boot_error, OK);
	CHECK_EQ(observations.retain_error, OK);
	CHECK_EQ(observations.copy_error, OK);
	CHECK_EQ(observations.during_entry.loader_initialize_returned, 0);
	CHECK_EQ(observations.during_entry.loader_kind, 2);
	CHECK_EQ(observations.during_entry.wrapper_association, 1);
	CHECK_NE(observations.during_entry.library_generation, 0);
	GDExtensionDeviceExitLibraryViewRef forbidden = { 42, 42 };
	CHECK_EQ(observations.retain(&request, observations.authority, &forbidden), ERR_INVALID_DATA);
	CHECK_EQ(forbidden.handle_serial, 0);
	GDExtensionDeviceExitLibraryViewRef clone = {};
	CHECK_EQ(observations.clone(&request, observations.view, &clone), OK);
	CHECK_NE(clone.handle_serial, observations.view.handle_serial);
	extension.unref();
	loader.unref();
	GDExtensionDeviceExitLibrarySnapshot snapshot = { { 1, 48, 0 }, 0, 0, 0, 0, 0, 0, 0 };
	CHECK_EQ(observations.copy(observations.view, &snapshot), OK);
	CHECK_EQ(snapshot.wrapper_association, 2);
	CHECK_EQ(snapshot.loader_initialize_returned, 1);
	CHECK_EQ(snapshot.loader_initialize_error, OK);
	CHECK_EQ(snapshot.guarded_qualified, 0);
	CHECK_EQ(observations.release(&request, observations.view), OK);
	CHECK_EQ(observations.release(&request, observations.view), ERR_INVALID_DATA);
	CHECK_EQ(observations.copy(clone, &snapshot), OK);
	CHECK_EQ(observations.release(&request, clone), OK);
	const auto before = snapshot;
	CHECK_EQ(observations.copy(clone, &snapshot), ERR_INVALID_DATA);
	CHECK_EQ(std::memcmp(&snapshot, &before, sizeof(snapshot)), 0);
	capture = nullptr;
}

// Exercise the registered C function pointers while the actual FunctionLoader
// owns bootstrap authority. Bad requests also carry invalid handles so ABI
// validation must win without consuming the live reference.
static void exercise_registered_abi(GDExtensionClassLibraryPtr p_library) {
	MetadataCapture &api = *capture;
	alignas(8) unsigned char bad_request_storage[32] = {};
	GDExtensionDeviceExitAbi invalid[] = {
		{ 0, 16, 0 }, { 2, 16, 0 }, { 1, 0, 0 }, { 1, 15, 0 },
		{ 1, 17, 0 }, { 1, 16, 1 }, { 1, 16, UINT64_MAX }
	};
	const GDExtensionDeviceExitAbi *requests[] = {
		nullptr, reinterpret_cast<const GDExtensionDeviceExitAbi *>(bad_request_storage + 1),
		&invalid[0], &invalid[1], &invalid[2], &invalid[3], &invalid[4], &invalid[5], &invalid[6]
	};
	for (const auto *bad : requests) {
		GDExtensionDeviceExitLibraryCapability capability = { 99, 99 };
		GDExtensionDeviceExitLibraryViewRef output = { 99, 99 };
		CHECK_EQ(api.bootstrap(bad, p_library, &capability), ERR_INVALID_PARAMETER);
		CHECK_EQ(capability.registry_instance, 0);
		CHECK_EQ(capability.handle_serial, 0);
		CHECK_EQ(api.retain(bad, {}, &output), ERR_INVALID_PARAMETER);
		CHECK_EQ(output.registry_instance, 0);
		CHECK_EQ(output.handle_serial, 0);
		output = { 99, 99 };
		CHECK_EQ(api.clone(bad, {}, &output), ERR_INVALID_PARAMETER);
		CHECK_EQ(output.registry_instance, 0);
		CHECK_EQ(output.handle_serial, 0);
		CHECK_EQ(api.release(bad, {}), ERR_INVALID_PARAMETER);
		CHECK_EQ(api.release(bad, api.view), ERR_INVALID_PARAMETER);
	}
	CHECK_EQ(api.bootstrap(&request, p_library, nullptr), ERR_INVALID_PARAMETER);
	CHECK_EQ(api.retain(&request, api.authority, nullptr), ERR_INVALID_PARAMETER);
	CHECK_EQ(api.clone(&request, api.view, nullptr), ERR_INVALID_PARAMETER);
	CHECK_EQ(api.copy(api.view, nullptr), ERR_INVALID_PARAMETER);
	alignas(8) unsigned char storage[80];
	unsigned char unchanged[80];
	std::memset(storage, 0xA5, sizeof(storage));
	std::memcpy(unchanged, storage, sizeof(storage));
	CHECK_EQ(api.bootstrap(&request, p_library, reinterpret_cast<GDExtensionDeviceExitLibraryCapability *>(storage + 1)), ERR_INVALID_PARAMETER);
	CHECK_EQ(api.retain(&request, api.authority, reinterpret_cast<GDExtensionDeviceExitLibraryViewRef *>(storage + 1)), ERR_INVALID_PARAMETER);
	CHECK_EQ(api.clone(&request, api.view, reinterpret_cast<GDExtensionDeviceExitLibraryViewRef *>(storage + 1)), ERR_INVALID_PARAMETER);
	CHECK_EQ(api.copy(api.view, reinterpret_cast<GDExtensionDeviceExitLibrarySnapshot *>(storage + 1)), ERR_INVALID_PARAMETER);
	CHECK_EQ(std::memcmp(storage, unchanged, sizeof(storage)), 0);
	struct SnapshotBuffer {
		uint64_t before = 0x123456789;
		GDExtensionDeviceExitLibrarySnapshot snapshot = { { 1, 48, 0 }, 0, 0, 0, 0, 0, 0, 0 };
		uint64_t after = 0x987654321;
	} buffer;
	const GDExtensionDeviceExitAbi snapshot_invalid[] = {
		{ 0, 48, 0 }, { 2, 48, 0 }, { 1, 0, 0 }, { 1, 47, 0 },
		{ 1, 49, 0 }, { 1, 48, 1 }, { 1, 48, UINT64_MAX }
	};
	for (const auto &bad : snapshot_invalid) {
		buffer.snapshot.abi = bad;
		const SnapshotBuffer saved = buffer;
		CHECK_EQ(api.copy(api.view, &buffer.snapshot), ERR_INVALID_PARAMETER);
		CHECK_EQ(api.copy({}, &buffer.snapshot), ERR_INVALID_PARAMETER);
		CHECK_EQ(std::memcmp(&buffer, &saved, sizeof(buffer)), 0);
	}
	buffer.snapshot.abi = { 1, 48, 0 };
	CHECK_EQ(api.copy(api.view, &buffer.snapshot), OK);
	CHECK_EQ(buffer.before, 0x123456789);
	CHECK_EQ(buffer.after, 0x987654321);
	CHECK_EQ(buffer.snapshot.reserved, 0);
	CHECK_EQ(buffer.snapshot.library_generation, api.during_entry.library_generation);
	GDExtensionDeviceExitLibraryViewRef output = { 99, 99 };
	CHECK_EQ(api.clone(&request, { api.authority.registry_instance, api.authority.handle_serial }, &output), ERR_INVALID_DATA);
	CHECK_EQ(output.handle_serial, 0);
	CHECK_EQ(api.retain(&request, { api.view.registry_instance, api.view.handle_serial }, &output), ERR_INVALID_DATA);
	CHECK_EQ(output.handle_serial, 0);
	CHECK_EQ(api.release(&request, { api.authority.registry_instance, api.authority.handle_serial }), ERR_INVALID_DATA);
	CHECK_EQ(api.copy(api.view, &buffer.snapshot), OK);
}

TEST_CASE("device_exit::foundation/abi/registered-five-operation-validation") {
	MetadataCapture observations;
	observations.during_callback = exercise_registered_abi;
	capture = &observations;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(metadata_entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	CHECK_EQ(extension->open_library("libgodot://registered-abi", loader), OK);
	CHECK(observations.available());
	if (observations.available()) {
		GDExtensionDeviceExitAbi bad = { 2, 16, 0 };
		GDExtensionDeviceExitLibraryCapability capability = { 99, 99 };
		GDExtensionDeviceExitLibraryViewRef output = { 99, 99 };
		CHECK_EQ(observations.bootstrap(&bad, extension.ptr(), &capability), ERR_INVALID_PARAMETER);
		CHECK_EQ(capability.handle_serial, 0);
		CHECK_EQ(observations.retain(&bad, observations.authority, &output), ERR_INVALID_PARAMETER);
		CHECK_EQ(output.handle_serial, 0);
		CHECK_EQ(extension->close_library(), OK);
		CHECK_EQ(observations.release(&request, observations.view), OK);
		CHECK_EQ(observations.release(&bad, observations.view), ERR_INVALID_PARAMETER);
		CHECK_EQ(observations.release(&request, observations.view), ERR_INVALID_DATA);
	}
	capture = nullptr;
}

using GDExtensionDeviceExit::RecordRef;
using GDExtensionDeviceExit::Registry;

struct NestedCapture {
	MetadataCapture outer;
	MetadataCapture inner;
	GDExtensionClassLibraryPtr outer_library = nullptr;
	Error inner_open = FAILED;
	int32_t foreign_boot = -1;
	int32_t foreign_retain = -1;
	int32_t restored_boot = -1;
	int32_t restored_retain = -1;
	int32_t worker_boot = -1;
	int32_t worker_retain = -1;
	GDExtensionDeviceExitLibraryCapability foreign = { 99, 99 };
	GDExtensionDeviceExitLibraryCapability restored = {};
	GDExtensionDeviceExitLibraryCapability worker = { 99, 99 };
	GDExtensionDeviceExitLibraryViewRef foreign_view = { 99, 99 };
	GDExtensionDeviceExitLibraryViewRef outer_second_view = {};
	GDExtensionDeviceExitLibraryViewRef worker_view = { 99, 99 };
};
static NestedCapture *nested_capture = nullptr;

static void nested_inner(GDExtensionClassLibraryPtr) {
	auto &n = *nested_capture;
	n.foreign_boot = n.inner.bootstrap(&request, n.outer_library, &n.foreign);
	n.foreign_retain = n.inner.retain(&request, n.outer.authority, &n.foreign_view);
}

static void nested_outer(GDExtensionClassLibraryPtr p_library) {
	auto &n = *nested_capture;
	n.outer_library = p_library;
#ifdef THREADS_ENABLED
	std::thread worker([&]() {
		n.worker_boot = n.outer.bootstrap(&request, p_library, &n.worker);
		n.worker_retain = n.outer.retain(&request, n.outer.authority, &n.worker_view);
	});
	worker.join();
#endif
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(metadata_entry);
	Ref<GDExtension> inner;
	inner.instantiate();
	capture = &n.inner;
	n.inner_open = inner->open_library("libgodot://nested-inner", loader);
	capture = &n.outer;
	n.restored_boot = n.outer.bootstrap(&request, p_library, &n.restored);
	n.restored_retain = n.outer.retain(&request, n.outer.authority, &n.outer_second_view);
}

// T02/T03: accepting a foreign owner, using non-TLS authority, or failing to
// restore the outer scope on nested return must fail observable API assertions.
TEST_CASE("device_exit::foundation/bootstrap/nested-owner-and-thread-scope") {
	NestedCapture observations;
	nested_capture = &observations;
	observations.outer.during_callback = nested_outer;
	observations.inner.during_callback = nested_inner;
	capture = &observations.outer;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(metadata_entry);
	Ref<GDExtension> outer;
	outer.instantiate();
	CHECK_EQ(outer->open_library("libgodot://nested-outer", loader), OK);
	CHECK(observations.outer.available());
	CHECK(observations.inner.available());
	if (observations.outer.available() && observations.inner.available()) {
		CHECK_EQ(observations.inner_open, OK);
		CHECK_EQ(observations.foreign_boot, ERR_INVALID_DATA);
		CHECK_EQ(observations.foreign_retain, ERR_INVALID_DATA);
		CHECK_EQ(observations.foreign.handle_serial, 0);
		CHECK_EQ(observations.foreign_view.handle_serial, 0);
		CHECK_EQ(observations.restored_boot, OK);
		CHECK_EQ(observations.restored_retain, OK);
		CHECK_EQ(observations.restored.handle_serial, observations.outer.authority.handle_serial);
		CHECK_NE(observations.inner.authority.handle_serial, observations.outer.authority.handle_serial);
		CHECK_NE(observations.inner.during_entry.library_generation, observations.outer.during_entry.library_generation);
		CHECK_NE(observations.outer_second_view.handle_serial, observations.outer.view.handle_serial);
#ifdef THREADS_ENABLED
		CHECK_EQ(observations.worker_boot, ERR_INVALID_DATA);
		CHECK_EQ(observations.worker_retain, ERR_INVALID_DATA);
		CHECK_EQ(observations.worker.handle_serial, 0);
		CHECK_EQ(observations.worker_view.handle_serial, 0);
#endif
		CHECK_EQ(observations.outer.release(&request, observations.outer.view), OK);
		GDExtensionDeviceExitLibrarySnapshot snapshot = { { 1, 48, 0 }, 0, 0, 0, 0, 0, 0, 0 };
		CHECK_EQ(observations.outer.copy(observations.outer_second_view, &snapshot), OK);
		CHECK_EQ(snapshot.library_generation, observations.outer.during_entry.library_generation);
		CHECK_EQ(observations.outer.release(&request, observations.outer_second_view), OK);
		CHECK_EQ(observations.inner.copy(observations.inner.view, &snapshot), OK);
		CHECK_EQ(snapshot.wrapper_association, 2);
		CHECK_EQ(observations.inner.release(&request, observations.inner.view), OK);
	}
	capture = nullptr;
	nested_capture = nullptr;
}

struct RegistryAccess {
	static void refuse_external_issuance(MetadataCapture &p_api, bool p_exhaust_serial) {
		auto &registry = Registry::get_singleton();
		const uint32_t old_limit = registry.view_limit;
		const bool old_exhausted = registry.serials.exhausted;
		if (p_exhaust_serial) {
			registry.serials.exhausted = true;
		} else {
			registry.view_limit = 0;
		}
		GDExtensionDeviceExitLibraryViewRef refused = { 99, 99 };
		const int32_t retained = p_api.retain(&request, p_api.authority, &refused);
		const auto retained_output = refused;
		// Restore lookup coverage before clone; cap at the occupied slot count
		// for quota refusal, so finding the source is still legal.
		registry.view_limit = old_limit;
		uint32_t occupied_end = 0;
		for (uint32_t i = 0; i < old_limit; i++) {
			if (registry.views[i].serial != 0) {
				occupied_end = i + 1;
			}
		}
		if (!p_exhaust_serial) {
			registry.view_limit = occupied_end;
		}
		refused = { 99, 99 };
		const int32_t cloned = p_api.clone(&request, p_api.view, &refused);
		registry.view_limit = old_limit;
		registry.serials.exhausted = old_exhausted;
		CHECK_EQ(retained, ERR_PARAMETER_RANGE_ERROR);
		CHECK_EQ(retained_output.registry_instance, 0);
		CHECK_EQ(retained_output.handle_serial, 0);
		CHECK_EQ(cloned, ERR_PARAMETER_RANGE_ERROR);
		CHECK_EQ(refused.registry_instance, 0);
		CHECK_EQ(refused.handle_serial, 0);
		GDExtensionDeviceExitLibrarySnapshot snapshot = { { 1, 48, 0 }, 0, 0, 0, 0, 0, 0, 0 };
		CHECK_EQ(p_api.copy(p_api.view, &snapshot), OK);
		GDExtensionDeviceExitLibraryViewRef retry = {};
		CHECK_EQ(p_api.retain(&request, p_api.authority, &retry), OK);
		CHECK_NE(retry.handle_serial, p_api.view.handle_serial);
		CHECK_EQ(p_api.release(&request, retry), OK);
		CHECK_EQ(p_api.clone(&request, p_api.view, &retry), OK);
		CHECK_EQ(p_api.release(&request, retry), OK);
	}
	static Error retain_current(RecordRef &r_ref) {
		auto *scope = GDExtensionDeviceExit::BootstrapScope::current;
		return scope ? scope->reference.registry->retain_private(scope->reference, r_ref) : ERR_INVALID_DATA;
	}
	static Error fail_reservation(const String &p_path, const String &p_operation, const Ref<GDExtension> &p_extension, const Ref<GDExtensionLibraryLoader> &p_loader) {
		auto &registry = Registry::get_singleton();
		const uint32_t record_limit = registry.record_limit;
		const uint32_t private_limit = registry.private_limit;
		const bool generation_exhausted = registry.generations.exhausted;
		const bool authority_exhausted = registry.serials.exhausted;
		if (p_operation == "reservation-failure-record") {
			registry.record_limit = 0;
		} else if (p_operation == "reservation-failure-private") {
			registry.private_limit = 1;
		} else if (p_operation == "reservation-failure-generation") {
			registry.generations.exhausted = true;
		} else if (p_operation == "reservation-failure-authority") {
			registry.serials.exhausted = true;
		} else {
			return ERR_INVALID_PARAMETER;
		}
		const Error result = p_extension->open_library(p_path, p_loader);
		registry.record_limit = record_limit;
		registry.private_limit = private_limit;
		registry.generations.exhausted = generation_exhausted;
		registry.serials.exhausted = authority_exhausted;
		return result;
	}
	static std::unique_ptr<Registry> make(uint32_t p_records = 2, uint32_t p_views = 3, uint32_t p_private = 4) {
		std::unique_ptr<Registry> result(new Registry);
		result->record_limit = p_records;
		result->view_limit = p_views;
		result->private_limit = p_private;
		return result;
	}
	static void counters(Registry &p_registry, uint64_t p_generation, uint64_t p_serial) {
		p_registry.generations.next = p_generation;
		p_registry.serials.next = p_serial;
	}
	static bool sequence_boundary() {
		Registry::Sequence seq;
		seq.next = UINT64_MAX;
		uint64_t value = 0;
		return seq.take(value) && value == UINT64_MAX && !seq.take(value) && value == UINT64_MAX;
	}
	static uint32_t occupied(Registry &p_registry) {
		uint32_t count = 0;
		for (uint32_t i = 0; i < p_registry.record_limit; i++) {
			count += p_registry.records[i].occupied ? 1 : 0;
		}
		return count;
	}
	static Error view(Registry &p_registry, RecordRef &&p_scope, GDExtensionDeviceExitLibraryViewRef &r_view) {
		// Isolated registry fixture; real loader bootstrap is tested separately.
		GDExtensionDeviceExit::BootstrapScope scope(std::move(p_scope), nullptr);
		GDExtensionDeviceExitLibraryCapability authority = {};
		Error err = Error(p_registry.bootstrap(&request, nullptr, &authority));
		if (err != OK) {
			return err;
		}
		return Error(p_registry.retain(&request, authority, &r_view));
	}
};

static Error reserved_dynamic_open(const String &p_path, const String &p_operation, const Ref<GDExtension> &p_extension, const Ref<GDExtensionLibraryLoader> &p_loader) {
	return RegistryAccess::fail_reservation(p_path, p_operation, p_extension, p_loader);
}

static void check_external_reservation_failures(GDExtensionClassLibraryPtr) {
	RegistryAccess::refuse_external_issuance(*capture, false);
	RegistryAccess::refuse_external_issuance(*capture, true);
}

TEST_CASE("device_exit::foundation/reservation/actual-bootstrap-view-capacity-and-serial") {
	MetadataCapture observations;
	observations.during_callback = check_external_reservation_failures;
	capture = &observations;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(metadata_entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	CHECK_EQ(extension->open_library("libgodot://view-reservation", loader), OK);
	CHECK(observations.available());
	extension.unref();
	if (observations.available()) {
		CHECK_EQ(observations.release(&request, observations.view), OK);
	}
	capture = nullptr;
}

static RecordRef *private_capture = nullptr;
static void retain_actual_core_reference(GDExtensionClassLibraryPtr) {
	CHECK_EQ(RegistryAccess::retain_current(*private_capture), OK);
}

TEST_CASE("device_exit::foundation/core-retention/actual-loader-private-hold") {
	auto &registry = Registry::get_singleton();
	const uint32_t before = RegistryAccess::occupied(registry);
	RecordRef work;
	private_capture = &work;
	MetadataCapture observations;
	observations.during_callback = retain_actual_core_reference;
	capture = &observations;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(metadata_entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	CHECK_EQ(extension->open_library("libgodot://private-retention", loader), OK);
	CHECK(work.is_valid());
	CHECK(observations.available());
	extension.unref();
	loader.unref();
	if (observations.available()) {
		CHECK_EQ(observations.release(&request, observations.view), OK);
	}
	CHECK_EQ(RegistryAccess::occupied(registry), before + 1);
	work.reset();
	CHECK_EQ(RegistryAccess::occupied(registry), before);
	private_capture = nullptr;
	capture = nullptr;
}

TEST_CASE("device_exit::foundation/isolated/capacity-reclaim-and-private-retention") {
	auto registry = RegistryAccess::make(1, 1, 3);
	uint64_t cycle = 0;
	REQUIRE_EQ(registry->begin_cycle(cycle), OK);
	RecordRef owner, scope, private_work, refused;
	REQUIRE_EQ(registry->reserve_open(2, owner, scope), OK);
	if (!owner.is_valid() || !scope.is_valid()) {
		return;
	}
	registry->attach(owner);
	CHECK_EQ(registry->retain_private(owner, private_work), OK);
	CHECK_EQ(registry->retain_private(owner, refused), ERR_PARAMETER_RANGE_ERROR);
	CHECK_FALSE(refused.is_valid());
	RecordRef extra, extra_scope;
	CHECK_EQ(registry->reserve_open(2, extra, extra_scope), ERR_PARAMETER_RANGE_ERROR);
	GDExtensionDeviceExitLibraryViewRef view = {};
	REQUIRE_EQ(RegistryAccess::view(*registry, std::move(scope), view), OK);
	GDExtensionDeviceExitLibraryViewRef clone = { 99, 99 };
	CHECK_EQ(registry->clone(&request, view, &clone), ERR_PARAMETER_RANGE_ERROR);
	CHECK_EQ(clone.handle_serial, 0);
	registry->detach(owner);
	owner.reset();
	CHECK_EQ(registry->release(&request, view), OK);
	CHECK_EQ(RegistryAccess::occupied(*registry), 1);
	CHECK(registry->begin_finalizing(cycle));
	registry->finish_engine_cycle(cycle);
	uint64_t next = 77;
	CHECK_EQ(registry->begin_cycle(next), ERR_BUSY);
	CHECK_EQ(next, 0);
	private_work.reset();
	CHECK_EQ(RegistryAccess::occupied(*registry), 0);
	CHECK_EQ(registry->begin_cycle(next), OK);
	CHECK_NE(next, cycle);
}

TEST_CASE("device_exit::foundation/isolated/finalizing-needs-engine-end-and-last-view") {
	auto registry = RegistryAccess::make();
	uint64_t cycle = 0;
	REQUIRE_EQ(registry->begin_cycle(cycle), OK);
	RecordRef owner, scope;
	REQUIRE_EQ(registry->reserve_open(2, owner, scope), OK);
	if (!owner.is_valid() || !scope.is_valid()) {
		return;
	}
	registry->attach(owner);
	GDExtensionDeviceExitLibraryViewRef view = {};
	REQUIRE_EQ(RegistryAccess::view(*registry, std::move(scope), view), OK);
	CHECK_FALSE(registry->begin_finalizing(cycle + 1));
	CHECK(registry->begin_finalizing(cycle));
	CHECK_FALSE(registry->begin_finalizing(cycle));
	GDExtensionDeviceExitLibraryViewRef clone = {};
	CHECK_EQ(registry->clone(&request, view, &clone), ERR_BUSY);
	registry->detach(owner);
	owner.reset();
	GDExtensionDeviceExitLibrarySnapshot snapshot = { { 1, 48, 0 }, 0, 0, 0, 0, 0, 0, 0 };
	CHECK_EQ(registry->copy(view, &snapshot), OK);
	CHECK_EQ(snapshot.wrapper_association, 2);
	CHECK_EQ(registry->release(&request, view), OK);
	CHECK_EQ(RegistryAccess::occupied(*registry), 0);
	uint64_t next = 0;
	CHECK_EQ(registry->begin_cycle(next), ERR_BUSY);
	registry->finish_engine_cycle(cycle + 1);
	CHECK_EQ(registry->begin_cycle(next), ERR_BUSY);
	registry->finish_engine_cycle(cycle);
	CHECK_EQ(registry->begin_cycle(next), OK);
	CHECK_NE(next, cycle);
	CHECK_EQ(registry->copy(view, &snapshot), ERR_INVALID_DATA);
	registry->finish_engine_cycle(cycle);
	CHECK_FALSE(registry->begin_finalizing(cycle));
	RecordRef new_owner, new_scope;
	CHECK_EQ(registry->reserve_open(2, new_owner, new_scope), OK);
}

TEST_CASE("device_exit::foundation/isolated/exhaustion-and-failed-reservation-slots") {
	CHECK(RegistryAccess::sequence_boundary());
	auto registry = RegistryAccess::make(1, 2);
	RegistryAccess::counters(*registry, UINT64_MAX, UINT64_MAX - 1);
	uint64_t cycle;
	REQUIRE_EQ(registry->begin_cycle(cycle), OK);
	RecordRef owner, scope;
	REQUIRE_EQ(registry->reserve_open(2, owner, scope), OK);
	if (!owner.is_valid() || !scope.is_valid()) {
		return;
	}
	registry->attach(owner);
	GDExtensionDeviceExitLibraryViewRef view = {};
	CHECK_EQ(RegistryAccess::view(*registry, std::move(scope), view), OK);
	CHECK_EQ(view.handle_serial, UINT64_MAX);
	GDExtensionDeviceExitLibraryViewRef clone = {};
	CHECK_EQ(registry->clone(&request, view, &clone), ERR_PARAMETER_RANGE_ERROR);
	GDExtensionDeviceExitLibrarySnapshot snapshot = { { 1, 48, 0 }, 0, 0, 0, 0, 0, 0, 0 };
	CHECK_EQ(registry->copy(view, &snapshot), OK);
	CHECK_EQ(snapshot.library_generation, UINT64_MAX);
	CHECK_EQ(registry->release(&request, view), OK);
	registry->detach(owner);
	owner.reset();
	CHECK_EQ(RegistryAccess::occupied(*registry), 0);
	CHECK_EQ(registry->reserve_open(2, owner, scope), ERR_PARAMETER_RANGE_ERROR);
	CHECK_EQ(RegistryAccess::occupied(*registry), 0);

	auto retry_registry = RegistryAccess::make(1, 1);
	REQUIRE_EQ(retry_registry->begin_cycle(cycle), OK);
	for (int i = 0; i < 20; i++) {
		RecordRef rejected_owner, rejected_scope;
		CHECK_EQ(retry_registry->reserve_open(1, rejected_owner, rejected_scope), OK);
		// No open/attach occurred. Both reserved holds must return their slot.
	}
	CHECK_EQ(RegistryAccess::occupied(*retry_registry), 0);
}

TEST_CASE("device_exit::foundation/isolated/abi-validation-and-cross-registry") {
	auto registry = RegistryAccess::make();
	auto foreign = RegistryAccess::make();
	uint64_t cycle, other;
	REQUIRE_EQ(registry->begin_cycle(cycle), OK);
	REQUIRE_EQ(foreign->begin_cycle(other), OK);
	CHECK_NE(cycle, other);
	RecordRef owner, scope;
	REQUIRE_EQ(registry->reserve_open(2, owner, scope), OK);
	if (!owner.is_valid() || !scope.is_valid()) {
		return;
	}
	registry->attach(owner);
	GDExtensionDeviceExitLibraryViewRef view = {};
	REQUIRE_EQ(RegistryAccess::view(*registry, std::move(scope), view), OK);
	GDExtensionDeviceExitLibrarySnapshot snapshot = { { 1, 48, 0 }, 0, 0, 0, 0, 0, 0, 0 };
	CHECK_EQ(foreign->copy(view, &snapshot), ERR_INVALID_DATA);
	for (int field = 0; field < 3; field++) {
		GDExtensionDeviceExitAbi bad = request;
		if (field == 0) {
			bad.abi_version = 2;
		}
		if (field == 1) {
			bad.struct_size = 8;
		}
		if (field == 2) {
			bad.reserved = 1;
		}
		GDExtensionDeviceExitLibraryViewRef output = { 99, 99 };
		CHECK_EQ(registry->clone(&bad, view, &output), ERR_INVALID_PARAMETER);
		CHECK_EQ(output.registry_instance, 0);
		CHECK_EQ(output.handle_serial, 0);
	}
	CHECK_EQ(registry->copy(view, nullptr), ERR_INVALID_PARAMETER);
	alignas(8) unsigned char unaligned[64] = {};
	CHECK_EQ(registry->copy(view, reinterpret_cast<GDExtensionDeviceExitLibrarySnapshot *>(unaligned + 1)), ERR_INVALID_PARAMETER);
	snapshot.abi.reserved = 1;
	const auto unchanged = snapshot;
	CHECK_EQ(registry->copy(view, &snapshot), ERR_INVALID_PARAMETER);
	CHECK_EQ(std::memcmp(&snapshot, &unchanged, sizeof(snapshot)), 0);
	CHECK_EQ(registry->release(&request, view), OK);
	registry->detach(owner);
}

#ifdef THREADS_ENABLED
// T15: each serialized outcome of clone versus last release is contractual.
// Sharing the original handle for clones breaks clone-first survival; accepting
// an already released handle breaks release-first rejection.
TEST_CASE("device_exit::foundation/isolated/clone-release-ordered-outcomes") {
	for (bool clone_first : { false, true }) {
		auto registry = RegistryAccess::make(1, 3);
		uint64_t cycle;
		REQUIRE_EQ(registry->begin_cycle(cycle), OK);
		RecordRef owner, scope;
		REQUIRE_EQ(registry->reserve_open(2, owner, scope), OK);
		if (!owner.is_valid() || !scope.is_valid()) {
			return;
		}
		registry->attach(owner);
		GDExtensionDeviceExitLibraryViewRef view = {};
		REQUIRE_EQ(RegistryAccess::view(*registry, std::move(scope), view), OK);
		registry->detach(owner);
		owner.reset();
		GDExtensionDeviceExitLibraryViewRef cloned = { 99, 99 };
		int32_t clone_result = -1;
		int32_t release_result = -1;
		std::atomic<bool> first_finished{ false };
		std::thread cloning([&]() {
			if (!clone_first) {
				while (!first_finished.load()) {
					std::this_thread::yield();
				}
			}
			clone_result = registry->clone(&request, view, &cloned);
			if (clone_first) {
				first_finished.store(true);
			}
		});
		std::thread releasing([&]() {
			if (clone_first) {
				while (!first_finished.load()) {
					std::this_thread::yield();
				}
			}
			release_result = registry->release(&request, view);
			if (!clone_first) {
				first_finished.store(true);
			}
		});
		cloning.join();
		releasing.join();
		CHECK_EQ(release_result, OK);
		if (clone_first) {
			CHECK_EQ(clone_result, OK);
			CHECK_NE(cloned.handle_serial, view.handle_serial);
			GDExtensionDeviceExitLibrarySnapshot snapshot = { { 1, 48, 0 }, 0, 0, 0, 0, 0, 0, 0 };
			CHECK_EQ(registry->copy(cloned, &snapshot), OK);
			CHECK_EQ(snapshot.wrapper_association, 2);
			CHECK_EQ(registry->release(&request, cloned), OK);
		} else {
			CHECK_EQ(clone_result, ERR_INVALID_DATA);
			CHECK_EQ(cloned.registry_instance, 0);
			CHECK_EQ(cloned.handle_serial, 0);
		}
		CHECK_EQ(RegistryAccess::occupied(*registry), 0);
	}
}

TEST_CASE("device_exit::foundation/isolated/view-transfer-and-concurrent-clones") {
	auto registry = RegistryAccess::make(1, 16);
	uint64_t cycle;
	REQUIRE_EQ(registry->begin_cycle(cycle), OK);
	RecordRef owner, scope;
	REQUIRE_EQ(registry->reserve_open(2, owner, scope), OK);
	if (!owner.is_valid() || !scope.is_valid()) {
		return;
	}
	registry->attach(owner);
	GDExtensionDeviceExitLibraryViewRef view = {};
	REQUIRE_EQ(RegistryAccess::view(*registry, std::move(scope), view), OK);
	std::atomic<int> failures{ 0 };
	std::thread threads[4];
	for (auto &thread : threads) {
		thread = std::thread([&]() {
			for (int i = 0; i < 100; i++) {
				GDExtensionDeviceExitLibraryViewRef cloned = {};
				if (registry->clone(&request, view, &cloned) != OK) {
					failures++;
					continue;
				}
				GDExtensionDeviceExitLibrarySnapshot snapshot = { { 1, 48, 0 }, 0, 0, 0, 0, 0, 0, 0 };
				if (registry->copy(cloned, &snapshot) != OK || snapshot.loader_kind != 2 || snapshot.wrapper_association != 1) {
					failures++;
				}
				if (registry->release(&request, cloned) != OK) {
					failures++;
				}
				if (registry->release(&request, cloned) != ERR_INVALID_DATA) {
					failures++;
				}
			}
		});
	}
	for (auto &thread : threads) {
		thread.join();
	}
	CHECK_EQ(failures.load(), 0);
	registry->detach(owner);
	owner.reset();
	std::thread last_release([&]() { if (registry->release(&request, view) != OK) { failures++; } });
	last_release.join();
	CHECK_EQ(failures.load(), 0);
	CHECK_EQ(RegistryAccess::occupied(*registry), 0);
}
#endif

// A custom loader's OK describes its logical close, not native-image unloading.
// The retained-image flag below is fixture state, not an actual DSO observation.
class ObservingCustomLoader : public GDExtensionLoader {
public:
	Error initialize_result = OK;
	bool open = false;
	bool native_image_retained = true;
	int closes = 0;
	Error open_library(const String &) override {
		open = true;
		return OK;
	}
	Error initialize(GDExtensionInterfaceGetProcAddress p_get, const Ref<GDExtension> &p_extension, GDExtensionInitialization *r_initialization) override {
		metadata_entry(p_get, p_extension.ptr(), r_initialization);
		return initialize_result;
	}
	Error close_library() override {
		open = false;
		closes++;
		return OK;
	}
	bool is_library_open() const override { return open; }
	bool has_library_changed() const override { return false; }
	bool library_exists() const override { return true; }
};

TEST_CASE("device_exit::foundation/observed-fields/custom-return-close-and-reopen") {
	for (const Error returned : { OK, ERR_CANT_CREATE, ERR_BUSY }) {
		MetadataCapture observations;
		capture = &observations;
		Ref<ObservingCustomLoader> loader;
		loader.instantiate();
		loader->initialize_result = returned;
		Ref<GDExtension> extension;
		extension.instantiate();
		CHECK_EQ(extension->open_library("custom://observations", loader), returned);
		CHECK(observations.available());
		if (!observations.available()) {
			capture = nullptr;
			continue;
		}
		CHECK_EQ(observations.during_entry.loader_kind, 3);
		CHECK_EQ(observations.during_entry.loader_initialize_returned, 0);
		CHECK_EQ(observations.during_entry.wrapper_association, 1);
		const auto first_view = observations.view;
		GDExtensionDeviceExitLibrarySnapshot first = { { 1, 48, 0 }, 0, 0, 0, 0, 0, 0, 0 };
		CHECK_EQ(observations.copy(first_view, &first), OK);
		CHECK_EQ(first.loader_initialize_returned, 1);
		CHECK_EQ(first.loader_initialize_error, returned);
		CHECK_EQ(first.wrapper_association, returned == OK ? 1 : 2);
		if (returned == OK) {
			CHECK_EQ(extension->close_library(), OK);
		}
		CHECK_EQ(loader->closes, 1);
		CHECK(loader->native_image_retained);
		CHECK_EQ(observations.copy(first_view, &first), OK);
		CHECK_EQ(first.wrapper_association, 2);
		CHECK_EQ(first.guarded_qualified, 0);
		loader->initialize_result = OK;
		CHECK_EQ(extension->open_library("custom://observations", loader), OK);
		CHECK_NE(observations.during_entry.library_generation, first.library_generation);
		GDExtensionDeviceExitLibrarySnapshot historical = { { 1, 48, 0 }, 0, 0, 0, 0, 0, 0, 0 };
		CHECK_EQ(observations.copy(first_view, &historical), OK);
		CHECK_EQ(std::memcmp(&historical, &first, sizeof(first)), 0);
		extension.unref();
		CHECK_EQ(loader->closes, 2);
		GDExtensionDeviceExitLibrarySnapshot latest = { { 1, 48, 0 }, 0, 0, 0, 0, 0, 0, 0 };
		CHECK_EQ(observations.copy(observations.view, &latest), OK);
		CHECK_EQ(latest.wrapper_association, 2);
		CHECK_EQ(latest.loader_initialize_error, OK);
		CHECK_EQ(latest.guarded_qualified, 0);
		CHECK_EQ(observations.release(&request, first_view), OK);
		CHECK_EQ(observations.release(&request, observations.view), OK);
		capture = nullptr;
	}
}

TEST_CASE("device_exit::foundation/observed-fields/entry-false-preserves-actual-error") {
	MetadataCapture observations;
	observations.return_success = false;
	capture = &observations;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(metadata_entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	ERR_PRINT_OFF;
	const Error result = extension->open_library("libgodot://entry-false", loader);
	ERR_PRINT_ON;
	CHECK_EQ(result, FAILED);
	CHECK_FALSE(extension->is_library_open());
	REQUIRE(observations.available());
	if (!observations.available()) {
		capture = nullptr;
		return;
	}
	GDExtensionDeviceExitLibrarySnapshot snapshot = { { 1, 48, 0 }, 0, 0, 0, 0, 0, 0, 0 };
	CHECK_EQ(observations.copy(observations.view, &snapshot), OK);
	CHECK_EQ(snapshot.loader_initialize_returned, 1);
	CHECK_EQ(snapshot.loader_initialize_error, FAILED);
	CHECK_EQ(snapshot.wrapper_association, 2);
	CHECK_EQ(observations.release(&request, observations.view), OK);
	capture = nullptr;
}

} // namespace TestDeviceExitContract
