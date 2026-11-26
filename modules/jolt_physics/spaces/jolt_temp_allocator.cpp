/**************************************************************************/
/*  jolt_temp_allocator.cpp                                               */
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

/*
Adapted to Godot from the Jolt Physics library.
*/

/*
Copyright 2021 Jorrit Rouwe

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR
A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "jolt_temp_allocator.h"

#include "../jolt_project_settings.h"

#include "core/variant/variant.h"

#include <Jolt/Core/Memory.h>

#if defined(__APPLE__) || defined(__unix__)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

template <typename TValue, typename TAlignment>
constexpr TValue align_up(TValue p_value, TAlignment p_alignment) {
	return (p_value + p_alignment - 1) & ~(p_alignment - 1);
}

} //namespace

JoltTempAllocator::JoltTempAllocator() :
		capacity((uint64_t)JoltProjectSettings::temp_memory_b) {
	if (capacity == 0) {
		return;
	}
#if (defined(__APPLE__) || defined(__unix__)) && (defined(MAP_ANONYMOUS) || defined(MAP_ANON))
	const long raw_page = sysconf(_SC_PAGESIZE);
	if (raw_page > 0) {
		const size_t page_size = (size_t)raw_page;
		if (capacity <= (uint64_t)(SIZE_MAX - (page_size - 1))) {
			const size_t aligned_size = ((size_t)capacity + page_size - 1) & ~(page_size - 1);
#if defined(MAP_ANONYMOUS)
			constexpr int anon_flag = MAP_ANONYMOUS;
#else
			constexpr int anon_flag = MAP_ANON;
#endif
			void *mapped = mmap(nullptr, aligned_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | anon_flag, -1, 0);
			if (mapped != MAP_FAILED) {
				base = static_cast<uint8_t *>(mapped);
				mmap_bytes = aligned_size;
			}
		}
	}
#endif
	if (base == nullptr) {
		base = static_cast<uint8_t *>(JPH::Allocate((size_t)capacity));
	}
}

JoltTempAllocator::~JoltTempAllocator() {
	if (base == nullptr) {
		return;
	}
#if (defined(__APPLE__) || defined(__unix__)) && (defined(MAP_ANONYMOUS) || defined(MAP_ANON))
	if (mmap_bytes > 0) {
		munmap(base, mmap_bytes);
		base = nullptr;
		mmap_bytes = 0;
		return;
	}
#endif
	JPH::Free(base);
	base = nullptr;
}

void *JoltTempAllocator::Allocate(uint32_t p_size) {
	if (p_size == 0) {
		return nullptr;
	}

	p_size = align_up(p_size, 16U);

	const uint64_t new_top = top + p_size;

	void *ptr = nullptr;

	if (new_top <= capacity) {
		ptr = base + top;
	} else {
		WARN_PRINT_ONCE(vformat("Jolt Physics temporary memory allocator exceeded capacity of %d MiB. "
								"Falling back to slower general-purpose allocator. "
								"Consider increasing maximum temporary memory in project settings.",
				JoltProjectSettings::temp_memory_mib));

		ptr = JPH::Allocate(p_size);
	}

	top = new_top;

	return ptr;
}

void JoltTempAllocator::Free(void *p_ptr, uint32_t p_size) {
	if (p_ptr == nullptr) {
		return;
	}

	p_size = align_up(p_size, 16U);

	const uint64_t new_top = top - p_size;

	if (top <= capacity) {
		if (base + new_top != p_ptr) {
			CRASH_NOW_MSG("Jolt Physics temporary memory was freed in the wrong order.");
		}
	} else {
		JPH::Free(p_ptr);
	}

	top = new_top;
}
