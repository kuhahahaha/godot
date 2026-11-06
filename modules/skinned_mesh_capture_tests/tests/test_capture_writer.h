/**************************************************************************/
/*  test_capture_writer.h                                                 */
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

#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_capture_writer.h"
#ifdef UNIX_ENABLED
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
TEST_CASE("[P01] Inherited writer emits original bytes and closes pipe without render work") {
	int handles[2];
	REQUIRE(pipe(handles) == 0);
	SkinnedMeshCaptureTestCaptureWriter writer;
	REQUIRE(writer.start(handles[1]));
	CHECK(writer.publish("{\"kind\":\"start\"}\n"));
	CHECK(writer.publish("{\"kind\":\"seal\"}\n"));
	writer.close();
	CHECK_FALSE(writer.failed());
	char bytes[256];
	const auto size = read(handles[0], bytes, sizeof(bytes));
	REQUIRE(size > 0);
	CHECK(std::string(bytes, size) == "{\"kind\":\"start\"}\n{\"kind\":\"seal\"}\n");
	CHECK(read(handles[0], bytes, sizeof(bytes)) == 0);
	CHECK_FALSE(writer.publish("{}\n"));
	::close(handles[0]);
}
TEST_CASE("[P01] Writer rejects nonpipe and broken receiver without SIGPIPE termination") {
	SkinnedMeshCaptureTestCaptureWriter missing;
	CHECK_FALSE(missing.start(-1));
	int handles[2];
	REQUIRE(pipe(handles) == 0);
	::close(handles[0]);
	SkinnedMeshCaptureTestCaptureWriter writer;
	REQUIRE(writer.start(handles[1]));
	CHECK(writer.publish("{}\n"));
	writer.close();
	CHECK(writer.failed());
}
TEST_CASE("[P01] Full inherited pipe poisons writer and never emits a later seal") {
	int handles[2];
	REQUIRE(pipe(handles) == 0);
	REQUIRE(fcntl(handles[1], F_SETFL, O_NONBLOCK) == 0);
	// Fill before starting the worker: no timing, reader, or GPU work can
	// change the transport admission condition under test.
	std::string fill(4096, 'x');
	size_t filled = 0;
	for (;;) {
		const ssize_t written = write(handles[1], fill.data(), fill.size());
		if (written < 0) {
			REQUIRE((errno == EAGAIN || errno == EWOULDBLOCK));
			if (fill.size() > 1) {
				fill.resize(1);
				continue;
			}
			break;
		}
		REQUIRE(written > 0);
		filled += size_t(written);
	}
	SkinnedMeshCaptureTestCaptureWriter writer;
	REQUIRE(writer.start(handles[1]));
	CHECK(writer.publish("{\"kind\":\"capture\"}\n"));
	writer.close();
	CHECK(writer.failed());
	CHECK_FALSE(writer.publish("{\"kind\":\"seal\"}\n"));
	char bytes[4096];
	size_t received = 0;
	ssize_t size;
	while ((size = read(handles[0], bytes, sizeof(bytes))) > 0) {
		CHECK(std::string(bytes, size) == std::string(size, 'x'));
		received += size_t(size);
	}
	CHECK(size == 0);
	CHECK(received == filled);
	::close(handles[0]);
}
#endif
