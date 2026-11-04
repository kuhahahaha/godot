/**************************************************************************/
/*  metal_buffer_size_table.h                                              */
/**************************************************************************/

#pragma once

#include <cstdint>
#include <cstring>
#include <limits>

// An argument-buffer set containing storage buffers always has this layout,
// even when its creating shader does not use OpArrayLength. This keeps sets
// interchangeable between shaders with the same reflected binding layout.
// The pointer and byte-size table live in the argument allocation itself.
struct MetalBufferSizeTable {
	uint32_t resource_count = 0;
	uint32_t sizes_offset = 0;
	uint32_t allocation_size = 0;

	bool initialize(uint32_t p_resource_count) {
		const uint64_t offset = (uint64_t(p_resource_count) + 1) * sizeof(uint64_t);
		const uint64_t size = (offset + uint64_t(p_resource_count) * sizeof(uint32_t) + 7) & ~uint64_t(7);
		if (p_resource_count == 0 || size > uint64_t(std::numeric_limits<int32_t>::max())) {
			return false;
		}
		resource_count = p_resource_count;
		sizes_offset = uint32_t(offset);
		allocation_size = uint32_t(size);
		return true;
	}

	bool write_length(void *p_data, uint32_t p_resource_index, uint64_t p_byte_length) const {
		if (p_resource_index >= resource_count || p_byte_length > std::numeric_limits<uint32_t>::max()) {
			return false;
		}
		const uint32_t length = uint32_t(p_byte_length);
		std::memcpy(static_cast<uint8_t *>(p_data) + sizes_offset + p_resource_index * sizeof(uint32_t), &length, sizeof(length));
		return true;
	}

	bool write_address(void *p_data, uint64_t p_buffer_address, uint64_t p_allocation_offset = 0) const {
		if (resource_count == 0 || (p_buffer_address & 7) != 0 || (p_allocation_offset & 7) != 0 ||
				p_allocation_offset > std::numeric_limits<uint64_t>::max() - sizes_offset ||
				p_buffer_address > std::numeric_limits<uint64_t>::max() - (p_allocation_offset + sizes_offset)) {
			return false;
		}
		const uint64_t address = p_buffer_address + p_allocation_offset + sizes_offset;
		std::memcpy(static_cast<uint8_t *>(p_data) + resource_count * sizeof(uint64_t), &address, sizeof(address));
		return true;
	}
};
