/**************************************************************************/
/*  test_qualification_finalize_tail.cpp                                  */
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

#include "core/os/os.h"
#include "servers/rendering/rendering_device.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
TEST_FORCE_LINK(test_qualification_finalize_tail)
class TestQualificationFinalizeAccess {
public:
	static void attach(RenderingDevice &device, RenderingDeviceDriver &driver, RenderingContextDriver &context, const char *mode) {
		device.driver = &driver;
		device.context = &context;
		if (!std::strcmp(mode, "dependency")) {
			device.dependency_map[RID::from_uint64(71)].insert(RID::from_uint64(72));
		}
		if (!std::strcmp(mode, "reverse")) {
			device.reverse_dependency_map[RID::from_uint64(72)].insert(RID::from_uint64(71));
		}
	}
	static void finish(RenderingDevice &device) { device._finalize_on_rendering_server_thread(); }
	static bool returned(const RenderingDevice &device) { return device.native_finalization_returned; }
};
namespace TestQualificationFinalizeTail {
class Context : public RenderingContextDriver {
	[[noreturn]] static void unexpected_operation() {
		CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation.");
		std::abort();
	}

public:
	Error initialize() override { unexpected_operation(); }
	const Device &device_get(uint32_t p_device_index) const override { unexpected_operation(); }
	uint32_t device_get_count() const override { unexpected_operation(); }
	bool device_supports_present(uint32_t p_device_index, SurfaceID p_surface) const override { unexpected_operation(); }
	RenderingDeviceDriver *driver_create() override { unexpected_operation(); }
	void driver_free(RenderingDeviceDriver *p_driver) override {
		std::fprintf(stderr, "QB1_DRIVER_FREE_RETURN\n");
		std::fflush(stderr);
	}
	SurfaceID surface_create(const void *p_platform_data) override { unexpected_operation(); }
	void surface_set_size(SurfaceID p_surface, uint32_t p_width, uint32_t p_height) override { unexpected_operation(); }
	void surface_set_vsync_mode(SurfaceID p_surface, DisplayServerEnums::VSyncMode p_vsync_mode) override { unexpected_operation(); }
	DisplayServerEnums::VSyncMode surface_get_vsync_mode(SurfaceID p_surface) const override { unexpected_operation(); }
	void surface_set_hdr_output_enabled(SurfaceID p_surface, bool p_enabled) override { unexpected_operation(); }
	bool surface_get_hdr_output_enabled(SurfaceID p_surface) const override { unexpected_operation(); }
	void surface_set_hdr_output_reference_luminance(SurfaceID p_surface, float p_reference_luminance) override { unexpected_operation(); }
	float surface_get_hdr_output_reference_luminance(SurfaceID p_surface) const override { unexpected_operation(); }
	void surface_set_hdr_output_max_luminance(SurfaceID p_surface, float p_max_luminance) override { unexpected_operation(); }
	float surface_get_hdr_output_max_luminance(SurfaceID p_surface) const override { unexpected_operation(); }
	void surface_set_hdr_output_linear_luminance_scale(SurfaceID p_surface, float p_linear_luminance_scale) override { unexpected_operation(); }
	float surface_get_hdr_output_linear_luminance_scale(SurfaceID p_surface) const override { unexpected_operation(); }
	float surface_get_hdr_output_max_value(SurfaceID p_surface) const override { unexpected_operation(); }
	uint32_t surface_get_width(SurfaceID p_surface) const override { unexpected_operation(); }
	uint32_t surface_get_height(SurfaceID p_surface) const override { unexpected_operation(); }
	void surface_set_needs_resize(SurfaceID p_surface, bool p_needs_resize) override { unexpected_operation(); }
	bool surface_get_needs_resize(SurfaceID p_surface) const override { unexpected_operation(); }
	void surface_destroy(SurfaceID p_surface) override { unexpected_operation(); }
	bool is_debug_utils_enabled() const override { unexpected_operation(); }
};
static void child() {
	if (!OS::get_singleton()->has_environment("GODOT_QB_FINALIZE_TAIL_CONTROL")) {
		std::_Exit(2);
	}
	const CharString mode_storage = OS::get_singleton()->get_environment("GODOT_QB_FINALIZE_TAIL_CONTROL").utf8();
	const char *mode = mode_storage.get_data();
	if ((std::strcmp(mode, "clean") && std::strcmp(mode, "dependency") && std::strcmp(mode, "reverse"))) {
		std::_Exit(2);
	}
	RenderingDevice device;
	TransferTestDriverBase driver;
	Context context;
	TestQualificationFinalizeAccess::attach(device, driver, context, mode);
	std::fprintf(stderr, "QB1_FINALIZE_ENTER mode=%s\n", mode);
	std::fflush(stderr);
	TestQualificationFinalizeAccess::finish(device);
	std::fprintf(stderr, "QB1_AFTER_FINALIZE returned=%d\n", int(TestQualificationFinalizeAccess::returned(device)));
	std::fflush(stderr);
	std::_Exit(!std::strcmp(mode, "clean") && TestQualificationFinalizeAccess::returned(device) ? 0 : 88);
}
REGISTER_TEST_COMMAND("--qualification-finalize-tail-child", child);
} //namespace TestQualificationFinalizeTail
