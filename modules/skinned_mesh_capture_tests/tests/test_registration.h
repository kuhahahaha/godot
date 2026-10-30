/**************************************************************************/
/*  test_registration.h                                                   */
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

// Included by modules_tests.gen.h only when this tests-only module is enabled.
// Keep the fixture test object in the module archive reachable by the linker.
namespace ForceLink {
#ifdef VULKAN_ENABLED
void force_link_test_capture_vulkan_fence_wait();
#endif
void force_link_test_capture_frame();
void force_link_test_capture_transport_queue();
void force_link_test_skinned_mesh_capture_test_probe_fixture();
} //namespace ForceLink

namespace TestSkinnedMeshCaptureTestProbeRegistration {
static const bool fixture_linked = []() {
#ifdef VULKAN_ENABLED
	ForceLink::force_link_test_capture_vulkan_fence_wait();
#endif
	ForceLink::force_link_test_capture_frame();
	ForceLink::force_link_test_capture_transport_queue();
	ForceLink::force_link_test_skinned_mesh_capture_test_probe_fixture();
	return true;
}();
} //namespace TestSkinnedMeshCaptureTestProbeRegistration
