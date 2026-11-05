/**************************************************************************/
/*  gdextension_qualification_rt_depth.h                                  */
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

#include <cstring>

namespace GDExtensionDeviceExit {
// RT20 copied wire: ticket(16), draw_count(4), reserved(4), push_bytes(8),
// then exactly draw_count descriptors followed by exactly push_bytes bytes.
// No caller pointer survives admission, and no independent draw quota exists.
constexpr uint64_t QUALIFICATION_DEPTH_RT_PREFIX = 32;
struct QualificationDepthRtPayload {
	GDExtensionQualificationTicketRefV1 ticket = {};
	uint32_t draw_count = 0;
	uint64_t push_count = 0;
	const GDExtensionQualificationDepthDrawV1 *draws = nullptr;
	const uint8_t *push = nullptr;
};
inline Error qualification_depth_rt_payload(const uint8_t *bytes, uint64_t size, QualificationDepthRtPayload &out) {
	out = {};
	if (!bytes || size < QUALIFICATION_DEPTH_RT_PREFIX) {
		return ERR_INVALID_PARAMETER;
	}
	memcpy(&out.ticket, bytes, sizeof(out.ticket));
	memcpy(&out.draw_count, bytes + 16, sizeof(out.draw_count));
	memcpy(&out.push_count, bytes + 24, sizeof(out.push_count));
	uint32_t reserved = 0;
	memcpy(&reserved, bytes + 20, sizeof(reserved));
	if (!out.ticket.registry_instance || !out.ticket.handle_serial || reserved) {
		return ERR_INVALID_PARAMETER;
	}
	const uint64_t remaining = size - QUALIFICATION_DEPTH_RT_PREFIX;
	if (uint64_t(out.draw_count) > remaining / sizeof(GDExtensionQualificationDepthDrawV1)) {
		return ERR_INVALID_PARAMETER;
	}
	const uint64_t draw_bytes = uint64_t(out.draw_count) * sizeof(GDExtensionQualificationDepthDrawV1);
	if (out.push_count != remaining - draw_bytes) {
		return ERR_INVALID_PARAMETER;
	}
	if (out.draw_count && uintptr_t(bytes + QUALIFICATION_DEPTH_RT_PREFIX) % alignof(GDExtensionQualificationDepthDrawV1)) {
		return ERR_INVALID_PARAMETER;
	}
	out.draws = out.draw_count ? reinterpret_cast<const GDExtensionQualificationDepthDrawV1 *>(bytes + QUALIFICATION_DEPTH_RT_PREFIX) : nullptr;
	out.push = out.push_count ? bytes + QUALIFICATION_DEPTH_RT_PREFIX + draw_bytes : nullptr;
	return OK;
}
} // namespace GDExtensionDeviceExit
