/**************************************************************************/
/*  test_frame.cpp                                                        */
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

#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_frame.h"
TEST_FORCE_LINK(test_capture_frame)
TEST_CASE("[P01] Frame identity rejects missing queue and nonmonotonic sequence") {
	SkinnedMeshCaptureTestCaptureEvent first{ 1, 1, 1, 2, 3, 4, 5, 6, 7, 0, 4, 0 };
	CHECK(skinned_mesh_capture_test_validate_frame_event(first, nullptr));
	auto next = first;
	CHECK_FALSE(skinned_mesh_capture_test_validate_frame_event(next, &first));
	next.queue_sequence = 9;
	CHECK(skinned_mesh_capture_test_validate_frame_event(next, nullptr));
	next.queue_sequence = 0;
	CHECK_FALSE(skinned_mesh_capture_test_validate_frame_event(next, nullptr));
}
