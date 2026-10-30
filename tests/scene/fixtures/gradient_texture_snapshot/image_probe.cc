/**************************************************************************/
/*  image_probe.cc                                                        */
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

// Standalone native image oracle. The caller waits for the ordinary deferred update.
#include "resource_snapshot_bridge.h"

#include <godot_cpp/classes/gradient_texture1_d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

#include <cstdio>
#include <cstring>
#include <vector>

static AngeTextureSnapshotPlanFnV1 plan_fn = nullptr;
static AngeTextureSnapshotFnV1 snapshot_fn = nullptr;
class TextureSnapshotImageProbe : public godot::RefCounted {
	GDCLASS(TextureSnapshotImageProbe, godot::RefCounted)
	static void _bind_methods() {
		godot::ClassDB::bind_method(godot::D_METHOD("run_case", "texture", "width", "profile", "mode", "space", "hdr", "points"), &TextureSnapshotImageProbe::run_case);
		godot::ClassDB::bind_method(godot::D_METHOD("finish"), &TextureSnapshotImageProbe::finish);
	}
	int failures = 0, cases = 0;
	uint64_t checks = 0;
	void check(bool ok, const char *what) {
		++checks;
		if (!ok) {
			++failures;
			std::printf("E22_GRADIENT_FAILURE case=%d %s\n", cases, what);
		}
	}

public:
	void run_case(const godot::Ref<godot::GradientTexture1D> &texture, int width, int profile, int mode, int space, bool hdr, int points) {
		++cases;
		std::printf("E22_GRADIENT_BEGIN case=%d width=%d profile=%d mode=%d space=%d hdr=%d points=%d\n", cases, width, profile, mode, space, int(hdr), points);
		if (texture.is_null() || !plan_fn || !snapshot_fn) {
			check(false, "missing input or native exports");
			return;
		}
		AngeTextureSnapshotPlanV1 plan = {};
		const uint32_t fmt = hdr ? ANGE_TEXTURE_FORMAT_RGBAF_V1 : ANGE_TEXTURE_FORMAT_RGBA8_V1;
		const uint32_t planned = plan_fn(texture->_owner, fmt, &plan);
		check(planned == ANGE_TEXTURE_OK_V1, "plan");
		const int pixel_bytes = hdr ? 16 : 4;
		check(planned == ANGE_TEXTURE_OK_V1 && plan.source.source_kind == ANGE_TEXTURE_SOURCE_GRADIENT_1D_V1 &&
						plan.source.width == uint32_t(width) && plan.source.native_mode == (hdr ? ANGE_TEXTURE_GRADIENT_HDR_V1 : ANGE_TEXTURE_GRADIENT_LDR_V1) &&
						plan.source.child_present == 1 && plan.point_count == uint32_t(points) && plan.real_bytes == sizeof(godot::real_t) &&
						plan.output_format == fmt && plan.output_bytes == uint64_t(width) * pixel_bytes && plan.scratch_bytes == 0,
				"plan fields");
		if (planned != ANGE_TEXTURE_OK_V1) {
			return;
		}
		std::vector<uint8_t> pixels(size_t(width) * pixel_bytes, 0xa5);
		AngeTextureSnapshotResultV1 result = {};
		const uint32_t status = snapshot_fn(texture->_owner, &plan, pixels.data(), pixels.size(), nullptr, 0, &result);
		check(status == ANGE_TEXTURE_OK_V1 && result.status == status && result.initialized_pixels == uint32_t(width) &&
						std::memcmp(&result.source, &plan.source, sizeof(result.source)) == 0,
				"snapshot status/tuple/count");
		if (status != ANGE_TEXTURE_OK_V1) {
			return;
		}
		// First RS read for this normally updated RID, strictly after capture.
		const godot::Ref<godot::Image> image = godot::RenderingServer::get_singleton()->texture_2d_get(texture->get_rid());
		if (image.is_null()) {
			check(false, "native image missing");
			return;
		}
		const auto bytes = image->get_data();
		const bool shape = image->get_width() == width && image->get_height() == 1 &&
				image->get_format() == (hdr ? godot::Image::FORMAT_RGBAF : godot::Image::FORMAT_RGBA8) &&
				bytes.size() == width * pixel_bytes;
		check(shape, "native image shape/format");
		if (!shape) {
			return;
		}
		for (int i = 0; i < width; ++i) {
			const bool equal = std::memcmp(pixels.data() + size_t(i) * pixel_bytes, bytes.ptr() + size_t(i) * pixel_bytes, pixel_bytes) == 0;
			if (!equal) {
				std::printf("E22_GRADIENT_PIXEL case=%d pixel=%d snapshot=", cases, i);
				for (int b = 0; b < pixel_bytes; ++b) {
					std::printf("%02x", unsigned(pixels[size_t(i) * pixel_bytes + b]));
				}
				std::printf(" image=");
				for (int b = 0; b < pixel_bytes; ++b) {
					std::printf("%02x", unsigned(bytes.ptr()[size_t(i) * pixel_bytes + b]));
				}
				std::printf("\n");
			}
			check(equal, "native image pixel bytes");
		}
		std::printf("E22_GRADIENT_CASE width=%d profile=%d mode=%d space=%d hdr=%d points=%d status=%u\n", width, profile, mode, space, int(hdr), points, status);
	}
	int finish() {
		std::printf("E22_GRADIENT_RESULT cases=%d checks=%llu failures=%d\n", cases, (unsigned long long)checks, failures);
		std::fflush(stdout);
		return failures ? 1 : 0;
	}
};
static void initialize(godot::ModuleInitializationLevel level) {
	if (level == godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
		godot::ClassDB::register_class<TextureSnapshotImageProbe>();
	}
}
static void terminate(godot::ModuleInitializationLevel) {}
extern "C" GDExtensionBool GDE_EXPORT texture_snapshot_image_init(GDExtensionInterfaceGetProcAddress get, GDExtensionClassLibraryPtr lib, GDExtensionInitialization *out) {
	plan_fn = reinterpret_cast<AngeTextureSnapshotPlanFnV1>(get(ANGE_TEXTURE_SNAPSHOT_PLAN_PROC_V1));
	snapshot_fn = reinterpret_cast<AngeTextureSnapshotFnV1>(get(ANGE_TEXTURE_SNAPSHOT_PROC_V1));
	godot::GDExtensionBinding::InitObject init(get, lib, out);
	init.register_initializer(initialize);
	init.register_terminator(terminate);
	init.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
	return init.init();
}
