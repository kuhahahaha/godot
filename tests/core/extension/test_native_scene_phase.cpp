/**************************************************************************/
/*  test_native_scene_phase.cpp                                           */
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

#include "core/extension/gdextension_production_request.h"
#include "tests/test_macros.h"

#include <type_traits>

TEST_FORCE_LINK(test_native_scene_phase)

TEST_CASE("[ScenePhase][ABI] phase wire envelope is bounded POD with exact K06 layout") {
	static_assert(std::is_standard_layout_v<GDExtensionScenePhaseSinkConfigV1>);
	static_assert(std::is_trivially_copyable_v<GDExtensionNativeScenePhaseV1>);
	CHECK(sizeof(GDExtensionScenePhaseSinkConfigV1) == 56);
	CHECK(sizeof(GDExtensionNativeScenePhaseV1) == 128);
	GDExtensionScenePhaseSinkConfigV1 config{};
	config.abi = { 1, sizeof(config), 0 };
	config.phase = 1;
	config.max_operations = 64;
	CHECK(GDExtensionDeviceExit::scene_phase_config_valid(config));
	config.max_operations = 65;
	CHECK_FALSE(GDExtensionDeviceExit::scene_phase_config_valid(config));
	config.max_operations = 0;
	CHECK_FALSE(GDExtensionDeviceExit::scene_phase_config_valid(config));
	config.max_operations = 1;
	config.phase = 2;
	CHECK_FALSE(GDExtensionDeviceExit::scene_phase_config_valid(config));
}

TEST_CASE("[ScenePhase][Authority] copied phase bytes cannot create a physical context or registration") {
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	GDExtensionScenePhaseSinkConfigV1 config{};
	config.abi = { 1, sizeof(config), 0 };
	config.phase = config.max_operations = 1;
	GDExtensionScenePhaseRegistrationRefV1 registration{};
	CHECK(registry.scene_phase_register({}, {}, config, registration) != OK);
	CHECK(GDExtensionDeviceExit::scene_phase_word(registration.token, 0) == 0);
	CHECK(GDExtensionDeviceExit::scene_phase_word(registration.token, 1) == 0);
	GDExtensionScenePhaseCloseV1 closed{};
	CHECK(registry.scene_phase_unregister({}, closed) != OK);
	CHECK(closed.registration_closed == 0);
	CHECK(closed.relay_holder_closed == 0);
}
