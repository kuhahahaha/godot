/**************************************************************************/
/*  native_disposal_witness.h                                             */
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

#include "core/object/ref_counted.h"

#include <atomic>

class RenderingDevice;
class TestNativeDisposalAccess;

namespace GDExtensionDeviceExit {
class Registry;
class ProductionRequest;
struct ProductionDisposalObservation {
	uint32_t held = 0, queued = 0, physical_returned = 0, borrowed_detached = 0, uncreated = 0, owner_returned = 0, unknown = 0;
	bool dropped = false, settled = false;
};
// Membership storage lives only in the already quoted request arena.
class ProductionDisposalUse {
#ifdef TESTS_ENABLED
	friend class ::TestNativeDisposalAccess;
#endif
	friend class Registry;
	friend class ::RenderingDevice;
	ProductionRequest *request = nullptr;
	uint64_t request_serial = 0, group = 0;
	uint64_t rid = 0, serial = 0, native_driver_id = 0;
	uint32_t kind = UINT32_MAX;
	ProductionDisposalUse *next = nullptr;
	// Multiplicity belongs to this physical incarnation, not another free node.
	uint32_t counter_pins_remaining = 0;
	bool owned = false, counter_dropped = false, qualified_borrow = false;
	std::atomic<uint8_t> state{ 0 }; // unused,held,queued,physical-return,detached-borrow,unknown
public:
	ProductionDisposalUse() = default;
	ProductionDisposalUse(const ProductionDisposalUse &) = delete;
	ProductionDisposalUse &operator=(const ProductionDisposalUse &) = delete;
};
} //namespace GDExtensionDeviceExit

// Core-owned, single-object driver-free observation. This is neither a native
// operation/source capability nor an aggregate OutputReleased acknowledgment.
// All access is through the original main RD on its render thread.
class NativeDisposalWitness {
	friend class RenderingDevice;
	friend class GDExtensionDeviceExit::Registry;
#ifdef TESTS_ENABLED
	friend class TestNativeDisposalAccess;
	friend class TestExternalOutputPrefixAccess;
#endif

public:
	enum class State : uint8_t { EMPTY,
		LIVE,
		ENQUEUED,
		RETURNED,
		UNKNOWN };
	struct Report {
		State state = State::EMPTY;
		uint64_t rid = 0;
		uint64_t serial = 0;
	};

private:
	class Cell : public RefCounted {
	public:
		uint64_t device = 0;
		uint64_t rid = 0;
		uint64_t serial = 0;
		uint64_t backing = 0;
		GDExtensionDeviceExit::ProductionRequest *production_request = nullptr;
		uint64_t production_request_serial = 0, production_group_serial = 0, production_pin_serial = 0;
		uint32_t production_ordinal = UINT32_MAX;
		uint32_t production_dependent_sets = 0;
		State state = State::LIVE;
		// Both the actual RD resource/pending-free entry and the observer retain
		// this cell. No pointer into K storage or DSO callback survives here.
		bool matches(uint64_t p_rid, uint64_t p_serial, uint64_t p_backing) const {
			return rid && serial && backing && rid == p_rid && serial == p_serial && backing == p_backing;
		}
		bool enqueue(uint64_t p_rid, uint64_t p_serial, uint64_t p_backing) {
			if (state != State::LIVE || !matches(p_rid, p_serial, p_backing)) {
				state = State::UNKNOWN;
				return false;
			}
			state = State::ENQUEUED;
			return true;
		}
		void returned(uint64_t p_rid, uint64_t p_serial, uint64_t p_backing) {
			state = state == State::ENQUEUED && matches(p_rid, p_serial, p_backing) ? State::RETURNED : State::UNKNOWN;
		}
	};
	Ref<Cell> cell;
};
