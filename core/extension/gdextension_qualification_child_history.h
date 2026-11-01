/**************************************************************************/
/*  gdextension_qualification_child_history.h                             */
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

#include "core/error/error_list.h"
#include "core/error/error_macros.h"
#include "core/os/memory.h"

#include <cstring>
#include <type_traits>

namespace GDExtensionDeviceExit {
// Exact immutable origins belonging to the original bridge child table. The
// caller holds Registry::mutex. This storage never issues identities, performs
// native ACK, or grants allocation/account authority.
template <typename Origin, uint32_t Capacity>
class QualificationChildHistory {
public:
	struct Entry {
		Origin origin = {};
		uint64_t serial = 0;
		uint64_t retire_serial = 0; // Nonzero only after native ACK AND metadata pins return.
	};
	static constexpr uint32_t CAPACITY = Capacity;

private:
	static_assert(Capacity > 0 && Capacity <= SIZE_MAX / sizeof(Entry));
	static_assert(std::is_trivially_copyable_v<Origin> && std::is_trivially_destructible_v<Origin>);
	Entry *entries = nullptr;
	uint32_t occupied = 0;
	uint32_t retired = 0;

public:
	QualificationChildHistory() = default;
	QualificationChildHistory(const QualificationChildHistory &) = delete;
	QualificationChildHistory &operator=(const QualificationChildHistory &) = delete;
	~QualificationChildHistory() {
		if (entries) {
			Memory::free_static(entries, false);
		}
	}
	uint32_t used_count() const { return occupied; }
	uint32_t retired_count() const { return retired; }
	static constexpr uint64_t allocation_payload_bytes() { return uint64_t(Capacity) * sizeof(Entry); }
	static constexpr uint64_t allocation_request_bytes() {
		return allocation_payload_bytes()
#ifdef DEBUG_ENABLED
				+ Memory::DATA_OFFSET
#endif
				;
	}
	uint64_t allocated_bytes() const { return entries ? allocation_request_bytes() : 0; }
	// This pre-effect preparation may allocate the one fixed arena. begin() is
	// committed under the same Registry lock before the first native call.
	Error prepare(uint32_t &r_index) {
		r_index = UINT32_MAX;
		if (occupied == Capacity) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
		if (!entries) {
			entries = static_cast<Entry *>(Memory::alloc_static(allocation_payload_bytes(), false));
			if (!entries) {
				return ERR_OUT_OF_MEMORY;
			}
			for (uint32_t i = 0; i < Capacity; ++i) {
				memnew_placement(entries + i, Entry);
			}
		}
		for (uint32_t i = 0; i < Capacity; ++i) {
			if (!entries[i].serial) {
				r_index = i;
				return OK;
			}
		}
		CRASH_NOW(); // Count and original slots must agree.
		return ERR_BUG;
	}
	void begin(uint32_t p_index, const Origin &p_origin, uint64_t p_serial) {
		CRASH_COND(!entries || p_index >= Capacity || !p_serial || entries[p_index].serial || occupied == Capacity);
		std::memcpy(&entries[p_index].origin, &p_origin, sizeof(Origin));
		entries[p_index].serial = p_serial;
		++occupied;
	}
	// Only the original definitive no-native-result rollback may discard a live
	// reservation. Once cold, an origin remains exact until original bridge close.
	void discard(uint32_t p_index, uint64_t p_serial) {
		CRASH_COND(!entries || p_index >= Capacity || !p_serial || entries[p_index].serial != p_serial || entries[p_index].retire_serial || !occupied);
		entries[p_index] = {};
		--occupied;
	}
	void archive_retired_origin(uint32_t p_index, uint64_t p_serial, uint64_t p_retire_serial) {
		CRASH_COND(!entries || p_index >= Capacity || !p_serial || !p_retire_serial || entries[p_index].serial != p_serial || entries[p_index].retire_serial);
		entries[p_index].retire_serial = p_retire_serial;
		++retired;
	}
	template <typename Predicate>
	const Entry *find_retired(Predicate p_match) const {
		if (entries) {
			for (uint32_t i = 0; i < Capacity; ++i) {
				if (entries[i].serial && entries[i].retire_serial && p_match(entries[i].origin)) {
					return &entries[i];
				}
			}
		}
		return nullptr;
	}
};
} // namespace GDExtensionDeviceExit
