/**************************************************************************/
/*  test_capture_codec.h                                                  */
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

#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_capture_codec.h"
TEST_CASE("[P01] Native codec fixes identities origin and lossless event integers") {
	SkinnedMeshCaptureTestCaptureCodec codec;
	SkinnedMeshCaptureTestCaptureIdentity identity{ "run", "S02.N", std::string(64, 'a'), "build" };
	std::string line;
	CHECK(codec.start(identity, line));
	CHECK(line.find("\"frame_seq\":\"0\"") != std::string::npos);
	SkinnedMeshCaptureTestHookEvent event;
	event.kind = 11;
	event.epoch = 2;
	event.sequence = 1;
	event.rid = UINT64_MAX;
	CHECK(codec.event(event, "a01", line));
	CHECK(line.find("18446744073709551615") != std::string::npos);
	CHECK(line.find("\"hook\":\"skeleton_upload\"") != std::string::npos);
	CHECK(line.find("\"origin\":\"engine\"") != std::string::npos);
	CHECK(codec.seal(line));
	CHECK_FALSE(codec.event(event, "a02", line));
}
TEST_CASE("[P01] Native codec rejects unknown hooks and noncanonical launch identity") {
	SkinnedMeshCaptureTestCaptureCodec codec;
	std::string line;
	CHECK_FALSE(codec.start({ "run", "S02.N", "bad", "build" }, line));
	CHECK(codec.start({ "run", "S02.N", std::string(64, 'a'), "build" }, line));
	SkinnedMeshCaptureTestHookEvent event;
	event.kind = 255;
	event.epoch = 1;
	event.sequence = 1;
	CHECK_FALSE(codec.event(event, "a01", line));
	CHECK_FALSE(codec.seal(line));
}

TEST_CASE("[P01] Native raw64 blob preserves fixed bytes and exact digest") {
	SkinnedMeshCaptureTestCaptureCodec codec;
	std::string line;
	REQUIRE(codec.start({ "run", "S02.N", std::string(64, 'a'), "build" }, line));
	uint8_t bytes[64] = {};
	CHECK(codec.blob("snapshot-0", bytes, sizeof(bytes), line));
	CHECK(line.find("\"declared_total\":\"64\"") != std::string::npos);
	CHECK(line.find("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA==") != std::string::npos);
	CHECK(codec.seal(line));
	CHECK(line.find("f5a5fd42d16a20302798ef6ed309979b43003d2320d9f0e8ea9831a92759fb4b") != std::string::npos);
}

TEST_CASE("[P01] Full RGBA8 image blob has exact size digest and line budget") {
	SkinnedMeshCaptureTestCaptureCodec codec;
	std::string line;
	REQUIRE(codec.start({ "run", "S02.N", std::string(64, 'a'), "build" }, line));
	std::vector<uint8_t> bytes(32768, 0);
	REQUIRE(codec.blob("image-0", bytes.data(), bytes.size(), line));
	CHECK(line.find("\"declared_total\":\"32768\"") != std::string::npos);
	CHECK(line.size() <= 65536);
	REQUIRE(codec.seal(line));
	CHECK(line.find("\"bytes\":\"32768\"") != std::string::npos);
	CHECK(line.find("c35020473aed1b4642cd726cad727b63fff2824ad68cedd7ffb73c7cbd890479") != std::string::npos);
}
