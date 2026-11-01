/**************************************************************************/
/*  test_device_exit_core_cycle.cpp                                       */
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
#include "core/config/project_settings.h"
#include "core/core_globals.h"
#include "core/extension/gdextension.h"
#include "core/extension/gdextension_function_loader.h"
#include "core/extension/gdextension_manager.h"
#include "core/os/os.h"
#include "core/register_core_types.h"
#include "tests/core/object/device_exit_startup_fixture.h"
#include "tests/test_macros.h"

#include <cstdio>
#include <cstring>

TEST_FORCE_LINK(test_device_exit_core_cycle)

namespace TestDeviceExitStartup {
namespace {
const GDExtensionDeviceExitAbi abi = { 1, 16, 0 };
GDExtensionDeviceExitLibraryViewRef retained = {};
GDExtensionInterfaceDeviceExitCopyLibrarySnapshotV1 copy_view = nullptr;
GDExtensionInterfaceDeviceExitCloneLibraryViewV1 clone_view = nullptr;
GDExtensionInterfaceDeviceExitReleaseLibraryViewV1 release_view = nullptr;
int initializations = 0;
int deinitializations = 0;
int failures = 0;
int checks = 0;

void expect(bool p_condition, const char *p_name) {
	checks++;
	std::printf("DEVICE_EXIT_CORE_CHECK %s pass=%d\n", p_name, p_condition ? 1 : 0);
	if (!p_condition) {
		failures++;
		std::fprintf(stderr, "DEVICE_EXIT_CORE_CYCLE_CHECK_FAILED %s\n", p_name);
	}
}

void initialize_level(void *, GDExtensionInitializationLevel p_level) {
	initializations++;
	expect(p_level == GDEXTENSION_INITIALIZATION_CORE, "actual-core-initialize-level");
}

void deinitialize_level(void *, GDExtensionInitializationLevel p_level) {
	deinitializations++;
	expect(p_level == GDEXTENSION_INITIALIZATION_CORE, "actual-core-deinitialize-level");
	// This invokes the actual owner hook from a real manager callback. A missing
	// reentry guard clears the interface map before the current callback returns.
	unregister_core_extensions();
	expect(GDExtension::get_interface_function("device_exit_copy_library_snapshot_v1") != nullptr,
			"interface-map-remains-live-inside-deinit-reentry");
	GDExtensionDeviceExitLibrarySnapshot snapshot = { { 1, 48, 0 }, 0, 0, 0, 0, 0, 0, 0 };
	expect(copy_view && copy_view(retained, &snapshot) == OK, "saved-view-inside-deinit");
}

GDExtensionBool entry(GDExtensionInterfaceGetProcAddress p_get, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_init) {
	*r_init = {};
	r_init->initialize = initialize_level;
	r_init->deinitialize = deinitialize_level;
	auto bootstrap = (GDExtensionInterfaceDeviceExitBootstrapLibraryV1)p_get("device_exit_bootstrap_library_v1");
	auto retain = (GDExtensionInterfaceDeviceExitRetainLibraryViewV1)p_get("device_exit_retain_library_view_v1");
	copy_view = (GDExtensionInterfaceDeviceExitCopyLibrarySnapshotV1)p_get("device_exit_copy_library_snapshot_v1");
	clone_view = (GDExtensionInterfaceDeviceExitCloneLibraryViewV1)p_get("device_exit_clone_library_view_v1");
	release_view = (GDExtensionInterfaceDeviceExitReleaseLibraryViewV1)p_get("device_exit_release_library_view_v1");
	expect(bootstrap && retain && copy_view && clone_view && release_view, "actual-five-interfaces");
	if (!bootstrap || !retain || !copy_view || !clone_view || !release_view) {
		return false;
	}
	GDExtensionDeviceExitLibraryCapability capability = {};
	expect(bootstrap(&abi, p_library, &capability) == OK, "actual-entry-bootstrap");
	expect(retain(&abi, capability, &retained) == OK, "actual-entry-retain");
	return true;
}

void cycle(bool p_should_open) {
	const Error registered = register_core_extensions();
	expect(registered == (p_should_open ? OK : ERR_BUSY), "actual-core-cycle-register-result");
	if (registered == OK && p_should_open) {
		expect(register_core_extensions() == ERR_BUSY, "duplicate-register-refused");
		{
			Ref<GDExtensionFunctionLoader> loader;
			loader.instantiate();
			loader->set_initialization_function(entry);
			expect(GDExtensionManager::get_singleton()->load_extension_with_loader("libgodot://core-cycle", loader) ==
							GDExtensionManager::LOAD_STATUS_OK,
					"actual-manager-load");
		}
	}
	const int before = deinitializations;
	unregister_core_extensions();
	expect(deinitializations == before + (registered == OK && p_should_open ? 1 : 0), "one-owned-deinitialize");
	unregister_core_extensions();
	expect(deinitializations == before + (registered == OK && p_should_open ? 1 : 0), "duplicate-unregister-is-no-op");
	if (copy_view && clone_view) {
		GDExtensionDeviceExitLibrarySnapshot snapshot = { { 1, 48, 0 }, 0, 0, 0, 0, 0, 0, 0 };
		expect(copy_view(retained, &snapshot) == OK, "saved-view-after-map-clear");
		GDExtensionDeviceExitLibraryViewRef denied = { 99, 99 };
		expect(clone_view(&abi, retained, &denied) == ERR_BUSY, "finalizing-refuses-new-clones");
		expect(denied.handle_serial == 0, "failed-clone-output-cleared");
	}
}
} //namespace

bool core_cycle_requested(int p_argc, char **p_argv) {
	for (int i = 0; i < p_argc; i++) {
		if (std::strcmp(p_argv[i], "--") == 0 || std::strcmp(p_argv[i], "++") == 0) {
			return false;
		}
		if (std::strcmp(p_argv[i], "--device-exit-core-cycle-child") == 0 ||
				std::strcmp(p_argv[i], "--device-exit-instance-exhaustion-child") == 0) {
			return true;
		}
	}
	return false;
}

int run_core_cycle() {
	CoreGlobals::print_ready = true;
	Engine *engine = memnew(Engine);
	register_core_types();
	ProjectSettings *settings = memnew(ProjectSettings);
	register_core_settings();
	if (instance_exhaustion_requested()) {
		cycle(true);
		expect(retained.registry_instance == UINT64_MAX, "instance-max-issued-once");
		replace_core_extension_manager();
		GDExtensionDeviceExitLibrarySnapshot snapshot = { { 1, 48, 0 }, 0, 0, 0, 0, 0, 0, 0 };
		expect(copy_view && copy_view(retained, &snapshot) == OK && snapshot.wrapper_association == 2, "max-instance-view-after-manager-destruction");
		expect(register_core_extensions() == ERR_BUSY, "max-instance-retained-cycle-refuses-restart");
		expect(release_view && release_view(&abi, retained) == OK, "max-instance-last-view-release");
		expect(register_core_extensions() == ERR_PARAMETER_RANGE_ERROR, "instance-exhaustion-refuses-next-cycle");
		expect(register_core_extensions() == ERR_PARAMETER_RANGE_ERROR, "instance-exhaustion-retry-remains-refused");
		expect(GDExtension::get_interface_function("device_exit_copy_library_snapshot_v1") == nullptr, "exhausted-cycle-publishes-no-interface");
		unregister_core_extensions();
		memdelete(settings);
		memdelete(engine);
		unregister_core_types();
		expect(copy_view && copy_view(retained, &snapshot) == ERR_INVALID_DATA, "max-instance-released-view-stays-invalid");
		expect(initializations == 1 && deinitializations == 1, "exhaustion-exact-callback-counts");
		CoreGlobals::print_ready = false;
		std::printf("DEVICE_EXIT_INSTANCE_RESULT checks=%d failures=%d initialize=%d deinitialize=%d\n", checks, failures, initializations, deinitializations);
		return failures == 0 ? 0 : 1;
	}
	cycle(true); // A: its last observer outlives the actual owned manager.
	replace_core_extension_manager();
	const auto old_view = retained;
	GDExtensionDeviceExitLibrarySnapshot first = { { 1, 48, 0 }, 0, 0, 0, 0, 0, 0, 0 };
	if (!copy_view || !release_view) {
		return 2;
	}
	expect(copy_view(retained, &first) == OK && first.wrapper_association == 2, "detached-after-real-manager-teardown");
	cycle(false); // B: must neither begin nor deinitialize A's extension.
	replace_core_extension_manager();
	expect(initializations == 1 && deinitializations == 1, "rejected-cycle-does-not-own-old-callbacks");
	expect(release_view(&abi, retained) == OK, "last-view-release-after-rejected-cycle");
	cycle(true); // C: now admissible, with fresh registry and generation identity.
	memdelete(settings);
	memdelete(engine);
	unregister_core_types();
	GDExtensionDeviceExitLibrarySnapshot third = { { 1, 48, 0 }, 0, 0, 0, 0, 0, 0, 0 };
	expect(copy_view(retained, &third) == OK && third.wrapper_association == 2, "new-cycle-detached-observation");
	expect(retained.registry_instance != old_view.registry_instance, "fresh-registry-cycle-identity");
	expect(third.library_generation > first.library_generation, "generation-not-reset-across-cycles");
	expect(copy_view(old_view, &first) == ERR_INVALID_DATA, "old-cycle-handle-remains-stale");
	expect(release_view(&abi, retained) == OK, "new-cycle-last-view-release-with-no-core-services");
	expect(initializations == 2 && deinitializations == 2, "exact-real-callback-counts");
	CoreGlobals::print_ready = false;
	std::printf("DEVICE_EXIT_CORE_CYCLE_RESULT checks=%d failures=%d initialize=%d deinitialize=%d\n", checks, failures, initializations, deinitializations);
	return failures == 0 ? 0 : 1;
}
} //namespace TestDeviceExitStartup
