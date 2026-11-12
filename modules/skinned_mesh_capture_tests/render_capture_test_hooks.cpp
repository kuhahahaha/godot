/**************************************************************************/
/*  render_capture_test_hooks.cpp                                         */
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

#include "skinned_mesh_capture_test_native_session.h"
#include "skinned_mesh_capture_test_native_source.h"
uint64_t RenderCaptureTestHooks::draw_record_count() {
	return SkinnedMeshCaptureTestNativeSource::draw_record_count();
}
bool RenderCaptureTestHooks::observe_draw(RID p_mesh_instance, uint32_t p_surface, RID p_vertex_array, uint64_t p_before_count, uint32_t p_renderer, uint32_t p_pass) {
	return SkinnedMeshCaptureTestNativeSource::observe_draw(p_mesh_instance, p_surface, p_vertex_array, p_before_count, p_renderer, p_pass);
}
void RenderCaptureTestHooks::retirement_pump() {
	SkinnedMeshCaptureTestNativeSession::service_retirement();
}
