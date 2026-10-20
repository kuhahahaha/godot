#pragma once

#include <cstdint>

struct P01KernelInput {
	uint32_t value = 0;
};

inline uint32_t p01_kernel_identity(P01KernelInput p_input) {
	return p_input.value;
}
