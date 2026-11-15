/**************************************************************************/
/*  gdextension_output_config_facts.h                                        */
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

// Native Output configuration value projections; none grants owner/native authority.
#pragma once
#include "core/extension/gdextension_interface.gen.h"

#include <cmath>
namespace GDExtensionDeviceExit {
// V2 canonical descriptor: full original Registry/DeviceSessionKey plus type word.
// Same LE64 convention as original native Output tokens; never a profile digest.
inline bool output_device_tag_v2(uint64_t registry, uint64_t epoch, uint64_t session, GDExtensionOutputTokenV1 &out) {
	if (!registry || !epoch || !session) {
		return false;
	}
	const uint64_t words[] = { registry, epoch, session, UINT64_C(0x3256454454554f47) };
	GDExtensionOutputTokenV1 tag{};
	for (uint32_t word = 0; word < 4; ++word) {
		for (uint32_t byte = 0; byte < 8; ++byte) {
			tag.bytes[word * 8 + byte] = uint8_t(words[word] >> (byte * 8));
		}
	}
	out = tag;
	return true;
}
// Requested zero is observation-only, never an implicit pass policy/default.
inline bool output_pass_intent_v2(uint32_t requested, uint32_t eligible, uint32_t &selected) {
	if ((requested | eligible) & ~uint32_t(GDEXTENSION_OUTPUT_PASS_KNOWN_MASK_V2) || (requested & ~eligible)) {
		return false;
	}
	selected = requested;
	return true;
}
// Native mesh bounds may be planar. Do not substitute these for declared source
// bounds or the separately owned generated geometry envelope of Indexed144.
inline bool output_mesh_bounds_valid_v2(const GDExtensionOutputBoundsV1 &bounds) {
	for (uint32_t i = 0; i < 3; ++i) {
		if (!std::isfinite(bounds.position[i]) || !std::isfinite(bounds.size[i]) || bounds.size[i] < 0 || !std::isfinite(bounds.position[i] + bounds.size[i])) {
			return false;
		}
	}
	return true;
}
} // namespace GDExtensionDeviceExit
