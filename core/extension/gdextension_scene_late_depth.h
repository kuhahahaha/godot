/**************************************************************************/
/*  gdextension_scene_late_depth.h                                        */
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

#include "core/extension/gdextension_scene_phase.h"
#include "core/math/projection.h"
#include "core/math/transform_3d.h"
#include "core/templates/rid.h"
#include "core/templates/safe_refcount.h"

class RenderingDevice;

namespace GDExtensionDeviceExit {

inline constexpr uint64_t LATE_DEPTH_SAMPLE_KIND = 0x4b30364c44505448ull; // "K06LDPTH"

struct LateTransparentDepthDescriptor {
	RID viewport;
	RID depth_texture;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t view_count = 1;
	bool reverse_z = true;
	float z_near = 0.05f;
	float z_far = 4000.0f;
	uint64_t frame_number = 0;
	Transform3D camera_transform;
	Projection camera_projection;
};

class LateTransparentDepthContent : public SceneReadContent {
	GDExtensionSceneOpaque32V1 expected_device = {};
	GDExtensionSceneOpaque32V1 expected_sample = {};
	LateTransparentDepthDescriptor descriptor = {};
	SafeNumeric<uint32_t> *active_lease_counter = nullptr;
	bool returned = false;

public:
	LateTransparentDepthContent(
			const GDExtensionSceneOpaque32V1 &p_device,
			const GDExtensionSceneOpaque32V1 &p_sample,
			const LateTransparentDepthDescriptor &p_descriptor,
			SafeNumeric<uint32_t> *p_active_counter = nullptr);
	~LateTransparentDepthContent() override;

	Error validate(const GDExtensionSceneOpaque32V1 &p_device, const GDExtensionSceneOpaque32V1 &p_sample) const override;
	Error return_after_reads() override;

	bool is_returned() const { return returned; }
	const LateTransparentDepthDescriptor &get_descriptor() const { return descriptor; }
	const GDExtensionSceneOpaque32V1 &get_sample_token() const { return expected_sample; }
};

GDExtensionSceneOpaque32V1 make_late_depth_sample_token(uint64_t p_viewport_serial, uint64_t p_frame_number, uint32_t p_width, uint32_t p_height);

bool has_active_late_transparent_depth_phase(const RenderingDevice *p_device = nullptr, RID p_viewport = RID());
void dispatch_late_transparent_depth_phase(RenderingDevice &p_device);
void dispatch_late_transparent_depth_phase(RenderingDevice &p_device, const LateTransparentDepthDescriptor &p_descriptor);
bool get_current_late_transparent_depth_descriptor(LateTransparentDepthDescriptor &r_descriptor);
bool get_last_published_late_transparent_depth_descriptor(RID p_viewport, LateTransparentDepthDescriptor &r_descriptor);

} // namespace GDExtensionDeviceExit
