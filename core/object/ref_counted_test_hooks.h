/**************************************************************************/
/*  ref_counted_test_hooks.h                                              */
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

#ifdef TESTS_ENABLED
#include <cstdint>

// Test scheduling receives detached values only, including after completion.
// No callback target or Object pointer may be carried into this channel.
namespace RefCountedReleaseTest {
enum class Phase {
	ADMITTED,
	BEFORE_MUTATION,
	CALLBACK_RESULTS,
	PIN_RETIRED,
	COMPLETE,
	JOIN_PENDING,
	JOIN_ZERO,
	TERMINAL_READY,
	INIT_PIN_RETIRED,
	INIT_COMPLETE,
};
struct Observation {
	uint64_t operation_id = 0;
	uint32_t refcount_result = 0;
	bool terminal = false;
	bool borrowed = false;
	uint32_t object_read_pins = 0;
	uint32_t completion_count = 0;
	bool pin_retired = false;
};
using Observer = void (*)(void *, Phase, const Observation &);
struct Channel {
	Observer observer = nullptr;
	void *context = nullptr;
	void emit(Phase p_phase, const Observation &p_observation) const {
		if (observer) {
			observer(context, p_phase, p_observation);
		}
	}
};
inline thread_local Channel channel;
} // namespace RefCountedReleaseTest
#endif
