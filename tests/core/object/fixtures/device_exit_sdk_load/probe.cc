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
extern "C" __attribute__((visibility("default"))) GDExtensionBool sdk62_entry(GDExtensionInterfaceGetProcAddress original_get, GDExtensionClassLibraryPtr owner, GDExtensionInitialization *out) {
	library = owner;
	legacy = original_get("device_exit_register_guard_v1") == nullptr;
	unsigned visible = 0;
	for (const char *name : names) {
		auto p = original_get(name);
		visible += p != nullptr;
		std::printf("ORIGINAL_LOOKUP %s=%p\n", name, reinterpret_cast<void *>(p));
	}
	std::fflush(stdout);
	bool loaded = api.load(original_get, true, true);
	std::printf("SDK62_LOAD=%d LEGACY=%d VISIBLE=%u EXPECTED=41 V2_NULL=%d\n", loaded, int(legacy), visible, api.register_scene_ready_v2 == nullptr);
	std::fflush(stdout);
	if (legacy) {
		if (loaded || visible != 0) {
			std::_Exit(102);
		}
	} else {
		if (!loaded || visible != 41 || api.register_scene_ready_v2) {
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
