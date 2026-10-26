/**************************************************************************/
/*  skinned_mesh_capture_test_phase_observer.cpp                          */
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

#include "skinned_mesh_capture_test_phase_observer.h"

#include "skinned_mesh_capture_test_native_session.h"

#include "servers/rendering/render_capture_test_hooks.h"

#include <mutex>
#include <thread>

void skinned_mesh_capture_test_phase_capture_service(uint64_t p_epoch, uint64_t p_frame, bool p_after_views);

static thread_local bool capture_window = false;
static thread_local uint64_t capture_frame = 0;
static thread_local bool capture_after_views = false;
static std::mutex phase_mutex;
static std::vector<SkinnedMeshCaptureTestPhaseEvent> phase_events;
static uint64_t phase_epoch = 0, last_epoch = 0, last_frame = 0;
static bool active = false, awaiting_views = false, poisoned = false;
static std::thread::id phase_thread;

bool skinned_mesh_capture_test_phase_begin(uint64_t p_epoch) {
	std::lock_guard<std::mutex> lock(phase_mutex);
	if (active || !p_epoch || p_epoch <= last_epoch) {
		return false;
	}
	phase_epoch = last_epoch = p_epoch;
	last_frame = 0;
	active = true;
	awaiting_views = poisoned = false;
	phase_thread = {};
	phase_events.clear();
	return true;
}
void skinned_mesh_capture_test_phase_end() {
	std::lock_guard<std::mutex> lock(phase_mutex);
	if (active && awaiting_views) {
		poisoned = true;
	}
	active = false;
}
void RenderCaptureTestHooks::observe_phase(uint64_t p_frame, bool p_after_views) {
	std::lock_guard<std::mutex> lock(phase_mutex);
	if (!active) {
		return;
	}
	if (phase_thread == std::thread::id()) {
		phase_thread = std::this_thread::get_id();
	}
	if (poisoned || phase_thread != std::this_thread::get_id() || phase_events.size() >= 128 || p_after_views != awaiting_views || (p_after_views ? p_frame != last_frame : (!phase_events.empty() && p_frame <= last_frame))) {
		poisoned = true;
		return;
	}
	phase_events.push_back({ phase_events.size() + 1, phase_epoch, p_frame, p_after_views });
	last_frame = p_frame;
	awaiting_views = !p_after_views;
	capture_window = true;
	capture_frame = p_frame;
	capture_after_views = p_after_views;
	skinned_mesh_capture_test_phase_capture_service(phase_epoch, p_frame, p_after_views);
	SkinnedMeshCaptureTestNativeSession::service_phase(phase_epoch, p_frame, p_after_views);
	capture_window = false;
}
std::vector<SkinnedMeshCaptureTestPhaseEvent> skinned_mesh_capture_test_phase_events() {
	std::lock_guard<std::mutex> lock(phase_mutex);
	return phase_events;
}
bool skinned_mesh_capture_test_phase_poisoned() {
	std::lock_guard<std::mutex> lock(phase_mutex);
	return poisoned;
}

bool skinned_mesh_capture_test_phase_capture_window(uint64_t &r_frame, bool &r_after_views) {
	if (!capture_window) {
		return false;
	}
	r_frame = capture_frame;
	r_after_views = capture_after_views;
	return true;
}
