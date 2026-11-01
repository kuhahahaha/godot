/**************************************************************************/
/*  client.c                                                              */
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

#include "core/extension/gdextension_interface.gen.h"

#include <dlfcn.h>
#include <stdio.h>

static GDExtensionInterfaceGetProcAddress lookup;
static GDExtensionClassLibraryPtr library;
static GDExtensionDeviceExitGuardToken token;
static const GDExtensionDeviceExitAbi abi = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
static int32_t preflight(GDExtensionDeviceExitDeinitPreflightContextRef context, void *userdata) {
	(void)userdata;
	GDExtensionDeviceExitExitDiagnosticV1 report = { { 1, sizeof(report), 0 }, 0, GDEXTENSION_INITIALIZATION_SCENE, 0 };
	return ((GDExtensionInterfaceDeviceExitPublishExitResultV1)lookup("device_exit_publish_exit_result_v1"))(&abi, token, context, &report);
}
static void initialize(void *userdata, GDExtensionInitializationLevel level) {
	(void)userdata;
	GDExtensionDeviceExitInitializationLevelContextRef frame = { 0 };
	if (((GDExtensionInterfaceDeviceExitBorrowInitializeLevelV1)lookup("device_exit_borrow_initialize_level_v1"))(&abi, library, level, &frame)) {
		__builtin_trap();
	}
	if (level == GDEXTENSION_INITIALIZATION_SCENE) {
		int (*run)(void) = (int (*)(void))dlsym(RTLD_DEFAULT, "b4_render_list_fixture_run");
		if (!run || run() != 0) {
			fprintf(stderr, "B4_LIST_FIXTURE_UNAVAILABLE\n");
			__builtin_trap();
		}
	}
	if (((GDExtensionInterfaceDeviceExitAfterBindingInitializeV1)lookup("device_exit_after_binding_initialize_v1"))(&abi, frame)) {
		__builtin_trap();
	}
}
static void terminate(void *userdata, GDExtensionInitializationLevel level) {
	(void)userdata;
	GDExtensionDeviceExitDeinitLevelContextRef frame = { 0 };
	if (((GDExtensionInterfaceDeviceExitBorrowDeinitLevelV1)lookup("device_exit_borrow_deinit_level_v1"))(&abi, library, level, &frame)) {
		__builtin_trap();
	}
	if (((GDExtensionInterfaceDeviceExitBeginUserTerminatorV1)lookup("device_exit_begin_user_terminator_v1"))(&abi, token, frame)) {
		__builtin_trap();
	}
	if (((GDExtensionInterfaceDeviceExitAfterUserTerminatorV1)lookup("device_exit_after_user_terminator_v1"))(&abi, frame)) {
		__builtin_trap();
	}
	if (((GDExtensionInterfaceDeviceExitAfterBindingDeinitV1)lookup("device_exit_after_binding_deinit_v1"))(&abi, frame)) {
		__builtin_trap();
	}
}
GDExtensionBool typed_dynamic_entry(GDExtensionInterfaceGetProcAddress get, GDExtensionClassLibraryPtr owner, GDExtensionInitialization *out) {
	lookup = get;
	library = owner;
	GDExtensionInterfaceDeviceExitRegisterGuardV1 register_guard = (GDExtensionInterfaceDeviceExitRegisterGuardV1)get("device_exit_register_guard_v1");
	if (!register_guard || !get("device_session_bind_v1")) {
		return 0;
	}
	*out = (GDExtensionInitialization){ GDEXTENSION_INITIALIZATION_SCENE, 0, initialize, terminate };
	GDExtensionDeviceExitGuardRegistrationV1 registration = { 0 };
	registration.abi = (GDExtensionDeviceExitAbi){ 1, sizeof(registration), 0 };
	registration.binding_protocol = 1;
	registration.level_bitmap = registration.binding_cleanup_bitmap = 12;
	registration.user_cleanup_bitmap = 4;
	registration.expected_initialization = *out;
	registration.user_initializer = initialize;
	registration.user_terminator = terminate;
	registration.preflight = preflight;
	return register_guard(owner, &registration, &token) == 0;
}
