/**************************************************************************/
/*  skinned_mesh_capture_test_phase_observer.h                            */
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

#include "servers/rendering/render_capture_test_hooks.h"
#pragma once
#include <cstdint>
#include <vector>

struct SkinnedMeshCaptureTestPhaseEvent {
	uint64_t sequence = 0, epoch = 0, frame = 0;
	bool after_views = false;
};
// Private acquisition control. No script binding or external callback.
bool skinned_mesh_capture_test_phase_begin(uint64_t p_epoch);
void skinned_mesh_capture_test_phase_end();
std::vector<SkinnedMeshCaptureTestPhaseEvent> skinned_mesh_capture_test_phase_events();
bool skinned_mesh_capture_test_phase_poisoned();

// True only on the actual RT hook stack while queued capture work executes.
bool skinned_mesh_capture_test_phase_capture_window(uint64_t &r_frame, bool &r_after_views);
