/**************************************************************************/
/*  test_image_collector.h                                                */
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

#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_image_collector.h"

#ifdef THREADS_ENABLED
#include <thread>
#endif

namespace TestImageCollector {
static PackedByteArray pixels() {
	PackedByteArray bytes;
	bytes.resize(128 * 64 * 4);
	bytes.fill(37);
	return bytes;
}
TEST_CASE("[P01] Image undelivered callback rejects only after its last holder") {
	SkinnedMeshCaptureTestImageCollector collector;
	Callable callback;
	REQUIRE(collector.reserve(0, callback) == OK);
	Callable retained = callback;
	collector.close();
	callback = Callable();
	PackedByteArray output;
	CHECK(collector.copy(0, output) == ERR_BUSY);
	CHECK_FALSE(collector.drained());
	retained = Callable();
	CHECK(collector.copy(0, output) == ERR_UNAVAILABLE);
	CHECK(collector.drained());
	CHECK_FALSE(collector.poisoned());
}

TEST_CASE("[P01] Image undelivered callback rejects before collector close") {
	SkinnedMeshCaptureTestImageCollector collector;
	Callable callback;
	REQUIRE(collector.reserve(0, callback) == OK);
	callback = Callable();
	PackedByteArray output;
	CHECK(collector.copy(0, output) == ERR_UNAVAILABLE);
	CHECK_FALSE(collector.drained());
	collector.close();
	CHECK(collector.drained());
	CHECK_FALSE(collector.poisoned());
}

#ifdef THREADS_ENABLED
TEST_CASE("[P01] Image undelivered callback release races safely with close") {
	SkinnedMeshCaptureTestImageCollector collector;
	Callable callback;
	REQUIRE(collector.reserve(0, callback) == OK);
	std::thread closer([&collector]() { collector.close(); });
	callback = Callable();
	closer.join();
	PackedByteArray output;
	CHECK(collector.copy(0, output) == ERR_UNAVAILABLE);
	CHECK(collector.drained());
	CHECK_FALSE(collector.poisoned());
}
#endif

TEST_CASE("[P01] Image callback close waits for receipt and final copied holder") {
	SkinnedMeshCaptureTestImageCollector collector;
	CHECK_FALSE(collector.drained());
	Callable callback;
	REQUIRE(collector.reserve(0, callback) == OK);
	Callable retained = callback;
	PackedByteArray output;
	CHECK(collector.copy(0, output) == ERR_BUSY);
	collector.close();
	Callable refused;
	CHECK(collector.reserve(1, refused) == ERR_UNAVAILABLE);
	CHECK_FALSE(collector.drained());
	auto bytes = pixels();
	callback.call(bytes);
	REQUIRE(collector.copy(0, output) == OK);
	bytes.set(0, 99);
	CHECK(output[0] == 37);
	output.set(0, 0);
	REQUIRE(collector.copy(0, output) == OK);
	CHECK(output[0] == 37);
	callback = Callable();
	CHECK_FALSE(collector.drained());
	retained = Callable();
	CHECK(collector.drained());
}
TEST_CASE("[P01] Image callback invalid payload and duplicate receipt poison capture") {
	for (int invalid = 0; invalid < 4; ++invalid) {
		SkinnedMeshCaptureTestImageCollector collector;
		Callable callback;
		REQUIRE(collector.reserve(0, callback) == OK);
		if (invalid == 0) {
			callback.call();
		}
		if (invalid == 1) {
			callback.call(12);
		}
		if (invalid == 2) {
			PackedByteArray small;
			small.resize(32767);
			callback.call(small);
		}
		if (invalid == 3) {
			callback.call(pixels());
			callback.call(pixels());
		}
		PackedByteArray output;
		CHECK(collector.copy(0, output) == ERR_INVALID_DATA);
		callback = Callable();
		collector.close();
		CHECK(collector.drained());
	}
}
TEST_CASE("[P01] Image rejected enqueue drains holders and forbids late callback") {
	SkinnedMeshCaptureTestImageCollector collector;
	Callable callback;
	REQUIRE(collector.reserve(0, callback) == OK);
	REQUIRE(collector.reject(0) == OK);
	CHECK(collector.reject(0) == ERR_INVALID_PARAMETER);
	CHECK(collector.poisoned());
	PackedByteArray output;
	// A duplicate rejection poisons the collector, so no result may be
	// certified from this instance.  The rejected-slot status is covered by a
	// fresh collector below, before exercising the poisoned late callback.
	CHECK(collector.copy(0, output) == ERR_INVALID_DATA);
	CHECK_FALSE(collector.drained());
	callback.call(pixels());
	CHECK(collector.copy(0, output) == ERR_INVALID_DATA);
	collector.close();
	callback = Callable();
	CHECK(collector.drained());

	SkinnedMeshCaptureTestImageCollector rejected;
	Callable rejected_callback;
	REQUIRE(rejected.reserve(0, rejected_callback) == OK);
	REQUIRE(rejected.reject(0) == OK);
	CHECK(rejected.copy(0, output) == ERR_UNAVAILABLE);
	rejected.close();
	rejected_callback = Callable();
	CHECK(rejected.drained());
}
TEST_CASE("[P01] Image callback survives collector and bounds reservations") {
	Callable surviving;
	{
		SkinnedMeshCaptureTestImageCollector collector;
		CHECK(collector.reserve(18, surviving) == ERR_INVALID_PARAMETER);
		REQUIRE(collector.reserve(17, surviving) == OK);
		Callable duplicate;
		CHECK(collector.reserve(17, duplicate) == ERR_ALREADY_IN_USE);
		CHECK(collector.reserve(0, surviving) == ERR_INVALID_PARAMETER);
		collector.close();
	}
	surviving.call(pixels());
	surviving = Callable();
}
} //namespace TestImageCollector
