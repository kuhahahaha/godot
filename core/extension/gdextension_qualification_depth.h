/**************************************************************************/
/*  gdextension_qualification_depth.h                                     */
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

#include "core/error/error_list.h"
#include "core/extension/gdextension_interface.gen.h"

namespace GDExtensionDeviceExit {
// Pure shape validation shared by direct and original copied RT admission.
// This validates a profile, never a source issuer or a resource reference.
inline Error validate_qualification_texture_shape(const GDExtensionQualificationTextureCreateV1 &q) {
	if (q.abi.abi_version != 1 || q.abi.struct_size != sizeof(GDExtensionDeviceExitAbi) || q.abi.reserved || !q.source.session_serial || !q.source.generation || !q.library_generation || !q.create_attempt || !q.width || !q.height || !q.depth || q.width > 4096 || q.height > 4096 || q.depth > 4096 || q.texture_type < 1 || q.texture_type > 3 || q.format < 1 || q.format > GDEXTENSION_QUALIFICATION_TEXTURE_FORMAT_D32_SFLOAT_V1 || !q.usage) {
		return ERR_INVALID_PARAMETER;
	}
	if (q.format == GDEXTENSION_QUALIFICATION_TEXTURE_FORMAT_D32_SFLOAT_V1) {
		if (q.texture_type != GDEXTENSION_QUALIFICATION_TEXTURE_TYPE_2D_V1 || q.depth != 1 || q.usage != (GDEXTENSION_QUALIFICATION_TEXTURE_USAGE_SAMPLING_V1 | GDEXTENSION_QUALIFICATION_TEXTURE_USAGE_DEPTH_ATTACHMENT_V1)) {
			return ERR_INVALID_PARAMETER;
		}
	} else if (q.usage & ~3U) {
		return ERR_INVALID_PARAMETER;
	}
	if ((q.texture_type == 1 && (q.height != 1 || q.depth != 1)) || (q.texture_type == 2 && q.depth != 1) || uint64_t(q.height) * q.depth > 4096 || uint64_t(q.width) * q.height * q.depth > 4194304) {
		return ERR_INVALID_PARAMETER;
	}
	return OK;
}
// Descriptor bounds only: the caller must authenticate the original raster,
// source, operation, target and content revision before any native effect.
// Draw/payload limits come from the existing admitted storage, never the
// logical operation range. A clear-only pass has no draw descriptors.
inline Error validate_qualification_depth_draws(const GDExtensionQualificationDepthDrawV1 *draws, uint32_t draw_count, const void *push_bytes, uint64_t push_byte_count, uint32_t width, uint32_t height, uint32_t reflected_push_size, uint32_t admitted_draw_capacity, uint64_t admitted_push_capacity) {
	if (!width || !height || reflected_push_size > 128 || (reflected_push_size & 3) || (draw_count && (!draws || uintptr_t(draws) % alignof(GDExtensionQualificationDepthDrawV1))) || (push_byte_count && !push_bytes)) {
		return ERR_INVALID_PARAMETER;
	}
	if (draw_count > admitted_draw_capacity || push_byte_count > admitted_push_capacity) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	for (uint32_t i = 0; i < draw_count; ++i) {
		const auto &draw = draws[i];
		if (draw.abi.abi_version != 1 || draw.abi.struct_size != sizeof(GDExtensionDeviceExitAbi) || draw.abi.reserved || draw.reserved || !draw.procedural_vertices || !draw.instances || !draw.scissor_width || !draw.scissor_height || draw.scissor_x > width || draw.scissor_width > width - draw.scissor_x || draw.scissor_y > height || draw.scissor_height > height - draw.scissor_y || draw.push_size != reflected_push_size || (draw.push_offset & 3) || draw.push_offset > push_byte_count || draw.push_size > push_byte_count - draw.push_offset) {
			return ERR_INVALID_PARAMETER;
		}
	}
	return OK;
}
} // namespace GDExtensionDeviceExit
