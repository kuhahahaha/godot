/**************************************************************************/
/*  skinned_mesh_capture_test_capture_writer.h                            */
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
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

// Private byte transport only. It grants no event origin, completion or verdict.
// Takes ownership only of a validated inherited POSIX write pipe. No path open,
// listener, reconnect or script binding. Render-side publication never does I/O.
class SkinnedMeshCaptureTestCaptureWriter {
	mutable std::mutex mutex;
	std::condition_variable ready;
	std::deque<std::string> queue;
	std::thread worker;
	int descriptor = -1;
	bool started = false, closing = false, poisoned = false;
	uint64_t bytes = 0;
	void run();

public:
	SkinnedMeshCaptureTestCaptureWriter() = default;
	SkinnedMeshCaptureTestCaptureWriter(const SkinnedMeshCaptureTestCaptureWriter &) = delete;
	SkinnedMeshCaptureTestCaptureWriter &operator=(const SkinnedMeshCaptureTestCaptureWriter &) = delete;
	~SkinnedMeshCaptureTestCaptureWriter();
	bool start(int p_inherited_write_descriptor);
	bool publish(const std::string &p_line);
	void close();
	bool failed() const;
};
