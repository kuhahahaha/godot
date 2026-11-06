/**************************************************************************/
/*  native_drain_diagnostic.h                                            */
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

#ifndef NATIVE_DRAIN_DIAGNOSTIC_H
#define NATIVE_DRAIN_DIAGNOSTIC_H

#include "core/os/os.h"
#include "core/os/thread.h"

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

// Temporary, opt-in observation only. Never participates in ownership or budgets.
namespace NativeDrainDiagnostic {
inline std::atomic<uint32_t> lines_reserved{ 0 };
inline std::atomic<uint64_t> next_pair{ 1 };
inline bool enabled() {
	static const bool value = []() {
		const char *setting = std::getenv("ANGE_NATIVE_DRAIN_DIAGNOSTIC");
		const char *ring = std::getenv("ANGE_NATIVE_DRAIN_DIAGNOSTIC_RING");
		return (setting && std::strcmp(setting, "1") == 0) || (ring && *ring);
	}();
	return value;
}
struct Sink {
	static constexpr uint32_t SLOT_BYTES = 768;
	static constexpr uint32_t SLOT_COUNT = 4096;
	std::mutex mutex;
	FILE *ring = nullptr;
	uint64_t next_sequence = 1;
	bool quota_reported = false;
	bool ring_failed = false;
	Sink() {
		const char *path = std::getenv("ANGE_NATIVE_DRAIN_DIAGNOSTIC_RING");
		if (!path || !*path) {
			std::fprintf(stderr, "[NATIVE-DRAIN-CORE] diagnostic=ring_disabled stderr_limit=4096 no_cleanup_arm_required=1\n");
			std::fflush(stderr);
			return;
		}
		if (path[0] != '/') {
			failure("ring_path_not_absolute", EINVAL);
			return;
		}
		ring = std::fopen(path, "w+b");
		if (!ring) {
			failure("ring_open_failed", errno);
			return;
		}
		// Fixed-size sparse initialization; no growing log and no background work.
		if (std::fseek(ring, long(SLOT_BYTES * SLOT_COUNT - 1), SEEK_SET) || std::fputc(0, ring) == EOF || std::fflush(ring)) {
			failure("ring_initialize_failed", errno);
			return;
		}
		std::fprintf(stderr, "[NATIVE-DRAIN-CORE] diagnostic=ring_ready slots=4096 slot_bytes=768 stderr_limit=4096 no_cleanup_arm_required=1\n");
		std::fflush(stderr);
	}
	void failure(const char *reason, int error) {
		if (!ring_failed) {
			std::fprintf(stderr, "[NATIVE-DRAIN-CORE] diagnostic=%s errno=%d ring_disabled=1\n", reason, error);
			std::fflush(stderr);
			ring_failed = true;
		}
	}
};
inline Sink &sink() {
	static Sink instance;
	return instance;
}
class Span {
	const char *event;
	uint64_t token = 0;
	bool stderr_admitted = false;
	uint64_t fields[6];
	int64_t code = 0;
	const char *reason = "returned";
	void emit(const char *phase) const {
		auto &output = sink();
		std::lock_guard<std::mutex> lock(output.mutex);
		const uint64_t sequence = output.next_sequence++;
		char record[Sink::SLOT_BYTES] = {};
		const int length = std::snprintf(record, sizeof(record), "[NATIVE-DRAIN-CORE] seq=%llu t=%llu thread=%llu pair=%llu event=%s phase=%s code=%lld reason=%s a=%llu b=%llu c=%llu d=%llu e=%llu f=%llu",
				(unsigned long long)sequence, (unsigned long long)OS::get_singleton()->get_ticks_usec(), (unsigned long long)Thread::get_caller_id(), (unsigned long long)token, event, phase,
				(long long)code, reason, (unsigned long long)fields[0], (unsigned long long)fields[1], (unsigned long long)fields[2],
				(unsigned long long)fields[3], (unsigned long long)fields[4], (unsigned long long)fields[5]);
		if (length < 0 || size_t(length) + 28 >= sizeof(record)) {
			output.failure("ring_record_too_long", EOVERFLOW);
			return;
		}
		// The checksum rejects torn slots if the process is killed during overwrite.
		uint64_t checksum = 14695981039346656037ull;
		for (int i = 0; i < length; ++i) {
			checksum = (checksum ^ uint8_t(record[i])) * 1099511628211ull;
		}
		std::snprintf(record + length, sizeof(record) - length, " checksum=%016llx\n", (unsigned long long)checksum);
		if (stderr_admitted) {
			std::fputs(record, stderr);
			std::fflush(stderr);
		} else if (!output.quota_reported) {
			std::fprintf(stderr, "[NATIVE-DRAIN-CORE] diagnostic=stderr_quota_exhausted reserved_lines=4096 ring_active=%d seq=%llu existing_span_exits_reserved=1\n", int(output.ring && !output.ring_failed), (unsigned long long)sequence);
			std::fflush(stderr);
			output.quota_reported = true;
		}
		if (output.ring && !output.ring_failed) {
			const long offset = long((sequence - 1) % Sink::SLOT_COUNT) * Sink::SLOT_BYTES;
			if (std::fseek(output.ring, offset, SEEK_SET) || std::fwrite(record, 1, sizeof(record), output.ring) != sizeof(record) || std::fflush(output.ring)) {
				output.failure("ring_write_failed", errno);
			}
		}
	}

public:
	Span(const char *p_event, uint64_t a = 0, uint64_t b = 0, uint64_t c = 0, uint64_t d = 0, uint64_t e = 0, uint64_t f = 0) :
			event(p_event), fields{ a, b, c, d, e, f } {
		if (!enabled()) {
			return;
		}
		token = next_pair.fetch_add(1, std::memory_order_relaxed);
		uint32_t current = lines_reserved.load(std::memory_order_relaxed);
		while (current < 4096) {
			if (lines_reserved.compare_exchange_weak(current, current + 2, std::memory_order_relaxed)) {
				stderr_admitted = true;
				break;
			}
		}
		emit("enter");
	}
	~Span() {
		if (token) {
			emit("exit");
		}
	}
	Span(const Span &) = delete;
	Span &operator=(const Span &) = delete;
	template <class T>
	T result(T value, const char *p_reason) {
		code = int64_t(value);
		reason = p_reason;
		return value;
	}
	void state(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
		fields[0] = a;
		fields[1] = b;
		fields[2] = c;
		fields[3] = d;
		fields[4] = e;
		fields[5] = f;
	}
};
} // namespace NativeDrainDiagnostic
#endif
