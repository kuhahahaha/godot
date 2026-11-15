/**************************************************************************/
/*  probe.cc                                                              */
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

// Standalone SDK62 real-entry probe; see README.md.
#include <godot_cpp/core/device_exit_api.hpp>

#include <godot_cpp/core/gdextension_interface_loader.hpp>
#include <cstring>

#include <cstdio>
#include <cstdlib>
static godot::DeviceExitApi api;
static GDExtensionClassLibraryPtr library;
static GDExtensionDeviceExitGuardToken token{};
static const GDExtensionDeviceExitAbi abi{ 1, sizeof(GDExtensionDeviceExitAbi), 0 };
static bool legacy;
static void check(const char *step, int32_t result) {
	std::printf("ABI_CALL %s=%d\n", step, result);
	std::fflush(stdout);
	if (result) {
		std::_Exit(101);
	}
}
static int32_t preflight(GDExtensionDeviceExitDeinitPreflightContextRef context, void *) {
	GDExtensionDeviceExitExitDiagnosticV1 diagnostic{ { 1, sizeof(diagnostic), 0 }, 0, GDEXTENSION_INITIALIZATION_SCENE, 0 };
	int32_t e = api.publish_exit_result(&abi, token, context, &diagnostic);
	check("publish_exit_result", e);
	return e;
}
static void initialize(void *, GDExtensionInitializationLevel level) {
	std::printf("INITIALIZE level=%d legacy=%d\n", int(level), int(legacy));
	std::fflush(stdout);
	if (legacy) {
		return;
	}
	GDExtensionDeviceExitInitializationLevelContextRef frame{};
	check("borrow_initialize_level", api.borrow_initialize_level(&abi, library, uint32_t(level), &frame));
	check("after_binding_initialize", api.after_binding_initialize(&abi, frame));
}
static void terminate(void *, GDExtensionInitializationLevel level) {
	std::printf("TERMINATE level=%d legacy=%d\n", int(level), int(legacy));
	std::fflush(stdout);
	if (legacy) {
		return;
	}
	GDExtensionDeviceExitDeinitLevelContextRef frame{};
	check("borrow_deinit_level", api.borrow_deinit_level(&abi, library, uint32_t(level), &frame));
	check("begin_user_terminator", api.begin_user_terminator(&abi, token, frame));
	check("after_user_terminator", api.after_user_terminator(&abi, frame));
	check("after_binding_deinit", api.after_binding_deinit(&abi, frame));
}
static const char *names[] = {
	"device_exit_register_guard_v1",
	"device_exit_borrow_guarded_library_v1",
	"device_exit_borrow_initialize_level_v1",
	"device_exit_borrow_deinit_level_v1",
	"device_exit_borrow_level_cleanup_v1",
	"device_exit_schedule_object_cleanup_v1",
	"device_exit_acquire_code_hold_v1",
	"device_exit_clone_code_hold_v1",
	"device_exit_release_code_hold_v1",
	"device_exit_after_binding_initialize_v1",
	"device_exit_begin_user_terminator_v1",
	"device_exit_after_user_terminator_v1",
	"device_exit_after_binding_deinit_v1",
	"device_exit_publish_exit_result_v1",
	"device_exit_terminate_initialize_blocked_v1",
	"device_exit_terminate_blocked_v1",
	"device_exit_register_scene_ready_v1",
	"device_exit_scene_ready_schedule_object_cleanup_v1",
	"device_session_bind_scene_ready_v1",
	"device_exit_scene_ready_abort_v1",
	"device_session_bind_main_scene_ready_v1",
	"device_session_qualification_open_v1",
	"device_session_qualification_backing_v1",
	"device_session_qualification_record_v1",
	"device_session_qualification_observe_v1",
	"device_session_qualification_facts_v1",
	"device_session_qualification_copy_v1",
	"device_session_qualification_release_v1",
	"device_session_qualification_close_v1",
	"device_session_qualification_cancel_v1",
	"device_session_qualification_storage_create_v1",
	"device_session_qualification_storage_domain_install_v1",
	"device_session_qualification_storage_generation_finalize_v1",
	"device_session_qualification_storage_retire_v1",
	"device_session_qualification_storage_retire_observe_v1",
	"device_session_qualification_storage_retire_release_v1",
	"device_session_qualification_prepare_uploads_v1",
	"device_session_qualification_upload_v1",
	"device_session_qualification_record_prepared_v1",
	"device_session_qualification_native_unit_create_v1",
	"device_session_qualification_native_unit_retire_v1",
	"device_session_qualification_native_unit_retire_observe_v1",
	"device_session_qualification_native_unit_retire_release_v1",
};
static const char *domain_name = "device_session_qualification_storage_domain_install_v1";
static const char *finalize_name = "device_session_qualification_storage_generation_finalize_v1";
static GDExtensionInterfaceGetProcAddress entry_lookup;
static const char *missing_capability;
static unsigned eager_session_lookups;
static GDExtensionInterfaceFunctionPtr ordinary_lookup(const char *name) {
	if (std::strncmp(name, "device_session_", 15) == 0) {
		++eager_session_lookups;
	}
	return entry_lookup(name);
}
static GDExtensionInterfaceFunctionPtr capability_lookup(const char *name) {
	return missing_capability && std::strcmp(name, missing_capability) == 0 ? nullptr : entry_lookup(name);
}
// Per-entry capability state; never publish these guarded pointers into globals.
static bool configured_finality_available(GDExtensionInterfaceGetProcAddress lookup) {
	const auto domain = reinterpret_cast<GDExtensionInterfaceDeviceSessionQualificationStorageDomainInstallV1>(lookup(domain_name));
	const auto finalize = reinterpret_cast<GDExtensionInterfaceDeviceSessionQualificationStorageGenerationFinalizeV1>(lookup(finalize_name));
	return domain && finalize;
}
extern "C" __attribute__((visibility("default"))) GDExtensionBool sdk62_entry(GDExtensionInterfaceGetProcAddress original_get, GDExtensionClassLibraryPtr owner, GDExtensionInitialization *out) {
	library = owner;
	entry_lookup = original_get;
	const bool ordinary_loaded = godot::internal::load_gdextension_interface(ordinary_lookup);
	const bool globals_null = !godot::gdextension_interface::device_session_qualification_storage_domain_install_v1 && !godot::gdextension_interface::device_session_qualification_storage_generation_finalize_v1;
	if (!ordinary_loaded || eager_session_lookups || !globals_null) std::_Exit(104);
	const bool available = configured_finality_available(capability_lookup);
	missing_capability = domain_name;
	const bool without_domain = configured_finality_available(capability_lookup);
	missing_capability = finalize_name;
	const bool without_finalize = configured_finality_available(capability_lookup);
	missing_capability = nullptr;
	std::printf("FINALITY_CAPABILITIES available=%d missing_domain=%d missing_finalize=%d ordinary_loaded=%d eager_session=%u globals_null=%d\n", available, without_domain, without_finalize, ordinary_loaded, eager_session_lookups, globals_null);
	if (without_domain || without_finalize) std::_Exit(105);

	legacy = original_get("device_exit_register_guard_v1") == nullptr;
	if (available == legacy) std::_Exit(106);
	unsigned visible = 0;
	for (const char *name : names) {
		auto p = original_get(name);
		visible += p != nullptr;
		std::printf("ORIGINAL_LOOKUP %s=%p\n", name, reinterpret_cast<void *>(p));
	}
	std::fflush(stdout);
	bool loaded = api.load(original_get, true, true);
	std::printf("SDK62_LOAD=%d LEGACY=%d VISIBLE=%u EXPECTED=43 V2_NULL=%d\n", loaded, int(legacy), visible, api.register_scene_ready_v2 == nullptr);
	std::fflush(stdout);
	if (legacy) {
		if (loaded || visible != 0) {
			std::_Exit(102);
		}
	} else {
		if (!loaded || visible != 43 || api.register_scene_ready_v2) {
			std::_Exit(103);
		}
	}
	*out = { GDEXTENSION_INITIALIZATION_SCENE, nullptr, initialize, terminate };
	if (legacy) {
		return 1;
	}
	GDExtensionDeviceExitGuardRegistrationV1 registration{};
	registration.abi = { 1, sizeof(registration), 0 };
	registration.binding_protocol = 1;
	registration.level_bitmap = registration.binding_cleanup_bitmap = 12;
	registration.user_cleanup_bitmap = 4;
	registration.expected_initialization = *out;
	registration.user_initializer = initialize;
	registration.user_terminator = terminate;
	registration.preflight = preflight;
	check("register_guard", api.register_guard(owner, &registration, &token));
	return 1;
}
