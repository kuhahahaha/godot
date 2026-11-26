/**************************************************************************/
/*  test_temp_allocator.h                                                 */
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

#include "modules/jolt_physics/jolt_project_settings.h"
#include "modules/jolt_physics/spaces/jolt_temp_allocator.h"
#include "tests/test_macros.h"

#include <cstdint>
#include <cstring>

#if defined(__APPLE__) || defined(__unix__)
#include <unistd.h>
#endif

namespace TestJoltTempAllocator {

TEST_CASE("[JoltTempAllocator][BUG-188B] demand-paged anonymous mmap backing preserves capacity, alignment, LIFO reuse, and overflow fallback") {
	const int64_t saved_bytes = JoltProjectSettings::temp_memory_b;
	const int saved_mib = JoltProjectSettings::temp_memory_mib;
	struct SettingsRestore {
		int64_t bytes;
		int mib;
		~SettingsRestore() {
			JoltProjectSettings::temp_memory_b = bytes;
			JoltProjectSettings::temp_memory_mib = mib;
		}
	} restore{ saved_bytes, saved_mib };

	constexpr uint64_t kCapBytes = 4ull * 1024u * 1024u;
	JoltProjectSettings::temp_memory_mib = 4;
	JoltProjectSettings::temp_memory_b = int64_t(kCapBytes);

	JoltTempAllocator alloc;
	CHECK(alloc.get_capacity() == kCapBytes);
	CHECK(alloc.get_top() == 0u);
#if defined(__APPLE__) || defined(__unix__)
	const long page_size = sysconf(_SC_PAGESIZE);
	REQUIRE(page_size > 0);
	CHECK(alloc.get_mmap_bytes() >= size_t(kCapBytes));
	CHECK((alloc.get_mmap_bytes() % size_t(page_size)) == 0u);
#endif

	// Zero-size allocation returns nullptr without advancing top.
	CHECK(alloc.Allocate(0u) == nullptr);
	CHECK(alloc.get_top() == 0u);

	// Nested LIFO allocations are 16-byte aligned and writable without overlap.
	void *p0 = alloc.Allocate(13u);
	void *p1 = alloc.Allocate(4097u);
	void *p2 = alloc.Allocate(65536u);
	REQUIRE(p0 != nullptr);
	REQUIRE(p1 != nullptr);
	REQUIRE(p2 != nullptr);
	CHECK((uintptr_t(p0) & 15u) == 0u);
	CHECK((uintptr_t(p1) & 15u) == 0u);
	CHECK((uintptr_t(p2) & 15u) == 0u);
	CHECK(alloc.get_top() == 16u + 4112u + 65536u);

	memset(p0, 0xA1, 13u);
	memset(p1, 0xB2, 4097u);
	memset(p2, 0xC3, 65536u);
	CHECK(static_cast<const uint8_t *>(p0)[0] == 0xA1);
	CHECK(static_cast<const uint8_t *>(p1)[4096] == 0xB2);
	CHECK(static_cast<const uint8_t *>(p2)[65535] == 0xC3);

	// Overflow allocation (> capacity) falls back cleanly to JPH::Allocate / JPH::Free.
	constexpr uint32_t kOverflowSize = 5u * 1024u * 1024u;
	void *p_over = alloc.Allocate(kOverflowSize);
	REQUIRE(p_over != nullptr);
	memset(p_over, 0xD4, 4096u);
	CHECK(static_cast<const uint8_t *>(p_over)[4095] == 0xD4);
	alloc.Free(p_over, kOverflowSize);

	// LIFO frees unwind cleanly back to 0 and reuse exact base address.
	alloc.Free(p2, 65536u);
	alloc.Free(p1, 4097u);
	alloc.Free(p0, 13u);
	CHECK(alloc.get_top() == 0u);

	void *p_reuse = alloc.Allocate(1024u);
	CHECK(p_reuse == p0);
	alloc.Free(p_reuse, 1024u);
	CHECK(alloc.get_top() == 0u);
}

} // namespace TestJoltTempAllocator
