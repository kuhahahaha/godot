/**************************************************************************/
/*  test_transport_queue.cpp                                              */
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

#include "tests/test_macros.h"

#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_transport.h"
TEST_FORCE_LINK(test_capture_transport_queue)
TEST_CASE("[P01] Transport overflow preserves already queued events for draining") {
	SkinnedMeshCaptureTestTransportQueue queue;
	SkinnedMeshCaptureTestCaptureEvent event{ 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0 };
	for (int i = 0; i < 128; i++) {
		CHECK(queue.push(event));
	}
	CHECK_FALSE(queue.push(event));
	CHECK(queue.is_poisoned());
	SkinnedMeshCaptureTestCaptureEvent received;
	for (int i = 0; i < 128; i++) {
		CHECK(queue.pop(received));
		CHECK(received.token == event.token);
	}
	CHECK_FALSE(queue.pop(received));
}
