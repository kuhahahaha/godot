/**************************************************************************/
/*  skinned_mesh_capture_test_capture.h                                   */
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

#include <cstdint>
#include <mutex>
#include <vector>

struct SkinnedMeshCaptureTestCaptureEvent {
	uint64_t sequence = 0;
	uint8_t kind = 0;
	uint64_t token = 0;
	uint64_t epoch = 0;
	uint64_t rid = 0;
	uint64_t allocation_serial = 0;
	uint64_t owner = 0;
	uint64_t alias = 0;
	uint64_t queue_sequence = 0;
	uint32_t offset = 0;
	uint32_t size = 0;
	uint32_t result = 0;
};

class SkinnedMeshCaptureTestCapture {
	std::vector<SkinnedMeshCaptureTestCaptureEvent> events;
	struct Pending {
		uint64_t token, epoch, rid, serial, owner, alias, queue;
	};
	std::vector<Pending> pending;
	uint64_t epoch = 0;
	uint64_t next_sequence = 1;
	bool poisoned = false;
	bool sealed = false;
	bool disposed = false;
	bool eof = false;
	bool transport_failed = false;
	bool active = false;
	uint64_t last_epoch = 0;
	mutable std::mutex mutex;
	static constexpr size_t MAX_EVENTS = 4096;
	static constexpr size_t MAX_PENDING = 64;

public:
	SkinnedMeshCaptureTestCapture() = default;
	void begin(uint64_t p_epoch);
	bool append(const SkinnedMeshCaptureTestCaptureEvent &p_event);
	bool seal();
	bool dispose_physical();
	bool drain_complete() const;
	bool write_transport(const uint8_t *p_data, size_t p_size, size_t p_written);
	bool is_poisoned() const;
	bool is_sealed() const;
	bool is_disposed() const;
	bool has_transport_failure() const;
	std::vector<SkinnedMeshCaptureTestCaptureEvent> get_events() const;
};
