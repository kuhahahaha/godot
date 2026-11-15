#pragma once

#ifdef TESTS_ENABLED
#include <cstdint>

// Count allocator entry attempts on the current test thread, including
// reallocations. The observation itself never allocates or calls user code.
namespace MemoryAllocationTest {
inline thread_local bool enabled = false;
inline thread_local uint64_t attempts = 0;
inline void observe() {
	if (enabled) {
		attempts++;
	}
}
} // namespace MemoryAllocationTest
#endif
