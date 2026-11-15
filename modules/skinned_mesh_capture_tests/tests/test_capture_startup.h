/**************************************************************************/
/*  test_capture_startup.h                                                */
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
#include "tests/test_macros.h"

#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_capture_startup.h"
TEST_CASE("[P01] Startup parses exactly one inherited endpoint and parent fixed identity") {
	SkinnedMeshCaptureTestCaptureStartup startup;
	CHECK(skinned_mesh_capture_test_parse_capture_startup({}, startup) == SkinnedMeshCaptureTestStartupParse::Absent);
	CHECK(skinned_mesh_capture_test_parse_capture_startup({ "--p01-capture-fd", "9" }, startup) == SkinnedMeshCaptureTestStartupParse::Invalid);
	std::vector<std::string> args{ "--p01-capture-fd", "9", "--p01-run-id", "run", "--p01-child-id", "S02.N", "--p01-manifest-sha256", std::string(64, 'a') };
	CHECK(skinned_mesh_capture_test_parse_capture_startup(args, startup) == SkinnedMeshCaptureTestStartupParse::Valid);
	CHECK(startup.descriptor == 9);
	args.push_back("--p01-capture-fd");
	args.push_back("9");
	CHECK(skinned_mesh_capture_test_parse_capture_startup(args, startup) == SkinnedMeshCaptureTestStartupParse::Invalid);
	CHECK(skinned_mesh_capture_test_parse_capture_startup({ "--p01-capture-fd=9" }, startup) == SkinnedMeshCaptureTestStartupParse::Invalid);
}
