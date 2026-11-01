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

// Standalone godot-cpp fixture, compiled by run_image_probe.py.
#include "resource_snapshot_bridge.h"

#include <godot_cpp/classes/curve.hpp>
#include <godot_cpp/classes/curve_texture.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

static AngeTextureSnapshotPlanFnV1 plan_fn = nullptr;
static AngeTextureSnapshotFnV1 snapshot_fn = nullptr;

class TextureSnapshotImageProbe : public godot::RefCounted {
	GDCLASS(TextureSnapshotImageProbe, godot::RefCounted)
	static void _bind_methods() {
		godot::ClassDB::bind_method(godot::D_METHOD("run"), &TextureSnapshotImageProbe::run);
	}
	static bool complete(uint32_t status, const AngeTextureSnapshotResultV1 &result, const AngeTextureSnapshotPlanV1 &plan) {
		return status == ANGE_TEXTURE_OK_V1 && result.status == ANGE_TEXTURE_OK_V1 && result.initialized_pixels == plan.source.width &&
				std::memcmp(&result.source, &plan.source, sizeof(result.source)) == 0;
	}
	int failures = 0;
	uint64_t checks = 0;
	void check(bool ok, const char *what) {
		++checks;
		if (!ok) {
			++failures;
			std::printf("E22_IMAGE_FAILURE %s\n", what);
		}
	}
	void compare(const godot::Ref<godot::CurveTexture> &texture, const std::vector<uint8_t> &pixels) {
		// Read actual normal _update output, outside snapshot. CurveTexture has
		// no get_image override in this fork; read its uploaded RID through RS.
		const godot::Ref<godot::Image> image = godot::RenderingServer::get_singleton()->texture_2d_get(texture->get_rid());
		if (image.is_null()) {
			check(false, "native image missing");
			return;
		}
		const bool rgb = texture->get_texture_mode() == godot::CurveTexture::TEXTURE_MODE_RGB;
		const auto bytes = image->get_data();
		const int channels = rgb ? 3 : 1;
		const bool shape = image->get_width() == texture->get_width() && image->get_height() == 1 &&
				image->get_format() == (rgb ? godot::Image::FORMAT_RGBF : godot::Image::FORMAT_RF) &&
				bytes.size() == texture->get_width() * channels * int(sizeof(float));
		check(shape, "native image shape/format");
		if (!shape) {
			return;
		}
		for (int i = 0; i < texture->get_width(); ++i) {
			float expected[4] = { 0, 0, 0, 1 };
			std::memcpy(expected, bytes.ptr() + i * channels * sizeof(float), channels * sizeof(float));
			check(std::memcmp(pixels.data() + i * 16, expected, sizeof(expected)) == 0, "native image pixel bytes");
		}
	}

public:
	int run() {
		if (!plan_fn || !snapshot_fn) {
			std::printf("E22_EXPORTS_UNAVAILABLE\n");
			return 3;
		}
		int cases = 0;
		for (int width : { 32, 33, 4096 }) {
			for (bool rgb : { false, true }) {
				for (int points : { 0, 1, 3 }) {
					for (int resolution : { 1, 2, 1000 }) {
						godot::Ref<godot::Curve> curve;
						curve.instantiate();
						if (points == 1) {
							curve->add_point({ .5, .75 });
						} else if (points == 3) {
							curve->add_point({ 0, .25 });
							curve->add_point({ .5, 1 });
							curve->add_point({ 1, .75 });
						}
						curve->set_bake_resolution(resolution);
						godot::Ref<godot::CurveTexture> texture;
						texture.instantiate();
						texture->set_width(width);
						texture->set_texture_mode(rgb ? godot::CurveTexture::TEXTURE_MODE_RGB : godot::CurveTexture::TEXTURE_MODE_RED);
						texture->set_curve(curve);
						AngeTextureSnapshotPlanV1 plan = {};
						const uint32_t planned = plan_fn(texture->_owner, ANGE_TEXTURE_FORMAT_RGBAF_V1, &plan);
						check(planned == 0, "plan");
						check(plan.real_bytes == sizeof(godot::real_t) && plan.bake_resolution == uint32_t(resolution) && plan.scratch_bytes == uint64_t(resolution) * plan.real_bytes, "actual scratch size");
						std::vector<uint8_t> pixels(size_t(width) * 16, 0xa5);
						alignas(8) std::array<uint8_t, 8000> scratch = {};
						AngeTextureSnapshotResultV1 result = {};
						const uint32_t status = snapshot_fn(texture->_owner, &plan, pixels.data(), pixels.size(), scratch.data(), scratch.size(), &result);
						check(complete(status, result, plan), "snapshot");
						if (status == 0) {
							compare(texture, pixels);
						}
						++cases;
						std::printf("E22_IMAGE_CASE width=%d mode=%s points=%d resolution=%d status=%u\n", width, rgb ? "RGB" : "RED", points, resolution, status);
					}
				}
			}
		}
		godot::Ref<godot::Curve> curve;
		curve.instantiate();
		curve->add_point({ 0, 0 });
		curve->add_point({ .5, 1 });
		curve->add_point({ 1, 0 });
		curve->set_bake_resolution(8);
		godot::Ref<godot::CurveTexture> texture;
		texture.instantiate();
		texture->set_width(32);
		texture->set_curve(curve);
		AngeTextureSnapshotPlanV1 old_plan = {}, new_plan = {};
		AngeTextureSnapshotResultV1 result = {};
		std::vector<uint8_t> old_pixels(512), fresh(512, 0xa5);
		alignas(8) std::array<uint8_t, 8000> scratch = {};
		check(plan_fn(texture->_owner, ANGE_TEXTURE_FORMAT_RGBAF_V1, &old_plan) == 0 && old_plan.real_bytes == sizeof(godot::real_t) && old_plan.bake_resolution == 8 && old_plan.scratch_bytes == 8 * sizeof(godot::real_t), "old plan");
		check(complete(snapshot_fn(texture->_owner, &old_plan, old_pixels.data(), old_pixels.size(), scratch.data(), scratch.size(), &result), result, old_plan), "old capture");
		const auto retained = old_pixels;
		curve->set_bake_resolution(2);
		check(plan_fn(texture->_owner, ANGE_TEXTURE_FORMAT_RGBAF_V1, &new_plan) == 0 && new_plan.real_bytes == sizeof(godot::real_t) && new_plan.bake_resolution == 2 && new_plan.scratch_bytes == 2 * sizeof(godot::real_t), "new plan");
		const uint32_t stale = snapshot_fn(texture->_owner, &old_plan, fresh.data(), fresh.size(), scratch.data(), scratch.size(), &result);
		check(stale == ANGE_TEXTURE_STALE_SOURCE_V1 && result.status == ANGE_TEXTURE_STALE_SOURCE_V1 && result.initialized_pixels == 0, "old plan stale");
		for (uint8_t b : fresh) {
			check(b == 0xa5, "stale output unchanged");
		}
		check(complete(snapshot_fn(texture->_owner, &new_plan, fresh.data(), fresh.size(), scratch.data(), scratch.size(), &result), result, new_plan), "fresh capture");
		check(old_pixels == retained && fresh != old_pixels, "immutable old bytes and changed values");
		// Silent writer retains its notification behavior. Refresh the normal
		// texture using public mode setters outside the capture call.
		curve->bake();
		texture->set_texture_mode(godot::CurveTexture::TEXTURE_MODE_RED);
		texture->set_texture_mode(godot::CurveTexture::TEXTURE_MODE_RGB);
		compare(texture, fresh);
		float before = 0, after = 0;
		std::memcpy(&before, old_pixels.data() + 16 * 16, 4);
		std::memcpy(&after, fresh.data() + 16 * 16, 4);
		std::printf("E22_IMAGE_REVISION stale=%u old_resolution=%u new_resolution=%u old_scratch=%llu new_scratch=%llu first_mid=%g second_mid=%g\n", stale, old_plan.bake_resolution, new_plan.bake_resolution, (unsigned long long)old_plan.scratch_bytes, (unsigned long long)new_plan.scratch_bytes, double(before), double(after));
		std::printf("E22_IMAGE_RESULT cases=%d checks=%llu failures=%d\n", cases, (unsigned long long)checks, failures);
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
